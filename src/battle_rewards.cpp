#include "ff6/battle_rewards.hpp"
#include "ff6/battle_dance.hpp"
#include <algorithm>
#include <stdexcept>
#include "rom-materials/battle_reward_experience_cap.macros.inc"
#include "rom-materials/battle_reward_gil_cap.macros.inc"
#include "rom-materials/battle_reward_attribute_cap.macros.inc"
#include "rom-materials/battle_reward_level_cap.macros.inc"
#include "rom-materials/battle_reward_drop_threshold.macros.inc"
#include "rom-materials/battle_character_vital_caps.macros.inc"
namespace ff6 {
BattleRewardData load_battle_reward_data(const ContentPack& pack) {
    BattleRewardData data;
    data.espers = load_espers(pack);
    data.terrain_dances = load_battle_dance(pack).terrain_dances;
    const auto ap = pack.get("data_dfb400_battle_magic_points");
    const auto items = pack.get("data_d85000_item_prop");
    if (ap.size() != 512 || items.size() != 256 * 30)
        throw std::invalid_argument("Invalid victory learning tables");
    std::copy(ap.begin(), ap.end(), data.magic_points.begin());
    for (unsigned i = 0; i < 256; ++i) data.item_magic[i] = {items[i * 30 + 3], items[i * 30 + 4]};
    return data;
}
namespace {
void apply_esper_bonus(Character& character, std::uint8_t bonus, unsigned& hp, unsigned& mp) {
    // C2/6170: bonuses multiply this level's growth, never the accumulated stat.
    if (bonus < 6) {
        auto& growth = bonus < 3 ? hp : mp;
        constexpr unsigned fraction[] = {
#include "rom-materials/esper_growth_fractions.inc"
        };
        growth += std::max(1U, (growth * fraction[bonus % 3]) >> 8U);
    } else if (bonus == 6) hp *= 2;
    else if (bonus >= 9 && bonus <= 16) {
        auto& stat = character.attributes[(bonus - 9) / 2];
        stat = static_cast<std::uint8_t>(std::min(unsigned{FF6_ROM_VALUE_BATTLE_REWARD_ATTRIBUTE_CAP_0},
                                               unsigned{stat} + 1U + ((bonus - 9U) & 1U)));
    } else if (bonus != 7 && bonus != 8 && bonus != 255)
        throw std::invalid_argument("Invalid Esper level bonus");
}
}

void learn_battle_rages(LearnedAbilities& abilities, const BattleFormation& formation) {
    for (const auto& monster : formation.monsters)
        if (monster.monster && *monster.monster < 256) {
            const auto id = *monster.monster;
            abilities.rages[id / 8U] |= 1U << (id % 8U);
        }
}
void retain_battle_party(const BattleState& battle, const BattleParty& party,
                        Roster& roster, Inventory& inventory) {
    auto next_roster = roster;
    auto next_inventory = inventory;
    // EndBattle calls SaveMorphCounter before rewards or a scripted return.
    // A still-active transformation retains only its unused fraction.
    next_roster.morph_gauge = battle.morph.owner
        ? static_cast<std::uint8_t>((unsigned{battle.morph.saved_gauge} *
                                    (battle.morph.counter >> 8U)) >> 8U)
        : battle.morph.saved_gauge;
    for (unsigned i = 0; i < 4; ++i) {
        if (!party.slots[i].roster_index) continue;
        const auto identity = *party.slots[i].roster_index;
        auto& persistent = next_roster.characters.at(identity);
        if (battle.recruited_characters[i]) {
            next_roster.recruited |= 1U << 11U;
            auto& member = next_roster.membership.at(identity);
            member.enabled = true;
            member.party = next_roster.active_party;
            member.position = static_cast<std::uint8_t>(i);
            // C2/1EA9 preserves the row/visibility bits above membership.
        }
        // UpdateSRAM C2/494C calls RemoveChar for the retained removal mask.
        // Clear membership, including row/position, but keep recruitment,
        // equipment and vitals so the character remains available later.
        if (battle.removed_from_party[i])
            next_roster.membership.at(identity) = {};
        if (battle.shadow_departure == i) {
            // C2/48A6: mercenary departure clears availability and the equipped
            // Esper, but retains equipment, vitals and prior learning.
            next_roster.recruited &= ~(1U << 3U);
            persistent.esper.reset();
        }
        if (battle.inventory.hands[i])
            for (unsigned hand = 0; hand < 2; ++hand) {
                const auto& held = (*battle.inventory.hands[i])[hand];
                persistent.equipment[hand] = held.item ? held.item->identity : std::uint8_t{255};
            }
        const auto& stats = battle.stats[i];
        if (const auto& refreshed = battle.equipment_characters[i]) {
            persistent.hp.boost = refreshed->hp.boost;
            persistent.mp.boost = refreshed->mp.boost;
        }
        // UpdateSRAM preserves only the two documented persistent status bytes.
        persistent.hp.current = stats.hp.current;
        if (stats.mp.maximum) persistent.mp.current = stats.mp.current;
        persistent.status = wrap8(battle.status.targets[i].active);
        persistent.persistent_effects = wrap8(battle.status.targets[i].active >> 24U) & 0xc0U;
    }
    // GP Rain debits the shared wallet during combat. Preserve it on every
    // return path (including escape) before applying any victory reward.
    next_inventory.gil = battle.inventory.gil;
    // Copy the combat inventory before adding drops. Empty slots lose their old
    // quantity; occupied duplicates keep their individual stored order.
    for (unsigned i = 0; i < 256; ++i) {
        const auto& stack = battle.inventory.items[i];
        next_inventory.slots[i] = {stack.item ? stack.item->identity : std::uint8_t{255},
                                  stack.item ? stack.quantity : std::uint8_t{0}};
    }
    if (battle.end_event == 4) {
        // C2/48E0 removes Gau's entire membership byte and availability bit;
        // the learned set and introduction flag survive his absence.
        for (unsigned slot = 0; slot < 4; ++slot)
            if (party.slots[slot].actor == 11 && party.slots[slot].roster_index)
                next_roster.membership[*party.slots[slot].roster_index] = {};
        next_roster.recruited &= ~(1U << 11U);
    }
    roster = std::move(next_roster);
    inventory = std::move(next_inventory);
}
BattleRewards award_colosseum_victory(const BattleState& battle, const BattleParty& party,
                                       Roster& roster, Inventory& inventory, std::uint8_t prize) {
    if (!battle.colosseum) throw std::invalid_argument("Coliseum reward requires Coliseum battle");
    auto next_roster = roster;
    auto next_inventory = inventory;
    retain_battle_party(battle, party, next_roster, next_inventory);
    BattleRewards rewards;
    if (prize != 255) { give_inventory_item(next_inventory, prize); rewards.items.push_back({prize, 1}); }
    // C2/5D57 returns before ordinary EXP, gil, drops and ability learning.
    roster = std::move(next_roster);
    inventory = std::move(next_inventory);
    return rewards;
}
BattleRewards award_battle_victory(BattleState& battle, const BattleParty& party,
                                    const BattlePreparation& preparation,
                                    const MonsterStatsData& monsters, const PartyData& growth,
                                    const std::array<std::uint8_t, 256>& random,
                                    Roster& roster, Inventory& inventory, bool on_veldt,
                                    const BattleRewardData& learning, std::uint16_t formation,
                                    bool morph_available, std::uint8_t cursed_shield_battles) {
    if (battle.participation.enemy_count || !battle.participation.ally_count ||
        battle.colosseum || battle.end_event)
        throw std::invalid_argument("Rewards require an ordinary living-party victory");
    auto next_roster = roster;
    auto next_inventory = inventory;
    auto next_random = battle.random;
    retain_battle_party(battle, party, next_roster, next_inventory);
    BattleRewards rewards;
    rewards.magic_points = formation < learning.magic_points.size() ? learning.magic_points[formation] : 0;
    bool learning_active = false;
    const auto defeated = [&](unsigned slot) {
        constexpr auto statuses = status_mask(BattleStatus::dead) | status_mask(BattleStatus::petrify) |
                                  status_mask(BattleStatus::zombie);
        return preparation.monsters.slots[slot] &&
               (battle.status.targets[slot + 4].active & statuses);
    };
    for (unsigned i = 0; i < 6; ++i)
        if (defeated(i)) {
            // C2/5DB0: Veldt battles still award gil and items, but no EXP.
            if (!on_veldt)
                rewards.experience += preparation.monsters.slots[i]->experience;
            // C2/5DCF reads live MonsterGil, including enemy GP Rain spending.
            rewards.gil += battle.monster_information[i].remaining_gil;
        }
    rewards.experience /= battle.participation.ally_count;
    bool doubled_gil = false;
    // C2/5E13 visits party slots in reverse order. The shared shield counter
    // makes that ordering observable if more than one shield is equipped.
    for (unsigned reverse = 4; reverse; --reverse) {
        const auto i = reverse - 1;
        const auto character = party.slots[i].roster_index;
        if (!character) continue;
        auto& persistent = next_roster.characters.at(*character);
        const auto& stats = battle.stats[i];
        if (!battle.participation.living_allies[i]) continue;
        const auto actor = persistent.properties.value_or(255);
        const auto prior_magic = actor < 12 ? next_roster.abilities.magic[actor]
                                           : std::array<std::uint8_t, 54>{};
        if (actor == 0 && morph_available) {
            next_roster.morph_gauge = static_cast<std::uint8_t>(
                std::min(255U, unsigned{next_roster.morph_gauge} + 2U * rewards.magic_points));
            learning_active = true;
        }
        if (actor < 12) {
            auto& progress = next_roster.abilities.magic[actor];
            const auto learn = [&](std::uint8_t rate, std::uint8_t spell) {
                if (!rate || spell == 255) return;
                if (spell >= progress.size()) throw std::invalid_argument("Invalid taught spell");
                if (progress[spell] == 255) return;
                learning_active = true;
                // C2/604B deliberately keeps only the low byte of rate * AP.
                const unsigned value = progress[spell] + wrap8(unsigned{rate} * rewards.magic_points);
                progress[spell] = value >= 100 ? 128 : static_cast<std::uint8_t>(value);
            };
            for (auto& item : persistent.equipment) {
                // LearnItemMagic C2/5FEF: every eligible equipped copy advances
                // the retained byte even with zero AP or on the Veldt. Learning
                // on the wraparound victory uses the replacement item's data.
                if (item == 102) {
                    cursed_shield_battles = wrap8(unsigned{cursed_shield_battles} + 1U);
                    if (!cursed_shield_battles) {
                        item = 103;
                        rewards.dispelled_cursed_shield = true;
                    }
                }
                if (item != 255) learn(learning.item_magic[item][0], learning.item_magic[item][1]);
            }
            if (persistent.esper) {
                const auto& esper = learning.espers.at(*persistent.esper);
                for (unsigned j = 0; j < 5; ++j) learn(esper.rates[j], esper.spells[j]);
            }
        }
        if ((stats.relic_effects[3] & 16U) && !doubled_gil) {
            rewards.gil *= 2;
            doubled_gil = true;
        }
        const auto gained = rewards.experience * ((stats.relic_effects[3] & 8U) ? 2U : 1U);
        // AddExp's three-byte cap is assembled from its low word and high byte.
        persistent.experience = std::min(persistent.experience + gained,
            unsigned{FF6_ROM_VALUE_BATTLE_REWARD_EXPERIENCE_CAP_0 + FF6_ROM_VALUE_BATTLE_REWARD_EXPERIENCE_CAP_1});
        const auto before = persistent.level;
        while (persistent.level < FF6_ROM_VALUE_BATTLE_REWARD_LEVEL_CAP_0) {
            std::uint32_t threshold = 0;
            for (unsigned level = 0; level < persistent.level; ++level)
                threshold += growth.experience_growth[level] * 8U;
            if (persistent.experience < threshold) break;
            if (!persistent.level) throw std::invalid_argument("Invalid victory character level");
            const auto index = persistent.level - 1U;
            ++persistent.level;
            unsigned hp = growth.hp_growth[index], mp = growth.mp_growth[index];
            if (persistent.esper) apply_esper_bonus(persistent, learning.espers.at(*persistent.esper).bonus, hp, mp);
            persistent.hp.base = static_cast<std::uint16_t>(std::min(unsigned{persistent.hp.base} + hp,
                unsigned{FF6_ROM_VALUE_BATTLE_CHARACTER_VITAL_CAPS_0}));
            persistent.mp.base = static_cast<std::uint16_t>(std::min(unsigned{persistent.mp.base} + mp,
                unsigned{FF6_ROM_VALUE_BATTLE_CHARACTER_VITAL_CAPS_1}));
            learn_character_level_abilities(next_roster.abilities, growth, persistent);
        }
        if (persistent.level != before) rewards.levelled_characters.push_back(static_cast<std::uint8_t>(i));
        if (actor < 12) for (unsigned spell = 54; spell; --spell) {
            auto& value = next_roster.abilities.magic[actor][spell - 1];
            if (value == 128 || (value == 255 && prior_magic[spell - 1] != 255)) {
                value = 255;
                rewards.learned_spells.push_back({static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(spell - 1)});
            }
        }
    }
    // C2/5EC5 commits observed Lores in descending order, independently of
    // Strago's survival at victory, EXP eligibility, AP or Veldt encounters.
    for (unsigned reverse = 24; reverse; --reverse) {
        const auto lore = reverse - 1U;
        const auto bit = static_cast<std::uint8_t>(1U << (lore % 8U));
        auto& known = next_roster.abilities.lores[lore / 8U];
        if (battle.observed_lores[lore] && !(known & bit)) {
            known |= bit;
            rewards.learned_lores.push_back(static_cast<std::uint8_t>(139U + lore));
        }
    }
    // LearnMagic C2/5EE2 checks Mog's actor mapping, not survival or EXP
    // eligibility. Use the terrain at victory, after any successful Dance.
    if (battle.terrain_background && std::ranges::any_of(party.slots,
            [](const auto& member) { return member.actor == 10 && member.present; })) {
        const auto dance = learning.terrain_dances.at(*battle.terrain_background);
        if (dance < 8) {
            const auto bit = static_cast<std::uint8_t>(1U << dance);
            rewards.learned_dance = !(next_roster.abilities.dances & bit);
            next_roster.abilities.dances |= bit;
        }
    }
    rewards.announce_magic_points = morph_available && learning_active && rewards.magic_points;
    for (unsigned reverse = 6; reverse; --reverse) {
        const auto i = reverse - 1;
        if (!defeated(i)) continue;
        const auto identity = preparation.monsters.slots[i]->identity;
        const auto& items = monsters.monsters.at(identity).drop_items;
        const auto item = items[next_random.next(random) < FF6_ROM_VALUE_BATTLE_REWARD_DROP_THRESHOLD_0 ? 0 : 1];
        if (item == 255) continue;
        auto found = std::ranges::find_if(rewards.items, [&](auto value) { return value.item == item; });
        if (found == rewards.items.end()) rewards.items.push_back({item, 1});
        else ++found->quantity;
        give_inventory_item(next_inventory, item);
    }
    next_inventory.gil = std::min(next_inventory.gil + rewards.gil,
        unsigned{FF6_ROM_VALUE_BATTLE_REWARD_GIL_CAP_0 + FF6_ROM_VALUE_BATTLE_REWARD_GIL_CAP_1});
    roster = std::move(next_roster);
    inventory = std::move(next_inventory);
    battle.random = next_random;
    rewards.cursed_shield_battles = cursed_shield_battles;
    return rewards;
}
} // namespace ff6
