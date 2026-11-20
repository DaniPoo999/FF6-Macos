#include "ff6/codecs.hpp"
#include "ff6/content.hpp"
#include <algorithm>
#include <sstream>

namespace ff6 {
std::string audit_json(const ContentPack& pack) {
    verify_canonical(pack);
    std::ostringstream out;
    out << "{\n  \"schema\": 1,\n  \"coverage\": \"selected structural codecs, not gameplay/audio "
           "fidelity\",\n  \"decoded\": [\n";
    bool first = true;
    const auto record = [&](const std::string& id, ByteView bytes, std::size_t count) {
        if (!first)
            out << ",\n";
        first = false;
        out << "    {\"id\": \"" << id << "\", \"count\": " << count
            << ", \"size\": " << bytes.size() << ", \"sha256\": \"" << hex(sha256(bytes)) << "\"}";
    };
    const auto tiles = [&](std::string_view id, unsigned bpp, bool compressed = false) {
        const auto resource = pack.get(id);
        const auto raw =
            compressed ? decompress_lzss(resource) : Bytes(resource.begin(), resource.end());
        const auto decoded = decode_tiles(raw, bpp);
        if (encode_tiles(decoded, bpp) != raw)
            throw std::runtime_error("Tile roundtrip mismatch");
        Bytes pixels;
        for (const auto& tile : decoded)
            append(pixels, tile);
        record(std::string(id), pixels, decoded.size());
    };
    tiles("menu_graphics_debug_font_2bpp", 2);
    tiles("menu_graphics_small_font_2bpp", 2);
    tiles("menu_graphics_slot_gfx_4bpp", 4);
    tiles("menu_graphics_window_gfx_4bpp", 4);
    tiles("menu_graphics_portrait_gfx_4bpp", 4);
    const auto portrait_order = decode_portrait_order(pack.get("field_portrait_tile_order"));
    record("portrait_tile_order", portrait_order, portrait_order.size());
    tiles("menu_graphics_menu_sprite_4bpp", 4);
    tiles("menu_graphics_status_gfx_4bpp_lz", 4, true);
    const auto glyphs = decode_glyphs(pack.get("large_font_large_font_rows_1bpp"));
    Bytes glyph_pixels;
    for (const auto& glyph : glyphs)
        for (auto row : glyph)
            for (unsigned x = 0; x < 16; ++x)
                glyph_pixels.push_back(static_cast<std::uint8_t>((row >> (15U - x)) & 1U));
    record("large_font", glyph_pixels, glyphs.size());
    const auto formations = pack.get("formation_battle_monsters_bin");
    if (formations.size() != 576 * 15)
        throw std::runtime_error("Formation inventory changed");
    Bytes semantic_formations;
    for (std::size_t i = 0; i < 576; ++i) {
        const auto raw = slice(formations, i * 15, 15);
        const auto formation = decode_formation(raw);
        if (!std::ranges::equal(encode_formation(formation), raw))
            throw std::runtime_error("Formation roundtrip mismatch");
        semantic_formations.push_back(formation.layout_id);
        semantic_formations.push_back(formation.extension_high_bits);
        for (const auto& slot : formation.slots) {
            append16(semantic_formations, slot.monster_id);
            semantic_formations.push_back(slot.x);
            semantic_formations.push_back(slot.y);
            semantic_formations.push_back(slot.present ? 1 : 0);
            semantic_formations.push_back(slot.background_layer ? 1 : 0);
        }
    }
    record("formations", semantic_formations, 576);
    const auto samples =
        split_instrument_samples(pack.get("brr_sample_brr_bin"), pack.get("brr_tables_pointers"));
    for (const auto& sample : samples) {
        const auto blocks = decode_brr(sample.brr);
        if (encode_brr(blocks) != sample.brr)
            throw std::runtime_error("BRR roundtrip mismatch");
        Bytes fields;
        for (const auto& block : blocks) {
            fields.push_back(block.shift);
            fields.push_back(block.filter);
            fields.push_back(block.loop ? 1 : 0);
            fields.push_back(block.end ? 1 : 0);
            for (auto residual : block.residuals)
                fields.push_back(static_cast<std::uint8_t>(int{residual} + 8));
        }
        record("instrument_" + std::to_string(sample.index), fields, blocks.size());
    }
    const auto pointers = pack.get("submap_sub_tilemap_ptr_bin");
    const auto maps = pack.get("submap_sub_tilemap_bin_lz");
    if (pointers.size() != 351 * 3 || read24(pointers, 0) != 0 ||
        read24(pointers, 350 * 3) != maps.size())
        throw std::runtime_error("Submap pointer bounds mismatch");
    for (std::size_t i = 0; i < 350; ++i) {
        const auto start = read24(pointers, i * 3), end = read24(pointers, (i + 1) * 3);
        if (end <= start)
            throw std::runtime_error("Unordered submap pointers");
        const auto decoded = decompress_lzss(slice(maps, start, end - start), 8192);
        const auto size = decoded.size();
        if (size < 256 || size > 8192 || (size & (size - 1)) != 0)
            throw std::runtime_error("Invalid submap dimensions");
        record("submap_" + std::to_string(i), decoded, size);
    }
    out << "\n  ]\n}\n";
    return out.str();
}
} // namespace ff6
