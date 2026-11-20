#include "ff6/game_time.hpp"
#include "ff6/bytes.hpp"

namespace ff6 {
void advance_game_time(GameTime& time, std::uint8_t& frames) {
    // The comparison precedes the final increment: a freshly cleared counter
    // takes 61 calls to reach its first second. See GAME_TIME.md.
    if (frames == 60) {
        frames = 0;
        if (time.seconds != 59)
            time.seconds = wrap8(unsigned{time.seconds} + 1U);
        else {
            time.seconds = 0;
            if (time.minutes != 59)
                time.minutes = wrap8(unsigned{time.minutes} + 1U);
            else {
                time.minutes = 0;
                if (time.hours != 99)
                    time.hours = wrap8(unsigned{time.hours} + 1U);
            }
        }
    }
    // This is deliberately not conventional saturation of the entire clock.
    if (time.hours == 99 && time.minutes == 59)
        time.seconds = 0;
    frames = wrap8(unsigned{frames} + 1U);
}
} // namespace ff6
