#include "ff6/audio_performance.hpp"
#include <deque>
#include <limits>
#include <stdexcept>

namespace ff6 {
namespace {
// Semantic publication histories, bounded to one producer's lookahead. Queries
// are exclusive and monotonic; separate component queries preserve intermediate
// pitch, stereo and envelope values without a register map or device scheduler.
template <class T> class History {
    struct Change {
        SoundTime at;
        T value;
    };
    std::deque<Change> pending_;
    T value_{};
    SoundTime queried_ = 0, appended_ = 0;

  public:
    explicit History(T initial = {}) : value_(initial) {}
    void append(SoundTime at, T value) {
        if (at < queried_ || at < appended_ || pending_.size() >= 8192)
            throw std::invalid_argument("Late, unordered or excessive music publications");
        pending_.push_back({at, value});
        appended_ = at;
    }
    std::optional<T> take_before(SoundTime at) {
        if (at < queried_)
            throw std::invalid_argument("Music control time moved backwards");
        queried_ = at;
        std::optional<T> changed;
        while (!pending_.empty() && pending_.front().at < at) {
            value_ = pending_.front().value;
            pending_.pop_front();
            changed = value_;
        }
        return changed;
    }
    T before(SoundTime at) {
        (void)take_before(at);
        return value_;
    }
};
struct VoiceControls {
    History<StereoGain> gain;
    History<std::uint16_t> pitch;
    History<DriverEnvelope> envelope;
    History<std::optional<SampleIdentity>> sample;
};
} // namespace

struct AudioPerformance::Impl {
    const SampleBank& bank_;
    std::array<VoiceControls, 8> voices_;
    History<PublishedEchoFilter> filter_;
    History<StereoGain> echo_gain_;
    History<std::int8_t> feedback_;
    History<std::uint8_t> echo_, noise_, modulation_, noise_rate_, starts_, releases_{255};
    std::uint8_t pending_starts_ = 0, previous_starts_ = 0;
    std::optional<SoundTime> previous_frame_;

  public:
    explicit Impl(const SampleBank& bank) : bank_(bank) {}
    void append(const PublishedVoiceChange& change) {
        if (change.voice >= voices_.size())
            throw std::invalid_argument("Invalid music publication voice");
        auto& voice = voices_[change.voice];
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, StereoGain>)
                    voice.gain.append(change.at, value);
                else if constexpr (std::is_same_v<T, DriverPitchRequest>)
                    voice.pitch.append(change.at, value.value);
                else if constexpr (std::is_same_v<T, DriverEnvelope>) {
                    if (value.attack > 15 || value.decay > 7 || value.sustain_level > 7 ||
                        value.sustain_rate > 31)
                        throw std::invalid_argument("Invalid published envelope");
                    voice.envelope.append(change.at, value);
                } else {
                    if (value.identity && !sample_asset(bank_, *value.identity).sample)
                        throw std::invalid_argument("Missing published sample resource");
                    voice.sample.append(change.at, value.identity);
                }
            },
            change.value);
    }
    void append(const PublishedGlobalChange& change) {
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, PublishedEchoFilter>)
                    filter_.append(change.at, value);
                else if constexpr (std::is_same_v<T, PublishedEchoFeedback>)
                    feedback_.append(change.at, value.value);
                else if constexpr (std::is_same_v<T, PublishedEchoGain>)
                    echo_gain_.append(change.at, value.stereo);
                else if constexpr (std::is_same_v<T, PublishedNoiseRate>) {
                    if (value.value > 31)
                        throw std::invalid_argument("Invalid published noise rate");
                    noise_rate_.append(change.at, value.value);
                } else if constexpr (std::is_same_v<T, PublishedNoteGate>) {
                    if (value.kind != NoteGate::starts && value.kind != NoteGate::releases)
                        throw std::invalid_argument("Invalid published note gate");
                    (value.kind == NoteGate::starts ? starts_ : releases_)
                        .append(change.at, value.voices);
                } else {
                    if (value.kind != VoiceRouting::echo && value.kind != VoiceRouting::noise &&
                        value.kind != VoiceRouting::modulation)
                        throw std::invalid_argument("Invalid published voice routing");
                    auto& history = value.kind == VoiceRouting::echo    ? echo_
                                    : value.kind == VoiceRouting::noise ? noise_
                                                                        : modulation_;
                    history.append(change.at, value.voices);
                }
            },
            change.value);
    }
    template <class Publication> void append(const Publication& publication) {
        for (const auto& change : publication.changes)
            append(change);
    }
    void append(const DriverServicePublication& publication) {
        if (publication.buffered)
            append(*publication.buffered);
        for (const auto& script : publication.scripts)
            append(script.publication);
        if (publication.voices)
            append(publication.voices->publication.output);
    }
    SynthesisFrameInput controls(SoundTime at, bool consume_notes) {
        if (at < 76 || at > std::numeric_limits<SoundTime>::max() - 64 ||
            (previous_frame_ && at != *previous_frame_ + 64))
            throw std::invalid_argument("Invalid effective audio frame time");
        previous_frame_ = at;
        // Measured FFVI propagation deadlines from AUDIO_TIMING.md, expressed
        // directly as semantic interval queries. No generic hardware work loop.
        std::uint8_t on = 0, off = 0;
        if (consume_notes) {
            if (const auto changed = starts_.take_before(at - 60))
                pending_starts_ = *changed;
            pending_starts_ &= static_cast<std::uint8_t>(~previous_starts_);
            if (const auto changed = starts_.take_before(at - 58))
                pending_starts_ = *changed;
            on = pending_starts_;
            off = releases_.before(at - 58);
            previous_starts_ = on;
        }
        const auto noise = noise_.before(at - 62), echo = echo_.before(at - 62);
        const auto modulation = modulation_.before(at - 64);
        SynthesisFrameInput result;
        for (std::size_t i = 0; i < 8; ++i) {
            auto& history = voices_[i];
            auto& voice = result.voices[i];
            const auto point = at - 58 + 6 * i;
            const auto early = point - (i == 0 ? 18 : 2);
            const auto high = point - (i == 0 ? 16 : 0);
            const auto low_pitch = history.pitch.before(early);
            const auto high_pitch = history.pitch.before(high);
            voice.pitch = static_cast<std::uint16_t>((low_pitch & 255U) | (high_pitch & 0x3f00U));
            const auto attack = history.envelope.before(early);
            const auto sustain = history.envelope.before(point);
            voice.envelope = {attack.attack, attack.decay, sustain.sustain_level,
                              sustain.sustain_rate};
            voice.envelope_behavior =
                attack.enabled ? EnvelopeBehavior::adsr : EnvelopeBehavior::pause_hold;
            voice.volume[0] = history.gain.before(point + 2)[0];
            voice.volume[1] = history.gain.before(point + 4)[1];
            const auto mask = 1U << i;
            voice.release = (off & mask) != 0;
            voice.noise = (noise & mask) != 0;
            voice.echo = (echo & mask) != 0;
            voice.pitch_modulation = i != 0 && (modulation & mask) != 0;
            // Drain selection history even without a start, bounding memory.
            const auto selected = history.sample.before(point + (i < 2 ? 38 : 54));
            if (on & mask) {
                voice.trigger = true;
                if (selected)
                    voice.start = sample_asset(bank_, *selected).sample;
            }
        }
        result.noise_rate = noise_rate_.before(at - 58);
        constexpr std::array<unsigned, 8> delays{10, 8, 8, 6, 6, 6, 4, 4};
        for (std::size_t tap = 0; tap < 8; ++tap)
            result.echo.coefficients[tap] = filter_.before(at - delays[tap]).taps[tap];
        result.echo.echo_volume[0] = echo_gain_.before(at - 2)[0];
        result.echo.echo_volume[1] = echo_gain_.before(at)[1];
        result.echo.feedback = feedback_.before(at - 2);
        return result;
    }
};
AudioPerformance::AudioPerformance(const SampleBank& bank) : impl_(std::make_unique<Impl>(bank)) {}
AudioPerformance::~AudioPerformance() = default;
void AudioPerformance::append(const PublishedVoiceChange& change) {
    impl_->append(change);
}
void AudioPerformance::append(const PublishedGlobalChange& change) {
    impl_->append(change);
}
void AudioPerformance::append(const VoicePublication& publication) {
    impl_->append(publication);
}
void AudioPerformance::append(const GlobalPublication& publication) {
    impl_->append(publication);
}
void AudioPerformance::append(const DriverServicePublication& publication) {
    impl_->append(publication);
}
SynthesisFrameInput AudioPerformance::controls(SoundTime at, bool consume_notes) {
    return impl_->controls(at, consume_notes);
}
} // namespace ff6
