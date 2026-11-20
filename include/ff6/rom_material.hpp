#pragma once
#include "ff6/content.hpp"

namespace ff6 {
struct RomMaterialContainer {
    std::uint32_t offset = 0, size = 0, decoded_size = 0;
    std::string_view digest{}, decoded_digest{};
};
struct RomMaterialSpec {
    std::string_view id;
    std::uint32_t offset, count, source_width, native_width, source_count;
    std::string_view digest, native_digest;
    std::string_view transform;
    std::span<const std::uint32_t> sites;
    std::span<const std::uint32_t> groups;
    RomMaterialContainer container{};
};
std::span<const RomMaterialSpec> rom_material_specs();
Digest rom_material_recipe();
// Widen little-endian unsigned ROM values to their existing native table type.
// Kept separate so conversion/bounds tests can use entirely synthetic fixtures.
Bytes widen_rom_material(ByteView input, unsigned source_width);
Bytes derive_rom_material(ByteView source, unsigned source_width, unsigned native_width,
                          std::string_view transform, unsigned output_count,
                          std::span<const std::uint32_t> groups = {});
std::vector<Bytes> extract_rom_materials(const RomImage& rom);
// Independently selected ROM operands may occupy one pooled native constant.
// Validate their equality before restoring that constant through its one owner.
void validate_rom_material_alias(ByteView value, ByteView target,
                                 std::uint32_t first, std::uint32_t count);
// Selected, authenticated data operands for private developer-generated source.
// Does not expose an executable container or unclassified surrounding bytes.
Bytes read_rom_material_source(const RomImage& rom, const RomMaterialSpec& spec);

struct RomMaterialRange {
    std::uint32_t offset, material;
};
enum class NativeMaterialProjection : std::uint32_t {
    sum, repeated_bit16, bit_indices32, packed_bytes, complement_byte, equal_values,
    pair_clear_bit32, consecutive_first, color_math_layers, color_math_mode, logical_mask32,
    complement_word, low_mask_width, byte_bound_mask32, negative_fixed8_shift,
    negative_fixed8_rounding, unsigned_reciprocal_low16,
    unsigned_reciprocal_high16, unsigned_reciprocal_shift,
    composite_covered_mode, composite_uncovered_mode, repeated_byte_logical_immediate,
    simd_movi_byte, simd_right_shift32, power_of_two_shift, period_mask32,
    palette_color_offset, byte_period_bias, byte_period_mask32,
    equal_power_of_two_left_shift32, unsigned_difference, simd_difference_byte,
    simd_affine_byte, byte_reciprocal_low16, byte_reciprocal_high16,
    byte_min_reciprocal_multiplier, byte_min_reciprocal_shift, bg2_priority_depth,
    incremented_byte_reciprocal16, sum_low_byte_complement, power_of_two_remainder,
    power_of_two_word_left_shift, unsigned_bit_width, power_of_two_plus_one_shift,
    byte_mask_add_logical64, power_of_two_fixed8_shift, pair_product_sum,
    difference_scale_word_shift, difference_scale_subtract_shift,
    two_bit_scale_word_shift, two_bit_scale_add_shift, word_bound_mask32,
    word_bound_shift, word_bound_units,
    corrected_reciprocal_low16, corrected_reciprocal_high16, corrected_reciprocal_shift, two_bit_scale_long_shift, power_of_two_long_left_shift,
    power_of_two_nonnegative_insert32, zero_register,
    scaled_floor_bias, simd_scaled_floor_bias, two_bit_scaled_right_shift, structure_load_opcode, nonnegative_byte_insert_width, byte_power_of_two_bit_select, power_of_two_fixed8_bias,
    domain_complement_mask32, opposite_byte_complement,
    scaled_byte_logical64, word_bound_previous_units, adjacent_power_of_two_shift,
    simd_affine_halfword_byte, range_alignment_mask32, halfword_lane_insert64,
    halfword_lane_shift64, halfword_pair_residue64, relative_membership_halfword,
    scaled_quotient_shift, scaled_quotient_mask32, simd_affine_halfword_clear_byte,
    unsigned_reciprocal_narrow_shift, power_of_two_halfword_left_shift
};
// Data relocation in an already compiled native word. This describes a
// bounded arithmetic projection, not an original CPU instruction or scheduler.
struct NativeMaterialField {
    std::uint32_t offset, material, first, count, multiplier;
    std::int32_t addend;
    std::uint32_t divisor_shift, destination_shift, width;
    std::uint32_t source_stride = 1;
    NativeMaterialProjection projection = NativeMaterialProjection::sum;
};
std::uint32_t native_material_field(ByteView values, unsigned element_width,
                                    const NativeMaterialField& field);
struct NativeRestoration {
    std::uint32_t image_size, signed_prefix_size;
    Digest template_hash, restored_prefix_hash;
    std::vector<RomMaterialRange> ranges;
    std::vector<NativeMaterialField> fields;
};
// Bound the current format to under one MiB of field metadata. Capacity is
// independent of content identities; every destination and digest still validates.
inline constexpr std::uint32_t max_native_restoration_fields = 16384;
// Maps contain locations, recipe IDs and digests only; never replacement bytes.
NativeRestoration decode_native_restoration(ByteView data);
Bytes restore_native_image(ByteView image, const NativeRestoration& plan,
                           const std::vector<Bytes>& materials);
} // namespace ff6
