cmake_minimum_required(VERSION 3.25)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS")
endif()
project(ff6_native VERSION 0.1.0 LANGUAGES CXX)
# Builder releases are independent of the frozen extractor/game format version.
set(FF6_BUILDER_VERSION "0.5.2")
set(FF6_BUILDER_BUILD "7")
if(APPLE)
  enable_language(OBJC OBJCXX)
  set(CMAKE_OBJCXX_STANDARD 20)
  set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(FF6_BUILD_INSPECTOR "Build the SDL3 development content inspector" OFF)
option(FF6_BUILD_BUILDER "Build the self-contained macOS builder" ON)
option(FF6_USE_SYSTEM_SDL "Use installed SDL3 for development only (not self-contained)" OFF)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_custom_target(ff6-source-check
  COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_repository.py"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)

add_library(ff6_options INTERFACE)
target_compile_options(ff6_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

set(inventory_header "${CMAKE_CURRENT_BINARY_DIR}/generated/inventory.inc")
add_custom_command(OUTPUT "${inventory_header}"
  COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_inventory.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/config/rom_inventory.json" "${inventory_header}"
  DEPENDS tools/generate_inventory.py config/rom_inventory.json VERBATIM)
add_library(ff6_foundation src/sha256.cpp src/io.cpp)
target_include_directories(ff6_foundation PUBLIC include)
target_link_libraries(ff6_foundation PUBLIC ff6_options)
add_library(ff6_content src/content.cpp src/codecs.cpp src/content_audit.cpp "${inventory_header}")
target_include_directories(ff6_content PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(ff6_content PRIVATE FF6_EXTRACTOR_VERSION="${PROJECT_VERSION}")
target_link_libraries(ff6_content PUBLIC ff6_foundation)

# Private native inputs are produced by the same extractor used by Builder.
# Only metadata is generated without a ROM. No content-bearing include is tracked.
set(FF6_ROM_FILE "" CACHE FILEPATH "Private user-provided USA Rev 1 ROM for developer packaging")
set(material_metadata "${CMAKE_CURRENT_BINARY_DIR}/generated/rom_material_metadata.inc")
add_custom_command(OUTPUT "${material_metadata}"
  COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_material_metadata.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/rom_materials.json" "${material_metadata}"
  DEPENDS tools/generate_material_metadata.py tools/native_fields.py config/native_fields.json
          config/rom_materials.json config/rom_inventory.json VERBATIM)
add_library(ff6_rom_material src/rom_material.cpp src/native_fill.cpp "${material_metadata}")
target_include_directories(ff6_rom_material PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(ff6_rom_material PUBLIC ff6_content)
add_executable(ff6-rom-inputs tools/rom_inputs_main.cpp)
target_link_libraries(ff6-rom-inputs PRIVATE ff6_rom_material)
set(material_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/rom-materials")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/rom_materials.json" material_document)
string(JSON material_count LENGTH "${material_document}" materials)
math(EXPR material_last "${material_count} - 1")
set(material_outputs)
foreach(material_index RANGE ${material_last})
  string(JSON material_id GET "${material_document}" materials ${material_index} id)
  list(APPEND material_outputs "${material_directory}/${material_id}.inc"
                               "${material_directory}/${material_id}.first.inc"
                               "${material_directory}/${material_id}.macros.inc"
                               "${material_directory}/${material_id}.bin")
  string(JSON material_transform ERROR_VARIABLE material_transform_error
    GET "${material_document}" materials ${material_index} transform)
  if(material_transform STREQUAL "attribute_rectangle_words" OR
     material_transform STREQUAL "unit_acceleration_motion")
    list(APPEND material_outputs "${material_directory}/${material_id}.source.macros.inc")
  endif()
endforeach()
add_custom_command(OUTPUT ${material_outputs}
  COMMAND ff6-rom-inputs "${FF6_ROM_FILE}" "${material_directory}"
  DEPENDS ff6-rom-inputs "${FF6_ROM_FILE}" config/rom_materials.json VERBATIM)
add_custom_target(ff6-private-rom-inputs DEPENDS ${material_outputs})

add_library(ff6_video src/video.cpp src/raster.cpp src/perspective.cpp)
target_link_libraries(ff6_video PUBLIC ff6_content)
# Camera defaults in public headers are private ROM-derived build inputs.
target_include_directories(ff6_video PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(ff6_audio src/audio.cpp src/samples.cpp src/voice.cpp src/mixer.cpp src/synthesis.cpp src/sequencing.cpp src/music_scripts.cpp src/driver_controls.cpp src/driver_updates.cpp src/note_generation.cpp src/playback_controls.cpp src/driver_loading.cpp src/driver_sequence.cpp src/driver_clock.cpp src/driver_poll.cpp src/driver_publication.cpp src/driver_timeline.cpp)
target_link_libraries(ff6_audio PUBLIC ff6_content)
# DriverState's initial countdown is a private ROM-derived input, also needed
# by consumers that instantiate the public state type.
target_include_directories(ff6_audio PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_sources(ff6_audio PRIVATE src/music_player.cpp src/audio_performance.cpp)

add_library(ff6_input src/input.cpp)
target_link_libraries(ff6_input PUBLIC ff6_foundation)
# Public input-state defaults use private ROM inputs in every consuming target.
target_include_directories(ff6_input PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(ff6_game_time src/game_time.cpp)
target_link_libraries(ff6_game_time PUBLIC ff6_foundation)
add_library(ff6_battle_formation src/battle_formation.cpp)
target_link_libraries(ff6_battle_formation PUBLIC ff6_content)
add_library(ff6_original_sram src/original_sram.cpp)
target_link_libraries(ff6_original_sram PUBLIC ff6_foundation)
add_library(ff6_party src/party.cpp src/esper.cpp)
target_link_libraries(ff6_party PUBLIC ff6_content)
add_library(ff6_battle_party src/battle_party.cpp)
target_link_libraries(ff6_battle_party PUBLIC ff6_party ff6_battle_formation)
add_library(ff6_equipment src/equipment.cpp)
target_link_libraries(ff6_equipment PUBLIC ff6_party)
add_library(ff6_battle_stats src/battle_stats.cpp)
target_link_libraries(ff6_battle_stats PUBLIC ff6_battle_party ff6_equipment)
add_library(ff6_battle_commands src/battle_commands.cpp)
target_link_libraries(ff6_battle_commands PUBLIC ff6_battle_stats)
add_library(ff6_field_events src/field_events.cpp)
target_link_libraries(ff6_field_events PUBLIC ff6_content)
add_library(ff6_field_map src/field_map.cpp src/field_camera.cpp src/field_animation.cpp src/field_display.cpp src/field_sprites.cpp)
target_sources(ff6_field_map PRIVATE src/field_pyramid.cpp)
target_link_libraries(ff6_field_map PUBLIC ff6_video)
add_library(ff6_field_map_updates src/field_map_updates.cpp)
target_link_libraries(ff6_field_map_updates PUBLIC ff6_field_map ff6_field_events)
add_library(ff6_inventory src/inventory.cpp)
target_link_libraries(ff6_inventory PUBLIC ff6_foundation)
add_library(ff6_battle_preparation src/battle_inventory.cpp src/battle_skills.cpp src/battle_preparation.cpp)
target_link_libraries(ff6_battle_preparation PUBLIC ff6_battle_commands ff6_inventory ff6_input)
add_library(ff6_battle_status src/battle_status.cpp)
target_link_libraries(ff6_battle_status PUBLIC ff6_battle_preparation)
add_library(ff6_battle_refresh src/battle_refresh.cpp)
target_link_libraries(ff6_battle_refresh PUBLIC ff6_battle_status)
add_library(ff6_battle_background src/battle_background.cpp)
target_link_libraries(ff6_battle_background PUBLIC ff6_video)
add_library(ff6_battle_monster_graphics src/battle_monster_graphics.cpp)
target_link_libraries(ff6_battle_monster_graphics PUBLIC ff6_battle_background)
add_library(ff6_battle_monster_view src/battle_monster_view.cpp src/battle_drawing.cpp)
target_link_libraries(ff6_battle_monster_view PUBLIC ff6_battle_monster_graphics ff6_battle_state)
add_library(ff6_battle_character_graphics src/battle_character_graphics.cpp)
target_link_libraries(ff6_battle_character_graphics PUBLIC ff6_video)
add_library(ff6_battle_character_view src/battle_character_view.cpp src/battle_character_presentation.cpp)
target_link_libraries(ff6_battle_character_view PUBLIC ff6_battle_character_graphics ff6_battle_monster_view)
add_library(ff6_battle_animation src/battle_animation.cpp src/battle_character_motion.cpp)
target_link_libraries(ff6_battle_animation PUBLIC ff6_battle_character_view)
add_library(ff6_battle_effects src/battle_monster_transition.cpp src/battle_action_animation.cpp src/battle_effect_assets.cpp src/battle_effects.cpp src/battle_effect_render.cpp src/battle_effect_palette.cpp src/battle_effect_motion.cpp src/battle_effect_scroll.cpp src/battle_effect_mask.cpp src/battle_effect_gradient.cpp src/battle_effect_raster.cpp src/battle_effect_affine.cpp src/battle_actor_background.cpp)
target_link_libraries(ff6_battle_effects PUBLIC ff6_battle_actor_scene ff6_audio)
target_sources(ff6_battle_effects PRIVATE src/battle_raster_timeline.cpp)
target_sources(ff6_battle_effects PRIVATE src/battle_display_clock.cpp src/battle_task_clock.cpp src/battle_scroll_publication.cpp)
add_library(ff6_battle_actor_scene src/battle_actor_scene.cpp src/battle_fade.cpp src/battle_display_transfers.cpp)
target_sources(ff6_battle_actor_scene PRIVATE src/battle_damage_numbers.cpp)
target_sources(ff6_battle_actor_scene PRIVATE src/battle_monster_death.cpp)
target_link_libraries(ff6_battle_actor_scene PUBLIC ff6_battle_animation)
add_library(ff6_battle_scene src/battle_readout.cpp src/battle_scene.cpp)
target_link_libraries(ff6_battle_scene PUBLIC ff6_battle_animation ff6_menu)
add_library(ff6_battle_entrance src/battle_entrance.cpp src/battle_final.cpp)
target_link_libraries(ff6_battle_entrance PUBLIC ff6_battle_actor_scene ff6_battle_scene ff6_audio ff6_battle_effects)
add_library(ff6_battle_state src/battle_state.cpp src/battle_schedule.cpp src/battle_entry.cpp src/battle_display.cpp src/battle_time.cpp src/battle_submission.cpp src/battle_dance.cpp)
target_link_libraries(ff6_battle_state PUBLIC ff6_battle_refresh)
add_library(ff6_battle_menu_navigation src/battle_menu_navigation.cpp)
target_link_libraries(ff6_battle_menu_navigation PUBLIC ff6_battle_state ff6_input)

add_library(ff6_battle_target_selection src/battle_target_selection.cpp)
target_link_libraries(ff6_battle_target_selection PUBLIC ff6_battle_menu_navigation ff6_battle_monster_view)
add_library(ff6_battle_command_selection src/battle_command_selection.cpp)
target_link_libraries(ff6_battle_command_selection PUBLIC ff6_battle_target_selection)
add_library(ff6_battle_menu_transition src/battle_menu_transition.cpp)
target_link_libraries(ff6_battle_menu_transition PUBLIC ff6_video)
add_library(ff6_battle_menu_surfaces src/battle_menu_surfaces.cpp src/battle_menu_initialization.cpp)
target_link_libraries(ff6_battle_menu_surfaces PUBLIC ff6_battle_scene ff6_battle_menu_navigation ff6_battle_menu_transition)

add_library(ff6_battle_cursors src/battle_cursors.cpp)
target_link_libraries(ff6_battle_cursors PUBLIC ff6_battle_character_view ff6_battle_target_selection)
add_library(ff6_field_state src/field_state.cpp)
target_link_libraries(ff6_field_state PUBLIC ff6_field_events ff6_equipment ff6_inventory)
add_library(ff6_field_object_view src/field_object_view.cpp)
target_link_libraries(ff6_field_object_view PUBLIC ff6_field_state ff6_field_map)
add_library(ff6_field_movement src/field_movement.cpp)
target_link_libraries(ff6_field_movement PUBLIC ff6_field_object_view ff6_input)
add_library(ff6_field_overlay src/field_overlay.cpp)
target_link_libraries(ff6_field_overlay PUBLIC ff6_field_movement)
add_library(ff6_field_npcs src/field_npcs.cpp)
target_link_libraries(ff6_field_npcs PUBLIC ff6_field_movement)
add_library(ff6_field_object_actions src/field_object_actions.cpp src/field_action_schedule.cpp src/field_player_effects.cpp src/field_doors.cpp src/field_treasure.cpp src/field_contact.cpp src/field_player.cpp)
target_link_libraries(ff6_field_object_actions PUBLIC ff6_field_npcs ff6_field_overlay)
add_library(ff6_field_map_state src/field_map_state.cpp src/field_map_transition.cpp src/field_map_startup.cpp src/field_entrances.cpp)
target_link_libraries(ff6_field_map_state PUBLIC ff6_field_npcs)
add_library(ff6_field_loop src/field_loop.cpp src/field_map_entry.cpp src/field_load_menu.cpp src/field_battle.cpp)
target_sources(ff6_field_loop PRIVATE src/field_party_switch.cpp src/event_timer.cpp)
add_library(ff6_field_dialogue src/field_dialogue.cpp)
target_link_libraries(ff6_field_dialogue PUBLIC ff6_field_events ff6_video ff6_input)
target_link_libraries(ff6_field_loop PUBLIC ff6_field_map_state ff6_field_object_actions ff6_field_map_updates ff6_field_dialogue ff6_original_sram ff6_game_time ff6_menu)
add_library(ff6_battle_magitek src/battle_magitek.cpp)
target_link_libraries(ff6_battle_magitek PUBLIC ff6_battle_state ff6_battle_action_preparation)
add_library(ff6_battle_physical src/battle_physical.cpp)
target_link_libraries(ff6_battle_physical PUBLIC ff6_battle_action_preparation)
add_library(ff6_battle_action_preparation src/battle_action_preparation.cpp)
target_link_libraries(ff6_battle_action_preparation PUBLIC ff6_battle_state)
add_library(ff6_battle_ai src/battle_ai.cpp)
target_link_libraries(ff6_battle_ai PUBLIC ff6_battle_state)
add_library(ff6_battle_queue src/battle_queue.cpp)
target_link_libraries(ff6_battle_queue PUBLIC ff6_battle_ai ff6_battle_action_preparation)
add_library(ff6_battle_execution src/battle_execution.cpp)
target_link_libraries(ff6_battle_execution PUBLIC ff6_battle_queue)
add_library(ff6_battle_rewards src/battle_rewards.cpp)
target_link_libraries(ff6_battle_rewards PUBLIC ff6_battle_state)
add_library(ff6_battle_message src/battle_message.cpp)
target_link_libraries(ff6_battle_message PUBLIC ff6_battle_scene)
add_library(ff6_battle_event src/battle_event.cpp)
target_link_libraries(ff6_battle_event PUBLIC ff6_battle_effects ff6_battle_actor_scene ff6_battle_message ff6_battle_menu_transition)
add_library(ff6_battle_victory src/battle_victory.cpp)
target_link_libraries(ff6_battle_victory PUBLIC ff6_battle_rewards ff6_battle_actor_scene ff6_battle_message)
add_library(ff6_battle_menu_session src/battle_menu_session.cpp src/battle_blitz.cpp src/battle_slot.cpp)
target_link_libraries(ff6_battle_menu_session PUBLIC ff6_battle_menu_surfaces ff6_battle_cursors ff6_battle_command_selection ff6_battle_entrance ff6_battle_magitek ff6_battle_effects ff6_battle_execution ff6_battle_physical ff6_battle_rewards ff6_battle_victory ff6_battle_event)
add_library(ff6_world src/world_map.cpp src/world_scene.cpp src/world_chocobo.cpp
  src/world_script.cpp src/world_vehicle.cpp src/world_approach.cpp src/world_airship.cpp src/world_aftermath.cpp src/world_explosion.cpp src/world_falcon.cpp src/world_judgment.cpp)
target_link_libraries(ff6_world PUBLIC ff6_field_loop)
add_library(ff6_mine_cart src/mine_cart.cpp)
target_link_libraries(ff6_mine_cart PUBLIC ff6_field_loop)
add_library(ff6_field_session src/field_session.cpp src/field_battle_change.cpp src/field_boot.cpp src/world_session.cpp src/mine_cart_session.cpp src/failure_log.cpp)
target_compile_definitions(ff6_field_session PRIVATE FF6_ENGINE_VERSION="${PROJECT_VERSION}")
target_link_libraries(ff6_field_session PUBLIC ff6_mine_cart)
target_link_libraries(ff6_field_session PUBLIC ff6_world)
add_library(ff6_saves src/saved_game_codec.cpp src/saved_game.cpp src/save_store.cpp src/save_menu.cpp)
target_link_libraries(ff6_saves PUBLIC ff6_field_loop ff6_menu)
target_link_libraries(ff6_field_session PUBLIC ff6_field_loop ff6_audio ff6_battle_formation ff6_battle_party ff6_battle_state ff6_battle_entrance ff6_battle_menu_session)
add_library(ff6_title src/title.cpp src/title_animation.cpp src/title_presentation.cpp src/opening_landscape.cpp src/cutscene_animation.cpp src/opening_assets.cpp src/opening.cpp src/opening_presentation.cpp)
target_link_libraries(ff6_title PUBLIC ff6_video ff6_content ff6_audio)
# Opening state defaults are shared by the title owner's public interfaces.
# Their data-only includes remain private build products, never repository assets.
target_include_directories(ff6_title PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_sources(ff6_title PRIVATE src/floating_continent.cpp src/world_ruin.cpp src/ending_book.cpp src/ending_characters.cpp src/ending_flight.cpp src/ending_flight_assets.cpp src/ending_finale.cpp)
target_link_libraries(ff6_field_session PUBLIC ff6_title)
add_library(ff6_menu src/menu_windows.cpp src/main_menu.cpp src/menu_navigation.cpp src/party_menu.cpp src/party_selection.cpp src/final_order.cpp src/name_menu.cpp src/menu_items.cpp src/menu_skills.cpp src/menu_canvas.cpp src/field_menu.cpp src/field_menu_render.cpp src/field_menu_status.cpp src/field_menu_config.cpp src/field_menu_skills.cpp src/shop.cpp src/shop_render.cpp src/colosseum.cpp src/colosseum_menu.cpp)
target_link_libraries(ff6_field_session PUBLIC ff6_menu ff6_saves)
target_link_libraries(ff6_menu PUBLIC ff6_battle_monster_graphics ff6_battle_background ff6_field_map ff6_video ff6_input ff6_party ff6_equipment ff6_inventory ff6_battle_commands ff6_game_time)

foreach(material_owner ff6_field_dialogue ff6_battle_character_view ff6_battle_physical ff6_video ff6_battle_scene ff6_battle_magitek ff6_battle_queue ff6_world ff6_title
                       ff6_battle_monster_view ff6_battle_stats ff6_battle_state ff6_battle_refresh ff6_battle_execution ff6_party ff6_battle_target_selection
                       ff6_battle_menu_session ff6_battle_menu_surfaces ff6_mine_cart ff6_menu ff6_input ff6_audio
                       ff6_battle_victory ff6_battle_message ff6_saves ff6_battle_status ff6_battle_rewards ff6_battle_effects
                       ff6_field_map ff6_field_movement ff6_field_overlay ff6_field_object_actions ff6_battle_actor_scene ff6_field_session ff6_field_loop ff6_field_map_state
                       ff6_battle_entrance ff6_field_events ff6_field_state ff6_battle_cursors)
  add_dependencies(${material_owner} ff6-private-rom-inputs)
  target_include_directories(${material_owner} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endforeach()

if(FF6_BUILD_INSPECTOR OR FF6_BUILD_BUILDER)
  if(FF6_USE_SYSTEM_SDL)
    find_package(SDL3 3.4.16 EXACT CONFIG REQUIRED)
  else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
      URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(SDL3)
  endif()
  add_executable(ff6-inspector src/inspector_main.cpp)
  target_link_libraries(ff6-inspector PRIVATE ff6_menu ff6_field_session ff6_battle_background SDL3::SDL3)
  add_executable(ff6-game src/game_main.cpp)
  add_library(ff6_presentation src/presentation.cpp src/game_presenter.cpp)
  target_include_directories(ff6_presentation PUBLIC src)
  target_link_libraries(ff6_presentation PUBLIC ff6_video SDL3::SDL3)
  if(APPLE)
    # A per-build selection leaves xcode-select and the player's machine alone.
    set(FF6_METAL_DEVELOPER_DIR "$ENV{DEVELOPER_DIR}" CACHE PATH
        "Developer directory providing the offline Metal compiler")
    if(NOT FF6_METAL_DEVELOPER_DIR)
      execute_process(COMMAND xcode-select -p OUTPUT_VARIABLE FF6_METAL_DEVELOPER_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    endif()
    set(crt_library "${CMAKE_CURRENT_BINARY_DIR}/crt.metallib")
    add_custom_command(OUTPUT "${crt_library}"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_metal.py"
        --identity "${CMAKE_CURRENT_SOURCE_DIR}/config/metal_identity.json"
        --source "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/crt.metal"
        --output "${crt_library}" --developer-dir "${FF6_METAL_DEVELOPER_DIR}"
      DEPENDS src/shaders/crt.metal tools/build_metal.py config/metal_identity.json VERBATIM)
    add_custom_target(ff6-crt-shaders DEPENDS "${crt_library}")
    target_sources(ff6_presentation PRIVATE src/metal_pipeline.mm src/metal_presenter.mm src/macos_settings.mm)
    target_compile_options(ff6_presentation PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
    target_link_libraries(ff6_presentation PRIVATE "-framework AppKit" "-framework Metal" "-framework QuartzCore")
    add_dependencies(ff6-game ff6-crt-shaders)
  endif()
  target_link_libraries(ff6-game PRIVATE ff6_field_session ff6_presentation)
endif()

if(FF6_BUILD_BUILDER)
  if(NOT APPLE OR FF6_USE_SYSTEM_SDL)
    message(FATAL_ERROR "Builder requires macOS and bundled static SDL")
  endif()
  add_library(ff6_builder_icon src/builder_icon.mm)
  target_link_libraries(ff6_builder_icon PUBLIC ff6_field_map "-framework Foundation" "-framework CoreGraphics" "-framework ImageIO")
  target_compile_options(ff6_builder_icon PRIVATE -fobjc-arc)
  add_executable(ff6-builder-icon-generator tools/builder_icon_main.cpp)
  target_include_directories(ff6-builder-icon-generator PRIVATE src)
  target_link_libraries(ff6-builder-icon-generator PRIVATE ff6_builder_icon)
  set(builder_icon "${CMAKE_CURRENT_BINARY_DIR}/generated/BuilderIcon.icns")
  set(FF6_BUILDER_ICON_ARTWORK "" CACHE FILEPATH "Optional local builder icon PNG; empty uses procedural artwork")
  set(builder_icon_arguments "${builder_icon}")
  set(builder_icon_dependencies ff6-builder-icon-generator)
  if(FF6_BUILDER_ICON_ARTWORK)
    list(APPEND builder_icon_arguments "${FF6_BUILDER_ICON_ARTWORK}")
    list(APPEND builder_icon_dependencies "${FF6_BUILDER_ICON_ARTWORK}")
  endif()
  # Also regenerate when changing back to the source-only procedural design.
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/builder-icon-input.txt"
    CONTENT "${FF6_BUILDER_ICON_ARTWORK}\n")
  add_custom_command(OUTPUT "${builder_icon}"
    COMMAND ff6-builder-icon-generator ${builder_icon_arguments}
    DEPENDS ${builder_icon_dependencies} "${CMAKE_CURRENT_BINARY_DIR}/generated/builder-icon-input.txt" VERBATIM)
  add_custom_target(ff6-builder-icon DEPENDS "${builder_icon}")
  add_executable(ff6-builder MACOSX_BUNDLE src/builder_main.mm)
  target_link_libraries(ff6-builder PRIVATE ff6_builder_icon ff6_rom_material "-framework AppKit" "-framework Foundation")
  target_compile_options(ff6-builder PRIVATE -fobjc-arc)
  set_target_properties(ff6-builder PROPERTIES OUTPUT_NAME "FFVI Builder"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.ff6native.builder"
    MACOSX_BUNDLE_BUNDLE_NAME "FFVI Builder"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/config/BuilderInfo.plist.in")
  add_dependencies(ff6-builder ff6-inspector ff6-game)
  # Re-seal both templates and the outer bundle after either host changes.
  add_custom_target(ff6-builder-package ALL
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_builder.py"
      "$<TARGET_BUNDLE_DIR:ff6-builder>" "$<TARGET_FILE:ff6-inspector>" "$<TARGET_FILE:ff6-game>" "${sdl3_SOURCE_DIR}/LICENSE.txt" "${crt_library}" "${builder_icon}" "${material_directory}"
    DEPENDS ff6-builder ff6-inspector ff6-game ff6-crt-shaders ff6-builder-icon
    VERBATIM)
endif()
