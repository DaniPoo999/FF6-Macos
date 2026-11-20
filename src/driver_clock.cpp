#include "ff6/driver_clock.hpp"
#include <stdexcept>

namespace ff6 {
namespace {
constexpr unsigned quantum = 256; // Four canonical samples.
constexpr unsigned normal_period = FF6_ROM_VALUE_AUDIO_TIMER_PERIOD_CHOICES_0 * quantum;
constexpr unsigned fast_period = FF6_ROM_VALUE_AUDIO_TIMER_PERIOD_CHOICES_1 * quantum;
// The canonical fast period shares a compiled constant with the clock quantum.
// Authenticate that alias instead of storing a second copy of the ROM choice.
static_assert(FF6_ROM_VALUE_AUDIO_TIMER_PERIOD_CHOICES_1 == 1);
} // namespace
DriverClock::DriverClock(SoundTime activated_at, std::uint16_t until_four_sample_boundary)
    : last_(activated_at), boundary_phase_(0), remaining_(0) {
    if (until_four_sample_boundary == 0 || until_four_sample_boundary > quantum)
        throw std::invalid_argument("Invalid sound-clock startup phase");
    boundary_phase_ =
        static_cast<std::uint16_t>((activated_at % quantum + until_four_sample_boundary) % quantum);
    remaining_ = static_cast<std::uint16_t>(normal_period - quantum + until_four_sample_boundary);
}
void DriverClock::advance(SoundTime now) {
    if (now < last_)
        throw std::invalid_argument("Sound time moved backwards");
    const auto elapsed = now - last_;
    if (elapsed < remaining_) {
        remaining_ = static_cast<std::uint16_t>(remaining_ - elapsed);
    } else {
        const auto after_first = elapsed - remaining_;
        // Reduce before addition; even the largest representable time interval
        // needs no unbounded loop, deadline addition or overflowing counter.
        const auto increments = 1 + (after_first / period_) % 16;
        pending_ = static_cast<std::uint8_t>((pending_ + increments) % 16);
        remaining_ = static_cast<std::uint16_t>(period_ - after_first % period_);
    }
    last_ = now;
}
DriverOpportunity DriverClock::poll(SoundTime now) {
    advance(now);
    const DriverOpportunity result{pending_};
    pending_ = 0;
    return result;
}
std::uint8_t DriverClock::discard_after_song_load(SoundTime now) {
    return poll(now).elapsed;
}
void DriverClock::restart(SoundTime now, DriverRate rate) {
    if (now < last_ || (rate != DriverRate::normal && rate != DriverRate::fast_forward))
        throw std::invalid_argument("Invalid sound-clock restart");
    const auto phase_distance = (boundary_phase_ + quantum - now % quantum) % quantum;
    // A command at the boundary starts counting AFTER that boundary. Rate
    // changes do not move the underlying four-sample phase.
    const auto until_boundary = phase_distance == 0 ? quantum : phase_distance;
    period_ = static_cast<std::uint16_t>(rate == DriverRate::normal ? normal_period : fast_period);
    remaining_ = static_cast<std::uint16_t>(period_ - quantum + until_boundary);
    pending_ = 0;
    last_ = now;
}
} // namespace ff6
