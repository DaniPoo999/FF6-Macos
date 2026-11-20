#pragma once
#include "ff6/content.hpp"
#include "ff6/sequencing.hpp"
#include <optional>

namespace ff6 {
enum class ScriptRegion : std::uint8_t { song, game_effects, system_effects };
struct ScriptPosition {
    ScriptRegion region = ScriptRegion::song;
    std::uint16_t offset = 0;
    bool operator==(const ScriptPosition&) const = default;
};

// The grammar is a game data language. These values are never CPU instructions.
enum class MusicCommand : std::uint8_t {
    volume = 0xc4,
    volume_slide,
    pan,
    pan_slide,
    pitch_slide,
    vibrato,
    vibrato_off,
    tremolo,
    tremolo_off,
    pan_sweep,
    pan_sweep_off,
    noise_rate,
    noise_on,
    noise_off,
    modulation_on,
    modulation_off,
    echo_on,
    echo_off,
    octave,
    octave_up,
    octave_down,
    transpose,
    transpose_add,
    detune,
    sample,
    attack,
    decay,
    sustain_level,
    sustain_rate,
    default_envelope,
    loop_start,
    loop_end,
    slur_on,
    slur_off_marker,
    drum_roll_on,
    drum_roll_off_marker,
    duration_override,
    effect_a,
    effect_b,
    end,
    end_ec,
    end_ed,
    end_ee,
    end_ef,
    tempo,
    tempo_slide,
    echo_volume,
    echo_volume_slide,
    song_volume,
    loop_ending,
    jump,
    echo_feedback,
    echo_filter,
    output_increment,
    output_clear,
    ignore_song_volume,
    conditional_jump,
    end_fd,
    end_fe,
    end_ff
};
std::uint8_t music_operand_count(MusicCommand command);
struct ScriptInstruction {
    std::uint8_t code = 0;
    std::array<std::uint8_t, 3> operands{};
    std::uint8_t operand_count = 0;
    bool is_note() const {
        return code < 0xc4;
    }
};

class SongScript {
    Bytes code_;
    std::uint16_t branch_origin_ = 0;
    std::array<std::optional<std::uint16_t>, 16> entries_{};

  public:
    // Header without the outer length prefix. Branch encodings are normalized
    // here; callers use offsets and cannot read outside the script region.
    explicit SongScript(ByteView serialized);
    ByteView code() const {
        return code_;
    }
    std::optional<ScriptPosition> entry(std::uint8_t voice, bool alternate = false) const;
    ScriptPosition branch(std::uint16_t encoded) const;
};

class MusicScripts {
    std::vector<SongScript> songs_;
    std::array<std::size_t, 85> identities_{};
    Bytes game_code_, system_code_;
    std::array<std::optional<std::uint16_t>, 512> game_entries_{};
    std::array<std::optional<std::uint16_t>, 16> system_entries_{};
    std::array<std::uint8_t, 14> durations_{};
    friend MusicScripts load_music_scripts(const ContentPack&);

  public:
    const SongScript& song(std::uint8_t identity) const;
    std::size_t stored_song_count() const {
        return songs_.size();
    }
    std::size_t song_record(std::uint8_t identity) const;
    std::optional<ScriptPosition> effect(std::uint8_t identity, bool second) const;
    std::optional<ScriptPosition> system_effect(std::uint8_t identity) const;
    ByteView region(std::uint8_t song_identity, ScriptRegion which) const;
    ScriptInstruction read(std::uint8_t song_identity, ScriptPosition position) const;
    std::uint8_t duration(std::uint8_t index) const;
};
MusicScripts load_music_scripts(const ContentPack& pack);

struct ScriptLoop {
    std::optional<ScriptPosition> start;
    std::uint8_t remaining = 0, iteration = 0;
    bool operator==(const ScriptLoop&) const = default;
};
struct ScriptFlow {
    ScriptPosition position;
    std::array<ScriptLoop, 4> loops{};
    std::uint8_t loop_slot = 0, wait = 0;
    bool active = true, conditional_pending = false;
    bool operator==(const ScriptFlow&) const = default;
};
struct ScriptContext {
    std::uint8_t song = 0;
    SequenceKind kind = SequenceKind::music;
    // An effect's ending test aliases the music loop counts for the same voice.
    // This explicit dependency replaces the original overlapping storage.
    std::array<std::uint8_t, 4> music_loop_remaining{};
};
enum class ScriptStepKind : std::uint8_t { note, control, flow, marker, stopped };
struct ScriptStep {
    ScriptStepKind kind = ScriptStepKind::control;
    ScriptInstruction instruction;
    // Sound ticks through the next token for flow/markers, or handoff to the
    // native note, control or termination handler for the other kinds.
    std::uint16_t work = 0;
    // Control handlers still fetch operands after the first. This is their
    // combined read/return work, excluding caller setup; zero for other kinds.
    std::uint16_t operand_work = 0;
};
// A control result MUST be applied by a native driver consumer. This component
// implements data-language flow, not those pitch/volume/envelope handlers.
// A failed operation leaves the supplied flow state unchanged.
ScriptStep step_script(const MusicScripts& scripts, const ScriptContext& context, ScriptFlow& flow);
std::vector<ScriptStep> run_script_to_note(const MusicScripts& scripts,
                                           const ScriptContext& context, ScriptFlow& flow,
                                           std::size_t command_limit = 4096);

struct ScriptArticulation {
    bool slur = false, drum_roll = false;
    bool operator==(const ScriptArticulation&) const = default;
};
struct ScriptLookahead {
    std::uint8_t next_code = 0;
    ScriptArticulation articulation;
    // Complete native scan work, including return, in 1/64-sample sound ticks.
    // Derived while scanning; does not advance the real flow a second time.
    std::uint32_t work = 0;
};
// Lookahead returns early articulation changes without changing real flow,
// loop counts or the conditional branch request. It has its own bounded cursor.
ScriptLookahead inspect_next_note(const MusicScripts& scripts, const ScriptContext& context,
                                  const ScriptFlow& flow, ScriptArticulation articulation,
                                  std::size_t command_limit = 4096);
} // namespace ff6
