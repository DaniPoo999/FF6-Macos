#include "ff6/driver_poll.hpp"
#include <limits>
#include <stdexcept>

namespace ff6 {
DriverDispatchWork driver_dispatch_work(DriverRequestFamily family) {
    // Aggregate native work at semantic boundaries, verified independently from
    // handler execution. Stable request parameters do not affect these costs.
    // See POLL_WORK.md for evidence and excluded transport/handler behavior.
    switch (family) {
    case DriverRequestFamily::allocation:
        return {66, 140};
    case DriverRequestFamily::system_effect:
        return {66, 100};
    case DriverRequestFamily::mix_controls:
        return {66, 146};
    case DriverRequestFamily::playback_controls:
        return {66, 142};
    }
    throw std::invalid_argument("Invalid driver request family");
}
DriverCommandPoll complete_driver_command_poll(SoundTime started_at,
                                               std::optional<DriverHandlerWork> handled) {
    // The call is shared by idle and post-music inspection. Handler work already
    // includes its return; appending the empty-path return would double-count it.
    // See MUSIC_POLL_PUBLICATION.md and POLL_WORK.md for the bounded stable path.
    constexpr std::uint32_t entry = 16, empty_return = 10;
    std::optional<DriverDispatchWork> dispatch;
    auto duration = entry + empty_driver_command_work + empty_return;
    if (handled) {
        if (handled->duration == 0)
            throw std::invalid_argument("Missing native command handler work");
        dispatch = driver_dispatch_work(handled->family);
        duration = entry + dispatch->duration() + handled->duration;
    }
    if (started_at > std::numeric_limits<SoundTime>::max() - duration)
        throw std::invalid_argument("Driver command poll exceeds sound-time range");
    DriverCommandPoll result{started_at + entry, started_at + duration, {}, {}, false};
    if (dispatch) {
        result.accepted_at = result.inspected_at + dispatch->acceptance;
        result.handler_at = result.inspected_at + dispatch->duration();
        result.abandon_service = handled->abandon_service;
    }
    return result;
}
DriverIdlePoll poll_idle_driver(DriverClock& clock, SoundTime started_at,
                                std::optional<DriverHandlerWork> handled) {
    if (handled && handled->abandon_service)
        throw std::invalid_argument("Abandoned handler cannot sample idle eligibility");
    const auto completed_at = complete_driver_command_poll(started_at, handled).completed_at;
    // Validate the entire possible interval before consuming clock backlog.
    if (completed_at > std::numeric_limits<SoundTime>::max() - 14)
        throw std::invalid_argument("Driver idle poll exceeds sound-time range");
    const auto sampled_at = completed_at + 6;
    const auto opportunity = clock.poll(sampled_at);
    // A due service leaves the idle loop earlier than another empty poll.
    return {sampled_at, sampled_at + (opportunity.due() ? 4U : 8U), opportunity};
}
} // namespace ff6
