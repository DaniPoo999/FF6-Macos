#pragma once
#include <cstdint>
#include "rom-materials/audio_timer_period_choices.macros.inc"

namespace ff6 {
// Integer sound time, in 1/64 of a canonical output sample. This is independent
// of the device rate, callback size and wall clock. See DRIVER_CLOCK.md.
using SoundTime = std::uint64_t;
enum class DriverRate { normal, fast_forward };

struct DriverOpportunity {
    // Original backlog wraps modulo sixteen. A nonzero count authorizes ONE
    // service, never a loop of elapsed services; a wrapped zero authorizes none.
    std::uint8_t elapsed = 0;
    bool due() const {
        return elapsed != 0;
    }
    bool operator==(const DriverOpportunity&) const = default;
};

// Game-specific periodic eligibility; it does not choose when work can poll.
// Loading, work completion and command-arrival deadlines belong to the caller.
class DriverClock {
  public:
    // The first four-sample boundary must be 1..256 ticks after activation.
    // Its phase is an explicit startup contract, not inferred from host time.
    DriverClock(SoundTime activated_at, std::uint16_t until_four_sample_boundary);
    DriverOpportunity poll(SoundTime now);
    // Song loading clears backlog without changing the periodic phase. Return
    // the discarded count for diagnostics; it must not trigger service work.
    std::uint8_t discard_after_song_load(SoundTime now);
    // F6 restarts the cadence even when selecting the current rate. It clears
    // backlog but retains the four-sample phase shared with normal cadence.
    void restart(SoundTime now, DriverRate rate);

  private:
    void advance(SoundTime now);
    SoundTime last_;
    std::uint16_t boundary_phase_;
    std::uint16_t period_ = FF6_ROM_VALUE_AUDIO_TIMER_PERIOD_CHOICES_0 * 256U;
    std::uint16_t remaining_;
    std::uint8_t pending_ = 0;
};
} // namespace ff6
