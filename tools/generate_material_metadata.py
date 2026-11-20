#!/usr/bin/env python3
"""Generate extraction metadata only. This tool never reads ROM payloads."""
import hashlib
import json
from pathlib import Path
import re
import sys


def validate_byte_publication(row):
    """Validate content-free native byte-publication ABI and extraction format."""
    if row['transform'] not in ('arm64_inline_byte_append', 'arm64_stack_byte_argument'):
        raise ValueError('Unknown byte-publication transform')
    g = row['groups']; inline = row['transform'] == 'arm64_inline_byte_append'
    if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
            (row['source_count'],row['source_width'],row['native_width']) != (1,1,1) or
            row['native_representation'] != 'generated_native_block' or
            len(g) not in ((13,) if inline else (10,12)) or any(type(v) is not int for v in g) or
            not 1 <= g[1] <= 8 or not 0 <= g[0] <= 8-g[1]):
        raise ValueError('Invalid native byte publication source format')
    address, target = g[-2:]
    if inline:
        regs = [g[2], *g[4:9]]
        if (row['count'] not in (40,44) or g[3] not in (0,1) or
                any(not 0 <= r < 31 for r in regs) or len(set(regs)) != 6 or
                not 0 <= g[9] < 512 or not 0 <= g[10] <= 480 or g[10] % 8 or
                g[10] <= g[9] < g[10]+32):
            raise ValueError('Invalid native byte append ABI')
        branch = address + row['count'] - 12; reach = 20
    else:
        scratch,byte,arg,obj,save,link = g[2:8]
        frame = len(g) == 12
        mirror = g[9] if frame else 31
        if frame and (g[8] != 29 or byte > 256 or 29 in (scratch,arg,obj,save) or
                      (mirror != 31 and (not 0 < mirror < 29 or mirror in (scratch,arg,obj,save)))):
            raise ValueError('Invalid frame byte ABI')
        base = 12 + 4*(obj != 31) + 4*(save != 31) + 4*(mirror != 31)
        if (row['count'] not in (base,base+4) or not 0 < scratch < 31 or
                not 0 <= byte <= 4095 or not 0 < arg < 8 or
                not 0 < obj <= 31 or not 0 < save <= 31 or
                scratch in (arg,obj,save) or arg in (obj,save) or
                (save != 31 and save == obj) or link not in (0,1)):
            raise ValueError('Invalid native stack byte ABI')
        branch = address + row['count'] - 4; reach = 27
    if (any(not 0 <= v < 2**32 or v % 4 for v in (address,target)) or
            address + row['count'] > 2**32 or address <= target < address+row['count'] or
            not -(1 << reach) <= target-branch < (1 << reach)):
        raise ValueError('Invalid native byte publication linkage')


def validate_palette_pair(row):
    """Validate palette representation metadata and native consumer ABI."""
    if row['transform'] not in ('palette_pair_value','arm64_palette_boolean_value', 'arm64_palette_boolean_result','arm64_palette_optional_search'):
        raise ValueError('Unknown paired palette transform')
    g=row['groups']; scalar=row['transform']=='palette_pair_value'
    call=row['transform']=='arm64_palette_boolean_result'
    value=row['transform']=='arm64_palette_boolean_value'
    if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
            row['source_width']!=1 or row['native_width']!=(4 if scalar else 1) or
            len(g)!=(9 if scalar else 10 if value else 12 if call else 20) or any(type(v) is not int for v in g) or
            not 1<=g[0]<=256 or not 0<=g[1]<8 or not 0<g[2]<=(255>>g[1]) or
            not 1<=g[3]<=256 or not 1<=g[4]<=256 or not 1<=g[5]<=min(g[3],g[4]) or
            not 1<=g[6]<=8 or g[7] not in (0,1) or
            row['source_count']<=2*g[0] or (row['source_count']-2*g[0])%2 or
            not (g[6]-1)*g[4]+g[5]<=(row['source_count']-2*g[0])//2<=65536 or
            row['native_representation']!=('generated_block_input' if scalar else 'generated_native_block')):
        raise ValueError('Invalid paired palette representation')
    if scalar:
        if row['count']!=1 or g[8] not in (0,1):
            raise ValueError('Invalid paired palette projection')
    elif value:
        if any(not 0<=v<31 for v in g[8:]) or row['count'] not in range(4,21,4):
            raise ValueError('Invalid Boolean palette value ABI')
    elif call:
        if (not 0<=g[8]<31 or not 0<=g[9]<31 or row['count'] not in range(8,25,4) or
                any(not 0<=v<2**32 or v%4 for v in g[10:]) or g[10]>2**32-25 or
                g[10]<=g[11]<g[10]+row['count'] or not -(1<<27)<=g[11]-g[10]<(1<<27)):
            raise ValueError('Invalid Boolean palette call ABI')
    else:
        if (any(not 0<=v<31 for v in g[8:15]) or len(set(g[8:15]))!=7 or
                not 1<=g[15]<=255 or not 1<=g[16]<=256 or not 0<=g[17]<=511 or
                any(not 0<=v<2**32 or v%4 for v in g[18:]) or g[18]>2**32-69 or
                row['count'] not in range(48,69,4) or g[18]<=g[19]<g[18]+row['count'] or
                not -(1<<27)<=g[19]-(g[18]+row['count']-4)<(1<<27) or
                (row['count']==48 and not -(1<<15)<=g[19]-(g[18]+32)<(1<<15))):
            raise ValueError('Invalid optional palette search ABI')


def load_specs(path):
    path = Path(path)
    raw = Path(path).read_bytes()
    doc = json.loads(raw)
    if doc['schema'] != 1 or not 0 < len(doc['materials']) <= 4096:
        raise ValueError('Unsupported material schema/count')
    ids = set()
    inventory = json.loads(path.with_name('rom_inventory.json').read_text())
    containers = {row['id']: row for row in inventory.get('decoded_sources', [])}
    for row in doc['materials']:
        row.setdefault('source_count', row['count'])
        row.setdefault('transform', 'unsigned_values')
        row.setdefault('sites', [])
        row.setdefault('groups', [])
        row.setdefault('decoded_source', '')
        row.setdefault('native_representation', 'constant_array')
        if row['native_representation'] not in ('constant_array', 'reviewed_fields', 'folded_pending',
                                                'generated_native_block', 'generated_block_input'):
            raise ValueError('Unreviewed native representation')
        container = containers.get(row['decoded_source']) if row['decoded_source'] else None
        if row['decoded_source'] and not container:
            raise ValueError('Unknown decoded material source')
        row['container'] = ({key: container[key] for key in
            ('offset', 'size', 'decoded_size', 'sha256', 'decoded_sha256')} if container else None)
        if container:
            if (container['codec'] != 'lzss' or
                    any(type(container[k]) is not int for k in ('offset', 'size', 'decoded_size')) or
                    container['offset'] < 0 or container['size'] <= 0 or
                    container['offset'] + container['size'] > 3145728 or
                    not 0 < container['decoded_size'] <= 65536 or
                    any(not re.fullmatch(r'[0-9a-f]{64}', container[k])
                        for k in ('sha256', 'decoded_sha256'))):
                raise ValueError('Unsupported decoded material container')
        extent = container['decoded_size'] if container else 3145728
        if not re.fullmatch(r'[a-z][a-z0-9_]{0,79}', row['id']) or row['id'] in ids:
            raise ValueError('Invalid or duplicate material identity')
        ids.add(row['id'])
        if (row['source_width'] not in (1, 2) or row['native_width'] not in (1, 4) or
                not 0 < row['count'] <= 65536 or row['offset'] < 0 or
                not 0 < row['source_count'] <= 65536 or
                row['offset'] + row['source_count'] * row['source_width'] > extent):
            raise ValueError('Unsupported material conversion/range')
        if row['transform'] not in ('repeating_tile_pages', 'fixed_fraction_term', 'unsigned_values', 'equal_unsigned', 'doubling_series_base', 'zero_extend_bytes', 'set_bit_indices', 'signed_values',
                                    'cumulative_unsigned', 'color_channel_5', 'color_arithmetic_mode', 'tile_index_6',
                                    'pairwise_unsigned_differences', 'equal_unsigned_differences',
                                    'grouped_unsigned_sums', 'grouped_equal_unsigned', 'grouped_equal_bit_fields', 'grouped_consecutive_bit_field_base', 'equal_unsigned_fill_pattern', 'grouped_consecutive_base', 'coordinate_combination', 'scrolled_positions', 'linear_coordinate_series', 'strided_clip_layout', 'affine_coordinate_grid', 'fixed_color_lengths', 'fixed_color_shades',
                                    'gray_fill_pattern_16', 'span_fill_pattern_16', 'button_mapping', 'menu_defaults',
                                    'reverse_range_indices', 'offset_unsigned', 'magnitude_7', 'fade_scroll_schedule',
                                    'scaled_le_values', 'unsigned_bit_fields', 'equal_bit_field', 'wrapped_text_layout', 'wrapped_scroll_schedule', 'scaled_byte_division',
                                    'cell_run_origins', 'arm64_cell_apertures', 'bit_transition_index', 'record_loop_tail', 'record_indices', 'scaled_record_indices',
                                    'cell_coordinates', 'cell_position_components', 'framed_rectangle_metric', 'scaled_record_sum', 'indexed_record_pair', 'equal_color_arithmetic_mode',
                                    'condition_choice_value', 'arm64_condition_choice', 'condition_cell_value', 'arm64_cell_condition', 'arm64_coordinate_pair', 'text_field_metric', 'arm64_text_column_fragment', 'text_field_columns', 'arm64_dimension_choice', 'number_choice_values', 'arm64_number_choice', 'arm64_bit_number_prefix', 'arm64_palette_initialization', 'row_band_pattern', 'arm64_row_band_fill', 'arm64_flag_caption', 'caption_choice_value', 'arm64_caption_choice', 'arm64_tagged_byte_constant', 'arm64_boolean_argument', 'arm64_inline_byte_append', 'arm64_stack_byte_argument', 'arm64_short_fill', 'arm64_vector_fill', 'arm64_vector_rows', 'banked_word_copy_value', 'arm64_banked_word_copy', 'label_region_extent', 'coalesced_text_region', 'arm64_text_region_copy', 'arm64_text_region_clear', 'arm64_label_region_clear', 'paired_tile_copy_layout', 'arm64_paired_tile_copy', 'particle_pair_value', 'arm64_particle_pairs', 'arm64_split_index_branch', 'arm64_selection_branch', 'arm64_strided_row_fragment', 'arm64_gray_fill', 'arm64_inline_gray_fill', 'arm64_tile_runs', 'tile_run_layout',
                                    'affine_copy_layout', 'arm64_affine_copy', 'mosaic_envelope', 'scrolled_row_cells', 'scrolled_grid_cells', 'scrolled_cell_coordinates', 'palette_group_index', 'palette_pair_value', 'arm64_palette_boolean_value', 'arm64_palette_boolean_result', 'arm64_palette_optional_search', 'unique_value_index', 'row_active_boundary', 'scaled_endpoint_span', 'affine_view_origin',
                                    'trail_layout', 'arm64_trail_update', 'arm64_trail_draw', 'attribute_rectangle_words',
                                    'unsigned_pair_product', 'unsigned_pair_quotient', 'descending_ramp_iterations', 'scaled_span_duration', 'linear_gradient_intercept', 'position_cycle_offset', 'rgb555_channel_values',
                                    'offset_span_duration', 'delayed_linear_motion', 'delayed_cycle_phase',
                                    'unit_acceleration_motion', 'packed_bit_fields', 'byte_power_geometry', 'opposite_word_magnitude',
                                    'byte_phase_span', 'byte_countdown_periods', 'signed_word_divisor_parameters', 'signed_halfword_divisor_parameters', 'tiled_transfer_schedule', 'strided_row_layout', 'palette_cycle_layout', 'arm64_palette_cycle', 'byte_choice_value', 'arm64_byte_choice', 'clamped_phase_value', 'arm64_clamped_phase', 'particle_strip_layout', 'arm64_particle_strip', 'compressed_row_layout', 'compressed_text_layout'):
            raise ValueError('Unknown material transform')
        if row['transform'] not in ('repeating_tile_pages', 'cell_position_components', 'set_bit_indices', 'equal_unsigned', 'doubling_series_base', 'zero_extend_bytes', 'grouped_unsigned_sums', 'grouped_equal_unsigned', 'grouped_equal_bit_fields', 'grouped_consecutive_bit_field_base', 'equal_unsigned_fill_pattern', 'grouped_consecutive_base', 'coordinate_combination', 'scrolled_positions', 'linear_coordinate_series', 'strided_clip_layout', 'affine_coordinate_grid', 'pairwise_unsigned_differences', 'equal_unsigned_differences', 'fixed_color_lengths',
                                    'fixed_color_shades', 'gray_fill_pattern_16', 'span_fill_pattern_16', 'button_mapping',
                                    'menu_defaults', 'reverse_range_indices',
                                    'condition_choice_value', 'arm64_condition_choice', 'condition_cell_value', 'arm64_cell_condition', 'arm64_coordinate_pair', 'text_field_metric', 'arm64_text_column_fragment', 'text_field_columns', 'arm64_dimension_choice', 'number_choice_values', 'arm64_number_choice', 'arm64_bit_number_prefix', 'arm64_palette_initialization', 'row_band_pattern', 'arm64_row_band_fill', 'arm64_flag_caption', 'caption_choice_value', 'arm64_caption_choice', 'arm64_tagged_byte_constant', 'arm64_boolean_argument', 'arm64_inline_byte_append', 'arm64_stack_byte_argument', 'arm64_short_fill', 'arm64_vector_fill', 'arm64_vector_rows', 'banked_word_copy_value', 'arm64_banked_word_copy', 'label_region_extent', 'coalesced_text_region', 'arm64_text_region_copy', 'arm64_text_region_clear', 'arm64_label_region_clear', 'paired_tile_copy_layout', 'arm64_paired_tile_copy', 'particle_pair_value', 'arm64_particle_pairs', 'arm64_split_index_branch', 'arm64_selection_branch', 'arm64_strided_row_fragment', 'arm64_gray_fill', 'arm64_inline_gray_fill', 'arm64_tile_runs', 'tile_run_layout',
                                    'affine_copy_layout', 'arm64_affine_copy', 'mosaic_envelope', 'scrolled_row_cells', 'scrolled_grid_cells', 'scrolled_cell_coordinates', 'palette_group_index', 'palette_pair_value', 'arm64_palette_boolean_value', 'arm64_palette_boolean_result', 'arm64_palette_optional_search', 'unique_value_index', 'row_active_boundary', 'scaled_endpoint_span', 'affine_view_origin',
                                    'trail_layout', 'arm64_trail_update', 'arm64_trail_draw',
                                    'attribute_rectangle_words', 'fade_scroll_schedule', 'scaled_le_values', 'wrapped_text_layout', 'wrapped_scroll_schedule',
                                    'cell_run_origins', 'arm64_cell_apertures', 'bit_transition_index', 'record_loop_tail', 'equal_bit_field', 'equal_color_arithmetic_mode',
                                    'framed_rectangle_metric', 'scaled_record_sum', 'indexed_record_pair', 'unsigned_pair_product', 'unsigned_pair_quotient',
                                    'descending_ramp_iterations', 'scaled_span_duration', 'linear_gradient_intercept', 'position_cycle_offset', 'rgb555_channel_values',
                                    'offset_span_duration', 'delayed_linear_motion', 'delayed_cycle_phase',
                                    'packed_bit_fields', 'byte_power_geometry', 'opposite_word_magnitude', 'byte_phase_span', 'byte_countdown_periods', 'signed_word_divisor_parameters', 'signed_halfword_divisor_parameters', 'tiled_transfer_schedule', 'strided_row_layout', 'palette_cycle_layout', 'arm64_palette_cycle', 'byte_choice_value', 'arm64_byte_choice', 'clamped_phase_value', 'arm64_clamped_phase', 'particle_strip_layout', 'arm64_particle_strip', 'compressed_row_layout', 'compressed_text_layout') and row['source_count'] != row['count']:
            raise ValueError('Unsigned projection changes the element count')
        if row['transform'] == 'doubling_series_base' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                (row['source_width'], row['native_width'], row['count']) != (2,4,1) or
                not 2 <= row['source_count'] <= 16 or row['groups']):
            raise ValueError('Invalid doubling-series metadata')
        if row['transform'] == 'linear_coordinate_series' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                (row['source_width'], row['native_width'], row['count']) != (1,4,3) or
                len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                not 1 <= row['groups'][0] <= 256 or
                not row['groups'][0] + 2 <= row['source_count'] <= row['groups'][0] + 255):
            raise ValueError('Invalid linear coordinate-series metadata')
        if row['transform'] == 'affine_coordinate_grid' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                row['source_width'] not in (1,2) or row['native_width'] != 4 or row['count'] != 5 or
                not 8 <= row['source_count'] <= 65536 or row['source_count'] % 2 or row['groups']):
            raise ValueError('Invalid affine coordinate-grid representation')
        if row['transform'] == 'scrolled_positions' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                row['source_width'] != 2 or row['native_width'] != 4 or
                row['source_count'] != row['count'] * 2 or row['groups']):
            raise ValueError('Invalid scrolled-position representation')
        if row['transform'] == 'strided_clip_layout' and (
                any(type(row[k]) is not int for k in ('source_width','native_width','source_count','count')) or
                (row['source_width'],row['native_width'],row['count']) != (1,4,6) or
                not 3 <= row['source_count'] <= 257 or row['groups']):
            raise ValueError('Invalid strided clip layout')
        if row['transform'] == 'coordinate_combination':
            groups = row['groups']
            if ((row['source_width'], row['native_width'], row['count']) != (1,4,1) or
                    not groups or len(groups) % 7 or len(groups) > 256 * 7 or
                    any(type(n) is not int for n in groups)):
                raise ValueError('Invalid coordinate combination layout')
            for i in range(0, len(groups), 7):
                width, base, stride, columns, axis, scale, sign = groups[i:i+7]
                if (width not in (1,2) or not 0 <= base <= 65535 or
                        not 1 <= stride <= 256 or not 1 <= columns <= 256 or
                        not 0 <= axis <= 2 or not 1 <= scale <= 65536 or sign not in (1,2)):
                    raise ValueError('Invalid coordinate combination format')
            if sum(groups[::7]) != row['source_count']:
                raise ValueError('Invalid coordinate combination input extent')
        if row['transform'] == 'tiled_transfer_schedule':
            groups = row['groups']
            entries = (row['source_count'] - 6) // 4
            phases = (row['count'] - 3) // 2
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 4 or
                    row['source_count'] < 14 or (row['source_count'] - 6) % 4 or
                    not 2 <= entries <= 256 or entries & (entries - 1) or
                    row['count'] < 5 or row['count'] % 2 != 1 or
                    not 1 <= phases <= entries or phases & (phases - 1) or
                    len(groups) != 3 or any(type(n) is not int for n in groups) or
                    not 0 <= groups[0] <= 65535 or not 1 <= groups[1] <= 256 or
                    not 1 <= groups[2] <= 2048):
                raise ValueError('Invalid tiled-transfer representation')
        if row['transform'] == 'byte_power_geometry' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                row['source_width'] != 1 or row['native_width'] != 4 or
                not 1 <= row['source_count'] <= 32768 or
                row['count'] != row['source_count'] * 2 or row['groups']):
            raise ValueError('Invalid byte-power geometry representation')
        if row['transform'] == 'opposite_word_magnitude' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                (row['source_count'],row['count'],row['source_width'],row['native_width']) != (2,1,2,4) or row['groups']):
            raise ValueError('Invalid opposite-word representation')
        if row['transform'] == 'rgb555_channel_values' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                not 1 <= row['source_count'] <= 3 or row['count'] != 1 or
                row['source_width'] != 1 or row['native_width'] != 4 or row['groups']):
            raise ValueError('Invalid RGB channel representation')
        if row['transform'] == 'byte_countdown_periods' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                (row['source_width'],row['native_width'],row['count']) != (1,4,3) or
                not 2 <= row['source_count'] <= 65536 or row['source_count'] % 2 or row['groups']):
            raise ValueError('Invalid byte countdown representation')
        if row['transform'] in ('signed_word_divisor_parameters', 'signed_halfword_divisor_parameters') and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                (row['source_count'],row['count'],row['source_width'],row['native_width']) != (1,4 if row['transform'] == 'signed_halfword_divisor_parameters' else 10,1,4) or row['groups']):
            raise ValueError('Invalid signed-word divisor parameter representation')
        if row['transform'] == 'position_cycle_offset' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                row['source_count'] != 3 or row['count'] != 2 or
                row['source_width'] not in (1,2) or row['native_width'] != 4 or row['groups']):
            raise ValueError('Invalid cyclic coordinate representation')
        if row['transform'] in ('unsigned_pair_product', 'unsigned_pair_quotient', 'descending_ramp_iterations', 'scaled_span_duration', 'linear_gradient_intercept'):
            expected = 2 if row['transform'] in ('unsigned_pair_product', 'unsigned_pair_quotient') else 3
            if (any(type(row[k]) is not int for k in
                    ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_count'] != expected or row['count'] != 1 or
                    row['source_width'] not in (1, 2) or row['native_width'] != 4 or row['groups']):
                raise ValueError('Invalid bounded operand arithmetic')
        if row['transform'] == 'fade_scroll_schedule' and (
                row['source_width'], row['source_count'], row['native_width'], row['count']) != (1, 7, 4, 8):
            raise ValueError('Invalid fade/scroll schedule representation')
        if row['transform'] == 'equal_unsigned_fill_pattern' and (
                any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                row['source_width'] not in (1,2) or row['native_width'] != 1 or
                row['count'] % row['source_width'] or row['groups']):
            raise ValueError('Invalid shared fill-pattern representation')
        if row['transform'] == 'equal_unsigned' and (row['count'], row['native_width']) != (1, 4):
            raise ValueError('Shared material requires one native word')
        if row['transform'] == 'equal_color_arithmetic_mode' and (
                row['count'], row['source_width'], row['native_width']) != (1, 1, 4):
            raise ValueError('Shared color arithmetic requires byte inputs and one word')
        if row['transform'] == 'grouped_equal_unsigned' and row['native_width'] != 4:
            raise ValueError('Shared material groups require native words')
        if row['transform'] == 'zero_extend_bytes' and (
                row['source_width'] != 1 or row['native_width'] != 1 or
                row['source_count'] > row['count']):
            raise ValueError('Invalid zero-extended byte representation')
        if (row['transform'] == 'pairwise_unsigned_differences' and
                row['source_count'] != 2 * row['count']):
            raise ValueError('Invalid material endpoint pair count')
        if row['transform'] == 'equal_unsigned_differences' and (
                row['count'] != 1 or row['native_width'] != 4 or row['source_count'] % 2):
            raise ValueError('Shared difference requires complete pairs and one word')
        if row['transform'] == 'set_bit_indices' and (row['source_count'] != 4 or row['source_width'] != 1):
            raise ValueError('Bit selection requires a four-byte source mask')
        if row['transform'] == 'signed_values' and row['native_width'] != 4:
            raise ValueError('Signed native material requires word output')
        if row['transform'] == 'button_mapping' and (
                row['source_width'], row['source_count'], row['native_width'], row['count']) != (1, 4, 4, 8):
            raise ValueError('Invalid button mapping projection')
        if row['transform'] == 'menu_defaults' and (
                row['source_width'], row['source_count'], row['native_width'], row['count']) != (1, 2, 4, 3):
            raise ValueError('Invalid menu-default projection')
        if row['transform'] in ('color_channel_5', 'color_arithmetic_mode', 'tile_index_6', 'magnitude_7') and row['source_width'] != 1:
            raise ValueError('Fixed-color field requires byte input')
        if row['sites'] and (len(row['sites']) != row['source_count'] or
                any(not isinstance(at, int) or at < 0 or at + row['source_width'] > extent for at in row['sites'])):
            raise ValueError('Invalid material operand/data locations')
        if row['transform'] in ('fixed_color_lengths', 'fixed_color_shades') and (
                row['source_width'] != 1 or row['source_count'] % 2 or
                row['count'] not in (row['source_count'] // 2, row['source_count'] // 2 + 1)):
            raise ValueError('Invalid gray-band projection')
        if row['transform'] == 'span_fill_pattern_16' and (
                row['source_width'], row['source_count'], row['native_width'], row['count']) != (1, 2, 4, 4):
            raise ValueError('Invalid span fill projection')
        if row['transform'] == 'gray_fill_pattern_16' and (
                row['source_width'] != 1 or row['source_count'] != 1 or
                row['native_width'] != 4 or row['count'] != 4):
            raise ValueError('Invalid gray fill projection')
        if row['transform'] in ('delayed_linear_motion', 'delayed_cycle_phase', 'unit_acceleration_motion'):
            source_count, count = {'delayed_linear_motion': (3,5), 'delayed_cycle_phase': (2,1),
                                   'unit_acceleration_motion': (4,4)}[row['transform']]
            if (any(type(row[k]) is not int for k in
                    ('source_count', 'count', 'source_width', 'native_width')) or
                    (row['source_count'], row['count'], row['source_width'], row['native_width']) !=
                    (source_count, count, 2, 4)):
                raise ValueError('Invalid motion operand representation')
            if row['transform'] == 'delayed_linear_motion':
                if (len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                        not 0 < row['groups'][0] <= 65536):
                    raise ValueError('Invalid explicit motion step')
            elif row['groups']:
                raise ValueError('Unexpected motion preset metadata')
        elif row['transform'] == 'compressed_text_layout':
            g=row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width']!=1 or row['native_width']!=4 or len(g)!=9 or
                    any(type(v) is not int for v in g) or not 1<=g[0]<=256 or not 0<=g[1]<g[0] or
                    not 10<=g[2]<=388 or g[2]%3!=1 or row['source_count']<g[2]+2 or
                    (row['source_count']-g[2])%2 or row['count']!=3+row['source_count']-g[2] or
                    not 0<=g[3]<=65535 or not 1<=g[4]<=4 or not 1<=g[5]<=256 or
                    not 1<=g[6]<=256 or not g[4]*g[5]*g[6]<=g[7]<=65536 or not 1<=g[8]<=256):
                raise ValueError('Invalid compressed text layout')
        elif row['transform'] == 'compressed_row_layout':
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['count']!=3 or row['source_width']!=1 or row['native_width']!=4 or
                    not 10<=row['source_count']<=388 or row['source_count']%3!=1 or
                    len(row['groups'])!=2 or any(type(v) is not int for v in row['groups']) or
                    not 1<=row['groups'][0]<=256 or not 0<=row['groups'][1]<row['groups'][0]):
                raise ValueError('Invalid compressed row layout')
        elif row['transform'] in ('particle_strip_layout', 'arm64_particle_strip'):
            g=row['groups']; native=row['transform']=='arm64_particle_strip'
            if (any(type(row[k]) is not int for k in
                    ('source_count','count','source_width','native_width')) or
                    row['source_width']!=1 or row['native_width']!=(1 if native else 4) or
                    len(g)!=(18 if native else 5) or any(type(v) is not int for v in g) or
                    not 6<=g[0]<=255 or any(not 1<=v<=32 for v in g[1:5]) or
                    row['source_count']!=2*g[1]+sum(g[2:5]) or
                    row['native_representation']!=('generated_native_block' if native else 'generated_block_input') or
                    (not native and row['count']!=4)):
                raise ValueError('Invalid particle strip representation')
            if native:
                f=g[5:]; kind=f[2]
                if (kind not in (0,1,2,3) or not 1<=f[0]<=254 or f[1]!=g[0] or
                        f[0]*f[1]+5>4095 or any(not 0<=v<=2**32-1 or v%4 for v in f[3:10]) or
                        f[3]>2**32-1-148 or row['count']!=(148,116,28,144)[kind] or
                        not 0<=f[10]<=32760 or f[10]%8 or not 0<=f[11]<=32752 or f[11]%8 or
                        not 0<=f[12]<=4095 or
                        (kind==2 and any(f[i]!=0 for i in (*range(4,9),10,11,12))) or
                        (kind in (0,3) and f[5]!=0) or (kind!=0 and (f[7]!=0 or f[8]!=0)) or
                        (kind==0 and f[6]!=f[3]+112)):
                    raise ValueError('Invalid native particle strip ABI')
        elif row['transform'] in ('palette_cycle_layout', 'arm64_palette_cycle'):
            g=row['groups']; native=row['transform']=='arm64_palette_cycle'
            if (any(type(row[k]) is not int for k in
                    ('source_count','count','source_width','native_width')) or
                    row['source_width']!=2 or row['native_width']!=(1 if native else 4) or
                    len(g)!=(6 if native else 2) or any(type(v) is not int for v in g) or
                    not 2<=g[1]<=128 or not 0<=g[0]<=65536-2*g[1] or g[0]%2 or
                    not 5<=row['source_count']<=2*g[1]+1 or row['source_count']%2!=1 or
                    (not native and row['count']!=3) or
                    (native and (row['native_representation']!='generated_native_block' or
                        not 32<=row['count']<=4096 or row['count']%4 or
                        not 0<=g[2]<=16380 or g[2]%4 or not 0<=g[3]<=16380 or g[3]%4 or
                        not 0<=g[4]<=32760 or g[4]%8 or not 0<=g[5]<=8192-2*g[1] or g[5]%2))):
                raise ValueError('Invalid palette cycle representation')
        elif row['transform'] in ('strided_row_layout', 'arm64_strided_row_fragment'):
            g = row['groups']; native = row['transform'] == 'arm64_strided_row_fragment'
            if (any(type(row[k]) is not int for k in
                    ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] != 1 or row['native_width'] != (1 if native else 4) or
                    len(g) != (10 if native else 4) or any(type(v) is not int for v in g) or
                    not 1 <= g[0] <= 256 or not 1 <= g[1] <= 256 or
                    not 1 <= g[2] <= 256 or not 2 <= g[3] <= 256 or
                    row['source_count'] != 10 + 2*g[1] + g[2] + g[3] or
                    (not native and row['count'] != 6) or
                    (native and (row['native_representation'] != 'generated_native_block' or
                        g[4] not in (0,1,2) or any(not 0 <= v < 31 for v in g[5:8]) or g[8] != 0 or
                        row['count'] != (4 if g[4] == 0 else 8) or
                        (g[4] == 0 and (g[9] != 0 or g[6] == g[7])) or
                        (g[4] == 1 and (not 0 <= g[9] <= 16380 or g[9]%4 or g[6] == g[7])) or
                        (g[4] == 2 and (not 0 < g[9] <= 4095 or g[5] == g[6]))))):
                raise ValueError('Invalid strided-row representation')
        elif row['transform'] == 'offset_span_duration':
            if (any(type(row[k]) is not int for k in
                    ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_count'] != 3 or row['count'] != 1 or
                    row['source_width'] not in (1, 2) or row['native_width'] != 4 or
                    len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                    not 0 < row['groups'][0] <= 65536):
                raise ValueError('Invalid offset span representation')
        elif row['transform'] == 'attribute_rectangle_words':
            groups = row['groups']
            if ((row['source_width'], row['source_count'], row['native_width']) != (1, 9, 4) or
                    len(groups) != 5 or any(type(n) is not int for n in groups) or
                    not 0 <= groups[0] <= 65535 or not 0 < groups[1] <= 256 or
                    not 0 < groups[2] <= 256 or not 0 < groups[3] <= 4 or
                    not 0 <= groups[4] < (groups[1] * groups[2] + 31) // 32 or
                    row['count'] > (groups[1] * groups[2] + 31) // 32 - groups[4]):
                raise ValueError('Invalid attribute rectangle word slice')
        elif row['transform'] == 'arm64_cell_apertures':
            groups = row['groups']
            if (any(type(row[k]) is not int for k in ('source_width', 'native_width', 'count', 'source_count')) or
                    row['source_width'] != 2 or row['native_width'] != 1 or
                    row['native_representation'] != 'generated_native_block' or
                    row['count'] > 4096 or row['count'] % 4 or len(groups) != 7 or
                    any(type(n) is not int for n in groups) or
                    not 0 <= groups[0] <= 65535 or not 1 <= groups[1] <= 4 or
                    not 1 <= groups[2] <= 32 or not 2 <= groups[3] <= 8 or
                    not 1 <= groups[4] <= groups[2] or not 1 <= groups[5] <= groups[3] or
                    not 0 <= groups[6] <= 65535 or
                    row['source_count'] <= groups[5] or
                    (row['source_count'] - groups[5]) % groups[4] or
                    not 2 <= (row['source_count'] - groups[5]) // groups[4] <= 32):
                raise ValueError('Invalid generated native aperture format/extent')
        elif row['transform'] == 'cell_run_origins':
            groups = row['groups']
            if (row['source_width'] != 2 or row['native_width'] != 4 or
                    not 2 <= row['count'] <= 256 or len(groups) != 6 or
                    any(type(n) is not int for n in groups) or
                    not 0 <= groups[0] <= 65535 or not 1 <= groups[1] <= 4 or
                    not 1 <= groups[2] <= 256 or not 1 <= groups[3] <= 256 or
                    not 1 <= groups[4] <= groups[2] or not 1 <= groups[5] <= groups[3] or
                    row['source_count'] != groups[5] + row['count'] * groups[4]):
                raise ValueError('Invalid cell-run format/extent')
        elif row['transform'] == 'framed_rectangle_metric':
            groups = row['groups']
            if ((row['source_width'], row['source_count'], row['native_width'], row['count']) != (1,4,4,1) or
                    len(groups) != 6 or any(type(n) is not int for n in groups) or
                    not 0 <= groups[0] <= 65535 or not 0 < groups[1] <= 4 or
                    not 0 < groups[2] <= 256 or not 0 < groups[3] <= 256 or
                    not 0 < groups[4] <= 256 or not 0 <= groups[5] <= 1):
                raise ValueError('Invalid framed-rectangle format')
        elif row['transform'] in ('cell_coordinates', 'cell_position_components'):
            groups = row['groups']
            components = row['transform'] == 'cell_position_components'
            if (row['source_width'] != 2 or row['native_width'] != 4 or
                    len(groups) != (4 if components else 5) or
                    row['count'] != row['source_count'] * (2 if components else 1) or
                    any(type(n) is not int for n in groups) or not 0 <= groups[0] <= 65535 or
                    not 0 < groups[1] <= 4 or not 0 < groups[2] <= 256 or
                    not 0 < groups[3] <= 256 or (not components and not 0 <= groups[4] <= 1)):
                raise ValueError('Invalid cell-coordinate format')
        elif row['transform'] == 'byte_phase_span':
            if ((row['source_width'], row['native_width'], row['count']) != (1,4,2) or
                    not 0 < row['source_count'] <= 65535 or row['source_count'] % 3 or
                    len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                    not 0 < row['groups'][0] <= 256):
                raise ValueError('Invalid byte phase span representation')
        elif row['transform'] in ('grouped_equal_bit_fields', 'grouped_consecutive_bit_field_base'):
            g = row['groups']
            consecutive = row['transform'] == 'grouped_consecutive_bit_field_base'
            invalid_shape = ((row['count'] != 1 or not 4 <= len(g) <= 65538)
                             if consecutive else len(g) != row['count']+2)
            if (any(type(row[k]) is not int for k in ('source_count','source_width','native_width','count')) or
                    row['source_width'] not in (1,2) or row['native_width'] != 4 or
                    invalid_shape or any(type(v) is not int for v in g) or
                    not 0 <= g[0] < row['source_width']*8 or
                    not 0 < g[1] <= row['source_width']*8-g[0] or
                    any(v <= 0 for v in g[2:]) or sum(g[2:]) != row['source_count']):
                raise ValueError('Invalid shared bit-field group format')
        elif row['transform'] == 'record_loop_tail':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','source_width','native_width','count')) or
                    (row['source_count'],row['source_width'],row['native_width']) != (2,2,4) or
                    len(g) != 4 or any(type(v) is not int for v in g) or
                    not 0 <= g[0] <= 65535 or not 0 < g[1] <= 256 or
                    not 0 < g[2] <= 65536 or not 1 < g[3] <= 16 or g[3] & (g[3]-1) or
                    row['count'] < 4 or row['count'] > 2+2*(g[3]-1) or row['count'] % 2):
                raise ValueError('Invalid native record-loop tail format')
        elif row['transform'] == 'repeating_tile_pages':
            g = row['groups']
            power = lambda value: value > 0 and value & (value - 1) == 0
            if (any(type(row[k]) is not int for k in ('source_width','native_width','source_count','count')) or
                    (row['source_width'],row['native_width'],row['count']) != (2,4,1) or
                    not 1 <= row['source_count'] <= 32768 or row['source_count'] % 2 or
                    len(g) != 6 or any(type(n) is not int for n in g) or
                    not 0 <= g[5] < 3 or not power(g[0]) or g[0] > 65536 or
                    not 1 <= g[1] <= 65536 or not 1 <= g[2] <= 65536 or
                    not power(g[3]) or g[3] > 256 or not power(g[4]) or g[4] > 256 or
                    g[3] * g[1] > g[2] or row['source_count'] // 2 % g[3]):
                raise ValueError('Invalid repeating tile-page format')
            rows = row['source_count'] // 2 // g[3]
            if (not power(rows) or not 2 <= rows <= 256 or
                    (rows - 1) * g[2] + (g[3] - 1) * g[1] > 65535):
                raise ValueError('Invalid repeating tile-page grid extent')
        elif row['transform'] == 'indexed_record_pair':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_width','native_width','source_count','count')) or
                    (row['source_width'],row['native_width'],row['source_count'],row['count']) != (1,4,3,2) or
                    len(g) != 3 or any(type(n) is not int for n in g) or
                    not 0 <= g[0] <= 65535 or not 1 <= g[1] <= 256 or not 1 <= g[2] <= 65536):
                raise ValueError('Invalid indexed record pair format')
        elif row['transform'] in ('record_indices', 'scaled_record_indices', 'scaled_record_sum'):
            fields = 3 if row['transform'] == 'record_indices' else 4
            if (row['source_width'] != 2 or row['native_width'] != 4 or
                    (row['transform'] == 'scaled_record_sum' and row['count'] != 1) or
                    len(row['groups']) != row['source_count'] * fields or
                    any(type(n) is not int for n in row['groups']) or
                    any(not 0 <= base <= 65535 or not 0 < stride <= 256 or
                        not 0 < extent <= 65536 for base, stride, extent in
                        zip(row['groups'][::fields], row['groups'][1::fields], row['groups'][2::fields])) or
                    (fields == 4 and any(not 0 < scale <= 65536 for scale in row['groups'][3::fields]))):
                raise ValueError('Invalid record-index format')
        elif row['transform'] == 'bit_transition_index':
            if (row['native_width'] != 4 or row['count'] != 1 or row['source_count'] < 2 or
                    len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                    not 0 <= row['groups'][0] < row['source_width'] * 8):
                raise ValueError('Invalid bit transition representation')
        elif row['transform'] == 'scaled_byte_division':
            if (any(type(row[k]) is not int for k in
                    ('source_width', 'source_count', 'native_width', 'count')) or
                    (row['source_width'], row['source_count'], row['native_width'], row['count']) != (2,3,4,3) or
                    row['groups']):
                raise ValueError('Invalid scaled byte division format')
        elif row['transform'] == 'wrapped_text_layout':
            if ((row['source_width'], row['source_count'], row['native_width'], row['count']) != (2,7,4,7) or
                    len(row['groups']) != 5 or any(type(n) is not int for n in row['groups']) or
                    not 0 <= row['groups'][0] <= 65535 or
                    any(not 0 < n <= 256 for n in row['groups'][1:])):
                raise ValueError('Invalid wrapped text layout format')
        elif row['transform'] == 'packed_bit_fields':
            groups = row['groups']
            if (any(type(row[k]) is not int for k in
                    ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] not in (1, 2) or row['native_width'] != 4 or
                    not isinstance(groups, list) or not 0 < len(groups) <= 96 or
                    len(groups) % 3 or any(type(n) is not int for n in groups) or
                    row['source_count'] != row['count'] * (len(groups) // 3)):
                raise ValueError('Invalid packed field representation')
            assigned = 0
            for shift, bits, destination in zip(groups[::3], groups[1::3], groups[2::3]):
                if (not 1 <= bits <= row['source_width'] * 8 or
                        not 0 <= shift <= row['source_width'] * 8 - bits or
                        not 0 <= destination <= 32 - bits):
                    raise ValueError('Invalid packed field bit range')
                mask = ((1 << bits) - 1) << destination
                if assigned & mask:
                    raise ValueError('Overlapping packed fields')
                assigned |= mask
        elif row['transform'] in ('unsigned_bit_fields', 'equal_bit_field'):
            groups = row['groups']
            bits = row['source_width'] * 8
            if (row['native_width'] != 4 or len(groups) != row['count'] * 2 or
                    (row['transform'] == 'equal_bit_field' and row['count'] != 1) or
                    any(type(n) is not int for n in groups) or
                    any(not 0 <= shift < bits or not 1 <= width <= bits - shift
                        for shift, width in zip(groups[::2], groups[1::2]))):
                raise ValueError('Invalid unsigned bit-field layout')
        elif row['transform'] == 'fixed_fraction_term':
            if (any(type(row[k]) is not int for k in
                    ('source_width','source_count','native_width','count')) or
                    row['source_width'] not in (1,2) or
                    (row['source_count'],row['native_width'],row['count']) != (1,4,1) or
                    not isinstance(row['groups'],list) or len(row['groups']) != 2 or
                    any(type(n) is not int for n in row['groups']) or
                    not 0 <= row['groups'][0] <= 16 or row['groups'][1] not in (0,1)):
                raise ValueError('Invalid fixed-fraction representation')
        elif row['transform'] == 'scaled_le_values':
            if (row['source_width'] != 1 or row['native_width'] != 4 or
                    len(row['groups']) != row['count'] * 2 or
                    any(type(n) is not int for n in row['groups']) or
                    any(n not in (1, 2) for n in row['groups'][::2]) or
                    any(not 1 <= n <= 65536 for n in row['groups'][1::2]) or
                    sum(row['groups'][::2]) != row['source_count']):
                raise ValueError('Invalid scaled little-endian value layout')
        elif row['transform'] == 'reverse_range_indices':
            if (row['source_width'] != 2 or row['native_width'] != 4 or
                    row['source_count'] < 2 or row['count'] < row['source_count'] - 1 or
                    len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                    not 0 < row['groups'][0] <= 65536):
                raise ValueError('Invalid range partition metadata')
        elif row['transform'] == 'offset_unsigned':
            if (len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                    not 0 <= row['groups'][0] <= 0xffffffff):
                raise ValueError('Invalid material coordinate offset')
        elif row['transform'] == 'grouped_consecutive_base':
            groups = row['groups']
            if (any(type(row[k]) is not int for k in
                    ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] not in (1,2) or row['native_width'] != 4 or
                    row['count'] != 1 or not isinstance(groups, list) or
                    not 2 <= len(groups) <= 65536 or
                    any(type(n) is not int or not 1 <= n <= 65536 for n in groups) or
                    sum(groups) != row['source_count']):
                raise ValueError('Invalid consecutive-base input groups')
        elif row['transform'] in ('grouped_unsigned_sums', 'grouped_equal_unsigned'):
            if (len(row['groups']) != row['count'] or
                    any(type(n) is not int or n < 1 for n in row['groups']) or
                    sum(row['groups']) != row['source_count']):
                raise ValueError('Invalid material groups')
        elif row['transform'] in ('trail_layout', 'arm64_trail_update', 'arm64_trail_draw'):
            generated = row['transform'] != 'trail_layout'
            group_count = 0 if not generated else 2 if row['transform'] == 'arm64_trail_draw' else 1
            if (any(type(row[k]) is not int for k in ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] != 1 or row['source_count'] != 5 or
                    row['native_width'] != (1 if generated else 4) or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input') or
                    len(row['groups']) != group_count or
                    any(type(n) is not int or not 0 <= n < 2**32 or n % 4 for n in row['groups']) or
                    (group_count == 1 and not 0 < row['groups'][0] <= 4096) or
                    (generated and (row['count'] > 4096 or row['count'] % 4)) or
                    (not generated and row['count'] != 2)):
                raise ValueError('Invalid native trail layout/fragment representation')
        elif row['transform'] == 'affine_view_origin':
            groups = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_count'],row['count'],row['source_width'],row['native_width']) != (8,3,1,4) or
                    len(groups) != 3 or any(type(n) is not int for n in groups) or
                    not 0 < groups[0] <= 65536 or groups[0] & (groups[0]-1) or
                    not 0 < groups[1] <= groups[0] or groups[1] & (groups[1]-1) or
                    not 0 < groups[2] <= 65536):
                raise ValueError('Invalid affine sampling representation/format')
        elif row['transform'] == 'scaled_endpoint_span':
            if (row['source_width'] not in (1,2) or row['native_width'] != 4 or
                    row['source_count'] != 2 or row['count'] != 1 or
                    len(row['groups']) != 1 or type(row['groups'][0]) is not int or
                    not 0 < row['groups'][0] <= 65536):
                raise ValueError('Invalid scaled endpoint span')
        elif row['transform'] == 'row_active_boundary':
            g = row['groups']
            if (row['source_width'] not in (1,2) or row['native_width'] != 4 or row['count'] != 1 or
                    len(g) != 4 or any(type(v) is not int for v in g) or
                    not 0 < g[0] <= 256 or not 0 <= g[1] < row['source_width']*8 or
                    not 0 < g[2] <= 256 or g[3] not in (0,1) or
                    row['source_count'] % g[0] or not 1 <= row['source_count']//g[0] <= 256):
                raise ValueError('Invalid active-row boundary format')
        elif row['transform'] == 'palette_group_index':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 4 or row['count'] != 1 or
                    len(g) != 8 or any(type(n) is not int for n in g) or
                    not 1 <= g[0] <= 256 or not 0 <= g[1] < 8 or
                    not 0 < g[2] <= (255 >> g[1]) or not 1 <= g[3] <= 256 or
                    not 1 <= g[4] <= 256 or not 1 <= g[5] <= min(g[3],g[4]) or
                    not 1 <= g[6] <= 256 or g[7] not in (0,1) or
                    row['source_count'] <= g[0] or (row['source_count']-g[0]) % 2 or
                    not (g[6]-1)*g[4]+g[5] <= (row['source_count']-g[0])//2 <= 65536):
                raise ValueError('Invalid palette-group representation/format')
        elif row['transform'] in ('palette_pair_value','arm64_palette_boolean_value', 'arm64_palette_boolean_result','arm64_palette_optional_search'):
            validate_palette_pair(row)
        elif row['transform'] == 'unique_value_index':
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] not in (1,2) or row['native_width'] != 4 or
                    row['count'] != 1 or not 2 <= row['source_count'] <= 65536 or row['groups']):
                raise ValueError('Invalid unique-index representation')
        elif row['transform'] == 'scrolled_grid_cells':
            groups = row['groups']
            columns = row['source_count'] - 12
            if (any(type(row[k]) is not int for k in ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 4 or not 1 <= columns <= 255 or
                    not 1 <= row['count'] <= 255 * columns or row['count'] % columns or len(groups) != 5 or
                    any(type(n) is not int for n in groups) or not 0 <= groups[0] <= 65535 or
                    not 1 <= groups[1] <= 4 or any(not 1 <= n <= 256 for n in groups[2:])):
                raise ValueError('Invalid scrolled-grid representation/format')
        elif row['transform'] in ('coalesced_text_region','arm64_text_region_copy','arm64_text_region_clear'):
            g=row['groups']; native=row['transform']!='coalesced_text_region'
            copy=row['transform']=='arm64_text_region_copy'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width']!=2 or row['native_width']!=(1 if native else 4) or
                    row['native_representation']!=('generated_native_block' if native else 'generated_block_input') or
                    (native and (row['count']>4096 or row['count']%4)) or (not native and row['count']!=3) or
                    len(g)!=(8 if copy else 5) or any(type(v) is not int for v in g) or
                    not 0<=g[0]<=65535 or not 1<=g[1]<=4 or any(not 1<=v<=256 for v in g[2:4]) or
                    not 1<=g[4]<=32 or g[2]*g[1]*(g[3]+1)>65536-g[0] or
                    not 3<=row['source_count']-g[4]<=96 or (row['source_count']-g[4])%3 or
                    (copy and (not 18<=g[5]<31 or any(v<0 or v>32760 or v%8 for v in g[6:])))):
                raise ValueError('Invalid text-region representation/format')
        elif row['transform'] in ('label_region_extent','arm64_label_region_clear'):
            g=row['groups']; native=row['transform']=='arm64_label_region_clear'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width']!=2 or row['native_width']!=(1 if native else 4) or
                    (native and (row['count']>4096 or row['count']%4)) or (not native and row['count']!=2) or
                    row['native_representation']!=('generated_native_block' if native else 'generated_block_input') or
                    len(g)!=5 or any(type(v) is not int for v in g) or any(not 1<=v<=32 for v in g[:2]) or
                    not 1<=g[2]<=4 or any(not 1<=v<=256 for v in g[3:]) or row['source_count']!=sum(g[:2])):
                raise ValueError('Invalid label-region representation/format')
        elif row['transform'] == 'wrapped_scroll_schedule':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 2 or row['native_width'] != 4 or
                    not 1 <= row['count'] <= 64 or row['source_count'] != row['count'] + 1 or
                    len(g) != 5 or any(type(v) is not int for v in g) or
                    not 0 <= g[0] <= 65535 or any(not 1 <= v <= 256 for v in g[1:4]) or
                    not 1 <= g[4] <= 4 or g[1] * g[2] * g[4] > 65536 - g[0]):
                raise ValueError('Invalid wrapped scroll format')
        elif row['transform'] == 'scrolled_cell_coordinates':
            groups = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 2 or row['native_width'] != 4 or
                    not 0 < row['count'] <= 65536 or row['count'] % 2 or
                    row['source_count'] != row['count'] or len(groups) != 6 or
                    any(type(n) is not int for n in groups) or not 0 <= groups[0] <= 65535 or
                    not 1 <= groups[1] <= 4 or any(not 1 <= n <= 256 for n in groups[2:5]) or
                    not 0 <= groups[5] < groups[4]):
                raise ValueError('Invalid scrolled-cell representation/format')
        elif row['transform'] == 'scrolled_row_cells':
            groups = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 4 or not 1 <= row['count'] <= 256 or
                    row['source_count'] != 8 + row['count'] or len(groups) != 5 or
                    any(type(n) is not int for n in groups) or not 0 <= groups[0] <= 65535 or
                    not 1 <= groups[1] <= 4 or any(not 1 <= n <= 256 for n in groups[2:])):
                raise ValueError('Invalid scrolled-row representation/format')
        elif row['transform'] == 'mosaic_envelope':
            if (any(type(row[k]) is not int for k in ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 4 or row['count'] != 3 or
                    not 4 <= row['source_count'] <= 32 or row['groups']):
                raise ValueError('Invalid mosaic envelope representation')
        elif row['transform'] in ('affine_copy_layout', 'arm64_affine_copy'):
            generated = row['transform'] == 'arm64_affine_copy'
            if (any(type(row[k]) is not int for k in ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] != 1 or not 9 <= row['source_count'] <= 114 or
                    (row['source_count'] - 2) % 7 or row['groups'] or
                    row['native_width'] != (1 if generated else 4) or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input') or
                    (generated and (row['count'] > 4096 or row['count'] % 4)) or
                    (not generated and row['count'] != 7)):
                raise ValueError('Invalid generated affine copy layout/extent')
        elif row['transform'] in ('tile_run_layout', 'arm64_tile_runs'):
            generated = row['transform'] == 'arm64_tile_runs'
            if (any(type(row[k]) is not int for k in ('source_count', 'count', 'source_width', 'native_width')) or
                    row['source_width'] != 1 or not 20 <= row['source_count'] <= 256 or
                    row['source_count'] % 4 or row['groups'] or
                    row['native_width'] != (1 if generated else 4) or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input') or
                    (generated and (row['count'] > 4096 or row['count'] % 4)) or
                    (not generated and row['count'] != 8)):
                raise ValueError('Invalid generated native tile-run layout/extent')
        elif row['transform'] in ('number_choice_values', 'arm64_number_choice'):
            g = row['groups']; generated = row['transform'] == 'arm64_number_choice'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or len(g) != (16 if generated else 10) or
                    any(type(value) is not int for value in g) or not 1 <= g[0] <= 256 or
                    not 1 <= g[2] <= 3 or not 0 <= g[1] <= 8-g[2] or
                    not 0 <= g[3] <= 65535 or not 1 <= g[4] <= 4 or
                    any(not 1 <= value <= 256 for value in g[5:8]) or not 0 <= g[8] < g[7] or
                    not 2*g[0]+4 <= row['source_count'] <= 2*g[0]+514 or row['source_count'] % 2 or
                    row['native_width'] != (1 if generated else 4) or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input')):
                raise ValueError('Invalid numbered choice input format')
            positions = row['source_count'] - 2*g[0] - 2
            if (not generated and (g[9] not in (0,1,2) or row['count'] != (positions if g[9] == 2 else 1)) or
                    generated and (row['count'] not in range(28,41,4) or not 0 <= g[9] < positions//2 or
                        not 0 <= g[10] <= 4095 or g[10] % 8 or not 0 <= g[11] <= 65535 or
                        not 1 <= g[12] <= 10 or not 0 <= g[13] < 14 or
                        any(not 0 <= value < 2**32 or value % 4 for value in g[14:]) or
                        g[14]+row['count'] > 2**32 or g[14] <= g[15] < g[14]+row['count'] or
                        not -(1 << 27) <= g[15]-(g[14]+row['count']-4) < (1 << 27))):
                raise ValueError('Invalid numbered choice projection/native ABI')
        elif row['transform'] in ('row_band_pattern','arm64_row_band_fill'):
            g=row['groups']; native=row['transform']=='arm64_row_band_fill'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_count'] != 4 or row['source_width'] != 2 or
                    row['native_width'] != (1 if native else 4) or len(g) != (7 if native else 3) or
                    any(type(v) is not int for v in g) or not 32 <= g[1] <= 256 or g[1]%32 or
                    not 0 <= g[0] <= 65536-g[1]*4 or
                    row['native_representation'] != ('generated_native_block' if native else 'constant_array')):
                raise ValueError('Invalid row-band format/representation')
            if native:
                size=g[1]+24
                if (row['count'] != size or not 0 <= g[2] <= 4095-g[1]*4 or g[2]%2 or
                        not 0 <= g[3] <= 2**32-size or g[3]%4 or
                        len(set(g[4:])) != 3 or any(not 0 <= v <= 2**32-16 or v%16 or
                            not (v+16 <= g[3] or v >= g[3]+size) for v in g[4:])):
                    raise ValueError('Invalid row-band native linkage')
            elif not 0 <= g[2] <= 3 or row['count'] != (12 if g[2] == 3 else 4):
                raise ValueError('Invalid row-band vector identity')
        elif row['transform'] == 'arm64_palette_initialization':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_count'],row['count'],row['source_width'],row['native_width']) != (18,88,2,1) or
                    row['native_representation'] != 'generated_native_block' or
                    len(g) != 7 or any(type(value) is not int for value in g) or
                    not 0 <= g[0] <= 65536-512 or not 1 <= g[2] <= 16 or
                    not 0 <= g[1] <= 65536-g[2]*32 or not 0 <= g[3] <= 65535 or
                    g[1] <= g[3] < g[1]+g[2]*32 or
                    not 0 <= g[4] <= 4095-15*32 or g[4] % 2 or
                    not 0 <= g[5] <= 1024-g[2]*32 or g[5] % 16 or
                    not 0 <= g[6] <= 992 or g[6] % 16):
                raise ValueError('Invalid native palette initialization format')
            spans = [(g[4],512),(g[5],g[2]*32),(g[6],32)]
            if any(a < b+m and b < a+n for i,(a,n) in enumerate(spans) for b,m in spans[i+1:]):
                raise ValueError('Overlapping native palette initialization members')
        elif row['transform'] == 'arm64_bit_number_prefix':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 1 or
                    row['native_representation'] != 'generated_native_block' or
                    row['count'] not in range(12,33,4) or len(g) != 10 or
                    any(type(value) is not int for value in g) or
                    not 1 <= g[0] <= 256 or not 1 <= g[2] <= 3 or not 0 <= g[1] <= 8-g[2] or
                    row['source_count'] != 2*g[0]+2 or not 6 <= g[3] < 31 or not 0 <= g[4] < 32 or
                    g[5] not in (0,1) or not (g[6] == 31 or
                        6 <= g[6] < 31 and g[6] != g[3] and g[4] == 0 and g[5] == 1) or
                    not 0 <= g[7] <= 4096 or g[7] % 8 or not 0 <= g[8] <= 65535 or
                    not 1 <= g[9] <= 10):
                raise ValueError('Invalid bit-number prefix native format')
        elif row['transform'] == 'arm64_flag_caption':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 1 or
                    row['native_representation'] != 'generated_native_block' or
                    row['count'] not in range(32,45,4) or len(g) != 9 or
                    any(type(value) is not int for value in g) or
                    not 1 <= g[0] <= 256 or not 1 <= g[2] <= 3 or not 0 <= g[1] <= 8-g[2] or
                    row['source_count'] != 2*g[0]+2 or not 0 <= g[3] < 14 or
                    not 0 <= g[4] <= 4095 or g[4] % 8 or not 0 <= g[5] < 2**32 or
                    not 1 <= g[6] <= 65535 or g[5]+g[6] > 2**32 or
                    any(not 0 <= value < 2**32 or value % 4 for value in g[7:]) or
                    g[7]+row['count'] > 2**32 or g[7] <= g[8] < g[7]+row['count'] or
                    not (g[5]+g[6] <= g[7] or g[5] >= g[7]+row['count']) or
                    not -(1 << 27) <= g[8]-(g[7]+row['count']-4) < (1 << 27)):
                raise ValueError('Invalid flag caption native format')
        elif row['transform'] == 'arm64_dimension_choice':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_width'],row['native_width']) != (1,1) or len(g) not in (5,7) or
                    any(type(v) is not int for v in g) or not 1 <= g[0] <= 256 or
                    row['source_count'] != 2*g[0] or not 0 <= g[1] <= 2 or
                    not 0 <= g[2] < 31 or not 0 <= g[3] < 31 or g[2] == g[3] or
                    not 0 <= g[4] < 14 or
                    row['count'] not in (range(8,21,4) if len(g)==7 else range(4,17,4)) or
                    (len(g)==7 and (not 0 <= g[5] < 31 or not 0 <= g[6] <= 4095)) or
                    row['native_representation'] != 'generated_native_block'):
                raise ValueError('Invalid dimension choice native ABI')
        elif row['transform'] == 'arm64_coordinate_pair':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_width'],row['native_width'],row['source_count']) != (1,1,2) or
                    row['count'] not in (8,12) or len(g) != 3 or
                    any(type(v) is not int or not 0 <= v < 31 for v in g) or
                    g[1] in (g[0],g[2]) or row['native_representation'] != 'generated_native_block'):
                raise ValueError('Invalid coordinate-pair native ABI')
        elif row['transform'] in ('text_field_metric', 'arm64_text_column_fragment'):
            g = row['groups']; native = row['transform'] == 'arm64_text_column_fragment'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != (1 if native else 4) or
                    not 1 <= row['source_count'] <= 256 or len(g) != (10 if native else 4) or
                    any(type(v) is not int for v in g) or not 1 <= g[0] < 32 or g[0] in (4,5) or
                    not 1 <= g[1] <= g[2] <= (64 if native else 256) or
                    (not native and (row['count'] != 1 or g[3] not in (0,1))) or
                    (native and (not 0 < row['count'] <= 4096 or row['count'] % 4)) or
                    (native and row['native_representation'] != 'generated_native_block') or
                    (not native and row['native_representation'] not in ('reviewed_fields','generated_block_input'))):
                raise ValueError('Invalid text-column metric format')
            if native and (g[3] not in (0,1) or not 0 <= g[5] < 31 or not 0 <= g[6] < 31 or g[5] == g[6] or
                    (g[3] == 0 and (not 1 <= g[4] <= 2 or not 0 <= g[7] < 32 or
                     any(not 0 <= v < 31 or v in (g[5],g[6]) for v in g[8:10]))) or
                    (g[3] == 1 and (g[4] != 0 or any(g[7:]) or row['count'] != 8))):
                raise ValueError('Invalid text-column native ABI')
        elif row['transform'] == 'text_field_columns':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 4 or
                    not 1 <= row['source_count'] <= 256 or not 1 <= row['count'] <= 32 or
                    len(g) != 3 or any(type(v) is not int for v in g) or
                    not 1 <= g[0] < 32 or g[0] in (4,5) or not 1 <= g[1] <= g[2] <= 256 or
                    row['native_representation'] != 'reviewed_fields'):
                raise ValueError('Invalid text field column format')
        elif row['transform'] in ('condition_cell_value', 'arm64_cell_condition'):
            g = row['groups']; native = row['transform'] == 'arm64_cell_condition'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != (1 if native else 4) or
                    len(g) != (10 if native else 6) or any(type(v) is not int for v in g) or
                    not 1 <= g[0] <= 256 or not 1 <= g[2] <= 3 or not 0 <= g[1] <= 8-g[2] or
                    row['source_count'] != 2*g[0]+2 or not 0 <= g[3] <= 65535 or not 1 <= g[4] <= 64 or
                    row['native_representation'] != ('generated_native_block' if native else 'generated_block_input') or
                    (not native and (row['count'] != 1 or g[5] not in (0,1,2))) or
                    (native and (row['count'] not in range(8,21,4) or not 0 <= g[5] < 31 or
                        not 0 <= g[6] < 14 or not 0 <= g[7] < 31 or not 0 <= g[8] < 31 or
                        not 0 <= g[9] <= 16380 or g[9] % 4))):
                raise ValueError('Invalid conditional cell source/native format')
        elif row['transform'] in ('condition_choice_value', 'arm64_condition_choice'):
            g = row['groups']; native = row['transform'] == 'arm64_condition_choice'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or len(g) not in ((5,7,8,9) if native else (4,)) or
                    any(type(v) is not int for v in g) or not 1 <= g[0] <= 256 or
                    not 1 <= g[2] <= 3 or not 0 <= g[1] <= 8-g[2] or
                    row['source_count'] != 2*g[0] or row['native_width'] != (1 if native else 4) or
                    row['native_representation'] != ('generated_native_block' if native else 'generated_block_input') or
                    (not native and (row['count'] != 1 or g[3] not in (0,1))) or
                    (native and (row['count'] not in (range(8,21,4) if len(g) in (8,9) else (4,) if len(g)==7 else range(4,17,4)) or
                                 not 0 <= g[3] < 31 or not 0 <= g[4] < 14 or
                                 (len(g)==7 and (not 0 <= g[5] < 31 or not 0 <= g[6] < 8)) or
                                 (len(g)==9 and (g[5] not in (0,1) or not 0 <= g[6] < 31 or
                                     (g[5]==0 and (not 0 <= g[7] <= 4095 or g[8]!=0)) or
                                     (g[5]==1 and (not 0 <= g[7] < 31 or not 0 <= g[8] <= 16380 or g[8]%4)))) or
                                 (len(g)==8 and (not 0 <= g[5] < 31 or not 0 <= g[6] < 31 or
                                                g[5]==g[3] or not 0 <= g[7] < 14))))):
                raise ValueError('Invalid condition choice format/native ABI')
        elif row['transform'] in ('caption_choice_value', 'arm64_caption_choice'):
            g = row['groups']; generated = row['transform'] == 'arm64_caption_choice'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or len(g) != (8 if generated else 4) or
                    any(type(value) is not int for value in g) or not 1 <= g[0] <= 256 or
                    not 1 <= g[2] <= 3 or not 0 <= g[1] <= 8-g[2] or
                    row['source_count'] != 2*g[0]+2 or
                    row['native_width'] != (1 if generated else 4) or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input') or
                    (not generated and (row['count'] != 1 or not 0 <= g[3] < 3)) or
                    (generated and (row['count'] not in range(16,33,4) or
                        not 0 <= g[3] < 31 or g[4] not in (0,1) or not 0 <= g[5] <= 65535 or
                        any(not 0 <= value < 2**32 or value % 4 for value in g[6:]) or
                        g[6] + row['count'] > 2**32 or g[6] <= g[7] < g[6]+row['count'] or
                        not -(1 << 27) <= g[7]-(g[6]+row['count']-4) < (1 << 27)))):
                raise ValueError('Invalid caption choice layout/native format')
        elif row['transform'] in ('byte_choice_value', 'arm64_byte_choice'):
            g = row['groups']; generated = row['transform'] == 'arm64_byte_choice'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != (1 if generated else 4) or
                    (row['count'] not in (8,20) if generated else row['count'] != 1) or
                    len(g) != (8 if generated else 2) or any(type(v) is not int for v in g) or
                    not 1 <= g[0] <= 256 or row['source_count'] != 2*g[0] or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input') or
                    (generated and (any(not 0 <= v < 31 for v in g[1:7]) or g[3] in g[1:3] or
                                    g[4] in g[1:4] or not 0 <= g[7] < 64)) or
                    (not generated and g[1] not in (0,1))):
                raise ValueError('Invalid byte-choice value/native metadata')
        elif row['transform'] in ('clamped_phase_value', 'arm64_clamped_phase'):
            g = row['groups']; generated = row['transform'] == 'arm64_clamped_phase'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != (1 if generated else 4) or
                    row['count'] != (16 if generated else 1) or len(g) != (7 if generated else 3) or
                    any(type(value) is not int for value in g) or
                    not 1 <= g[0] <= 256 or not 1 <= g[1] <= 256 or
                    row['source_count'] != g[0]+2*g[1] or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input') or
                    (generated and (not 1 <= g[2] <= 8 or any(not 0 <= v < 31 for v in g[3:]) or
                                    len(set(g[3:6])) != 3))):
                raise ValueError('Invalid clamped phase layout/native format')
            if not generated and g[2] not in (0,1):
                raise ValueError('Invalid clamped phase component')
        elif row['transform'] in ('arm64_inline_byte_append', 'arm64_stack_byte_argument'):
            validate_byte_publication(row)
        elif row['transform'] == 'arm64_tagged_byte_constant':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_count'],row['count'],row['source_width'],row['native_width']) != (1,4,1,1) or
                    row['native_representation'] != 'generated_native_block' or
                    len(g) != 4 or any(type(v) is not int for v in g) or
                    not 1 <= g[1] <= 8 or not 0 <= g[0] <= 8-g[1] or
                    not 0 <= g[2] < 31 or not 0 <= g[3] <= 255):
                raise ValueError('Invalid tagged-byte constant format')
        elif row['transform'] == 'arm64_boolean_argument':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_count'],row['source_width'],row['native_width']) != (1,1,1) or
                    row['count'] not in (8,12,16) or
                    row['native_representation'] != 'generated_native_block' or
                    len(g) != 6 or any(type(value) is not int for value in g) or
                    not 1 <= g[1] <= 3 or not 0 <= g[0] <= 8-g[1] or
                    not 0 <= g[2] < 31 or not 0 <= g[3] < 8 or
                    any(not 0 <= value < 2**32 or value % 4 for value in g[4:]) or
                    g[4] + row['count'] > 2**32 or g[4] <= g[5] < g[4] + row['count'] or
                    not -(1 << 27) <= g[5] - (g[4] + row['count'] - 4) < (1 << 27)):
                raise ValueError('Invalid native Boolean argument format')
        elif row['transform'] == 'arm64_selection_branch':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','source_width','native_width','count')) or
                    (row['source_width'],row['native_width']) != (2,1) or
                    row['native_representation'] != 'generated_native_block' or len(g) not in (7,9) or
                    any(type(v) is not int for v in g) or not 0 < g[1] <= 5 or
                    not 0 <= g[0] <= 16-g[1] or not 0 <= g[2] <= 11-g[1] or
                    any(not 0 <= v < 31 for v in g[3:5]) or g[3] == g[4] or
                    not 4 <= g[5] < (1 << 18) or g[6] not in (1,2) or
                    not 0 < row['source_count'] <= 256 or
                    (len(g) == 9 and (g[6] != 1 or not 0 <= g[7] < 31 or not 0 <= g[8] <= 32760 or g[8] % 8)) or
                    (g[6] == 1 and (g[2] != 0 or row['count'] not in ((4,8) if len(g) == 7 else (8,12)))) or
                    (g[6] == 2 and (row['source_count'] != 2 or row['count'] not in (8,12,16)))):
                raise ValueError('Invalid native selection branch format')
        elif row['transform'] == 'arm64_split_index_branch':
            g = row['groups']
            if (any(type(row[k]) is not int for k in ('source_count','source_width','native_width','count')) or
                    (row['source_width'],row['native_width'],row['count']) != (2,1,8) or
                    row['native_representation'] != 'generated_native_block' or len(g) != 9 or
                    any(type(v) is not int for v in g) or not 0 <= g[0] <= 65535 or
                    not 0 < g[1] <= 256 or not 0 < g[2] <= 256 or row['source_count'] != g[2] or
                    not 0 < g[3] <= 11 or not 0 < g[4] <= 12-g[3] or
                    any(not 0 <= v < 31 for v in g[5:8]) or len(set(g[5:8])) != 3 or
                    not 0 < g[8] < (1 << 18)):
                raise ValueError('Invalid native split-index branch format')
        elif row['transform'] in ('particle_pair_value','arm64_particle_pairs'):
            g=row['groups']; native=row['transform']=='arm64_particle_pairs'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_count'],row['source_width'],row['native_width'])!=(6,1,1 if native else 4) or
                    row['native_representation']!=('generated_native_block' if native else 'generated_block_input') or
                    len(g)!=(1 if native else 2) or any(type(v) is not int for v in g) or not 0<g[0]<=65535 or
                    (not native and g[1] not in (0,1)) or
                    row['count'] not in (range(348,1289,188) if native else (1,))):
                raise ValueError('Invalid paired-particle representation')
        elif row['transform'] in ('paired_tile_copy_layout', 'arm64_paired_tile_copy'):
            g=row['groups']; native=row['transform']=='arm64_paired_tile_copy'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_count'],row['source_width'],row['native_width'])!=(14,1,1 if native else 4) or
                    row['native_representation']!=('generated_native_block' if native else 'generated_block_input') or
                    len(g)!=(6 if native else 2) or any(type(v) is not int for v in g) or
                    not 0<=g[0]<65536 or g[0]%16 or not 1<=g[1]<=512 or g[1]*16>65536-g[0] or
                    (not native and row['count']!=5)):
                raise ValueError('Invalid paired tile-copy representation')
            if native and (not 96<=row['count']<=656 or (row['count']-96)%80 or
                    not 0<=g[2]<=32752 or g[2]%8 or g[3]!=g[2]+8 or
                    any(not 0<=v<=2**32-1 or v%4 for v in g[4:]) or
                    g[4]>2**32-1-656 or g[4]<=g[5]<g[4]+row['count'] or
                    not -262144<=(g[5]-g[4]-row['count']+4)//4 or
                    (g[5]-g[4])//4>=262144):
                raise ValueError('Invalid native paired tile-copy ABI')
        elif row['transform'] in ('banked_word_copy_value', 'arm64_banked_word_copy'):
            g = row['groups']; generated = row['transform'] == 'arm64_banked_word_copy'
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    (row['source_count'],row['source_width']) != (2,2) or
                    row['native_width'] != (1 if generated else 4) or
                    row['native_representation'] != ('generated_native_block' if generated else 'generated_block_input') or
                    len(g) != (7 if generated else 4) or any(type(x) is not int for x in g) or
                    not 0 <= g[0] < 65536 or g[0] % 2 or not 0 < g[1] <= 65536-g[0] or
                    not 2 <= g[2] <= 128 or g[2] & (g[2]-1) or g[1] % g[2] or
                    (not generated and (row['count'] != 1 or g[3] not in (0,1))) or
                    (generated and (not 12 <= row['count'] <= 140 or row['count'] % 4 or
                     any(not 0 <= x < 31 for x in g[3:6]) or len(set(g[3:6])) != 3 or
                     not 0 <= g[6] <= 4095))):
                raise ValueError('Invalid banked word-copy representation')
        elif row['transform'] == 'arm64_vector_rows':
            g = row['groups']
            if (row['native_representation'] != 'generated_native_block' or
                    (row['source_count'],row['source_width'],row['native_width']) != (1,2,1) or
                    not 12 <= row['count'] <= 2048 or row['count'] % 4 or len(g) != 8 or
                    any(type(v) is not int for v in g) or not 0 <= g[0] <= 4 or
                    not 0 <= g[1] < 31 or not 0 <= g[2] < 31 or g[1] == g[2] or
                    not 0 <= g[3] < 32 or not 0 <= g[4] <= 65535 or
                    not 1 <= g[5] <= 4096 or not 1 <= g[6] <= 32 or not 0 <= g[7] < (1 << g[6]) or
                    g[4]+(g[6]-1)*g[5] > 65535 or (g[6] > 1 and g[4]+(g[6]-2)*g[5] > 4095)):
                raise ValueError('Invalid vector-row native format')
        elif row['transform'] == 'arm64_vector_fill':
            g = row['groups']
            if (row['native_representation'] != 'generated_native_block' or
                    (row['source_count'],row['source_width'],row['native_width']) != (1,2,1) or
                    not 4 <= row['count'] <= 32 or row['count'] % 4 or len(g) != 4 or
                    any(type(v) is not int for v in g) or not 0 <= g[0] <= 4 or
                    not 0 <= g[1] < 31 or not 0 <= g[2] < 32 or g[3] not in (0,1)):
                raise ValueError('Invalid vector-fill native format')
        elif row['transform'] == 'arm64_short_fill':
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['native_representation'] != 'generated_native_block' or
                    (row['source_count'],row['count'],row['source_width'],row['native_width']) != (1,8,2,1) or
                    len(row['groups']) != 2 or
                    any(type(n) is not int or not 0 <= n < 31 for n in row['groups']) or
                    row['groups'][0] == row['groups'][1]):
                raise ValueError('Invalid short-fill native format')
        elif row['transform'] == 'arm64_gray_fill':
            if (row['native_representation'] != 'generated_native_block' or
                    row['source_width'] != 1 or row['native_width'] != 1 or
                    row['source_count'] % 2 or row['source_count'] > 448 or
                    row['count'] > 4096 or row['count'] % 4 or
                    len(row['groups']) != 3 or
                    any(type(n) is not int or not 0 <= n < 2**32 for n in row['groups']) or
                    any(n % alignment for n, alignment in zip(row['groups'], (4, 16, 4))) or
                    row['sites']):
                raise ValueError('Invalid generated native fill linkage/extent')
        elif row['transform'] == 'arm64_inline_gray_fill':
            if (row['native_representation'] != 'generated_native_block' or
                    row['source_width'] != 1 or row['native_width'] != 1 or
                    row['source_count'] % 2 or row['source_count'] > 448 or
                    row['count'] > 4096 or row['count'] % 4 or row['groups'] or row['sites']):
                raise ValueError('Invalid inline native fill extent')
        elif row['groups'] and row['transform'] not in ('tiled_transfer_schedule', 'coordinate_combination', 'linear_coordinate_series'):
            raise ValueError('Unexpected material groups')
        if not row['owner'] or not row['evidence']:
            raise ValueError('Unreviewed material')
        for key in ('sha256', 'native_sha256'):
            if not re.fullmatch(r'[0-9a-f]{64}', row[key]):
                raise ValueError('Invalid material hash')
    # Evidence/status text must not invalidate already-reviewed restoration maps.
    semantic = [{k: row[k] for k in ('id', 'offset', 'count', 'source_width',
                 'native_width', 'sha256', 'native_sha256', 'source_count', 'transform', 'sites', 'groups',
                 'decoded_source', 'container', 'native_representation')}
                for row in doc['materials']]
    profiles = None
    if 'compiled_fields' in doc:
        from native_fields import load_profiles
        name = doc['compiled_fields']
        if not isinstance(name, str) or Path(name).name != name:
            raise ValueError('Invalid compiled field profile location')
        profiles = load_profiles(path.with_name(name), doc['materials'])
        semantic = dict(materials=semantic, compiled_fields=profiles['profiles'])
        if profiles.get('blocks'):
            semantic['compiled_blocks'] = profiles['blocks']
        if profiles.get('aliases'):
            semantic['compiled_aliases'] = profiles['aliases']
    from native_fields import material_field_coverage
    coverage = material_field_coverage(doc['materials'], profiles or {})
    for row in doc['materials']:
        if row['native_representation'] == 'reviewed_fields' or (
                row['native_representation'] == 'generated_block_input' and (row['count'] == 1 or
                row['transform'] in ('byte_choice_value', 'clamped_phase_value', 'number_choice_values', 'strided_row_layout', 'palette_cycle_layout', 'particle_strip_layout', 'particle_pair_value', 'paired_tile_copy_layout', 'coalesced_text_region', 'label_region_extent'))):
            # A reviewed scalar, including a scalar block input, accounts for every derived
            # native element. The extractor authenticates all raw input sites,
            # including agreeing sites coalesced into one scalar. Profile
            # hashes and the full-bundle comparison separately
            # establish the actual compiled locations; coverage alone is not
            # provenance evidence.
            covered = coverage[row['id']]
            if covered != set(range(row['count'])):
                raise ValueError('Incomplete reviewed native field coverage')
    blocks = profiles.get('blocks', []) if profiles else []
    for row in doc['materials']:
        if row['native_representation'] == 'generated_native_block':
            if sum(list(block['material'].values()).count(row['id']) for block in blocks) != 1:
                raise ValueError('Generated native material needs one authenticated owner')
        elif row['native_representation'] == 'generated_block_input':
            if not any(row['id'] in block['inputs'] for block in blocks):
                raise ValueError('Generated native input is not consumed by a reviewed block')
    digest = hashlib.sha256(json.dumps(semantic, sort_keys=True,
                                     separators=(',', ':')).encode()).hexdigest()
    return doc['materials'], digest


def main():
    rows, digest = load_specs(sys.argv[1])
    lines = ['// Extraction metadata only; no original material.',
             f'constexpr std::string_view material_recipe_hash = "{digest}";']
    for row in rows:
        for field in ('sites', 'groups'):
            if row[field]:
                lines.append('constexpr std::uint32_t ' + row['id'] + '_' + field + '[]{' +
                             ','.join(str(at) + 'U' for at in row[field]) + '};')
    lines.append('constexpr RomMaterialSpec material_specs[] = {')
    for row in rows:
        container = row['container']
        container_value = ('{' + ', '.join([str(container[k]) + 'U' for k in
            ('offset', 'size', 'decoded_size')] + [json.dumps(container[k]) for k in
            ('sha256', 'decoded_sha256')]) + '}') if container else '{}'
        values = [json.dumps(row['id']), str(row['offset']) + 'U',
                  str(row['count']) + 'U', str(row['source_width']) + 'U',
                  str(row['native_width']) + 'U', str(row['source_count']) + 'U',
                  json.dumps(row['sha256']), json.dumps(row['native_sha256']),
                  json.dumps(row['transform']), row['id'] + '_sites' if row['sites'] else '{}',
                  row['id'] + '_groups' if row['groups'] else '{}', container_value]
        lines.append('    {' + ', '.join(values) + '},')
    lines.append('};\n')
    doc = json.loads(Path(sys.argv[1]).read_text())
    profiles = json.loads(Path(sys.argv[1]).with_name(doc['compiled_fields']).read_text()) if 'compiled_fields' in doc else {}
    aliases = profiles.get('aliases', [])
    ids = {row['id']: i for i, row in enumerate(rows)}
    lines.append(f'constexpr std::array<RomMaterialAlias, {len(aliases)}> material_aliases{{{{')
    for alias in aliases:
        values = (ids[alias['material']], ids[alias['target']], alias['first'], alias['count'])
        lines.append('    {' + ', '.join(str(value) + 'U' for value in values) + '},')
    lines.append('}};\n')
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('\n'.join(lines))


if __name__ == '__main__':
    main()
