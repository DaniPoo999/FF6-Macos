#include "ff6/music_scripts.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::uint8_t wrap_increment(std::uint8_t value) {
    return static_cast<std::uint8_t>((unsigned{value} + 1U) & 255U);
}
std::uint8_t previous_slot(std::uint8_t slot) {
    return static_cast<std::uint8_t>((unsigned{slot} + 3U) & 3U);
}
std::uint16_t word(const ScriptInstruction& instruction, std::size_t first = 0) {
    return static_cast<std::uint16_t>(unsigned{instruction.operands[first]} |
                                      (unsigned{instruction.operands[first + 1]} << 8U));
}
void validate(const MusicScripts& scripts, const ScriptContext& context, const ScriptFlow& flow) {
    (void)scripts.song(context.song);
    require(context.kind == SequenceKind::music || context.kind == SequenceKind::effect,
            "Invalid script channel kind");
    require(flow.loop_slot < 4, "Invalid script loop slot");
    require(flow.active, "Script channel has stopped");
}
std::uint8_t iteration(const ScriptContext& context, const ScriptFlow& flow, std::uint8_t slot) {
    return context.kind == SequenceKind::music ? flow.loops[slot].iteration
                                               : context.music_loop_remaining[slot];
}
ScriptPosition loop_start(const ScriptLoop& loop) {
    require(loop.start.has_value(), "Script used an uninitialized loop target");
    return *loop.start;
}
void advance(ScriptPosition& position, const ScriptInstruction& instruction) {
    const auto next = unsigned{position.offset} + 1U + instruction.operand_count;
    require(next <= 65535, "Script position overflow");
    position.offset = static_cast<std::uint16_t>(next);
}
bool terminating(MusicCommand command) {
    const auto value = static_cast<std::uint8_t>(command);
    return (value >= 0xeb && value <= 0xef) || value >= 0xfd;
}
unsigned script_read_work(ScriptPosition position, unsigned byte_index) {
    // Serialized-region alignment affects the original cursor carry path.
    // Preserve that work from a native offset, without constructing an address
    // or accessing a device/memory map. SCRIPT_WORK.md documents the layout.
    const unsigned phase = position.region == ScriptRegion::song             ? 36U
                           : position.region == ScriptRegion::system_effects ? 255U
                                                                             : 0U;
    return ((phase + position.offset + byte_index) & 255U) == 255U ? 54U : 48U;
}
} // namespace

std::uint8_t music_operand_count(MusicCommand command) {
    // Parameter arity is the grammar, independently checked against the
    // extracted declaration. It is not a table of original song content.
    switch (command) {
    case MusicCommand::vibrato:
    case MusicCommand::tremolo:
    case MusicCommand::loop_ending:
        return 3;
    case MusicCommand::volume_slide:
    case MusicCommand::pan_slide:
    case MusicCommand::pitch_slide:
    case MusicCommand::pan_sweep:
    case MusicCommand::tempo_slide:
    case MusicCommand::echo_volume_slide:
    case MusicCommand::jump:
    case MusicCommand::echo_feedback:
    case MusicCommand::echo_filter:
    case MusicCommand::conditional_jump:
        return 2;
    case MusicCommand::volume:
    case MusicCommand::pan:
    case MusicCommand::noise_rate:
    case MusicCommand::octave:
    case MusicCommand::transpose:
    case MusicCommand::transpose_add:
    case MusicCommand::detune:
    case MusicCommand::sample:
    case MusicCommand::attack:
    case MusicCommand::decay:
    case MusicCommand::sustain_level:
    case MusicCommand::sustain_rate:
    case MusicCommand::loop_start:
    case MusicCommand::duration_override:
    case MusicCommand::effect_a:
    case MusicCommand::effect_b:
    case MusicCommand::tempo:
    case MusicCommand::echo_volume:
    case MusicCommand::song_volume:
        return 1;
    default:
        require(static_cast<std::uint8_t>(command) >= 0xc4, "Not a music command");
        return 0;
    }
}

SongScript::SongScript(ByteView serialized) {
    require(serialized.size() >= 36 && serialized.size() <= 65535, "Invalid song extent");
    branch_origin_ = read16(serialized, 0);
    const auto end = read16(serialized, 2);
    const auto length = serialized.size() - 36;
    require(((unsigned{end} + 65536U - branch_origin_) & 65535U) == length,
            "Song end does not match its extent");
    code_.assign(serialized.begin() + 36, serialized.end());
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto encoded = read16(serialized, 4 + 2 * i);
        if (encoded != end)
            entries_[i] = branch(encoded).offset;
    }
}
std::optional<ScriptPosition> SongScript::entry(std::uint8_t voice, bool alternate) const {
    require(voice < 8, "Invalid song voice");
    const auto offset = entries_[voice + (alternate ? 8U : 0U)];
    if (!offset)
        return std::nullopt;
    return ScriptPosition{ScriptRegion::song, *offset};
}
ScriptPosition SongScript::branch(std::uint16_t encoded) const {
    const auto offset = (unsigned{encoded} + 65536U - branch_origin_) & 65535U;
    require(offset < code_.size(), "Song branch target outside script data");
    return {ScriptRegion::song, static_cast<std::uint16_t>(offset)};
}

MusicScripts load_music_scripts(const ContentPack& pack) {
    MusicScripts bank;
    const auto durations = pack.get("audio_note_durations");
    const auto arities = pack.get("audio_script_operand_counts");
    require(durations.size() == 14 &&
                std::none_of(durations.begin(), durations.end(), [](auto b) { return b == 0; }),
            "Invalid note durations");
    std::copy(durations.begin(), durations.end(), bank.durations_.begin());
    require(arities.size() == 60, "Invalid music command grammar extent");
    for (unsigned i = 0; i < 60; ++i)
        require(arities[i] == music_operand_count(static_cast<MusicCommand>(i + 0xc4U)),
                "Music command grammar mismatch");
    const auto archive = pack.get("data_c85c7a_song_scripts");
    const auto properties = pack.get("data_c53d1c_sample_properties_and_song_tables");
    const auto count = pack.get("data_c53c5e_song_count");
    require(count.size() == 1 && count[0] == 85 && properties.size() == 3353,
            "Invalid song directory");
    std::map<std::size_t, std::size_t> starts;
    std::size_t position = 0;
    while (position < archive.size()) {
        const auto size = read16(archive, position);
        starts.emplace(position, bank.songs_.size());
        bank.songs_.emplace_back(slice(archive, position + 2, size));
        position += 2U + size;
    }
    require(!bank.songs_.empty(), "Empty song archive");
    for (std::size_t i = 0; i < bank.identities_.size(); ++i) {
        // This origin belongs to the serialized directory format only. Native
        // execution uses the selected record and a bounded byte offset.
        const auto encoded = read24(properties, 378 + 3 * i);
        require(encoded >= 0xc85c7a, "Song directory pointer precedes its container");
        const auto found = starts.find(encoded - 0xc85c7aU);
        require(found != starts.end(), "Song directory pointer does not identify a record");
        bank.identities_[i] = found->second;
    }
    const auto effects = pack.get("data_c52016_sfx_scripts_and_properties");
    require(effects.size() >= 1096 && read16(effects, 0) == 32 && read16(effects, 34) == 16 &&
                read16(effects, 52) == 16,
            "Invalid game-effect property headers");
    // The upload includes sixteen reserved bytes excluded from our data pack.
    require(read16(effects, 70) == effects.size() - 72 + 16, "Invalid game-effect transfer extent");
    bank.game_code_.assign(effects.begin() + 1096, effects.end());
    for (std::size_t i = 0; i < bank.game_entries_.size(); ++i) {
        const auto encoded = read16(effects, 72 + 2 * i);
        if (encoded == 0)
            continue;
        require(encoded >= 0x3000 && encoded - 0x3000U < bank.game_code_.size(),
                "Game-effect entry outside script data");
        bank.game_entries_[i] = static_cast<std::uint16_t>(encoded - 0x3000U);
    }
    const auto system = pack.get("audio_system_effect_scripts");
    require(system.size() >= 32 && system.size() <= 65535, "Invalid system-effect extent");
    bank.system_code_.assign(system.begin() + 32, system.end());
    for (std::size_t i = 0; i < bank.system_entries_.size(); ++i) {
        const auto encoded = read16(system, 2 * i);
        if (encoded == 0)
            continue;
        require(encoded >= 0x17ff && encoded - 0x17ffU < bank.system_code_.size(),
                "System-effect entry outside script data");
        bank.system_entries_[i] = static_cast<std::uint16_t>(encoded - 0x17ffU);
    }
    return bank;
}
std::size_t MusicScripts::song_record(std::uint8_t identity) const {
    require(identity < identities_.size(), "Unknown song identity");
    return identities_[identity];
}
const SongScript& MusicScripts::song(std::uint8_t identity) const {
    return songs_.at(song_record(identity));
}
std::optional<ScriptPosition> MusicScripts::effect(std::uint8_t identity, bool second) const {
    const auto entry = game_entries_[2U * identity + (second ? 1U : 0U)];
    if (!entry)
        return std::nullopt;
    return ScriptPosition{ScriptRegion::game_effects, *entry};
}
std::optional<ScriptPosition> MusicScripts::system_effect(std::uint8_t identity) const {
    require(identity < system_entries_.size(), "Unknown system effect");
    const auto entry = system_entries_[identity];
    if (!entry)
        return std::nullopt;
    return ScriptPosition{ScriptRegion::system_effects, *entry};
}
ByteView MusicScripts::region(std::uint8_t song_identity, ScriptRegion which) const {
    switch (which) {
    case ScriptRegion::song:
        return song(song_identity).code();
    case ScriptRegion::game_effects:
        return game_code_;
    case ScriptRegion::system_effects:
        return system_code_;
    }
    throw std::runtime_error("Invalid script region");
}
ScriptInstruction MusicScripts::read(std::uint8_t song_identity, ScriptPosition position) const {
    const auto code = region(song_identity, position.region);
    const auto first = slice(code, position.offset, 1)[0];
    ScriptInstruction instruction{first, {}, 0};
    if (first >= 0xc4)
        instruction.operand_count = music_operand_count(static_cast<MusicCommand>(first));
    const auto operands = slice(code, std::size_t{position.offset} + 1, instruction.operand_count);
    std::copy(operands.begin(), operands.end(), instruction.operands.begin());
    return instruction;
}
std::uint8_t MusicScripts::duration(std::uint8_t index) const {
    require(index < durations_.size(), "Invalid note-duration index");
    return durations_[index];
}

ScriptStep step_script(const MusicScripts& scripts, const ScriptContext& context,
                       ScriptFlow& flow) {
    validate(scripts, context, flow);
    auto next = flow;
    const auto instruction = scripts.read(context.song, next.position);
    advance(next.position, instruction);
    ScriptStep result{ScriptStepKind::control, instruction};
    const auto read = [&](unsigned index) { return script_read_work(flow.position, index); };
    auto work = 16U + read(0); // Token fetch, including its caller.
    if (instruction.is_note()) {
        result.kind = ScriptStepKind::note;
        work += next.wait == 0 ? 82U : 28U;
        if (next.wait == 0)
            next.wait = scripts.duration(static_cast<std::uint8_t>(instruction.code % 14));
    } else {
        const auto command = static_cast<MusicCommand>(instruction.code);
        const auto parameter = instruction.operands[0];
        work += instruction.operand_count == 0 ? 112U : 98U + read(1);
        for (unsigned i = 1; i < instruction.operand_count; ++i)
            result.operand_work = static_cast<std::uint16_t>(result.operand_work + read(i + 1));
        if (terminating(command)) {
            result.kind = ScriptStepKind::stopped;
            next.active = false;
        } else
            switch (command) {
            case MusicCommand::loop_start: {
                result.kind = ScriptStepKind::flow;
                work += (context.kind == SequenceKind::music ? 170U : 158U) +
                        (next.loop_slot == 3 ? 14U : 0U);
                next.loop_slot = static_cast<std::uint8_t>((unsigned{next.loop_slot} + 1U) & 3U);
                auto& loop = next.loops[next.loop_slot];
                loop.remaining = parameter == 0 ? 0 : wrap_increment(parameter);
                if (context.kind == SequenceKind::music)
                    loop.iteration = 1;
                loop.start = next.position;
                break;
            }
            case MusicCommand::loop_end: {
                result.kind = ScriptStepKind::flow;
                auto& loop = next.loops[next.loop_slot];
                work += context.kind == SequenceKind::music ? 22U : 0U;
                work += loop.remaining == 1   ? (next.loop_slot == 0 ? 120U : 98U)
                        : loop.remaining == 0 ? 108U
                                              : 128U;
                if (context.kind == SequenceKind::music)
                    loop.iteration = wrap_increment(loop.iteration);
                if (loop.remaining == 1)
                    next.loop_slot = previous_slot(next.loop_slot);
                else {
                    if (loop.remaining != 0)
                        --loop.remaining;
                    next.position = loop_start(loop);
                }
                break;
            }
            case MusicCommand::loop_ending:
                result.kind = ScriptStepKind::flow;
                work += read(2) + read(3);
                if (iteration(context, next, next.loop_slot) == parameter) {
                    work += next.loops[next.loop_slot].remaining == 1
                                ? (next.loop_slot == 0 ? 212U : 198U)
                                : 164U;
                    if (next.loops[next.loop_slot].remaining == 1)
                        next.loop_slot = previous_slot(next.loop_slot);
                    next.position = scripts.song(context.song).branch(word(instruction, 1));
                } else
                    work += 106;
                break;
            case MusicCommand::jump:
                result.kind = ScriptStepKind::flow;
                work += 78U + read(2);
                next.position = scripts.song(context.song).branch(word(instruction));
                break;
            case MusicCommand::conditional_jump:
                result.kind = ScriptStepKind::flow;
                work += read(2) + (next.conditional_pending ? 106U : 58U);
                if (next.conditional_pending) {
                    next.position = scripts.song(context.song).branch(word(instruction));
                    next.conditional_pending = false;
                }
                break;
            case MusicCommand::effect_a:
            case MusicCommand::effect_b: {
                result.kind = ScriptStepKind::flow;
                const auto target = scripts.effect(parameter, command == MusicCommand::effect_b);
                if (target) {
                    work += command == MusicCommand::effect_a ? 210U : 202U;
                    next.position = *target;
                } else {
                    // Stop at the shared termination handoff. The native owner
                    // must still perform channel termination and its work.
                    work += command == MusicCommand::effect_a ? 194U : 186U;
                    next.active = false;
                    result.kind = ScriptStepKind::stopped;
                }
                break;
            }
            case MusicCommand::duration_override:
                result.kind = ScriptStepKind::flow;
                work += 28;
                next.wait = parameter;
                break;
            case MusicCommand::slur_off_marker:
            case MusicCommand::drum_roll_off_marker:
                result.kind = ScriptStepKind::marker;
                work += 18;
                break;
            default:
                break; // An explicit control result must be consumed by the driver.
            }
    }
    if (result.kind != ScriptStepKind::control)
        result.operand_work = 0;
    result.work = static_cast<std::uint16_t>(work);
    flow = next;
    return result;
}

std::vector<ScriptStep> run_script_to_note(const MusicScripts& scripts,
                                           const ScriptContext& context, ScriptFlow& flow,
                                           std::size_t command_limit) {
    require(command_limit > 0 && command_limit <= 65536, "Invalid script command budget");
    auto next = flow;
    std::vector<ScriptStep> steps;
    for (std::size_t i = 0; i < command_limit; ++i) {
        steps.push_back(step_script(scripts, context, next));
        if (steps.back().kind == ScriptStepKind::note ||
            steps.back().kind == ScriptStepKind::stopped) {
            flow = next;
            return steps;
        }
    }
    throw std::runtime_error("Script did not yield within its command budget");
}

ScriptLookahead inspect_next_note(const MusicScripts& scripts, const ScriptContext& context,
                                  const ScriptFlow& flow, ScriptArticulation articulation,
                                  std::size_t command_limit) {
    validate(scripts, context, flow);
    require(command_limit > 0 && command_limit <= 65536, "Invalid lookahead command budget");
    auto cursor = flow.position;
    auto slot = flow.loop_slot;
    // Setup includes the private cursor/loop selection and first scan entry.
    // Aggregate FFVI path durations are derived in LOOKAHEAD_WORK.md. The bounded
    // scan keeps the total below u32 even at the maximum command budget.
    std::uint32_t work = 46;
    for (std::size_t i = 0; i < command_limit; ++i) {
        const auto instruction = scripts.read(context.song, cursor);
        if (instruction.is_note() || instruction.code == 0xeb)
            return {instruction.code, articulation, work + (instruction.is_note() ? 34U : 54U)};
        advance(cursor, instruction);
        switch (static_cast<MusicCommand>(instruction.code)) {
        case MusicCommand::jump:
            work += 154;
            cursor = scripts.song(context.song).branch(word(instruction));
            break;
        case MusicCommand::loop_end:
            if (flow.loops[slot].remaining == 1) {
                work += slot == 0 ? 178 : 160;
                slot = previous_slot(slot);
            } else {
                work += flow.loops[slot].remaining == 0 ? 170 : 178;
                cursor = loop_start(flow.loops[slot]);
            }
            break;
        case MusicCommand::loop_ending:
            if (iteration(context, flow, slot) == instruction.operands[0]) {
                if (flow.loops[slot].remaining == 1) {
                    work += slot == 0 ? 292 : 282;
                    slot = previous_slot(slot);
                } else
                    work += 252;
                cursor = scripts.song(context.song).branch(word(instruction, 1));
            } else
                work += 186;
            break;
        case MusicCommand::slur_off_marker:
            work += context.kind == SequenceKind::music ? 156 : 152;
            articulation.slur = false;
            break;
        case MusicCommand::drum_roll_off_marker:
            work += context.kind == SequenceKind::music ? 168 : 164;
            articulation.drum_roll = false;
            break;
        case MusicCommand::sample:
            work += context.kind == SequenceKind::music ? 264 : 256;
            articulation = {};
            // The original lookahead forgets to skip this command's operand.
            // Re-scan the sample number as a token, including its note meaning.
            --cursor.offset;
            break;
        case MusicCommand::effect_a:
        case MusicCommand::effect_b:
            require(scripts.effect(instruction.operands[0], instruction.code == 0xea).has_value(),
                    "Null effect target in note lookahead");
            return {0xeb, articulation, work + (instruction.code == 0xe9 ? 344U : 348U)};
        default:
            work += 170U + 24U * instruction.operand_count;
            break; // Only the original lookahead subset has side effects.
        }
    }
    throw std::runtime_error("Note lookahead did not yield within its command budget");
}
} // namespace ff6
