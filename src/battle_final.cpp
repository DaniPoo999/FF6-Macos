#include "ff6/battle_final.hpp"
#include "ff6/battle_schedule.hpp"
#include "rom-materials/final_battle_formations.macros.inc"
#include "rom-materials/final_battle_song.macros.inc"
#include "rom-materials/final_battle_volume_period.macros.inc"
#include "rom-materials/final_battle_viewport.macros.inc"
#include <algorithm>

namespace ff6 {
std::optional<std::uint16_t> next_final_battle(std::uint16_t formation) {
    // C2/4AAB supplies the identities; only the dispatch is native code.
    // Generated literals preserve the frozen compiler output. The final entry
    // has no following tier and uses the ordinary ending.
    switch (formation) {
    case FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_0: return FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_1;
    case FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_1: return FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_2;
    case FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_2: return FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_3;
    default: return {};
    }
}
BattlePartyContext final_battle_party_context(const BattleState& battle, const BattleParty& party) {
    BattlePartyContext result;
    result.final_lineup = true; result.final_order = party.final_order;
    result.retained = retain_battle_party(party);
    constexpr auto disqualified = status_mask(BattleStatus::dead) | status_mask(BattleStatus::petrify) |
                                  status_mask(BattleStatus::zombie);
    for (unsigned slot = 0; slot < 4; ++slot)
        if (!battle.status.targets[slot].present || (battle.status.targets[slot].active & disqualified) ||
            battle.deferred_death[slot]) result.retained[slot] = {};
    return result;
}
void retain_final_battle_state(BattleState& next, const BattleState& previous, const BattlePartyContext& retained) {
    // C2/4A22 retains surviving actors/statuses and their once-per-battle flags.
    // Fresh equipment/commands, queues, links and timers belong to InitBattle.
    constexpr auto cleared = status_mask(BattleStatus::dance) | status_mask(BattleStatus::rage) |
                             status_mask(BattleStatus::control);
    next.entry.final_scrolling = true;
    next.field_steps = previous.field_steps;
    next.terra_magic_pending = previous.terra_magic_pending;
    next.morph = previous.morph;
    for (unsigned slot = 0; slot < 4; ++slot) if (retained.retained[slot].actor) {
        next.status.targets[slot].active = previous.status.targets[slot].active & ~cleared;
        next.status.targets[slot].pending = {};
        // InitBattle requests its ordinary refresh flags, not every status
        // callback. Setting all bits would spuriously apply Palidor to survivors.
        next.status.targets[slot].updates = 0xbc;
        next.away_from_battle[slot] = previous.away_from_battle[slot];
        next.availability.used_espers[slot] = previous.availability.used_espers[slot];
        next.desperation_used[slot] = previous.desperation_used[slot];
    }
    if (next.morph.owner)
        next.status.targets[*next.morph.owner].updates.set(static_cast<unsigned>(BattleStatusUpdate::morph));
}
void resume_final_battle_jumps(BattleState& battle) {
    // C2/24FE turns retained Jump/Seize departures into ordinary landing actions
    // after InitGauge. Old target identities belong to the departed formation.
    for (unsigned slot = 4; slot-- > 0;) if (battle.away_from_battle[slot]) {
        auto& turn = battle.turns[slot];
        turn.atb &= 255U;
        turn.atb_stopped = turn.action_ready = true;
        turn.menu_requests_allowed = false;
        turn.normal = {{22, 0, 0, 0}};
        schedule_battle_turn(battle, static_cast<std::uint8_t>(slot), true);
    }
}
BattleTierPicture final_tier_picture(const BattleEntranceAssets& assets, const BattleEntranceSeed& seed) {
    BattleTierPicture out;
    out.terrain = compose_battle_scene(assets.background, assets.readouts, seed.readout, {});
    out.terrain.layers.resize(1);
    out.actors = make_battle_actor_sprite_groups(assets.actors, seed.actors.publication);
    return out;
}
BattleFinalTransition::BattleFinalTransition(BattleTierPicture outgoing, BattleTierPicture incoming, bool last)
    : outgoing_(std::move(outgoing)), incoming_(std::move(incoming)), last_(last) {
    if (outgoing_.terrain.layers.size() != 1 || incoming_.terrain.layers.size() != 1)
        throw std::invalid_argument("Final scroll requires two owned terrain images");
}
std::vector<MusicPlayerCommand> BattleFinalTransition::advance() {
    if (complete()) return {};
    std::vector<MusicPlayerCommand> audio;
    if (last_ && age_ == 0)
        audio.push_back(PlaybackVolumeRamp{PlaybackGroup::music,FF6_ROM_VALUE_FINAL_BATTLE_VOLUME_PERIOD_0,0});
    // The source changes music after waiting at its checkpoint, before
    // the next downward pixel. The extracted schedule preserves this ordering.
    if (last_ && age_ == FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_7)
        audio.push_back(GameSong{FF6_ROM_VALUE_FINAL_BATTLE_SONG_0,255,false});
    ++age_;
    return audio;
}
RasterScene BattleFinalTransition::scene() const {
    const bool scrolling = age_ >= FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_3 &&
                           age_ < FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_5;
    const auto& picture = age_ < FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_5 ? outgoing_ : incoming_;
    auto out = picture.terrain;
    if (scrolling) {
        const int shift = int((age_ - FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_3) /
                              FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_1);
        auto incoming = incoming_.terrain.layers.front();
        auto& outgoing = out.layers.front();
        for (unsigned y = 0; y < out.height; ++y) {
            const bool visible = y >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 &&
                                 y < FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1;
            // TfrBattleBGTiles stages the incoming rows immediately before
            // the old map's row zero in the scrolling image. Select by SOURCE
            // row, including the ordinary one-pixel viewport offset. Splitting
            // at destination y == shift sampled the blank row following the
            // incoming image and skipped outgoing row zero, producing a seam.
            const int source_row = int(y) + outgoing.lines[y].offset_y - shift;
            outgoing.lines[y].enabled = visible && source_row >= 0;
            incoming.lines[y].enabled = visible && source_row < 0;
            outgoing.lines[y].offset_y -= shift;
            incoming.lines[y].offset_y += FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_2 - shift;
        }
        out.layers.push_back(std::move(incoming));
        return out;
    }
    const unsigned darkness = age_ < FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_3
        ? std::min(31U, age_ / FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_0)
        : 31U - std::min(31U, (age_ - FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_5) /
                             FF6_ROM_VALUE_FINAL_BATTLE_SCHEDULE_0);
    for (const auto& group : picture.actors) for (auto piece : group.sprites) {
        if (group.participant < 4)
            for (auto& color : piece.image.colors)
                color = blend_color(color, static_cast<Rgb555>(darkness * 0x421U), ColorBlend::subtract);
        out.objects->sprites.push_back(std::move(piece.image));
    }
    return out;
}
} // namespace ff6
