#include "ff6/field_session.hpp"
#include "rom-materials/audio_loading_choices.macros.inc"
#include "ff6/game_time.hpp"

namespace ff6 {
FieldSessionTick FieldSession::advance_mine_cart(const ContentPack& pack) {
    FieldSessionTick tick;
    append_field_audio(tick.audio);
    mine_cart_->advance();
    if (mine_cart_->take_battle_sound()) tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_10, 128});
    visible_ = render_scene(mine_cart_->scene());
    advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
    ++ticks_;
    tick.complete = tick.input_sampled = true;
    if (mine_cart_->complete()) {
        scene_.complete_cutscene(cutscene_request_, EventCutscene::mine_cart, false);
        mine_cart_.reset();
    } else if (mine_cart_->battle_ready()) {
        scene_.owned_.battle = select_mine_cart_battle(load_field_battle_data(pack),
            *mine_cart_->battle_group(), scene_.owned_.actions.random_index);
        enter_battle(pack);
    }
    return tick;
}
} // namespace ff6
