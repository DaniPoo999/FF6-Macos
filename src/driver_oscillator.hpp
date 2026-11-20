#pragma once
#include "ff6/driver_controls.hpp"
#include "rom-materials/audio_oscillator_initial_gain.macros.inc"

namespace ff6::detail {
// Both original initializers write the vibrato gain counter in this revision.
// Restarting a note uses the stored maximum rate, without recalculating it.
inline void restart_oscillator(DriverOscillator& oscillator, std::uint8_t& vibrato_gain) {
    oscillator.delay_remaining = oscillator.delay;
    oscillator.period_remaining = oscillator.period;
    auto step = oscillator.delay == 0 ? oscillator.maximum_step : oscillator.maximum_step / 4U;
    if ((oscillator.shape & 0xc0U) == 0x80U)
        step = (65536U - step) & 65535U;
    oscillator.step = static_cast<std::uint16_t>(step);
    oscillator.position = 0;
    vibrato_gain = oscillator.delay == 0 ? 0 : FF6_ROM_VALUE_AUDIO_OSCILLATOR_INITIAL_GAIN_0;
}
} // namespace ff6::detail
