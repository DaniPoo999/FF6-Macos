#include "ff6/driver_timeline.hpp"
#include <limits>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool value, const char* reason) {
    if (!value)
        throw std::invalid_argument(reason);
}
} // namespace
DriverTimeline::DriverTimeline(SoundTime activated_at, std::uint16_t phase)
    : clock_(activated_at, phase), available_at_(activated_at) {}

DriverIdlePoll DriverTimeline::poll_idle(std::optional<DriverHandlerWork> handled) {
    require(stage_ == Stage::idle, "Idle inspection during unfinished work");
    auto clock = clock_;
    const auto result = poll_idle_driver(clock, available_at_, handled);
    clock_ = clock;
    available_at_ = result.continuation;
    if (result.opportunity.due()) {
        selected_ = result;
        stage_ = Stage::eligible;
    }
    return result;
}

DriverTimedService DriverTimeline::begin_service(SoundTime sampled_at,
                                                 const DriverServiceOutput& requested) {
    require((stage_ == Stage::idle && sampled_at >= available_at_) ||
                (stage_ == Stage::eligible && selected_ && sampled_at == selected_->sampled_at),
            "Service eligibility sampled outside idle time");
    require(requested.begin_work && requested.buffered, "Missing service-begin preparation");
    // Untaken zero test followed by native flush selection. Full output takes
    // the second branch; echo-disabled output falls through. DRIVER_TIMELINE.md.
    const unsigned selection = requested.buffered->echo ? 28U : 24U;
    require(sampled_at <= std::numeric_limits<SoundTime>::max() - selection,
            "Service selection exceeds sound-time range");
    auto next = *this;
    const auto opportunity = selected_ ? selected_->opportunity : next.clock_.poll(sampled_at);
    require(opportunity.due(), "No service opportunity at the supplied sample time");
    DriverTimedService result;
    result.begun = sampled_at + selection;
    result.opportunity = opportunity;
    result.publication = next.publisher_.service(result.begun, requested);
    next.available_at_ = result.publication.completed_at;
    next.selected_.reset();
    next.stage_ = requested.music_poll    ? Stage::music_poll
                  : requested.script_pass ? Stage::effects
                                          : Stage::idle;
    *this = next;
    return result;
}
DriverTimedService DriverTimeline::finish_service(const DriverServiceOutput& requested,
                                                  std::optional<DriverHandlerWork> handled) {
    require(stage_ == Stage::music_poll || stage_ == Stage::effects, "No service awaiting effects");
    require(!requested.begin_work && requested.voice_pass, "Missing effects-stage preparation");
    require(!handled || stage_ == Stage::music_poll, "Command without a music inspection");
    require(!handled || !handled->abandon_service, "Abandoned handler cannot continue effects");
    auto next = *this;
    DriverTimedService result;
    result.begun = available_at_;
    if (stage_ == Stage::music_poll) {
        result.command_poll = complete_driver_command_poll(available_at_, handled);
        result.begun = result.command_poll->completed_at;
    }
    result.publication = next.publisher_.service(result.begun, requested);
    next.available_at_ = result.publication.completed_at;
    next.stage_ = Stage::idle;
    *this = next;
    return result;
}
DriverTimedPause DriverTimeline::publish_pause(const PlaybackRequests& requested) {
    require(stage_ == Stage::idle || stage_ == Stage::music_poll,
            "Pause outside a command inspection");
    auto next = *this;
    const DriverHandlerWork handled{DriverRequestFamily::playback_controls, requested.work,
                                    requested.abandon_service};
    DriverTimedPause result;
    result.command_poll = complete_driver_command_poll(available_at_, handled);
    result.publication = next.publisher_.pause(*result.command_poll.handler_at, requested);
    next.available_at_ = result.publication.completed_at;
    if (requested.abandon_service) {
        // The original transfers directly to Main. In particular it neither
        // finishes effects nor consumes the eligibility count on this path.
        next.stage_ = Stage::idle;
    } else if (stage_ == Stage::idle) {
        result.idle = poll_idle_driver(next.clock_, available_at_, handled);
        next.available_at_ = result.idle->continuation;
        if (result.idle->opportunity.due()) {
            next.selected_ = result.idle;
            next.stage_ = Stage::eligible;
        }
    } else {
        // The command inspection has already returned; finish_service must not
        // inspect another command before completing this service's effects.
        next.stage_ = Stage::effects;
    }
    *this = next;
    return result;
}
DriverLoadStart DriverTimeline::begin_load(SoundTime begun, const DriverSongOutput& requested) {
    require((stage_ == Stage::idle || stage_ == Stage::music_poll) && begun >= available_at_,
            "Loading outside a command opportunity");
    if (stage_ == Stage::music_poll) {
        // Loading has an unbounded content wait, so only derive dispatch here.
        const auto dispatch =
            16U + driver_dispatch_work(DriverRequestFamily::allocation).duration();
        require(available_at_ <= std::numeric_limits<SoundTime>::max() - dispatch &&
                    begun == available_at_ + dispatch,
                "Loading does not begin at the pending command dispatch");
    }
    auto next = *this;
    auto result = next.publisher_.begin_load(begun, requested);
    next.available_at_ = result.awaiting_content_at;
    next.stage_ = Stage::loading;
    *this = next;
    return result;
}
DriverLoadCompletion DriverTimeline::finish_load(SoundTime content_ready) {
    require(stage_ == Stage::loading, "No loading operation awaiting content");
    auto next = *this;
    auto result = next.publisher_.finish_load(content_ready, next.clock_);
    next.available_at_ = result.output.completed_at;
    next.stage_ = Stage::idle;
    *this = next;
    return result;
}
} // namespace ff6
