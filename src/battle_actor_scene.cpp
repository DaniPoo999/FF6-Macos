#include "rom-materials/actor_reflection_displacement.macros.inc"
#include "rom-materials/actor_reflection_priority.macros.inc"
#include "rom-materials/actor_status_kneeling_frames.macros.inc"
#include "rom-materials/actor_countdown_refresh_mask.macros.inc"
#include "ff6/battle_actor_scene.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool valid, const char* why) {
    if (!valid)
        throw std::invalid_argument(why);
}
void validate_armor(const BattleActorSceneAssets& assets, BattleArmorImageRequest request) {
    require(request.slot < 4 && request.frame < assets.graphics.magitek_frames.size(),
            "Invalid pending armor image");
}
BattleSceneAnchors anchors(const BattleActorSceneAssets& assets, const BattleActorScene& state) {
    std::array<BattleCharacterAnchorInput, 4> characters;
    std::array<BattleMonsterAnchorInput, 6> monsters;
    for (unsigned slot = 0; slot < 4; ++slot) {
        const auto& p = state.characters.layout.poses[slot];
        characters[slot] = {p.x,        p.y,           p.offset_x,
                            p.offset_y, p.animation_x, state.monsters.character_cursor_front[slot]};
    }
    for (unsigned slot = 0; slot < 6; ++slot) {
        const auto& p = state.monsters.positions[slot];
        const auto& size = assets.monsters.dimensions[slot];
        const auto overlap = state.animation.monster_overlap
                                 ? (*state.animation.monster_overlap)[slot] : std::uint16_t{size.overlap_y};
        monsters[slot] = {p.x, p.y, overlap, p.cursor_front, size.width, size.height};
    }
    return calculate_battle_anchors(monsters, characters);
}
} // namespace
BattleStatusSpriteAssets load_battle_status_sprite_assets(const ContentPack& pack) {
    BattleStatusSpriteAssets out;
    const auto layouts = pack.get("battle_status_sprite_layouts");
    const auto frames = pack.get("battle_status_sprite_frames");
    require(layouts.size() == 72 && frames.size() == 64, "Invalid status sprite tables");
    out.tiles = decode_tiles(decompress_lzss(pack.get("menu_graphics_status_gfx_4bpp_lz")), 4);
    require(out.tiles.size() == 256, "Invalid status sprite image");
    for (unsigned i = 0; i < 8; ++i)
        out.mirrored_x[i] = read16(layouts, i * 2);
    for (unsigned i = 0; i < 14; ++i) {
        const unsigned at = 16 + i * 4;
        const auto flags = layouts[at + 3];
        require(layouts[at + 2] >= 32 && layouts[at + 2] <= 44 && !(flags & 1U),
                "Invalid status sprite tile");
        out.layouts[i] = {static_cast<std::int16_t>(layouts[at] < 128 ? int(layouts[at]) : int(layouts[at]) - 256),
                          static_cast<std::int16_t>(layouts[at + 1] < 128 ? int(layouts[at + 1]) : int(layouts[at + 1]) - 256),
                          static_cast<std::uint8_t>(layouts[at + 2] - 32),
                          {static_cast<std::uint8_t>((flags >> 1U) & 7U),
                           static_cast<std::uint8_t>((flags >> 4U) & 3U),
                           (flags & 64U) != 0, (flags & 128U) != 0}};
    }
    for (unsigned i = 0; i < 32; ++i) {
        const auto address = read16(frames, i * 2);
        require(address >= 0xa400 && address <= 0xb380 && (address - 0xa400) % 128 == 0,
                "Invalid status image frame selection");
        out.frame_tiles[i] = static_cast<std::uint16_t>((address - 0xa400) / 32);
    }
    return out;
}
BattleCharacterPaletteBanks battle_character_colors(const BattleActorScene& state,
                                                    const BattleActorSceneAssets& assets) {
    BattleCharacterPaletteBanks colors;
    const auto& status =
        state.retained_status_colors ? *state.retained_status_colors : assets.status_colors;
    for (unsigned slot = 0; slot < 4; ++slot) {
        std::ranges::copy(state.characters.layout.visible_colors[slot], colors[slot].begin());
        std::ranges::copy(status[slot], colors[slot].begin() + 12);
    }
    return colors;
}
BattleMonsterPaletteBanks battle_monster_colors(const BattleActorScene& state,
                                                const BattleActorSceneAssets& assets) {
    if (state.monster_colors)
        return *state.monster_colors;
    BattleMonsterPaletteBanks colors{};
    for (unsigned bank = 0; bank < colors.size(); ++bank)
        if (assets.monsters.colors[bank])
            colors[bank] = *assets.monsters.colors[bank];
    return colors;
}
void set_battle_character_colors(BattleActorScene& state,
                                 const BattleCharacterPaletteBanks& colors) {
    if (!state.retained_status_colors)
        state.retained_status_colors.emplace();
    for (unsigned slot = 0; slot < 4; ++slot) {
        std::copy_n(colors[slot].begin(), 12, state.characters.layout.visible_colors[slot].begin());
        std::copy_n(colors[slot].begin() + 12, 4, (*state.retained_status_colors)[slot].begin());
    }
}
void apply_battle_actor_appearance_changes(const BattleActorSceneAssets& assets,
                                          const BattleActorSceneRules& rules,
                                          std::span<const BattleActorAppearanceChange> changes,
                                          BattleActorScene& state, bool staged) {
    if (changes.empty())
        return;
    auto next = state;
    for (const auto& change : changes) {
        require(change.participant < 10, "Invalid appearance-change participant");
        require(change.operation == BattleActorAppearanceOperation::toggle_imp ||
                    change.operation == BattleActorAppearanceOperation::vanish ||
                    change.operation == BattleActorAppearanceOperation::replace_graphics ||
                    change.operation == BattleActorAppearanceOperation::toggle_status,
                "Unowned actor appearance operation");
        require(change.operation != BattleActorAppearanceOperation::replace_graphics ||
                    (change.participant < 4 && change.graphics < assets.graphics.graphics.size()),
                "Character image replacement needs a valid character and graphics identity");
        if (change.participant < 4) {
            auto& characters = next.characters;
            auto& control = characters.control[change.participant];
            if (change.operation == BattleActorAppearanceOperation::toggle_imp)
                control.active_status ^= status_mask(BattleStatus::imp);
            else if (change.operation == BattleActorAppearanceOperation::vanish)
                control.active_status |= status_mask(BattleStatus::vanish);
            else if (change.operation == BattleActorAppearanceOperation::toggle_status)
                control.active_status ^= change.status;
            else
                control.base_graphics = change.graphics;
            characters.status_cursor = change.participant;
            update_battle_character_status(assets.graphics, rules.appearance, characters);
        } else if (change.operation == BattleActorAppearanceOperation::toggle_imp) {
            require(assets.monster_graphics_data && assets.monster_graphics_selection,
                    "Monster appearance change requires its graphics source and selection");
            require(!next.monster_images || next.monster_images->published_tiles == 256,
                    "Previous monster image publication is still active");
            const auto slot = change.participant - 4U;
            auto selection = next.monster_images ? next.monster_images->selection
                                                : *assets.monster_graphics_selection;
            require(selection.identities[slot].has_value(),
                    "Appearance change targets an absent monster");
            selection.retained_dimensions = assets.monsters.dimensions;
            selection.imp.flip(slot);
            const auto loaded =
                initialize_battle_monster_graphics(*assets.monster_graphics_data, selection);
            require(loaded.dimensions == assets.monsters.dimensions,
                    "Changed monster dimensions require renewed sprite geometry");
            auto prepared = std::make_shared<BattleMonsterTiles>(loaded.tiles);
            auto visible = next.monster_images ? next.monster_images->visible
                                              : std::make_shared<const BattleMonsterTiles>(
                                                    assets.monsters.tiles);
            // InitMonsterGfx preserves working palettes and sprite geometry.
            // Reload all shared tiles: several actors can alias an allocation.
            next.monster_images = {selection, prepared, staged ? visible : prepared,
                                    staged ? 0U : 256U};
        }
    }
    state = std::move(next);
}
void publish_battle_actor_scene(BattleActorScene& state, const BattleActorSceneAssets& assets,
                                bool hide_characters) {
    auto& out = state.publication;
    if (out.pending_armor)
        validate_armor(assets, *out.pending_armor);
    if (state.animation.force_character_frame)
        require(*state.animation.force_character_frame < 4, "Invalid forced character image");
    // Allocate the replacement before modifying retained image/control state.
    auto visible = out.prepared;
    auto transfers = battle_actor_transfers(hide_characters, state.characters.layout.palette_phase,
                                             state.animation.force_character_frame,
                                             out.pending_armor.has_value());
    if (state.monster_images) {
        auto& images = *state.monster_images;
        if (images.published_tiles < 256) {
            auto tiles = std::make_shared<BattleMonsterTiles>(*images.visible);
            // One explicit loading wait publishes 1024 original image bytes,
            // represented here as 32 decoded tiles, without a device/bus model.
            std::copy_n(images.prepared->begin() + images.published_tiles, 32,
                        tiles->begin() + images.published_tiles);
            images.visible = std::move(tiles);
            images.published_tiles += 32;
        }
        out.monster_tiles = images.visible;
    }
    if (!hide_characters) {
        if (assets.status_sprites) {
            // The eight image strips cycle through four authored phases. Keep
            // top/bottom publication independent, including the reused last strip.
            const unsigned phase = state.characters.layout.palette_phase & 31U;
            const unsigned destination = (phase & 6U) * 2U + (phase & 1U) * 16U;
            const unsigned source = assets.status_sprites->frame_tiles[phase];
            require(source + 4 <= assets.status_sprites->tiles.size(), "Invalid status image strip");
            std::copy_n(assets.status_sprites->tiles.begin() + source, 4,
                        out.status_tiles.begin() + destination);
        }
        const auto slot = state.characters.layout.palette_phase & 3U;
        out.character_frames[slot] = state.characters.frames[slot];
        if (!out.pending_armor && state.animation.force_character_frame) {
            const auto forced = *state.animation.force_character_frame;
            out.character_frames[forced] = state.characters.frames[forced];
            state.animation.force_character_frame.reset();
        }
    }
    if (out.pending_armor) {
        out.armor_frames[out.pending_armor->slot] = out.pending_armor->frame;
        out.pending_armor.reset();
    }
    out.colors = battle_character_colors(state, assets);
    out.monster_colors = battle_monster_colors(state, assets);
    out.visible = std::move(visible);
    out.transfers = std::move(transfers);
}
void prepare_battle_actor_scene(const BattleActorSceneAssets& assets,
                                const BattleActorSceneRules& rules, BattleActorScene& state) {
    auto next = state;
    auto& characters = next.characters;
    require(!next.animation.echo_character || *next.animation.echo_character < 4,
            "Invalid echo character");
    if (next.echo_reset_serial != next.animation.echo_reset_serial) {
        next.echo_history = {};
        next.echo_reset_serial = next.animation.echo_reset_serial;
    }
    // C2/BF53 selects the countdown refresh cadence; each value is encoded
    // as displayed seconds + 1. Retain normal and reversed glyphs separately.
    if ((next.condemned_phase & FF6_ROM_VALUE_ACTOR_COUNTDOWN_REFRESH_MASK_0) == 0) {
        const auto slot = (next.condemned_phase >> std::countr_one(
            unsigned{FF6_ROM_VALUE_ACTOR_COUNTDOWN_REFRESH_MASK_0})) & 3U;
        auto& normal = next.condemned_digits[slot];
        auto& flipped = next.condemned_flipped[slot];
        normal = {};
        flipped = {};
        if (rules.condemned[slot]) {
            require(assets.status_sprites && assets.status_sprites->tiles.size() >= 234,
                    "Missing condemned numeral graphics");
            const unsigned value = rules.condemned[slot] - 1U;
            require(value < 100, "Condemned numeral exceeds two digits");
            const auto& tiles = assets.status_sprites->tiles;
            normal = {tiles[208 + value / 10], tiles[208 + value % 10]};
            flipped = {tiles[224 + value % 10], tiles[224 + value / 10]};
        }
    }
    next.condemned_phase = wrap8(unsigned{next.condemned_phase} + 1U);
    for (unsigned i = 0; i < 4; ++i) {
        auto& overlay = characters.control[i].condemned_overlay;
        if (!rules.condemned[i])
            overlay.reset();
        else {
            const auto& pose = characters.layout.poses[i];
            const bool reversed = ((pose.facing & 1U) != 0) != (pose.current_frame >= 48);
            overlay = reversed ? next.condemned_flipped[i] : next.condemned_digits[i];
        }
    }
    auto& pieces = next.publication.prepared;
    pieces.clear();
    publish_battle_draw_order(next.order);
    update_battle_character_x(characters.layout.poses);
    for (const auto& target : next.order.published) {
        if (!target.participant)
            continue;
        const auto identity = unsigned{*target.participant};
        require(identity < 10, "Invalid battle drawing participant");
        if (identity < 4) {
            const auto slot = static_cast<std::uint8_t>(identity);
            const auto& control = characters.control[slot];
            if (!rules.shown_characters[slot] || !next.animation.visible[slot] ||
                characters.layout.graphics_ids[slot] == 255 ||
                (control.active_status & status_mask(BattleStatus::hide)) != 0 ||
                rules.hide_characters)
                continue;
            BattleCharacterDrawContext context;
            context.magitek = rules.magitek;
            context.shown_status = control.shown_status;
            context.enemy = rules.enemy_characters[slot];
            context.battle_time_stopped = rules.battle_time_stopped;
            context.forced_running_background = rules.forced_running_background;
            context.run_buttons = rules.run_buttons;
            context.run_animation_blocked = state.animation.run_animation_blocked;
            auto& pose = characters.layout.poses[slot];
            if (!rules.magitek && control.status_sprite != 0) {
                require(assets.status_sprites.has_value() && control.status_sprite <= 7,
                        "Missing character status sprite resources");
                // DrawStatusSprites precedes DrawCharSprite, so its stance is
                // the retained frame, before the character advances this pass.
                const bool kneeling = pose.current_frame == FF6_ROM_VALUE_ACTOR_STATUS_KNEELING_FRAMES_0 ||
                    pose.current_frame == FF6_ROM_VALUE_ACTOR_STATUS_KNEELING_FRAMES_1;
                const auto& data = *assets.status_sprites;
                const auto& layout = data.layouts[control.status_sprite - 1U + (kneeling ? 7U : 0U)];
                auto style = layout.style;
                const bool mirrored = ((pose.facing & 1U) != 0) != (pose.current_frame >= 48);
                style.flip_x = style.flip_x || mirrored;
                style.flip_y = style.flip_y || (pose.facing & 2U) != 0;
                style.priority |= pose.appearance.priority;
                const auto x = wrap16(unsigned{pose.x} + pose.offset_x + pose.animation_x +
                    data.mirrored_x[mirrored ? control.status_sprite : 0]);
                unsigned y = pose.y, carry = 0;
                const auto floating = wrap16(static_cast<std::uint32_t>(
                    battle_character_y_offset(assets.view, control.shown_status, pose, false)));
                for (const auto offset : {pose.offset_y, pose.jump_y, floating}) {
                    const unsigned sum = y + offset + carry;
                    y = wrap16(sum);
                    carry = sum > 65535U;
                }
                pieces.emplace_back(BattleStatusScenePiece{slot, layout.tile,
                    project_battle_piece(x, static_cast<std::uint16_t>(y), layout.x, layout.y), style});
            }
            auto character = draw_battle_character(assets.view, context, pose);
            if (rules.magitek) {
                const auto armor = draw_battle_armor(
                    assets.view, assets.graphics, pose, slot, next.animation.armor_actions[slot],
                    wrap8(next.clock), rules.battle_time_stopped, characters.layout.armor_palette,
                    next.rider_offsets[slot], character);
                if (armor.frame_update)
                    next.publication.pending_armor =
                        BattleArmorImageRequest{slot, armor.frame_update->frame};
                pieces.emplace_back(BattleCharacterScenePiece{slot, character});
                pieces.emplace_back(BattleArmorScenePiece{slot, armor});
            } else {
                pieces.emplace_back(BattleCharacterScenePiece{slot, character});
                if (next.animation.reflected_characters & (1U << slot)) {
                    // DrawCharSprite's second call uses the ROM-selected displacement
                    // and priority with vertical flip. Retain its frame counter.
                    auto reflected = pose;
                    reflected.y = wrap16(unsigned{reflected.y} + FF6_ROM_VALUE_ACTOR_REFLECTION_DISPLACEMENT_0);
                    reflected.appearance.flip_y = true;
                    reflected.appearance.priority = FF6_ROM_VALUE_ACTOR_REFLECTION_PRIORITY_0;
                    pieces.emplace_back(BattleCharacterScenePiece{
                        slot, draw_battle_character(assets.view, context, reflected)});
                    pose.counter = reflected.counter;
                    pose.current_frame = reflected.current_frame;
                }
                // C1/2A4A samples eight poses and alternates four even/odd
                // ages. It follows the ordinary character, never the armor.
                if (next.animation.echo_character == slot) {
                    if ((next.clock & next.animation.echo_delay_mask) == 0) {
                        std::move_backward(next.echo_history.begin(), next.echo_history.end() - 1,
                                           next.echo_history.end());
                        next.echo_history[0] = BattleCharacterScenePiece{slot, character};
                    }
                    for (unsigned age = next.clock & 1U; age < next.echo_history.size(); age += 2)
                        if (next.echo_history[age])
                            pieces.emplace_back(*next.echo_history[age]);
                }
            }
            // C1/294D, C1/2975 and C1/2D92: Image rotates through the four
            // slots, drawing a copy after that character's ordinary pieces.
            // Use the displayed status, so effect publication and removal
            // retain their normal presentation boundary.
            if (slot == next.image_phase &&
                (control.shown_status & status_mask(BattleStatus::image)) != 0) {
                context.image_copy = true;
                pieces.emplace_back(BattleCharacterScenePiece{
                    slot, draw_battle_character(assets.view, context, pose)});
            }
        } else {
            const auto slot = static_cast<std::uint8_t>(identity - 4U);
            if (!rules.shown_monsters[slot] || !next.animation.monster_entry_visible[slot] ||
                !next.animation.monster_visible[slot] || rules.invisible_monsters[slot])
                continue;
            for (const auto& piece :
                 project_battle_monster(assets.monster_pieces, slot, next.monsters.positions[slot],
                                        rules.retained_monster_facing[slot],
                                        next.monster_styles[slot], next.order.monster_masks[slot]))
                pieces.emplace_back(BattleMonsterScenePiece{slot, piece});
            if (next.animation.reflected_monsters & (1U << slot)) {
                auto position = next.monsters.positions[slot];
                position.y = wrap16(unsigned{position.y} + assets.monsters.dimensions[slot].height * 8U);
                auto style = next.monster_styles[slot];
                style.priority = FF6_ROM_VALUE_ACTOR_REFLECTION_PRIORITY_0;
                const auto facing = rules.retained_monster_facing[slot];
                // The original pre-combines facing, sets vertical, then the
                // ordinary projector applies its retained facing once more.
                position.flip = static_cast<std::uint8_t>((position.flip ^ facing) | 2U);
                for (const auto& piece : project_battle_monster(assets.monster_pieces, slot,
                         position, facing, style, next.order.monster_masks[slot]))
                    pieces.emplace_back(BattleMonsterScenePiece{slot, piece});
            }
        }
    }
    next.anchors = anchors(assets, next);
    next.image_preparation_work = publish_battle_character_frames(rules.magitek, characters);
    if (!rules.freeze_palettes)
        update_battle_character_palette(assets.palettes, wrap8(next.clock), characters);
    next.clock = wrap16(unsigned{next.clock} + 1U);
    next.image_phase = static_cast<std::uint8_t>((next.image_phase + 1U) & 3U);
    characters.layout.palette_phase = wrap8(unsigned{characters.layout.palette_phase} + 1U);
    state = std::move(next);
}
std::vector<BattleActorSpriteGroup>
make_battle_actor_sprite_groups(const BattleActorSceneAssets& assets,
                                const BattleActorPublication& publication,
                                const BattleActorPaletteOverrides& overrides) {
    std::vector<BattleActorSpriteGroup> groups;
    for (const auto& item : publication.visible) {
        std::visit(
            [&](const auto& piece) {
                using T = std::decay_t<decltype(piece)>;
                const auto participant = static_cast<std::uint8_t>(
                    piece.slot + (std::is_same_v<T, BattleMonsterScenePiece> ? 4U : 0U));
                if (groups.empty() || groups.back().participant != participant)
                    groups.push_back({participant, {}});
                std::vector<RasterSprite> out;
                if constexpr (std::is_same_v<T, BattleCharacterScenePiece>) {
                    require(piece.slot < 4, "Invalid character scene slot");
                    for (const auto& p : piece.draw)
                        require(p.style.palette >= 4 && p.style.palette < 8,
                                "Unsupported character scene palette");
                    auto sprites = make_battle_character_sprites(
                        piece.draw, publication.character_frames[piece.slot],
                        overrides[piece.slot].value_or(
                            publication.colors[piece.draw[0].style.palette - 4U]));
                    for (auto& sprite : sprites)
                        sprite.mix = {ColorBlend::none, ColorBlend::subtract};
                    out.insert(out.end(), sprites.begin(), sprites.end());
                } else if constexpr (std::is_same_v<T, BattleArmorScenePiece>) {
                    require(piece.slot < 4 && piece.draw.style.palette >= 4 &&
                                piece.draw.style.palette < 8,
                            "Invalid armor scene slot/palette");
                    if (const auto selected = publication.armor_frames[piece.slot]) {
                        validate_armor(assets, {piece.slot, *selected});
                        out.push_back(make_battle_armor_sprite(
                            piece.draw, assets.graphics.magitek_frames[*selected],
                            publication.colors[piece.draw.style.palette - 4U]));
                        out.back().mix = {ColorBlend::none, ColorBlend::subtract};
                    }
                    // Cleared initial armor image has no opaque pixels until its
                    // first due publication. Absence explicitly owns that blank.
                } else if constexpr (std::is_same_v<T, BattleStatusScenePiece>) {
                    require(piece.slot < 4 && piece.tile <= 14 && piece.style.palette >= 4 &&
                                piece.style.palette < 8 && piece.style.priority < 4,
                            "Invalid status sprite publication");
                    RasterSprite sprite;
                    sprite.x = piece.position.x;
                    sprite.y = piece.position.y;
                    sprite.width = sprite.height = 16;
                    constexpr std::array<std::uint16_t, 4> depths{2, 3, 6, 9};
                    sprite.depth = depths[piece.style.priority];
                    sprite.flip_x = piece.style.flip_x;
                    sprite.flip_y = piece.style.flip_y;
                    sprite.colors = publication.colors[piece.style.palette - 4U];
                    sprite.mix = {ColorBlend::none, ColorBlend::subtract};
                    for (unsigned y = 0; y < 16; ++y)
                        for (unsigned x = 0; x < 16; ++x)
                            sprite.pixels.push_back(publication.status_tiles[
                                piece.tile + (y / 8) * 16 + x / 8][(y % 8) * 8 + x % 8]);
                    out.push_back(std::move(sprite));
                } else {
                    require(piece.slot < 6 && piece.draw.style.palette < 3,
                            "Invalid monster scene slot/palette");
                    const auto& colors = assets.monsters.colors[piece.draw.style.palette];
                    require(colors.has_value(), "Visible monster has no owned palette");
                    out.push_back(make_battle_monster_sprite(
                        piece.draw, publication.monster_tiles ? *publication.monster_tiles
                                                             : assets.monsters.tiles,
                        overrides[piece.slot + 4].value_or(
                            publication.monster_colors
                                ? (*publication.monster_colors)[piece.draw.style.palette]
                                : *colors)));
                }
                const bool color_math =
                    std::is_same_v<T, BattleArmorScenePiece> || std::is_same_v<T, BattleStatusScenePiece> ||
                    (std::is_same_v<T, BattleCharacterScenePiece> && !overrides[piece.slot]);
                for (auto& sprite : out)
                    groups.back().sprites.push_back({std::move(sprite), color_math});
            },
            item);
    }
    return groups;
}
std::vector<RasterSprite> make_battle_actor_sprites(const BattleActorSceneAssets& assets,
                                                    const BattleActorPublication& publication,
                                                    const BattleActorPaletteOverrides& colors) {
    std::vector<RasterSprite> out;
    for (auto& group : make_battle_actor_sprite_groups(assets, publication, colors))
        for (auto& sprite : group.sprites)
            out.push_back(std::move(sprite.image));
    return out;
}
} // namespace ff6
