"""Remove data relocations from reviewed native compiler output.

Profiles contain source identities, arithmetic, locations and hashes only.
There are no replacement bytes or original CPU instructions in these profiles.
The native instruction/control-flow scaffolding requires its own provenance
review; restoring these data fields does not clear that separate question.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

KEYS = ('offset', 'material', 'first', 'count', 'multiplier', 'addend',
        'divisor_shift', 'destination_shift', 'width', 'source_stride', 'projection')
PROJECTIONS = {'sum': 0, 'repeated_bit16': 1, 'bit_indices32': 2, 'packed_bytes': 3,
               'complement_byte': 4, 'equal_values': 5, 'pair_clear_bit32': 6,
               'consecutive_first': 7, 'color_math_layers': 8, 'color_math_mode': 9,
               'logical_mask32': 10, 'complement_word': 11, 'low_mask_width': 12, 'byte_bound_mask32': 13,
               'negative_fixed8_shift': 14, 'negative_fixed8_rounding': 15,
               'unsigned_reciprocal_low16': 16, 'unsigned_reciprocal_high16': 17,
               'unsigned_reciprocal_shift': 18, 'composite_covered_mode': 19,
               'composite_uncovered_mode': 20, 'repeated_byte_logical_immediate': 21,
               'simd_movi_byte': 22, 'simd_right_shift32': 23,
               'power_of_two_shift': 24, 'period_mask32': 25,
               'palette_color_offset': 26, 'byte_period_bias': 27, 'byte_period_mask32': 28,
               'equal_power_of_two_left_shift32': 29, 'unsigned_difference': 30,
               'simd_difference_byte': 31, 'simd_affine_byte': 32,
               'byte_reciprocal_low16': 33, 'byte_reciprocal_high16': 34,
               'byte_min_reciprocal_multiplier': 35, 'byte_min_reciprocal_shift': 36,
               'bg2_priority_depth': 37, 'incremented_byte_reciprocal16': 38,
               'sum_low_byte_complement': 39, 'power_of_two_remainder': 40,
               'power_of_two_word_left_shift': 41, 'unsigned_bit_width': 42,
               'power_of_two_plus_one_shift': 43, 'byte_mask_add_logical64': 44,
               'power_of_two_fixed8_shift': 45, 'pair_product_sum': 46,
               'difference_scale_word_shift': 47, 'difference_scale_subtract_shift': 48,
               'two_bit_scale_word_shift': 49, 'two_bit_scale_add_shift': 50, 'word_bound_mask32': 51,
               'word_bound_shift': 52, 'word_bound_units': 53,
               'corrected_reciprocal_low16': 54, 'corrected_reciprocal_high16': 55,
               'corrected_reciprocal_shift': 56, 'two_bit_scale_long_shift': 57, 'power_of_two_long_left_shift': 58,
               'power_of_two_nonnegative_insert32': 59, 'zero_register': 60, 'scaled_floor_bias': 61,
               'simd_scaled_floor_bias': 62, 'two_bit_scaled_right_shift': 63, 'structure_load_opcode': 64, 'nonnegative_byte_insert_width': 65, 'byte_power_of_two_bit_select': 66, 'power_of_two_fixed8_bias': 67,
               'domain_complement_mask32': 68, 'opposite_byte_complement': 69,
               'scaled_byte_logical64': 70, 'word_bound_previous_units': 71,
               'adjacent_power_of_two_shift': 72, 'simd_affine_halfword_byte': 73, 'range_alignment_mask32': 74,
               'halfword_lane_insert64': 75, 'halfword_lane_shift64': 76,
               'halfword_pair_residue64': 77, 'relative_membership_halfword': 78,
               'scaled_quotient_shift': 79, 'scaled_quotient_mask32': 80, 'simd_affine_halfword_clear_byte': 81, 'unsigned_reciprocal_narrow_shift': 82,
               'power_of_two_halfword_left_shift': 83}


def load_profiles(path, specs):
    doc = json.loads(Path(path).read_text())
    if (not {'schema', 'scope', 'profiles'} <= set(doc) or
            set(doc) - {'schema', 'scope', 'profiles', 'blocks', 'aliases'} or
            doc['schema'] != 1):
        raise ValueError('Unknown native field profile schema')
    by_id = {row['id']: row for row in specs}
    # Bounded developer metadata; additional independently reviewed owners do
    # not change the runtime map's per-field/overlap/identity checks.
    if not 0 < len(doc['profiles']) <= 512:
        raise ValueError('Invalid native profile count')
    for profile in doc['profiles']:
        required = {'symbol', 'start', 'size', 'sha256', 'fields'}
        binary = profile.get('binary')
        binaries = {binary} if isinstance(binary, str) else {'ff6-game', 'ff6-inspector'}
        if (not required <= set(profile) or
                set(profile) - required - {'constant_reference', 'binary'} or
                ('binary' in profile and binary not in ('ff6-game', 'ff6-inspector')) or
                not isinstance(profile['symbol'], str) or
                not (re.fullmatch(r'__[A-Za-z0-9_$]+', profile['symbol']) or
                     (binary is not None and profile['symbol'] == '_main')) or
                type(profile['start']) is not int or profile['start'] < 0 or
                type(profile['size']) is not int or not 0 < profile['size'] <= 65536 or
                set(profile['sha256']) != binaries or
                any(not re.fullmatch(r'[0-9a-f]{64}', v) for v in profile['sha256'].values()) or
                not 0 < len(profile['fields']) <= 4096):
            raise ValueError('Invalid native field profile')
        reference = profile.get('constant_reference')
        if 'constant_reference' in profile:
            if (not isinstance(reference, dict) or set(reference) not in
                    ({'page', 'loads', 'sha256'}, {'page', 'loads', 'sha256', 'load_width'}) or
                    type(reference.get('load_width', 8)) is not int or
                    reference.get('load_width', 8) not in (8, 16) or
                    type(reference['page']) is not int or not 0 <= reference['page'] <= 65532 or
                    reference['page'] % 4 or not isinstance(reference['loads'], list) or
                    not 1 <= len(reference['loads']) <= 8 or
                    any(type(at) is not int or not reference['page'] < at <= 65532 or at % 4
                        for at in reference['loads']) or
                    reference['loads'] != sorted(set(reference['loads'])) or
                    not isinstance(reference['sha256'], dict) or
                    set(reference['sha256']) != binaries or
                    any(not isinstance(v, str) or not re.fullmatch(r'[0-9a-f]{64}', v)
                        for v in reference['sha256'].values()) or
                    profile['start'] + profile['size'] > reference.get('load_width', 8)):
                raise ValueError('Invalid native constant reference')
        end = profile['start']
        for field in profile['fields']:
            field.setdefault('source_stride', 1)
            field.setdefault('projection', 'sum')
            spec = by_id.get(field.get('material'))
            if (set(field) != set(KEYS) or spec is None or
                    any(type(field[k]) is not int for k in KEYS if k not in ('material', 'projection')) or
                    field['offset'] < end or field['offset'] % 4 or
                    field['offset'] + 4 > profile['start'] + profile['size'] or
                    not 0 <= field['first'] < spec['count'] or
                    not 0 < field['source_stride'] <= 65536 or field['count'] <= 0 or
                    field['count'] - 1 > (spec['count'] - 1 - field['first']) // field['source_stride'] or
                    not 0 < field['multiplier'] <= 65536 or
                    not -(1 << 31) <= field['addend'] < (1 << 31) or
                    not 0 <= field['divisor_shift'] <= 16 or
                    not 0 < field['width'] <= 24 or
                    not 0 <= field['destination_shift'] <= 32 - field['width'] or
                    type(field['projection']) is not str or field['projection'] not in PROJECTIONS or
                    (field['projection'] in ('scaled_floor_bias', 'simd_scaled_floor_bias') and
                     (not 1 <= field['count'] <= 16 or
                      (field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                      (1,0,0,5,16 if field['projection'] == 'scaled_floor_bias' else 14))) or
                    (field['projection'] in ('scaled_quotient_shift', 'scaled_quotient_mask32') and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],
                      field['destination_shift'],field['width']) !=
                     (2,1,0,0,16 if field['projection'] == 'scaled_quotient_shift' else 10,
                      6 if field['projection'] == 'scaled_quotient_shift' else 13)) or
                    (field['projection'] == 'relative_membership_halfword' and
                     (not 1 <= field['count'] <= 64 or not 0 <= field['addend'] < 4 or
                      (field['multiplier'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                      (1,0,5,16))) or
                    (field['projection'] == 'halfword_lane_shift64' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],
                      field['destination_shift'],field['width']) != (1,1,0,0,10,6)) or
                    (field['projection'] == 'halfword_pair_residue64' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],
                      field['destination_shift'],field['width']) != (2,1,0,0,10,13)) or
                    (field['projection'] == 'halfword_lane_insert64' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],
                      field['destination_shift'],field['width']) != (1,1,0,0,10,12)) or
                    (field['projection'] == 'two_bit_scaled_right_shift' and
                     ((field['count'],field['multiplier'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                      (1,1,0,10,12) or not 0 < field['addend'] < 32)) or
                    (field['projection'] == 'byte_power_of_two_bit_select' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                     (1,1,0,0,10,12)) or
                    (field['projection'] == 'nonnegative_byte_insert_width' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                     (1,1,0,0,10,6)) or
                    (field['projection'] == 'structure_load_opcode' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                     (1,1,0,0,12,4)) or
                    (field['projection'] == 'zero_register' and
                     ((field['count'], field['width']) != (1,5) or field['destination_shift'] not in (0,5,16))) or
                    (field['projection'] == 'packed_bytes' and field['count'] > 4) or
                    (field['projection'] == 'pair_product_sum' and field['count'] % 2 != 0) or
                    (field['projection'] == 'bg2_priority_depth' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift']) != (1,1,0,0)) or
                    (field['projection'] == 'simd_movi_byte' and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) != (1,1,0,0,5,14)) or
                    (field['projection'] in ('unsigned_difference', 'simd_difference_byte', 'power_of_two_remainder') and field['count'] != 2) or
                    (field['projection'] == 'simd_difference_byte' and
                     (field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) != (1,0,0,5,14)) or
                    (field['projection'] in ('simd_affine_byte', 'simd_affine_halfword_byte', 'simd_affine_halfword_clear_byte') and
                     (field['count'], field['destination_shift'], field['width']) != (1,5,14)) or
                    (field['projection'] in ('byte_reciprocal_low16', 'byte_reciprocal_high16',
                                              'byte_min_reciprocal_multiplier', 'byte_min_reciprocal_shift',
                                              'incremented_byte_reciprocal16') and field['count'] != 1) or
                    (field['projection'] == 'simd_right_shift32' and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) != (1,1,0,0,16,6)) or
                    (field['projection'] in ('corrected_reciprocal_low16', 'corrected_reciprocal_high16',
                                              'corrected_reciprocal_shift') and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) !=
                     (1,1,0,0,16 if field['projection'] == 'corrected_reciprocal_shift' else 5,
                      6 if field['projection'] == 'corrected_reciprocal_shift' else 16)) or
                    (field['projection'] in ('word_bound_shift', 'word_bound_units') and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) !=
                     (1,1,0,0,16 if field['projection'] == 'word_bound_shift' else 10,
                      6 if field['projection'] == 'word_bound_shift' else 12)) or
                    (field['projection'] in ('complement_byte', 'complement_word', 'low_mask_width', 'power_of_two_shift', 'unsigned_bit_width', 'power_of_two_fixed8_shift', 'power_of_two_fixed8_bias',
                                            'negative_fixed8_shift', 'negative_fixed8_rounding',
                                            'unsigned_reciprocal_low16', 'unsigned_reciprocal_high16',
                                            'unsigned_reciprocal_shift', 'unsigned_reciprocal_narrow_shift') and field['count'] != 1) or
                    (field['projection'] == 'consecutive_first' and field['count'] < 2) or
                    (field['projection'] == 'palette_color_offset' and field['count'] != 3) or
                    (field['projection'] in ('byte_period_bias', 'byte_period_mask32') and field['count'] != 2) or
                    (field['projection'] == 'byte_period_mask32' and
                     (field['multiplier'], field['addend'], field['divisor_shift'], field['width']) != (1,0,0,13)) or
                    (field['projection'] == 'equal_power_of_two_left_shift32' and
                     (field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) != (1,0,0,16,5)) or
                    (field['projection'] in ('power_of_two_word_left_shift', 'power_of_two_long_left_shift',
                                            'power_of_two_nonnegative_insert32', 'power_of_two_halfword_left_shift') and
                     (field['count'], field['multiplier'], field['addend'],
                      field['destination_shift'], field['width']) != (1,1,0,10,12)) or
                    (field['projection'] == 'power_of_two_plus_one_shift' and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) != (1,1,0,0,10,6)) or
                    (field['projection'] == 'two_bit_scale_long_shift' and
                     ((field['count'],field['multiplier'],field['divisor_shift'],
                       field['destination_shift'],field['width']) != (1,1,0,10,12) or
                      not 0 <= field['addend'] < 64)) or
                    (field['projection'] in ('two_bit_scale_word_shift', 'two_bit_scale_add_shift') and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],
                      field['destination_shift'],field['width']) !=
                     (1,1,0,0,10,12 if field['projection'] == 'two_bit_scale_word_shift' else 6)) or
                    (field['projection'] in ('difference_scale_word_shift', 'difference_scale_subtract_shift') and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) !=
                     (1,1,0,0,10,12 if field['projection'] == 'difference_scale_word_shift' else 6)) or
                    (field['projection'] == 'byte_mask_add_logical64' and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) != (2,1,0,0,10,13)) or
                    (field['projection'] in ('color_math_layers', 'color_math_mode') and field['count'] % 3 != 0) or
                    (field['projection'] in ('composite_covered_mode', 'composite_uncovered_mode') and field['count'] != 2) or
                    (field['projection'] in ('logical_mask32', 'byte_bound_mask32', 'word_bound_mask32', 'period_mask32',
                                            'repeated_byte_logical_immediate') and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'], field['width']) !=
                     (1, 1, 0, 0, 13)) or
                    (field['projection'] == 'scaled_byte_logical64' and
                     (field['count'] != 1 or field['multiplier'] & (field['multiplier'] - 1) or
                      (field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                      (0,0,10,13))) or
                    (field['projection'] == 'word_bound_previous_units' and
                     (field['count'],field['multiplier'],field['addend'],field['divisor_shift'],
                      field['destination_shift'],field['width']) != (1,1,0,0,10,12)) or
                    (field['projection'] == 'adjacent_power_of_two_shift' and
                     (not 2 <= field['count'] <= 16 or
                      (field['multiplier'], field['addend'], field['divisor_shift'],
                       field['destination_shift'], field['width']) != (1,0,0,5,16))) or
                    (field['projection'] == 'opposite_byte_complement' and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'],
                      field['destination_shift'], field['width']) != (2,1,0,0,5,16)) or
                    (field['projection'] == 'range_alignment_mask32' and
                     ((field['count'],field['multiplier'],field['divisor_shift'],field['width']) !=
                      (2,1,0,13) or not 1 < field['addend'] < 32)) or
                    (field['projection'] == 'domain_complement_mask32' and
                     ((field['count'],field['divisor_shift'],field['width']) != (1,0,13) or
                      field['multiplier'] & (field['multiplier']-1) or not 0 < field['addend'] < 32 or
                      field['multiplier'].bit_length()-1 > 32-field['addend'])) or
                    (field['projection'] == 'pair_clear_bit32' and
                     (field['count'], field['multiplier'], field['addend'], field['divisor_shift'], field['width']) !=
                     (2, 1, 0, 0, 13)) or
                    (field['projection'] == 'repeated_bit16' and
                     (field['multiplier'], field['addend'], field['divisor_shift'], field['width']) !=
                     (1, 0, 0, 13))):
                raise ValueError('Invalid native field expression/location')
            end = field['offset'] + 4
    blocks = doc.get('blocks', [])
    if not isinstance(blocks, list) or len(blocks) > 256:
        raise ValueError('Invalid generated native block count')
    for block in blocks:
        if (set(block) != {'symbol', 'start', 'size', 'material', 'sha256', 'inputs'} or
                not re.fullmatch(r'__[A-Za-z0-9_$]+', block['symbol']) or
                type(block['start']) is not int or block['start'] < 0 or block['start'] % 4 or
                type(block['size']) is not int or not 0 < block['size'] <= 4096 or block['size'] % 4 or
                set(block['material']) != {'ff6-game', 'ff6-inspector'} or
                set(block['sha256']) != {'ff6-game', 'ff6-inspector'} or
                any(not re.fullmatch(r'[0-9a-f]{64}', v) for v in block['sha256'].values()) or
                not isinstance(block['inputs'], list) or not block['inputs'] or
                len(block['inputs']) != len(set(block['inputs']))):
            raise ValueError('Invalid generated native block profile')
        for binary, material in block['material'].items():
            spec = by_id.get(material)
            if (spec is None or spec['native_representation'] != 'generated_native_block' or
                    spec['transform'] not in ('arm64_coordinate_pair', 'arm64_text_column_fragment', 'arm64_bit_number_prefix', 'arm64_palette_initialization', 'arm64_banked_word_copy', 'arm64_paired_tile_copy', 'arm64_particle_pairs', 'arm64_text_region_copy', 'arm64_text_region_clear', 'arm64_label_region_clear', 'arm64_split_index_branch', 'arm64_selection_branch', 'arm64_strided_row_fragment', 'arm64_byte_choice', 'arm64_clamped_phase', 'arm64_particle_strip', 'arm64_palette_cycle', 'arm64_row_band_fill', 'arm64_flag_caption', 'arm64_number_choice', 'arm64_condition_choice', 'arm64_cell_condition', 'arm64_palette_boolean_value', 'arm64_palette_boolean_result', 'arm64_palette_optional_search', 'arm64_dimension_choice', 'arm64_caption_choice', 'arm64_tagged_byte_constant', 'arm64_boolean_argument', 'arm64_inline_byte_append', 'arm64_stack_byte_argument', 'arm64_short_fill', 'arm64_vector_fill', 'arm64_vector_rows', 'arm64_gray_fill', 'arm64_inline_gray_fill', 'arm64_tile_runs',
                                              'arm64_trail_update', 'arm64_trail_draw',
                                              'arm64_cell_apertures', 'arm64_affine_copy') or spec['native_width'] != 1 or
                    spec['count'] != block['size'] or spec['native_sha256'] != block['sha256'][binary]):
                raise ValueError('Generated block does not match its material recipe')
            for identity in block['inputs']:
                source = by_id.get(identity)
                if (source is None or source['native_representation'] != 'generated_block_input' or
                        source['transform'] not in (('trail_layout',) if spec['transform'] in ('arm64_trail_update', 'arm64_trail_draw')
                                                     else ('record_indices',) if spec['transform'] == 'arm64_row_band_fill'
                                                     else ('banked_word_copy_value',) if spec['transform'] == 'arm64_banked_word_copy'
                                                     else ('coalesced_text_region',) if spec['transform'] in ('arm64_text_region_copy','arm64_text_region_clear')
                                                     else ('unsigned_values',) if spec['transform']=='arm64_coordinate_pair'
                                                     else ('text_field_metric',) if spec['transform']=='arm64_text_column_fragment'
                                                     else ('label_region_extent',) if spec['transform']=='arm64_label_region_clear'
                                                     else ('particle_pair_value',) if spec['transform'] == 'arm64_particle_pairs'
                                                     else ('paired_tile_copy_layout',) if spec['transform'] == 'arm64_paired_tile_copy'
                                                     else ('equal_unsigned',) if spec['transform'] == 'arm64_split_index_branch'
                                                     else ('equal_bit_field', 'packed_bit_fields') if spec['transform'] == 'arm64_selection_branch'
                                                     else ('particle_strip_layout',) if spec['transform'] == 'arm64_particle_strip'
                                                     else ('palette_pair_value',) if spec['transform'] in ('arm64_palette_boolean_value', 'arm64_palette_boolean_result','arm64_palette_optional_search')
                                                     else ('condition_cell_value',) if spec['transform'] == 'arm64_cell_condition'
                                                     else ('condition_choice_value',) if spec['transform'] == 'arm64_condition_choice'
                                                     else ('byte_choice_value',) if spec['transform'] in ('arm64_byte_choice', 'arm64_dimension_choice')
                                                     else ('clamped_phase_value',) if spec['transform'] == 'arm64_clamped_phase'
                                                     else ('palette_cycle_layout',) if spec['transform'] == 'arm64_palette_cycle'
                                                     else ('strided_row_layout',) if spec['transform'] == 'arm64_strided_row_fragment'
                                                     else ('unsigned_bit_fields',) if spec['transform'] in ('arm64_vector_fill','arm64_vector_rows','arm64_tagged_byte_constant')
                                                     else ('unsigned_values',) if spec['transform'] in ('arm64_short_fill', 'arm64_palette_initialization')
                                                     else ('number_choice_values',) if spec['transform'] == 'arm64_number_choice'
                                                     else ('caption_choice_value',) if spec['transform'] in ('arm64_caption_choice', 'arm64_flag_caption', 'arm64_bit_number_prefix')
                                                     else ('equal_bit_field',) if spec['transform'] in ('arm64_boolean_argument', 'arm64_inline_byte_append', 'arm64_stack_byte_argument')
                                                     else ('affine_copy_layout',) if spec['transform'] == 'arm64_affine_copy'
                                                     else ('tile_run_layout',) if spec['transform'] == 'arm64_tile_runs'
                                                     else ('cell_run_origins',) if spec['transform'] == 'arm64_cell_apertures'
                                                     else ('fixed_color_lengths', 'fixed_color_shades')) or
                        source.get('sites', []) != spec.get('sites', []) or
                        any(source[k] != spec[k] for k in ('offset', 'source_count', 'source_width',
                                                           'decoded_source', 'sha256'))):
                    raise ValueError('Generated block does not consume the declared ROM inputs')
                if spec['transform'] == 'arm64_particle_pairs' and (
                        len(block['inputs'])!=2 or source['native_width']!=4 or
                        source['count']!=1 or source['groups'][:1]!=spec['groups'] or
                        any(by_id[key]['transform']!='particle_pair_value' or
                            len(by_id[key]['groups'])!=2 for key in block['inputs']) or
                        {by_id[key]['groups'][1] for key in block['inputs']}!={0,1}):
                    raise ValueError('Paired particles differ from their declared layout')
                if spec['transform'] == 'arm64_coordinate_pair' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or
                        source['count'] != 2 or source['groups']):
                    raise ValueError('Coordinate generator differs from its declared pair')
                if spec['transform'] == 'arm64_text_column_fragment' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'] != spec['groups'][:3] + [1]):
                    raise ValueError('Text-column generator differs from its declared span')
                if spec['transform'] == 'arm64_row_band_fill' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or
                        source['count'] != 4 or source['transform'] != 'record_indices' or
                        source['groups'] != [spec['groups'][0],4,spec['groups'][1],0,4,spec['groups'][1],0,1,65536,0,1,65536]):
                    raise ValueError('Row-band generator differs from its declared inputs')
                if spec['transform'] == 'arm64_palette_initialization' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or
                        source['count'] != 18 or source['groups']):
                    raise ValueError('Palette initializer differs from its declared transfers')
                if spec['transform'] in ('arm64_text_region_copy','arm64_text_region_clear') and (
                        len(block['inputs'])!=1 or source['native_width']!=4 or source['count']!=3 or
                        source['groups']!=spec['groups'][:5]):
                    raise ValueError('Text region differs from its declared rectangles')
                if spec['transform']=='arm64_label_region_clear' and (
                        len(block['inputs'])!=1 or source['native_width']!=4 or source['count']!=2 or
                        source['groups']!=spec['groups']):
                    raise ValueError('Label region differs from its declared aliases')
                if spec['transform'] == 'arm64_paired_tile_copy' and (
                        len(block['inputs'])!=1 or source['native_width']!=4 or source['count']!=5 or
                        source['groups']!=spec['groups'][:2]):
                    raise ValueError('Paired tile copy differs from its declared intervals/destinations')
                if spec['transform'] == 'arm64_banked_word_copy' and (
                        len(block['inputs']) != 2 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'][:3] != spec['groups'][:3] or
                        not all(by_id.get(key,{}).get('transform') == 'banked_word_copy_value' and
                                isinstance(by_id[key].get('groups'), list) and len(by_id[key]['groups']) == 4
                                for key in block['inputs']) or
                        {by_id[key]['groups'][3] for key in block['inputs']} != {0,1}):
                    raise ValueError('Word copy differs from its declared destination/extent')
                if spec['transform'] == 'arm64_strided_row_fragment' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or source['count'] != 6 or
                        source['groups'] != spec['groups'][:4]):
                    raise ValueError('Row fragment differs from its declared layout')
                if spec['transform'] == 'arm64_selection_branch' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or
                        source['count'] != 1 or
                        source['groups'] != (spec['groups'][:2] if spec['groups'][6] == 1 else
                                              spec['groups'][:2]+[0]+spec['groups'][:2]+[8]) or
                        source['transform'] != ('equal_bit_field' if spec['groups'][6] == 1 else 'packed_bit_fields')):
                    raise ValueError('Selection branch differs from its declared choices')
                if spec['transform'] == 'arm64_split_index_branch' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or
                        source['count'] != 1 or source['groups']):
                    raise ValueError('Split-index branch differs from its declared boundary')
                if spec['transform'] in ('arm64_vector_fill','arm64_vector_rows') and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'] != [spec['groups'][0],16-spec['groups'][0]]):
                    raise ValueError('Vector fill differs from its declared transfer units')
                if spec['transform'] == 'arm64_short_fill' and (
                        source['native_width'] != 4 or source['count'] != 1 or source['groups']):
                    raise ValueError('Short-fill extent differs from its declared input')
                if spec['transform'] == 'arm64_number_choice' and (
                        len(block['inputs']) != 3 or source['native_width'] != 4 or
                        source['groups'][:9] != spec['groups'][:9] or
                        not all(by_id.get(key,{}).get('transform') == 'number_choice_values' and
                                isinstance(by_id[key].get('groups'), list) and len(by_id[key]['groups']) == 10
                                for key in block['inputs']) or
                        {by_id[key]['groups'][9] for key in block['inputs']} != {0,1,2} or
                        source['count'] != (source['source_count'] - 2*source['groups'][0] - 2
                                            if source['groups'][9] == 2 else 1)):
                    raise ValueError('Numbered call differs from its declared choices/positions')
                if spec['transform'] == 'arm64_bit_number_prefix' and (
                        len(block['inputs']) != 2 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'][:3] != spec['groups'][:3] or
                        not all(by_id.get(key,{}).get('transform') == 'caption_choice_value' and
                                isinstance(by_id[key].get('groups'), list) and len(by_id[key]['groups']) == 4
                                for key in block['inputs']) or
                        {by_id[key]['groups'][3] for key in block['inputs']} != {0,1}):
                    raise ValueError('Bit-number prefix differs from its declared palette choices')
                if spec['transform'] in ('arm64_caption_choice', 'arm64_flag_caption') and (
                        len(block['inputs']) != 3 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'][:3] != spec['groups'][:3] or
                        not all(by_id.get(key,{}).get('transform') == 'caption_choice_value' and
                                isinstance(by_id[key].get('groups'), list) and len(by_id[key]['groups']) == 4
                                for key in block['inputs']) or
                        {by_id[key]['groups'][3] for key in block['inputs']} != {0,1,2}):
                    raise ValueError('Caption call differs from its declared choices/scroll')
                if spec['transform'] == 'arm64_particle_strip' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or source['count'] != 4 or
                        source['groups'] != spec['groups'][:5]):
                    raise ValueError('Particle strip differs from its declared layout')
                if spec['transform'] in ('arm64_palette_boolean_value', 'arm64_palette_boolean_result','arm64_palette_optional_search') and (
                        len(block['inputs'])!=2 or source['native_width']!=4 or source['count']!=1 or
                        source['groups'][:8]!=spec['groups'][:8] or
                        not all(by_id.get(key,{}).get('transform')=='palette_pair_value' and
                                isinstance(by_id[key].get('groups'),list) and len(by_id[key]['groups'])==9
                                for key in block['inputs']) or
                        {by_id[key]['groups'][8] for key in block['inputs']}!={0,1}):
                    raise ValueError('Palette generator differs from its declared pair')
                if spec['transform'] == 'arm64_cell_condition' and (
                        len(block['inputs']) != 3 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'][:5] != spec['groups'][:5] or
                        not all(by_id.get(key,{}).get('transform') == 'condition_cell_value' and
                                isinstance(by_id[key].get('groups'),list) and len(by_id[key]['groups']) == 6
                                for key in block['inputs']) or
                        {by_id[key]['groups'][5] for key in block['inputs']} != {0,1,2}):
                    raise ValueError('Conditional cell differs from its declared inputs')
                if spec['transform'] == 'arm64_condition_choice' and (
                        len(block['inputs']) != 2 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'][:3] != spec['groups'][:3] or
                        not all(by_id.get(key,{}).get('transform') == 'condition_choice_value' and
                                isinstance(by_id[key].get('groups'),list) and len(by_id[key]['groups']) == 4
                                for key in block['inputs']) or
                        {by_id[key]['groups'][3] for key in block['inputs']} != {0,1}):
                    raise ValueError('Condition choice differs from its declared inputs')
                if spec['transform'] in ('arm64_byte_choice', 'arm64_dimension_choice') and (
                        len(block['inputs']) != 2 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'][:1] != spec['groups'][:1] or
                        not all(by_id.get(key,{}).get('transform') == 'byte_choice_value' and
                                isinstance(by_id[key].get('groups'),list) and len(by_id[key]['groups']) == 2
                                for key in block['inputs']) or
                        {by_id[key]['groups'][1] for key in block['inputs']} != {0,1}):
                    raise ValueError('Byte-choice fragment differs from its declared inputs')
                if spec['transform'] == 'arm64_clamped_phase' and (
                        len(block['inputs']) != 2 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'][:2] != spec['groups'][:2] or
                        not all(by_id.get(key,{}).get('transform') == 'clamped_phase_value' and
                                isinstance(by_id[key].get('groups'),list) and len(by_id[key]['groups']) == 3
                                for key in block['inputs']) or
                        {by_id[key]['groups'][2] for key in block['inputs']} != {0,1}):
                    raise ValueError('Clamped phase differs from its declared inputs')
                if spec['transform'] == 'arm64_palette_cycle' and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or source['count'] != 3 or
                        source['groups'] != spec['groups'][:2]):
                    raise ValueError('Palette cycle differs from its declared layout')
                if spec['transform'] in ('arm64_tagged_byte_constant', 'arm64_boolean_argument', 'arm64_inline_byte_append', 'arm64_stack_byte_argument') and (
                        len(block['inputs']) != 1 or source['native_width'] != 4 or source['count'] != 1 or
                        source['groups'] != spec['groups'][:2]):
                    raise ValueError('Byte/Boolean argument differs from its declared input')
                if spec['transform'] == 'arm64_cell_apertures' and (
                        source['groups'] != spec['groups'][:6] or
                        source['count'] != (spec['source_count'] - spec['groups'][5]) // spec['groups'][4]):
                    raise ValueError('Generated aperture layout differs from its declared input')
    validate_aliases(specs, doc.get('aliases', []))
    return doc


def validate_aliases(specs, aliases):
    """Only single words may alias directly covered, independently extracted data.

    This describes compiler pooling, not interchangeable game resource identities.
    Chains, cycles and uncovered targets cannot confer removal coverage.
    """
    if not isinstance(aliases, list) or len(aliases) > 4096:
        raise ValueError('Invalid native alias count')
    by_id = {row['id']: row for row in specs}
    seen = set()
    for alias in aliases:
        if (not isinstance(alias, dict) or
                set(alias) != {'material', 'target', 'first', 'count'} or
                not isinstance(alias['material'], str) or not isinstance(alias['target'], str)):
            raise ValueError('Invalid native alias metadata')
        source, target = by_id.get(alias['material']), by_id.get(alias['target'])
        if (source is None or target is None or alias['material'] in seen or
                alias['material'] == alias['target'] or
                (source['count'], source['native_width']) != (1, 4) or
                target['native_width'] != 4 or
                source['native_representation'] != 'reviewed_fields' or
                target['native_representation'] != 'reviewed_fields' or
                type(alias['first']) is not int or type(alias['count']) is not int or
                not 0 <= alias['first'] < target['count'] or
                not 0 < alias['count'] <= target['count'] - alias['first']):
            raise ValueError('Invalid native alias source/range')
        seen.add(alias['material'])
    if any(alias['target'] in seen for alias in aliases):
        raise ValueError('Native alias chains are not permitted')


def material_field_coverage(specs, profiles, binary=None):
    """Cover selected values by fields or an explicitly whole-value lowering.

    A Boolean scale can disappear into an instruction's opcode rather than an
    immediate. Its generator replaces the entire argument and call; a numeric
    singleton such as a fill extent still needs its separate scalar fields.
    Coverage is not proof that no other compiled consumer exists; source/owner
    review and generated-byte comparison remain required.
    """
    coverage = {row['id']: set() for row in specs}
    for profile in profiles.get('profiles', []):
        if binary is not None and profile.get('binary', binary) != binary:
            continue
        for field in profile['fields']:
            coverage[field['material']].update(
                field['first'] + i * field.get('source_stride', 1) for i in range(field['count']))
    by_id = {row['id']: row for row in specs}
    for block in profiles.get('blocks', []):
        if all(by_id[identity]['transform']=='arm64_label_region_clear' for identity in block['material'].values()):
            for identity in block['inputs']:
                coverage[identity].add(0)
            continue
        if all(by_id[identity]['transform'] in ('arm64_coordinate_pair', 'arm64_text_column_fragment', 'arm64_tagged_byte_constant', 'arm64_inline_byte_append', 'arm64_stack_byte_argument', 'arm64_boolean_argument', 'arm64_condition_choice', 'arm64_cell_condition', 'arm64_palette_boolean_value', 'arm64_palette_boolean_result', 'arm64_palette_optional_search', 'arm64_dimension_choice', 'arm64_caption_choice', 'arm64_flag_caption', 'arm64_bit_number_prefix', 'arm64_palette_initialization', 'arm64_banked_word_copy', 'arm64_paired_tile_copy', 'arm64_particle_pairs', 'arm64_text_region_copy', 'arm64_text_region_clear', 'arm64_label_region_clear', 'arm64_split_index_branch', 'arm64_selection_branch', 'arm64_byte_choice', 'arm64_clamped_phase', 'arm64_palette_cycle', 'arm64_row_band_fill')
               for identity in block['material'].values()):
            for identity in block['inputs']:
                coverage[identity].update(range(by_id[identity]['count']))
        if all(by_id[identity]['transform'] == 'arm64_particle_strip'
               for identity in block['material'].values()):
            kinds = {by_id[identity]['groups'][7] for identity in block['material'].values()}
            records = {tuple(by_id[identity]['groups'][5:7]) for identity in block['material'].values()}
            if len(kinds) != 1 or len(records) != 1:
                raise ValueError('Particle operations differ across executables')
            used = {0:(0,1,2,3), 1:(0,1,2), 2:(0,3), 3:(0,1,2,3)}[kinds.pop()]
            for identity in block['inputs']:
                coverage[identity].update(used)
        if all(by_id[identity]['transform'] == 'arm64_strided_row_fragment'
               for identity in block['material'].values()):
            operations = {by_id[identity]['groups'][4] for identity in block['material'].values()}
            if len(operations) != 1:
                raise ValueError('Row fragment operations differ across executables')
            used = {0: (0,2), 1: (3,4,5), 2: (0,2,3)}[operations.pop()]
            for identity in block['inputs']:
                coverage[identity].update(used)
        if all(by_id[identity]['transform'] == 'arm64_number_choice'
               for identity in block['material'].values()):
            for identity in block['inputs']:
                if by_id[identity]['groups'][9] < 2:
                    coverage[identity].add(0)
                else:
                    for output in block['material'].values():
                        index = by_id[output]['groups'][9]
                        coverage[identity].update((index*2,index*2+1))
    aliases = profiles.get('aliases', [])
    validate_aliases(specs, aliases)
    for alias in aliases:
        selected = set(range(alias['first'], alias['first'] + alias['count']))
        if not selected <= coverage[alias['target']]:
            raise ValueError('Native alias target lacks direct field coverage')
        coverage[alias['material']].add(0)
    return coverage


def validate_material_alias_values(specs, materials, profiles):
    aliases = profiles.get('aliases', [])
    validate_aliases(specs, aliases)
    if len(materials) != len(specs):
        raise ValueError('Native alias material extent mismatch')
    values = {row['id']: value for row, value in zip(specs, materials)}
    sizes = {row['id']: row['count'] * row['native_width'] for row in specs}
    for alias in aliases:
        source, target = values[alias['material']], values[alias['target']]
        if len(source) != 4 or len(target) != sizes[alias['target']]:
            raise ValueError('Native alias input extent mismatch')
        for i in range(alias['first'], alias['first'] + alias['count']):
            if source != target[i * 4:(i + 1) * 4]:
                raise ValueError('Independent ROM operands disagree at a pooled native field')


def block_plan(path, image, specs, materials, profiles):
    """Replace a whole native fragment, including content-dependent code shape.

    The extractor independently generates it from ROM data. This routine only
    authenticates its linked owner/location; it never copies old code to a map.
    """
    blocks = profiles.get('blocks', []) if profiles else []
    if not blocks:
        return []
    symbols = symbol_offsets(path, image)
    by_id = {row['id']: i for i, row in enumerate(specs)}
    result = []
    for block in blocks:
        base = symbols.get(block['symbol'])
        material = by_id.get(block['material'].get(Path(path).name))
        if base is None or material is None:
            raise ValueError('Generated native owner or material is unavailable')
        start = base + block['start']
        stop = start + block['size']
        expected = materials[material]
        if (start % 4 or stop > len(image) or len(expected) != block['size'] or
                hashlib.sha256(image[start:stop]).hexdigest() != block['sha256'][Path(path).name] or
                image[start:stop] != expected):
            raise ValueError('Generated native block differs from reviewed compiler output')
        result.append((start, material))
    result.sort()
    if any(at < previous + len(materials[m]) for (previous, m), (at, _) in zip(result, result[1:])):
        raise ValueError('Overlapping generated native blocks')
    return result


def field_value(data, element_width, field):
    if element_width not in (1, 4) or len(data) % element_width:
        raise ValueError('Invalid native field input width')
    first, count = field['first'], field['count']
    stride = field.get('source_stride', 1)
    projection = field.get('projection', 'sum')
    if (first < 0 or count <= 0 or not 0 < stride <= 65536 or
            first + (count - 1) * stride >= len(data) // element_width):
        raise ValueError('Invalid native field input extent')
    values = [int.from_bytes(data[i * element_width:(i + 1) * element_width], 'little')
              for i in range(first, first + count * stride, stride)]
    if projection in ('scaled_quotient_shift', 'scaled_quotient_mask32'):
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) !=
                (2,1,0,0,16 if projection == 'scaled_quotient_shift' else 10,
                 6 if projection == 'scaled_quotient_shift' else 13) or
                not 2 <= values[0] <= 255 or not 0 < values[1] <= 256 or values[1] & (values[1]-1)):
            raise ValueError('Invalid scaled quotient field')
        divisor, scale = values
        for bits in range(17):
            reciprocal = ((1 << bits) + divisor - 1) // divisor
            if all(n * reciprocal // (1 << bits) == n // divisor for n in range(256)):
                break
        else:
            raise ValueError('No exact byte reciprocal')
        shift = bits - (scale.bit_length() - 1)
        if shift < 0:
            raise ValueError('Scaled quotient requires a negative shift')
        if projection == 'scaled_quotient_shift':
            return shift << 16
        mask = ((1 << (255 // divisor).bit_length()) - 1) * scale
        return field_value(mask.to_bytes(4, 'little'), 4,
                           dict(field,first=0,count=1,source_stride=1,projection='logical_mask32'))
    if projection == 'relative_membership_halfword':
        if (not 1 <= len(values) <= 64 or not 0 <= field['addend'] < 4 or
                (field['multiplier'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                (1,0,5,16) or max(values)-min(values) >= 64):
            raise ValueError('Invalid relative membership halfword')
        # The lane is native MOVZ/MOVK format metadata. Both the origin and
        # every selected bit derive from supplied identities, not the profile.
        origin = min(values)
        mask = 0
        for value in values:
            mask |= 1 << (value-origin)
        return ((mask >> (16*field['addend'])) & 65535) << 5
    if projection == 'adjacent_power_of_two_shift':
        if (not 2 <= len(values) <= 16 or
                (field['multiplier'],field['addend'],field['divisor_shift'],
                 field['destination_shift'],field['width']) != (1,0,0,5,16) or
                not 0 < values[0] <= 0x40000000 or values[0] & (values[0]-1) or
                any(v != values[0] for v in values[1:-1]) or values[-1] != 2*values[0]):
            raise ValueError('Native conditional shift requires ordered adjacent powers of two')
        # Both original rates participate in the shift/conditional-increment fold.
        return (values[0].bit_length()-1) << 5
    if projection == 'halfword_lane_shift64':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (1,1,0,0,10,6) or
                not 0 <= values[0] <= 0xffffffff):
            raise ValueError('Invalid packed halfword shift field')
        return ((values[0] % 4) * 16) << 10
    if projection == 'halfword_pair_residue64':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (2,1,0,0,10,13) or
                any(not 0 <= value <= 0xffffffff for value in values) or
                values[0] // 4 != values[1] // 4):
            raise ValueError('Paired halfwords must share one native word')
        lanes = [value % 4 for value in values]
        if 0 in lanes or lanes[0] == lanes[1]:
            raise ValueError('Paired halfwords must be distinct nonzero lanes')
        # A separate low-halfword copy preserves lane zero. The mask keeps the
        # other untouched lane; both overwritten lanes come from ROM inputs.
        remaining = next(lane for lane in range(1,4) if lane not in lanes)
        rotation = (-16 * remaining) & 63
        return (4096 | (rotation << 6) | 15) << 10
    if projection == 'halfword_lane_insert64':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (1,1,0,0,10,12) or
                not 0 <= values[0] < 4):
            raise ValueError('Native halfword insertion requires a lane within one 64-bit word')
        # BFM rotates the low halfword into the selected destination lane.
        # Encode both immediates; the surrounding opcode/registers are native.
        rotation = (-16 * values[0]) & 63
        return ((rotation << 6) | 15) << 10
    if projection == 'scaled_byte_logical64':
        scale = field['multiplier']
        if ((len(values),field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                (1,0,0,10,13) or not 0 < scale <= 65536 or scale & (scale-1) or
                not 0 < values[0] <= 255):
            raise ValueError('Invalid scaled-byte logical field')
        value = values[0] * scale
        trailing = (value & -value).bit_length()-1
        run = value >> trailing
        if run & (run+1):
            raise ValueError('Scaled byte is not one contiguous logical run')
        return ((1 << 12) | (((64-trailing) & 63) << 6) | (run.bit_length()-1)) << 10
    if projection == 'word_bound_previous_units':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (1,1,0,0,10,12) or
                not 0 < values[0] <= 65535):
            raise ValueError('Invalid preceding word-bound field')
        shift = (values[0] & -values[0]).bit_length()-1
        previous = (values[0] >> shift)-1
        if previous >= 4096:
            raise ValueError('Preceding bound exceeds its native field')
        return previous << 10
    if projection == 'opposite_byte_complement':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (2,1,0,0,5,16) or
                not 128 < values[0] <= 255 or not 0 < values[1] < 128 or
                values[0] + values[1] != 256):
            raise ValueError('Native conditional negation requires opposite nonzero byte values')
        # MOVN's complemented magnitude followed by CNEG yields either source
        # byte. Validate both operands; a missing endpoint cannot use this fold.
        return (255 - values[0]) << 5
    if projection in ('scaled_floor_bias', 'simd_scaled_floor_bias'):
        if (not 1 <= len(values) <= 16 or
                (field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                (1,0,0,5,16 if projection == 'scaled_floor_bias' else 14) or
                any(not 1 <= scale <= 255 for scale in values)):
            raise ValueError('Invalid fixed-eight product scales')
        combined = 0
        for scale in values:
            combined |= scale
        bias = 256 - (combined & -combined)
        if projection == 'scaled_floor_bias':
            return bias << 5
        return ((bias & 31) | ((bias >> 5) << 11) | 32) << 5
    if projection == 'two_bit_scaled_right_shift':
        if ((len(values),field['multiplier'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                (1,1,0,10,12) or not 0 < field['addend'] < 32 or bin(values[0]).count('1') != 2):
            raise ValueError('Invalid scaled extraction field')
        low = (values[0] & -values[0]).bit_length()-1
        if low > field['addend']:
            raise ValueError('Product alignment exceeds fractional width')
        return (((field['addend']-low) << 6) | (31-low)) << 10
    if projection == 'byte_power_of_two_bit_select':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                (1,1,0,0,10,12) or not 0 < values[0] <= 128 or values[0] & (values[0]-1)):
            raise ValueError('Native byte bit selection requires one byte power of two')
        bit = values[0].bit_length()-1
        # A64 UBFM for (zero-extended byte / period) & 1. At the byte's top
        # bit the compiler may omit the mask, choosing the word-wide LSR alias.
        return ((bit << 6) | (31 if bit == 7 else bit)) << 10
    if projection == 'nonnegative_byte_insert_width':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                (1,1,0,0,10,6) or values[0] > 127):
            raise ValueError('Native byte insertion lacks a nonnegative bound')
        # This lowering only removes the byte sign bit. It does not trim every
        # leading zero proved by a smaller bound. Larger bounds change lowering.
        return (8-1-1) << 10
    if projection == 'structure_load_opcode':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],field['destination_shift'],field['width']) !=
                (1,1,0,0,12,4) or not 1 <= values[0] <= 4):
            raise ValueError('Invalid native interleaved load stride')
        # Encoding metadata for A64 AdvSIMD multiple-structure loads.
        return (7,8,4,0)[values[0]-1] << 12
    if projection == 'zero_register':
        if ((len(values),field['width']) != (1,5) or field['destination_shift'] not in (0,5,16) or
                not 0 < field['multiplier'] <= 65536 or not 0 <= field['divisor_shift'] <= 16 or
                not -(1 << 31) <= field['addend'] < (1 << 31) or
                values[0] * field['multiplier'] + field['addend'] != 0):
            raise ValueError('Native zero-register source is not exactly zero')
        # A64 register 31 supplies zero to reviewed Rt, Rn or Rm operands. It is
        # an ISA encoding, not a retained game choice. Nonzero values require
        # another lowering; never turn a selected value into a live register.
        return 31 << field['destination_shift']
    if projection in ('corrected_reciprocal_low16', 'corrected_reciprocal_high16',
                      'corrected_reciprocal_shift'):
        shift_field = projection == 'corrected_reciprocal_shift'
        if ((len(values), field['multiplier'], field['addend'], field['divisor_shift'],
             field['destination_shift'], field['width']) !=
                (1,1,0,0,16 if shift_field else 5,6 if shift_field else 16) or
                not 2 <= values[0] <= 256 or values[0] & (values[0] - 1) == 0):
            raise ValueError('Invalid corrected reciprocal field')
        divisor = values[0]
        shift = divisor.bit_length() - 1
        scale = 1 << (33 + shift)
        full = (scale + divisor - 1) // divisor
        if not (1 << 32) < full < (1 << 33):
            raise ValueError('Corrected reciprocal multiplier has no native representation')
        multiplier = full - (1 << 32)
        def quotient(n):
            high = (n * multiplier) >> 32
            return (high + ((n - high) >> 1)) >> shift
        # The rounded-up reciprocal cannot underestimate. At a fixed remainder,
        # its excess increases with the quotient. Check the last full interval
        # and the final partial interval to prove the entire uint32 domain.
        last = 0xffffffff // divisor
        if quotient(last * divisor - 1) != last - 1 or quotient(0xffffffff) != last:
            raise ValueError('Corrected reciprocal is inexact over the uint32 domain')
        value = (shift if shift_field else multiplier & 65535
                 if projection == 'corrected_reciprocal_low16' else multiplier >> 16)
        return value << field['destination_shift']
    if projection in ('word_bound_shift', 'word_bound_units'):
        shift_field = projection == 'word_bound_shift'
        if ((len(values), field['multiplier'], field['addend'], field['divisor_shift'],
             field['destination_shift'], field['width']) !=
                (1,1,0,0,16 if shift_field else 10,6 if shift_field else 12) or
                not 0 < values[0] <= 65535):
            raise ValueError('Invalid shifted word-bound field')
        # Discarding bits below the bound's alignment preserves unsigned
        # comparison. Restore both the extraction shift and normalized bound.
        shift = (values[0] & -values[0]).bit_length() - 1
        value = shift if shift_field else values[0] >> shift
        if value >= 1 << field['width']:
            raise ValueError('Shifted word bound exceeds its native field')
        return value << field['destination_shift']
    if projection in ('simd_affine_byte', 'simd_affine_halfword_byte', 'simd_affine_halfword_clear_byte'):
        if (len(values),field['destination_shift'],field['width']) != (1,5,14):
            raise ValueError('Invalid affine SIMD byte field')
        if not 0 < field['multiplier'] <= 65536 or not 0 <= field['divisor_shift'] <= 16:
            raise ValueError('Invalid affine SIMD arithmetic')
        value = values[0] * field['multiplier'] + field['addend']
        divisor = 1 << field['divisor_shift']
        if value < 0 or value % divisor or value // divisor > 255:
            raise ValueError('Affine SIMD result is not exactly one byte')
        value //= divisor
        mode = 1056 if projection == 'simd_affine_halfword_byte' else 1184 if projection == 'simd_affine_halfword_clear_byte' else 32
        return ((value & 31) | ((value >> 5) << 11) | mode) << 5
    if projection == 'byte_mask_add_logical64':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (2,1,0,0,10,13) or
                any(v > 255 for v in values) or values[0] ^ values[1] != 255 or
                not values[1]):
            raise ValueError('Native byte mask/add fold requires complementary byte inputs')
        bias = values[1]
        trailing = (bias & -bias).bit_length() - 1
        run = bias >> trailing
        if run & (run + 1):
            raise ValueError('Native byte bias has no 64-bit logical-immediate representation')
        # For a zero-extended byte, complementary mask/add is equivalent to OR.
        # A nonzero byte in a 64-bit result has one nonrepeated run (N=1).
        return ((1 << 12) | (((64-trailing) & 63) << 6) | (run.bit_length()-1)) << 10
    if projection == 'two_bit_scale_long_shift':
        if ((len(values),field['multiplier'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (1,1,0,10,12) or
                not 0 <= field['addend'] < 64 or
                not 0 < values[0] <= 0xffffffff or bin(values[0]).count('1') != 2):
            raise ValueError('Invalid native two-bit long-shift input')
        shift = (values[0] & -values[0]).bit_length()-1 + field['addend']
        if shift >= 64:
            raise ValueError('Native packed scale exceeds long-shift width')
        return ((((64-shift) & 63) << 6) | (63-shift)) << 10
    if projection in ('two_bit_scale_word_shift', 'two_bit_scale_add_shift'):
        width = 12 if projection == 'two_bit_scale_word_shift' else 6
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (1,1,0,0,10,width) or
                not 0 < values[0] <= 0xffffffff or bin(values[0]).count('1') != 2):
            raise ValueError('Invalid native two-bit scale domain/field')
        low = (values[0] & -values[0]).bit_length()-1
        high = values[0].bit_length()-1
        if projection == 'two_bit_scale_add_shift':
            return (high-low) << 10
        return ((((32-low) & 31) << 6) | (31-low)) << 10
    if projection in ('difference_scale_word_shift', 'difference_scale_subtract_shift'):
        width = 12 if projection == 'difference_scale_word_shift' else 6
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (1,1,0,0,10,width) or
                not 0 < values[0] < 0x80000000):
            raise ValueError('Invalid native difference-scale domain/field')
        scale = values[0]
        low = (scale & -scale).bit_length()-1
        run = scale >> low
        if run < 3 or run & (run+1):
            raise ValueError('Difference scale requires one run of at least two bits')
        high = scale.bit_length()
        # Encode only the two data fields used by the native compiler's
        # unsigned shift/subtract multiplication; no selected factor is stored.
        if projection == 'difference_scale_subtract_shift':
            return low << 10
        return (((32-high) << 6) | (31-high)) << 10
    if projection == 'power_of_two_plus_one_shift':
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift'],
             field['destination_shift'],field['width']) != (1,1,0,0,10,6) or
                values[0] < 2 or (values[0]-1) & (values[0]-2)):
            raise ValueError('Native add-shift scale requires a power of two plus one')
        return ((values[0]-1).bit_length()-1) << 10
    if projection in ('power_of_two_word_left_shift', 'power_of_two_long_left_shift',
                      'power_of_two_nonnegative_insert32', 'power_of_two_halfword_left_shift'):
        if ((len(values), field['multiplier'], field['addend'],
             field['destination_shift'], field['width']) != (1,1,0,10,12) or
                not 0 <= field['divisor_shift'] <= 16 or
                not 0 < values[0] <= 0x80000000 or values[0] & (values[0] - 1) or
                values[0] < (1 << field['divisor_shift'])):
            raise ValueError('Native word left shift requires an exactly divisible power-of-two scale')
        # Factoring a record-index bit into its scale must remain exact.
        exponent = values[0].bit_length() - 1 - field['divisor_shift']
        # Restore both UBFM operands. Keeping the source-dependent mask length
        # would retain the selected scale even with its rotation removed.
        bits = 64 if projection == 'power_of_two_long_left_shift' else 32
        preserved_high = (16 if projection == 'power_of_two_halfword_left_shift'
                          else int(projection == 'power_of_two_nonnegative_insert32'))
        if exponent + preserved_high >= bits:
            raise ValueError('Native insert has no retained destination bits')
        # BFI of a nonnegative scaled index preserves destination bit 31 and
        # the low origin bits. A narrowed product keeps only the low halfword.
        # Neither data-dependent operand may be retained.
        return ((((bits - exponent) & (bits - 1)) << 6) |
                (bits - 1 - preserved_high - exponent)) << 10
    if projection == 'equal_power_of_two_left_shift32':
        if ((field['multiplier'], field['addend'], field['divisor_shift'],
             field['destination_shift'], field['width']) != (1,0,0,16,5) or
                not 0 < values[0] <= 0x80000000 or values[0] & (values[0] - 1) or
                any(value != values[0] for value in values)):
            raise ValueError('Native left shift requires equal power-of-two scales')
        return ((33 - values[0].bit_length()) & 31) << 16
    if projection in ('byte_period_bias', 'byte_period_mask32'):
        if len(values) != 2 or not 1 <= min(values) < max(values) <= 255 or any(v % 2 == 0 for v in values):
            raise ValueError('Native period rounding requires two distinct odd byte rates')
        upper = 1 << max(values).bit_length()
        if projection == 'byte_period_bias':
            values = [(upper - 1) * 256]
        else:
            changes = (values[0] - 1) | (values[1] - 1)
            alignment = changes & -changes
            values = [(upper - alignment) * 256]
    if projection in ('range_alignment_mask32', 'domain_complement_mask32', 'logical_mask32', 'byte_bound_mask32', 'word_bound_mask32', 'period_mask32', 'repeated_byte_logical_immediate', 'byte_period_mask32'):
        if projection == 'range_alignment_mask32':
            if ((len(values),field['multiplier'],field['divisor_shift'],field['width']) != (2,1,0,13) or
                    not 1 < field['addend'] < 32 or not 0 <= field['destination_shift'] <= 19):
                raise ValueError('Invalid native range-alignment projection')
            upper, lower = values
            domain = (1 << field['addend']) - 1
            if not 0 <= lower < upper <= domain:
                raise ValueError('Native range requires ordered endpoints within its unsigned domain')
            # Only bits below both endpoints' common alignment are irrelevant
            # to membership. The width is format metadata; both ends are data.
            combined = upper | lower
            values = [domain & ~((combined & -combined) - 1)]
        elif projection == 'domain_complement_mask32':
            if ((len(values),field['divisor_shift'],field['width']) != (1,0,13) or
                    not 0 < field['multiplier'] <= 65536 or field['multiplier'] & (field['multiplier']-1) or
                    not 0 < field['addend'] < 32 or not 0 <= field['destination_shift'] <= 19 or
                    field['multiplier'].bit_length()-1 > 32-field['addend']):
                raise ValueError('Invalid native domain-mask projection')
            domain = (1 << field['addend'])-1
            if not 0 < values[0] <= domain or values[0] & (values[0]-1):
                raise ValueError('Native domain mask requires one selected bit within its format')
            # The selected bit comes from data; width/scale describe its format.
            values = [(domain ^ values[0]) * field['multiplier']]
        elif ((len(values), field['multiplier'], field['addend'], field['divisor_shift'], field['width']) !=
                (1, 1, 0, 0, 13) or not 0 <= field['destination_shift'] <= 19):
            raise ValueError('Invalid native logical mask projection')
        if projection == 'period_mask32':
            period = values[0]
            if not 1 < period <= 0x80000000 or period & (period - 1):
                raise ValueError('Native period mask requires a power-of-two period')
            values = [period - 1]
        if projection in ('byte_bound_mask32', 'word_bound_mask32'):
            bound = values[0]
            domain = 255 if projection == 'byte_bound_mask32' else 65535
            if not 0 < bound <= domain:
                raise ValueError('Native comparison has an invalid bound')
            # Bits below the bound's alignment cannot affect unsigned comparison.
            values = [domain & ~((bound & -bound) - 1)]
        if projection == 'repeated_byte_logical_immediate':
            if values[0] > 255:
                raise ValueError('Native repeated value is not a byte')
            # Elements at most eight bits use the same fields for 32/64-bit
            # destinations (N=0); only the caller's native opcode differs.
            values = [values[0] * 0x01010101]
        # Encode a repeated rotated run, choosing the smallest element size.
        # This is AArch64 data relocation, not original machine-code execution.
        for size in (2, 4, 8, 16, 32):
            mask = (1 << size) - 1
            for ones in range(1, size):
                run = (1 << ones) - 1
                for rotation in range(size):
                    element = ((run >> rotation) | (run << (size - rotation))) & mask
                    repeated = sum(element << bit for bit in range(0, 32, size))
                    if repeated == values[0]:
                        imms = ((~(size - 1) << 1) | (ones - 1)) & 63
                        return ((rotation << 6) | imms) << field['destination_shift']
        raise ValueError('Native mask has no 32-bit logical-immediate representation')
    if projection == 'pair_clear_bit32':
        if ((len(values), field['multiplier'], field['addend'], field['divisor_shift'], field['width']) !=
                (2, 1, 0, 0, 13) or not 0 <= field['destination_shift'] <= 19):
            raise ValueError('Invalid native pair-mask projection')
        difference = values[0] ^ values[1]
        if not difference or difference & (difference - 1) or values[0] & difference:
            raise ValueError('Native folded pair does not differ by one added bit')
        return (30 | ((32 - difference.bit_length()) << 6)) << field['destination_shift']
    if projection == 'repeated_bit16':
        value = values[0]
        if ((field['multiplier'], field['addend'], field['divisor_shift'], field['width']) != (1, 0, 0, 13) or
                not 0 < value <= 65535 or value & (value - 1) or any(v != value for v in values)):
            raise ValueError('Native shared logical mask disagrees with source values')
        return (32 | (((16 - (value.bit_length() - 1)) & 15) << 6)) << field['destination_shift']
    if projection not in ('sum', 'pair_product_sum', 'bit_indices32', 'packed_bytes', 'complement_byte', 'complement_word', 'low_mask_width', 'power_of_two_shift', 'unsigned_bit_width', 'power_of_two_fixed8_shift', 'power_of_two_fixed8_bias', 'equal_values',
                          'consecutive_first', 'color_math_layers', 'color_math_mode',
                          'negative_fixed8_shift', 'negative_fixed8_rounding',
                          'unsigned_reciprocal_low16', 'unsigned_reciprocal_high16',
                          'unsigned_reciprocal_shift', 'unsigned_reciprocal_narrow_shift', 'composite_covered_mode', 'composite_uncovered_mode',
                          'simd_movi_byte', 'simd_right_shift32', 'palette_color_offset', 'byte_period_bias',
                          'unsigned_difference', 'simd_difference_byte',
                          'byte_reciprocal_low16', 'byte_reciprocal_high16',
                          'byte_min_reciprocal_multiplier', 'byte_min_reciprocal_shift', 'bg2_priority_depth',
                          'incremented_byte_reciprocal16', 'sum_low_byte_complement', 'power_of_two_remainder'):
        raise ValueError('Unknown native field projection')
    if projection in ('unsigned_difference', 'simd_difference_byte'):
        if len(values) != 2 or values[0] < values[1]:
            raise ValueError('Native difference requires two ordered unsigned endpoints')
        total = values[0] - values[1]
        if projection == 'simd_difference_byte':
            if (total > 255 or (field['multiplier'], field['addend'], field['divisor_shift'],
                    field['destination_shift'], field['width']) != (1,0,0,5,14)):
                raise ValueError('Invalid native SIMD endpoint difference')
            total = (total & 31) | ((total >> 5) << 11) | 32
    elif projection == 'palette_color_offset':
        # Selected original palette destination plus the two caption-row tile
        # attributes. Native Palette records have 70 bytes; each color has two.
        # These layout rules are metadata, not stored authored selections.
        if len(values) != 3 or any(value > 65535 for value in values):
            raise ValueError('Native palette address requires three word inputs')
        destination, top, bottom = values
        group = (top >> 10) & 7
        if (not 0x3049 <= destination < 0x3049 + 8 * 32 or
                (destination - 0x3049) % 32 or group >= 4 or
                ((bottom >> 10) & 7) != group):
            raise ValueError('Native palette address exceeds its bank or rows disagree')
        total = ((destination - 0x3049) // 32) * 70 + group * 8
    elif projection in ('color_math_layers', 'color_math_mode'):
        # Encode the native renderer's layer pair and ColorBlend enum from a
        # selected ROM preset. These are format rules, not stored presets or
        # an original rendering program. Both fields authenticate all inputs.
        if len(values) % 3 or any(value > 255 for value in values):
            raise ValueError('Invalid native color-math preset')
        projected = []
        for i in range(0, len(values), 3):
            operation, affected, operand = values[i:i + 3]
            if operation not in (0, 2, 32) or affected & 8 or operand & 232:
                raise ValueError('Invalid native color-math preset')
            if projection == 'color_math_layers':
                projected.append((operand if operation & 2 else 0) | ((affected & 55) << 8))
            else:
                projected.append(0 if not (operation or affected or operand)
                                 else 1 + (affected >> 7) + 2 * ((affected >> 6) & 1))
        if any(value != projected[0] for value in projected):
            raise ValueError('Native shared color-math field has unequal source values')
        total = projected[0]
    elif projection in ('composite_covered_mode', 'composite_uncovered_mode'):
        if len(values) != 2 or any(value > 255 for value in values):
            raise ValueError('Native composite requires two byte flags')
        fixed = not values[0] & 2
        half = bool(values[1] & 64) and (fixed or projection == 'composite_covered_mode')
        total = 1 + (values[1] >> 7) + 2*half
    elif projection == 'bit_indices32':
        if any(value >= 32 for value in values):
            raise ValueError('Native membership index exceeds its word')
        total = 0
        for value in values:
            total |= 1 << value
    elif projection == 'packed_bytes':
        if len(values) > 4 or any(value > 255 for value in values):
            raise ValueError('Native packed field requires at most four byte values')
        total = sum(value << (8 * i) for i, value in enumerate(values))
    elif projection in ('complement_byte', 'complement_word'):
        mask = 255 if projection == 'complement_byte' else 65535
        if len(values) != 1 or values[0] > mask:
            raise ValueError('Native complement requires one value within its width')
        total = mask - values[0]
    elif projection == 'simd_movi_byte':
        if (len(values),field['multiplier'],field['addend'],field['divisor_shift'],
                field['destination_shift'],field['width']) != (1,1,0,0,5,14) or values[0] > 255:
            raise ValueError('Invalid native SIMD byte immediate')
        total = (values[0] & 31) | ((values[0] >> 5) << 11) | 32
    elif projection == 'unsigned_bit_width':
        if (len(values) != 1 or not 0 < field['width'] <= 24 or
                not 0 <= field['destination_shift'] <= 32-field['width'] or
                not 0 < field['multiplier'] <= 65536 or
                not -(1 << 31) <= field['addend'] < (1 << 31) or
                not 0 <= field['divisor_shift'] <= 16):
            raise ValueError('Invalid native result-width input or arithmetic')
        total = values[0].bit_length()
    elif projection in ('power_of_two_fixed8_shift', 'power_of_two_fixed8_bias'):
        if (len(values) != 1 or not 0 < values[0] <= 256 or values[0] & (values[0]-1) or
                not 0 < field['width'] <= 24 or not 0 <= field['destination_shift'] <= 32-field['width'] or
                not 0 < field['multiplier'] <= 65536 or not -(1 << 31) <= field['addend'] < (1 << 31) or
                not 0 <= field['divisor_shift'] <= 16):
            raise ValueError('Native fixed-eight scale requires one fractional power of two')
        # Unsigned fixed-eight scaling folds to division by a power of two.
        # Ceiling division also needs the period-minus-one rounding bias.
        total = (9 - values[0].bit_length() if projection == 'power_of_two_fixed8_shift'
                 else 256 // values[0] - 1)
    elif projection == 'power_of_two_shift':
        if len(values) != 1 or not 0 < values[0] <= 0x80000000 or values[0] & (values[0] - 1):
            raise ValueError('Native division shift requires a power-of-two period')
        total = values[0].bit_length() - 1
    elif projection in ('low_mask_width', 'simd_right_shift32'):
        if len(values) != 1 or values[0] & (values[0] + 1):
            raise ValueError('Native shift source is not a contiguous low-bit mask')
        total = values[0].bit_length()
        if projection == 'simd_right_shift32':
            if (not 1 <= total <= 32 or
                    (len(values),field['multiplier'],field['addend'],field['divisor_shift'],
                     field['destination_shift'],field['width']) != (1,1,0,0,16,6)):
                raise ValueError('Invalid native SIMD word shift')
            total = 64 - total
    elif projection in ('negative_fixed8_shift', 'negative_fixed8_rounding'):
        if len(values) != 1 or not 65280 <= values[0] <= 65535:
            raise ValueError('Native fixed-point shift requires a negative fractional word')
        magnitude = 65536 - values[0]
        if magnitude & (magnitude - 1):
            raise ValueError('Native fixed-point shift is not an exact power of two')
        total = 9 - magnitude.bit_length()
        if projection == 'negative_fixed8_rounding':
            total = (1 << total) - 1
    elif projection in ('byte_min_reciprocal_multiplier', 'byte_min_reciprocal_shift'):
        if len(values) != 1 or not 2 <= values[0] <= 256:
            raise ValueError('Native byte divisor exceeds its domain')
        divisor = values[0]
        # Use the smallest exact scale for a byte-masked numerator. Derive
        # both fields from the ROM divisor rather than storing a magic pair.
        for shift in range(17):
            multiplier = ((1 << shift) + divisor - 1) // divisor
            if all((value * multiplier >> shift) == value // divisor for value in range(256)):
                total = multiplier if projection == 'byte_min_reciprocal_multiplier' else shift
                break
        else:
            raise ValueError('No bounded exact byte reciprocal')
    elif projection == 'incremented_byte_reciprocal16':
        if len(values) != 1 or not 2 <= values[0] <= 256:
            raise ValueError('Native incremented-byte divisor exceeds its domain')
        divisor = values[0]
        total = (65536 + divisor - 1) // divisor
        if any(value * total >> 16 != value // divisor for value in range(257)):
            raise ValueError('Native incremented-byte reciprocal is inexact')
    elif projection in ('byte_reciprocal_low16', 'byte_reciprocal_high16'):
        if len(values) != 1 or not 2 <= values[0] <= 256:
            raise ValueError('Native byte divisor exceeds its domain')
        divisor = values[0]
        multiplier = ((1 << 32) + divisor - 1) // divisor
        if any((value * multiplier >> 32) != value // divisor for value in range(256)):
            raise ValueError('Native byte reciprocal is inexact')
        total = multiplier & 65535 if projection == 'byte_reciprocal_low16' else multiplier >> 16
    elif projection in ('unsigned_reciprocal_low16', 'unsigned_reciprocal_high16',
                        'unsigned_reciprocal_shift', 'unsigned_reciprocal_narrow_shift'):
        if len(values) != 1 or not 2 <= values[0] <= 256:
            raise ValueError('Native reciprocal divisor exceeds its domain')
        divisor = values[0]
        shift = 31 + divisor.bit_length()
        multiplier = ((1 << shift) + divisor - 1) // divisor
        quotient = 0xffffffff // divisor
        # Rounded-up reciprocals never underestimate. Full-interval maxima
        # grow monotonically; check the last one and the final partial interval
        # to prove exact division over all uint32_t values, not sampled ages.
        if (multiplier > 0xffffffff or
                ((quotient * divisor - 1) * multiplier >> shift) != quotient - 1 or
                (0xffffffff * multiplier >> shift) != quotient):
            raise ValueError('Native reciprocal requires an unsupported correction')
        # SHRN follows the high word of the product: its immediate is
        # 32 - (shift - 32), not a chosen animation parameter.
        total = (64 - shift if projection == 'unsigned_reciprocal_narrow_shift' else
                 shift if projection == 'unsigned_reciprocal_shift' else
                 multiplier & 65535 if projection == 'unsigned_reciprocal_low16' else multiplier >> 16)
    elif projection == 'bg2_priority_depth':
        # Native renderer rank identities, not the original selected priority.
        if ((len(values),field['multiplier'],field['addend'],field['divisor_shift']) != (1,1,0,0) or
                values[0] not in (0,1)):
            raise ValueError('Invalid native background priority')
        total = 4 if values[0] == 0 else 7
    elif projection == 'sum_low_byte_complement':
        # A byte consumer can fold a summed duration into a complemented native
        # immediate. Keep the source sum bounded, then encode its low byte.
        total = sum(values)
        if total > 0xffffffff:
            raise ValueError('Native field sum overflow')
        total = 255 - (total & 255)
    elif projection == 'power_of_two_remainder':
        if len(values) != 2 or not 0 < values[1] <= 0x80000000 or values[1] & (values[1] - 1):
            raise ValueError('Native remainder requires a power-of-two period')
        total = values[0] % values[1]
    elif projection == 'equal_values':
        if any(value != values[0] for value in values):
            raise ValueError('Native shared field has unequal source values')
        total = values[0]
    elif projection == 'consecutive_first':
        if len(values) < 2 or any(value != values[0] + i for i, value in enumerate(values)):
            raise ValueError('Native consecutive field has nonconsecutive source values')
        total = values[0]
    else:
        total = sum(values)
    if projection == 'pair_product_sum':
        if (len(values) % 2 or not 0 < field['width'] <= 24 or
                not 0 <= field['destination_shift'] <= 32-field['width'] or
                not 0 < field['multiplier'] <= 65536 or not -(1 << 31) <= field['addend'] < (1 << 31) or
                not 0 <= field['divisor_shift'] <= 16):
            raise ValueError('Invalid paired-product field or arithmetic')
        # Selected rectangle dimensions determine their payload lengths.
        # Sum complete pairs, with the same unsigned word bound as restoration.
        total = sum(values[i] * values[i+1] for i in range(0,len(values),2))
    if total > 0xffffffff:
        raise ValueError('Native field sum overflow')
    value = total * field['multiplier'] + field['addend']
    divisor = 1 << field['divisor_shift']
    if value < 0 or value % divisor or value // divisor >= 1 << field['width']:
        raise ValueError('Native field is not exactly representable')
    return (value // divisor) << field['destination_shift']


def symbol_offsets(path, data):
    # nm is a developer-packaging dependency only. The distributed Builder has
    # the bounded locations and expressions already, and uses no developer tools.
    output = subprocess.check_output(['/usr/bin/nm', '-nm', str(path)], text=True)
    symbols = {}
    count, commands_size = struct.unpack_from('<II', data, 16)
    cursor, mappings = 32, []
    for _ in range(count):
        command, size = struct.unpack_from('<II', data, cursor)
        if size < 8 or cursor + size > 32 + commands_size:
            raise ValueError('Invalid native segment commands')
        if command == 0x19:
            address, _, offset, extent = struct.unpack_from('<QQQQ', data, cursor + 24)
            if extent:
                mappings.append((address, address + extent, offset))
        cursor += size
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 2 or not re.fullmatch('[0-9a-fA-F]{16}', parts[0]):
            continue
        address = int(parts[0], 16)
        for start, stop, offset in mappings:
            if start <= address < stop:
                symbols[parts[-1]] = offset + address - start
    return symbols


def field_plan(path, original, specs, materials, profiles):
    validate_material_alias_values(specs, materials, profiles)
    symbols = symbol_offsets(path, original)
    identities = {row['id']: i for i, row in enumerate(specs)}
    plan = []
    for profile in profiles['profiles']:
        # Entry-point inlining differs between the two applications. A profile
        # for one image cannot authenticate or provide coverage for the other.
        if profile.get('binary', Path(path).name) != Path(path).name:
            continue
        base = profile_base(path, original, symbols, profile)
        start = base + profile['start']
        end = start + profile['size']
        if end > len(original) or hashlib.sha256(original[start:end]).hexdigest() != profile['sha256'][Path(path).name]:
            raise ValueError('Native compiler output changed; folded fields require review')
        for item in profile['fields']:
            field = dict(item, offset=base + item['offset'], material=identities[item['material']],
                         source_stride=item.get('source_stride', 1), projection=item.get('projection', 'sum'))
            value = field_value(materials[field['material']], specs[field['material']]['native_width'], field)
            mask = ((1 << field['width']) - 1) << field['destination_shift']
            if struct.unpack_from('<I', original, field['offset'])[0] & mask != value:
                raise ValueError('Native data field disagrees with ROM projection')
            plan.append(tuple(PROJECTIONS[field[k]] if k == 'projection' else field[k] for k in KEYS))
    plan.sort()
    if any(a[0] + 4 > b[0] for a, b in zip(plan, plan[1:])):
        raise ValueError('Overlapping native field profiles')
    return plan


def profile_base(path, image, symbols, profile):
    """Resolve a reviewed native constant through its consumer's own loads.

    Compiler constants need not have symbols or a stable distance from code in
    two differently linked executables. Authenticate the bounded consumer range
    and decode only its ADRP/LDR address relocation. No instruction payload or
    original CPU program is copied; the Builder receives ordinary data offsets.
    """
    if profile['symbol'] not in symbols:
        raise ValueError('Native field owner is absent')
    base = symbols[profile['symbol']]
    reference = profile.get('constant_reference')
    if reference is None:
        return base
    width = reference.get('load_width', 8)
    if type(width) is not int or width not in (8, 16):
        raise ValueError('Native constant has an unsupported load width')
    start, stop = base + reference['page'], base + reference['loads'][-1] + 4
    if (start < 0 or start % 4 or stop > len(image) or
            hashlib.sha256(image[start:stop]).hexdigest() != reference['sha256'][Path(path).name]):
        raise ValueError('Native constant consumer changed')
    page = struct.unpack_from('<I', image, start)[0]
    if page & 0x9f000000 != 0x90000000 or page & 31 == 31:
        raise ValueError('Constant consumer has no supported page relocation')
    displacement = ((page >> 29) & 3) | (((page >> 5) & 0x7ffff) << 2)
    if displacement & (1 << 20):
        displacement -= 1 << 21
    target_page = (start & ~4095) + displacement * 4096
    address = None
    for at in reference['loads']:
        load = struct.unpack_from('<I', image, base + at)[0]
        opcodes = (0xfd400000, 0xf9400000) if width == 8 else (0x3dc00000,)
        if (load & 0xffc00000 not in opcodes or
                (load >> 5) & 31 != page & 31):
            raise ValueError('Constant consumer has no matching scalar/vector load')
        current = target_page + ((load >> 10) & 4095) * width
        if address is not None and current != address:
            raise ValueError('Constant consumer aliases disagree')
        address = current
    # Both accepted consumers and their constants must be in __TEXT, where
    # Mach-O file/virtual page displacement is identical. The section check
    # additionally excludes executable words, signatures and linkage tables.
    from native_template import layout
    _, sections = layout(image)
    cursor = 32
    text_range = None
    for _ in range(struct.unpack_from('<I', image, 16)[0]):
        command, size = struct.unpack_from('<II', image, cursor)
        if command == 0x19 and image[cursor + 8:cursor + 24].rstrip(b'\0') == b'__TEXT':
            virtual = struct.unpack_from('<Q', image, cursor + 24)[0]
            offset, extent = struct.unpack_from('<QQ', image, cursor + 40)
            if (virtual - offset) % 4096:
                raise ValueError('Native text mapping has unsupported page alignment')
            text_range = (offset, offset + extent)
        cursor += size
    if (text_range is None or not text_range[0] <= start < stop <= text_range[1] or
            address is None or not text_range[0] <= address <= text_range[1] - width or
            not any(a <= address and address + width <= b for a, b in sections)):
        raise ValueError('Native constant is outside its reviewed text data section')
    return address
