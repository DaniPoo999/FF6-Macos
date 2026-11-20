#include "battle_effect_study.hpp"
#include "ff6/battle_background.hpp"
#include "ff6/content.hpp"
#include "ff6/equipment.hpp"
#include "ff6/field_camera.hpp"
#include "ff6/field_display.hpp"
#include "ff6/field_movement.hpp"
#include "ff6/field_object_view.hpp"
#include "ff6/io.hpp"
#include "ff6/main_menu.hpp"
#include "ff6/menu_navigation.hpp"
#include "ff6/music_player.hpp"
#include "ff6/party_menu.hpp"
#include "ff6/video.hpp"
#include "field_session_host.hpp"
#include "sdl_input.hpp"
#include <SDL3/SDL.h>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>

namespace {
enum class Page {
    portraits,
    fonts,
    windows,
    menu,
    field,
    scene,
    audio,
    mix,
    battle_background,
    battle_effect
};
struct MenuPreview {
    ff6::MainMenuNavigation navigation;
    ff6::CharacterMenuNavigation character_navigation;
    ff6::PartyData data;
    ff6::Roster roster;
    ff6::InputState input;
    ff6::InputSettings settings;
    std::optional<ff6::MainMenuRequest> pending;
    std::optional<ff6::CharacterMenuRequest> character_pending;
    bool selecting_character = false;
    bool memory = true, invalid_notice = false;

    explicit MenuPreview(const ff6::ContentPack& pack)
        : data(ff6::load_party_data(pack)), roster(ff6::new_roster(data)) {
        // Authored development composition, not a claimed opening game state.
        // Every name/stat comes from native initialization with extracted data.
        roster.active_party = 1;
        for (std::uint8_t i = 0; i < 3; ++i) {
            ff6::assign_character_name(roster, data, i, i);
            ff6::assign_character_properties(roster, data, i, i);
            roster.characters[i].graphics = i;
            roster.membership[i] = {true, false, 1, i};
            roster.recruited = static_cast<std::uint16_t>(roster.recruited | (1U << i));
        }
        // Apply the same equipment-derived vital flags needed by the future
        // field owner. This still uses the explicit development composition.
        (void)ff6::refresh_equipment(roster, data, ff6::load_equipment_data(pack),
                                     std::array<std::uint8_t, 3>{0, 1, 2});
    }

    void enter() {
        navigation.enter(memory);
        pending.reset();
        character_pending.reset();
        selecting_character = false;
        invalid_notice = false;
        input = {};
    }
    ff6::MenuNavigationUpdate tick(ff6::Buttons buttons) {
        ff6::sample_input(input, settings, ff6::InputContext::menu, buttons, 0);
        if (character_pending) {
            if (ff6::held(input.pressed, ff6::Button::b)) {
                character_pending.reset();
                character_navigation.enter(character_navigation.kind(),
                                           ff6::current_menu_party(roster), memory);
            }
            return {};
        }
        if (selecting_character) {
            const auto update =
                character_navigation.advance(input.pressed, input.repeating, roster, data);
            if (input.pressed || input.repeating)
                invalid_notice = update.invalid_mosaic;
            if (update.request == ff6::CharacterMenuRequest::sidebar) {
                selecting_character = false;
                navigation.enter(memory);
            } else if (update.request) {
                character_pending = update.request;
                if (character_navigation.phase() == ff6::MenuNavigationPhase::leaving)
                    character_navigation.finish_transition();
            }
            return {std::nullopt, update.sounds, update.sound_count, update.invalid_mosaic};
        }
        if (pending) {
            if (ff6::held(input.pressed, ff6::Button::b)) {
                // Development-only return; no fabricated character/submenu state.
                pending.reset();
                return navigation.enter(memory, input.repeating);
            }
            return {};
        }
        const auto update = navigation.advance(input.pressed, input.repeating, false);
        if (input.pressed || input.repeating)
            invalid_notice = update.invalid_mosaic;
        if (update.request) {
            std::optional<ff6::CharacterMenuKind> kind;
            switch (*update.request) {
            case ff6::MainMenuRequest::skill_character:
                kind = ff6::CharacterMenuKind::skills;
                break;
            case ff6::MainMenuRequest::equipment_character:
                kind = ff6::CharacterMenuKind::equipment;
                break;
            case ff6::MainMenuRequest::relic_character:
                kind = ff6::CharacterMenuKind::relic;
                break;
            case ff6::MainMenuRequest::status_character:
                kind = ff6::CharacterMenuKind::status;
                break;
            default:
                break;
            }
            if (kind) {
                character_navigation.enter(*kind, ff6::current_menu_party(roster), memory);
                selecting_character = true;
            } else
                pending = update.request;
            // Inspector displays the handoff; it does not claim to run the fade.
            if (navigation.phase() == ff6::MenuNavigationPhase::leaving)
                navigation.finish_transition();
        }
        return update;
    }
};
struct FieldPreview {
    ff6::FieldDisplay display;
    ff6::FieldObjectViewAssets object_assets;
    ff6::FieldSpritePublication published;
    ff6::FieldObjects objects;
    ff6::FieldMoveRates move_rates;
    ff6::FieldScroll camera;
    bool follow_camera = true;
    ff6::FieldPlayerTerrain terrain;
    ff6::FieldLocalTiles local_tiles;
    ff6::InputState input;
    ff6::InputSettings settings;
    ff6::Buttons step_request = 0;
    ff6::FieldStep last_step;
    std::array<std::array<ff6::Rgb555, 16>, 8> sprite_colors;
    std::uint8_t visits = 0;
    bool show_actor = true, smoke_on_foot = false, smoke_camera_checked = false;
    unsigned smoke_directions = 0;
    ff6::Rgb555 font_color;
    bool animate = true;
    std::uint8_t layer = 0;
    bool combined = true;
    std::array<ff6::FieldOrigin, 3> origins{};
    std::int32_t x = 0, y = 0;
    std::string error;
    static ff6::FieldDisplay load(const ff6::ContentPack& pack) {
        auto map = ff6::load_field_map(pack, 19);
        auto animation = ff6::load_field_animation(pack, map.definition);
        // Explicit development phase; native startup will supply retained phase.
        return ff6::FieldDisplay(std::move(map), std::move(animation), false);
    }
    FieldPreview(const ff6::ContentPack& pack, const ff6::MainMenuAssets& menu)
        : display(load(pack)), object_assets(ff6::load_field_object_view(pack)),
          move_rates(ff6::load_field_move_rates(pack)), font_color(menu.text_colors[3]) {
        display.set_window_colors(menu.windows.skins[0].colors);
        display.refresh_text_color(font_color);
        display.commit(15);
        std::copy_n(object_assets.sprites.palettes.begin(), 8, sprite_colors.begin());
        objects.update_order[0] = 0;
        objects.update_count = 1;
        objects.animation_queue[0] = 0;
        auto& actor = objects.objects[0];
        actor.animation_slot = 0;
        actor.membership.visible = actor.membership.enabled = true;
        actor.x = 38 * 16 * 256;
        actor.y = 49 * 16 * 256;
        actor.facing = 2;
        actor.walking_animation = true;
        actor.upper.palette = actor.lower.palette = 2;
        actor.upper.priority = actor.lower.priority = 2;
        actor.ride = ff6::FieldRide::magitek;
        actor.rider_visible = true;
        actor.speed = 2;
        local_tiles = ff6::field_local_tiles(display.map(), actor.x, actor.y);
        ff6::initialize_field_player_terrain(actor, terrain, local_tiles, 1);
        refresh_actor();
        const auto& map = display.map();
        camera = ff6::initialize_field_scroll(map.definition, map.parallax, 38, 49);
        reset();
    }
    void tick(ff6::Buttons buttons = 0) {
        display.finish();
        display.commit(15);
        display.advance_palette();
        display.refresh_text_color(font_color);
        visits = ff6::wrap8(unsigned{visits} + 1U);
        objects.event_visits = visits;
        ff6::sample_input(input, settings, ff6::InputContext::field, buttons, 0);
        auto& actor = objects.objects[0];
        if (visits % 4 == 0 && actor.x % 4096 == 0 && actor.y % 4096 == 0) {
            local_tiles = ff6::field_local_tiles(display.map(), actor.x, actor.y);
            // This fixture has one actor and no NPC population. Terrain decisions
            // are native; occupancy, step effects and the full scheduler remain open.
            const auto decision = ff6::plan_field_player_step(
                actor, terrain, local_tiles, {}, input.held | step_request, {}, move_rates,
                object_assets.sprites.selection);
            if (decision.kind != ff6::FieldStep::Kind::stationary ||
                last_step.kind != ff6::FieldStep::Kind::door)
                last_step = decision;
            step_request = 0;
            const auto& map = display.map();
            ff6::FieldScrollFocus focus;
            focus.party_tile = {ff6::wrap8(actor.x / 4096U), ff6::wrap8(actor.y / 4096U)};
            focus.party_speed = {actor.velocity_x, actor.velocity_y};
            ff6::update_field_scroll_rate(camera, ff6::field_scroll_limits(map.definition),
                                          map.parallax.multiplier, focus);
        }
        // Native field order advances scroll before integrating object positions.
        // Inspection panning changes the view only; the camera keeps its history.
        ff6::advance_field_scroll(camera);
        if (follow_camera)
            origins = ff6::field_scroll_origins(camera);
        ff6::step_field_objects(objects, object_assets,
                                {visits, visits, (local_tiles.cells[7].flags & 192U) != 0,
                                 local_tiles.cells[7].directions != 255 &&
                                     (local_tiles.cells[7].directions & 64U) != 0});
        ff6::latch_field_object_poses(objects);
        ff6::publish_field_objects(objects, object_assets, published);
    }
    void refresh_actor() {
        auto& actor = objects.objects[0];
        actor.pending_pose =
            actor.ride == ff6::FieldRide::none
                ? object_assets.sprites.selection.standing[actor.facing]
                : object_assets.sprites.selection.vehicle[std::size_t{actor.facing} * 4];
        const auto phase = objects.event_visits;
        objects.event_visits = 0;
        ff6::latch_field_object_poses(objects);
        ff6::publish_field_objects(objects, object_assets, published);
        objects.event_visits = phase;
    }
    void turn_actor() {
        auto& actor = objects.objects[0];
        actor.facing = static_cast<std::uint8_t>((actor.facing + 1U) % 4);
        refresh_actor();
    }
    void toggle_vehicle() {
        auto& actor = objects.objects[0];
        actor.ride =
            actor.ride == ff6::FieldRide::none ? ff6::FieldRide::magitek : ff6::FieldRide::none;
        refresh_actor();
    }
    void step_actor() {
        auto& actor = objects.objects[0];
        if (actor.velocity_x != 0 || actor.velocity_y != 0)
            return;
        constexpr std::array<ff6::Button, 4> directions{ff6::Button::up, ff6::Button::right,
                                                        ff6::Button::down, ff6::Button::left};
        step_request = ff6::button(directions[actor.facing]);
    }
    ff6::RasterScene scene() const {
        if (!combined)
            return ff6::make_field_layer_scene(display.map(), layer, x, y);
        auto scene = display.scene(origins);
        if (show_actor) {
            ff6::FieldObjectView view{{origins[0], origins[1]},
                                      0,
                                      terrain.low_priority,
                                      sprite_colors,
                                      display.map().composition.objects};
            scene.objects = ff6::make_field_object_layer(objects, object_assets, published, view);
        }
        return scene;
    }
    void reset() {
        follow_camera = true;
        origins = ff6::field_scroll_origins(camera);
        // Isolated inspection views accept sampled rather than logical origins.
        x = origins[layer].x - 8;
        y = origins[layer].y + 1;
    }
    void scroll(std::int32_t dx, std::int32_t dy) {
        const auto& map = display.map();
        const auto& layout = map.definition.layouts[layer];
        const auto wrap = [](std::int32_t value, std::int32_t extent) {
            return (value % extent + extent) % extent;
        };
        if (combined) {
            follow_camera = false;
            for (std::size_t i = 0; i < origins.size(); ++i) {
                const auto mx = i == 0 ? 16 : map.parallax.multiplier[(i - 1) * 2];
                const auto my = i == 0 ? 16 : map.parallax.multiplier[(i - 1) * 2 + 1];
                origins[i].x =
                    wrap(origins[i].x + dx * mx / 16, map.definition.layouts[i].width * 16);
                origins[i].y =
                    wrap(origins[i].y + dy * my / 16, map.definition.layouts[i].height * 16);
            }
        } else {
            x = wrap(x + dx, layout.width * 16);
            y = wrap(y + dy, layout.height * 16);
        }
    }
};
const char* request_label(ff6::MainMenuRequest request) {
    switch (request) {
    case ff6::MainMenuRequest::items:
        return "Item screen";
    case ff6::MainMenuRequest::skill_character:
        return "Skill character";
    case ff6::MainMenuRequest::equipment_character:
        return "Equipment character";
    case ff6::MainMenuRequest::relic_character:
        return "Relic character";
    case ff6::MainMenuRequest::status_character:
        return "Status character";
    case ff6::MainMenuRequest::configuration:
        return "Configuration";
    case ff6::MainMenuRequest::save:
        return "Save screen";
    case ff6::MainMenuRequest::party_order:
        return "Party ordering";
    case ff6::MainMenuRequest::close_and_refresh_equipment:
        return "Field return";
    }
    throw std::logic_error("Unknown menu request");
}
ff6::FrameBuffer render(const ff6::ContentPack& pack, const ff6::MainMenuAssets& menu, Page page,
                        std::uint8_t skin, MenuPreview& preview, FieldPreview& field) {
    ff6::FrameBuffer output(256, 224, 0x101827ffU);
    if (page == Page::audio || page == Page::mix || page == Page::scene ||
        page == Page::battle_background || page == Page::battle_effect)
        return output;
    if (page == Page::field) {
        field.error.clear();
        try {
            const auto frame = ff6::render_scene(field.scene());
            for (std::size_t i = 0; i < frame.colors.size(); ++i)
                output.rgba[i] = ff6::rgba8(ff6::decode_color(ff6::quantize_intensity_rgb555(
                    frame.colors[i], frame.intensity[i / frame.width])));
        } catch (const std::exception& error) {
            // Inspector diagnostics stay visible and never count as a rendered
            // layer. The game loader itself does not replace failures with pixels.
            field.error = error.what();
        }
    } else if (page == Page::windows || page == Page::menu) {
        ff6::RasterScene scene;
        if (page == Page::menu) {
            ff6::MainMenuView view;
            view.window.skin = skin;
            view.cursor = preview.navigation.cursor();
            ff6::populate_party_menu(preview.roster, view);
            if (preview.selecting_character) {
                view.sidebar_cursor_visible = false;
                view.character_cursor = preview.character_navigation.cursor();
            }
            scene = ff6::make_main_menu_scene(menu, view);
        } else {
            scene.intensity.assign(224, 15);
            scene.layers.push_back(ff6::make_main_menu_windows(menu.windows, {skin, std::nullopt}));
        }
        const auto frame = ff6::render_scene(scene);
        for (std::size_t i = 0; i < frame.colors.size(); ++i)
            output.rgba[i] = ff6::rgba8(ff6::decode_color(frame.colors[i]));
    } else if (page == Page::fonts) {
        const auto glyphs = ff6::decode_glyphs(pack.get("large_font_large_font_rows_1bpp"));
        for (std::size_t i = 0; i < glyphs.size(); ++i)
            ff6::draw_glyph(output, glyphs[i], static_cast<int>(i % 16) * 16,
                            35 + static_cast<int>(i / 16) * 12, 0xe8eaf0ffU);
    } else {
        const auto tiles = ff6::decode_tiles(pack.get("menu_graphics_portrait_gfx_4bpp"), 4);
        const auto palettes = pack.get("menu_graphics_portrait_pal_pal");
        // Storage order differs from the 5x5 display grid. Keep this layout as
        // extracted content; provenance is field.o:PortraitTiles (C0:51DA).
        const auto order = ff6::decode_portrait_order(pack.get("field_portrait_tile_order"));
        for (std::size_t portrait = 0; portrait < 19; ++portrait) {
            std::array<std::uint16_t, 16> palette{};
            for (std::size_t c = 0; c < 16; ++c)
                palette[c] = ff6::read16(palettes, portrait * 32 + c * 2);
            for (std::size_t t = 0; t < 25; ++t)
                ff6::draw_tile(
                    output,
                    {tiles.at(portrait * 25 + order[t]), palette,
                     8 + static_cast<int>(portrait % 5) * 49 + static_cast<int>(t % 5) * 8,
                     35 + static_cast<int>(portrait / 5) * 45 + static_cast<int>(t / 5) * 8});
        }
    }
    return output;
}
void require(bool result) {
    if (!result)
        throw std::runtime_error(SDL_GetError());
}
} // namespace
int main(int argc, char** argv) {
    try {
        bool smoke = false;
        bool start_music = false;
        bool continuous_attack = false;
        bool automatic_attack = false;
        bool startup_entry = false;
        std::optional<std::uint8_t> background_entry;
        std::optional<std::uint16_t> attack_entry;
        std::optional<BattleEffectStudy::Target> attack_target;
        std::optional<std::uint16_t> field_entry;
        std::uint8_t song = 2;
        std::uint8_t effect = 0, system_effect = 0;
        std::filesystem::path input;
        std::filesystem::path save_folder;
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--smoke-test")
                smoke = true;
            else if (std::string_view(argv[i]) == "--attack-sequence")
                continuous_attack = true;
            else if (std::string_view(argv[i]) == "--attack-auto")
                automatic_attack = true;
            else if (std::string_view(argv[i]) == "--startup")
                startup_entry = true;
            else if (std::string_view(argv[i]) == "--save-folder") {
                if (++i == argc)
                    throw std::invalid_argument("--save-folder requires a directory");
                save_folder = argv[i];
            } else if (std::string_view(argv[i]) == "--attack-animation") {
                if (++i == argc)
                    throw std::invalid_argument("--attack-animation requires an ID (0..405)");
                const std::string_view value = argv[i];
                unsigned identity = 0;
                const auto parsed =
                    std::from_chars(value.data(), value.data() + value.size(), identity);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    identity >= 406)
                    throw std::invalid_argument("Attack animation ID must be 0..405");
                attack_entry = static_cast<std::uint16_t>(identity);
            } else if (std::string_view(argv[i]) == "--attack-target") {
                if (++i == argc)
                    throw std::invalid_argument(
                        "--attack-target requires monster, character or party");
                const std::string_view value = argv[i];
                if (value == "monster")
                    attack_target = BattleEffectStudy::Target::monster;
                else if (value == "character")
                    attack_target = BattleEffectStudy::Target::character;
                else if (value == "party")
                    attack_target = BattleEffectStudy::Target::party;
                else
                    throw std::invalid_argument("Unknown attack study target");
            } else if (std::string_view(argv[i]) == "--battle-background") {
                if (++i == argc)
                    throw std::invalid_argument("--battle-background requires an ID (0..55)");
                const std::string_view value(argv[i]);
                unsigned identity = 0;
                const auto parsed =
                    std::from_chars(value.data(), value.data() + value.size(), identity);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    identity >= 56)
                    throw std::invalid_argument("Battle background ID must be 0..55");
                background_entry = static_cast<std::uint8_t>(identity);
            } else if (std::string_view(argv[i]) == "--field-map") {
                if (++i == argc)
                    throw std::invalid_argument("--field-map requires 19 or 20");
                const std::string_view value(argv[i]);
                unsigned identity = 0;
                const auto parsed =
                    std::from_chars(value.data(), value.data() + value.size(), identity);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    (identity != 19 && identity != 20))
                    throw std::invalid_argument("Development field map must be 19 or 20");
                field_entry = static_cast<std::uint16_t>(identity);
            } else if (std::string_view(argv[i]) == "--music") {
                if (++i == argc)
                    throw std::invalid_argument("--music requires a song ID (0..84)");
                const std::string_view value(argv[i]);
                unsigned parsed_song = 0;
                const auto parsed =
                    std::from_chars(value.data(), value.data() + value.size(), parsed_song);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    parsed_song >= 85)
                    throw std::invalid_argument("Song ID must be 0..84");
                song = static_cast<std::uint8_t>(parsed_song);
                start_music = true;
            } else if (input.empty())
                input = argv[i];
            else
                throw std::runtime_error(
                    "Usage: ff6-inspector [--smoke-test] [--music SONG_ID] "
                    "[--field-map 19|20 | --startup | --battle-background ID "
                    "| --attack-animation ID [--attack-target monster|character|party] "
                    "[--attack-auto | --attack-sequence]] [PACK]");
        }
        if (attack_target && !attack_entry)
            throw std::invalid_argument("--attack-target requires --attack-animation");
        if (automatic_attack && continuous_attack)
            throw std::invalid_argument("Choose automatic or strict sequence playback");
        if (unsigned{startup_entry} + unsigned{field_entry.has_value()} +
                unsigned{background_entry.has_value()} + unsigned{attack_entry.has_value()} >
            1)
            throw std::invalid_argument(
                "Choose one startup, field, background or attack-animation entry");
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
        struct Quit {
            ~Quit() {
                SDL_Quit();
            }
        } quit;
        if (input.empty()) {
            const auto* base = SDL_GetBasePath();
            if (!base)
                throw std::runtime_error(SDL_GetError());
            input = std::filesystem::path(base) / "content.ff6pack";
        }
        const auto pack = ff6::decode_pack(ff6::read_file(input));
        ff6::verify_canonical(pack);
        const auto menu = ff6::load_main_menu(pack);
        std::unique_ptr<ff6::MusicPlayer> music;
        bool silent_audio_test = false;
        bool pause_requested = false;
        bool mono_requested = false;
        std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> audio(
            nullptr, SDL_DestroyAudioStream);
        // The generic audio smoke sequence schedules future controls. The
        // attack study supplies its own live script cues instead, so injecting
        // that sequence would put new cues behind already-scheduled future work.
        const bool audio_controls_smoke = smoke && !attack_entry;
        const auto toggle_music = [&] {
            if (music) {
                audio.reset();
                music.reset();
                silent_audio_test = false;
                pause_requested = false;
                mono_requested = false;
                return;
            }
            if (!SDL_WasInit(SDL_INIT_AUDIO))
                require(SDL_InitSubSystem(SDL_INIT_AUDIO));
            auto next = std::make_unique<ff6::MusicPlayer>(pack, song);
            pause_requested = false;
            mono_requested = false;
            if (audio_controls_smoke) {
                next->schedule(2048, ff6::GameSoundEffect{0, 128});
                next->schedule(4096, ff6::SystemSoundEffect{0});
                next->schedule(6144, ff6::PauseMusic{true});
                next->schedule(8192, ff6::PauseMusic{false});
                next->schedule(10240, ff6::SongLoad{static_cast<std::uint8_t>((song + 1U) % 85),
                                                    255, false, true});
                next->schedule(14336, ff6::SongLoad{song, 255, false, false});
                next->schedule(16000, ff6::PlaybackVolumeRamp{ff6::PlaybackGroup::music, 4, 96});
                next->schedule(18000, ff6::EffectPanRamp{0, 16});
                next->schedule(19000, ff6::MusicTempoRamp{0, -64});
                next->schedule(20000, ff6::MusicPitchRamp{0, 32});
                next->schedule(21000, ff6::MonoPlayback{});
                next->schedule(22000, ff6::MonoPlayback{false});
                next->schedule(23000, ff6::MusicCue{});
                next->schedule(24000, ff6::MutedPlaybackVoices{1});
                next->schedule(25000, ff6::MutedPlaybackVoices{});
                next->schedule(26000, ff6::StopPlayback{ff6::PlaybackGroup::music});
                next->schedule(27000, ff6::SystemSoundEffect{0});
                next->schedule(28000, ff6::StopPlayback{ff6::PlaybackGroup::effects});
                next->schedule(29000, ff6::StopPlayback{});
                next->schedule(30000, ff6::SongLoad{song});
            }
            const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, static_cast<int>(ff6::music_sample_rate)};
            audio.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr,
                                                  nullptr));
            if (!audio)
                throw std::runtime_error(SDL_GetError());
            // Queue an initial block before unpausing. Simulation runs here on
            // the producer thread; SDL's device callback performs no game work.
            std::array<ff6::StereoFrame, 1024> block;
            next->render(block);
            require(
                SDL_PutAudioStreamData(audio.get(), block.data(), static_cast<int>(sizeof(block))));
            require(SDL_ResumeAudioStreamDevice(audio.get()));
            const auto* driver = SDL_GetCurrentAudioDriver();
            const auto* device = SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(audio.get()));
            silent_audio_test = driver && std::string_view(driver) == "dummy";
            std::cout << "Music preview output: " << (driver ? driver : "unknown driver") << " / "
                      << (device ? device : "default device") << std::endl;
            music = std::move(next);
        };
        if (start_music)
            toggle_music();
        SDL_Window* raw_window = nullptr;
        SDL_Renderer* raw_renderer = nullptr;
        require(SDL_CreateWindowAndRenderer(
            "FFVI Content Inspector — development tool, not playable", 768, 672,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &raw_window, &raw_renderer));
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(raw_window,
                                                                         SDL_DestroyWindow);
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(raw_renderer,
                                                                               SDL_DestroyRenderer);
        require(SDL_SetRenderLogicalPresentation(renderer.get(), 256, 224,
                                                 SDL_LOGICAL_PRESENTATION_LETTERBOX));
        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
            SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                              256, 224),
            SDL_DestroyTexture);
        if (!texture)
            throw std::runtime_error(SDL_GetError());
        require(SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST));
        std::vector<std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>> controllers;
        bool running = true;
        Page page = attack_entry                     ? Page::battle_effect
                    : background_entry               ? Page::battle_background
                    : (field_entry || startup_entry) ? Page::scene
                                                     : Page::portraits;
        std::unique_ptr<BattleEffectStudy> attack_study;
        bool all_attack_definitions = false;
        const auto open_attack_study = [&] {
            if (!attack_study) {
                const auto mode = continuous_attack ? BattleEffectStudy::Playback::sequence
                    : automatic_attack || !smoke ? BattleEffectStudy::Playback::automatic
                                                : BattleEffectStudy::Playback::body;
                attack_study = std::make_unique<BattleEffectStudy>(pack, mode);
            }
            if (attack_entry) {
                attack_study->identity = *attack_entry;
                if (attack_target)
                    attack_study->target = *attack_target;
                attack_study->restart();
                attack_entry.reset();
            }
        };
        if (attack_entry)
            open_attack_study();
        std::uint8_t background_id = background_entry.value_or(8);
        std::optional<ff6::BattleBackground> background_preview;
        std::optional<ff6::BattleTrainGraphics> train_preview;
        std::uint16_t scene_entry = field_entry.value_or(20);
        std::unique_ptr<FieldSessionHost> field_session;
        bool scene_audio_active = false;
        const auto restart_scene = [&] {
            field_session = std::make_unique<FieldSessionHost>(
                pack, startup_entry ? std::nullopt : std::optional{scene_entry});
            if (save_folder.empty()) {
                char* location = SDL_GetPrefPath("FFVI Native", "Final Fantasy VI");
                if (!location)
                    throw std::runtime_error(SDL_GetError());
                save_folder = std::filesystem::path(location) / "Saves";
                SDL_free(location);
            }
            field_session->set_save_folder(save_folder);
        };
        if (field_entry || startup_entry)
            restart_scene();
        std::uint8_t skin = 0;
        MenuPreview menu_preview(pack);
        FieldPreview field_preview(pack, menu);
        Uint64 menu_clock = SDL_GetTicksNS(), menu_fraction = 0;
        Uint64 field_clock = menu_clock, field_fraction = 0;
        ff6::Buttons menu_taps = 0;
        bool prime_menu_input = true;
        const auto reset_menu_clock = [&] {
            menu_clock = SDL_GetTicksNS();
            menu_fraction = 0;
            prime_menu_input = true;
            menu_taps = 0;
        };
        const auto next_page = [&] {
            switch (page) {
            case Page::portraits:
                page = Page::fonts;
                break;
            case Page::fonts:
                page = Page::windows;
                break;
            case Page::windows:
                page = Page::menu;
                menu_preview.enter();
                break;
            case Page::menu:
                page = Page::field;
                break;
            case Page::field:
                page = smoke ? Page::audio : Page::scene;
                if (page == Page::scene && !field_session)
                    restart_scene();
                break;
            case Page::scene:
                page = Page::audio;
                break;
            case Page::audio:
                page = Page::mix;
                break;
            case Page::mix:
                page = Page::battle_background;
                break;
            case Page::battle_background:
                page = Page::battle_effect;
                open_attack_study();
                break;
            case Page::battle_effect:
                page = Page::portraits;
                break;
            }
            reset_menu_clock();
        };
        while (running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT ||
                    event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    running = false;
                // Automated previews own their input. A held controller button
                // or typing in another app must not switch the requested study
                // to another page while the delivery check is running.
                if (smoke && (event.type == SDL_EVENT_KEY_DOWN ||
                              event.type == SDL_EVENT_KEY_UP ||
                              event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                              event.type == SDL_EVENT_GAMEPAD_BUTTON_UP))
                    continue;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (page == Page::menu)
                        for (const auto binding : menu_bindings)
                            if (binding.key == event.key.scancode)
                                menu_taps |= ff6::button(binding.button);
                    if (event.key.key == SDLK_ESCAPE)
                        running = false;
                    if (event.key.key == SDLK_F9) {
                        page = Page::battle_effect;
                        open_attack_study();
                    }
                    if (page == Page::battle_effect) {
                        if (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) {
                            attack_study->select(event.key.key == SDLK_RIGHT ? 1 : -1,
                                                 all_attack_definitions);
                        }
                        if (event.key.key == SDLK_A)
                            all_attack_definitions = !all_attack_definitions;
                        if (event.key.key == SDLK_HOME || event.key.key == SDLK_E) {
                            attack_study->identity = event.key.key == SDLK_E ? 54 : 0;
                            attack_study->restart();
                        }
                        if (event.key.key == SDLK_RETURN)
                            attack_study->restart();
                        if (event.key.key == SDLK_T) {
                            attack_study->target = static_cast<BattleEffectStudy::Target>(
                                (static_cast<unsigned>(attack_study->target) + 1U) % 3U);
                            attack_study->restart();
                        }
                        if (event.key.key == SDLK_S) {
                            attack_study->cycle_playback();
                        }
                        if (event.key.key == SDLK_P)
                            attack_study->paused = !attack_study->paused;
                    }
                    if (event.key.key == SDLK_F8)
                        page = Page::battle_background;
                    if (page == Page::battle_background &&
                        (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT)) {
                        background_id = static_cast<std::uint8_t>(
                            (unsigned{background_id} + (event.key.key == SDLK_RIGHT ? 1U : 55U)) %
                            56U);
                        background_preview.reset();
                    }
                    if (event.key.key == SDLK_F5 || event.key.key == SDLK_F6 ||
                        event.key.key == SDLK_F7) {
                        if (event.key.key == SDLK_F6) {
                            startup_entry = false;
                            scene_entry = scene_entry == 19 ? 20 : 19;
                        }
                        if (event.key.key == SDLK_F7)
                            startup_entry = true;
                        restart_scene();
                        page = Page::scene;
                    }
                    if (page == Page::scene) {
                        for (const auto binding : scene_bindings)
                            if (binding.key == event.key.scancode)
                                field_session->taps |= ff6::button(binding.button);
                        if (event.key.key == SDLK_M)
                            field_session->toggle_mute();
                        if (event.key.key == SDLK_F10)
                            field_session->toggle_logging();
                        if (event.key.key == SDLK_P)
                            field_session->paused = !field_session->paused;
                    }
                    if (event.key.key == SDLK_M && page != Page::scene)
                        toggle_music();
                    if (event.key.key == SDLK_P && music && page != Page::scene &&
                        page != Page::battle_effect) {
                        // Track requested toggles as well as the native status,
                        // so two quick presses still enqueue pause then unpause.
                        pause_requested = !pause_requested;
                        music->schedule(music->rendered_frames(), ff6::PauseMusic{pause_requested});
                    }
                    if (event.key.key == SDLK_TAB || event.key.key == SDLK_SPACE)
                        next_page();
                    if (page == Page::menu && event.key.key == SDLK_K)
                        menu_preview.memory = !menu_preview.memory;
                    if (page == Page::field) {
                        if (event.key.key == SDLK_O)
                            field_preview.show_actor = !field_preview.show_actor;
                        if (event.key.key == SDLK_R)
                            field_preview.turn_actor();
                        if (event.key.key == SDLK_V)
                            field_preview.toggle_vehicle();
                        if (event.key.key == SDLK_T)
                            field_preview.step_actor();
                        if (event.key.key == SDLK_A)
                            field_preview.animate = !field_preview.animate;
                        if (!field_preview.combined || (event.key.mod & SDL_KMOD_SHIFT)) {
                            if (event.key.key == SDLK_LEFT)
                                field_preview.scroll(-16, 0);
                            if (event.key.key == SDLK_RIGHT)
                                field_preview.scroll(16, 0);
                            if (event.key.key == SDLK_UP)
                                field_preview.scroll(0, -16);
                            if (event.key.key == SDLK_DOWN)
                                field_preview.scroll(0, 16);
                        }
                        if (event.key.key == SDLK_C) {
                            field_preview.combined = true;
                            field_preview.reset();
                        }
                        if (event.key.key == SDLK_L) {
                            field_preview.combined = false;
                            field_preview.layer =
                                static_cast<std::uint8_t>((field_preview.layer + 1U) % 3);
                            field_preview.reset();
                        }
                    }
                    if (page == Page::windows && event.key.key == SDLK_RIGHT)
                        skin = static_cast<std::uint8_t>((skin + 1U) % 8);
                    if (page == Page::windows && event.key.key == SDLK_LEFT)
                        skin = static_cast<std::uint8_t>((skin + 7U) % 8);
                    if (page != Page::scene &&
                        (event.key.key == SDLK_LEFTBRACKET || event.key.key == SDLK_RIGHTBRACKET ||
                         (page == Page::audio && event.key.key == SDLK_I))) {
                        const auto direction = event.key.key == SDLK_LEFTBRACKET ? 84U : 1U;
                        song = static_cast<std::uint8_t>((song + direction) % 85);
                        if (music) {
                            music->schedule(
                                music->rendered_frames(),
                                ff6::SongLoad{song, 255, false, event.key.key == SDLK_I});
                            pause_requested = false;
                        }
                    }
                    if (page == Page::audio) {
                        if (event.key.key == SDLK_UP)
                            effect = static_cast<std::uint8_t>((effect + 1U) % 256);
                        if (event.key.key == SDLK_DOWN)
                            effect = static_cast<std::uint8_t>((effect + 255U) % 256);
                        if (event.key.key == SDLK_RIGHT)
                            system_effect = static_cast<std::uint8_t>((system_effect + 1U) % 16);
                        if (event.key.key == SDLK_LEFT)
                            system_effect = static_cast<std::uint8_t>((system_effect + 15U) % 16);
                        if (event.key.key == SDLK_E || event.key.key == SDLK_S) {
                            if (!music)
                                toggle_music();
                            if (event.key.key == SDLK_E)
                                music->schedule(music->rendered_frames(),
                                                ff6::GameSoundEffect{effect, 128});
                            else
                                music->schedule(music->rendered_frames(),
                                                ff6::SystemSoundEffect{system_effect});
                        }
                        if (event.key.key == SDLK_R && music && music->suspended_song()) {
                            song = *music->suspended_song();
                            music->schedule(music->rendered_frames(),
                                            ff6::SongLoad{song, 255, false, false});
                            pause_requested = false;
                        }
                    }
                    if (page == Page::mix && music) {
                        const auto at = music->rendered_frames();
                        if (event.key.key == SDLK_F || event.key.key == SDLK_G)
                            music->schedule(at, ff6::PlaybackVolumeRamp{
                                                    ff6::PlaybackGroup::music, 32,
                                                    static_cast<std::uint8_t>(
                                                        event.key.key == SDLK_F ? 0 : 255)});
                        if (event.key.key == SDLK_O) {
                            mono_requested = !mono_requested;
                            music->schedule(at, ff6::MonoPlayback{mono_requested});
                        }
                        if (event.key.key == SDLK_X)
                            music->schedule(at, ff6::StopPlayback{ff6::PlaybackGroup::music});
                        if (event.key.key == SDLK_U)
                            music->schedule(at, ff6::StopPlayback{ff6::PlaybackGroup::effects});
                        if (event.key.key == SDLK_B)
                            music->schedule(at, ff6::StopPlayback{});
                        if (event.key.key == SDLK_C)
                            music->schedule(at, ff6::MusicCue{});
                    }
                }
                if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                    if (auto* controller = SDL_OpenGamepad(event.gdevice.which))
                        controllers.emplace_back(controller, SDL_CloseGamepad);
                }
                if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                    std::erase_if(controllers, [&](const auto& controller) {
                        return SDL_GetGamepadID(controller.get()) == event.gdevice.which;
                    });
                }
                if (page != Page::menu && page != Page::scene &&
                    event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                    event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
                    next_page();
                else if ((page == Page::menu || page == Page::scene) &&
                         event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && !controllers.empty() &&
                         SDL_GetGamepadID(controllers[0].get()) == event.gbutton.which)
                    for (const auto binding :
                         (page == Page::scene ? std::span<const MenuBinding>{scene_bindings}
                                              : std::span<const MenuBinding>{menu_bindings})) {
                        if (binding.pad == event.gbutton.button) {
                            if (page == Page::scene)
                                field_session->taps |= ff6::button(binding.button);
                            else
                                menu_taps |= ff6::button(binding.button);
                        }
                    }
                if (page == Page::scene && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                    controllers.size() > 1 &&
                    SDL_GetGamepadID(controllers[1].get()) == event.gbutton.which)
                    for (const auto binding : scene_bindings)
                        if (binding.pad == event.gbutton.button)
                            field_session->second_taps |= ff6::button(binding.button);
                if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
                    event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                    reset_menu_clock();
                    if (field_session) {
                        field_session->clock.update(SDL_GetTicksNS(), false);
                        field_session->taps = field_session->second_taps = 0;
                        field_session->activate(false);
                    }
                }
            }
            if (page == Page::menu && smoke) {
                for (unsigned tick = 0; tick < 25; ++tick)
                    menu_preview.tick(ff6::button(ff6::Button::down));
                menu_preview.tick(0);
                const auto invalid = menu_preview.tick(ff6::button(ff6::Button::a));
                if (!invalid.invalid_mosaic || menu_preview.navigation.cursor() != 6)
                    throw std::runtime_error("Menu smoke test did not reach disabled Save");
                menu_preview.tick(0);
                menu_preview.tick(ff6::button(ff6::Button::up));
                menu_preview.tick(ff6::button(ff6::Button::a));
                if (menu_preview.pending != ff6::MainMenuRequest::configuration)
                    throw std::runtime_error("Menu smoke test did not request configuration");
                menu_preview.tick(ff6::button(ff6::Button::b));
                if (menu_preview.pending || menu_preview.navigation.cursor() != 5)
                    throw std::runtime_error("Menu smoke test did not restore cursor memory");
                menu_preview.navigation.enter(false);
                menu_preview.tick(ff6::button(ff6::Button::down));
                menu_preview.tick(ff6::button(ff6::Button::a));
                if (!menu_preview.selecting_character ||
                    menu_preview.character_navigation.cursor() != 0)
                    throw std::runtime_error("Menu smoke test did not enter character selection");
                menu_preview.tick(ff6::button(ff6::Button::down));
                if (menu_preview.character_navigation.cursor() != 1)
                    throw std::runtime_error("Menu smoke test did not move between characters");
                menu_preview.tick(ff6::button(ff6::Button::b));
                if (menu_preview.selecting_character || menu_preview.navigation.cursor() != 1)
                    throw std::runtime_error("Menu smoke test did not return to sidebar");
            } else if (page == Page::menu &&
                       (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_INPUT_FOCUS) != 0) {
                const auto now = SDL_GetTicksNS();
                const auto buttons =
                    mapped_buttons(controllers.empty() ? nullptr : controllers[0].get());
                if (prime_menu_input) {
                    menu_preview.input = {};
                    // Entry/focus recovery must not turn already held buttons
                    // into confirm/cancel presses in this new screen.
                    ff6::sample_input(menu_preview.input, menu_preview.settings,
                                      ff6::InputContext::menu, buttons, 0);
                    prime_menu_input = false;
                }
                // Development cadence only. Rational accumulation decouples
                // repeats from refresh rate; cap suspended time, bound catch-up
                // work and retain ordinary fractional/backlogged samples.
                menu_fraction += std::min<Uint64>(now - menu_clock, 250000000) * 60;
                menu_clock = now;
                for (unsigned tick = 0; tick < 8 && menu_fraction >= 1000000000; ++tick) {
                    menu_fraction -= 1000000000;
                    // Retain very short host taps for one logical sample. OS key
                    // repeat is excluded; held-input repeat remains native.
                    const auto update = menu_preview.tick(buttons | menu_taps);
                    menu_taps = 0;
                    if (music)
                        for (std::uint8_t i = 0; i < update.sound_count; ++i)
                            music->schedule(
                                music->rendered_frames(),
                                ff6::SystemSoundEffect{ff6::menu_sound_effect(update.sounds[i])});
                }
            } else
                reset_menu_clock();
            const auto field_now = SDL_GetTicksNS();
            if (page == Page::field && smoke) {
                if (!field_preview.smoke_camera_checked) {
                    const auto actor_y = field_preview.objects.objects[0].y;
                    const auto camera_y = field_preview.camera.layers[0][1].position;
                    field_preview.step_actor();
                    for (unsigned step = 0; step < 120; ++step)
                        field_preview.tick();
                    if (field_preview.objects.objects[0].y != actor_y + 4096U ||
                        field_preview.camera.layers[0][1].position != camera_y + 4096U)
                        throw std::runtime_error(
                            "Camera smoke test did not follow one native step");
                    const auto camera = field_preview.camera;
                    field_preview.scroll(16, 0);
                    const auto manual = field_preview.origins;
                    field_preview.tick();
                    if (field_preview.follow_camera || field_preview.origins != manual)
                        throw std::runtime_error("Camera smoke test lost manual panning");
                    field_preview.reset();
                    if (!field_preview.follow_camera || field_preview.camera != camera ||
                        field_preview.origins != ff6::field_scroll_origins(camera))
                        throw std::runtime_error(
                            "Camera smoke test did not resume retained follow");
                    field_preview.smoke_camera_checked = true;
                }
                for (unsigned step = 0; step < 120; ++step)
                    field_preview.tick();
            } else if (page == Page::field && field_preview.animate &&
                       (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_INPUT_FOCUS) != 0) {
                // Development cadence, not yet the complete field scheduler.
                // Rational accumulation keeps host refresh separate; suspend time
                // is capped and each loop has a bounded amount of catch-up work.
                field_fraction += std::min<Uint64>(field_now - field_clock, 250000000) * 60;
                for (unsigned step = 0; step < 8 && field_fraction >= 1000000000; ++step) {
                    field_fraction -= 1000000000;
                    const auto buttons =
                        field_preview.combined && !(SDL_GetModState() & SDL_KMOD_SHIFT)
                            ? mapped_buttons(controllers.empty() ? nullptr : controllers[0].get())
                            : 0;
                    field_preview.tick(static_cast<ff6::Buttons>(buttons));
                }
            } else
                field_fraction = 0;
            field_clock = field_now;
            const bool scene_selected = page == Page::scene;
            if (scene_audio_active != scene_selected && audio) {
                require(scene_selected ? SDL_PauseAudioStreamDevice(audio.get())
                                       : SDL_ResumeAudioStreamDevice(audio.get()));
            }
            scene_audio_active = scene_selected;
            if (field_session) {
                const bool active =
                    scene_selected && !field_session->paused && !field_session->halted() &&
                    (smoke || (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_INPUT_FOCUS) != 0);
                field_session->clock.update(SDL_GetTicksNS(), active);
                if (!active)
                    field_session->taps = field_session->second_taps = 0;
                for (unsigned work = 0, completed = 0;
                     active && work < 64 && completed < 8 && field_session->clock.due(); ++work) {
                    auto buttons = mapped_buttons(
                        controllers.empty() ? nullptr : controllers[0].get(), scene_bindings);
                    if (smoke)
                        buttons =
                            field_session->session.ticks() < 128 ? 0 : ff6::button(ff6::Button::up);
                    const auto second =
                        smoke ? 0
                              : mapped_buttons(controllers.size() > 1 ? controllers[1].get()
                                                                      : nullptr,
                                               scene_bindings, false);
                    if (field_session->tick(pack, buttons, static_cast<ff6::Buttons>(second))) {
                        field_session->clock.consume();
                        ++completed;
                        if (smoke &&
                            field_session->session.ticks() == (startup_entry ? 420U : 320U))
                            break;
                    }
                    if (field_session->halted())
                        break;
                }
                field_session->activate(active);
                if (smoke && scene_selected && field_session->halted())
                    throw std::runtime_error("Field session smoke: " + field_session->notice());
            }
            if (attack_study) {
                const bool active =
                    page == Page::battle_effect && !attack_study->paused &&
                    (smoke || (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_INPUT_FOCUS) != 0);
                attack_study->clock.update(SDL_GetTicksNS(), active);
                for (unsigned tick = 0; tick < 4 && active && (smoke || attack_study->clock.due());
                     ++tick) {
                    if (const auto cue = attack_study->advance(); cue && music)
                        music->schedule(music->rendered_frames(), *cue);
                    if (!smoke)
                        attack_study->clock.consume();
                    if (!attack_study->available() || attack_study->complete())
                        break;
                }
            }
            if (music && !scene_selected) {
                constexpr int target_bytes = 4096 * 4;
                auto queued = SDL_GetAudioStreamQueued(audio.get());
                if (queued < 0)
                    throw std::runtime_error(SDL_GetError());
                // At most four blocks per main-loop turn; queue growth is bounded.
                for (unsigned n = 0; n < 4 && queued < target_bytes; ++n) {
                    std::array<ff6::StereoFrame, 1024> block;
                    music->render(block);
                    require(SDL_PutAudioStreamData(audio.get(), block.data(),
                                                   static_cast<int>(sizeof(block))));
                    queued += static_cast<int>(sizeof(block));
                }
            }
            const auto title =
                page == Page::battle_effect
                    ? std::string("FFVI Inspector / Attack animation study ") +
                          std::to_string(attack_study->identity) +
                          "/405 / " + (attack_study->error.empty()
                              ? attack_study->sequence_error.empty() ? attack_study->mode_label()
                                  : "Body only: " + attack_study->sequence_error
                              : attack_study->error)
                : page == Page::battle_background
                    ? "FFVI Inspector / " +
                          std::string(background_id == 49   ? "unused background definition "
                                      : background_id == 50 ? "assembled Ghost Train graphics "
                                                            : "static battle background ") +
                          std::to_string(background_id) +
                          "/55 / Left-Right: select / Tab: portraits / animation and combat "
                          "unfinished"
                : page == Page::windows
                    ? "FFVI Inspector / native window theme " + std::to_string(skin + 1) +
                          "/8 / Left-Right: theme / Tab: menu layout / not playable"
                : page == Page::field
                    ? "FFVI Inspector / map 19 / " +
                          (field_preview.combined
                               ? std::string(field_preview.show_actor
                                                 ? "terrain movement study (no NPCs)"
                                                 : "framed animated backgrounds")
                               : "isolated layer " + std::to_string(field_preview.layer + 1)) +
                          (field_preview.combined ? " / Arrows: move / Shift+Arrows: pan"
                                                  : " / Arrows: pan") +
                          (field_preview.combined
                               ? (field_preview.follow_camera ? " / camera follows"
                                                              : " / manual view")
                               : "") +
                          " / T: step / R: turn / V: vehicle / C: follow / L: layer / A: " +
                          (field_preview.animate ? "pause anim" : "play anim") +
                          " / Tab: native scene / F5: scene" +
                          (field_preview.last_step.kind == ff6::FieldStep::Kind::door
                               ? " / Door request: map handler unfinished"
                               : "") +
                          (field_preview.error.empty() ? "" : " / " + field_preview.error)
                : page == Page::scene
                    ? std::string(field_session->initial_start
                                      ? "FFVI / native startup / map "
                                      : "FFVI / field development entry / map ") +
                          std::to_string(
                              field_session->session.world() ? field_session->session.world()->map :
                              field_session->session.scene().state().state.field.current_map) +
                          (field_session->session.name_menu()
                               ? " / Naming / Arrows: choose / Return: add / Backspace: erase / Right Shift: finish"
                           : field_session->session.battle_menus()
                               ? " / Battle "
                                 "/ Arrows: choose / "
                                 "Return: confirm / Backspace: cancel / X,Y: next,previous "
                                 "character / P: pause / M: mute / F7: restart"
                           : field_session->session.world() && field_session->session.world()->chocobo
                               ? " / Chocobo / Up,Return: forward / Left,Right: steer / "
                                 "Q+Left,W+Right: sharp turn / Backspace: dismount / "
                                 "Right Shift: map / P: pause / M: mute"
                               : " / Arrows: move / Return: talk / X: menu / P: pause / M: mute / "
                                 "F5: restart / F6: other Narshe / F7: startup / Tab: audio") +
                          (field_session->session.title() ? " / Title: Return to skip after splash"
                                                          : "") +
                          (field_session->session.credits() ? " / Credits: Return to skip" : "") +
                          (field_session->paused ? " / Paused" : "") +
                          (field_session->muted ? " / Muted" : "") +
                          (field_session->silent_test ? " / Silent audio test" : "") +
                          (field_session->halted() ? " / " + field_session->notice()
                           : !field_session->session.notice().empty()
                               ? " / " + field_session->session.notice()
                               : "")
                : page == Page::menu
                    ? std::string("FFVI Inspector / party setup preview (not opening gameplay) / "
                                  "Arrows: move / Return: A / Backspace: B / Tab: field")
                    : std::string("FFVI Content Inspector — development tool, not playable");
            const auto audio_title =
                page == Page::scene
                    ? title
                    : title + " / Song " + std::to_string(song) + " / [ ]: song" +
                          (music ? (silent_audio_test ? " / Silent audio test / M: stop"
                                                      : " / M: stop music")
                                 : " / M: play music") +
                          (music ? (pause_requested ? " / P: resume" : " / P: pause") : "");
            require(SDL_SetWindowTitle(window.get(), audio_title.c_str()));
            auto frame = render(pack, menu, page, skin, menu_preview, field_preview);
            if (page == Page::battle_effect) {
                const auto study = ff6::render_scene(attack_study->visible);
                for (std::size_t i = 0; i < study.colors.size(); ++i)
                    frame.rgba[i] = ff6::rgba8(ff6::decode_color(study.colors[i]));
            }
            if (page == Page::battle_background) {
                if (!background_preview)
                    background_preview = ff6::load_battle_background(pack, background_id);
                if (background_id == 50 && !train_preview)
                    train_preview = ff6::load_battle_train_graphics(pack);
                const auto study = ff6::render_scene(
                    background_id == 50 ? ff6::make_battle_train_study(*train_preview)
                                        : ff6::make_battle_background_study(*background_preview));
                for (std::size_t i = 0; i < study.colors.size(); ++i)
                    frame.rgba[i] = ff6::rgba8(ff6::decode_color(study.colors[i]));
            }
            if (page == Page::scene) {
                frame = ff6::FrameBuffer(256, 224, 0x000000ffU);
                if (field_session->session.visible()) {
                    const auto& native = *field_session->session.visible();
                    for (std::size_t i = 0; i < native.colors.size(); ++i)
                        frame.rgba[i] = ff6::rgba8(ff6::decode_color(ff6::quantize_intensity_rgb555(
                            native.colors[i], native.intensity[i / native.width])));
                }
            }
            if (smoke && page == Page::field && !field_preview.error.empty())
                throw std::runtime_error("Field preview smoke failed: " + field_preview.error);
            require(SDL_UpdateTexture(texture.get(), nullptr, frame.rgba.data(), frame.width * 4));
            require(SDL_SetRenderDrawColor(renderer.get(), 16, 24, 39, 255));
            require(SDL_RenderClear(renderer.get()));
            require(SDL_RenderTexture(renderer.get(), texture.get(), nullptr, nullptr));
            require(SDL_SetRenderDrawColor(renderer.get(), 200, 214, 230, 255));
            if (page == Page::battle_effect) {
                const auto line = [&](int y, const std::string& text) {
                    require(SDL_RenderDebugText(renderer.get(), 6, static_cast<float>(y), text.c_str()));
                };
                line(156, attack_study->mode_label());
                line(166, all_attack_definitions ? "L/R: all IDs / A: playable" : "L/R: playable / A: all IDs");
                line(176, "Home: spells / E: espers");
                line(186, "Enter: replay / P: pause");
                line(196, !attack_study->error.empty() ? "Unavailable in this mode"
                          : attack_study->paused ? "Paused" : attack_study->phase_name());
                if (attack_study->error.empty()) {
                    line(206, std::string("T: ") +
                        (attack_study->target == BattleEffectStudy::Target::party ? "party"
                         : attack_study->target == BattleEffectStudy::Target::character ? "character" : "monster") +
                        (music ? " / M: sound on" : " / M: sound off"));
                    line(216, attack_study->sequence_error.empty() ? "Preview / fixed test actors"
                                                                : "Sequence remains unfinished");
                } else {
                    // Debug text has eight-pixel cells in the 256-pixel view.
                    // Put the reason on screen; the title retains the full text.
                    line(206, attack_study->error.substr(0, 30));
                    line(216, attack_study->error.substr(std::min<std::size_t>(30, attack_study->error.size()), 30));
                }
            }

            if (page == Page::menu) {
                const auto line = [&](int y, const char* value) {
                    require(SDL_RenderDebugText(renderer.get(), 12, static_cast<float>(y), value));
                };
                line(168, "Party setup preview");
                if (menu_preview.pending || menu_preview.character_pending) {
                    line(178, menu_preview.pending ? request_label(*menu_preview.pending)
                              : *menu_preview.character_pending ==
                                      ff6::CharacterMenuRequest::party_equipment_selection
                                  ? "Party equipment"
                                  : "Character submenu");
                    line(188, "Not implemented yet");
                    line(198, "Backspace: return");
                } else {
                    line(178, menu_preview.invalid_notice        ? "Unavailable"
                              : menu_preview.selecting_character ? "Choose a character"
                                                                 : "Arrows: menu cursor");
                    line(188, "Enter: A / BS: B");
                    line(198, menu_preview.memory ? "K: memory on" : "K: memory off");
                }
            } else if (page == Page::field) {
                // Controls remain in the title so the field's actual framing is
                // visible. Only an unresolved-view diagnostic overlays the scene.
                if (!field_preview.error.empty()) {
                    require(SDL_SetRenderDrawColor(renderer.get(), 16, 24, 39, 255));
                    const SDL_FRect band{0, 0, 256, 36};
                    require(SDL_RenderFillRect(renderer.get(), &band));
                    require(SDL_SetRenderDrawColor(renderer.get(), 220, 232, 240, 255));
                    require(SDL_RenderDebugText(renderer.get(), 6, 4, "View unavailable"));
                    require(SDL_RenderDebugText(renderer.get(), 6, 15, "Pan back or change layer"));
                }
            } else if (page == Page::battle_background) {
                require(SDL_RenderDebugText(renderer.get(), 8, 164,
                                            background_id == 49 ? "Unused background definition"
                                            : background_id == 50
                                                ? "Assembled Ghost Train graphics"
                                                : "Static battlefield background"));
                require(
                    SDL_RenderDebugText(renderer.get(), 8, 180, "Left/Right: select / Tab: exit"));
                require(SDL_RenderDebugText(renderer.get(), 8, 196,
                                            background_id == 49
                                                ? "Mismatched source tiles/layout"
                                                : "Actors and animation unfinished"));
            } else if (page == Page::scene &&
                       (field_session->halted() || !field_session->session.notice().empty())) {
                const bool menus = field_session->session.battle_menus().has_value();
                const auto* result = menus && field_session->session.battle_menus()->action_result()
                                         ? &*field_session->session.battle_menus()->action_result()
                                         : nullptr;
                require(SDL_SetRenderDrawColor(renderer.get(), 16, 24, 39, 255));
                const SDL_FRect band{0, 0, 256, result ? 68.0F : 44.0F};
                require(SDL_RenderFillRect(renderer.get(), &band));
                require(SDL_SetRenderDrawColor(renderer.get(), 220, 232, 240, 255));
                require(SDL_RenderDebugText(renderer.get(), 6, 4,
                                            result ? "Magitek effect resolved"
                                            : field_session->halted()
                                                ? "Scene stopped"
                                                : "Menu not implemented yet"));
                require(SDL_RenderDebugText(renderer.get(), 6, 16,
                                            result ? "Enemy turns / damage display pending"
                                            : field_session->halted()
                                                ? "See window title for reason"
                                                : "Choose another command"));
                require(SDL_RenderDebugText(renderer.get(), 6, 28,
                                            menus ? "F7: restart / Tab: inspector"
                                                  : "F5: restart / Tab: inspector"));
                if (result && !result->targets.empty()) {
                    const auto& target = result->targets.front();
                    const auto summary =
                        !target.hit ? std::string("Miss")
                        : !target.amount && target.status_before != target.status_after
                            ? std::string("Status applied")
                            : std::string(target.healing ? "Healing " : "Damage ") +
                                  std::to_string(target.amount);
                    const auto hp =
                        "HP " + std::to_string(target.hp_before) + " -> " +
                        std::to_string(target.hp_after) +
                        (result->targets.size() > 1
                             ? " / " + std::to_string(result->targets.size()) + " targets"
                             : "");
                    require(SDL_RenderDebugText(renderer.get(), 6, 40, summary.c_str()));
                    require(SDL_RenderDebugText(renderer.get(), 6, 52, hp.c_str()));
                }
            } else if (page == Page::audio) {
                const auto line = [&](int y, const std::string& value) {
                    require(SDL_RenderDebugText(renderer.get(), 8, static_cast<float>(y),
                                                value.c_str()));
                };
                line(8, "Native audio / Tab: mix");
                line(28, music ? (silent_audio_test       ? "SILENT dummy audio test"
                                  : music->paused()       ? "Paused"
                                  : music->playing_song() ? "Playing"
                                                          : "Music stopped")
                               : "Stopped / M: start");
                line(48, "Song " + std::to_string(song) + " / [ ]: prev-next");
                line(68, "Effect " + std::to_string(effect) + " / Up-Down");
                line(80, "E: play selected effect");
                line(100, "System " + std::to_string(system_effect) + " / Left-Right");
                line(112, "S: play system effect");
                line(128, pause_requested ? "P: resume" : "P: pause");
                line(140, "I: next song, save current");
                line(152, "R: return to saved song");
                line(164, "Saved: " + (music && music->suspended_song()
                                           ? std::to_string(*music->suspended_song())
                                           : std::string("none")));
                line(188, "M: stop / start from beginning");
                line(208, "Preview / fixed echo delay");
            } else if (page == Page::mix) {
                const auto line = [&](int y, const std::string& value) {
                    require(SDL_RenderDebugText(renderer.get(), 8, static_cast<float>(y),
                                                value.c_str()));
                };
                line(8, "Native mix / Tab: portraits");
                line(28, !music                  ? "M: start music preview"
                         : silent_audio_test     ? "SILENT dummy audio test"
                         : music->paused()       ? "Paused / P: resume"
                         : music->playing_song() ? "Playing / P: pause"
                                                 : "Music stopped / [ ]: song");
                if (music) {
                    const auto controls = music->playback_controls();
                    line(48, "Music volume: " + std::to_string(controls.music_volume.value >> 8));
                    line(60, "Effect volume: " + std::to_string(controls.effect_volume.value >> 8));
                    line(72, controls.mono ? "Mono" : "Stereo");
                }
                line(96, "F / G: fade music out / in");
                line(108, "O: toggle mono / stereo");
                line(120, "X: stop music");
                line(132, "U: stop ordinary effects");
                line(144, "B: stop both (system stays)");
                line(156, "C: signal music branch cue");
                line(184, "[ ]: load / M: stop or start");
                line(208, "Preview / fixed echo delay");
            } else if (page == Page::portraits || page == Page::fonts) {
                require(SDL_RenderDebugText(renderer.get(), 8, 7,
                                            page == Page::fonts ? "ROM font / Tab: windows"
                                                                : "ROM portraits / Tab: font"));
                require(SDL_RenderDebugText(renderer.get(), 8, 19, "Native decode / not gameplay"));
                require(SDL_RenderDebugText(renderer.get(), 8, 211,
                                            music ? (silent_audio_test ? "M: stop silent audio test"
                                                                       : "M: stop music preview")
                                                  : "M: native music preview"));
            }
            if (page == Page::scene && !field_session->logging_notice().empty()) {
                require(SDL_SetRenderDrawColor(renderer.get(), 16, 24, 39, 255));
                const SDL_FRect band{0, 208, 256, 16};
                require(SDL_RenderFillRect(renderer.get(), &band));
                require(SDL_SetRenderDrawColor(renderer.get(), 220, 232, 240, 255));
                require(SDL_RenderDebugText(renderer.get(), 4, 212,
                                            field_session->logging_notice().c_str()));
            }
            require(SDL_RenderPresent(renderer.get()));
            if (smoke) {
                if (page == Page::battle_effect) {
                    if (!attack_study->error.empty())
                        throw std::runtime_error(attack_study->error);
                    if (attack_study->complete() && (!music || music->pending_commands() == 0)) {
                        std::cout << "Attack study: definition=" << attack_study->identity
                                  << " frames=" << attack_study->frames()
                                  << " cues=" << attack_study->cues
                                  << " playback=" << (attack_study->action ? "sequence" : "body") << '\n';
                        running = false;
                    } else
                        SDL_Delay(1);
                } else if (page == Page::battle_background) {
                    running = false;
                } else if (page == Page::scene) {
                    if (field_session->session.ticks() >= (startup_entry ? 420U : 320U))
                        running = false;
                    else
                        SDL_Delay(1);
                } else if (page == Page::windows && skin < 7)
                    ++skin;
                else if (page == Page::field && field_preview.combined &&
                         field_preview.smoke_directions < 3) {
                    field_preview.turn_actor();
                    ++field_preview.smoke_directions;
                } else if (page == Page::field && field_preview.combined &&
                           !field_preview.smoke_on_foot) {
                    field_preview.toggle_vehicle();
                    field_preview.smoke_on_foot = true;
                    field_preview.smoke_directions = 0;
                    field_preview.step_actor();
                } else if (page == Page::field && field_preview.combined &&
                           field_preview.show_actor) {
                    field_preview.show_actor = false;
                } else if (page == Page::field && field_preview.combined) {
                    field_preview.combined = false;
                    field_preview.layer = 0;
                    field_preview.reset();
                } else if (page == Page::field && field_preview.layer < 2) {
                    ++field_preview.layer;
                    field_preview.reset();
                } else if (page == Page::mix) {
                    if (!music || music->rendered_frames() >= ff6::music_sample_rate)
                        running = false;
                    else
                        SDL_Delay(16);
                } else
                    next_page();
            } else
                SDL_Delay(16);
        }
        if (smoke && music && music->pending_commands() != 0)
            throw std::runtime_error("Audio smoke test did not consume its command sequence");
        if (field_session && (field_entry || startup_entry)) {
            std::cout << "Field session: ticks=" << field_session->session.ticks()
                      << " audio_frames=" << field_session->audio_frames
                      << " commands=" << field_session->commands
                      << " maps=" << field_session->session.maps_loaded()
                      << " max_queued_bytes=" << field_session->max_queued_bytes << '\n';
        }
        std::cout << "Native content inspector completed; music preview frames: "
                  << (music ? music->rendered_frames() : 0)
                  << "; no full gameplay or audio accuracy claim\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ff6-inspector: " << error.what() << '\n';
        return 1;
    }
}
