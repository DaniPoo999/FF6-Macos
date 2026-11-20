#pragma once
#include "ff6/driver_clock.hpp"
#include <optional>

namespace ff6 {
inline constexpr std::uint16_t empty_driver_command_work = 14;
// Accepted, stable native request families. No original command bytes or ports
// enter this interface; validation and execution belong to their typed handlers.
enum class DriverRequestFamily : std::uint8_t {
    allocation, // Song or ordinary-effect allocation.
    system_effect,
    mix_controls,
    playback_controls
};
struct DriverDispatchWork {
    std::uint16_t acceptance = 0, dispatch = 0;
    std::uint32_t duration() const {
        return std::uint32_t{acceptance} + dispatch;
    }
    bool operator==(const DriverDispatchWork&) const = default;
};
// From command inspection to handler entry; excludes caller entry/handler work.
DriverDispatchWork driver_dispatch_work(DriverRequestFamily family);

struct DriverHandlerWork {
    DriverRequestFamily family;
    // Already-applied handler, including its natural return or transfer to idle.
    // Loading with an external readiness wait needs its separate two-stage API.
    std::uint16_t duration = 0;
    bool abandon_service = false;
};
struct DriverCommandPoll {
    SoundTime inspected_at = 0, completed_at = 0;
    std::optional<SoundTime> accepted_at, handler_at;
    bool abandon_service = false;
    bool operator==(const DriverCommandPoll&) const = default;
};
// From either caller's inspection boundary through an empty return or one
// stable typed handler. Consumes derived work, never advances logical state.
// This does not sample eligibility; an idle caller must do that after return.
DriverCommandPoll
complete_driver_command_poll(SoundTime started_at,
                             std::optional<DriverHandlerWork> handled = std::nullopt);

struct DriverIdlePoll {
    SoundTime sampled_at = 0, continuation = 0;
    DriverOpportunity opportunity;
    bool operator==(const DriverIdlePoll&) const = default;
};
// One idle inspection, optionally containing an already-applied returning
// handler with no immediate publication (allocation or buffered playback controls).
// Abandoning handlers and loading require their separate continuation paths.
// On a zero count,
// continuation is the next poll start; otherwise it is service-selection entry.
// The original modulo-sixteen backlog behavior belongs to DriverClock.
DriverIdlePoll poll_idle_driver(DriverClock& clock, SoundTime started_at,
                                std::optional<DriverHandlerWork> handled = std::nullopt);
} // namespace ff6
