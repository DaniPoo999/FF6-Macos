#include "ff6/saved_game.hpp"
#include "ff6/sha256.hpp"
#include <algorithm>
#include <type_traits>
namespace ff6 {
namespace {
constexpr std::array<std::uint8_t, 8> magic{'F', 'F', '6', 'S', 'A', 'V', 'E', 0};
// A single explicit member list serves both directions. No object layout,
// padding, compiler bitfields or host pointer sizes enter the wire format.
struct Writer {
    Bytes bytes;
    template <class T> void scalar(T& value) {
        if constexpr (std::is_same_v<T, bool>)
            bytes.push_back(value ? 1 : 0);
        else {
            using U = std::make_unsigned_t<T>;
            const auto bits = static_cast<U>(value);
            for (unsigned i = 0; i < sizeof(T); ++i)
                bytes.push_back(static_cast<std::uint8_t>((bits >> (i * 8)) & 255U));
        }
    }
};
struct Reader {
    ByteView bytes;
    std::size_t at = 0;
    template <class T> void scalar(T& value) {
        if (at > bytes.size() || sizeof(T) > bytes.size() - at)
            throw std::invalid_argument("Truncated native save fields");
        const auto part = slice(bytes, at, sizeof(T));
        at += sizeof(T);
        if constexpr (std::is_same_v<T, bool>) {
            if (part[0] > 1)
                throw std::invalid_argument("Invalid saved boolean");
            value = part[0] != 0;
        } else {
            std::uint64_t bits = 0;
            for (unsigned i = 0; i < sizeof(T); ++i)
                bits |= std::uint64_t{part[i]} << (i * 8);
            value = static_cast<T>(bits);
        }
    }
};
template <class A, class T> void field(A& a, T& value);
template <class A, class... T> void members(A& a, T&... values) {
    (field(a, values), ...);
}
template <class A, class T, std::size_t N> void field(A& a, std::array<T, N>& values) {
    for (auto& v : values)
        field(a, v);
}
template <class A, std::size_t N> void field(A& a, std::bitset<N>& values) {
    for (unsigned i = 0; i < (N + 7) / 8; ++i) {
        std::uint8_t byte = 0;
        if constexpr (std::is_same_v<A, Writer>)
            for (unsigned bit = 0; bit < 8 && i * 8 + bit < N; ++bit)
                if (values[i * 8 + bit])
                    byte |= static_cast<std::uint8_t>(1U << bit);
        a.scalar(byte);
        if constexpr (std::is_same_v<A, Reader>)
            for (unsigned bit = 0; bit < 8; ++bit) {
                if (i * 8 + bit < N)
                    values[i * 8 + bit] = (byte & (1U << bit)) != 0;
                else if (byte & (1U << bit))
                    throw std::invalid_argument("Invalid saved flag padding");
            }
    }
}
template <class A> void field(A& a, std::optional<std::uint8_t>& value) {
    auto encoded = value.value_or(255);
    a.scalar(encoded);
    if constexpr (std::is_same_v<A, Reader>)
        value = encoded == 255 ? std::nullopt : std::optional{encoded};
}
template <class A> void field(A& a, Bytes& value) {
    auto count = static_cast<std::uint32_t>(value.size());
    a.scalar(count);
    if (count != 0 && count != original_save_slot_size)
        throw std::invalid_argument("Invalid saved compatibility extent");
    if constexpr (std::is_same_v<A, Reader>)
        value.resize(count);
    for (auto& byte : value)
        a.scalar(byte);
}
template <class A> void field(A& a, FieldEventTimer& v) {
    members(a, v.remaining, v.event, v.pause_in_menu_battle, v.visible_field,
            v.visible_menu_battle, v.terminate_external);
}
template <class A> void field(A& a, VitalStat& v) {
    members(a, v.current, v.base, v.boost);
}
template <class A> void field(A& a, Character& v) {
    members(a, v.properties, v.graphics, v.level, v.name, v.hp, v.mp, v.experience, v.status,
            v.persistent_effects, v.commands, v.attributes, v.equipment, v.esper);
}
template <class A> void field(A& a, PartyMembership& v) {
    members(a, v.enabled, v.back_row, v.party, v.position, v.visible);
}
template <class A> void field(A& a, LearnedAbilities& v) {
    members(a, v.magic, v.bushido, v.blitz, v.dances, v.rages, v.lores);
}
template <class A> void field(A& a, Roster& v) {
    members(a, v.characters, v.membership, v.abilities, v.recruited, v.espers_owned, v.active_party,
            v.morph_gauge);
}
template <class A> void field(A& a, InventorySlot& v) {
    members(a, v.item, v.quantity);
}
template <class A> void field(A& a, Inventory& v) {
    members(a, v.slots, v.gil);
}
template <class A> void field(A& a, FieldFlags& v) {
    members(a, v.story, v.npc, v.treasure);
}
template <class A> void field(A& a, GameTime& v) {
    members(a, v.hours, v.minutes, v.seconds);
}
template <class A> void field(A& a, MenuCursorPosition& v) {
    members(a, v.x, v.y);
}
template <class A> void field(A& a, MenuListPosition& v) {
    members(a, v.position, v.scroll);
}
template <class A> void field(A& a, MenuCursorMemory& v) {
    members(a, v.main, v.character, v.rare_items, v.item_options, v.items, v.skills, v.magic);
}
template <class A> void field(A& a, GameMenuState& v) {
    members(a, v.cursors, v.time, v.saved_time, v.time_frames, v.battle_speed, v.message_speed,
            v.magic_order, v.save_count, v.battle_wait, v.short_commands, v.hide_gauge, v.mono,
            v.cursor_memory, v.remove_equipment, v.custom_buttons, v.multiplayer,
            v.second_player_characters, v.saved_buttons);
}
template <class A> void field(A& a, InputSettings& v) {
    members(a, v.mapping, v.repeat_delay, v.repeat_period);
}
template <class A> void field(A& a, FieldCharacterLocation& v) {
    members(a, v.map, v.x, v.y);
}
template <class A> void field(A& a, SavedFieldCharacter& v) {
    members(a, v.location, v.palette, v.facing, v.ride, v.rider_visible);
}
template <class A> void field(A& a, FieldDestination& v) {
    members(a, v.map, v.x, v.y, v.facing);
}
template <class A> void field(A& a, FieldTilePosition& v) {
    members(a, v.x, v.y);
}
template <class A> void field(A& a, FieldMapArrival& v) {
    members(a, v.position, v.facing, v.show_name, v.preserve_facing);
}
template <class A> void field(A& a, FieldEntranceMemory& v) {
    members(a, v.placement, v.characters, v.saved_level, v.world_vehicle, v.return_facing,
            v.run_startup, v.fade_in, v.keep_dimensions);
}
template <class A> void field(A& a, FieldBattleRandom& v) {
    members(a, v.encounter_index, v.encounter_offset, v.formation_index, v.formation_offset,
            v.veldt);
}
template <class A> void field(A& a, BattleCommandRules& v) {
    members(a, v.magic_only, v.morph_available, v.permanent_morph, v.on_veldt,
            v.double_morph_duration);
}
template <class A> void world_field(A& a, std::optional<WorldState>& value) {
    bool present = value.has_value();
    a.scalar(present);
    if constexpr (std::is_same_v<A, Reader>) {
        if (present) value.emplace(); else value.reset();
    }
    if (present) members(a, value->map, value->facing, value->x, value->y, value->minimap);
}
template <class A> void parked_airship_field(A& a, std::optional<FieldTilePosition>& value) {
    bool present = value.has_value();
    a.scalar(present);
    if constexpr (std::is_same_v<A, Reader>) {
        if (present) value.emplace(); else value.reset();
    }
    if (present) field(a, *value);
}
template <class A> void airship_resume_field(A& a, std::optional<WorldFlightResume>& value) {
    bool present = value.has_value();
    a.scalar(present);
    if constexpr (std::is_same_v<A, Reader>) {
        if (present) value.emplace(); else value.reset();
    }
    if (present) members(a, value->heading, value->altitude);
}
template <class A> void field(A& a, SavedGame& v) {
    members(a, v.roster, v.inventory, v.flags, v.menu, v.input, v.colors, v.skin, v.font_color,
            v.steps, v.characters, v.destination, v.parent, v.scroll, v.leader, v.terrain_level,
            v.map_song, v.field_random, v.travel, v.battle_random, v.encounter_counter,
            v.doom_gaze_hp, v.battle_conditions, v.battle_rules, v.original_slot);
}
template <class A, class T> void field(A& a, T& value) {
    if constexpr (std::is_enum_v<T>) {
        auto v = static_cast<std::underlying_type_t<T>>(value);
        a.scalar(v);
        value = static_cast<T>(v);
    } else {
        static_assert(std::is_integral_v<T>);
        a.scalar(value);
    }
}
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
} // namespace
void validate_saved_game(const SavedGame& v) {
    for (const auto& timer : v.timers)
        require(timer.event < 0x40000, "Invalid saved timer callback");
    if (v.world)
        require(v.world->map < 2 && v.world->facing < 4 && v.world->x < 4096 && v.world->y < 4096 &&
                    v.world->x % 16 == 0 && v.world->y % 16 == 0 && !v.world->remaining &&
                    !v.world->chocobo && !v.world->vehicle && !v.world->airship_transition,
                "Invalid saved world position");
    if (v.airship_resume)
        require(v.airship_resume->heading < 360 && v.airship_resume->altitude <= 0x7e00,
                "Invalid saved airship navigation");
    for (auto level : v.party_levels)
        require(level <= 3, "Invalid saved party terrain level");
    require(v.destination.map >= 3 && v.destination.map < 415,
            "This save requires an unsupported world-map destination");
    require(v.leader < 16 && v.destination.facing < 4 && v.terrain_level <= 3,
            "Invalid saved field placement");
    require(v.skin < 8 && v.font_color <= 32767, "Invalid saved menu appearance");
    for (auto palette : v.colors)
        for (auto color : palette)
            require(color <= 32767, "Invalid saved window color");
    require(v.steps <= 0xffffff && v.inventory.gil <= 9999999, "Invalid saved counters");
    require(v.menu.time.hours < 100 && v.menu.time.minutes < 60 && v.menu.time.seconds < 60 &&
                v.menu.time_frames <= 60,
            "Invalid saved clock");
    require(v.menu.battle_speed < 6 && v.menu.message_speed < 6 && v.menu.magic_order < 6,
            "Invalid saved preferences");
    require(v.roster.active_party > 0 && v.roster.active_party <= 7 &&
                v.roster.espers_owned < 0x8000000,
            "Invalid saved party metadata");
    require(v.original_slot.empty() || v.original_slot.size() == original_save_slot_size,
            "Invalid SRAM compatibility data");
    const auto party = current_menu_party(v.roster);
    require(std::ranges::any_of(party, [](auto id) { return id.has_value(); }),
            "Save has no active party");
    require(std::ranges::find(party, std::optional{v.leader}) != party.end(),
            "Saved leader is outside the active party");
    for (unsigned i = 0; i < 16; ++i) {
        const auto& c = v.roster.characters[i];
        const auto& m = v.roster.membership[i];
        const auto& loc = v.characters[i];
        require(m.party <= 7 && m.position < 4, "Invalid saved party membership");
        require(!c.properties || *c.properties < 64, "Invalid saved character properties");
        require(!c.esper || *c.esper < 27, "Invalid saved esper");
        require(c.experience <= 0xffffff && c.hp.base <= 0x3fff && c.mp.base <= 0x3fff &&
                    static_cast<unsigned>(c.hp.boost) < 4 && static_cast<unsigned>(c.mp.boost) < 4,
                "Invalid saved character values");
        require(loc.palette < 8 && loc.facing < 4 && static_cast<unsigned>(loc.ride) < 4,
                "Invalid saved field appearance");
        // EventCmd_37 persists the field appearance independently of battle
        // graphics. Story-controlled actors can use any loaded field identity;
        // restricting this to the battle character sheets rejects their saves.
        if (m.enabled && m.party == v.roster.active_party)
            require(c.properties && c.level > 0 && c.level <= 99 &&
                        c.graphics < field_sprite_graphics_count,
                    "Invalid active saved character");
    }
    for (auto mapped : v.input.mapping)
        require(mapped >= Button::a && mapped <= Button::select &&
                    (button(mapped) & (button(mapped) - 1)) == 0,
                "Invalid saved input mapping");
    require(v.input.repeat_period != 0, "Invalid saved input repeat period");
    const auto& c = v.menu.cursors;
    require(c.main.y < 7 && c.character.y < 4 && c.rare_items.x < 2 && c.rare_items.y < 10 &&
                c.item_options.x < 3 && c.items.scroll <= 245 && c.items.position.y < 10,
            "Invalid saved menu cursor");
    for (auto s : c.skills)
        require(s.y < 7, "Invalid saved skills cursor");
    for (auto s : c.magic)
        require(s.position.x < 2 && s.position.y < 8 && s.scroll <= 19,
                "Invalid saved magic cursor");
}
Bytes encode_saved_game(const SavedGame& game) {
    validate_saved_game(game);
    auto copy = game;
    Writer w;
    field(w, copy);
    field(w, copy.party_levels);
    w.scalar(copy.world_minimap);
    world_field(w, copy.world);
    w.scalar(copy.terra_magic_pending);
    field(w, copy.event_variables);
    field(w, copy.battle_variables);
    field(w, copy.encountered_formations);
    parked_airship_field(w, copy.parked_airship);
    airship_resume_field(w, copy.airship_resume);
    field(w, copy.timers);
    Bytes out(magic.begin(), magic.end());
    append32(out, 10);
    append(out, parse_digest(rom_hash));
    append32(out, static_cast<std::uint32_t>(w.bytes.size()));
    append(out, sha256(w.bytes));
    append(out, w.bytes);
    return out;
}
SavedGame decode_saved_game(ByteView bytes) {
    require(bytes.size() >= 80 && bytes.size() <= 16384, "Invalid native save extent");
    const auto version = read32(bytes, 8);
    require(std::ranges::equal(bytes.first(8), magic) && (version >= 1 && version <= 10),
            "Unsupported native save format/version");
    require(std::ranges::equal(bytes.subspan(12, 32), parse_digest(rom_hash)),
            "Save belongs to another game revision");
    require(read32(bytes, 44) == bytes.size() - 80, "Native save payload length mismatch");
    const auto payload = bytes.subspan(80);
    require(std::ranges::equal(bytes.subspan(48, 32), sha256(payload)),
            "Native save checksum mismatch");
    SavedGame result;
    Reader r{payload};
    field(r, result);
    if (version >= 2) field(r, result.party_levels);
    else if (result.roster.active_party < result.party_levels.size())
        result.party_levels[result.roster.active_party] = result.terrain_level;
    if (version >= 3) { r.scalar(result.world_minimap); world_field(r, result.world); }
    if (version >= 4)
        r.scalar(result.terra_magic_pending);
    if (version >= 5)
        field(r, result.event_variables);
    if (version >= 6)
        field(r, result.battle_variables);
    if (version >= 7)
        field(r, result.encountered_formations);
    if (version >= 8) parked_airship_field(r, result.parked_airship);
    if (version >= 9) airship_resume_field(r, result.airship_resume);
    if (version >= 10) field(r, result.timers);
    require(r.at == payload.size(), "Unexpected native save fields");
    validate_saved_game(result);
    return result;
}
} // namespace ff6
