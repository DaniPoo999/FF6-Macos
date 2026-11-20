#include "ff6/battle_formation.hpp"
#include <algorithm>

namespace ff6 {
BattleFormationData decode_battle_formation_data(ByteView random, ByteView conditional,
                                                 ByteView properties, ByteView monsters) {
    if (random.size() != 256 || conditional.size() != 64 || properties.size() != 576 * 4 ||
        monsters.size() != 576 * 15)
        throw std::invalid_argument("Invalid battle formation table extent");
    BattleFormationData result;
    std::ranges::copy(random, result.random.begin());
    for (std::size_t i = 0; i < result.conditional.size(); ++i)
        result.conditional[i] = {read16(conditional, i * 4), read16(conditional, i * 4 + 2)};
    for (std::size_t i = 0; i < result.formations.size(); ++i) {
        auto& out = result.formations[i];
        const auto prop = properties.subspan(i * 4, 4);
        const auto group = monsters.subspan(i * 15, 15);
        out.entrance = prop[0] & 15U;
        for (unsigned approach = 0; approach < 4; ++approach)
            out.allowed_approaches[approach] = (prop[0] & (16U << approach)) == 0;
        out.victory_music = (prop[1] & 2U) == 0;
        out.joker_doom = (prop[1] & 4U) == 0;
        out.leap = (prop[1] & 8U) == 0;
        out.character_ai_enabled = (prop[1] & 128U) != 0;
        out.character_ai = prop[2];
        if (out.character_ai_enabled && out.character_ai >= 24)
            throw std::invalid_argument("Formation character AI identity outside table");
        out.reserved_behavior = prop[1] & 0x71U;
        out.escape = (prop[3] & 1U) == 0;
        out.veldt = (prop[3] & 2U) == 0;
        out.preemptive = (prop[3] & 4U) == 0;
        out.song_slot = (prop[3] >> 3U) & 7U;
        out.continue_music = (prop[3] & 128U) != 0;
        out.reserved_music = prop[3] & 64U;
        out.graphics_layout = group[0] >> 4U;
        if (out.graphics_layout >= 13)
            throw std::invalid_argument("Formation graphics layout outside table");
        out.reserved_monsters = group[14] >> 6U;
        const unsigned backgrounds = ((group[0] & 15U) << 2U) | (group[1] >> 6U);
        for (unsigned slot = 0; slot < 6; ++slot) {
            auto& placement = out.monsters[slot];
            const unsigned identity = group[2 + slot] | (((group[14] >> slot) & 1U) << 8U);
            if (identity < 384)
                placement.monster = static_cast<std::uint16_t>(identity);
            else if (identity != 511)
                throw std::invalid_argument("Formation monster identity outside table");
            placement.present = (group[1] & (1U << slot)) != 0;
            placement.background_one = (backgrounds & (1U << slot)) != 0;
            placement.x_tiles = group[8 + slot] >> 4U;
            placement.y_tiles = group[8 + slot] & 15U;
        }
    }
    return result;
}
BattleFormationData load_battle_formation_data(const ContentPack& pack) {
    return decode_battle_formation_data(
        pack.get("encounter_rng_tbl_bin"), pack.get("data_cf3780_cond_battle"),
        pack.get("formation_battle_prop_bin"), pack.get("formation_battle_monsters_bin"));
}
BattleRandom BattleRandom::from_game_clock(std::uint8_t frames) {
    return {wrap8(unsigned{frames} * 4U)};
}
std::uint8_t BattleRandom::next(const std::array<std::uint8_t, 256>& table) {
    index = wrap8(unsigned{index} + 1U);
    return table[index];
}
BattleFormationSelection select_battle_formation(const BattleFormationData& data,
                                                 std::uint16_t request, std::bitset<16> conditions,
                                                 std::optional<std::uint16_t> retained_identity,
                                                 BattleRandom& random) {
    if (retained_identity && *retained_identity >= data.formations.size())
        throw std::invalid_argument("Invalid retained battle identity");
    for (unsigned remaining = 8; remaining != 0; --remaining) {
        const auto index = remaining - 1;
        const auto& rule = data.conditional[index];
        // Matches cascade in descending order. Low condition bits and the
        // last eight records are ignored by the original sizeof declaration.
        if (conditions[index + 8] && rule.from == request)
            request = rule.to;
    }
    auto next = random;
    const bool variant = (request & 0x8000U) != 0;
    request &= 0x7fffU;
    if (variant)
        request = wrap16(unsigned{request} + (next.next(data.random) & 3U));
    if (request >= data.formations.size())
        throw std::invalid_argument("Selected battle formation outside table");
    random = next;
    return {request, retained_identity.value_or(request)};
}
void remember_veldt_formation(std::bitset<512>& history, const BattleFormationEntry& entry) {
    // C2/49DE–4A06: a disabled formation, identity >=512, or an entirely
    // high-index/empty monster roster cannot become a Veldt encounter.
    if (!entry.formation.veldt || entry.selection.battle_identity >= history.size())
        return;
    if (std::ranges::any_of(entry.formation.monsters, [](const auto& slot) {
            return slot.monster && *slot.monster < 256;
        }))
        history.set(entry.selection.battle_identity);
}
} // namespace ff6
