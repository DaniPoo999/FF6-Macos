#pragma once
#include "ff6/content.hpp"

namespace ff6 {
// Two matching little-endian extent words and two matching delay bytes.
// Decode the group count using the original record stride, then lower the
// native optional-task copies/publications for the pinned C++ ABI.
std::array<std::uint32_t, 2> native_particle_pair_layout(ByteView operands,
                                                       unsigned group_stride);
Bytes make_native_particle_pairs(ByteView operands, unsigned group_stride);
// Lower first + index * (second - first) for two selected byte coordinates.
// Format: native input, scratch and output W registers. Emits the complete
// arithmetic expression, including the omission of an add when first is zero.
Bytes make_native_coordinate_pair(ByteView coordinates,
                                  std::span<const std::uint32_t> format);
// Lower a blank-cell rectangle or column-stride multiply for the pinned ABI.
// Width comes from decoded text positions. Format: operation, row count,
// base/output register, address/input register, dword/word/byte value registers.
// Blank-cell payload and record padding are native layout, not ROM content.
Bytes make_native_text_column_fragment(unsigned width, unsigned columns,
                                      std::span<const std::uint32_t> format);
// Publication-origin aliases followed by destination/byte-width/height word records.
// Format: scratch base, bytes per cell, canvas columns/rows, alias count.
// Adjacent rectangles must have the same height and begin at published row zero.
std::array<std::uint32_t, 3> native_text_region_layout(
    ByteView operands, std::span<const std::uint32_t> format);
// Native copy lowering appends owner register, source vector member and
// destination stack slot to the format. Selected extents are ROM inputs.
Bytes make_native_text_region_copy(ByteView operands, std::span<const std::uint32_t> format);
// Native blank-cell loop; caller supplies pointer, row counter and blank value.
Bytes make_native_text_region_clear(ByteView operands, std::span<const std::uint32_t> format);
// Aliased byte widths followed by aliased heights. Format: both alias counts,
// bytes per cell, canvas columns/rows. The caller owns vertical clipping.
std::array<std::uint32_t, 2> native_label_region_layout(
    ByteView operands, std::span<const std::uint32_t> format);
Bytes make_native_label_region_clear(ByteView operands, std::span<const std::uint32_t> format);

// Selected little-endian words: common source origin, two begin/end pairs,
// and two destination word addresses. The intervals must agree and contain
// complete 4bpp tiles. Result: count, first source byte offset, second-source
// displacement, and both decoded destination tile indices.
std::array<std::uint32_t, 5> native_paired_tile_copy_layout(
    ByteView operands, unsigned destination_origin, unsigned destination_tiles);
// Bounded lowering of checked paired Tile copies for the pinned native ABI.
// Format: destination origin/capacity, source-vector begin/end member offsets,
// native code address and bounds-error target. All selected indices and the
// number of emitted copies come from data; no original CPU code is consumed.
Bytes make_native_paired_tile_copy(ByteView operands, std::span<const std::uint32_t> format);
// Agreeing byte intervals followed by agreeing unsigned endpoint pairs.
// Return the interval and descending image span; aliases are independently read.
std::array<std::uint32_t, 2> native_clamped_phase_layout(
    ByteView operands, unsigned intervals, unsigned endpoints);
// Lower min(span, (age / interval) modulo 2^bits) for the pinned native ABI.
// Format: phase bit width, input, phase scratch, limit scratch, output register.
// This generates native arithmetic from data; it never translates CPU code.
Bytes make_native_clamped_phase(std::span<const std::uint32_t> layout,
                               std::span<const std::uint32_t> format);
// Paired traversal extents/strides followed by aliased phase masks, jitter
// masks and image origins. Group sizes describe the selected operand grammar.
std::array<std::uint32_t, 4> native_particle_strip_layout(
    ByteView operands, std::span<const std::uint32_t> groups);
// Lower one native unrolled-record region. Layout supplies emitter count and
// selected presentation values; format supplies record/ABI and native links.
Bytes make_native_particle_strip(std::span<const std::uint32_t> layout,
                                std::span<const std::uint32_t> format);
// Source/destination word-address pairs followed by a period word. Require a
// complete contiguous rotation; return first color, cycle length and period.
std::array<std::uint32_t, 3> native_palette_cycle_layout(
    ByteView operands, unsigned address_origin, unsigned colors);
// Native palette rotation lowering. Format: source address origin/color count,
// then SP byte offsets for clock, view flag, source pointer and destination
// palette. Register allocation and the interleaved view-flag publication belong
// to this compiler ABI; every selected color, period and copy count is input.
Bytes make_native_palette_cycle(ByteView operands, std::span<const std::uint32_t> format);
// Bounded native row-fill lowering. Inputs are selected data, never CPU code.
Bytes make_native_row_band_fill(ByteView operands, std::span<const std::uint32_t> format);
Bytes make_row_band_pattern(ByteView operands, std::span<const std::uint32_t> format);
// Nine destination/source word pairs describe three independent palette copies
// and six copies from one shared source. Generate the pinned native vector-copy
// prefix from those operands, ending before the caller's task search and final
// vector store. Its selected priority belongs to a separate ROM relocation.
// Format: original destination/source origins, source count, separate neutral-
// source address and native destination/source/neutral member offsets.
Bytes make_native_palette_initialization(ByteView operands,
                                         std::span<const std::uint32_t> format);
// Aliased true/false bit-field choices; order is all true sites, then false.
std::array<std::uint32_t, 2> native_condition_choices(
    ByteView operands, unsigned aliases, unsigned shift, unsigned width);
// A seven-word format instead supplies a caller-known register/value pair.
// It reuses that register on the true branch only when both extracted choices
// match the known value and zero; other values require another lowering.
// Materialize a value from existing NZCV flags without changing them or using
// scratch registers. Format: alias count, bit shift/width, result, condition.
// An optional caller-value selection adds retained register, final result and
// condition. The caller provides that retained value; its data is owned separately.
// A nine-word format instead prefixes one caller operation: kind 0 compares
// source register with a loop ordinal (last word zero); kind 1 publishes a word
// from source to base register plus byte offset. The four added words are kind,
// source, ordinal/base, zero/offset. Comparison sets NZCV; publication preserves
// it. The subsequent selection preserves these flags and needs no scratch.
Bytes make_native_condition_choice(ByteView operands, std::span<const std::uint32_t> format);
// Aliased palette choices followed by one little-endian canvas address.
// Return both choices and its byte offset in the native cell layout.
std::array<std::uint32_t, 3> native_condition_cell_values(
    ByteView operands, std::span<const std::uint32_t> format);
// Publish a preceding cell word, then select a palette from live NZCV flags.
// Format: the five source-format words above, result/condition, publication
// source/base registers and member/relative-cell byte offset. The origin comes
// from the ROM; native register/member bindings contain no authored position.
Bytes make_native_cell_condition(ByteView operands, std::span<const std::uint32_t> format);
// Two aliased positive power-of-two dimensions. Format: aliases per side,
// projection (value, unsigned negative, division exponent), scratch, result,
// existing NZCV condition. An optional selector register/immediate pair first
// establishes caller flags; subsequent dimension arithmetic preserves them.
Bytes make_native_dimension_choice(ByteView operands, std::span<const std::uint32_t> format);

// Lower a normalized native Boolean to two ROM-selected palette indices.
// Format: input and result registers; choices are ordered (true, false).
Bytes make_native_boolean_value(ByteView choices, std::span<const std::uint32_t> format);
// The call form first obtains the Boolean from an existing native helper.
// Format: input/result registers, call/helper addresses.
Bytes make_native_boolean_result(ByteView choices, std::span<const std::uint32_t> format);
// Search native optional records by a byte key, then choose (absent, present).
// Format: source/pointer/remaining/key-counter/presence/needle/result registers,
// record stride/count, signed key displacement biased by 256, code/exit addresses.
// Record layout belongs to the native caller; neither colors nor original
// machine-code sequences are encoded in this contract.
Bytes make_native_optional_byte_search(ByteView choices,
                                      std::span<const std::uint32_t> format);


// Two agreeing groups of byte attributes followed by a signed scroll word.
// Return the compact palette choices and the native caption displacement.
std::array<std::uint32_t, 3> native_caption_choice_layout(
    ByteView operands, unsigned aliases, unsigned shift, unsigned width);
// Native caption ABI: normalized Boolean input, palette in w3, horizontal
// displacement in w4, vertical displacement in w5. A nonzero key length adds
// its w2 argument; zero means the caller already supplied it. The key is an
// independently named resource identifier, not localized game text.
Bytes make_native_caption_choice(ByteView operands, unsigned aliases,
    unsigned shift, unsigned width, unsigned input_register, bool inverted,
    unsigned key_length, std::uint32_t code_address, std::uint32_t helper_address);
// Numbered palette prefix selected by one bit in a native scratch register.
// Format: attribute aliases/shift/width, source register, bit, inversion,
// optional unit-mask register (31 = absent), canvas stack offset (4096 = already
// supplied), ordinal and precision. Only the two palette projections are used.
Bytes make_native_bit_number_prefix(ByteView operands, std::span<const std::uint32_t> format);
// Flag-based caption call, with a resource key supplied through native linkage.
// Format: attribute aliases/shift/width, condition, stack displacement, key
// address/length and code/helper addresses. Both choices and scroll are ROM data.
Bytes make_native_flag_caption(ByteView operands, std::span<const std::uint32_t> format);
// Agreeing palette attributes, signed scroll, then original word cell addresses.
// The nine format fields describe the two attribute groups and cell encoding.
// Result: both compact palettes followed by interleaved native X/Y positions.
std::vector<std::uint32_t> native_number_choice_layout(
    ByteView operands, std::span<const std::uint32_t> format);
// Append cell index, native stack displacement, ordinal, digit count, ARM64
// condition and code/helper addresses to the layout format. The caller owns
// the comparison flags; the fragment preserves them until its helper call.
Bytes make_native_number_choice(ByteView operands, std::span<const std::uint32_t> format);
// Linked-image locations for a native constant-fill fragment. File offsets and
// VM offsets have the same page-relative displacement in the reviewed image.
struct NativeFillLayout {
    std::uint32_t code, patterns, fill_helper;
};
// Rebuild a compiler-lowered RGB555 row fill from data, not from original CPU
// instructions. This bounded ARM64/Clang profile uses the native scene ABI:
// x28 owns the scene, x19 the row buffer, and w8/v0 are scratch registers.
// It is a packaging operation; the game executes the resulting native block.
Bytes make_native_gray_fill(ByteView bands, NativeFillLayout layout);
// Alternate reviewed compiler lowering: small stores through the scene's row
// vector, without helper calls or linked constant pools. Unwritten rows retain
// the zero initialization performed by the surrounding native routine.
Bytes make_native_inline_gray_fill(ByteView bands);
// Compact row records contain a little-endian cell address and byte start/end
// values. Validate a two-layer rectangular layout and derive its native loop
// parameters. Repeated zero-based rows precede the first increasing tile run.
// This profile consumes selected data operands, never CPU instructions.
std::array<std::uint32_t, 8> native_tile_run_layout(ByteView rows);
// Reviewed compiler lowering for the native RasterCell loop. The complete
// fragment, including unrolled stores and branches, depends on the row data.
Bytes make_native_tile_runs(ByteView rows);
// A word copy extent followed by source/destination/clear word addresses and
// a byte empty-cell identity per row. Decode the rectangular image operation;
// the input contains selected operands, never executable instructions.
std::array<std::uint32_t, 7> native_affine_copy_layout(ByteView operands);
// Pinned compiler lowering of the decoded native cell copy/clear operation.
// Register allocation and vector width are ABI metadata; all image selections
// and the number of emitted operations depend on the supplied layout.
Bytes make_native_affine_copy(ByteView operands);
// Selected record extents (word + two byte aliases) and phase count. The
// original eight-byte record format is converted to native element counts.
std::array<std::uint32_t, 2> native_trail_layout(ByteView operands);
// Pinned native compiler lowerings for publication/search and drawing. Counts
// and repeated operations come from ROM data; linkage/registers are native ABI
// metadata. No original CPU instructions are decoded or executed.
Bytes make_native_trail_update(ByteView operands, std::uint32_t spawn_bytes);
Bytes make_native_trail_draw(ByteView operands, std::uint32_t code, std::uint32_t draw_helper);
// Clear the complement of extracted rectangular apertures in a native cell
// canvas. Origins are little-endian native indices, not original instructions.
// The pinned lowering uses RasterCell's 16-byte stride and a native blank tile.
Bytes make_native_cell_apertures(ByteView origins, unsigned columns, unsigned rows,
                                unsigned width, unsigned height, unsigned blank_tile);
// Two-store lowering for a short uniform-byte fill. The extent is a selected
// little-endian word; registers describe the reviewed native caller ABI.
// Supports strictly between one and two eight-byte stores, including a precise
// four-byte tail. Other compiler shapes fail closed.
Bytes make_native_short_fill(ByteView extent, unsigned pointer_register, unsigned value_register);
// Whole-vector uniform fill. The selected byte extent is scaled from a source
// transfer unit; native register identities and store order describe the caller.
// Only complete 16-byte vectors up to 256 bytes are admitted.
// Rebuild a complete native row-fill fragment. Format contains only unit,
// register, buffer-layout and compiler store-order metadata.
Bytes make_native_vector_rows(ByteView extent, std::span<const std::uint32_t> format);
Bytes make_native_vector_fill(ByteView extent, unsigned unit_shift,
                              unsigned pointer_register, unsigned vector_register,
                              bool reverse);
// Selected destination address and byte extent for a two-byte-element copy.
// Format: address-space origin/span and bank size. Return the within-bank word
// index and word count; reject odd, empty, or cross-bank ranges.
std::array<std::uint32_t, 2> native_banked_word_copy_layout(
    ByteView operands, std::span<const std::uint32_t> format);
// Bounded native scalar-copy lowering. Format extends the layout above with
// source, destination and scratch registers, then the destination's SP offset.
// The complete copy shape depends on the supplied extent, including its tail.
Bytes make_native_banked_word_copy(ByteView operands, std::span<const std::uint32_t> format);
// Pass (!input_boolean) * scale to a native helper. The scale is a selected
// one-to-three-bit field in the supplied byte; registers/linkage are native ABI.
// The argument lowering has no scratch register or flag side effects. Its size
// varies with the scale, so callers authenticate the entire generated fragment.
// Two independently agreeing byte choices selected by a normalized native Boolean.
// The first alias group is selected for false, the second for true.
std::array<std::uint32_t, 2> native_byte_choice_layout(ByteView source, unsigned aliases);
// Preserves flags; only the result and declared scratch register may change.
// Zero/one choices use native Boolean encoding; other bytes use ordinary arithmetic.
Bytes make_native_byte_choice(const std::array<std::uint32_t, 2>& values,
                              unsigned input, unsigned result, unsigned scratch);
// Append the selection to a scaled native pointer argument. Format contains
// choice input/result/scratch, then pointer output/base/index/element shift.
// The pointer expression executes first, before either choice clobber.
Bytes make_native_pointer_byte_choice(const std::array<std::uint32_t, 2>& values,
                                     std::span<const std::uint32_t> format);
Bytes make_native_boolean_argument(ByteView operand, unsigned shift, unsigned width,
                                   unsigned input_register, unsigned argument_register,
                                   std::uint32_t code_address, std::uint32_t helper_address);
// Native vector<byte> publication, with an external cold allocation path. The
// supplied value chooses zero-register, existing unit-register or literal form.
// Format: value register, known-unit flag, vector/end/capacity/temp/temp registers,
// stack byte/guard offsets, code address and cold target. Guards preserve the
// original native ABI; no original CPU instruction stream is consumed.
// One native constant for a byte value plus a C++ variant tag at bit 32.
// Reject constants requiring a larger fragment; no original instruction is read.
Bytes make_native_tagged_byte_constant(unsigned value, unsigned tag, unsigned destination);
Bytes make_native_inline_byte_append(unsigned value, std::span<const std::uint32_t> format);
// Form a stack-byte reference for a native call/tail branch. Format: scratch,
// stack byte offset, argument register, optional x0 source and save register
// (31 means absent), link flag, code address and helper/continuation address.
// Optional two words before linkage: frame register (29) and mirrored-address register
// (31 means absent). This form addresses a byte below FP instead of above SP;
// the displacement must fit the native signed byte-store encoding.
Bytes make_native_stack_byte_argument(unsigned value, std::span<const std::uint32_t> format);
// Compare a split index against a ROM-selected address boundary. Format:
// address origin/stride, agreeing operand count, low/high index bit widths,
// full-index/high-part/scratch registers and forward branch word displacement.
// The compiler uses a high-part test at the field boundary; other boundaries
// use an unsigned comparison. Both complete native instructions are generated.
Bytes make_native_split_index_branch(ByteView operands, std::span<const std::uint32_t> format);
// Branch when a packed index matches neither ROM-selected choice. Format:
// source field shift/width, runtime field shift, input/scratch registers,
// forward target in words from the fragment start, and one/two choices.
// A single choice may repeat across authenticated operand aliases. Complete
// zero-test/masked-test/explicit-comparison forms are generated, not patched.
// An optional single-choice suffix supplies a pointer register and stack byte
// displacement: load that pointer only when the selected choice matched.
Bytes make_native_selection_branch(ByteView operands, std::span<const std::uint32_t> format);
// Lower bounded row selection, variant addition or loop comparison from a
// decoded layout (rows, style boundary, tail boundary, step, tail, variants).
// Format: operation, three native registers, reserved zero and stack/step
// operand. The optional adjacent load/increment makes the fragment unique.
Bytes make_native_strided_row_fragment(std::span<const std::uint32_t> layout,
                                      std::span<const std::uint32_t> format,
                                      unsigned variant_count);
} // namespace ff6
