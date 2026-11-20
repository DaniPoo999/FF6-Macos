#include "ff6/rom_material.hpp"
#include "ff6/codecs.hpp"
#include "ff6/native_fill.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <map>
#include <numeric>
#include <cstddef>

namespace ff6 {
#if defined(__APPLE__) && defined(__aarch64__)
// Packaging validates these generated format fields independently of content.
// Keep the scanner's ARM64 layout explicit and reject toolchain ABI drift.
static_assert(sizeof(RomMaterialSpec)==168 && offsetof(RomMaterialSpec,offset)==16 &&
              offsetof(RomMaterialSpec,count)==20 && offsetof(RomMaterialSpec,source_width)==24 &&
              offsetof(RomMaterialSpec,native_width)==28 && offsetof(RomMaterialSpec,source_count)==32);
#endif
namespace {
struct RomMaterialAlias {
    std::uint32_t material, target, first, count;
};
#include "rom_material_metadata.inc"
constexpr std::array<std::uint8_t, 8> restoration_magic{'F','F','6','R','S','T','0','3'};
void require(bool ok, const char* reason) {
    if (!ok) throw std::runtime_error(reason);
}
void validate_field(const NativeMaterialField& field, std::size_t count) {
    require(field.first < count && field.count > 0 &&
                field.source_stride > 0 && field.source_stride <= 65536 &&
                field.count - 1U <= (count - 1U - field.first) / field.source_stride &&
                field.multiplier > 0 && field.multiplier <= 65536 &&
                field.divisor_shift <= 16 && field.width > 0 && field.width <= 24 &&
                field.destination_shift <= 32 - field.width,
            "Invalid native material field");
    require(((field.projection == NativeMaterialProjection::scaled_quotient_shift ||
                  field.projection == NativeMaterialProjection::scaled_quotient_mask32) &&
                 field.count == 2 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 &&
                 field.destination_shift == (field.projection == NativeMaterialProjection::scaled_quotient_shift ? 16U : 10U) &&
                 field.width == (field.projection == NativeMaterialProjection::scaled_quotient_shift ? 6U : 13U)) ||
                ((field.projection == NativeMaterialProjection::scaled_floor_bias ||
                  field.projection == NativeMaterialProjection::simd_scaled_floor_bias) &&
                 field.count <= 16 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 5 &&
                 field.width == (field.projection == NativeMaterialProjection::scaled_floor_bias ? 16U : 14U)) ||
                (field.projection == NativeMaterialProjection::relative_membership_halfword &&
                 field.count <= 64 && field.multiplier == 1 && field.addend >= 0 && field.addend < 4 &&
                 field.divisor_shift == 0 && field.destination_shift == 5 && field.width == 16) ||
                (field.projection == NativeMaterialProjection::halfword_lane_shift64 &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 6) ||
                (field.projection == NativeMaterialProjection::halfword_pair_residue64 &&
                 field.count == 2 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 13) ||
                (field.projection == NativeMaterialProjection::halfword_lane_insert64 &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 12) ||
                (field.projection == NativeMaterialProjection::two_bit_scaled_right_shift &&
                 field.count == 1 && field.multiplier == 1 && field.addend > 0 && field.addend < 32 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 12) ||
                (field.projection == NativeMaterialProjection::byte_power_of_two_bit_select &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 12) ||
                (field.projection == NativeMaterialProjection::nonnegative_byte_insert_width &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 6) ||
                (field.projection == NativeMaterialProjection::structure_load_opcode &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 12 && field.width == 4) ||
                (field.projection == NativeMaterialProjection::simd_movi_byte &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 5 && field.width == 14) ||
                (field.projection == NativeMaterialProjection::bg2_priority_depth &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 && field.divisor_shift == 0) ||
                (field.projection == NativeMaterialProjection::simd_right_shift32 &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 16 && field.width == 6) ||
                ((field.projection == NativeMaterialProjection::corrected_reciprocal_low16 ||
                  field.projection == NativeMaterialProjection::corrected_reciprocal_high16 ||
                  field.projection == NativeMaterialProjection::corrected_reciprocal_shift) &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 &&
                 field.destination_shift == (field.projection == NativeMaterialProjection::corrected_reciprocal_shift ? 16U : 5U) &&
                 field.width == (field.projection == NativeMaterialProjection::corrected_reciprocal_shift ? 6U : 16U)) ||
                ((field.projection == NativeMaterialProjection::word_bound_shift ||
                  field.projection == NativeMaterialProjection::word_bound_units) &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 &&
                 field.destination_shift == (field.projection == NativeMaterialProjection::word_bound_shift ? 16U : 10U) &&
                 field.width == (field.projection == NativeMaterialProjection::word_bound_shift ? 6U : 12U)) ||
                (field.projection == NativeMaterialProjection::scaled_byte_logical64 &&
                 field.count == 1 && std::has_single_bit(field.multiplier) && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 13) ||
                (field.projection == NativeMaterialProjection::word_bound_previous_units &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 12) ||
                (field.projection == NativeMaterialProjection::adjacent_power_of_two_shift &&
                 field.count >= 2 && field.count <= 16 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 5 && field.width == 16) ||
                (field.projection == NativeMaterialProjection::opposite_byte_complement &&
                 field.count == 2 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 5 && field.width == 16) ||
                (field.projection == NativeMaterialProjection::unsigned_difference && field.count == 2) ||
                (field.projection == NativeMaterialProjection::simd_difference_byte && field.count == 2 &&
                 field.multiplier == 1 && field.addend == 0 && field.divisor_shift == 0 &&
                 field.destination_shift == 5 && field.width == 14) ||
                ((field.projection == NativeMaterialProjection::simd_affine_byte ||
                  field.projection == NativeMaterialProjection::simd_affine_halfword_byte ||
                  field.projection == NativeMaterialProjection::simd_affine_halfword_clear_byte) && field.count == 1 &&
                 field.destination_shift == 5 && field.width == 14) ||
                ((field.projection == NativeMaterialProjection::byte_reciprocal_low16 ||
                  field.projection == NativeMaterialProjection::byte_reciprocal_high16 ||
                  field.projection == NativeMaterialProjection::byte_min_reciprocal_multiplier ||
                  field.projection == NativeMaterialProjection::byte_min_reciprocal_shift ||
                  field.projection == NativeMaterialProjection::incremented_byte_reciprocal16) && field.count == 1) ||
                (field.projection == NativeMaterialProjection::zero_register &&
                 field.count == 1 && (field.destination_shift == 0 || field.destination_shift == 5 ||
                                     field.destination_shift == 16) && field.width == 5) ||
                field.projection == NativeMaterialProjection::sum ||
                (field.projection == NativeMaterialProjection::pair_product_sum && field.count % 2 == 0) ||
                field.projection == NativeMaterialProjection::sum_low_byte_complement ||
                (field.projection == NativeMaterialProjection::power_of_two_remainder && field.count == 2) ||
                field.projection == NativeMaterialProjection::bit_indices32 ||
                field.projection == NativeMaterialProjection::equal_values ||
                (field.projection == NativeMaterialProjection::equal_power_of_two_left_shift32 &&
                 field.multiplier == 1 && field.addend == 0 && field.divisor_shift == 0 &&
                 field.destination_shift == 16 && field.width == 5) ||
                ((field.projection == NativeMaterialProjection::power_of_two_word_left_shift ||
                  field.projection == NativeMaterialProjection::power_of_two_long_left_shift ||
                  field.projection == NativeMaterialProjection::power_of_two_nonnegative_insert32 ||
                  field.projection == NativeMaterialProjection::power_of_two_halfword_left_shift) &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.destination_shift == 10 && field.width == 12) ||
                (field.projection == NativeMaterialProjection::power_of_two_plus_one_shift &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 6) ||
                ((field.projection == NativeMaterialProjection::difference_scale_word_shift ||
                  field.projection == NativeMaterialProjection::difference_scale_subtract_shift) &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 &&
                 field.width == (field.projection == NativeMaterialProjection::difference_scale_word_shift
                                     ? 12U : 6U)) ||
                (field.projection == NativeMaterialProjection::two_bit_scale_long_shift &&
                 field.count == 1 && field.multiplier == 1 && field.addend >= 0 && field.addend < 64 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 12) ||
                ((field.projection == NativeMaterialProjection::two_bit_scale_word_shift ||
                  field.projection == NativeMaterialProjection::two_bit_scale_add_shift) &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 &&
                 field.width == (field.projection == NativeMaterialProjection::two_bit_scale_word_shift
                                     ? 12U : 6U)) ||
                (field.projection == NativeMaterialProjection::byte_mask_add_logical64 &&
                 field.count == 2 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.destination_shift == 10 && field.width == 13) ||
                (field.projection == NativeMaterialProjection::palette_color_offset && field.count == 3) ||
                (field.projection == NativeMaterialProjection::byte_period_bias && field.count == 2) ||
                (field.projection == NativeMaterialProjection::byte_period_mask32 &&
                 field.count == 2 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.width == 13) ||
                ((field.projection == NativeMaterialProjection::composite_covered_mode ||
                  field.projection == NativeMaterialProjection::composite_uncovered_mode) && field.count == 2) ||
                (field.projection == NativeMaterialProjection::consecutive_first && field.count >= 2) ||
                ((field.projection == NativeMaterialProjection::color_math_layers ||
                  field.projection == NativeMaterialProjection::color_math_mode) && field.count % 3 == 0) ||
                (field.projection == NativeMaterialProjection::packed_bytes && field.count <= 4) ||
                ((field.projection == NativeMaterialProjection::complement_byte ||
                  field.projection == NativeMaterialProjection::complement_word ||
                  field.projection == NativeMaterialProjection::low_mask_width ||
                  field.projection == NativeMaterialProjection::unsigned_bit_width ||
                  field.projection == NativeMaterialProjection::power_of_two_fixed8_shift ||
                  field.projection == NativeMaterialProjection::power_of_two_fixed8_bias ||
                  field.projection == NativeMaterialProjection::power_of_two_shift ||
                  field.projection == NativeMaterialProjection::negative_fixed8_shift ||
                  field.projection == NativeMaterialProjection::negative_fixed8_rounding ||
                  field.projection == NativeMaterialProjection::unsigned_reciprocal_low16 ||
                  field.projection == NativeMaterialProjection::unsigned_reciprocal_high16 ||
                  field.projection == NativeMaterialProjection::unsigned_reciprocal_shift ||
                  field.projection == NativeMaterialProjection::unsigned_reciprocal_narrow_shift) && field.count == 1) ||
                (field.projection == NativeMaterialProjection::range_alignment_mask32 &&
                 field.count == 2 && field.multiplier == 1 &&
                 field.addend > 1 && field.addend < 32 &&
                 field.divisor_shift == 0 && field.width == 13) ||
                (field.projection == NativeMaterialProjection::domain_complement_mask32 &&
                 field.count == 1 && std::has_single_bit(field.multiplier) &&
                 field.addend > 0 && field.addend < 32 &&
                 static_cast<unsigned>(std::bit_width(field.multiplier)) - 1U <=
                     32U - static_cast<unsigned>(field.addend) &&
                 field.divisor_shift == 0 && field.width == 13) ||
                (field.projection == NativeMaterialProjection::pair_clear_bit32 &&
                 field.count == 2 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.width == 13) ||
                ((field.projection == NativeMaterialProjection::logical_mask32 ||
                  field.projection == NativeMaterialProjection::byte_bound_mask32 ||
                  field.projection == NativeMaterialProjection::word_bound_mask32 ||
                  field.projection == NativeMaterialProjection::period_mask32 ||
                  field.projection == NativeMaterialProjection::repeated_byte_logical_immediate) &&
                 field.count == 1 && field.multiplier == 1 && field.addend == 0 &&
                 field.divisor_shift == 0 && field.width == 13) ||
                (field.projection == NativeMaterialProjection::repeated_bit16 &&
                 field.multiplier == 1 && field.addend == 0 && field.divisor_shift == 0 &&
                 field.width == 13), "Invalid native field projection");
}
Digest digest_at(ByteView bytes, std::size_t at) {
    Digest out{};
    std::ranges::copy(slice(bytes, at, out.size()), out.begin());
    return out;
}
Bytes selected_material_source(ByteView input, const RomMaterialSpec& spec) {
    Bytes source;
    if (spec.sites.empty())
        append(source, slice(input, spec.offset, std::size_t{spec.source_count} * spec.source_width));
    else
        for (auto at : spec.sites) append(source, slice(input, at, spec.source_width));
    require(hex(sha256(source)) == spec.digest, "ROM material source mismatch");
    return source;
}
} // namespace
std::span<const RomMaterialSpec> rom_material_specs() { return material_specs; }
Digest rom_material_recipe() { return parse_digest(material_recipe_hash); }
Bytes widen_rom_material(ByteView input, unsigned source_width) {
    require((source_width == 1 || source_width == 2) && !input.empty() &&
                input.size() % source_width == 0 && input.size() <= 131072,
            "Invalid material input width/extent");
    Bytes out;
    out.reserve(input.size() / source_width * 4);
    for (std::size_t at = 0; at < input.size(); at += source_width)
        append32(out, source_width == 1 ? input[at] : read16(input, at));
    return out;
}
std::vector<Bytes> extract_rom_materials(const RomImage& rom) {
    require(rom.bytes.size() == rom_size && hex(sha256(rom.bytes)) == rom_hash,
            "Unvalidated ROM supplied to material extractor");
    std::vector<Bytes> out;
    std::map<std::uint32_t, Bytes> decoded;
    for (const auto& spec : material_specs) {
        ByteView input = rom.bytes;
        if (spec.container.size) {
            const auto& container = spec.container;
            const auto encoded = slice(rom.bytes, container.offset, container.size);
            require(hex(sha256(encoded)) == container.digest, "Material container input mismatch");
            auto found = decoded.find(container.offset);
            if (found == decoded.end())
                found = decoded.emplace(container.offset,
                    decompress_lzss(encoded, container.decoded_size)).first;
            require(found->second.size() == container.decoded_size &&
                        hex(sha256(found->second)) == container.decoded_digest,
                    "Material container output mismatch");
            // Mixed executable/data containers exist only during extraction.
            // Only the reviewed data projection is returned to the caller.
            input = found->second;
        }
        const auto source = selected_material_source(input, spec);
        auto native = derive_rom_material(source, spec.source_width, spec.native_width,
                                          spec.transform, spec.count, spec.groups);
        require(hex(sha256(native)) == spec.native_digest, "ROM material conversion mismatch");
        out.push_back(std::move(native));
    }
    for (const auto& alias : material_aliases) {
        require(alias.material < out.size() && alias.target < out.size() &&
                    alias.material != alias.target, "Invalid native material alias identity");
        validate_rom_material_alias(out[alias.material], out[alias.target], alias.first, alias.count);
    }
    return out;
}
void validate_rom_material_alias(ByteView value, ByteView target,
                                 std::uint32_t first, std::uint32_t count) {
    require(value.size() == 4 && target.size() % 4 == 0 &&
                first < target.size() / 4 && count > 0 &&
                count <= target.size() / 4 - first, "Invalid native material alias extent");
    const auto expected = read32(value, 0);
    for (std::size_t i = first; i < std::size_t{first} + count; ++i)
        require(read32(target, i * 4) == expected,
                "Independent ROM operands disagree at a pooled native field");
}
Bytes read_rom_material_source(const RomImage& rom, const RomMaterialSpec& spec) {
    require(rom.bytes.size() == rom_size && hex(sha256(rom.bytes)) == rom_hash,
            "Unvalidated ROM supplied to material source reader");
    if (!spec.container.size) return selected_material_source(rom.bytes, spec);
    const auto& container = spec.container;
    const auto encoded = slice(rom.bytes, container.offset, container.size);
    require(hex(sha256(encoded)) == container.digest, "Material container input mismatch");
    const auto decoded = decompress_lzss(encoded, container.decoded_size);
    require(decoded.size() == container.decoded_size && hex(sha256(decoded)) == container.decoded_digest,
            "Material container output mismatch");
    return selected_material_source(decoded, spec);
}
Bytes derive_rom_material(ByteView source, unsigned source_width, unsigned native_width,
                          std::string_view transform, unsigned output_count,
                          std::span<const std::uint32_t> groups) {
    require((native_width == 1 || native_width == 4) && output_count > 0 && output_count <= 65536,
            "Invalid native material width/count");
    Bytes wide;
    require(groups.empty() || transform == "fixed_fraction_term" || transform == "grouped_unsigned_sums" ||
                transform == "row_band_pattern" || transform == "arm64_row_band_fill" ||
                transform == "number_choice_values" || transform == "arm64_number_choice" ||
                transform == "grouped_equal_unsigned" || transform == "grouped_consecutive_base" ||
                transform == "reverse_range_indices" || transform == "offset_unsigned" ||
                transform == "arm64_bit_number_prefix" || transform == "arm64_palette_initialization" || transform == "arm64_flag_caption" || transform == "caption_choice_value" || transform == "arm64_caption_choice" ||
                transform == "arm64_inline_byte_append" || transform == "arm64_stack_byte_argument" ||
                transform == "arm64_tagged_byte_constant" || transform == "arm64_boolean_argument" || transform == "arm64_short_fill" || transform == "arm64_vector_fill" || transform == "arm64_vector_rows" ||
                transform == "banked_word_copy_value" || transform == "arm64_banked_word_copy" ||
                transform == "label_region_extent" || transform == "coalesced_text_region" || transform == "arm64_text_region_copy" ||
                transform == "arm64_text_region_clear" || transform == "arm64_label_region_clear" ||
                transform == "particle_pair_value" || transform == "arm64_particle_pairs" ||
                transform == "paired_tile_copy_layout" || transform == "arm64_paired_tile_copy" ||
                transform == "arm64_split_index_branch" ||
                transform == "arm64_selection_branch" || transform == "arm64_strided_row_fragment" ||
                transform == "condition_choice_value" || transform == "arm64_condition_choice" ||
                transform == "condition_cell_value" || transform == "arm64_cell_condition" ||
                transform == "arm64_coordinate_pair" || transform == "text_field_metric" || transform == "arm64_text_column_fragment" || transform == "text_field_columns" ||
                transform == "arm64_dimension_choice" ||
                transform == "byte_choice_value" || transform == "arm64_byte_choice" ||
                transform == "clamped_phase_value" || transform == "arm64_clamped_phase" ||
                transform == "particle_strip_layout" || transform == "arm64_particle_strip" ||
                transform == "palette_cycle_layout" || transform == "arm64_palette_cycle" ||
                transform == "arm64_gray_fill" || transform == "arm64_trail_update" ||
                transform == "arm64_trail_draw" || transform == "attribute_rectangle_words" ||
                transform == "scaled_le_values" || transform == "unsigned_bit_fields" ||
                transform == "equal_bit_field" || transform == "grouped_equal_bit_fields" ||
                transform == "grouped_consecutive_bit_field_base" || transform == "packed_bit_fields" ||
                transform == "wrapped_text_layout" || transform == "wrapped_scroll_schedule" || transform == "scaled_byte_division" || transform == "cell_run_origins" ||
                transform == "arm64_cell_apertures" ||
                transform == "byte_phase_span" || transform == "bit_transition_index" || transform == "record_loop_tail" || transform == "record_indices" ||
                transform == "scaled_record_indices" || transform == "scaled_record_sum" || transform == "indexed_record_pair" ||
                transform == "cell_coordinates" || transform == "cell_position_components" ||
                transform == "framed_rectangle_metric" || transform == "repeating_tile_pages" ||
                transform == "offset_span_duration" || transform == "delayed_linear_motion" ||
                transform == "compressed_text_layout" || transform == "compressed_row_layout" || transform == "tiled_transfer_schedule" || transform == "strided_row_layout" || transform == "scrolled_row_cells" ||
                transform == "scrolled_grid_cells" || transform == "scrolled_cell_coordinates" ||
                transform == "linear_coordinate_series" || transform == "affine_view_origin" || transform == "coordinate_combination" ||
                transform == "row_active_boundary" || transform == "scaled_endpoint_span" ||
                transform == "palette_group_index" || transform == "palette_pair_value" ||
                transform == "arm64_palette_boolean_value" ||
                transform == "arm64_palette_boolean_result" || transform == "arm64_palette_optional_search",
            "Unexpected material groups");
    if (transform == "fixed_fraction_term") {
        require((source_width == 1 || source_width == 2) && source.size() == source_width &&
                    native_width == 4 && output_count == 1 && groups.size() == 2 &&
                    groups[0] <= 16 && groups[1] <= 1,
                "Invalid fixed-fraction representation");
        const auto numerator = source_width == 1 ? std::uint32_t{source[0]} : read16(source,0);
        const auto denominator = std::uint32_t{1} << groups[0];
        const auto common = std::gcd(numerator,denominator);
        // Reduced terms preserve the existing native expression without baking
        // the selected gain or its reducible power-of-two factor into source.
        append32(wide, (groups[1] == 0 ? numerator : denominator) / common);
    } else if (transform == "compressed_text_layout") {
        require(source_width==1 && native_width==4 && groups.size()==9 &&
                    groups[2]<=source.size() && source.size()-groups[2]>=2 &&
                    (source.size()-groups[2])%2==0 && output_count==3+source.size()-groups[2],
                "Invalid compressed text layout");
        const auto layout=derive_rom_material(source.first(groups[2]),1,4,"compressed_row_layout",3,groups.first(2));
        const auto first=read32(layout,0), threshold=read32(layout,4), pitch=read32(layout,8);
        require(threshold*pitch<=first,"Negative compressed text intercept");
        const auto base=groups[3], bytes=groups[4], columns=groups[5], rows=groups[6];
        const auto page_stride=groups[7], pages=groups[8];
        require(base<=65535 && bytes>0 && bytes<=4 && columns>0 && columns<=256 &&
                    rows>0 && rows<=256 && page_stride>=bytes*columns*rows && page_stride<=65536 &&
                    pages>0 && pages<=256,"Invalid compressed text canvas");
        append32(wide,threshold);append32(wide,pitch);append32(wide,first-threshold*pitch);
        for(std::size_t at=groups[2];at<source.size();at+=2) {
            const auto address=std::uint32_t{read16(source,at)};
            require(address>=base,"Text cell precedes canvas");
            const auto page=(address-base)/page_stride, local=(address-base)%page_stride;
            require(page<pages && local<bytes*columns*rows && local%bytes==0,
                    "Text cell outside page or inside padding");
            const auto column=(local/bytes)%columns, row=page*rows+(local/bytes)/columns;
            const auto y=row<threshold?std::int64_t{row}*groups[0]-groups[1]
                                      :std::int64_t{row}*pitch+first-threshold*pitch;
            require(y>=0 && y<=UINT32_MAX,"Compressed text outside native canvas");
            append32(wide,column*groups[0]);append32(wide,static_cast<std::uint32_t>(y));
        }
    } else if (transform == "compressed_row_layout") {
        require(source_width==1 && native_width==4 && output_count==3 && groups.size()==2 &&
                    groups[0]>0 && groups[0]<=256 && groups[1]<groups[0] &&
                    source.size()>=10 && source.size()<=388 && source.size()%3==1 && source.back()==0,
                "Invalid compressed row layout");
        const unsigned first=source[0], period=source[3], step=read16(source,4);
        require(first>0 && first<128 && read16(source,1)==0 && period>0 && period<128 && step>0 &&
                    (first+groups[1])%groups[0]==0 && (period*groups[0])%(period+step)==0,
                "Inexact compressed row alignment");
        for (std::size_t at=3;at+1<source.size();at+=3)
            require(source[at]==period && read16(source,at+1)==(at/3)*step,
                    "Compressed row bands disagree");
        append32(wide,first);
        append32(wide,(first+groups[1])/groups[0]);
        append32(wide,period*groups[0]/(period+step));
    } else if (transform == "byte_choice_value" || transform == "arm64_byte_choice") {
        const bool native = transform == "arm64_byte_choice";
        require(source_width == 1 && native_width == (native ? 1U : 4U) &&
                    groups.size() == (native ? 8U : 2U) &&
                    (native ? (output_count == 8 || output_count == 20) : output_count == 1),
                "Invalid byte-choice representation");
        const auto choices = native_byte_choice_layout(source, groups[0]);
        if (native) {
            auto code = make_native_pointer_byte_choice(choices, groups.subspan(1));
            require(code.size() == output_count, "Byte-choice native extent differs");
            return code;
        }
        require(groups[1] < choices.size(), "Invalid byte-choice component");
        append32(wide, choices[groups[1]]);
    } else if (transform == "clamped_phase_value" || transform == "arm64_clamped_phase") {
        const bool native=transform=="arm64_clamped_phase";
        require(source_width==1 && native_width==(native?1U:4U) &&
                    groups.size()==(native?7U:3U) && output_count==(native?16U:1U),
                "Invalid clamped phase representation");
        const auto layout=native_clamped_phase_layout(source,groups[0],groups[1]);
        if (native) return make_native_clamped_phase(layout,groups.subspan(2));
        require(groups[2]<layout.size(),"Invalid clamped phase component");
        append32(wide,layout[groups[2]]);
    } else if (transform == "particle_strip_layout" || transform == "arm64_particle_strip") {
        const bool native=transform=="arm64_particle_strip";
        require(source_width==1 && native_width==(native?1U:4U) &&
                    groups.size()==(native?18U:5U) && (native || output_count==4),
                "Invalid particle strip representation");
        const auto layout=native_particle_strip_layout(source,groups.first(5));
        if (native) {
            require(groups[6]==groups[0],"Native particle stride differs from source format");
            wide=make_native_particle_strip(layout,groups.subspan(5));
            require(wide.size()==output_count,"Native particle strip extent mismatch");
            return wide;
        }
        for (const auto value : layout) append32(wide,value);
    } else if (transform == "palette_cycle_layout" || transform == "arm64_palette_cycle") {
        const bool native=transform=="arm64_palette_cycle";
        require(source_width==2 && native_width==(native?1U:4U) &&
                    groups.size()==(native?6U:2U) && (native || output_count==3),
                "Invalid palette cycle representation");
        if (native) {
            wide=make_native_palette_cycle(source,groups);
            require(wide.size()==output_count,"Native palette cycle extent mismatch");
            return wide;
        }
        for (const auto value : native_palette_cycle_layout(source,groups[0],groups[1]))
            append32(wide,value);
    } else if (transform == "equal_unsigned_fill_pattern") {
        require((source_width == 1 || source_width == 2) && native_width == 1 &&
                    !source.empty() && source.size() % source_width == 0 &&
                    source.size() / source_width <= 65536 && output_count % source_width == 0,
                "Invalid shared fill pattern extent");
        // Several selected operands can name one fill value. Authenticate every
        // alias before repeating its LE bytes to the native vector width. The
        // representation contains no selected value or original instruction.
        for (std::size_t at = source_width; at < source.size(); ++at)
            require(source[at] == source[at % source_width], "Shared fill operands disagree");
        Bytes result(output_count);
        for (std::size_t at = 0; at < result.size(); ++at)
            result[at] = source[at % source_width];
        return result;
    } else if (transform == "byte_phase_span") {
        // Each source record is a byte extent (LE16) and a byte phase stride.
        // The row record size is format metadata; the selected extent/stride
        // are supplied data. Independent writers must describe the same span.
        require(source_width == 1 && native_width == 4 && output_count == 2 &&
                    !source.empty() && source.size() <= 65535 && source.size() % 3 == 0 &&
                    groups.size() == 1 && groups[0] > 0 && groups[0] <= 256,
                "Invalid byte phase span representation");
        std::uint32_t common = 0;
        for (std::size_t at = 0; at < source.size(); at += 3) {
            const auto bytes = std::uint32_t{read16(source, at)};
            const auto step = std::uint32_t{source[at + 2]};
            require(bytes > 0 && bytes % groups[0] == 0 &&
                        bytes / groups[0] <= 256 && step > 0,
                    "Invalid phase extent or stride");
            const auto span = (bytes / groups[0] - 1U) * step;
            require(span <= 255 && (at == 0 || span == common),
                    "Phase span wraps or independent writers disagree");
            common = span;
        }
        append32(wide, common);
        append32(wide, 255U - common);
    } else if (transform == "linear_coordinate_series") {
        require(source_width == 1 && native_width == 4 && output_count == 3 &&
                    groups.size() == 1 && groups[0] > 0 && groups[0] <= 256 &&
                    source.size() >= groups[0] + 2 && source.size() <= groups[0] + 255,
                "Invalid linear coordinate series representation");
        const auto aliases = groups[0];
        const auto count = source.size() - aliases;
        // Independent callers state the row count; the complete coordinate
        // series supplies its origin and pitch. Preserve the one-past endpoint
        // too, because a native compiler may use it as the loop bound.
        for (unsigned i = 0; i < aliases; ++i)
            require(source[i] == count, "Coordinate series count aliases disagree");
        const auto origin = std::uint32_t{source[aliases]};
        require(source[aliases + 1] > origin, "Coordinate series must increase");
        const auto pitch = std::uint32_t{source[aliases + 1]} - origin;
        for (std::size_t i = 0; i < count; ++i)
            require(source[aliases + i] == origin + i * pitch,
                    "Coordinate series skips, wraps or changes pitch");
        // Byte inputs bound the endpoint below 511; never narrow it to a byte.
        append32(wide, origin);
        append32(wide, pitch);
        append32(wide, origin + static_cast<std::uint32_t>(count) * pitch);
    } else if (transform == "strided_clip_layout") {
        require(source_width == 1 && native_width == 4 && output_count == 6 &&
                    groups.empty() && source.size() >= 3 && source.size() <= 257,
                "Invalid strided clip layout");
        const std::uint32_t origin = source[0];
        require(source[1] > origin, "Clip origins must increase");
        const std::uint32_t pitch = source[1] - origin;
        for (std::size_t i = 1; i + 1 < source.size(); ++i)
            require(std::uint64_t{origin} + i * pitch == source[i],
                    "Clip origins are not uniformly spaced");
        const std::uint32_t height = std::uint32_t{source.back()} + 1U;
        const auto alignment = std::uint32_t{1} << std::countr_zero(pitch);
        require(origin < alignment, "Clip origin overlaps scaled slot bits");
        // Convert an inclusive last row to a half-open extent. The frozen
        // native loop splits its endpoint at the slot product's alignment;
        // derive both pieces instead of retaining a folded layout constant.
        const auto aligned_height = height & ~(alignment - 1U);
        for (const auto value : {origin, pitch, height, origin + height,
                                 aligned_height, origin + height - aligned_height})
            append32(wide, value);
    } else if (transform == "affine_coordinate_grid") {
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    output_count == 5 && source.size() % (2U * source_width) == 0 &&
                    source.size() / source_width >= 8 && source.size() / source_width <= 65536,
                "Invalid coordinate-grid representation");
        // Recover a rectangular row-major grid from its points. Every point,
        // including later rows, must agree before replacing the table with a
        // native origin/stride expression. No selected layout is stored here.
        const auto words = widen_rom_material(source, source_width);
        const auto points = words.size() / 8;
        const auto x0 = read32(words, 0), y0 = read32(words, 4);
        std::size_t columns = 1;
        while (columns < points && read32(words, columns * 8 + 4) == y0)
            ++columns;
        require(columns >= 2 && columns < points && points % columns == 0,
                "Coordinate grid needs complete rows and two axes");
        const auto next_x = read32(words, 8), next_y = read32(words, columns * 8 + 4);
        require(next_x > x0 && next_y > y0, "Coordinate grid strides must be positive");
        const auto dx = next_x - x0, dy = next_y - y0;
        for (std::size_t point = 0; point < points; ++point) {
            const auto x = std::uint64_t{x0} + (point % columns) * std::uint64_t{dx};
            const auto y = std::uint64_t{y0} + (point / columns) * std::uint64_t{dy};
            require(x == read32(words, point * 8) && y == read32(words, point * 8 + 4),
                    "Coordinate grid is not affine");
        }
        append32(wide, static_cast<std::uint32_t>(columns));
        append32(wide, x0);
        append32(wide, y0);
        append32(wide, dx);
        append32(wide, dy);
    } else if (transform == "scrolled_positions") {
        require(source_width == 2 && native_width == 4 && source.size() == std::size_t{output_count} * 4,
                "Invalid scrolled-position representation");
        // Source positions are unsigned words; scroll offsets are signed words.
        // Widen before subtraction, without signed shifts or narrowing casts.
        for (std::size_t at = 0; at < source.size(); at += 4) {
            const auto origin = std::int32_t{read16(source, at)};
            const auto encoded = std::int32_t{read16(source, at + 2)};
            const auto scroll = encoded < 32768 ? encoded : encoded - 65536;
            const auto position = origin - scroll;
            require(position >= 0, "Scrolled position precedes native canvas");
            append32(wide, static_cast<std::uint32_t>(position));
        }
    } else if (transform == "coordinate_combination") {
        require(source_width == 1 && native_width == 4 && output_count == 1 &&
                    !groups.empty() && groups.size() % 7 == 0 && groups.size() <= 256 * 7,
                "Invalid coordinate combination layout");
        // Each term describes a stored coordinate's format and sign. Origins
        // and cell/pixel units are metadata; the chosen positions come from ROM.
        // Signed wide accumulation allows subtraction without unsigned wrapping.
        std::size_t position = 0;
        std::int64_t total = 0;
        for (std::size_t i = 0; i < groups.size(); i += 7) {
            const auto width = groups[i], base = groups[i + 1], stride = groups[i + 2];
            const auto columns = groups[i + 3], axis = groups[i + 4];
            const auto scale = groups[i + 5], sign = groups[i + 6];
            require((width == 1 || width == 2) && base <= 65535 &&
                        stride > 0 && stride <= 256 && columns > 0 && columns <= 256 &&
                        axis <= 2 && scale > 0 && scale <= 65536 && (sign == 1 || sign == 2) &&
                        position <= source.size() && width <= source.size() - position,
                    "Invalid coordinate combination format");
            const std::uint32_t encoded = width == 1 ? source[position] : read16(source, position);
            require(encoded >= base && (encoded - base) % stride == 0,
                    "Coordinate outside its declared format");
            const auto index = (encoded - base) / stride;
            const auto coordinate = axis == 1 ? index % columns : axis == 2 ? index / columns : index;
            const auto term = std::int64_t{coordinate} * scale;
            total += sign == 1 ? term : -term;
            position += width;
        }
        require(position == source.size() && total >= 0 && total <= UINT32_MAX,
                "Coordinate combination exceeds output range or leaves input");
        append32(wide, static_cast<std::uint32_t>(total));
    } else if (transform == "affine_view_origin") {
        require(source_width == 1 && native_width == 4 && output_count == 3 &&
                    source.size() == 8 && groups.size() == 3,
                "Invalid affine origin representation");
        const auto unit = groups[0], quantum = groups[1], gain = groups[2];
        require(unit > 0 && unit <= 65536 && (unit & (unit - 1)) == 0 &&
                    quantum > 0 && quantum <= unit && (quantum & (quantum - 1)) == 0 &&
                    gain > 0 && gain <= 65536 && source[7] != 0,
                "Invalid affine sampling format");
        // Static coordinates and a unit coefficient define two axis-aligned
        // sample origins. The first center stores a high byte with a cleared
        // low byte; other coordinates are LE16. Gain describes arithmetic,
        // while the selected coefficient and coordinates are input data.
        const auto step = std::uint32_t{source[7]} * gain;
        const auto origin = [&](std::uint32_t center, std::uint32_t scroll) {
            const auto product = (std::int64_t{scroll} - center) * step;
            // Floor to the declared quantum without signed shifts or relying
            // on truncation toward zero for negative products.
            const auto remainder = product % quantum;
            const auto base = product - remainder - (remainder < 0 ? quantum : 0U);
            const auto value = std::int64_t{center} * unit + base;
            require(value >= 0 && value <= UINT32_MAX, "Affine origin outside native range");
            return static_cast<std::uint32_t>(value);
        };
        append32(wide, step);
        append32(wide, origin(std::uint32_t{source[0]} * 256U, read16(source,3)));
        append32(wide, origin(read16(source,1), read16(source,5)));
    } else if (transform == "scaled_endpoint_span") {
        require((source_width == 1 || source_width == 2) && native_width == 4 && output_count == 1 &&
                    source.size() == 2U * source_width && groups.size() == 1 &&
                    groups[0] > 0 && groups[0] <= 65536,
                "Invalid scaled endpoint span");
        const auto values = widen_rom_material(source,source_width);
        const auto begin = read32(values,0), end = read32(values,4);
        require(end >= begin && (end-begin) % groups[0] == 0,
                "Reversed or inexact endpoint span");
        append32(wide,(end-begin)/groups[0]);
    } else if (transform == "row_active_boundary") {
        require((source_width == 1 || source_width == 2) && native_width == 4 && output_count == 1 &&
                    groups.size() == 4 && groups[0] > 0 && groups[0] <= 256 &&
                    groups[1] < source_width*8U && groups[2] > 0 && groups[2] <= 256 &&
                    groups[3] <= 1 && source.size() % (groups[0]*source_width) == 0 &&
                    source.size() / (groups[0]*source_width) >= 1 &&
                    source.size() / (groups[0]*source_width) <= 256,
                "Invalid active-row boundary format");
        // The flag marks blank cells. Every row must describe the same single
        // nonempty run; glyph identities inside that run are separate content.
        // Reject holes or inconsistent rows rather than silently taking a bbox.
        const auto values = widen_rom_material(source,source_width);
        std::uint32_t first = 0, last = 0;
        for (std::size_t row = 0; row < values.size()/4; row += groups[0]) {
            auto begin = groups[0], end = groups[0];
            bool closed = false;
            for (unsigned col = 0; col < groups[0]; ++col) {
                const bool active = ((read32(values,(row+col)*4) >> groups[1]) & 1U) == 0;
                if (active) {
                    require(!closed,"Disjoint active cells in row");
                    if (begin == groups[0]) begin = col;
                    end = col+1;
                } else if (begin != groups[0]) closed = true;
            }
            require(begin != groups[0],"Empty active row");
            if (row == 0) { first = begin; last = end; }
            else require(begin == first && end == last,"Active row boundaries disagree");
        }
        append32(wide,(groups[3] == 0 ? first : last)*groups[2]);
    } else if (transform == "palette_group_index") {
        require(source_width == 1 && native_width == 4 && output_count == 1 &&
                    groups.size() == 8, "Invalid palette-group representation");
        const auto aliases = groups[0], shift = groups[1], mask = groups[2];
        const auto source_stride = groups[3], native_stride = groups[4];
        const auto colors = groups[5], count = groups[6], alias_policy = groups[7];
        require(aliases > 0 && aliases <= 256 && shift < 8 && mask > 0 &&
                    mask <= (255U >> shift) && source_stride > 0 && source_stride <= 256 &&
                    native_stride > 0 && native_stride <= 256 && colors > 0 &&
                    colors <= source_stride && colors <= native_stride && count > 0 &&
                    count <= 256 && alias_policy <= 1 && aliases < source.size() &&
                    (source.size() - aliases) % 2 == 0 &&
                    (source.size() - aliases) / 2 <= 65536,
                "Invalid palette-group format or extent");
        const auto selected = (source[0] >> shift) & mask;
        for (unsigned i = 1; i < aliases; ++i)
            require(((source[i] >> shift) & mask) == selected,
                    "Palette selection aliases disagree");
        const auto palette = source.subspan(aliases);
        const auto origin = selected * source_stride;
        require(origin + colors <= palette.size() / 2 &&
                    (count - 1) * native_stride + colors <= palette.size() / 2,
                "Palette group outside input");
        // The source bank and native group address the same ROM RGB555 colors
        // at different strides. Compare color tuples, not bank numbers. Policy
        // zero requires uniqueness; policy one explicitly selects the lowest
        // matching index when native palette groups are aliases. That policy is
        // a representation choice, not evidence of original layout accuracy.
        bool found = false;
        std::uint32_t result = 0;
        for (unsigned i = 0; i < count; ++i) {
            bool equal = true;
            for (unsigned color = 0; color < colors; ++color)
                equal &= (read16(palette, (origin + color) * 2) & 0x7fffU) ==
                         (read16(palette, (i * native_stride + color) * 2) & 0x7fffU);
            if (!equal) continue;
            require(!found || alias_policy == 1, "Ambiguous palette-group match");
            if (!found) result = i;
            found = true;
        }
        require(found, "Selected palette group has no native match");
        append32(wide, result);
    } else if (transform == "palette_pair_value" ||
               transform == "arm64_palette_boolean_value" ||
               transform == "arm64_palette_boolean_result" ||
               transform == "arm64_palette_optional_search") {
        const bool scalar = transform == "palette_pair_value";
        const bool call = transform == "arm64_palette_boolean_result";
        const bool value = transform == "arm64_palette_boolean_value";
        require(source_width == 1 && native_width == (scalar ? 4U : 1U) &&
                    groups.size() == (scalar ? 9U : value ? 10U : call ? 12U : 20U) &&
                    groups[0] > 0 && groups[0] <= 256 && groups[6] <= 8 &&
                    source.size() > 2U*groups[0] && (!scalar || (output_count == 1 && groups[8] < 2)),
                "Invalid paired palette representation");
        const auto aliases = groups[0];
        const auto palette = source.subspan(2U*aliases);
        Bytes choices;
        for (unsigned side = 0; side < 2; ++side) {
            const auto attributes = source.subspan(side*aliases, aliases);
            Bytes input(attributes.begin(), attributes.end());
            input.insert(input.end(), palette.begin(), palette.end());
            const auto value = derive_rom_material(input, 1, 4, "palette_group_index", 1,
                                                   groups.first(8));
            choices.push_back(static_cast<std::uint8_t>(read32(value,0)));
        }
        if (scalar) append32(wide, choices[groups[8]]);
        else {
            wide = value ? make_native_boolean_value(choices, groups.subspan(8))
                         : call ? make_native_boolean_result(choices, groups.subspan(8))
                                : make_native_optional_byte_search(choices, groups.subspan(8));
            require(wide.size() == output_count, "Paired palette native extent changed");
            return wide;
        }
    } else if (transform == "arm64_coordinate_pair") {
        require(source_width == 1 && native_width == 1,
                "Invalid coordinate-pair native representation");
        wide = make_native_coordinate_pair(source, groups);
        require(wide.size() == output_count, "Coordinate-pair native extent changed");
        return wide;
    } else if (transform == "text_field_metric" || transform == "arm64_text_column_fragment") {
        const bool native = transform == "arm64_text_column_fragment";
        require(source_width == 1 && native_width == (native ? 1U : 4U) &&
                    groups.size() == (native ? 10U : 4U) &&
                    (native || (output_count == 1 && groups[3] < 2)),
                "Invalid text-column metric representation");
        const auto decoded = derive_rom_material(source, 1, 4, "text_field_columns", 2,
                                                 groups.first(3));
        const auto first = read32(decoded, 0), second = read32(decoded, 4);
        require(second > first, "Text-column positions are not increasing");
        if (!native) append32(wide, groups[3] == 0 ? first : second - first);
        else {
            wide = make_native_text_column_fragment(second - first, groups[2], groups.subspan(3));
            require(wide.size() == output_count, "Text-column native extent changed");
            return wide;
        }
    } else if (transform == "text_field_columns") {
        require(source_width == 1 && native_width == 4 && !source.empty() && source.size() <= 256 &&
                    output_count > 0 && output_count <= 32 && groups.size() == 3 &&
                    groups[0] > 0 && groups[0] < 32 && groups[0] != 4 && groups[0] != 5 &&
                    groups[1] > 0 && groups[1] <= groups[2] && groups[2] <= 256,
                "Invalid text field column representation");
        // Decode the bounded data language's cursor movement, not CPU code.
        // A field's glyph width is an encoding rule; tabs and literal blanks
        // belong to the supplied text program. Never substitute fallback columns.
        unsigned column = 0, fields = 0;
        bool ended = false;
        for (std::size_t at = 0; at < source.size();) {
            const auto op = source[at++];
            if (op == 0) {
                require(at == source.size(), "Trailing text column data");
                ended = true;
            } else if (op >= 32) {
                ++column;
            } else {
                require(at < source.size(), "Truncated text column command");
                const auto value = source[at++];
                if (op == groups[0]) {
                    require(fields < output_count && column + groups[1] <= groups[2],
                            "Text field exceeds column bounds");
                    append32(wide, column);
                    ++fields;
                    column += groups[1];
                } else if (op == 5) column += value;
                else require(op == 4, "Unsupported text column command");
            }
            require(column <= groups[2], "Text cursor exceeds column bounds");
        }
        require(ended && fields == output_count, "Incomplete text field column program");
    } else if (transform == "condition_cell_value") {
        require(source_width == 1 && native_width == 4 && output_count == 1 &&
                    groups.size() == 6 && groups[5] < 3,
                "Invalid conditional cell value representation");
        append32(wide, native_condition_cell_values(source, groups.first(5))[groups[5]]);
    } else if (transform == "arm64_cell_condition") {
        require(source_width == 1 && native_width == 1,
                "Invalid conditional cell native representation");
        wide = make_native_cell_condition(source, groups);
        require(wide.size() == output_count, "Conditional cell native extent changed");
        return wide;
    } else if (transform == "unique_value_index") {
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    output_count == 1 && source.size() % source_width == 0 &&
                    source.size() / source_width >= 2 && source.size() / source_width <= 65536,
                "Invalid unique-index representation");
        // The first value selects an entry in the remaining data directory.
        // Ambiguous aliases cannot silently select the first matching record.
        const auto value_at = [&](std::size_t at) -> std::uint32_t {
            return source_width == 1 ? source[at] : read16(source, at * 2);
        };
        const auto selected = value_at(0);
        std::uint32_t index = 0;
        bool found = false;
        for (std::size_t i = 1; i < source.size() / source_width; ++i) {
            if (value_at(i) != selected) continue;
            require(!found, "Ambiguous selected directory value");
            index = static_cast<std::uint32_t>(i - 1);
            found = true;
        }
        require(found, "Selected directory value is absent");
        append32(wide, index);
    } else if (transform == "scrolled_grid_cells") {
        // Extend a static row with a selected byte stride and grid extent.
        // Reuse the row decoder for signed scroll and cursor alignment checks.
        require(source_width == 1 && native_width == 4 && groups.size() == 5 &&
                    source.size() >= 13 && source.size() <= 267,
                "Invalid scrolled-grid representation");
        const auto grid_rows = unsigned{source[10]}, grid_columns = unsigned{source[11]};
        require(grid_rows > 0 && grid_columns > 0 && source.size() == 12U + grid_columns &&
                    output_count == grid_rows * grid_columns,
                "Invalid scrolled-grid extent");
        Bytes row(source.begin(), source.begin() + 8);
        row.insert(row.end(), source.begin() + 12, source.end());
        const auto cells = derive_rom_material(row, 1, 4, "scrolled_row_cells", grid_columns, groups);
        const auto stride_bytes = unsigned{read16(source,8)};
        const auto row_bytes = groups[1] * groups[2];
        require(stride_bytes > 0 && stride_bytes % row_bytes == 0,
                "Invalid scrolled-grid row stride");
        const auto stride = stride_bytes / groups[1];
        for (unsigned y = 0; y < grid_rows; ++y)
            for (unsigned x = 0; x < grid_columns; ++x) {
                const auto cell = read32(cells, x * 4) + y * stride;
                require(cell < groups[2] * groups[3], "Scrolled grid outside native canvas");
                append32(wide, cell);
            }
    } else if (transform == "wrapped_scroll_schedule") {
        // A cell destination followed by initial scroll and cumulative changes.
        // The format describes adjacent tile pages, not a display program.
        require(source_width == 2 && native_width == 4 && output_count > 0 &&
                    output_count <= 64 && source.size() == (output_count + 1U) * 2 && groups.size() == 5,
                "Invalid wrapped scroll representation");
        const auto base = groups[0], columns = groups[1], rows = groups[2];
        const auto pixels = groups[3], pages = groups[4];
        require(base <= 65535 && columns > 0 && columns <= 256 && rows > 0 && rows <= 256 &&
                    pixels > 0 && pixels <= 256 && pages > 0 && pages <= 4 &&
                    columns * rows * pages <= 65536U - base,
                "Invalid wrapped scroll format");
        const auto address = read16(source, 0);
        require(address >= base && address - base < columns * rows * pages,
                "Wrapped scroll destination outside its pages");
        const auto wrap = rows * pixels;
        const auto origin = ((address - base) / columns % rows) * pixels;
        auto scroll = unsigned{read16(source, 2)} % wrap;
        for (unsigned i = 0; i < output_count; ++i) {
            if (i != 0) scroll = (scroll + read16(source, (i + 1U) * 2U)) % wrap;
            // Native row sampling includes the source convention's one-line
            // offset. Wrap explicitly; unsigned underflow is not modulo wrap.
            append32(wide, (origin + 2U * wrap - scroll - 1U) % wrap);
        }
    } else if (transform == "scrolled_cell_coordinates") {
        // Each selected pair is a tile-cell address and signed vertical offset.
        // Convert static text origins directly to native pixels at extraction;
        // no register program or display-device state is retained.
        require(source_width == 2 && native_width == 4 && output_count > 0 &&
                    output_count <= 65536 && output_count % 2 == 0 &&
                    source.size() == std::size_t{output_count} * 2 && groups.size() == 6,
                "Invalid scrolled-cell representation");
        const auto base = groups[0], bytes = groups[1], columns = groups[2];
        const auto rows = groups[3], pixels = groups[4], baseline = groups[5];
        require(base <= 65535 && bytes > 0 && bytes <= 4 && columns > 0 && columns <= 256 &&
                    rows > 0 && rows <= 256 && pixels > 0 && pixels <= 256 && baseline < pixels,
                "Invalid scrolled-cell format");
        for (std::size_t i = 0; i < output_count; i += 2) {
            const auto address = std::uint32_t{read16(source,i*2)};
            const auto encoded_scroll = read16(source,i*2+2);
            require(address >= base && (address-base) % bytes == 0 &&
                        (address-base) / bytes < columns * rows,
                    "Scrolled cell outside source canvas");
            const auto cell = (address-base) / bytes;
            const auto scroll = std::int32_t{encoded_scroll} - (encoded_scroll >= 32768 ? 65536 : 0);
            const auto y = static_cast<std::int32_t>((cell/columns)*pixels) -
                           static_cast<std::int32_t>(baseline) - scroll;
            require(y >= 0 && y < static_cast<std::int32_t>(rows*pixels),
                    "Scrolled cell outside native canvas");
            append32(wide,(cell%columns)*pixels);
            append32(wide,static_cast<std::uint32_t>(y));
        }
    } else if (transform == "scrolled_row_cells") {
        // Data contains a cell address, glyph byte displacement, two matching
        // signed scroll words and per-column cursor pixels. Convert the static
        // layout to native cell positions without a video register interface.
        require(source_width == 1 && native_width == 4 && groups.size() == 5 &&
                    output_count <= 256 && source.size() == 8U + output_count,
                "Invalid scrolled-row representation");
        const auto base = groups[0], bytes = groups[1], columns = groups[2];
        const auto rows = groups[3], pixels = groups[4];
        require(base <= 65535 && bytes > 0 && bytes <= 4 && columns > 0 && columns <= 256 &&
                    rows > 0 && rows <= 256 && pixels > 0 && pixels <= 256,
                "Invalid scrolled-row format");
        const auto address = std::uint32_t{read16(source,0)};
        const auto displacement = std::uint32_t{read16(source,2)};
        const auto encoded_scroll = read16(source,4);
        require(address >= base && (address - base) % bytes == 0 &&
                    (address - base) / bytes < columns * rows && displacement % bytes == 0 &&
                    encoded_scroll == read16(source,6),
                "Invalid scrolled-row address or aliases");
        const auto scroll = std::int32_t{encoded_scroll} - (encoded_scroll >= 32768 ? 65536 : 0);
        require(scroll % static_cast<std::int32_t>(pixels) == 0,
                "Scrolled row does not align with native cells");
        const auto origin = static_cast<std::int32_t>((address - base + displacement) / bytes) -
            scroll / static_cast<std::int32_t>(pixels) * static_cast<std::int32_t>(columns);
        require(origin >= 0 && origin < static_cast<std::int32_t>(columns * rows),
                "Scrolled row outside native canvas");
        const auto first = std::uint32_t{source[8]};
        for (unsigned i = 0; i < output_count; ++i) {
            const auto x = std::uint32_t{source[8 + i]};
            require(x >= first && (x - first) % pixels == 0 &&
                        (i == 0 || x > source[7 + i]) &&
                        static_cast<unsigned>(origin) % columns + (x - first) / pixels < columns,
                    "Invalid scrolled-row column spacing");
            append32(wide, static_cast<std::uint32_t>(origin) + (x - first) / pixels);
        }
    } else if (transform == "mosaic_envelope") {
        require(source_width == 1 && native_width == 4 && output_count == 3 &&
                    source.size() >= 4 && source.size() <= 32 &&
                    source[0] == source.size() - 1,
                "Invalid mosaic envelope extent");
        const auto length = source[0];
        const auto sample = [&](unsigned i) { return (unsigned{source[i + 1]} >> 4) + 1; };
        unsigned peak = 0;
        for (unsigned i = 1; i < length; ++i)
            if (sample(i) > sample(peak)) peak = i;
        require(peak > 0 && peak + 1 < length && sample(length - 1) == 1,
                "Unsupported mosaic envelope endpoints");
        const auto first = sample(0), top = sample(peak);
        // Unit-step ramps ending at one can use length - phase for the
        // descending half. This identity is proved from every selected sample.
        require(top + peak == length, "Mosaic descent does not reach its endpoint");
        for (unsigned i = 0; i < length; ++i) {
            const auto expected = i <= peak ? first + i : length - i;
            require(sample(i) == expected && (source[i + 1] & 15U) == (source[1] & 15U),
                    "Non-triangular or changing-layer mosaic envelope");
        }
        append32(wide, length);
        append32(wide, peak + 1);
        append32(wide, first);
    } else if (transform == "tiled_transfer_schedule") {
        // Decode static transfer records into native tile origins and publication
        // phases. This is build-time data conversion, not a video-device model.
        // Input: selected tile, source-buffer base, byte length, clock mask,
        // then equally sized source/destination word-address tables.
        require(source_width == 1 && native_width == 4 && groups.size() == 3 &&
                    source.size() >= 14 && (source.size() - 6) % 4 == 0 &&
                    output_count >= 5 && output_count % 2 == 1,
                "Invalid tiled-transfer representation");
        const auto entries = (source.size() - 6) / 4;
        const auto destination_base = groups[0], row_tiles = groups[1], tile_count = groups[2];
        const auto source_base = std::uint32_t{read16(source, 1)};
        const auto length = std::uint32_t{read16(source, 3)};
        require(entries <= 256 && std::has_single_bit(entries) && source[5] + 1U == entries &&
                    destination_base <= 65535 && row_tiles > 0 && row_tiles <= 256 &&
                    tile_count > 0 && tile_count <= 2048 && length >= 64 && length % 32 == 0 &&
                    source_base + tile_count * 32 <= 65536,
                "Invalid tiled-transfer format or extent");
        const auto phases = (output_count - 3) / 2;
        require(phases <= entries && std::has_single_bit(phases) && entries % phases == 0,
                "Invalid tiled-transfer phase count");
        const auto period = entries / phases;
        std::array<std::uint32_t, 2> first{};
        for (unsigned half = 0; half < 2; ++half) {
            const auto target = destination_base + (source[0] + half * row_tiles) * 16;
            require(target + 32 <= 65536, "Tiled-transfer destination overflows");
            unsigned found = 0;
            for (std::size_t row = 0; row < entries; ++row) {
                const auto src = std::uint32_t{read16(source, 6 + row * 2)};
                const auto dst = std::uint32_t{read16(source, 6 + entries * 2 + row * 2)};
                require(src >= source_base && (src - source_base) % 32 == 0 &&
                            src + length <= source_base + tile_count * 32 &&
                            dst % 16 == 0 && dst + length / 2 <= 65536,
                        "Tiled-transfer record outside declared buffers");
                if (target < dst || target + 32 > dst + length / 2) continue;
                if (found == 0) {
                    require(row < period, "Tiled-transfer first phase exceeds period");
                    first[half] = static_cast<std::uint32_t>(row);
                }
                require(found < phases && row == first[half] + found * period,
                        "Irregular tiled-transfer publication schedule");
                append32(wide, (src - source_base) / 32 + (target - dst) / 16);
                ++found;
            }
            require(found == phases, "Missing tiled-transfer image publication");
        }
        append32(wide, static_cast<std::uint32_t>(period));
        append32(wide, first[0]);
        append32(wide, first[1]);
    } else if (transform == "byte_power_geometry") {
        require(source_width == 1 && native_width == 4 && !source.empty() &&
                    source.size() <= 32768 && output_count == source.size() * 2,
                "Invalid byte-power geometry extent");
        // An optimizing compiler may materialize both negative extents and
        // division exponents. Derive the complete lookup from data, without
        // retaining any selected dimensions or a native instruction template.
        for (const auto value : source) {
            require(std::has_single_bit(std::uint32_t{value}),
                    "Byte-power geometry requires positive powers of two");
            append32(wide, 0U - std::uint32_t{value});
        }
        for (const auto value : source)
            append32(wide, static_cast<std::uint32_t>(std::countr_zero(std::uint32_t{value})));
    } else if (transform == "opposite_word_magnitude") {
        require(source_width == 2 && native_width == 4 && output_count == 1 && source.size() == 4,
                "Invalid opposite-word representation");
        const auto negative = std::uint32_t{read16(source, 0)};
        const auto positive = std::uint32_t{read16(source, 2)};
        require(positive > 0 && positive < 32768 && negative + positive == 65536,
                "Word displacements are not opposite signed magnitudes");
        append32(wide, positive);
    } else if (transform == "packed_bit_fields") {
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    !groups.empty() && groups.size() <= 96 && groups.size() % 3 == 0,
                "Invalid packed field representation");
        const auto arity = groups.size() / 3;
        require(source.size() == std::size_t{output_count} * arity * source_width,
                "Invalid packed field source extent");
        std::uint32_t assigned = 0;
        for (std::size_t field = 0; field < arity; ++field) {
            const auto shift = groups[field * 3], bits = groups[field * 3 + 1];
            const auto destination = groups[field * 3 + 2];
            require(bits > 0 && bits <= source_width * 8 &&
                        shift <= source_width * 8 - bits && destination <= 32 - bits,
                    "Invalid packed field bit range");
            const auto mask = ((std::uint32_t{1} << bits) - 1U) << destination;
            require((assigned & mask) == 0, "Overlapping packed fields");
            assigned |= mask;
        }
        // Layout triples describe encoding, never selected values. Each output
        // consumes a complete tuple, with unassigned destination bits cleared.
        for (unsigned row = 0; row < output_count; ++row) {
            std::uint32_t packed = 0;
            for (std::size_t field = 0; field < arity; ++field) {
                const auto at = (std::size_t{row} * arity + field) * source_width;
                const auto value = source_width == 1 ? std::uint32_t{source[at]}
                                                    : std::uint32_t{read16(source, at)};
                const auto shift = groups[field * 3], bits = groups[field * 3 + 1];
                const auto mask = (std::uint32_t{1} << bits) - 1U;
                packed |= ((value >> shift) & mask) << groups[field * 3 + 2];
            }
            append32(wide, packed);
        }
    } else if (transform == "rgb555_channel_values") {
        require(source_width == 1 && native_width == 4 && output_count == 1 &&
                    !source.empty() && source.size() <= 3,
                "Invalid RGB channel representation");
        unsigned assigned = 0, color = 0;
        for (const auto field : source) {
            const unsigned channels = field >> 5U;
            require(channels != 0 && (assigned & channels) == 0,
                    "Empty or overlapping RGB channel assignment");
            assigned |= channels;
            for (unsigned channel = 0; channel < 3; ++channel)
                if ((channels & (1U << channel)) != 0)
                    color |= unsigned(field & 31U) << (channel * 5U);
        }
        // Unselected channels contribute zero. This decodes static data fields;
        // it does not retain device state or execute register writes.
        append32(wide, color);
    } else if (transform == "delayed_linear_motion") {
        require(source_width == 2 && native_width == 4 && output_count == 5 &&
                    source.size() == 6 && groups.size() == 1 && groups[0] > 0 && groups[0] <= 65536,
                "Invalid delayed linear motion representation");
        const std::uint32_t delay = read16(source, 0), speed = read16(source, 2);
        const std::uint32_t origin = read16(source, 4);
        require(delay > 0 && (delay - 1) * groups[0] <= origin,
                "Invalid delayed linear motion origin");
        // Retain selected operands for native expressions, plus their folded
        // intercepts. The vertical step describes explicit source increments.
        // Word inputs and a bounded step keep both products within uint32.
        for (const auto value : {delay, speed, origin, (delay - 1) * speed,
                                 origin - (delay - 1) * groups[0]})
            append32(wide, value);
    } else if (transform == "position_cycle_offset") {
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    output_count == 2 && source.size() == 3 * source_width,
                "Invalid cyclic coordinate representation");
        const auto values = widen_rom_material(source, source_width);
        const auto position = read32(values, 0), origin = read32(values, 4);
        const auto period = read32(values, 8);
        require(period != 0, "Zero cyclic coordinate period");
        // Express a cycle whose zero lies at origin in canvas coordinates.
        // Keep the independent clipping position for callers that do not use
        // the cycle. All selected positions and the period come from input.
        append32(wide, position);
        append32(wide, (period - origin % period) % period);
    } else if (transform == "delayed_cycle_phase") {
        require(source_width == 2 && native_width == 4 && output_count == 1 && source.size() == 4,
                "Invalid delayed cycle representation");
        const std::uint32_t delay = read16(source, 0), mask = read16(source, 2);
        require(delay > 0 && mask > 0 && (mask & (mask + 1)) == 0,
                "Invalid delayed cycle period");
        append32(wide, (1U - delay) & mask);
    } else if (transform == "unit_acceleration_motion") {
        require(source_width == 2 && native_width == 4 && output_count == 4 && source.size() == 8,
                "Invalid accelerated motion representation");
        const auto origin = std::int64_t{read16(source, 0)};
        const auto encoded = std::int64_t{read16(source, 2)};
        const auto velocity = encoded < 32768 ? encoded : encoded - 65536;
        const auto horizontal = std::uint32_t{read16(source, 4)};
        const auto endpoint = std::int64_t{read16(source, 6)};
        require(origin < endpoint && velocity <= -2 && origin + velocity >= 0 && horizontal > 0,
                "Unsupported accelerated motion domain");
        // Sum unit acceleration independently of the optimizer. On this
        // bounded word domain, two complete word-counter ranges suffice even
        // for the most negative velocity. Widening also covers the lowest dip.
        std::uint32_t steps = 0;
        std::int64_t position = origin, speed = velocity;
        while (position < endpoint && steps < 131072) {
            position += speed++;
            ++steps;
        }
        require(position >= endpoint && steps > 0, "Unbounded accelerated motion");
        append32(wide, steps - 1);
        append32(wide, static_cast<std::uint32_t>(-velocity - 1));
        append32(wide, static_cast<std::uint32_t>(origin + velocity));
        append32(wide, horizontal - 1);
    } else if (transform == "byte_countdown_periods") {
        require(source_width == 1 && native_width == 4 && output_count == 3 &&
                    source.size() >= 2 && source.size() <= 65536 && source.size() % 2 == 0,
                "Invalid byte countdown representation");
        const std::uint32_t initial = source[0], reload = source[1];
        require(initial > 0 && reload > 0, "Countdown periods must be nonzero");
        for (std::size_t at = 2; at < source.size(); at += 2)
            require(source[at] == initial && source[at + 1] == reload,
                    "Shared countdown periods disagree");
        // One decrement happens on admission. An unsigned byte count of zero
        // requests 256 periods, with 255 reloads after the initial period.
        // Preserve that folded duration alongside the two selected periods.
        for (const auto value : {initial, reload, initial - 1U + 255U * reload})
            append32(wide, value);
    } else if (transform == "signed_word_divisor_parameters" ||
               transform == "signed_halfword_divisor_parameters") {
        const bool halfword = transform == "signed_halfword_divisor_parameters";
        require(source_width == 1 && native_width == 4 &&
                    output_count == (halfword ? 4U : 10U) && source.size() == 1,
                "Invalid signed-word divisor parameter shape");
        const auto divisor = std::uint32_t{source[0]};
        require(divisor > 2 && !std::has_single_bit(divisor),
                "Signed-word divisor needs the odd-factor form");
        const auto rotate = static_cast<unsigned>(std::countr_zero(divisor));
        const auto odd = divisor >> rotate;
        // Newton lifting computes the odd factor's inverse modulo 2^32.
        // Every truncation is explicit unsigned wrapping; these are numeric
        // compiler coefficients, never copied native or original instructions.
        std::uint32_t inverse = 1;
        for (unsigned step = 0; step < 5; ++step) {
            const auto product = static_cast<std::uint32_t>(std::uint64_t{odd} * inverse);
            const auto correction = std::uint32_t{2} - product;
            inverse = static_cast<std::uint32_t>(std::uint64_t{inverse} * correction);
        }
        require(static_cast<std::uint32_t>(std::uint64_t{odd} * inverse) == 1,
                "Odd divisor inverse did not converge");
        // Multiples span [-limit, limit]. Bias and rotate turn that signed
        // interval into one unsigned comparison after inverse multiplication.
        const auto limit = std::uint32_t{0x7fffffff} / divisor;
        const auto bias = limit << rotate;
        const auto bound = limit * 2U + 1U;
        const auto scale = std::uint64_t{1} << 32;
        const auto reciprocal = static_cast<std::uint32_t>((scale + divisor - 1U) / divisor);
        // This positive reciprocal covers signed 16-bit displacements plus
        // their negation. Error is <32769/2^32, below 1/255, the smallest
        // possible nonzero remainder fraction. Do not generalize this to all
        // int32 quotients: some divisors need a different compiler sequence.
        if (halfword) {
            // Narrowed callers use modulo 2^16 and the signed halfword bound.
            // Taking the high half of the 32-bit bias is not equivalent for
            // every divisor. Derive the actual interval at the caller's width.
            const auto half_limit = std::uint32_t{0x7fff} / divisor;
            for (const auto value : {inverse & 65535U, half_limit << rotate,
                                     half_limit * 2U + 1U, rotate})
                append32(wide, value);
        } else {
            for (const auto value : {divisor, inverse & 65535U, inverse >> 16,
                                     bias & 65535U, bias >> 16, bound & 65535U,
                                     bound >> 16, rotate, reciprocal & 65535U, reciprocal >> 16})
                append32(wide, value);
        }
    } else if (transform == "strided_row_layout") {
        require(source_width == 1 && native_width == 4 && output_count == 6 &&
                    groups.size() == 4 && groups[0] > 0 && groups[0] <= 256 &&
                    groups[1] > 0 && groups[1] <= 256 && groups[2] > 0 && groups[2] <= 256 &&
                    groups[3] >= 2 && groups[3] <= 256 &&
                    source.size() == 10U + groups[1] * 2U + groups[2] + groups[3],
                "Invalid strided-row representation");
        // Selected extent, columns and row destinations are data. The only
        // metadata is the byte size of one record. Derive row boundaries before
        // rendering; no original address space or transfer program is retained.
        const auto extent = std::uint32_t{read16(source, 0)};
        const auto columns = std::uint32_t{read16(source, 2)};
        const auto origin = std::uint32_t{read16(source, 4)};
        const auto row_bytes = columns * groups[0];
        require(extent > 0 && row_bytes > 0 && extent % row_bytes == 0 &&
                    extent <= 65536U - origin,
                "Invalid strided-row extent");
        append32(wide, extent / row_bytes);
        for (const unsigned offset : {6U, 8U}) {
            const auto address = std::uint32_t{read16(source, offset)};
            require(address >= origin && address - origin < extent &&
                        (address - origin) % row_bytes == 0,
                    "Row boundary outside selected extent or between rows");
            append32(wide, (address - origin) / row_bytes);
        }
        require(source[10] > source[11], "Invalid row-image step");
        const auto step = unsigned{source[10]} - source[11];
        const auto tail_offset = 10U + groups[1] * 2U;
        for (std::size_t offset = 12; offset < tail_offset; offset += 2)
            require(source[offset] >= source[offset + 1] &&
                        unsigned{source[offset]} - source[offset + 1] == step,
                    "Row-image step aliases disagree");
        append32(wide, step);
        const auto tail_image = unsigned{source[tail_offset]};
        for (unsigned i = 1; i < groups[2]; ++i)
            require(source[tail_offset + i] == tail_image, "Tail image aliases disagree");
        append32(wide, tail_image);
        const auto variant_offset = tail_offset + groups[2];
        const auto variant = unsigned{source[variant_offset]};
        for (unsigned i = 1; i < groups[3]; ++i)
            require(unsigned{source[variant_offset + i]} == variant + i,
                    "Row image variants skip or wrap");
        append32(wide, variant);
    } else if (transform == "offset_span_duration") {
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    output_count == 1 && source.size() == 3 * source_width && groups.size() == 1,
                "Invalid offset span representation");
        const auto step = groups[0];
        require(step > 0 && step <= 65536, "Invalid offset span step");
        const auto values = widen_rom_material(source, source_width);
        const auto delay = read32(values, 0), end = read32(values, 4), start = read32(values, 8);
        require(end >= start && (end - start) % step == 0, "Inexact or reversed offset span");
        // Endpoints and initial delay are selected data. The step describes
        // the source's explicit increments, not an additional content preset.
        // Unsigned byte/word inputs bound the sum at twice the word maximum.
        append32(wide, delay + (end - start) / step);
    } else if (transform == "unsigned_pair_product" || transform == "unsigned_pair_quotient" ||
               transform == "descending_ramp_iterations" ||
               transform == "scaled_span_duration" || transform == "linear_gradient_intercept") {
        const bool product = transform == "unsigned_pair_product";
        const bool quotient = transform == "unsigned_pair_quotient";
        const unsigned count = (product || quotient) ? 2U : 3U;
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    output_count == 1 && source.size() == count * source_width,
                "Invalid bounded operand arithmetic");
        const auto values = widen_rom_material(source, source_width);
        const auto first = read32(values, 0), second = read32(values, 4);
        if (product) {
            // Two unsigned source words fit one unsigned native word, including
            // the maximum pair. Widened operands avoid signed integer promotion.
            append32(wide, first * second);
        } else if (quotient) {
            // The endpoint and step are both ROM operands. Reject an incomplete
            // phase, zero divisor, or truncation instead of hiding a changed schedule.
            require(second != 0 && first % second == 0, "Inexact or zero-divisor operand quotient");
            append32(wide, first / second);
        } else if (transform == "linear_gradient_intercept") {
            const auto decrement = read32(values, 8);
            require(first != 0, "Gradient first sample must be one-based");
            // Convert a one-based sampled gradient to a zero-origin affine
            // expression. Unsigned byte/word operands bound the result below
            // UINT32_MAX, even at all three maximum values.
            append32(wide, second + (first - 1) * decrement);
        } else if (transform == "scaled_span_duration") {
            const auto period = read32(values, 8);
            require(first >= second && period != 0, "Invalid scaled span");
            // An elapsed schedule has one complete period per unit between
            // its source endpoints. Widened word operands bound the product
            // below UINT32_MAX; no authored duration or rate lives here.
            append32(wide, (first - second) * period);
        } else {
            const auto step = read32(values, 8);
            require(first >= second && step != 0, "Invalid descending ramp");
            // Count complete decrements before crossing the selected floor.
            // All three parameters are supplied data, not generator presets.
            append32(wide, (first - second) / step);
        }
    } else if (transform == "framed_rectangle_metric") {
        require(source_width == 1 && native_width == 4 && output_count == 1 &&
                    source.size() == 4 && groups.size() == 6,
                "Invalid framed-rectangle representation");
        const auto base = groups[0], bytes = groups[1], columns = groups[2];
        const auto rows = groups[3], pixels = groups[4], metric = groups[5];
        require(base <= 65535 && bytes > 0 && bytes <= 4 && columns > 0 && columns <= 256 &&
                    rows > 0 && rows <= 256 && pixels > 0 && pixels <= 256 && metric <= 1,
                "Invalid framed-rectangle format");
        const auto width = std::uint32_t{source[0]}, height = std::uint32_t{source[1]};
        const auto address = std::uint32_t{read16(source, 2)};
        require(address >= base && (address - base) % bytes == 0,
                "Unaligned framed-rectangle origin");
        const auto cell = (address - base) / bytes;
        const auto column = cell % columns, row = cell / columns;
        require(width >= 2 && height >= 2 && cell < columns * rows &&
                    width <= columns - column && height <= rows - row,
                "Framed rectangle outside canvas");
        // A one-cell frame surrounds the interior. The center and drawable
        // width are consequences of the selected rectangle, not new presets.
        append32(wide, metric == 0 ? (column * 2 + width) * pixels / 2
                                  : (width - 2) * pixels);
    } else if (transform == "cell_coordinates" || transform == "cell_position_components") {
        const bool components = transform == "cell_position_components";
        const auto arity = components ? 2U : 1U;
        require(source_width == 2 && native_width == 4 && groups.size() == (components ? 4U : 5U) &&
                    output_count % arity == 0 &&
                    source.size() == std::size_t{output_count / arity} * 2U,
                "Invalid cell-coordinate representation");
        const auto base = groups[0], bytes = groups[1], columns = groups[2];
        const auto rows = groups[3], axis = components ? 0U : groups[4];
        require(base <= 65535 && bytes > 0 && bytes <= 4 && columns > 0 && columns <= 256 &&
                    rows > 0 && rows <= 256 && axis <= 1,
                "Invalid cell-coordinate format");
        for (std::size_t i = 0; i < output_count / arity; ++i) {
            const auto address = std::uint32_t{read16(source, i * 2)};
            require(address >= base && (address - base) % bytes == 0 &&
                        (address - base) / bytes < columns * rows,
                    "Cell address outside declared canvas");
            const auto cell = (address - base) / bytes;
            // Component order is the native position format, not an authored table.
            append32(wide, axis == 0 ? cell % columns : cell / columns);
            if (components) append32(wide, cell / columns);
        }
    } else if (transform == "record_loop_tail") {
        require(source_width == 2 && native_width == 4 && source.size() == 4 &&
                    groups.size() == 4, "Invalid native record-loop representation");
        const auto origin = groups[0], stride = groups[1], capacity = groups[2], lanes = groups[3];
        require(origin <= 65535 && stride > 0 && stride <= 256 &&
                    capacity > 0 && capacity <= 65536 && lanes > 1 && lanes <= 16 &&
                    (lanes & (lanes-1U)) == 0, "Invalid native record-loop format");
        const unsigned extent = read16(source,0), address = read16(source,2);
        require(extent % stride == 0 && address >= origin && (address-origin) % stride == 0,
                "Unaligned record-loop extent or destination");
        const auto count = extent/stride, base = (address-origin)/stride;
        require(count >= lanes && count < capacity && base < capacity-count,
                "Record-loop destination outside declared rows");
        const auto tail = count % lanes, vector_count = count-tail;
        require(tail != 0 && output_count == 2U+2U*tail,
                "Record-loop tail differs from reviewed representation");
        // SIMD lane width describes the native format. Tail iteration identities
        // and destinations are computed from the selected extent, never retained
        // as an independent copy of the game's layout.
        append32(wide,count);
        append32(wide,base);
        for (unsigned i = 0; i < tail; ++i) append32(wide,vector_count+i);
        for (unsigned i = 0; i < tail; ++i) append32(wide,base+count-(vector_count+i));
    } else if (transform == "repeating_tile_pages") {
        require(source_width == 2 && native_width == 4 && output_count == 1 &&
                    groups.size() == 6 && groups[5] < 3 && !source.empty() && source.size() <= 65536 &&
                    source.size() % 4 == 0,
                "Invalid repeating tile-page representation");
        // Records pair source-page byte offsets with destination byte offsets.
        // Geometry is format metadata. The selected pages and repetition are
        // recovered from every record, never supplied as a fallback pattern.
        const auto page_bytes = groups[0], column_bytes = groups[1];
        const auto row_bytes = groups[2], columns = groups[3], height = groups[4];
        require(std::has_single_bit(page_bytes) && page_bytes <= 65536 &&
                    column_bytes > 0 && column_bytes <= 65536 &&
                    row_bytes > 0 && row_bytes <= 65536 &&
                    std::has_single_bit(columns) && columns <= 256 &&
                    std::has_single_bit(height) && height <= 256 &&
                    columns * column_bytes <= row_bytes &&
                    source.size() / 4 % columns == 0,
                "Invalid tile-page geometry");
        const auto rows = static_cast<std::uint32_t>(source.size() / 4 / columns);
        require(std::has_single_bit(rows) && rows >= 2 && rows <= 256 &&
                    (rows - 1U) * row_bytes + (columns - 1U) * column_bytes <= 65535,
                "Invalid tile-page grid extent");
        std::uint32_t period = columns;
        for (std::uint32_t column = 1; column < columns; ++column)
            if (read16(source, column * 4U) == 0) { period = column; break; }
        const auto stride = std::uint32_t{read16(source, columns * 4U)} / page_bytes;
        require(std::has_single_bit(period) && period >= 2 &&
                    std::has_single_bit(stride) && stride >= period && stride <= height,
                "Unsupported tile-page repetition");
        for (std::uint32_t row = 0; row < rows; ++row)
            for (std::uint32_t column = 0; column < columns; ++column) {
                const auto at = (row * columns + column) * 4U;
                require(read16(source, at) == (row * stride + column % period) * page_bytes &&
                            read16(source, at + 2U) == row * row_bytes + column * column_bytes,
                        "Tile-page records do not form a regular repeating grid");
            }
        // Compact native selector parameters. Reject irregular layouts instead
        // of generating a plausible image that differs from the ROM's records.
        const std::array<std::uint32_t,3> selectors{
            static_cast<std::uint32_t>(std::countr_zero(height) - std::countr_zero(stride)),
            (rows - 1U) * stride, static_cast<std::uint32_t>(std::countr_zero(period))};
        append32(wide, selectors[groups[5]]);
    } else if (transform == "indexed_record_pair") {
        require(source_width == 1 && native_width == 4 && output_count == 2 &&
                    source.size() == 3 && groups.size() == 3,
                "Invalid indexed record pair");
        const auto origin = groups[0], stride = groups[1], extent = groups[2];
        const auto address = std::uint32_t{read16(source, 0)};
        require(origin <= 65535 && stride > 0 && stride <= 256 &&
                    extent > 0 && extent <= 65536 && address >= origin &&
                    (address - origin) % stride == 0,
                "Invalid indexed record format/address");
        const auto base = (address - origin) / stride;
        // The word chooses a record bank; the byte selects a later record.
        // Both choices remain ROM data, including a compiler-folded sum.
        require(base < extent && source[2] < extent - base,
                "Indexed record exceeds its resource");
        append32(wide, base);
        append32(wide, base + source[2]);
    } else if (transform == "record_indices" || transform == "scaled_record_indices" ||
               transform == "scaled_record_sum") {
        const bool sum = transform == "scaled_record_sum";
        const auto fields = transform == "record_indices" ? 3U : 4U;
        const auto count = sum ? source.size() / 2 : std::size_t{output_count};
        require(source_width == 2 && native_width == 4 && output_count > 0 &&
                    output_count <= 65536 && count > 0 && count <= 65536 &&
                    (!sum || output_count == 1) && source.size() == count * 2U &&
                    groups.size() == count * fields,
                "Invalid record-index representation");
        // Each address/count uses a declared origin, record size and extent.
        // These describe the source format; the selected row remains ROM data.
        std::uint32_t total = 0;
        for (std::size_t i = 0; i < count; ++i) {
            const auto base = groups[i * fields], stride = groups[i * fields + 1];
            const auto extent = groups[i * fields + 2], address = std::uint32_t{read16(source, i * 2)};
            const auto scale = fields == 4 ? groups[i * fields + 3] : 1U;
            require(base <= 65535 && stride > 0 && stride <= 256 &&
                        extent > 0 && extent <= 65536 && address >= base &&
                        scale > 0 && scale <= 65536 &&
                        (address - base) % stride == 0 && (address - base) / stride < extent,
                    "Record index outside declared format");
            // Convert record counts to native units, such as cell rows to
            // pixels. The admitted word index and scale fit unsigned 32 bits.
            const auto value = ((address - base) / stride) * scale;
            if (sum) {
                require(value <= UINT32_MAX - total, "Scaled record sum overflows");
                total += value;
            } else {
                append32(wide, value);
            }
        }
        if (sum) append32(wide, total);
    } else if (transform == "arm64_cell_apertures") {
        require(source_width == 2 && native_width == 1 && groups.size() == 7 &&
                    groups[4] > 0 && groups[4] <= 32 && source.size() % 2 == 0 &&
                    groups[5] < source.size() / 2 &&
                    (source.size() / 2 - groups[5]) % groups[4] == 0,
                "Invalid native aperture representation");
        const auto count = static_cast<unsigned>((source.size() / 2 - groups[5]) / groups[4]);
        const auto origins = derive_rom_material(source, 2, 4, "cell_run_origins", count,
                                                groups.first(6));
        wide = make_native_cell_apertures(origins, groups[2], groups[3], groups[4],
                                         groups[5], groups[6]);
        require(wide.size() == output_count, "Native apertures no longer match reviewed extent");
        return wide;
    } else if (transform == "cell_run_origins") {
        require(source_width == 2 && native_width == 4 && groups.size() == 6 &&
                    output_count >= 2 && output_count <= 256,
                "Invalid cell-run representation");
        const auto base = groups[0], bytes = groups[1], columns = groups[2];
        const auto rows = groups[3], length = groups[4], run_rows = groups[5];
        require(base <= 65535 && bytes > 0 && bytes <= 4 && columns > 0 && columns <= 256 &&
                    rows > 0 && rows <= 256 && length > 0 && length <= columns &&
                    run_rows > 0 && run_rows <= rows &&
                    source.size() == 2U * (run_rows + output_count * length),
                "Invalid cell-run format/extent");
        const auto row_offset = read16(source,0);
        require(row_offset % (columns * bytes) == 0, "Unaligned cell-run row offset");
        for (unsigned row = 1; row < run_rows; ++row)
            require(read16(source,row * 2U) == row_offset + row * columns * bytes,
                    "Nonconsecutive cell-run row");
        std::uint32_t first = 0, stride = 0;
        for (unsigned run = 0; run < output_count; ++run) {
            const auto address = read16(source,2U * (run_rows + run));
            require(address >= base && (address - base) % bytes == 0,
                    "Invalid cell-run origin address");
            const auto cell = (address - base + row_offset) / bytes;
            require(cell < columns * rows && length <= columns - cell % columns &&
                        run_rows <= rows - cell / columns,
                    "Cell run leaves its canvas row");
            if (run == 0) first = cell;
            else if (run == 1) {
                require(cell >= first + length, "Overlapping or reversed cell runs");
                stride = cell - first;
            }
            require(cell == first + run * stride && cell / columns == first / columns,
                    "Cell-run origins are not one regular row");
            for (unsigned x = 1; x < length; ++x)
                require(read16(source,2U * (run_rows + x * output_count + run)) == address + x * bytes,
                        "Nonconsecutive cell-run address");
            // Convert selected absolute addresses to native cell indices. The
            // origin, stride and every authored address come from the ROM;
            // metadata only describes the buffer and interleaved record format.
            append32(wide,cell);
        }
    } else if (transform == "bit_transition_index") {
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    output_count == 1 && groups.size() == 1 && groups[0] < source_width * 8U &&
                    source.size() >= 2U * source_width && source.size() % source_width == 0 &&
                    source.size() / source_width <= 65536,
                "Invalid bit transition representation");
        const auto values = widen_rom_material(source,source_width);
        const auto count = values.size() / 4;
        std::size_t transition = count;
        for (std::size_t i = 0; i < count; ++i) {
            const bool set = ((read32(values,i * 4) >> groups[0]) & 1U) != 0;
            if (set && transition == count) transition = i;
            require(set || transition == count, "Bit transition reverses");
        }
        require(transition > 0 && transition < count, "Missing interior bit transition");
        append32(wide,static_cast<std::uint32_t>(transition));
    } else if (transform == "scaled_byte_division") {
        require(source_width == 2 && native_width == 4 && output_count == 3 &&
                    source.size() == 6 && groups.empty(),
                "Invalid scaled byte division representation");
        const auto divisor = read16(source, 0), reload = read16(source, 2);
        const auto scale = read16(source, 4);
        require(divisor >= 2 && divisor <= 255 && divisor == reload &&
                    scale > 0 && scale <= 256 && std::has_single_bit(scale),
                "Invalid scaled byte division operands");
        // For an explicitly byte-masked numerator, derive the smallest exact
        // reciprocal. Moving a power-of-two output scale before the final
        // shift requires removing the fractional low bits with a mask. Both
        // the shifted quotient and that mask depend on the extracted scale.
        unsigned shift = 0;
        std::uint32_t multiplier = 0;
        bool exact = false;
        for (; shift <= 16; ++shift) {
            multiplier = ((1U << shift) + divisor - 1U) / divisor;
            exact = true;
            for (unsigned value = 0; value < 256; ++value)
                exact = exact && ((value * multiplier) >> shift) == value / divisor;
            if (exact) break;
        }
        const auto scale_shift = static_cast<unsigned>(std::countr_zero(scale));
        require(exact && shift >= scale_shift, "Scaled byte quotient cannot use this representation");
        const auto width = std::bit_width(255U / divisor);
        const auto mask = ((1U << width) - 1U) * scale;
        append32(wide, multiplier);
        append32(wide, shift - scale_shift);
        append32(wide, mask);
    } else if (transform == "wrapped_text_layout") {
        require(source_width == 2 && native_width == 4 && source.size() == 14 &&
                    output_count == 7 && groups.size() == 5,
                "Invalid wrapped text layout representation");
        const auto base = groups[0], columns = groups[1], glyph = groups[2];
        const auto record_size = groups[3], wrap = groups[4];
        require(base <= 65535 && columns > 0 && columns <= 256 && glyph > 0 && glyph <= 256 &&
                    record_size > 0 && record_size <= 256 && wrap > 0 && wrap <= 256,
                "Invalid wrapped text layout format");
        const auto upper_record = read16(source,0), scroll = read16(source,2);
        const auto period = read16(source,4), reload = read16(source,6), step = read16(source,8);
        const auto lower_record = read16(source,10), address = read16(source,12);
        require(upper_record % record_size == 0 && lower_record % record_size == 0 &&
                    upper_record < lower_record && lower_record / record_size <= wrap &&
                    scroll < wrap && period > 0 && period == reload && step > 0 &&
                    period <= wrap && step <= wrap && address >= base,
                "Invalid wrapped text source geometry");
        const auto upper = upper_record / record_size, lower = lower_record / record_size;
        const auto source_start = (upper + scroll + 1U) % wrap;
        const auto cell = address - base;
        const auto row = (cell / columns) * glyph, x = (cell % columns) * glyph;
        require(row >= source_start && row < wrap && x < columns * glyph,
                "Text cell lies outside the wrapped canvas");
        // Invert a repeated visible-band/skipped-band layout. Parameters are
        // selected ROM operands; this is a bounded coordinate conversion,
        // not an interpreter for display writes or transfer instructions.
        const auto relative = row - source_start, block = relative / (period + step);
        const auto within = relative % (period + step);
        const auto y = upper + block * period + within;
        require(within + glyph <= period && y <= lower && glyph <= lower - y,
                "Text cell intersects a skipped or clipped band");
        for (const auto value : {lower, upper, std::uint32_t{period}, std::uint32_t{step},
                                 wrap - scroll - 1U, x, y}) append32(wide,value);
    } else if (transform == "grouped_equal_bit_fields" ||
               transform == "grouped_consecutive_bit_field_base") {
        const bool consecutive = transform == "grouped_consecutive_bit_field_base";
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    !source.empty() && source.size() % source_width == 0 &&
                    source.size() / source_width <= 65536 &&
                    (consecutive ? output_count == 1 && groups.size() >= 4 && groups.size() <= 65538
                                 : groups.size() == std::size_t{output_count} + 2),
                "Invalid shared bit-field groups");
        const auto shift = groups[0], width = groups[1], bits = source_width * 8U;
        require(shift < bits && width > 0 && width <= bits - shift,
                "Shared bit field exceeds its source width");
        const auto mask = (std::uint32_t{1} << width) - 1U;
        std::size_t position = 0;
        std::uint32_t base = 0;
        for (std::size_t group = 0; group < groups.size() - 2; ++group) {
            const auto count = groups[group + 2];
            require(count > 0 && count <= (source.size() - position) / source_width,
                    "Invalid shared bit-field group extent");
            std::uint32_t selected = 0;
            for (unsigned i = 0; i < count; ++i, position += source_width) {
                const auto raw = source_width == 1 ? std::uint32_t{source[position]}
                                                   : read16(source, position);
                const auto value = (raw >> shift) & mask;
                require(i == 0 || value == selected, "Shared bit-field group disagrees");
                selected = value;
            }
            if (consecutive) {
                // Folded alternatives can use one base and a native increment.
                // Validate every field and alias; do not invent a value table
                // or accept skipped/reversed/wrapped alternatives.
                if (group == 0) base = selected;
                require(std::uint64_t{selected} == std::uint64_t{base} + group,
                        "Consecutive bit-field choices disagree, skip or wrap");
            } else append32(wide, selected);
        }
        require(position == source.size(), "Unconsumed shared bit-field input");
        if (consecutive) append32(wide, base);
    } else if (transform == "unsigned_bit_fields" || transform == "equal_bit_field") {
        const bool shared = transform == "equal_bit_field";
        require((source_width == 1 || source_width == 2) && native_width == 4 &&
                    !source.empty() && source.size() % source_width == 0 &&
                    source.size() / source_width <= 65536 &&
                    (shared ? output_count == 1
                            : source.size() == std::size_t{output_count} * source_width) &&
                    groups.size() == std::size_t{output_count} * 2U,
                "Invalid unsigned bit-field layout");
        // Each pair describes the encoded field, not its selected value.
        // In particular, repeat/control flags are not part of a line count.
        const auto bits = source_width * 8U;
        for (std::size_t i = 0; i < source.size() / source_width; ++i) {
            const auto field = shared ? 0 : i;
            const auto shift = groups[field * 2], width = groups[field * 2 + 1];
            require(shift < bits && width > 0 && width <= bits - shift,
                    "Unsigned bit field exceeds its source width");
            const auto value = source_width == 1 ? std::uint32_t{source[i]} : read16(source, i * 2);
            const auto selected = (value >> shift) & ((std::uint32_t{1} << width) - 1U);
            if (!shared || i == 0) append32(wide,selected);
            else require(selected == read32(wide,0), "Shared encoded fields disagree");
        }
    } else if (transform == "scaled_le_values") {
        require(source_width == 1 && native_width == 4 && groups.size() == output_count * 2U,
                "Invalid scaled value layout");
        // Different operand widths can share a data recipe. Layout contains
        // only (LE width, native unit scale), never selected values. Scale is
        // for a documented native unit conversion, not a hidden content table.
        std::size_t position = 0;
        for (std::size_t i = 0; i < groups.size(); i += 2) {
            const auto width = groups[i], scale = groups[i + 1];
            require((width == 1 || width == 2) && scale > 0 && scale <= 65536 &&
                        width <= source.size() - position,
                    "Invalid scaled value extent/units");
            const std::uint32_t value = width == 1 ? source[position] : read16(source, position);
            // The admitted word/scale bounds fit uint32_t, including their
            // maximum product; no signed multiplication or narrowing occurs.
            append32(wide, value * scale);
            position += width;
        }
        require(position == source.size(), "Unconsumed scaled value bytes");
    } else if (transform == "doubling_series_base") {
        require(source_width == 2 && native_width == 4 && output_count == 1 &&
                    source.size() >= 4 && source.size() <= 32 && source.size() % 2 == 0,
                "Invalid doubling-series extent");
        // A compact native shift can replace a table only after every source
        // entry proves the series. Do not infer the rest from its first word.
        const std::uint32_t base = read16(source, 0);
        require(base != 0, "Doubling series has no positive base");
        for (std::size_t i = 0; i < source.size() / 2; ++i) {
            const auto expected = base << static_cast<unsigned>(i);
            require(expected <= 65535 && read16(source, i * 2) == expected,
                    "Doubling series disagrees or exceeds its word domain");
        }
        append32(wide, base);
    } else if (transform == "equal_unsigned") {
        require(native_width == 4 && output_count == 1,
                "Invalid shared material output extent");
        // Distinct authored sites can supply one shared native parameter.
        // Authenticate every site, then retain the agreed value rather than
        // inventing a repeated native table which the compiler never emitted.
        const auto values = widen_rom_material(source, source_width);
        const auto shared = read32(values, 0);
        for (std::size_t at = 4; at < values.size(); at += 4)
            require(read32(values, at) == shared, "Shared material operands disagree");
        append32(wide, shared);
    } else if (transform == "fade_scroll_schedule") {
        require(source_width == 1 && source.size() == 7 && native_width == 4 && output_count == 8,
                "Invalid fade/scroll schedule extent");
        // Project selected wait/count operands into the existing native elapsed
        // clock. Source order: fade period/count, scroll period, negative scroll
        // limit (LE16), negative cue checkpoint (LE16). No instructions or
        // authored durations are retained here. This arithmetic projection does
        // not clear the reconstructed transition's separate provenance review.
        const std::uint32_t fade_period = source[0], levels = source[1], scroll_period = source[2];
        const auto end = read16(source, 3), cue = read16(source, 5);
        require(fade_period != 0 && levels != 0 && scroll_period != 0 &&
                    end >= 0x8000U && (cue == 0 || cue >= end),
                "Invalid fade/scroll counter domain");
        const std::uint32_t distance = 0x10000U - end;
        // The cue is checked after waiting, before decrementing the counter.
        const std::uint32_t cue_step = ((0x10000U - cue) & 0xffffU) + 1;
        require(cue_step <= distance, "Fade/scroll cue follows the last step");
        const std::uint32_t fade = fade_period * levels, scroll = scroll_period * distance;
        // Byte periods and signed-word distances bound every result below 2^25.
        for (const auto value : {fade_period, scroll_period, distance, fade, scroll,
                                 fade + scroll, fade * 2 + scroll, fade + cue_step * scroll_period})
            append32(wide, value);
    } else if (transform == "zero_extend_bytes") {
        require(source_width == 1 && native_width == 1 && !source.empty() &&
                    source.size() <= output_count, "Invalid zero-extended byte representation");
        // A native serialized record may end in explicitly cleared fields.
        // Preserve selected operand bytes; zero is the generic clear value.
        Bytes out(output_count, 0);
        std::copy(source.begin(), source.end(), out.begin());
        return out;
    } else if (transform == "trail_layout") {
        require(source_width == 1 && native_width == 4 && output_count == 2,
                "Invalid native trail layout representation");
        for (const auto value : native_trail_layout(source)) append32(wide,value);
    } else if (transform == "arm64_trail_update" || transform == "arm64_trail_draw") {
        const bool draw = transform == "arm64_trail_draw";
        require(source_width == 1 && native_width == 1 && groups.size() == (draw ? 2 : 1),
                "Invalid native trail fragment representation");
        wide = draw ? make_native_trail_draw(source,groups[0],groups[1])
                    : make_native_trail_update(source,groups[0]);
        require(wide.size() == output_count, "Native trail fragment no longer matches reviewed extent");
        return wide;
    } else if (transform == "affine_copy_layout") {
        require(source_width == 1 && native_width == 4 && output_count == 7,
                "Invalid affine copy layout representation");
        for (const auto value : native_affine_copy_layout(source)) append32(wide, value);
    } else if (transform == "arm64_affine_copy") {
        require(source_width == 1 && native_width == 1, "Invalid affine copy native representation");
        wide = make_native_affine_copy(source);
        require(wide.size() == output_count, "Affine copy fragment no longer matches reviewed extent");
        return wide;
    } else if (transform == "tile_run_layout") {
        require(source_width == 1 && native_width == 4 && output_count == 8,
                "Invalid native tile layout representation");
        for (const auto value : native_tile_run_layout(source)) append32(wide, value);
    } else if (transform == "arm64_tile_runs") {
        require(source_width == 1 && native_width == 1, "Invalid native tile-run representation");
        wide = make_native_tile_runs(source);
        require(wide.size() == output_count, "Native tile runs no longer match their reviewed extent");
        return wide;
    } else if (transform == "number_choice_values") {
        require(source_width == 1 && native_width == 4 && groups.size() == 10 && groups[9] < 3,
                "Invalid numbered choice value representation");
        const auto layout = native_number_choice_layout(source, groups.first(9));
        if (groups[9] < 2) {
            require(output_count == 1, "Numbered palette must be one scalar");
            append32(wide, layout[groups[9]]);
        } else {
            require(output_count == layout.size() - 2, "Numbered position count differs");
            for (std::size_t i = 2; i < layout.size(); ++i) append32(wide, layout[i]);
        }
    } else if (transform == "arm64_number_choice") {
        require(source_width == 1 && native_width == 1,
                "Invalid native numbered choice representation");
        wide = make_native_number_choice(source, groups);
        require(wide.size() == output_count, "Native numbered choice changed reviewed extent");
        return wide;
    } else if (transform == "row_band_pattern" || transform == "arm64_row_band_fill") {
        require(source_width == 2 && native_width == (transform == "row_band_pattern" ? 4U : 1U),
                "Invalid row-band representation");
        wide = transform == "row_band_pattern" ? make_row_band_pattern(source,groups)
                                                : make_native_row_band_fill(source,groups);
        require(wide.size() == output_count*native_width, "Row-band output extent changed");
        return wide;
    } else if (transform == "arm64_palette_initialization") {
        require(source_width == 2 && native_width == 1 && output_count == 88,
                "Invalid native palette initialization representation");
        wide = make_native_palette_initialization(source, groups);
        require(wide.size() == output_count, "Native palette initialization changed reviewed extent");
        return wide;
    } else if (transform == "arm64_bit_number_prefix") {
        require(source_width == 1 && native_width == 1,
                "Invalid native bit-number prefix representation");
        wide = make_native_bit_number_prefix(source, groups);
        require(wide.size() == output_count, "Native bit-number prefix changed reviewed extent");
        return wide;
    } else if (transform == "arm64_flag_caption") {
        require(source_width == 1 && native_width == 1,
                "Invalid native flag caption representation");
        wide = make_native_flag_caption(source, groups);
        require(wide.size() == output_count, "Native flag caption changed reviewed extent");
        return wide;
    } else if (transform == "condition_choice_value") {
        require(source_width == 1 && native_width == 4 && output_count == 1 &&
                    groups.size() == 4 && groups[3] < 2,
                "Invalid condition choice value representation");
        const auto choices = native_condition_choices(source, groups[0], groups[1], groups[2]);
        append32(wide, choices[groups[3]]);
    } else if (transform == "arm64_dimension_choice") {
        require(source_width == 1 && native_width == 1,
                "Invalid native dimension choice representation");
        wide = make_native_dimension_choice(source, groups);
        require(wide.size() == output_count, "Native dimension choice changed reviewed extent");
        return wide;
    } else if (transform == "arm64_condition_choice") {
        require(source_width == 1 && native_width == 1,
                "Invalid native condition choice representation");
        wide = make_native_condition_choice(source, groups);
        require(wide.size() == output_count, "Native condition choice changed reviewed extent");
        return wide;
    } else if (transform == "caption_choice_value") {
        require(source_width == 1 && native_width == 4 && output_count == 1 && groups.size() == 4 && groups[3] < 3,
                "Invalid caption choice value representation");
        append32(wide, native_caption_choice_layout(source, groups[0], groups[1], groups[2])[groups[3]]);
    } else if (transform == "arm64_caption_choice") {
        require(source_width == 1 && native_width == 1 && groups.size() == 8 && groups[4] <= 1,
                "Invalid native caption choice representation");
        wide = make_native_caption_choice(source, groups[0], groups[1], groups[2],
                                          groups[3], groups[4] != 0, groups[5], groups[6], groups[7]);
        require(wide.size() == output_count, "Native caption choice changed reviewed extent");
        return wide;
    } else if (transform == "arm64_inline_byte_append" || transform == "arm64_stack_byte_argument") {
        const bool append = transform == "arm64_inline_byte_append";
        require(source_width == 1 && native_width == 1 && source.size() == 1 &&
                    (append ? groups.size() == 13U : (groups.size() == 10U || groups.size() == 12U)) && groups[1] >= 1 && groups[1] <= 8 &&
                    groups[0] <= 8 - groups[1], "Invalid native byte publication representation");
        const auto value = (unsigned{source[0]} >> groups[0]) & ((1U << groups[1]) - 1U);
        wide = append ? make_native_inline_byte_append(value, groups.subspan(2))
                      : make_native_stack_byte_argument(value, groups.subspan(2));
        require(wide.size() == output_count, "Native byte publication changed reviewed extent");
        return wide;
    } else if (transform == "arm64_tagged_byte_constant") {
        require(source_width == 1 && native_width == 1 && source.size() == 1 &&
                    output_count == 4 && groups.size() == 4 && groups[1] >= 1 && groups[1] <= 8 &&
                    groups[0] <= 8 - groups[1], "Invalid native tagged-byte representation");
        const auto value = (unsigned{source[0]} >> groups[0]) & ((1U << groups[1]) - 1U);
        return make_native_tagged_byte_constant(value, groups[3], groups[2]);
    } else if (transform == "arm64_boolean_argument") {
        require(source_width == 1 && native_width == 1 && groups.size() == 6,
                "Invalid native Boolean argument representation");
        wide = make_native_boolean_argument(source, groups[0], groups[1], groups[2],
                                           groups[3], groups[4], groups[5]);
        require(wide.size() == output_count, "Native Boolean argument changed reviewed extent");
        return wide;
    } else if (transform == "arm64_strided_row_fragment") {
        require(source_width == 1 && native_width == 1 && groups.size() == 10,
                "Invalid native strided-row representation");
        const auto decoded = derive_rom_material(source,1,4,"strided_row_layout",6,groups.first(4));
        std::array<std::uint32_t,6> layout{};
        for (unsigned i=0;i<layout.size();++i) layout[i]=read32(decoded,i*4);
        wide = make_native_strided_row_fragment(layout,groups.subspan(4),groups[3]);
        require(wide.size() == output_count,"Native strided-row extent mismatch");
        return wide;
    } else if (transform == "arm64_selection_branch") {
        require(source_width == 2 && native_width == 1 && output_count >= 4 && output_count <= 16 && output_count % 4 == 0,
                "Invalid native selection branch representation");
        wide = make_native_selection_branch(source,groups);
        require(wide.size() == output_count, "Native selection branch extent mismatch");
        return wide;
    } else if (transform == "arm64_split_index_branch") {
        require(source_width == 2 && native_width == 1 && output_count == 8,
                "Invalid split-index branch representation");
        wide = make_native_split_index_branch(source,groups);
        return wide;
    } else if (transform == "label_region_extent") {
        require(source_width==2 && native_width==4 && output_count==2,
                "Invalid label-region representation");
        for (auto value : native_label_region_layout(source,groups)) append32(wide,value);
    } else if (transform == "coalesced_text_region") {
        require(source_width==2 && native_width==4 && output_count==3,
                "Invalid text region representation");
        for (auto value : native_text_region_layout(source,groups)) append32(wide,value);
    } else if (transform == "arm64_text_region_copy" || transform == "arm64_text_region_clear" ||
               transform == "arm64_label_region_clear") {
        require(source_width==2 && native_width==1,"Invalid native text region representation");
        wide = transform == "arm64_text_region_copy" ? make_native_text_region_copy(source,groups) :
               transform == "arm64_text_region_clear" ? make_native_text_region_clear(source,groups) :
                                                        make_native_label_region_clear(source,groups);
        require(wide.size()==output_count,"Text region changed its reviewed extent");
        return wide;
    } else if (transform == "particle_pair_value" || transform == "arm64_particle_pairs") {
        const bool generated=transform=="arm64_particle_pairs";
        require(source_width==1 && native_width==(generated?1U:4U) && groups.size()==(generated?1U:2U) &&
                    (generated || (output_count==1 && groups[1]<2)), "Invalid paired-particle representation");
        if(generated) {
            wide=make_native_particle_pairs(source,groups[0]);
            require(wide.size()==output_count,"Paired particles changed reviewed extent");
            return wide;
        }
        append32(wide,native_particle_pair_layout(source,groups[0])[groups[1]]);
    } else if (transform == "paired_tile_copy_layout") {
        require(source_width==1 && native_width==4 && output_count==5 && groups.size()==2,
                "Invalid paired tile-copy layout representation");
        for (const auto value : native_paired_tile_copy_layout(source,groups[0],groups[1]))
            append32(wide,value);
    } else if (transform == "arm64_paired_tile_copy") {
        require(source_width==1 && native_width==1,"Invalid paired tile-copy native representation");
        wide=make_native_paired_tile_copy(source,groups);
        require(wide.size()==output_count,"Paired tile copy changed its reviewed extent");
        return wide;
    } else if (transform == "banked_word_copy_value") {
        require(source_width == 2 && native_width == 4 && output_count == 1 &&
                    groups.size() == 4 && groups[3] < 2, "Invalid word-copy layout projection");
        append32(wide,native_banked_word_copy_layout(source,groups.first(3))[groups[3]]);
    } else if (transform == "arm64_banked_word_copy") {
        require(source_width == 2 && native_width == 1, "Invalid native word-copy representation");
        wide = make_native_banked_word_copy(source,groups);
        require(wide.size() == output_count, "Native word copy changed reviewed extent");
        return wide;
    } else if (transform == "arm64_vector_rows") {
        require(source_width == 2 && native_width == 1,"Invalid vector-row native representation");
        wide = make_native_vector_rows(source,groups);
        require(wide.size() == output_count,"Vector rows changed their reviewed extent");
        return wide;
    } else if (transform == "arm64_vector_fill") {
        require(source_width == 2 && native_width == 1 && groups.size() == 4 && groups[3] <= 1,
                "Invalid vector-fill native representation");
        wide = make_native_vector_fill(source,groups[0],groups[1],groups[2],groups[3] != 0);
        require(wide.size() == output_count,"Vector fill changed its reviewed extent");
        return wide;
    } else if (transform == "arm64_short_fill") {
        require(source_width == 2 && native_width == 1 && output_count == 8 && groups.size() == 2,
                "Invalid short-fill native representation");
        return make_native_short_fill(source, groups[0], groups[1]);
    } else if (transform == "arm64_inline_gray_fill") {
        require(source_width == 1 && native_width == 1, "Invalid inline native fill projection");
        wide = make_native_inline_gray_fill(source);
        require(wide.size() == output_count, "Inline fill no longer matches its reviewed extent");
        return wide;
    } else if (transform == "arm64_gray_fill") {
        require(source_width == 1 && native_width == 1 && groups.size() == 3,
                "Invalid native gray-fill projection");
        wide = make_native_gray_fill(source, {groups[0], groups[1], groups[2]});
        require(wide.size() == output_count, "Native fill no longer matches its reviewed extent");
        return wide;
    } else if (transform == "attribute_rectangle_words") {
        require(source_width == 1 && source.size() == 9 && native_width == 4 && groups.size() == 5,
                "Invalid attribute rectangle representation");
        const auto origin = groups[0], columns = groups[1], rows = groups[2];
        const auto record_bytes = groups[3], first_word = groups[4];
        require(origin <= 65535 && columns > 0 && columns <= 256 && rows > 0 && rows <= 256 &&
                    record_bytes > 0 && record_bytes <= 4,
                "Invalid attribute rectangle format");
        const auto words = (columns * rows + 31U) / 32U;
        require(first_word < words && output_count <= words - first_word,
                "Attribute rectangle word slice exceeds layout");
        const auto width = unsigned{source[0]}, skip = unsigned{read16(source, 5)};
        const auto limit = unsigned{read16(source, 7)}, row_bytes = columns * record_bytes;
        require(width > 0 && width <= columns && width * record_bytes + skip == row_bytes &&
                    limit > 0 && limit % row_bytes == 0,
                "Invalid attribute rectangle row extent");
        const auto height = limit / row_bytes;
        std::vector<std::uint32_t> bits(words, 0);
        // Decode two attribute-address rectangles into native cell membership.
        // Addresses, row extents and width are ROM data; layout stride and
        // little-endian bit numbering are representation metadata. Validate
        // the entire layout even when the compiler consumes only a word slice.
        for (const auto at : {1U, 3U}) {
            const auto address = unsigned{read16(source, at)};
            require(address >= origin && (address - origin) % record_bytes == 0,
                    "Misaligned attribute rectangle address");
            const auto cell = (address - origin) / record_bytes;
            const auto x = cell % columns, y = cell / columns;
            require(y < rows && width <= columns - x && height <= rows - y,
                    "Attribute rectangle exceeds layout");
            for (unsigned dy = 0; dy < height; ++dy)
                for (unsigned dx = 0; dx < width; ++dx) {
                    const auto bit = (y + dy) * columns + x + dx;
                    bits[bit / 32] |= std::uint32_t{1} << (bit % 32);
                }
        }
        for (unsigned i = 0; i < output_count; ++i) append32(wide, bits[first_word + i]);
    } else if (transform == "menu_defaults") {
        require(source_width == 1 && source.size() == 2 && output_count == 3 &&
                    native_width == 4, "Invalid menu-default projection");
        // Decode the saved-options format, retaining the two speed fields in
        // separate native bytes. The default choices and skills cursor value
        // are ROM operands; this conversion contains only their bit layout.
        append32(wide, (source[0] & 7U) | ((source[0] & 112U) << 4U));
        append32(wide, (source[0] >> 3U) & 1U);
        append32(wide, source[1]);
    } else if (transform == "button_mapping") {
        require(source_width == 1 && source.size() == 4 && output_count == 8 &&
                    native_width == 4, "Invalid button mapping projection");
        // Packed mapping nibbles select Start, A/B/X/Y/L/R, then Select.
        // Native button bits have their own interface encoding. The default
        // choices are read from the ROM, never stored in this conversion.
        for (auto packed : source) {
            for (unsigned shift : {4U, 0U}) {
                const auto code = (packed >> shift) & 15U;
                require(code < 8, "Invalid mapped button identity");
                append32(wide, code == 0 ? 64U : code == 7 ? 128U : 1U << (code - 1U));
            }
        }
    } else if (transform == "span_fill_pattern_16") {
        require(source_width == 1 && source.size() == 2 && output_count == 4 &&
                    native_width == 4, "Invalid span fill pattern");
        // Decode inclusive byte endpoints into native half-open uint16 spans.
        // Widen before incrementing so the last byte coordinate becomes 256.
        // Repetition is the host fill-helper format, not an authored rectangle.
        const auto right = std::uint32_t{source[1]} + 1U;
        const auto pair = std::uint32_t{source[0]} | (right << 16U);
        for (unsigned i = 0; i < 4; ++i) append32(wide, pair);
    } else if (transform == "gray_fill_pattern_16") {
        require(source_width == 1 && source.size() == 1 && output_count == 4 &&
                    native_width == 4 && (source[0] & 224U) == 224U,
                "Invalid gray fill pattern");
        const auto color = std::uint32_t{source[0] & 31U} * 1057U;
        for (unsigned i = 0; i < 4; ++i) append32(wide, color | (color << 16U));
    } else if (transform == "fixed_color_lengths" || transform == "fixed_color_shades") {
        require(source_width == 1 && !source.empty() && source.size() % 2 == 0 &&
                    (output_count == source.size() / 2 || output_count == source.size() / 2 + 1),
                "Invalid fixed-color material extent");
        unsigned rows = 0;
        for (std::size_t at = 0; at < source.size(); at += 2) {
            const auto length = source[at], color = source[at + 1];
            require(length > 0 && length <= 127 && (color & 224U) == 224U &&
                        length <= 224U - rows,
                    "Invalid literal gray band");
            rows += length;
            append32(wide, transform == "fixed_color_lengths" ? length : color & 31U);
        }
        if (output_count == source.size() / 2 + 1) {
            require(rows < 224, "Fixed-color padding has no remaining rows");
            // The native 224-row canvas retains the final band after the
            // authored table ends. Padding is derived, never stored as content.
            append32(wide, transform == "fixed_color_lengths" ? 224U - rows : source.back() & 31U);
        }
    } else if (transform == "reverse_range_indices") {
        require(source_width == 2 && native_width == 4 && source.size() >= 4 &&
                    source.size() <= 131072 && source.size() % 2 == 0 &&
                    groups.size() == 1 && groups[0] > 0 && groups[0] <= 65536,
                "Invalid range partition input");
        // A producer writes fixed-width records into consecutive ranges. Each
        // consumer range selects a size in reverse order. Recover the per-record
        // choices from its boundaries, without embedding the authored grouping.
        // The stride describes the record format; boundaries are ROM operands.
        const auto endpoints = source.size() / 2;
        auto previous = read16(source, 0);
        for (std::size_t i = 1; i < endpoints; ++i) {
            const auto next = read16(source, i * 2);
            require(next > previous && (next - previous) % groups[0] == 0,
                    "Invalid or unaligned material range");
            const auto count = (next - previous) / groups[0];
            require(count <= output_count - wide.size() / 4,
                    "Material ranges exceed their output extent");
            for (unsigned record = 0; record < count; ++record)
                append32(wide, static_cast<std::uint32_t>(endpoints - i - 1));
            previous = next;
        }
    } else if (transform == "offset_unsigned") {
        require(groups.size() == 1, "Invalid material coordinate offset");
        const auto values = widen_rom_material(source, source_width);
        // Coordinates may use different origins in a resource and its native
        // consumer. Preserve the source values; metadata describes only the
        // origin conversion. Reject overflow before adding, never wrap it.
        for (std::size_t at = 0; at < values.size(); at += 4) {
            const auto value = read32(values, at);
            require(value <= UINT32_MAX - groups[0], "Material coordinate overflow");
            append32(wide, value + groups[0]);
        }
    } else if (transform == "pairwise_unsigned_differences" || transform == "equal_unsigned_differences") {
        const auto values = widen_rom_material(source, source_width);
        const bool shared = transform == "equal_unsigned_differences";
        require(shared ? native_width == 4 && output_count == 1 &&
                             !values.empty() && values.size() % 8U == 0
                       : values.size() == output_count * 8U,
                "Invalid material endpoint pairs");
        // Selected absolute deadlines and countdown checkpoints can share a
        // native elapsed-time representation. Each pair is (end, beginning).
        // The endpoints come from the ROM; no schedule or duration lives here.
        for (std::size_t at = 0; at < values.size(); at += 8) {
            const auto end = read32(values, at), beginning = read32(values, at + 4);
            require(end >= beginning, "Reversed material endpoints");
            if (!shared || wide.empty()) append32(wide, end - beginning);
            else require(read32(wide, 0) == end - beginning, "Unequal material endpoint differences");
        }
    } else if (transform == "grouped_consecutive_base") {
        require(native_width == 4 && output_count == 1 &&
                    groups.size() >= 2 && groups.size() <= 65536,
                "Invalid consecutive-base representation");
        const auto values = widen_rom_material(source, source_width);
        std::size_t position = 0;
        std::uint32_t base = 0;
        // A compiler may express consecutive choices as base plus selection.
        // Authenticate every repeated source operand and every later choice;
        // emit only the base, never an invented table or native instruction.
        for (std::size_t group = 0; group < groups.size(); ++group) {
            const auto count = groups[group];
            require(count > 0 && count <= (values.size() - position) / 4,
                    "Invalid consecutive-base group extent");
            if (group == 0) base = read32(values, position);
            const auto expected = std::uint64_t{base} + group;
            for (unsigned i = 0; i < count; ++i, position += 4)
                require(read32(values, position) == expected,
                        "Consecutive choices disagree, skip or wrap");
        }
        require(position == values.size(), "Unconsumed consecutive-base operands");
        append32(wide, base);
    } else if (transform == "grouped_unsigned_sums" || transform == "grouped_equal_unsigned") {
        require(groups.size() == output_count, "Material group count mismatch");
        const bool shared = transform == "grouped_equal_unsigned";
        require(!shared || native_width == 4, "Shared material groups require native words");
        const auto values = widen_rom_material(source, source_width);
        std::size_t position = 0;
        for (auto count : groups) {
            require(count > 0 && count <= (values.size() - position) / 4,
                    "Invalid material sum group");
            std::uint32_t total = 0;
            for (unsigned i = 0; i < count; ++i, position += 4) {
                const auto value = read32(values, position);
                if (shared) {
                    // Several authored sites can supply a single native phase.
                    // Check every site before emitting that shared value.
                    require(i == 0 || value == total, "Shared material group operands disagree");
                    total = value;
                } else {
                    require(value <= UINT32_MAX - total, "Material group sum overflow");
                    total += value;
                }
            }
            append32(wide, total);
        }
        require(position == values.size(), "Unconsumed material group input");
    } else if (transform == "unsigned_values" || transform == "signed_values" ||
        transform == "cumulative_unsigned" || transform == "color_channel_5" ||
        transform == "color_arithmetic_mode" || transform == "equal_color_arithmetic_mode" ||
        transform == "tile_index_6" || transform == "magnitude_7") {
        const bool shared_color = transform == "equal_color_arithmetic_mode";
        require(!shared_color || (native_width == 4 && output_count == 1 && !source.empty()),
                "Shared color arithmetic requires one native word");
        wide = widen_rom_material(source, source_width);
        if (transform != "unsigned_values") {
            Bytes converted;
            std::uint32_t total = 0;
            const unsigned bits = source_width * 8;
            for (std::size_t at = 0; at < wide.size(); at += 4) {
                auto value = read32(wide, at);
                if (transform == "signed_values") {
                    // Explicit two's-complement widening, without signed shifts
                    // or implementation-dependent unsigned-to-signed casts.
                    if (value & (std::uint32_t{1} << (bits - 1)))
                        value |= ~((std::uint32_t{1} << bits) - 1);
                } else if (transform == "cumulative_unsigned") {
                    require(value <= UINT32_MAX - total, "Material sum overflow");
                    total += value;
                    value = total;
                } else if (transform == "color_arithmetic_mode" || shared_color) {
                    require(source_width == 1, "Invalid color arithmetic field width");
                    // Decode only subtract/halve selection into ColorBlend.
                    // Layer-enable bits do not choose arithmetic; the consumer
                    // separately owns typed surface routing and coverage.
                    value = 1U + (value >> 7U) + 2U * ((value >> 6U) & 1U);
                } else {
                    require(source_width == 1, "Invalid encoded field width");
                    // A seven-bit magnitude can share its source byte with a
                    // direction flag. Only the field encoding lives here; the
                    // selected magnitudes remain supplied ROM values.
                    value &= transform == "color_channel_5" ? 31U
                             : transform == "tile_index_6" ? 63U : 127U;
                }
                if (!shared_color || converted.empty()) append32(converted, value);
                else require(read32(converted, 0) == value, "Shared color arithmetic differs");
            }
            wide = std::move(converted);
        }
    } else if (transform == "set_bit_indices") {
        require(source_width == 1 && source.size() == 4, "Invalid eligibility mask extent");
        const auto mask = read32(source, 0);
        for (unsigned bit = 0; bit < 32; ++bit)
            if ((mask & (std::uint32_t{1} << bit)) != 0) append32(wide, bit);
    } else {
        throw std::runtime_error("Unknown ROM material transformation");
    }
    require(wide.size() == output_count * 4U, "ROM material output count mismatch");
    if (native_width == 4) return wide;
    Bytes out;
    for (std::size_t at = 0; at < wide.size(); at += 4) {
        const auto value = read32(wide, at);
        require(value <= 255, "ROM material narrowing overflow");
        out.push_back(static_cast<std::uint8_t>(value));
    }
    return out;
}
std::uint32_t native_material_field(ByteView values, unsigned element_width,
                                    const NativeMaterialField& field) {
    require((element_width == 1 || element_width == 4) && values.size() % element_width == 0,
            "Invalid native field input width");
    validate_field(field, values.size() / element_width);
    if (field.projection == NativeMaterialProjection::scaled_quotient_shift ||
        field.projection == NativeMaterialProjection::scaled_quotient_mask32) {
        const auto get = [&](unsigned i) {
            const auto at = (field.first + i * field.source_stride) * element_width;
            return element_width == 1 ? std::uint32_t{values[at]} : read32(values, at);
        };
        const auto divisor = get(0), scale = get(1);
        require(divisor <= 65535 && scale <= 65535, "Scaled quotient operand exceeds a word");
        Bytes source;
        append16(source, static_cast<std::uint16_t>(divisor));
        append16(source, static_cast<std::uint16_t>(divisor));
        append16(source, static_cast<std::uint16_t>(scale));
        const auto layout = derive_rom_material(source, 2, 4, "scaled_byte_division", 3);
        if (field.projection == NativeMaterialProjection::scaled_quotient_shift)
            return read32(layout, 4) << 16U;
        auto mask = field;
        mask.first = 2; mask.count = 1; mask.source_stride = 1;
        mask.projection = NativeMaterialProjection::logical_mask32;
        return native_material_field(layout, 4, mask);
    }
    if (field.projection == NativeMaterialProjection::relative_membership_halfword) {
        const auto read = [&](std::size_t index) -> std::uint32_t {
            const auto at = (field.first + index * field.source_stride) * element_width;
            return element_width == 1 ? values[at] : read32(values, at);
        };
        auto origin = read(0), last = origin;
        for (std::size_t i = 1; i < field.count; ++i) {
            origin = std::min(origin, read(i));
            last = std::max(last, read(i));
        }
        require(last - origin < 64, "Native membership spans more than one long word");
        std::uint64_t mask = 0;
        for (std::size_t i = 0; i < field.count; ++i)
            mask |= std::uint64_t{1} << (read(i) - origin);
        // A compiler may store a relative set with several MOVZ/MOVK lanes.
        // The lane number is ISA layout metadata; no selected identity, origin
        // or bit pattern is retained. All subtraction and shifts are bounded.
        return static_cast<std::uint32_t>((mask >> (16U * static_cast<unsigned>(field.addend))) &
                                           65535U) << 5U;
    }
    if (field.projection == NativeMaterialProjection::adjacent_power_of_two_shift) {
        const auto at = std::size_t{field.first} * element_width;
        const auto next = at + std::size_t{field.count - 1U} * field.source_stride * element_width;
        const auto lower = element_width == 1 ? std::uint32_t{values[at]} : read32(values, at);
        const auto upper = element_width == 1 ? std::uint32_t{values[next]} : read32(values, next);
        for (std::uint32_t i = 1; i + 1 < field.count; ++i) {
            const auto copy = at + std::size_t{i} * field.source_stride * element_width;
            const auto value = element_width == 1 ? std::uint32_t{values[copy]} : read32(values, copy);
            require(value == lower, "Conditional shift base operands disagree");
        }
        require(std::has_single_bit(lower) && std::uint64_t{lower} * 2U == upper,
                "Native conditional shift requires ordered adjacent powers of two");
        // A conditional increment selects either exponent. Authenticate both
        // rates so the implicit increment cannot hide a retained source value.
        return static_cast<unsigned>(std::countr_zero(lower)) << 5U;
    }
    if (field.projection == NativeMaterialProjection::scaled_floor_bias ||
        field.projection == NativeMaterialProjection::simd_scaled_floor_bias) {
        std::uint32_t combined = 0;
        for (std::uint32_t i = 0; i < field.count; ++i) {
            const auto at = (std::size_t{field.first} + std::size_t{i} * field.source_stride) * element_width;
            const auto scale = element_width == 1 ? std::uint32_t{values[at]} : read32(values, at);
            require(scale > 0 && scale <= 255, "Invalid fixed-eight product scale");
            combined |= scale;
        }
        // Every possible product shares these low zero bits. The compiler can
        // remove them from the negative floor-division bias without changing
        // any signed input's quotient. Derive that bias, not its selected value.
        const auto alignment = std::uint32_t{1} << std::countr_zero(combined);
        const auto bias = 256U - alignment;
        if (field.projection == NativeMaterialProjection::scaled_floor_bias)
            return bias << 5U;
        return ((bias & 31U) | ((bias >> 5U) << 11U) | 32U) << 5U;
    }
    if (field.projection == NativeMaterialProjection::two_bit_scaled_right_shift) {
        const auto scale = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(std::popcount(scale) == 2, "Scaled extraction requires two set bits");
        const auto low = static_cast<unsigned>(std::countr_zero(scale));
        const auto fraction = static_cast<unsigned>(field.addend);
        require(low <= fraction, "Product alignment exceeds fractional width");
        // UBFX after the odd shift/add product replaces a left shift followed
        // by unsigned fixed-point division. A64's endpoints are inclusive.
        return (((fraction - low) << 6U) | (31U - low)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::byte_power_of_two_bit_select) {
        const auto period = element_width == 1 ? std::uint32_t{values[field.first]}
                                              : read32(values, std::size_t{field.first} * 4);
        require(period <= 128 && std::has_single_bit(period),
                "Native byte bit selection requires one byte power of two");
        const auto bit = static_cast<unsigned>(std::countr_zero(period));
        // (zero-extended byte / period) & 1 lowers to UBFM. For the top
        // source bit the compiler can omit masking and use the word-wide LSR
        // alias. These are native encoding rules, not original game choices.
        return ((bit << 6U) | (bit == 7U ? 31U : bit)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::nonnegative_byte_insert_width) {
        const auto bound = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(bound <= 127, "Native byte insertion lacks a nonnegative bound");
        // The reviewed lowering strips only the sign bit of a nonnegative
        // byte. Smaller bounds retain that width; larger bounds need a different
        // lowering. UBFIZ stores its inclusive source endpoint.
        return (8U - 1U - 1U) << 10U;
    }
    if (field.projection == NativeMaterialProjection::structure_load_opcode) {
        const auto count = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(count >= 1 && count <= 4, "Unsupported native interleaved load stride");
        // A64 AdvSIMD LD1/LD2/LD3/LD4 opcode fields, not game lookup data.
        constexpr std::array<unsigned, 4> opcodes{7, 8, 4, 0};
        return opcodes[count - 1U] << 12U;
    }
    if (field.projection == NativeMaterialProjection::corrected_reciprocal_low16 ||
        field.projection == NativeMaterialProjection::corrected_reciprocal_high16 ||
        field.projection == NativeMaterialProjection::corrected_reciprocal_shift) {
        const auto at = std::size_t{field.first} * element_width;
        const auto divisor = element_width == 1 ? std::uint32_t{values[at]} : read32(values, at);
        require(divisor >= 2 && divisor <= 256 && !std::has_single_bit(divisor),
                "Invalid corrected reciprocal divisor");
        const auto shift = static_cast<unsigned>(std::bit_width(divisor)) - 1U;
        const auto scale = std::uint64_t{1} << (33U + shift);
        const auto full = (scale + divisor - 1U) / divisor;
        require(full > (std::uint64_t{1} << 32U) && full < (std::uint64_t{1} << 33U),
                "Corrected reciprocal multiplier has no native representation");
        const auto multiplier = full - (std::uint64_t{1} << 32U);
        const auto quotient = [&](std::uint32_t n) {
            const auto high = (std::uint64_t{n} * multiplier) >> 32U;
            return (high + ((std::uint64_t{n} - high) >> 1U)) >> shift;
        };
        // A rounded-up reciprocal cannot underestimate. Its excess grows with
        // the quotient for each remainder, so these are the two worst endpoints:
        // the last complete quotient interval and the final partial interval.
        // The correction avoids a 65-bit product; every intermediate fits uint64.
        const auto last = UINT32_MAX / divisor;
        require(quotient(last * divisor - 1U) == last - 1U && quotient(UINT32_MAX) == last,
                "Corrected reciprocal is inexact over the uint32 domain");
        const auto value = field.projection == NativeMaterialProjection::corrected_reciprocal_shift
                               ? shift : field.projection == NativeMaterialProjection::corrected_reciprocal_low16
                               ? multiplier & 65535U : multiplier >> 16U;
        return static_cast<std::uint32_t>(value) << field.destination_shift;
    }
    if (field.projection == NativeMaterialProjection::word_bound_shift ||
        field.projection == NativeMaterialProjection::word_bound_units) {
        const auto at = std::size_t{field.first} * element_width;
        const auto bound = element_width == 1 ? std::uint32_t{values[at]} : read32(values, at);
        require(bound > 0 && bound <= 65535, "Invalid shifted word bound");
        // For an unsigned comparison, bits below the boundary's alignment
        // cannot change the result. Restore the shift and boundary in units
        // of that alignment, without retaining either as an authored literal.
        const auto shift = static_cast<unsigned>(std::countr_zero(bound));
        const auto value = field.projection == NativeMaterialProjection::word_bound_shift
                               ? shift : bound >> shift;
        require(value < (std::uint32_t{1} << field.width),
                "Shifted word bound exceeds its native field");
        return value << field.destination_shift;
    }
    if (field.projection == NativeMaterialProjection::simd_affine_byte ||
        field.projection == NativeMaterialProjection::simd_affine_halfword_byte ||
        field.projection == NativeMaterialProjection::simd_affine_halfword_clear_byte) {
        const auto at = std::size_t{field.first} * element_width;
        const auto source = element_width == 1 ? std::uint32_t{values[at]} : read32(values, at);
        const auto numerator = static_cast<std::int64_t>(std::uint64_t{source} * field.multiplier) + field.addend;
        const auto divisor = std::int64_t{1} << field.divisor_shift;
        require(numerator >= 0 && numerator % divisor == 0 && numerator / divisor <= 255,
                "Affine SIMD result is not exactly one byte");
        const auto value = static_cast<std::uint32_t>(numerator / divisor);
        // Arithmetic acts on the data before splitting the native immediate.
        const auto mode = field.projection == NativeMaterialProjection::simd_affine_halfword_byte
            ? 1056U : field.projection == NativeMaterialProjection::simd_affine_halfword_clear_byte ? 1184U : 32U;
        // MOVI word/halfword and BIC halfword use different fixed lane modes.
        return ((value & 31U) | ((value >> 5U) << 11U) | mode) << 5U;
    }
    if (field.projection == NativeMaterialProjection::byte_mask_add_logical64) {
        const auto read = [&](std::size_t index) -> std::uint32_t {
            const auto at = (field.first + index * field.source_stride) * element_width;
            return element_width == 1 ? values[at] : read32(values, at);
        };
        const auto mask = read(0), bias = read(1);
        require(mask <= 255 && bias > 0 && bias <= 255 && (mask ^ bias) == 255,
                "Native byte mask/add fold requires complementary byte inputs");
        const auto trailing = static_cast<unsigned>(std::countr_zero(bias));
        const auto run = bias >> trailing;
        require((run & (run+1U)) == 0,
                "Native byte bias has no 64-bit logical-immediate representation");
        // Complementary mask/add on a zero-extended byte folds to OR. Encode
        // its nonrepeated 64-bit run (N=1); retain neither selected operand.
        return ((1U << 12U) | (((64U-trailing) & 63U) << 6U) |
                (static_cast<unsigned>(std::bit_width(run))-1U)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::zero_register) {
        const auto source = element_width == 1 ? std::uint32_t{values[field.first]}
                                               : read32(values, std::size_t{field.first} * 4);
        const auto normalized = std::int64_t{source} * field.multiplier + field.addend;
        require(normalized == 0, "Native zero-register source is not exactly zero");
        // A64 register 31 supplies zero in reviewed Rt, Rn and Rm operands. Other
        // values need a different lowering and must not choose a live register.
        return 31U << field.destination_shift;
    }
    if (field.projection == NativeMaterialProjection::two_bit_scale_long_shift) {
        const auto scale = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(std::popcount(scale) == 2, "Native long scale requires two set bits");
        // Remove the common power of two from a shift/add product. The addend
        // is its declared ABI bit position, independent of the selected scale.
        const auto shift = static_cast<unsigned>(std::countr_zero(scale)) +
                           static_cast<unsigned>(field.addend);
        require(shift < 64, "Native packed scale exceeds long-shift width");
        return (((64U-shift) & 63U) << 6U | (63U-shift)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::two_bit_scale_word_shift ||
        field.projection == NativeMaterialProjection::two_bit_scale_add_shift) {
        const auto scale = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(std::popcount(scale) == 2, "Native two-bit scale requires exactly two set bits");
        const auto low = static_cast<unsigned>(std::countr_zero(scale));
        const auto high = static_cast<unsigned>(std::bit_width(scale)) - 1U;
        // Factor = (1 + 2^(high-low)) * 2^low. Only the native compiler's
        // two data shifts are restored; neither selected exponent is retained.
        if (field.projection == NativeMaterialProjection::two_bit_scale_add_shift)
            return (high-low) << 10U;
        return (((32U-low) & 31U) << 6U | (31U-low)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::difference_scale_word_shift ||
        field.projection == NativeMaterialProjection::difference_scale_subtract_shift) {
        const auto scale = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(scale > 0 && scale < 0x80000000U,
                "Difference scale exceeds native word-shift domain");
        const auto low = static_cast<unsigned>(std::countr_zero(scale));
        const auto run = scale >> low;
        require(run >= 3 && std::has_single_bit(run + 1U),
                "Difference scale requires one contiguous run of at least two bits");
        // A contiguous set-bit run is 2^high - 2^low. The existing native
        // compiler represents multiplication with LSL followed by shifted SUB.
        // Derive both selected shifts; neither an exponent nor instruction is
        // retained as game data. Restrict high < 32 to avoid a word-size shift.
        const auto high = static_cast<unsigned>(std::bit_width(scale));
        if (field.projection == NativeMaterialProjection::difference_scale_subtract_shift)
            return low << 10U;
        return (((32U - high) << 6U) | (31U - high)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::power_of_two_plus_one_shift) {
        const auto scale = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(scale >= 2 && std::has_single_bit(scale-1U),
                "Native add-shift scale requires a power of two plus one");
        // x + (x << shift) is a compiler representation of unsigned multiply.
        // Restore the selected factor's shift, not a stored native operand.
        return static_cast<unsigned>(std::countr_zero(scale-1U)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::power_of_two_word_left_shift ||
        field.projection == NativeMaterialProjection::power_of_two_long_left_shift ||
        field.projection == NativeMaterialProjection::power_of_two_nonnegative_insert32 ||
        field.projection == NativeMaterialProjection::power_of_two_halfword_left_shift) {
        const auto scale = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4);
        require(std::has_single_bit(scale) && scale >= (1U << field.divisor_shift),
                "Native word left shift requires an exactly divisible power-of-two scale");
        // An index division can fold into the selected scale. Validate exact
        // divisibility before restoring either native shift operand.
        const auto exponent = static_cast<unsigned>(std::countr_zero(scale)) - field.divisor_shift;
        // A word LSL encodes both rotation and mask length in UBFM. Remove and
        // restore the complete pair; either alone exposes the selected scale.
        const auto bits = field.projection == NativeMaterialProjection::power_of_two_long_left_shift ? 64U : 32U;
        const unsigned preserved_high =
            field.projection == NativeMaterialProjection::power_of_two_halfword_left_shift ? 16U :
            field.projection == NativeMaterialProjection::power_of_two_nonnegative_insert32 ? 1U : 0U;
        require(exponent + preserved_high < bits, "Native insert has no retained destination bits");
        // A nonnegative index insert preserves the destination sign and its
        // low origin bits; a narrowed product keeps only the low halfword.
        // Remove both rotation and length, not just rotation.
        return ((((bits - exponent) & (bits - 1U)) << 6U) |
                (bits - 1U - preserved_high - exponent)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::equal_power_of_two_left_shift32) {
        const auto read = [&](std::size_t index) {
            return element_width == 1 ? std::uint32_t{values[index]} : read32(values, index * 4);
        };
        const auto scale = read(field.first);
        require(std::has_single_bit(scale), "Native left shift requires a power-of-two scale");
        for (std::size_t i = 1; i < field.count; ++i)
            require(read(field.first + i * field.source_stride) == scale,
                    "Native left shift has unequal source steps");
        // A word UBFM left shift encodes the opposite rotation modulo 32.
        // Equal consecutive source differences justify folding a whole table
        // into one scale. No table entry or selected exponent lives in the map.
        return ((32U - static_cast<unsigned>(std::countr_zero(scale))) & 31U) << 16U;
    }
    const bool period_pair = field.projection == NativeMaterialProjection::byte_period_bias ||
                             field.projection == NativeMaterialProjection::byte_period_mask32;
    std::uint32_t period_bias = 0, period_mask = 0;
    if (period_pair) {
        const auto item = [&](std::size_t index) -> std::uint32_t {
            const auto at = (field.first + index * field.source_stride) * element_width;
            return element_width == 1 ? values[at] : read32(values, at);
        };
        const auto first = item(0), second = item(1);
        require(first > 0 && second > 0 && first <= 255 && second <= 255 && first != second &&
                    (first & 1U) && (second & 1U),
                "Native period rounding requires two distinct odd byte rates");
        // The optimized native countdown rounds to a bounding power-of-two
        // interval and clears invariant low bits. Derive both operands from
        // the selected rates; 256 is the declared sound-time quantum. Odd
        // rates leave its boundary bit clear for the final native OR.
        const auto upper = std::uint32_t{1} << std::bit_width(std::max(first, second));
        const auto changes = (first - 1U) | (second - 1U);
        const auto alignment = changes & (0U - changes);
        period_bias = (upper - 1U) * 256U;
        period_mask = (upper - alignment) * 256U;
    }
    if (field.projection == NativeMaterialProjection::range_alignment_mask32 ||
        field.projection == NativeMaterialProjection::domain_complement_mask32 ||
        field.projection == NativeMaterialProjection::logical_mask32 ||
        field.projection == NativeMaterialProjection::byte_bound_mask32 ||
        field.projection == NativeMaterialProjection::word_bound_mask32 ||
        field.projection == NativeMaterialProjection::period_mask32 ||
        field.projection == NativeMaterialProjection::byte_period_mask32 ||
        field.projection == NativeMaterialProjection::repeated_byte_logical_immediate) {
        const auto at = std::size_t{field.first} * element_width;
        auto wanted = element_width == 1 ? values[at] : read32(values, at);
        if (field.projection == NativeMaterialProjection::range_alignment_mask32) {
            const auto lower_at = (std::size_t{field.first} + field.source_stride) * element_width;
            const auto lower = element_width == 1 ? std::uint32_t{values[lower_at]} : read32(values, lower_at);
            const auto domain = (std::uint32_t{1} << static_cast<unsigned>(field.addend)) - 1U;
            require(lower < wanted && wanted <= domain,
                    "Native range requires ordered endpoints within its unsigned domain");
            // Dropping low bits preserves membership only when both bounds
            // have that alignment. Domain width belongs to the input format.
            const auto combined = wanted | lower;
            const auto alignment = combined & (0U - combined);
            wanted = domain & ~(alignment - 1U);
        }
        if (field.projection == NativeMaterialProjection::domain_complement_mask32) {
            const auto domain = (std::uint32_t{1} << static_cast<unsigned>(field.addend)) - 1U;
            require(wanted <= domain && std::has_single_bit(wanted),
                    "Native domain mask requires one selected bit within its format");
            // Equality with zero or one selected bit can test the other bits.
            // Domain width and scale describe the input format, not game data.
            wanted = (domain ^ wanted) * field.multiplier;
        }
        if (field.projection == NativeMaterialProjection::byte_period_mask32)
            wanted = period_mask;
        if (field.projection == NativeMaterialProjection::period_mask32) {
            // Elapsed modulo by a power-of-two interval folds to AND period-1.
            // Only the data operand is restored; the native algorithm stays fixed.
            require(wanted > 1 && std::has_single_bit(wanted),
                    "Native period mask requires a power-of-two period");
            --wanted;
        }
        if (field.projection == NativeMaterialProjection::byte_bound_mask32 ||
            field.projection == NativeMaterialProjection::word_bound_mask32) {
            const std::uint32_t domain = field.projection == NativeMaterialProjection::byte_bound_mask32
                                           ? 255U : 65535U;
            require(wanted > 0 && wanted <= domain, "Native comparison has an invalid bound");
            // Low bits below the bound's alignment cannot affect comparison
            // in this unsigned domain. Restore the mask from the ROM bound.
            const auto alignment = wanted & (0U - wanted);
            wanted = domain & ~(alignment - 1U);
        }
        if (field.projection == NativeMaterialProjection::repeated_byte_logical_immediate) {
            require(wanted <= 255, "Native repeated value is not a byte");
            // A repeated byte has identical logical-immediate fields in 32-
            // and 64-bit operands: its element is at most eight bits, N is zero.
            // The caller's opcode owns the destination width; this supplies data.
            wanted *= 0x01010101U;
        }
        // AArch64's 32-bit logical immediate is a repeated, rotated run of
        // one bits. Find its smallest element; N is zero. This converts a
        // native data value, not an original CPU instruction or game routine.
        for (unsigned size = 2; size <= 32; size *= 2) {
            const auto mask = (std::uint64_t{1} << size) - 1U;
            for (unsigned ones = 1; ones < size; ++ones) {
                const auto run = (std::uint64_t{1} << ones) - 1U;
                for (unsigned rotation = 0; rotation < size; ++rotation) {
                    const auto element = ((run >> rotation) | (run << (size - rotation))) & mask;
                    std::uint32_t repeated = 0;
                    for (unsigned bit = 0; bit < 32; bit += size)
                        repeated |= static_cast<std::uint32_t>(element << bit);
                    if (repeated == wanted) {
                        const auto imms = ((~(size - 1U) << 1U) | (ones - 1U)) & 63U;
                        return ((rotation << 6U) | imms) << field.destination_shift;
                    }
                }
            }
        }
        throw std::runtime_error("Native mask has no 32-bit logical-immediate representation");
    }
    if (field.projection == NativeMaterialProjection::halfword_lane_shift64) {
        const auto index = element_width == 1 ? std::uint32_t{values[field.first]}
                                             : read32(values, std::size_t{field.first} * 4U);
        return ((index % 4U) * 16U) << 10U;
    }
    if (field.projection == NativeMaterialProjection::halfword_pair_residue64) {
        const auto item = [&](std::size_t i) {
            const auto at = (std::size_t{field.first} + i * field.source_stride) * element_width;
            return element_width == 1 ? std::uint32_t{values[at]} : read32(values, at);
        };
        const auto first = item(0), second = item(1);
        const auto a = first % 4U, b = second % 4U;
        require(first / 4U == second / 4U && a && b && a != b,
                "Paired halfwords require distinct nonzero lanes in one word");
        // Lane zero is copied separately. Preserve the one remaining lane with
        // a 16-one-bit logical mask, derived from both selected destinations.
        unsigned remaining = 1;
        while (remaining == a || remaining == b) ++remaining;
        const auto rotation = (64U - remaining * 16U) & 63U;
        return (4096U | (rotation << 6U) | 15U) << 10U;
    }
    if (field.projection == NativeMaterialProjection::halfword_lane_insert64) {
        const auto lane = element_width == 1 ? values[field.first]
                                            : read32(values, std::size_t{field.first} * 4U);
        require(lane < 4, "Native halfword insertion exceeds its 64-bit word");
        // BFM rotates a low halfword into a selected native-word lane.
        // The lane is ROM data; widths and instruction encoding are metadata.
        const auto rotation = (64U - 16U * lane) & 63U;
        return ((rotation << 6U) | 15U) << 10U;
    }
    if (field.projection == NativeMaterialProjection::scaled_byte_logical64) {
        const auto source = element_width == 1 ? std::uint32_t{values[field.first]}
                                               : read32(values,std::size_t{field.first} * 4);
        require(source > 0 && source <= 255, "Invalid scaled-byte logical source");
        const auto value = std::uint64_t{source} * field.multiplier;
        const auto trailing = static_cast<unsigned>(std::countr_zero(value));
        const auto run = value >> trailing;
        require((run & (run+1U)) == 0, "Scaled byte is not one contiguous logical run");
        // A byte shifted into a native record occupies less than one 64-bit
        // element. Encode its nonrepeated run, without storing a selected mask.
        return ((1U << 12U) | (((64U-trailing) & 63U) << 6U) |
                (static_cast<unsigned>(std::bit_width(run))-1U)) << 10U;
    }
    if (field.projection == NativeMaterialProjection::word_bound_previous_units) {
        const auto source = element_width == 1 ? std::uint32_t{values[field.first]}
                                               : read32(values,std::size_t{field.first} * 4);
        require(source > 0 && source <= 65535, "Invalid preceding word-bound source");
        const auto previous = (source >> std::countr_zero(source))-1U;
        require(previous < 4096, "Preceding bound exceeds its native field");
        // x < bound is equivalent to (x >> trailing_zeroes(bound)) <= previous.
        // The companion shift is restored separately from the same input.
        return previous << 10U;
    }
    if (field.projection == NativeMaterialProjection::opposite_byte_complement) {
        const auto item = [&](std::size_t index) -> std::uint32_t {
            const auto at = index * element_width;
            return element_width == 1 ? values[at] : read32(values, at);
        };
        const auto negative = item(field.first);
        const auto positive = item(std::size_t{field.first} + field.source_stride);
        require(negative > 128 && negative <= 255 && positive > 0 && positive < 128 &&
                    negative + positive == 256,
                "Native conditional negation requires opposite nonzero byte values");
        // MOVN supplies a complemented magnitude; conditional negation selects
        // the other endpoint. Neither choice may remain in the template.
        return (255U - negative) << 5U;
    }
    if (field.projection == NativeMaterialProjection::pair_clear_bit32) {
        const auto item = [&](std::size_t index) -> std::uint32_t {
            const auto at = index * element_width;
            return element_width == 1 ? values[at] : read32(values, at);
        };
        const auto first = item(field.first);
        const auto second = item(std::size_t{field.first} + field.source_stride);
        const auto difference = first ^ second;
        require(std::has_single_bit(difference) && (first & difference) == 0,
                "Native folded pair does not differ by one added bit");
        // The compiler can combine two equality tests by clearing their one
        // differing bit. Derive that mask from both ROM values. AArch64 uses
        // N=0, imms=30 for 31 one-bits, rotated around a 32-bit element.
        const auto bit = static_cast<unsigned>(std::countr_zero(difference));
        return (30U | ((31U - bit) << 6U)) << field.destination_shift;
    }
    const bool color_math = field.projection == NativeMaterialProjection::color_math_layers ||
                            field.projection == NativeMaterialProjection::color_math_mode;
    const bool composite = field.projection == NativeMaterialProjection::composite_covered_mode ||
                           field.projection == NativeMaterialProjection::composite_uncovered_mode;
    const bool palette_color = field.projection == NativeMaterialProjection::palette_color_offset;
    std::array<std::uint32_t, 3> blend{};
    std::uint32_t product_factor = 0;
    std::uint64_t value = period_pair ? period_bias : 0;
    for (unsigned i = 0; i < (period_pair ? 0U : field.count); ++i) {
        const auto at = (std::size_t{field.first} + std::size_t{i} * field.source_stride) * element_width;
        const auto item = element_width == 1 ? values[at] : read32(values, at);
        if (palette_color) {
            require(item <= 65535, "Native palette address contains a non-word value");
            blend[i] = item;
            if (i == 2) {
                const auto destination = blend[0], group = (blend[1] >> 10U) & 7U;
                require(destination >= 0x3049U && destination < 0x3049U + 8U * 32U &&
                            (destination - 0x3049U) % 32U == 0 && group < 4 &&
                            ((blend[2] >> 10U) & 7U) == group,
                        "Native palette address exceeds its bank or rows disagree");
                // Original buffer/attribute formats select a four-color group.
                // Native Palette has 70 bytes per bank and two per color. The
                // owner/member base is supplied by the authenticated field map.
                // Reject selections crossing this consumer's 16-color bank.
                value = ((destination - 0x3049U) / 32U) * 70U + group * 8U;
            }
        } else if (color_math) {
            require(item <= 255, "Native color-math preset contains a non-byte value");
            blend[i % 3] = item;
            if (i % 3 == 2) {
                const auto operation = blend[0], affected = blend[1], operand = blend[2];
                require((operation == 0 || operation == 2 || operation == 32) &&
                            (affected & 8U) == 0 && (operand & 232U) == 0,
                        "Invalid native color-math preset");
                // Format conversion for native set_blend's layer pair and
                // ColorBlend enum. No authored preset or original rendering
                // program is embedded. Coalesced fields require all selected
                // presets to produce the same value before restoration.
                const auto projected = field.projection == NativeMaterialProjection::color_math_layers
                    ? ((operation & 2U) ? operand : 0U) | ((affected & 55U) << 8U)
                    : (operation == 0 && affected == 0 && operand == 0) ? 0U
                    : 1U + (affected >> 7U) + 2U * ((affected >> 6U) & 1U);
                require(i == 2 || value == projected,
                        "Native shared color-math field has unequal source values");
                value = projected;
            }
        } else if (composite) {
            require(item <= 255, "Native composite flags contain a non-byte value");
            blend[i] = item;
            if (i == 1) {
                // Selected format flags become a typed native blend pair.
                // Halving is suppressed for an absent image operand, but a
                // fixed operand still exists outside secondary-image coverage.
                const bool fixed = (blend[0] & 2U) == 0;
                const bool half = (blend[1] & 64U) != 0 &&
                    (fixed || field.projection == NativeMaterialProjection::composite_covered_mode);
                value = 1U + (blend[1] >> 7U) + (half ? 2U : 0U);
            }
        } else if (field.projection == NativeMaterialProjection::unsigned_difference ||
                   field.projection == NativeMaterialProjection::simd_difference_byte) {
            if (i == 0) value = item;
            else {
                require(value >= item, "Native endpoint difference underflow");
                value -= item;
            }
        } else if (field.projection == NativeMaterialProjection::power_of_two_remainder) {
            if (i == 0) value = item;
            else {
                require(std::has_single_bit(item), "Native remainder requires a power-of-two period");
                value %= item;
            }
        } else if (field.projection == NativeMaterialProjection::repeated_bit16) {
            require(item <= 65535 && std::has_single_bit(item) && (i == 0 || item == value),
                    "Native shared logical mask disagrees with source values");
            value = item;
        } else if (field.projection == NativeMaterialProjection::bit_indices32) {
            // A compiler can fold membership comparisons into a bit set. Its
            // selected identities still come from the ROM, not the profile.
            require(item < 32, "Native membership index exceeds its word");
            value |= std::uint32_t{1} << item;
        } else if (field.projection == NativeMaterialProjection::packed_bytes) {
            // Adjacent byte members can share one compiler-emitted immediate.
            // Their values remain separate ROM operands, in low-byte-first order.
            require(item <= 255, "Native packed field contains a non-byte value");
            value |= std::uint64_t{item} << (8U * i);
        } else if (field.projection == NativeMaterialProjection::complement_byte ||
                   field.projection == NativeMaterialProjection::complement_word) {
            // Overflow thresholds and complemented native immediates can encode
            // a source value indirectly. Complement within its declared width;
            // neither the original value nor its complement belongs in the map.
            const auto mask = field.projection == NativeMaterialProjection::complement_byte
                ? 255U : 65535U;
            require(item <= mask, "Native complement exceeds its input width");
            value = mask - item;
        } else if (field.projection == NativeMaterialProjection::simd_movi_byte) {
            require(item <= 255, "Native SIMD immediate requires one byte");
            // MOVI .4S, #imm8, LSL #0 splits the byte between bits 18:16
            // and 9:5. The intervening mode bits are fixed to 000001.
            // Restore that complete field without retaining an authored byte.
            value = (item & 31U) | ((item >> 5U) << 11U) | 32U;
        } else if (field.projection == NativeMaterialProjection::unsigned_bit_width) {
            // Known result bits can narrow a compiled insertion. Derive the
            // width from the selected bound; do not retain its folded copy.
            value = static_cast<unsigned>(std::bit_width(item));
        } else if (field.projection == NativeMaterialProjection::power_of_two_fixed8_shift ||
                   field.projection == NativeMaterialProjection::power_of_two_fixed8_bias) {
            require(item <= 256 && std::has_single_bit(item),
                    "Native fixed-eight scale requires one fractional power of two");
            // x * scale / 256 folds to a right shift. Derive the displacement
            // from the selected unsigned magnitude; eight is the format width.
            // Ceiling division uses period - 1 before the shift. Both fields
            // derive from the same fractional rate without storing its bias.
            value = field.projection == NativeMaterialProjection::power_of_two_fixed8_shift
                ? 8U - static_cast<unsigned>(std::countr_zero(item)) : 256U / item - 1U;
        } else if (field.projection == NativeMaterialProjection::power_of_two_shift) {
            require(std::has_single_bit(item),
                    "Native division shift requires a power-of-two period");
            value = static_cast<unsigned>(std::countr_zero(item));
        } else if (field.projection == NativeMaterialProjection::low_mask_width ||
                   field.projection == NativeMaterialProjection::simd_right_shift32) {
            // Division by a mask-plus-one period can compile to a shift. Only
            // contiguous low-bit masks have that representation; derive the
            // shift instead of storing the authored period's compiled copy.
            require(std::has_single_bit(std::uint64_t{item} + 1U),
                    "Native shift source is not a contiguous low-bit mask");
            value = static_cast<unsigned>(std::bit_width(item));
            if (field.projection == NativeMaterialProjection::simd_right_shift32) {
                require(value >= 1 && value <= 32, "Native SIMD right shift exceeds its lane");
                // USHR/USRA word lanes encode immh:immb as 64 - shift.
                value = 64U - value;
            }
        } else if (field.projection == NativeMaterialProjection::negative_fixed8_shift ||
                   field.projection == NativeMaterialProjection::negative_fixed8_rounding) {
            // A negative 8.8 displacement can become subtraction of an unsigned
            // shifted age. Only negative powers of two up to one pixel per
            // update have this representation. The speed itself stays in ROM.
            require(item >= 65280U && item <= 65535U,
                    "Native fixed-point shift requires a negative fractional word");
            const auto magnitude = 65536U - item;
            require(std::has_single_bit(magnitude),
                    "Native fixed-point shift is not an exact power of two");
            value = 8U - static_cast<unsigned>(std::countr_zero(magnitude));
            // Rounding the magnitude up preserves floor rounding of negative
            // subpixel displacement. Derive its bias from the same ROM speed.
            if (field.projection == NativeMaterialProjection::negative_fixed8_rounding)
                value = (std::uint32_t{1} << value) - 1U;
        } else if (field.projection == NativeMaterialProjection::byte_min_reciprocal_multiplier ||
                   field.projection == NativeMaterialProjection::byte_min_reciprocal_shift) {
            require(item >= 2 && item <= 256, "Native byte divisor exceeds its domain");
            // This compiler form first masks its numerator to one byte. Find
            // the smallest exact multiply/shift pair over that entire domain.
            // No original divisor or precomputed reciprocal is stored here.
            bool found = false;
            for (unsigned shift = 0; shift <= 16; ++shift) {
                const auto multiplier = ((std::uint32_t{1} << shift) + item - 1U) / item;
                bool exact = true;
                for (unsigned numerator = 0; numerator < 256; ++numerator)
                    if ((numerator * multiplier >> shift) != numerator / item) {
                        exact = false;
                        break;
                    }
                if (exact) {
                    value = field.projection == NativeMaterialProjection::byte_min_reciprocal_multiplier
                        ? multiplier : shift;
                    found = true;
                    break;
                }
            }
            require(found, "No bounded exact byte reciprocal");
        } else if (field.projection == NativeMaterialProjection::incremented_byte_reciprocal16) {
            require(item >= 2 && item <= 256, "Native incremented-byte divisor exceeds its domain");
            const auto multiplier = (65536U + item - 1U) / item;
            // The numerator is a promoted byte plus one, not a masked byte.
            // Include its upper endpoint when proving the compiler form exact.
            for (unsigned numerator = 0; numerator <= 256; ++numerator)
                require((numerator * multiplier >> 16U) == numerator / item,
                        "Native incremented-byte reciprocal is inexact");
            value = multiplier;
        } else if (field.projection == NativeMaterialProjection::byte_reciprocal_low16 ||
                   field.projection == NativeMaterialProjection::byte_reciprocal_high16) {
            require(item >= 2 && item <= 256, "Native byte divisor exceeds its domain");
            const auto multiplier = ((UINT64_C(1) << 32U) + item - 1U) / item;
            // Derive ceil(2^32 / byte divisor), proving the byte domain here.
            // A larger native numerator domain needs independent profile proof.
            for (unsigned numerator = 0; numerator < 256; ++numerator)
                require((numerator * multiplier >> 32U) == numerator / item,
                        "Native byte reciprocal is inexact");
            value = field.projection == NativeMaterialProjection::byte_reciprocal_low16
                ? multiplier & 65535U : multiplier >> 16U;
        } else if (field.projection == NativeMaterialProjection::unsigned_reciprocal_low16 ||
                   field.projection == NativeMaterialProjection::unsigned_reciprocal_high16 ||
                   field.projection == NativeMaterialProjection::unsigned_reciprocal_shift ||
                  field.projection == NativeMaterialProjection::unsigned_reciprocal_narrow_shift) {
            // A native unsigned division can fold to multiply/shift. Derive its
            // reciprocal from the extracted divisor, never a stored magic word.
            // This bounded form needs no correction instruction. Rounded-up
            // reciprocals cannot underestimate; their error at the end of each
            // full quotient interval increases monotonically. Check the last
            // full interval and the final partial interval to prove the whole
            // uint32_t domain. Other compiler forms must fail closed.
            require(item >= 2U && item <= 256U, "Native reciprocal divisor exceeds its domain");
            const unsigned shift = 31U + static_cast<unsigned>(std::bit_width(item));
            const auto scale = std::uint64_t{1} << shift;
            const auto multiplier = (scale + item - 1U) / item;
            const auto quotient = UINT32_MAX / item;
            require(multiplier <= UINT32_MAX &&
                        ((std::uint64_t{quotient} * item - 1U) * multiplier >> shift) == quotient - 1U &&
                        (std::uint64_t{UINT32_MAX} * multiplier >> shift) == quotient,
                    "Native reciprocal requires an unsupported correction");
            if (field.projection == NativeMaterialProjection::unsigned_reciprocal_narrow_shift) value = 64U - shift;
            else if (field.projection == NativeMaterialProjection::unsigned_reciprocal_shift) value = shift;
            else if (field.projection == NativeMaterialProjection::unsigned_reciprocal_low16)
                value = multiplier & 65535U;
            else value = multiplier >> 16U;
        } else if (field.projection == NativeMaterialProjection::bg2_priority_depth) {
            require(item <= 1, "Native background priority is not one bit");
            // RasterCell's BG2 ranks interleave other backgrounds and objects.
            // These rank identities describe the native renderer; the selected
            // low/high priority bit is authenticated ROM data.
            value = item == 0 ? 4U : 7U;
        } else if (field.projection == NativeMaterialProjection::equal_values) {
            // A compiler can share one constant across distinct source data
            // sites. Check every site before accepting that shared value.
            require(i == 0 || item == value, "Native shared field has unequal source values");
            value = item;
        } else if (field.projection == NativeMaterialProjection::consecutive_first) {
            // Adjacent data identities can fold into one base plus an index.
            // Validate every selected identity before restoring that base;
            // an input that would require different code must fail closed.
            if (i == 0) value = item;
            else require(std::uint64_t{item} == value + i,
                         "Native consecutive field has nonconsecutive source values");
        } else if (field.projection == NativeMaterialProjection::pair_product_sum) {
            if (i % 2 == 0) product_factor = item;
            else {
                // One word product plus the bounded previous sum fits uint64_t.
                // The common bound check rejects overflow before another pair.
                value += std::uint64_t{product_factor} * item;
            }
        } else value += item;
        require(value <= UINT32_MAX, "Native material field sum overflow");
    }
    if (field.projection == NativeMaterialProjection::repeated_bit16) {
        // AArch64 N:immr:imms encoding: repeat a 16-bit element with a single
        // set bit. N=0, imms=32; immr rotates bit zero into the source position.
        // This is native data encoding, not execution or translation of SNES code.
        const auto bit = static_cast<unsigned>(std::countr_zero(static_cast<std::uint32_t>(value)));
        return (32U | (((16U - bit) & 15U) << 6U)) << field.destination_shift;
    }
    if (field.projection == NativeMaterialProjection::simd_difference_byte) {
        require(value <= 255, "Native SIMD endpoint difference exceeds one byte");
        // Same native immediate encoding as simd_movi_byte; its value is a
        // derived interval length, never an authored length stored in the map.
        value = (value & 31U) | ((value >> 5U) << 11U) | 32U;
    }
    if (field.projection == NativeMaterialProjection::sum_low_byte_complement) {
        // Byte-valued output can discard upper bits of a source-derived sum.
        // The loop already rejected an overflowing sum before this narrowing.
        value = 255U - (value & 255U);
    }
    // The bounded unsigned product fits int64_t. Apply the signed relocation
    // bias before exact division, avoiding host signed overflow or truncation.
    const auto biased = static_cast<std::int64_t>(value * field.multiplier) + field.addend;
    const auto divisor = std::int64_t{1} << field.divisor_shift;
    require(biased >= 0 && biased % divisor == 0 &&
                biased / divisor < (std::int64_t{1} << field.width),
            "Native material field is not exactly representable");
    return static_cast<std::uint32_t>(biased / divisor) << field.destination_shift;
}
NativeRestoration decode_native_restoration(ByteView data) {
    constexpr unsigned header = 124;
    require(data.size() >= header &&
                std::ranges::equal(slice(data, 0, 8), restoration_magic) && read32(data, 8) == 3,
            "Invalid native restoration header");
    const auto count = read32(data, 12);
    const auto fields = read32(data, 120);
    require(count > 0 && count <= 4096 && fields <= max_native_restoration_fields &&
                data.size() == header + count * 8U + fields * 44U,
            "Invalid native restoration extent");
    require(digest_at(data, 16) == rom_material_recipe(), "Native restoration recipe mismatch");
    NativeRestoration out{read32(data, 112), read32(data, 116),
                          digest_at(data, 48), digest_at(data, 80), {}, {}};
    require(out.signed_prefix_size >= 32 && out.signed_prefix_size < out.image_size &&
                out.image_size <= 32U * 1024U * 1024U,
            "Invalid native template extent");
    std::uint32_t end = 32;
    for (unsigned i = 0; i < count; ++i) {
        const RomMaterialRange row{read32(data, header + i * 8), read32(data, header + i * 8 + 4)};
        require(row.material < std::size(material_specs), "Unknown native material identity");
        const auto bytes = material_specs[row.material].count * material_specs[row.material].native_width;
        require(row.offset >= end && row.offset <= out.signed_prefix_size &&
                    bytes <= out.signed_prefix_size - row.offset,
                "Overlapping or out-of-range native restoration");
        end = row.offset + bytes;
        out.ranges.push_back(row);
    }
    end = 32;
    for (unsigned i = 0; i < fields; ++i) {
        const auto at = header + count * 8U + i * 44U;
        const auto bias = read32(data, at + 20);
        NativeMaterialField field{read32(data, at), read32(data, at + 4),
            read32(data, at + 8), read32(data, at + 12), read32(data, at + 16),
            static_cast<std::int32_t>(bias <= INT32_MAX ? std::int64_t{bias} :
                                      std::int64_t{bias} - 0x100000000LL),
            read32(data, at + 24), read32(data, at + 28), read32(data, at + 32),
            read32(data, at + 36), static_cast<NativeMaterialProjection>(read32(data, at + 40))};
        require(field.material < std::size(material_specs) && field.offset >= end &&
                    field.offset % 4 == 0 && field.offset <= out.signed_prefix_size &&
                    4 <= out.signed_prefix_size - field.offset,
                "Invalid native field destination");
        const auto& spec = material_specs[field.material];
        validate_field(field, spec.count);
        for (const auto& range : out.ranges) {
            const auto& other = material_specs[range.material];
            require(field.offset + 4 <= range.offset ||
                        range.offset + other.count * other.native_width <= field.offset,
                    "Native field overlaps a data range");
        }
        end = field.offset + 4;
        out.fields.push_back(field);
    }
    return out;
}
Bytes restore_native_image(ByteView image, const NativeRestoration& plan,
                           const std::vector<Bytes>& materials) {
    require(image.size() == plan.image_size && sha256(image) == plan.template_hash,
            "Native template does not match restoration map");
    require(materials.size() == std::size(material_specs), "Incomplete ROM materials");
    for (std::size_t i = 0; i < materials.size(); ++i)
        require(materials[i].size() == material_specs[i].count * material_specs[i].native_width &&
                    hex(sha256(materials[i])) == material_specs[i].native_digest,
                "Unvalidated native material");
    Bytes out(image.begin(), image.end());
    std::size_t end = 32;
    for (const auto& row : plan.ranges) {
        require(row.material < materials.size(), "Unknown restoration material");
        const auto& bytes = materials[row.material];
        require(row.offset >= end && row.offset <= plan.signed_prefix_size &&
                    bytes.size() <= plan.signed_prefix_size - row.offset,
                "Invalid restoration destination");
        const auto blank = slice(image, row.offset, bytes.size());
        require(std::ranges::all_of(blank, [](auto value) { return value == 0; }),
                "Native material slot is not empty");
        std::copy(bytes.begin(), bytes.end(), out.begin() + row.offset);
        end = row.offset + bytes.size();
    }
    end = 32;
    for (const auto& field : plan.fields) {
        require(field.material < materials.size() && field.offset >= end &&
                    field.offset % 4 == 0 && field.offset <= plan.signed_prefix_size &&
                    4 <= plan.signed_prefix_size - field.offset,
                "Invalid native field destination");
        const auto value = native_material_field(materials[field.material],
                                                 material_specs[field.material].native_width, field);
        const auto mask = ((std::uint32_t{1} << field.width) - 1) << field.destination_shift;
        for (const auto& range : plan.ranges)
            require(field.offset + 4 <= range.offset ||
                        range.offset + materials.at(range.material).size() <= field.offset,
                    "Native field overlaps a data range");
        const auto word = read32(image, field.offset);
        require((word & mask) == 0, "Native data field is not empty");
        const auto restored = word | value;
        for (unsigned byte = 0; byte < 4; ++byte)
            out[field.offset + byte] = static_cast<std::uint8_t>(restored >> (byte * 8U));
        end = field.offset + 4;
    }
    require(sha256(slice(out, 0, plan.signed_prefix_size)) == plan.restored_prefix_hash,
            "Restored native code/data does not match the packaged build");
    return out;
}
} // namespace ff6
