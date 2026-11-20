#include "ff6/field_events.hpp"
#include <limits>
#include "rom-materials/field_event_background_palette_first.macros.inc"
#include "rom-materials/field_event_background_palette_end.macros.inc"
#include "rom-materials/field_event_fade_rate.macros.inc"
#include "rom-materials/field_event_song_fade_volume.macros.inc"
#include <sstream>

namespace ff6 {
void FieldEventRunner::resume_from_world(std::uint32_t entry) {
    validate_target(entry);
    if (request_ || continuation_ != EventContinuation::script)
        throw std::logic_error("Cannot resume an outstanding field operation from world script");
    position_ = entry;
}
namespace {
void valid_flag(std::uint16_t identity) {
    if (identity >= 0x700)
        throw std::invalid_argument("Field event flag is outside the seven owned pages");
}
void valid_object(std::uint8_t object) {
    // 0..48 are direct objects; 49..52 refer to current party slots. Resolution
    // belongs to the field owner, including the original absent-slot fallback.
    if (object > 0x34)
        throw std::invalid_argument("Invalid field event object reference");
}
void require(bool valid, const char* message) {
    if (!valid)
        throw std::invalid_argument(message);
}
} // namespace

ByteView field_event_script(const ContentPack& pack) {
    const auto script = pack.get("data_ca0000_field_event_scripts");
    require(script.size() == 189934, "Invalid field event resource length");
    return script;
}
FieldEventRunner::FieldEventRunner(ByteView script, std::uint32_t entry) : position_(entry) {
    require(!script.empty() && script.size() <= 0x1000000, "Invalid field event script extent");
    require(entry < script.size(), "Field event entry outside its resource");
    script_.assign(script.begin(), script.end());
    stack_[0].start = entry;
}
FieldEventRunner FieldEventRunner::parked(ByteView script, std::uint32_t entry) {
    FieldEventRunner runner(script, entry);
    require(script[entry] == 0xff, "Parked field event needs an idle command");
    runner.depth_ = 0;
    return runner;
}
void FieldEventRunner::begin_object_event(std::uint32_t entry) {
    validate_target(entry);
    require(!request_ && !return_pending_ && !finished_ &&
                continuation_ == EventContinuation::script && script_[position_] == 0xff,
            "Object event requires idle event ownership");
    stack_[0] = {FrameKind::call, entry, position_, 1};
    root_idle_ = position_;
    depth_ = 1;
    position_ = entry;
}
void FieldEventRunner::begin_contact_event(std::uint32_t entry, std::uint32_t idle_entry) {
    validate_target(entry);
    validate_target(idle_entry);
    require(script_[idle_entry] == 0xff, "Contact return needs an idle command");
    require(!request_ && !return_pending_ && continuation_ == EventContinuation::script,
            "Contact cannot interrupt an in-flight native event operation");
    // Synchronous dialogue uses an ordinary script subroutine, not a global
    // wait bit. Replacing the root discards that continuation, while leaving
    // the actual dialogue window and the global pause/object/fade waits alone.
    if (wait_ == Wait::dialogue_close)
        wait_ = Wait::none;
    stack_[0] = {FrameKind::call, entry, idle_entry, 1};
    root_idle_ = idle_entry;
    depth_ = 1;
    position_ = entry;
    finished_ = false;
    scroll_wait_ = true;
}
bool FieldEventRunner::begin_map_startup(std::uint32_t entry, EventFlagAccess& flags) {
    validate_target(entry);
    require(!request_ && !return_pending_ && !finished_,
            "Map startup requires a returned, unfinished event visit");
    if (script_[entry] == 0xfe)
        return false;
    require(depth_ + 2 <= stack_.size(), "Map startup nesting overflow");
    flags.set(0x1cf, true);
    stack_[depth_++] = {FrameKind::map_startup, entry, position_, 1, continuation_};
    continuation_ = EventContinuation::startup_call;
    return true;
}
bool FieldEventRunner::returning_to(std::uint32_t script_offset) const {
    validate_target(script_offset);
    if (request_ || depth_ == 0)
        return false;
    const auto& frame = stack_[depth_ - 1];
    const bool returning = continuation_ == EventContinuation::startup_return ||
                           (continuation_ == EventContinuation::script &&
                            script_[position_] == 0xfe && frame.kind == FrameKind::call &&
                            (depth_ == 1 || stack_[depth_ - 2].kind != FrameKind::map_startup));
    return returning && frame.resume_continuation == EventContinuation::script &&
           frame.resume == script_offset;
}
void FieldEventRunner::validate_target(std::uint32_t target) const {
    require(target < script_.size(), "Field event target outside its resource");
}
std::uint32_t FieldEventRunner::following(std::size_t length) const {
    (void)slice(script_, position_, length);
    const auto next = position_ + static_cast<std::uint32_t>(length);
    validate_target(next);
    return next;
}
EventYield FieldEventRunner::request(EventOperation operation, std::size_t length,
                                     Completion completion) {
    const auto next = completion == Completion::finish ? position_ : following(length);
    require(next_request_ != std::numeric_limits<std::uint64_t>::max(),
            "Field event request identity exhausted");
    request_ = EventRequest{next_request_++, position_, std::move(operation)};
    resume_ = next;
    completion_ = completion;
    return EventYield::operation;
}
void FieldEventRunner::complete_request(std::uint64_t identity, EventFlagAccess& flags,
                                        std::optional<bool> cutscene_skipped,
                                        std::optional<std::uint8_t> script_object,
                                        std::optional<std::uint8_t> dialogue_selection,
                                        std::optional<EventLoadResult> load_result,
                                        std::optional<std::uint8_t> random_draw) {
    if (!request_ || request_->identity != identity)
        throw std::logic_error("No matching field event request to complete");
    const bool cutscene = std::holds_alternative<EventRunCutscene>(request_->operation);
    const bool choice = std::holds_alternative<EventDialogueSelection>(request_->operation);
    const bool load = std::holds_alternative<EventLoadMenu>(request_->operation);
    require(load == load_result.has_value(), "Load-menu completion requires its actual result");
    require(!load_result || *load_result == EventLoadResult::new_game ||
                *load_result == EventLoadResult::saved_game,
            "Invalid load-menu result");
    require(choice == dialogue_selection.has_value(),
            "Only dialogue selection completion accepts a choice result");
    const bool random_branch = std::holds_alternative<EventRandomBranch>(request_->operation);
    require(random_branch == random_draw.has_value(),
            "Random branch completion requires exactly one field RNG draw");
    auto resume = resume_;
    if (random_draw && *random_draw >= 128)
        resume = read24(script_, std::size_t{position_} + 1);
    if (choice) {
        require(*dialogue_selection < 8, "Invalid dialogue branch selection");
        resume = read24(script_, std::size_t{position_} + 1 + std::size_t{*dialogue_selection} * 3);
        validate_target(resume);
    }
    require(cutscene == cutscene_skipped.has_value(),
            "Only cutscene completion requires its skipped/completed result");
    const auto* object_script = std::get_if<EventObjectScript>(&request_->operation);
    const bool wait_object = std::holds_alternative<EventWaitObject>(request_->operation);
    require((object_script != nullptr || wait_object) == script_object.has_value() &&
                (!script_object || *script_object <= 48),
            "Object operation completion requires its resolved native object");
    if (cutscene) {
        const auto scene = std::get<EventRunCutscene>(request_->operation).scene;
        require(scene != EventCutscene::the_end, "The End has no field continuation");
        // Only title/credits publish the skip flag. Other cinematics resume
        // their retained field visit without modifying that unrelated result.
        if (scene == EventCutscene::title || scene == EventCutscene::opening_credits)
            flags.set(0x2ff, *cutscene_skipped);
    }
    if (load)
        flags.set(0x2ff, *load_result == EventLoadResult::saved_game);
    position_ = resume;
    if (completion_ == Completion::dialogue_close)
        wait_ = Wait::dialogue_close;
    else if (completion_ == Completion::finish) {
        depth_ = 0;
        finished_ = !root_idle_;
        if (root_idle_) {
            position_ = *root_idle_;
            root_idle_.reset();
        }
    }
    if (completion_ == Completion::map_load || completion_ == Completion::map_load_fade ||
        completion_ == Completion::end_visit) {
        return_pending_ = true;
        if (completion_ == Completion::map_load_fade)
            wait_ = Wait::fade;
    }
    if (completion_ == Completion::startup_return) {
        continuation_ = stack_[depth_ - 1].resume_continuation;
        --depth_;
    }
    if (completion_ == Completion::stop_scroll)
        scroll_wait_ = false;
    if (wait_object || (object_script && object_script->wait)) {
        wait_ = Wait::object;
        waiting_object_ = script_object;
        // Both handlers replace the complete global wait selector. A contact
        // may have added a scroll wait since the preceding object wait began.
        scroll_wait_ = false;
    }
    request_.reset();
}
EventYield FieldEventRunner::advance(EventFlagAccess& flags, const EventInputs& input,
                                     std::size_t command_budget) {
    require(command_budget > 0, "Field event command budget must be positive");
    require(input.facing < 4, "Invalid field facing direction");
    if (request_)
        return EventYield::operation;
    if (finished_)
        return EventYield::finished;
    if (return_pending_) {
        return_pending_ = false;
        return EventYield::frame_wait;
    }
    if (input.map_load_pending)
        return EventYield::frame_wait;
    for (std::size_t used = 0; used < command_budget; ++used) {
        // ContinueEvent runs after immediate commands too. A newly installed
        // pause consumes its first decrement in this SAME field visit.
        if (pause_ != 0) {
            --pause_;
            return EventYield::frame_wait;
        }
        if (!input.objects_sorted || !input.backgrounds_ready)
            return EventYield::frame_wait;
        for (std::uint16_t direction = 0; direction < 4; ++direction)
            flags.set(static_cast<std::uint16_t>(0x1b0U + direction), direction == input.facing);
        flags.set(0x1b4, input.confirm_down);
        const bool object_wait = wait_ == Wait::object;
        if (object_wait) {
            if (input.object_script_active)
                return EventYield::frame_wait;
            wait_ = Wait::none;
            waiting_object_.reset();
        } else if (wait_ == Wait::dialogue_close) {
            if (input.dialogue_open)
                return EventYield::frame_wait;
            wait_ = Wait::none;
        } else if (wait_ == Wait::fade) {
            if (!input.fade_active)
                wait_ = Wait::none;
            // The original returns even on the iteration which clears this wait.
            return EventYield::frame_wait;
        }
        // Resolving an object wait jumps directly to command execution in the
        // original; it bypasses the scroll gate for that one command visit.
        if (!object_wait && scroll_wait_) {
            if (!input.scroll_arrived)
                return EventYield::frame_wait;
            return request(EventStopScroll{}, 0, Completion::stop_scroll);
        }
        if (continuation_ != EventContinuation::script) {
            const auto frame = stack_[depth_ - 1];
            require(frame.kind == FrameKind::map_startup, "Invalid startup continuation frame");
            switch (continuation_) {
            case EventContinuation::startup_call:
                require(depth_ < stack_.size(), "Map startup call nesting overflow");
                stack_[depth_++] = {FrameKind::call, frame.start, position_, 1};
                position_ = frame.start;
                continuation_ = EventContinuation::script;
                break;
            case EventContinuation::startup_release:
                flags.set(0x1cf, false);
                continuation_ = EventContinuation::startup_yield;
                break;
            case EventContinuation::startup_yield:
                continuation_ = EventContinuation::startup_return;
                return EventYield::frame_wait;
            case EventContinuation::startup_return:
                if (depth_ == 1) {
                    require(next_request_ != std::numeric_limits<std::uint64_t>::max(),
                            "Field event request identity exhausted");
                    request_ =
                        EventRequest{next_request_++, std::nullopt, EventRestorePartyMovement{}};
                    resume_ = frame.resume;
                    completion_ = Completion::startup_return;
                    return EventYield::operation;
                }
                position_ = frame.resume;
                continuation_ = frame.resume_continuation;
                --depth_;
                break;
            case EventContinuation::script:
                break;
            }
            continue;
        }
        const auto code = script_.at(position_);
        const auto bytes = [&](std::size_t count) { return slice(script_, position_, count); };
        if (code <= 0x34) {
            const auto descriptor = bytes(2)[1];
            const std::size_t length = 2U + (descriptor & 127U);
            return request(EventObjectScript{code, position_ + 2U, (descriptor & 128U) != 0},
                           length);
        } else if (code == 0xb8 || code == 0xb9) {
            const auto identity = bytes(2)[1];
            const auto next = following(2);
            flags.set_battle(identity, code == 0xb8);
            position_ = next;
        } else if (code == 0xb7) {
            const auto command = bytes(5);
            const auto target = read24(command, 2);
            validate_target(target);
            position_ = flags.test_battle(command[1]) ? following(5) : target;
        } else if (code >= 0xc0 && code <= 0xcf) {
            const unsigned count = (code & 7U) + 1U;
            const auto command = bytes(4 + count * 2);
            const auto target = read24(command, 1 + count * 2);
            validate_target(target);
            for (unsigned i = 0; i < count; ++i)
                valid_flag(read16(command, 1 + i * 2) & 0x7fffU);
            const bool all = code >= 0xc8;
            bool matched = all;
            for (unsigned i = 0; i < count; ++i) {
                const auto condition = read16(command, 1 + i * 2);
                const bool value = flags.test(condition & 0x7fffU) == ((condition & 0x8000U) != 0);
                if (value != all) {
                    matched = value;
                    break;
                }
            }
            position_ = matched ? target : following(command.size());
        } else if (code >= 0xd0 && code <= 0xdd) {
            const auto operand = bytes(2)[1];
            const auto next = following(2);
            const auto flag = static_cast<std::uint16_t>(((code - 0xd0U) / 2U) * 256U + operand);
            flags.set(flag, (code & 1U) == 0);
            position_ = next;
        } else if (code == 0xde) {
            return request(EventActivePartyMembers{}, 1);
        } else if (code >= 0xdf && code <= 0xe1) {
            const auto source = code == 0xdf ? CharacterMaskSource::enabled_objects
                              : code == 0xe0 ? CharacterMaskSource::initialized
                                             : CharacterMaskSource::available;
            return request(EventCharacterMask{source}, 1);
        } else if (code == 0xe2) {
            return request(EventPartyLeader{}, 1);
        } else if (code == 0xe3) {
            return request(EventCharacterMask{CharacterMaskSource::all_parties}, 1);
        } else if (code == 0xe4) {
            return request(EventActivePartyNumber{}, 1);
        } else if (code == 0xb2 || code == 0xb3) {
            const auto command = bytes(code == 0xb2 ? 4 : 5);
            const auto target = read24(command, code == 0xb2 ? 1 : 2);
            validate_target(target);
            const auto next = following(command.size());
            require(depth_ < stack_.size(), "Field event call nesting overflow");
            stack_[depth_++] = {FrameKind::call, target, next,
                                static_cast<std::uint8_t>(code == 0xb2 ? 1 : command[1])};
            position_ = target;
        } else if (code == 0xbd) {
            validate_target(read24(bytes(4), 1));
            return request(EventRandomBranch{}, 4);
        } else if (code == 0xbe) {
            const auto count = bytes(2)[1];
            // The original length accumulator is eight-bit. A zero or wrapped
            // length can scan beyond its table; malformed native input fails.
            require(count > 0 && count <= 84, "Invalid field event case-table length");
            const auto command = bytes(2U + 3U * count);
            const auto next = following(command.size());
            std::optional<std::uint32_t> selected;
            for (unsigned i = 0; i < count; ++i) {
                const auto entry = read24(command, 2U + i * 3U);
                if (flags.test(static_cast<std::uint16_t>(0x1a0U + (entry >> 20U)))) {
                    // Bits 18/19 are ignored by the original target consumer.
                    // The first matching record wins, including duplicate flags.
                    selected = entry & 0x3ffffU;
                    break;
                }
            }
            if (selected) {
                validate_target(*selected);
                require(depth_ < stack_.size(), "Field event case-call nesting overflow");
                stack_[depth_++] = {FrameKind::call, *selected, next, 1};
                position_ = *selected;
            } else
                position_ = next;
        } else if (code == 0xb0) {
            const auto count = bytes(2)[1];
            const auto start = following(2);
            require(depth_ < stack_.size(), "Field event loop nesting overflow");
            stack_[depth_++] = {FrameKind::loop, start, 0, count};
            position_ = start;
        } else if (code == 0xb1 || code == 0xbc) {
            const auto frame = stack_[depth_ - 1];
            require(frame.kind == FrameKind::loop, "Field event loop end without loop");
            auto remaining = frame.remaining;
            bool again;
            if (code == 0xbc) {
                const auto identity = read16(bytes(3), 1);
                valid_flag(identity);
                again = !flags.test(identity);
            } else {
                remaining = wrap8(unsigned{remaining} + 255U);
                again = remaining != 0;
            }
            const auto next = again ? frame.start : following(code == 0xb1 ? 1 : 3);
            if (again)
                stack_[depth_ - 1].remaining = remaining;
            else
                --depth_;
            position_ = next;
        } else if (code == 0xfe) {
            const auto frame = stack_[depth_ - 1];
            require(frame.kind == FrameKind::call, "Field event return inside an unclosed loop");
            const auto remaining = wrap8(unsigned{frame.remaining} + 255U);
            if (remaining != 0) {
                stack_[depth_ - 1].remaining = remaining;
                position_ = frame.start;
            } else if (depth_ == 1)
                return request(EventRestorePartyMovement{}, 1, Completion::finish);
            else {
                position_ = frame.resume;
                --depth_;
                if (stack_[depth_ - 1].kind == FrameKind::map_startup)
                    continuation_ = EventContinuation::startup_release;
            }
        } else if (code == 0xb4 || code == 0xb5 || (code >= 0x91 && code <= 0x95)) {
            std::uint16_t duration;
            std::size_t length = 1;
            if (code == 0xb4 || code == 0xb5) {
                duration = static_cast<std::uint16_t>(bytes(2)[1] * (code == 0xb4 ? 1U : 15U));
                length = 2;
            } else
                duration = static_cast<std::uint16_t>(code == 0x95 ? 120U : (code - 0x90U) * 15U);
            position_ = following(length);
            pause_ = duration;
        } else if (code == 0xe7) {
            return request(EventPortrait{bytes(2)[1]}, 2);
        } else if (code >= 0xe8 && code <= 0xeb) {
            const auto command = bytes(4);
            require(command[1] < 8, "Event variable is outside the eight owned words");
            return request(EventVariable{static_cast<EventVariableChange>(code - 0xe8),
                                          command[1], read16(command, 2)}, 4);
        } else if (code == 0xef || code == 0xf0 || code == 0xf1) {
            const auto command = bytes(code == 0xf0 ? 2 : 3);
            const auto volume = code == 0xef ? command[2] : code == 0xf1 ? FF6_ROM_VALUE_FIELD_EVENT_SONG_FADE_VOLUME_0 : 255;
            return request(EventPlaySong{static_cast<std::uint8_t>(command[1] & 127U),
                                         static_cast<std::uint8_t>(volume),
                                         (command[1] & 128U) != 0,
                                         code == 0xf1 ? std::optional{command[2]} : std::nullopt},
                           command.size());
        } else if (code == 0xf2) {
            return request(EventFadeSong{bytes(2)[1]}, 2);
        } else if (code == 0xf3) {
            return request(EventResumeSong{bytes(2)[1]}, 2);
        } else if (code == 0xf7) {
            // Advance conditional branches in the current song without
            // restarting it or replacing the map's return-song identity.
            return request(EventContinueSong{}, 1);
        } else if (code == 0xf9) {
            // C0/B8BA waits for equality with the song's authored cue counter.
            // It neither advances the song nor substitutes an elapsed-time wait.
            if (input.music_position != std::optional{bytes(2)[1]})
                return EventYield::frame_wait;
            position_ = following(2);
        } else if (code == 0xfa) {
            if (!input.music_finished)
                return EventYield::frame_wait;
            position_ = following(1);
        } else if (code == 0xf4) {
            // PlaySfx selects ordinary game effects at centered pan. The field
            // owner queues playback before continuing this same event visit.
            return request(EventGameSoundEffect{bytes(2)[1]}, 2);
        } else if (code == 0xf6) {
            const auto command = bytes(4);
            if (command[1] == 0x10 || command[1] == 0x11)
                // Both reach PlaySong's duplicate/suspension policy. Control
                // 11 selects the song's alternate eight channel entry points.
                return request(EventPlaySong{command[2], command[3], false, {}, false,
                                             command[1] == 0x11}, 4);
            // Sound_main C5/05C7 and SPC 0D6F distinguish stop-all, music
            // and effects. The two following operand bytes are ignored.
            if (command[1] >= 0xf0 && command[1] <= 0xf2)
                return request(EventStopSound{command[1] == 0xf0 ? PlaybackGroup::music_and_effects
                    : command[1] == 0xf1 ? PlaybackGroup::music : PlaybackGroup::effects}, 4);
            // EventCmd_f6 forwards duration/target in that order. Interrupt_83
            // owns the ordinary effects' retained pan, including immediate changes.
            if (command[1] == 0x83)
                return request(EffectPanRamp{command[2], command[3]}, 4);
            if (command[1] == 0x84 || command[1] == 0x85)
                return request(EventMusicAdjustment{command[1] == 0x85, command[2],
                    static_cast<std::int8_t>(signed8(command[3]))}, 4);
            // The data-language wrapper forwards sound controls. Decode the
            // supported volume family into an intention, never a device write.
            if (command[1] < 0x80 || command[1] > 0x82) {
                std::ostringstream message;
                message << "Unsupported field sound control 0x" << std::hex << unsigned{command[1]}
                        << " at script offset 0x" << position_;
                throw std::runtime_error(message.str());
            }
            const auto group = command[1] == 0x80   ? PlaybackGroup::music_and_effects
                               : command[1] == 0x81 ? PlaybackGroup::music
                                                    : PlaybackGroup::effects;
            return request(PlaybackVolumeRamp{group, command[2], command[3]}, 4);
        } else if (code == 0xfb || code == 0xfd) {
            position_ = following(1);
            return EventYield::frame_wait;
        } else if (code == 0xff) {
            return EventYield::idle;
        } else if (code == 0x49 || code == 0x4a) {
            if (code == 0x49 ? input.dialogue_open : !input.dialogue_text_ready)
                return EventYield::frame_wait;
            position_ = following(1);
        } else if (code == 0x50 || code == 0x51) {
            const auto data = bytes(code == 0x50 ? 2 : 4);
            // The all-background command owns its ROM-selected range; the
            // explicit-range command supplies both indices in script data.
            const auto first = code == 0x50 ? FF6_ROM_VALUE_FIELD_EVENT_BACKGROUND_PALETTE_FIRST_0 / 2 : (data[2] & 127U);
            const auto last = code == 0x50 ? FF6_ROM_VALUE_FIELD_EVENT_BACKGROUND_PALETTE_END_0 / 2 - 1 : (data[3] & 127U);
            // EventCmd_51/53 double indices in an eight-bit register. The
            // high bit is discarded; the color walker wraps within 128 colors.
            return request(EventBackgroundPalette{data[1], static_cast<std::uint8_t>(first),
                           static_cast<std::uint8_t>(last)}, data.size(), Completion::end_visit);
        } else if (code == 0x52 || code == 0x53) {
            const auto data = bytes(code == 0x52 ? 2 : 4);
            const auto first = code == 0x52 ? 0 : (data[2] & 127U);
            const auto last = code == 0x52 ? 127 : (data[3] & 127U);

            return request(EventSpritePalette{data[1], static_cast<std::uint8_t>(first),
                           static_cast<std::uint8_t>(last)}, data.size(), Completion::end_visit);
        } else if (code == 0x77) {
            const auto object = bytes(2)[1];
            valid_object(object);
            return request(EventNormalizeLevel{object}, 2);
        } else if (code == 0x79) {
            const auto data = bytes(4);
            require(data[1] < 8 && read16(data, 2) < 512, "Invalid party map");
            return request(EventPartyMap{data[1], read16(data, 2)}, 4);
        } else if (code == 0x7a) {
            const auto data = bytes(5);
            valid_object(data[1]);
            return request(EventObjectEvent{data[1], read24(data, 2)}, 5);
        } else if (code == 0x7b || code == 0x82) {
            return request(EventDefaultParty{code == 0x7b}, 1);
        } else if (code == 0x7c || code == 0x7d) {
            valid_object(bytes(2)[1]);
            return request(EventContactActivation{bytes(2)[1], code == 0x7c}, 2);
        } else if (code == 0x7e) {
            const auto data = bytes(3);
            return request(EventPartyPosition{data[1], data[2]}, 3, Completion::end_visit);
        } else if (code == 0x61) {
            const auto data = bytes(4);
            require(data[1] < 8 && data[2] <= data[3], "Invalid field palette filter");
            return request(EventPaletteFilter{data[1], data[2], data[3]}, 4);
        } else if (code == 0xa6) {
            return request(EventPyramid{}, 1);
        } else if (code == 0xa7) {
            const auto object = bytes(2)[1];
            require(object <= 48, "Pyramid requires a direct field object");
            return request(EventPyramid{object}, 2);
        } else if (code == 0x54) {
            return request(EventRestoreColorMath{}, 1);
        } else if (code == 0x56 || code == 0x57) {
            return request(EventFixedColor{bytes(2)[1], code == 0x57}, 2);
        } else if (code == 0x55) {
            return request(EventScreenFlash{static_cast<std::uint8_t>(bytes(2)[1] & 224U)}, 2);
        } else if ((code >= 0x5d && code <= 0x5f) || (code >= 0x70 && code <= 0x72)) {
            const auto data = bytes(3);
            const bool alternate = code < 0x70;
            const auto rate = [alternate](std::uint8_t value) {
                // The older commands extend the complemented magnitude: FF
                // becomes -1, whereas the newer family makes it -16.
                const int signed_value = value < 128 ? int(value) : int(value) - 256;
                return static_cast<std::int16_t>(signed_value * 16 +
                                                 (alternate && value >= 128 ? 15 : 0));
            };
            return request(
                EventBackgroundScroll{static_cast<std::uint8_t>(code - (alternate ? 0x5d : 0x70)),
                                      rate(data[1]), rate(data[2])},
                3);
        } else if (code == 0x5c) {
            position_ = following(1);
            wait_ = Wait::fade;
        } else if (code == 0x48 || code == 0x4b) {
            const auto value = read16(bytes(3), 1);
            if (code == 0x4b)
                require(depth_ < stack_.size(), "Field event dialogue nesting overflow");
            return request(EventDialogue{static_cast<std::uint16_t>(value & 0x1fffU),
                                         (value & 0x8000U) != 0, (value & 0x4000U) != 0},
                           3,
                           code == 0x4b ? Completion::dialogue_close : Completion::continue_script);
        } else if (code == 0xb6) {
            return request(EventDialogueSelection{}, 1);
        } else if (code == 0xab) {
            return request(EventLoadMenu{}, 1);
        } else if (code == 0x4f) {
            return request(EventRestartGame{}, 1, Completion::end_visit);
        } else if (code == 0xa2) {
            return request(EventClearOverlay{}, 1);
        } else if (code == 0xa0) {
            const auto data = bytes(6);
            const auto entry = read24(data, 3) & 0x3ffffU;
            if (entry)
                validate_target(entry);
            return request(EventSetTimer{static_cast<std::uint8_t>((data[5] >> 2U) & 3U),
                                         read16(data, 1), entry, (data[5] & 0x80U) != 0,
                                         (data[5] & 0x40U) != 0, (data[5] & 0x10U) != 0,
                                         (data[5] & 0x20U) != 0}, 6);
        } else if (code == 0xa1) {
            require(bytes(2)[1] < 4, "Invalid field timer identity");
            return request(EventResetTimer{bytes(2)[1]}, 2);
        } else if (code == 0x9c) {
            require(bytes(2)[1] < 16, "Invalid optimum equipment character");
            return request(EventOptimizeEquipment{bytes(2)[1]}, 2);
        } else if (code == 0x99) {
            const auto data = bytes(4);
            const auto count = static_cast<std::uint8_t>(data[1] & 0x7fU);
            require(count >= 1 && count <= 3, "Invalid party selection count");
            return request(EventPartyMenu{count, (data[1] & 128U) != 0, read16(data, 2)},
                           4, Completion::end_visit);
        } else if (code == 0x98) {
            const auto character = bytes(2)[1];
            valid_object(character);
            return request(EventNameMenu{character}, 2, Completion::end_visit);
        } else if (code == 0x65) {
            const auto data = bytes(3);
            require(data[1] < 8, "Invalid field background animation group");
            return request(EventBackgroundRate{data[1], data[2]}, 3);
        } else if (code == 0x63) {
            return request(EventFlashlight{static_cast<std::uint8_t>((bytes(2)[1] & 31U) * 2U)}, 2);
        } else if (code == 0x62) {
            return request(EventScreenMosaic{bytes(2)[1]}, 2);
        } else if (code == 0x4c || code == 0x4d) {
            const auto command = bytes(3);
            return request(EventStartBattle{code == 0x4c ? EventBattleKind::contact
                                                         : EventBattleKind::scripted,
                                            command[1], static_cast<std::uint8_t>(command[2] & 63U),
                                            (command[2] & 128U) == 0, (command[2] & 64U) == 0},
                           3, Completion::end_visit);
        } else if (code == 0x4e) {
            return request(EventStartBattle{EventBattleKind::random}, 1, Completion::end_visit);
        } else if (code == 0x8e) {
            return request(EventStartBattle{EventBattleKind::treasure}, 1, Completion::end_visit);
        } else if (code == 0xbf || code == 0xbb) {
            return request(EventRunCutscene{code == 0xbf ? EventCutscene::ending_airship : EventCutscene::the_end}, 1);
        } else if (code == 0xba) {
            return request(EventRunCutscene{EventCutscene::ending, bytes(2)[1]}, 2);
        } else if (code == 0xa8 || code == 0xa9 || code == 0xaa || code == 0xad || code == 0xae) {
            return request(EventRunCutscene{code == 0xa9 ? EventCutscene::title
                                           : code == 0xaa ? EventCutscene::opening_credits
                                           : code == 0xad ? EventCutscene::world_ruin
                                           : code == 0xa8 ? EventCutscene::floating_continent
                                                          : EventCutscene::mine_cart},
                           1);
        } else if (code == 0x7f || code == 0x40 || code == 0x3f || code == 0x37 || code == 0x43) {
            const auto command = bytes(3);
            valid_object(command[1]);
            if (code == 0x7f || code == 0x40)
                require(command[2] < 64, "Invalid event character name/property identity");
            if (code == 0x3f)
                require(command[2] < 8, "Invalid event party identity");
            switch (code) {
            case 0x7f:
                return request(EventCharacterName{command[1], command[2]}, 3);
            case 0x40:
                return request(EventCharacterProperties{command[1], command[2]}, 3);
            case 0x3f:
                return request(EventCharacterParty{command[1], command[2]}, 3);
            case 0x37:
                return request(EventObjectGraphics{command[1], command[2]}, 3);
            default:
                return request(EventObjectPalette{command[1], command[2]}, 3);
            }
        } else if (code == 0x35) {
            const auto object = bytes(2)[1];
            valid_object(object);
            return request(EventWaitObject{object}, 2);
        } else if (code == 0x36 || code == 0x78) {
            const auto object = bytes(2)[1];
            valid_object(object);
            return request(EventObjectCollision{object, code == 0x78}, 2);
        } else if (code == 0x44) {
            const auto command = bytes(3);
            valid_object(command[1]);
            return request(EventObjectVehicle{command[1],
                                              static_cast<std::uint8_t>((command[2] >> 5U) & 3U),
                                              (command[2] & 128U) != 0},
                           3);
        } else if (code == 0x38 || code == 0x39) {
            return request(EventCameraLock{code == 0x38}, 1);
        } else if (code == 0x45) {
            return request(EventSortObjects{}, 1);
        } else if (code == 0x58) {
            return request(EventScreenShake{bytes(2)[1]}, 2);
        } else if (code == 0x59 || code == 0x5a || code == 0x96 || code == 0x97) {
            const bool fixed = code >= 0x96;
            return request(EventScreenFade{fixed ? std::uint8_t{FF6_ROM_VALUE_FIELD_EVENT_FADE_RATE_0} : bytes(2)[1],
                                           code == 0x5a || code == 0x97},
                           fixed ? 1 : 2);
        } else if (code == 0x5b) {
            return request(EventStopScreenFade{}, 1);
        } else if (code == 0x3a || code == 0x3b) {
            return request(EventPartyControl{code == 0x3a}, 1);
        } else if (code == 0x3d) {
            const auto object = bytes(2)[1];
            valid_object(object);
            return request(EventCreateObject{object}, 2);
        } else if (code == 0x42) {
            const auto object = bytes(2)[1];
            valid_object(object);
            return request(EventHideObject{object}, 2);
        } else if (code == 0x41 || code == 0x3e) {
            const auto object = bytes(2)[1];
            valid_object(object);
            if (code == 0x41)
                return request(EventShowObject{object}, 2);
            return request(EventDeleteObject{object}, 2);
        } else if (code == 0x3c) {
            const auto command = bytes(5);
            EventPartyObjects party{};
            for (std::size_t i = 0; i < party.objects.size(); ++i) {
                const auto object = command[i + 1];
                // The authored empty form (all FF) releases the temporary
                // selectors; the ordinary object sorter then visits the party.
                if (object == 0xff || (i != 0 && object >= 128))
                    break;
                valid_object(object);
                party.objects[i] = object;
            }
            return request(party, 5);
        } else if (code == 0x46) {
            const auto party = bytes(2)[1];
            require(party < 8, "Invalid active event party");
            return request(EventActivateParty{party}, 2);
        } else if (code == 0x47) {
            return request(EventUpdateParty{}, 1);
        } else if (code == 0x80 || code == 0x81) {
            return request(EventInventoryItem{bytes(2)[1], code == 0x80}, 2);
        } else if (code == 0x84 || code == 0x85) {
            return request(EventInventoryGil{read16(bytes(3), 1), code == 0x84}, 3);
        } else if (code == 0x86 || code == 0x87) {
            const auto attack = bytes(2)[1];
            require(attack >= 54 && attack < 81, "Invalid event esper identity");
            return request(EventInventoryEsper{static_cast<std::uint8_t>(attack - 54U), code == 0x86}, 2);
        } else if (code == 0x8f || code == 0x90) {
            return request(code == 0x8f ? EventLearnTechniques{255, 0}
                                        : EventLearnTechniques{0, 128}, 1);
        } else if (code == 0x8d) {
            const auto character = bytes(2)[1];
            require(character < 16, "Equipment removal outside native character roster");
            return request(EventRemoveEquipment{character}, 2);
        } else if (code == 0x8b || code == 0x8c) {
            const auto command = bytes(3);
            valid_object(command[1]);
            const unsigned index = command[2] & 127U;
            require(index == 127 || index < (code == 0x8b ? 8U : 7U), "Unowned field HP/MP amount");
            return request(EventCharacterVital{command[1], command[2], code == 0x8c}, 3);
        } else if (code >= 0x88 && code <= 0x8a) {
            const auto command = bytes(4);
            valid_object(command[1]);
            const auto encoded = read16(command, 2);
            const auto mask =
                code == 0x88 ? static_cast<std::uint16_t>(encoded ^ 0xffffU) : encoded;
            const auto change = code == 0x88   ? EventStatusChange::remove
                                : code == 0x89 ? EventStatusChange::add
                                               : EventStatusChange::toggle;
            return request(EventCharacterStatus{command[1], change, mask}, 4);
        } else if (code == 0x60) {
            const auto command = bytes(3);
            require(command[1] < 16 && command[2] < 32, "Invalid field palette load");
            return request(EventLoadPalette{command[1], command[2]}, 3);
        } else if (code == 0x9d) {
            return request(EventFinalOrder{}, 1, Completion::end_visit);
        } else if (code == 0x9a) {
            return request(EventColosseumMenu{}, 1, Completion::end_visit);
        } else if (code == 0xaf) {
            return request(EventStartBattle{EventBattleKind::colosseum}, 1, Completion::end_visit);
        } else if (code == 0x9b) {
            return request(EventShop{bytes(2)[1]}, 2, Completion::end_visit);
        } else if (code == 0x6a || code == 0x6b) {
            const auto command = bytes(6);
            const auto destination = read16(command, 1);
            EventMapLoad load;
            load.destination = destination & 0x1ffU;
            load.x = command[3];
            load.y = command[4];
            load.facing = static_cast<std::uint8_t>((destination >> 12U) & 3U);
            load.set_parent = (destination & 0x200U) != 0;
            load.lower_level = (destination & 0x400U) != 0;
            load.show_name = (destination & 0x800U) != 0;
            load.world_vehicle = command[5] & 3U;
            load.keep_dimensions = (command[5] & 32U) != 0;
            load.fade_in = (command[5] & 64U) == 0;
            load.run_startup = (command[5] & 128U) != 0;
            load.fade_out = code == 0x6a;
            return request(load, 6,
                           load.fade_out ? Completion::map_load_fade : Completion::map_load);
        } else if (code == 0x73 || code == 0x74) {
            const auto header = bytes(5);
            const auto count = std::size_t{header[3]} * header[4];
            require(count != 0, "Empty field event map patch");
            const auto command = bytes(5 + count);
            const auto layer =
                static_cast<std::uint8_t>((header[2] & 128U) != 0 ? 2U : header[2] >> 6U);
            return request(EventMapEdit{layer, header[1],
                                        static_cast<std::uint8_t>(header[2] & 63U), header[3],
                                        header[4], Bytes(command.begin() + 5, command.end()),
                                        code == 0x73},
                           command.size(), Completion::end_visit);
        } else if (code == 0x75) {
            return request(EventApplyMapEdits{}, 1, Completion::end_visit);
        } else if (code == 0x6c) {
            const auto command = bytes(6);
            require(command[5] < 4, "Invalid parent-map facing direction");
            return request(EventParentMap{read16(command, 1), command[3], command[4], command[5]},
                           6);
        } else {
            std::ostringstream error;
            error << "Unsupported field event command 0x" << std::hex << unsigned{code}
                  << " at script offset 0x" << position_;
            throw std::runtime_error(error.str());
        }
    }
    return EventYield::budget;
}
} // namespace ff6
