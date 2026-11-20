#include "ff6/battle_effects.hpp"
#include "rom-materials/final_battle_viewport.macros.inc"
#include "rom-materials/battlefield_horizontal_window.macros.inc"
#include "rom-materials/battle_shake_phase_mask.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
constexpr std::array<std::uint16_t, 4> depths{2, 3, 6, 9};
void mosaic(TileLayer& layer, const BattleEffectDisplay& context, unsigned identity) {
    for (unsigned y = 0; y < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1) && y < layer.lines.size(); ++y) {
        auto& line = layer.lines[y];
        line.sample_width = context.mosaic_layers[identity] ? context.mosaic_size : 1;
        line.sample_row =
            context.mosaic_layers[identity]
                ? std::optional{static_cast<std::int32_t>(y - y % context.mosaic_size)}
                : std::nullopt;
    }
}
std::vector<BattleEffectPiece> pieces(const BattleEffectImage& image, std::uint8_t frame,
                                      BattleEffectLayer layer) {
    if (frame == (layer == BattleEffectLayer::foreground ? 15
                  : layer == BattleEffectLayer::overlay  ? 9
                                                         : 31) ||
        (layer == BattleEffectLayer::foreground && frame == 31))
        return {};
    // Background scripts can retain the shared blank-frame identity after
    // selecting the overlay plane. Honor that authored empty image when it is
    // outside the loaded frames; a real frame at that identity stays visible.
    // See BATTLE_SPELL_PRESENTATION.md for the Rasp source and visible experiment.
    if (layer == BattleEffectLayer::overlay && frame == 15 && frame >= image.frames.size())
        return {};
    if (frame >= image.frames.size())
        throw std::invalid_argument("Attack frame " + std::to_string(frame) + " outside " +
            std::to_string(image.frames.size()) + " loaded frames on layer " +
            std::to_string(static_cast<unsigned>(layer)));
    return image.frames[frame];
}
std::uint8_t pixel(const BattleEffectImage& image, BattleEffectPiece piece, unsigned x, unsigned y,
                   const std::optional<BattlePublishedCharacterFrames>& characters,
                   const std::optional<BattlePublishedStatusTiles>& statuses,
                   bool character_tiles = false) {
    if (piece.flip_x)
        x = 15 - x;
    if (piece.flip_y)
        y = 15 - y;
    const auto tile = unsigned{piece.tile} + x / 8 + (y / 8) * 16;
    if (image.character_frames || character_tiles) {
        require(characters.has_value(), "Character effect needs published actor images");
        if (image.adjacent_status_frames) {
            require(statuses.has_value(), "Actor effect needs published status images");
            return battle_actor_background_tile(*characters, *statuses, tile)[(y & 7U) * 8 + (x & 7U)];
        }
        return battle_character_background_tile(*characters, tile)[(y & 7U) * 8 + (x & 7U)];
    }
    require(tile < image.tiles.size() && (image.owned_tiles.empty() ||
                (tile < image.owned_tiles.size() && image.owned_tiles[tile])),
            "Attack piece references shared/retained tiles");
    return image.tiles[tile][(y & 7U) * 8 + (x & 7U)];
}
} // namespace
void apply_battle_effect_display(const BattleEffectDisplay& context, RasterScene& scene) {
    require(context.brightness <= 15 && context.mosaic_size >= 1 && context.mosaic_size <= 16,
            "Invalid attack display state");
    for (auto& intensity : scene.intensity)
        intensity = std::min(intensity, context.brightness);
    if (!scene.layers.empty())
        mosaic(scene.layers.front(), context, 1);
}
void BattleAttackAnimation::compose(
    RasterScene& scene, std::span<const BattleActorSpriteGroup> actors,
    const BattleAttackAnimation* prepared_sprites, const BattleEffectDisplay* visible_display,
    std::span<const std::array<Rgb555, 16>> visible_battlefield_colors,
    std::span<const std::array<Rgb555, 16>> visible_effect_colors) const {
    require(visible_effect_colors.empty() || visible_effect_colors.size() == 4,
            "Published effect palettes have the wrong extent");
    const auto colors = [&](unsigned layer) -> const std::array<Rgb555, 16>& {
        return visible_effect_colors.empty()
                   ? (layer == 3 ? secondary_foreground_colors() : effect_colors(static_cast<BattleEffectLayer>(layer)))
                   : visible_effect_colors[layer];
    };
    const auto& display = visible_display ? *visible_display : context_.display;
    apply_battle_effect_display(display, scene);
    if (!visible_battlefield_colors.empty()) {
        require(!scene.layers.empty() &&
                    visible_battlefield_colors.size() == scene.layers.front().palettes.size(),
                "Published battlefield palette has the wrong extent");
        scene.layers.front().palettes.assign(visible_battlefield_colors.begin(),
                                             visible_battlefield_colors.end());
    }
    // Script completion cannot erase an older sprite description that the
    // display pipeline has already prepared but has not yet shown.
    if (complete_ && !event_programs_ && !hold_completed_frame_ &&
        (!prepared_sprites || prepared_sprites->complete_)) {
        if (actors.empty())
            return;
        if (!scene.objects) {
            scene.objects.emplace();
            scene.objects->lines.resize(scene.height);
        }
        for (const auto& actor : actors)
            for (const auto& sprite : actor.sprites)
                scene.objects->sprites.push_back(sprite.image);
        return;
    }
    require(scene.width == 256 && scene.height >= (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1) && !scene.layers.empty(),
            "Attack effects require a canonical battlefield");
    // Do not discard the original D6 high bytes on a non-repeating source.
    // Ordinary extracted backgrounds prove this period through equal halves;
    // custom layouts must supply a separate owned displacement contract.
    const auto& base = scene.layers[0];
    for (unsigned axis = 0; axis < 2; ++axis)
        if (scrolling_[1].axes[axis].byte_displacement) {
            const unsigned extent = 8U * (axis == 0 ? base.columns : base.rows);
            require((axis == 0 ? base.wrap_x : base.wrap_y) && extent && 256U % extent == 0,
                    "Byte background displacement requires a verified 256-pixel source period");
        }
    if (!scene.objects) {
        scene.objects.emplace();
        scene.objects->lines.resize(scene.height);
    }
    const auto mix = [&](unsigned bit) -> CompositeBlend {
        const auto operation = (blended_layers_ & bit) ? blend_ : ColorBlend::none;
        return {operation, fixed_color_ ? operation : ColorBlend::none};
    };
    if (fixed_color_) {
        scene.operand_fallback_rows.resize(scene.height, scene.operand_fallback);
        const auto colors = presentation_timing_ ? raster_picture_.colors() : gradient_.rows;
        std::copy(colors.begin(), colors.end(), scene.operand_fallback_rows.begin());
    }
    const auto depth = [&](std::uint16_t value) -> std::uint16_t {
        // When the overlay loses its top priority it sits between object
        // priorities zero and one. Move the intervening depths together.
        return affine_ && affine_->enabled
                   ? (value == 2 ? 2 : static_cast<std::uint16_t>(value + 1))
               : !overlay_high_priority_ && value >= 3 && value <= 9
                   ? static_cast<std::uint16_t>(value + 1)
                   : value;
    };
    // Battle backgrounds are semantic BG2. Menus remain outside effect color
    // math and clipping; original hardware register values never enter the API.
    scene.layers[0].operand = (operand_layers_ & 2U) != 0;
    scene.layers[0].visible = (visible_layers_ & 2U) != 0;
    scene.layers[0].mix = mix(2);
    for (auto& cell : scene.layers[0].cells)
        cell.depth = depth(cell.depth);
    if (visible_battlefield_colors.empty()) {
        if (!context_.battlefield_colors.empty()) {
            require(context_.battlefield_colors.size() == scene.layers[0].palettes.size(),
                    "Retained battlefield palette has the wrong extent");
            scene.layers[0].palettes = context_.battlefield_colors;
        } else
            for (auto& palette : scene.layers[0].palettes)
                for (auto& color : palette)
                    color = color_offsets_[3].apply(color);
    }
    const auto signed_word = [](std::uint16_t value) {
        return value < 32768U ? int{value} : int{value} - 65536;
    };
    // The foreground can reuse the terrain image. C1/0839 selects the
    // battlefield origin in that case, before either layer adds its own wave;
    // it also omits the ordinary foreground's one-row correction.
    const auto terrain_origins =
        battlefield_layout_ ? scene.layers[0].lines : std::vector<RasterLine>{};
    for (unsigned y = 0; y < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1) && y < scene.layers[0].lines.size(); ++y) {
        auto& line = scene.layers[0].lines[y];
        line.offset_x += signed_word(scrolling_[1].axes[0].offsets[y & 31U]);
        line.offset_y += signed_word(scrolling_[1].axes[1].offsets[y & 31U]);
        if (display.screen_shake) {
            // C1/0D03 selects adjacent bytes by presentation frame, forcing
            // the high byte to FF. This affects terrain, not actors or menus.
            const auto phase = context_.presentation_tick & static_cast<unsigned>(FF6_ROM_VALUE_BATTLE_SHAKE_PHASE_MASK_0);
            line.offset_x += int{library_->screen_shake[phase]} - 256;
            if (display.screen_shake != 128)
                line.offset_y += int{library_->screen_shake[phase + 1]} - 256;
        }
    }
    // Effect/monster palettes occupy the lower object palette banks and are
    // exempt from math as main pixels. All objects may still supply operands.
    scene.objects->operand = (operand_layers_ & 16U) != 0;
    scene.objects->visible = (visible_layers_ & 16U) != 0;
    if (blended_layers_ & 32U) {
        // Odin blends the fixed gradient into uncovered battlefield pixels as
        // well as terrain. The original inverted color window excludes the
        // side borders. A clipped solid plane expresses that backdrop coverage
        // without changing the renderer's global backdrop or the menu below.
        TileLayer backdrop;
        backdrop.columns = backdrop.rows = 1;
        backdrop.cells.resize(1);
        backdrop.tiles.emplace_back().fill(1);
        backdrop.palettes.emplace_back()[1] = scene.backdrop;
        backdrop.mix = mix(32);
        backdrop.lines.resize(scene.height);
        for (unsigned y = 0; y < scene.height; ++y) {
            auto& line = backdrop.lines[y];
            line.enabled = y >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 && y < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1);
            line.left = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0;
            line.right = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1;
        }
        scene.layers.push_back(std::move(backdrop));
    }
    if (affine_ && affine_->enabled) {
        require(!display.mosaic_layers[0] || display.mosaic_size == 1,
                "Mosaic on an affine attack image needs a sampling contract");
        require(affine_image_ && images_[1], "Affine projection has no owned image");
        auto layer = *affine_image_;
        layer.depth = 3;
        std::copy(colors(1).begin(), colors(1).end(), layer.colors.begin());
        layer.visible = (visible_layers_ & 1U) != 0;
        layer.operand = (operand_layers_ & 1U) != 0;
        layer.clip_operand = false;
        layer.mix = mix(1);
        for (unsigned y = 0; y < scene.height; ++y) {
            auto line = project_battle_effect_affine(*affine_, background_views_[0], y);
            // This is a finite authored image, not a video-memory surface.
            // Rotation may cross its vertical bounds; the rasterizer clips
            // those samples transparently. Keep the unwrapped intercept so
            // a negative intercept can still enter the image later in a row.
            // Original leftover-memory edge pixels are a documented deferred
            // presentation difference, not missing content or a fatal error.
            layer.lines.push_back(line);
        }
        scene.affine_layers.push_back(std::move(layer));
    }
    for (unsigned layer_index = 1; layer_index <= 2; ++layer_index) {
        if (layer_index == 1 && affine_ && affine_->enabled)
            continue;
        const bool copied_monsters = layer_index == 1 && monster_background_.has_value();
        const bool battlefield = layer_index == 1 && battlefield_layout_;
        if (layer_index == 1)
            require(battlefield_layout_ == battlefield_pixels_,
                    "Foreground layout and image owners do not match");
        if (!battlefield && !copied_monsters &&
            (!images_[layer_index] || !background_frames_[layer_index - 1]))
            continue;
        const auto layer_kind = static_cast<BattleEffectLayer>(layer_index);
        TileLayer layer;
        if (battlefield) {
            // Confuser's family distorts a second view of the same terrain.
            // Keep its source layout, palettes and transparent texels; objects
            // and menu pixels are not part of this image.
            layer = scene.layers.front();
            for (auto& cell : layer.cells)
                cell.depth = cell.depth == depth(7) ? depth(8) : depth(5);
        } else if (copied_monsters) {
            layer = monster_background_->image;
            if (copied_background_colors_ && layer.palettes.size() > 1)
                layer.palettes[1] = !visible_effect_colors.empty() || images_[1]
                                        ? colors(1) : *copied_background_colors_;
            for (auto& cell : layer.cells)
                cell.depth = depth(cell.depth);
        } else {
            const auto& image = *images_[layer_index];
            // Retained 256-pixel images occupy independent quadrants. Uploading a
            // new top-left frame must not erase an off-screen image (e.g. Shiva).
            layer.columns = layer.rows = 64;
            layer.tiles.emplace_back();
            layer.cells.resize(64 * 64);
            layer.palettes.push_back(colors(layer_index));
            if (layer_index == 1) layer.palettes.push_back(colors(3));
            const bool horizontal = background_flipped_[layer_index - 1];
            for (unsigned quadrant = 0; quadrant < 4; ++quadrant) {
                const auto frame = layer_index == 1 && sketch_layout_
                    ? (quadrant == 0 ? sketch_frame_ : std::nullopt)
                    : background_images_[layer_index - 1][quadrant];
                if (!frame)
                    continue;
                for (const auto piece : pieces(image, *frame, layer_kind)) {
                    const auto column =
                        horizontal ? 30U - unsigned{piece.x} / 8 : unsigned{piece.x} / 8;
                    const auto row = unsigned{piece.y} / 8;
                    for (unsigned by = 0; by < 2; ++by)
                        for (unsigned bx = 0; bx < 2; ++bx) {
                            Tile tile{};
                            for (unsigned y = 0; y < 8; ++y)
                                for (unsigned x = 0; x < 8; ++x)
                                    tile[y * 8 + x] = pixel(
                                        image, piece, horizontal ? 15 - (bx * 8 + x) : bx * 8 + x,
                                        by * 8 + y, context_.published_character_frames,
                                        context_.published_status_tiles);
                            auto& cell = layer.cells[(row + by + (quadrant / 2) * 32) * 64 +
                                                     column + bx + (quadrant % 2) * 32];
                            if (layer_index == 1 && *frame < image.secondary_palette_cells.size())
                                cell.palette = image.secondary_palette_cells[*frame][(row / 2) * 16 + column / 2] ? 1 : 0;
                            cell.tile = static_cast<std::uint32_t>(layer.tiles.size());
                            cell.depth = layer_index == 1         ? depth(8)
                                         : overlay_high_priority_ ? 10
                                                                  : 3;
                            if (layer_index == 1 && *frame == 0 && row + by < foreground_low_rows_)
                                cell.depth = depth(4);
                            layer.tiles.push_back(tile);
                        }
                }
            }
        }
        const auto origin = background_views_[layer_index - 1];
        layer.lines.resize(scene.height);
        for (unsigned y = 0; y < scene.height; ++y) {
            auto& line = layer.lines[y];
            line.enabled = y >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 && y < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1);
            line.left = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0;
            line.right = FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1;
            if (y < mask_.rows.size() && masked_backgrounds_[layer_index - 1]) {
                const auto edge = presentation_timing_ ? raster_picture_.mask()[y] : mask_.rows[y];
                line.left = std::max<std::uint16_t>(FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0, edge.left);
                line.right = std::max(line.left, std::min<std::uint16_t>(FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1, edge.right));
            }
            line.offset_x = signed_word(origin.x);
            // The overlay writes its vertical origin directly. Unlike the
            // ordinary foreground, its original publication never subtracts
            // one; canonical screen row zero therefore samples source row one.
            line.offset_y = signed_word(origin.y) + (layer_index == 2 ? 1 : 0);
            if (battlefield) {
                line.offset_x = y < terrain_origins.size() ? terrain_origins[y].offset_x : 0;
                line.offset_y = y < terrain_origins.size() ? terrain_origins[y].offset_y : 0;
            }
            if (y < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1)) {
                const auto scroll_layer = layer_index == 1 ? 0U : 2U;
                const auto& scroll = scrolling_[scroll_layer];
                if ((scroll.pattern == BattleScrollPattern::wave32 ||
                     scroll.pattern == BattleScrollPattern::wave64) && layer_index == 1) {
                    const auto row = y & (scroll.pattern == BattleScrollPattern::wave64 ? 63U : 31U);
                    line.offset_x += signed_word(scroll.axes[0].offsets[row]);
                    line.offset_y += signed_word(scroll.axes[1].offsets[row]);
                } else if (scroll.pattern != BattleScrollPattern::fixed) {
                    require(scroll.projection_owned, "Per-line scroll needs an owned projection");
                    // These projections are written directly (BG1 full rows,
                    // BG3 full/repeating rows). Ordinary BG1/BG2 waves instead
                    // publish prepared displacements at the frame boundary.
                    // Keep mode changes with that publication; only a retained
                    // direct projection can supply an earlier picture sample.
                    const auto& prior = picture_scrolling_ ? (*picture_scrolling_)[scroll_layer] : scroll;
                    const auto& projection = prior.pattern == scroll.pattern && prior.projection_owned
                                                 ? prior.projection : scroll.projection;
                    const auto p =
                        projection[scroll.pattern == BattleScrollPattern::wave32 ? y & 31U
                                                                                        : y];
                    line.offset_x = signed_word(p.x);
                    // Ordinary BG1 subtracts one before publication. Direct
                    // per-line effects omit that subtraction; canonical row
                    // coordinates must preserve the one-line difference.
                    line.offset_y = signed_word(p.y) + 1;
                }
            }
        }
        layer.wrap_x = layer.wrap_y = true;
        const auto bit = layer_index == 1 ? 1U : 4U;
        layer.operand = (operand_layers_ & bit) != 0;
        // The attack caller enables visible masking; its operand masking is
        // disabled. A clipped main image can still supply a blend operand.
        layer.clip_operand = false;
        layer.visible = (visible_layers_ & bit) != 0;
        layer.mix = mix(bit);
        mosaic(layer, display, layer_index == 1 ? 0 : 2);
        scene.layers.push_back(std::move(layer));
    }
    const auto& objects = prepared_sprites ? *prepared_sprites : *this;
    std::array<std::array<std::vector<RasterSprite>, 2>, 10> effects;
    std::array<std::vector<RasterSprite>, 3> summons;
    for (unsigned i = 0; objects.images_[0] && i < objects.tasks_.size(); ++i) {
        if ((i >= 80 && i < 82) || (i < 80 && i % 8 >= objects.sprite_task_count(i >= 32)))
            continue;
        if (!objects.tasks_[i] || !objects.tasks_[i]->script.active ||
            !objects.tasks_[i]->sprite_visible || !objects.tasks_[i]->script.effect_frame)
            continue;
        const auto& task = *objects.tasks_[i];
        const auto& selected = task.selected_sprite;
        require(!selected.supplemental_image || objects.supplemental_image_.has_value(),
                "Effect task lost its supplemental image");
        const auto& image =
            selected.supplemental_image ? *objects.supplemental_image_ : *objects.images_[0];
        const auto& s = task.script;
        // Dedicated summon tasks have only a front-sprite drawing pass.
        if (i >= 82 && task.behind_actor)
            continue;
        const auto frame_pieces = pieces(image, *s.effect_frame, BattleEffectLayer::sprite);
        if (frame_pieces.empty())
            continue;
        const auto geometry = image.frame_geometry.at(*s.effect_frame);
        for (auto piece : frame_pieces) {
            const int ox = selected.flipped ? -int(piece.x) - 16 + geometry.width * 8
                                            : int(piece.x) - geometry.width * 8;
            const int oy = int(piece.y) - geometry.height * 8;
            const auto signed_byte = [](int value) {
                const auto b = wrap8(static_cast<std::uint32_t>(value));
                return static_cast<std::int16_t>(b < 128 ? int(b) : int(b) - 256);
            };
            const auto p = project_battle_piece(selected.position.x, selected.position.y,
                                                signed_byte(ox), signed_byte(oy));
            RasterSprite sprite;
            sprite.x = p.x;
            sprite.y = p.y;
            sprite.width = sprite.height = 16;
            sprite.depth = depth(depths[selected.priority]);
            // Palette publication is current even when sprite positions are from
            // the preceding preparation. Tile/image ownership remains explicit.
            sprite.colors = selected.palette == 3
                                ? (!visible_effect_colors.empty() ? colors(0)
                                   : selected.supplemental_image && supplemental_image_
                                       ? supplemental_image_->colors
                                   : images_[0] ? images_[0]->colors
                                                : image.colors)
                                : context_.character_colors.at(selected.palette - 4);
            sprite.mix = selected.palette >= 4 ? mix(16) : CompositeBlend{};
            sprite.pixels.resize(256);
            piece.flip_x = piece.flip_x != selected.flipped;
            for (unsigned y = 0; y < 16; ++y)
                for (unsigned x = 0; x < 16; ++x) {
                    const auto dx = sprite.x + int(x), dy = sprite.y + int(y);
                    sprite.pixels[y * 16 + x] =
                        dx >= FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_0 && dx < (FF6_ROM_VALUE_BATTLEFIELD_HORIZONTAL_WINDOW_1 + 1) && dy >= FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 && dy < (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1)
                            ? pixel(image, piece, x, y, context_.published_character_frames,
                                    context_.published_status_tiles,
                                    selected.character_image)
                            : 0;
                }
            if (i < 80)
                effects[objects.context_.reflection_targets[i / 8].value_or(
                    static_cast<std::uint8_t>(i / 8))][task.behind_actor ? 1 : 0]
                    .push_back(std::move(sprite));
            else
                summons[i - 82].push_back(std::move(sprite));
        }
    }
    std::vector<RasterSprite> sprites;
    const auto append = [&](const auto& group) {
        sprites.insert(sprites.end(), group.begin(), group.end());
    };
    if (!objects.esper_behind_actors_)
        append(summons[0]);
    append(summons[1]);
    append(summons[2]);
    std::bitset<10> included;
    // Use the caller's retained participant order, before it stages the next
    // sort. Empty participants still own effects (for example, a hidden actor).
    const auto participant = [&](unsigned slot) {
        require(slot < 10 && !included[slot], "Invalid effect sprite group order");
        included[slot] = true;
        append(effects[slot][0]);
        for (const auto& actor : actors)
            if (actor.participant == slot &&
                (slot < 4 || !objects.hidden_background_monsters_[slot - 4])) {
                for (const auto& piece : actor.sprites) {
                    auto sprite = piece.image;
                    sprite.depth = depth(sprite.depth);
                    sprite.mix = (piece.color_math || (slot >= 4 && monster_palette_shift_))
                                     ? mix(16)
                                     : CompositeBlend{};
                    sprites.push_back(std::move(sprite));
                }
            }
        append(effects[slot][1]);
    };
    for (const auto& item : objects.context_.draw_order)
        if (item.participant)
            participant(*item.participant);
    for (unsigned slot = 0; slot < 10; ++slot)
        if (!included[slot])
            participant(slot);
    if (objects.esper_behind_actors_)
        append(summons[0]);
    scene.objects->sprites.insert(scene.objects->sprites.begin(), sprites.begin(), sprites.end());
}
} // namespace ff6
