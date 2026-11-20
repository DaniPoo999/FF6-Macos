#pragma once
#include "ff6/field_sprites.hpp"
#include <filesystem>

namespace ff6 {
// macOS packaging only. Artwork is rendered from the supplied content, never
// stored in the asset-free builder or its application templates.
Bytes make_game_icon(const FieldSpriteAssets& sprites);
// Original procedural artwork; needs no ROM or content package.
Bytes make_builder_icon();
// Optional local PNG artwork, encoded for Finder/Dock during developer packaging.
Bytes make_builder_icon(ByteView artwork);
void install_game_icon(const ContentPack& pack, const std::filesystem::path& app);
} // namespace ff6
