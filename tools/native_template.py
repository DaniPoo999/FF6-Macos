"""Strip reviewed ROM tables and emit content-free, build-bound restoration maps.

Replacement values are private outputs of the shared C++ extractor, never
constants in this tool. Complete constant arrays and hash-pinned scalar native
fields have separate removal paths. Other compiled representations remain open.
"""
import hashlib
from pathlib import Path
import struct
import json
import re

from generate_material_metadata import load_specs, validate_byte_publication, validate_palette_pair
from native_fields import PROJECTIONS, block_plan, field_plan, load_profiles, material_field_coverage


# Keep the format capacity aligned with the native Builder parser. This bounds
# metadata size only; authentication and per-field validation are unchanged.
MAX_RESTORATION_FIELDS = 16384

def sha(data):
    return hashlib.sha256(data).digest()


def link_identity_fields(data):
    """Locate only non-runtime build identities in a bounded ARM64 Mach-O.

    An SDK/linker update can change these even when every instruction, data byte,
    import and deployment requirement is identical. The full remaining prefix is
    authenticated before normalization; this cannot conceal a code/data change.
    """
    if (len(data) < 32 or len(data) > 64 * 1024 * 1024 or
            struct.unpack_from('<II', data) != (0xfeedfacf, 0x100000c)):
        raise ValueError('Unsupported native link identity image')
    count, size = struct.unpack_from('<II', data, 16)
    end = 32 + size
    if not 0 < count <= 1024 or end > len(data):
        raise ValueError('Invalid link identity command extent')
    fields, cursor, signature = {}, 32, None

    def field(key, at, width):
        if key in fields:
            raise ValueError('Duplicate link identity field')
        fields[key] = (at, width)

    for _ in range(count):
        if cursor + 8 > end:
            raise ValueError('Truncated link identity command')
        command, length = struct.unpack_from('<II', data, cursor)
        if length < 8 or length % 8 or cursor + length > end:
            raise ValueError('Invalid link identity command length')
        if command == 0x1b:  # LC_UUID: linker's content-derived build identity.
            if length != 24:
                raise ValueError('Invalid UUID command')
            field('uuid', cursor + 8, 16)
        elif command == 0x32:  # LC_BUILD_VERSION: retain platform and minimum OS.
            if length != 32 or struct.unpack_from('<I', data, cursor + 8)[0] != 1 or \
                    struct.unpack_from('<II', data, cursor + 20) != (1, 3):
                raise ValueError('Unsupported native build-version shape')
            field('sdk', cursor + 16, 4)
            field('linker', cursor + 28, 4)
        elif command in (0xc, 0x80000018):  # Load or weak-load dylib.
            if length < 32 or struct.unpack_from('<I', data, cursor + 8)[0] != 24:
                raise ValueError('Unsupported dependency command shape')
            name = data[cursor + 24:cursor + length]
            stop = name.find(b'\0')
            if stop <= 0 or any(name[stop:]):
                raise ValueError('Invalid dependency name')
            try:
                identity = name[:stop].decode('utf-8')
            except UnicodeError as error:
                raise ValueError('Invalid dependency encoding') from error
            field('dependency:' + identity, cursor + 16, 4)
        elif command == 0x1d:
            if length != 16 or signature is not None:
                raise ValueError('Invalid link identity signature command')
            signature, signature_size = struct.unpack_from('<II', data, cursor + 8)
            if signature < end or signature + signature_size != len(data):
                raise ValueError('Invalid link identity signature extent')
        cursor += length
    if cursor != end or signature is None or not {'uuid', 'sdk', 'linker'} <= fields.keys():
        raise ValueError('Incomplete native link identity')
    return signature, fields


def normalize_link_identity(data, profile):
    """Pin link metadata only when all runtime and structural bytes agree.

    Profiles store hashes, UUIDs and platform version metadata, never code/data.
    The package is re-signed immediately afterward. An incompatible compiler or
    SDK fails closed; neither the baseline nor its executable hash is refreshed.
    """
    if (not isinstance(profile, dict) or set(profile) != {
            'image_size', 'signed_prefix_size', 'masked_prefix_sha256',
            'restored_prefix_sha256', 'fields'} or
            type(profile['image_size']) is not int or profile['image_size'] != len(data) or
            type(profile['signed_prefix_size']) is not int or
            any(not isinstance(profile[key], str) or not re.fullmatch('[0-9a-f]{64}', profile[key])
                for key in ('masked_prefix_sha256', 'restored_prefix_sha256')) or
            not isinstance(profile['fields'], dict)):
        raise ValueError('Invalid native link identity profile')
    signature, fields = link_identity_fields(data)
    if signature != profile['signed_prefix_size'] or fields.keys() != profile['fields'].keys():
        raise ValueError('Native link identity layout changed')
    masked, out = bytearray(data[:signature]), bytearray(data)
    for key, (at, width) in fields.items():
        value = profile['fields'][key]
        if width == 16:
            if not isinstance(value, str) or not re.fullmatch('[0-9a-f]{32}', value):
                raise ValueError('Invalid native UUID identity')
            encoded = bytes.fromhex(value)
        else:
            if type(value) is not int or not 0 <= value <= 0xffffffff:
                raise ValueError('Invalid native version identity')
            encoded = struct.pack('<I', value)
        masked[at:at + width] = bytes(width)
        out[at:at + width] = encoded
    if sha(masked).hex() != profile['masked_prefix_sha256']:
        raise ValueError('Native runtime bytes differ from the frozen identity profile')
    if sha(out[:signature]).hex() != profile['restored_prefix_sha256']:
        raise ValueError('Canonical native link prefix does not match its frozen hash')
    return bytes(out)


def layout(data):
    if len(data) < 32 or struct.unpack_from('<II', data) != (0xfeedfacf, 0x100000c):
        raise ValueError('Restoration requires the reviewed ARM64 Mach-O profile')
    commands, size = struct.unpack_from('<II', data, 16)
    end = 32 + size
    if end > len(data) or commands > 1024:
        raise ValueError('Invalid Mach-O command extent')
    cursor, signature, sections = 32, None, []
    for _ in range(commands):
        if cursor + 8 > end:
            raise ValueError('Truncated Mach-O command')
        command, length = struct.unpack_from('<II', data, cursor)
        if length < 8 or cursor + length > end:
            raise ValueError('Invalid Mach-O command length')
        if command == 0x1d:
            if length != 16 or signature is not None:
                raise ValueError('Invalid or duplicate signature command')
            signature, signature_size = struct.unpack_from('<II', data, cursor + 8)
            if signature < end or signature + signature_size != len(data):
                raise ValueError('Unreviewed Mach-O signature placement')
        if command == 0x19:
            if length < 72:
                raise ValueError('Truncated segment command')
            count = struct.unpack_from('<I', data, cursor + 64)[0]
            if length != 72 + count * 80:
                raise ValueError('Invalid segment section extent')
            for index in range(count):
                at = cursor + 72 + index * 80
                name = data[at:at + 16].rstrip(b'\0')
                section_size = struct.unpack_from('<Q', data, at + 40)[0]
                offset = struct.unpack_from('<I', data, at + 48)[0]
                flags = struct.unpack_from('<I', data, at + 64)[0]
                if name in (b'__const', b'__literal4', b'__literal8', b'__literal16'):
                    if flags & 0x80000400 or offset < end or offset + section_size > len(data):
                        raise ValueError('Material candidate is not a bounded constant section')
                    sections.append((offset, offset + section_size))
        cursor += length
    if cursor != end or signature is None:
        raise ValueError('Missing signature or incomplete Mach-O command traversal')
    return signature, sections


def read_materials(spec_path, private_folder):
    specs, recipe = load_specs(spec_path)
    values = []
    for spec in specs:
        value = (Path(private_folder) / (spec['id'] + '.bin')).read_bytes()
        if len(value) != spec['count'] * spec['native_width'] or sha(value).hex() != spec['native_sha256']:
            raise ValueError('Private material input does not match reviewed recipe: ' + spec['id'])
        values.append(value)
    doc = json.loads(Path(spec_path).read_text())
    profiles = load_profiles(Path(spec_path).with_name(doc['compiled_fields']), specs) if 'compiled_fields' in doc else None
    return bytes.fromhex(recipe), values, [row['native_representation'] for row in specs], specs, profiles


def material_probes(materials, representations, specs=None):
    """Byte identity is useful for arrays, not individual reviewed scalars.

    A single item/song identity also occurs in unrelated native instructions,
    format metadata and hashes. Those occurrences do not establish provenance.
    Singleton reviewed_fields recipes and scalar generated-block inputs instead
    require complete, authenticated value coverage below and a source/compiled-
    owner review of their copies. Generated blocks still require whole removal.
    This exception does not clear any source or unresolved compiled structure.
    """
    if len(materials) != len(representations) or (specs is not None and len(specs) != len(materials)):
        raise ValueError('Material probe metadata extent mismatch')
    return [(index, value) for index, (value, representation) in
            enumerate(zip(materials, representations)) if representation != 'generated_native_block' and not (
                representation in ('reviewed_fields', 'generated_block_input') and specs is not None and
                specs[index]['count'] == 1)]


def load_artifact_probe_reviews(path, root, specs):
    """Admit only hash-bound reviews of incidental short icon-byte matches.

    This never exempts executable data or a material recipe from stripping.
    Compressed independently drawn pixels may contain the same few bytes by
    chance. Review pins the entire artifact, its producers and every match;
    neither a filename nor absence of a long-byte probe clears an artifact.
    """
    doc = json.loads(Path(path).read_text())
    if set(doc) != {'schema', 'artifacts'} or doc['schema'] != 1:
        raise ValueError('Invalid artifact probe review schema')
    identities = {row['id'] for row in specs}
    for name, row in doc['artifacts'].items():
        target = Path(name)
        if (target.is_absolute() or '..' in target.parts or
                target.parts[:2] != ('Contents', 'Resources') or target.suffix != '.icns' or
                set(row) != {'sha256', 'producers', 'matches', 'evidence'} or
                not re.fullmatch('[0-9a-f]{64}', row['sha256']) or
                not row['producers'] or not row['matches'] or
                not isinstance(row['evidence'], str) or not row['evidence'].strip()):
            raise ValueError('Invalid artifact probe review')
        for producer, expected in row['producers'].items():
            source = Path(producer)
            if (source.is_absolute() or '..' in source.parts or
                    source.suffix not in ('.cpp', '.mm', '.py') or
                    not re.fullmatch('[0-9a-f]{64}', expected) or
                    sha((Path(root) / source).read_bytes()).hex() != expected):
                raise ValueError('Artifact producer review is stale')
        for match in row['matches']:
            if (set(match) != {'material', 'offset', 'length'} or
                    match['material'] not in identities or type(match['offset']) is not int or
                    match['offset'] < 0 or type(match['length']) is not int or
                    not 1 <= match['length'] <= 4):
                raise ValueError('Invalid incidental artifact match review')
    return doc['artifacts']


def check_artifact_probe_matches(name, data, matches, reviews):
    review = reviews.get(name)
    if review is not None:
        if sha(data).hex() != review['sha256'] or matches != review['matches']:
            raise ValueError('Artifact match review is stale: ' + name)
    elif matches:
        raise ValueError('Unreviewed original-material byte match: ' + name)


def check_restoration_format_matches(data, matches, generated):
    """Classify only full format words in this build's authenticated field map.

    Source element index/count, fixed-point/bitfield shifts, width, source
    stride and projection IDs are metadata, not extracted values. Their
    concatenation can equal a short original table incidentally. Require exact
    bytes from write_map's current authenticated plan. Admit complete words
    wholly within index/count, the shift pair, or the final three representation
    words. Shift-pair matches additionally require a plain unit-scale conversion
    with no additive operand and valid widths. Hashes, ranges, material IDs,
    multipliers/addends and cross-boundary/partial-word matches remain rejected.
    The destination/width pair of an authenticated AdvSIMD structure-load field
    is also fixed ISA metadata, with its entire projection contract validated.
    This does not exempt any executable or weaken native content stripping.
    """
    if (data != generated or len(data) < 124 or data[:8] != b'FF6RST03' or
            struct.unpack_from('<I', data, 8)[0] != 3):
        raise ValueError('Restoration map differs from current generated proof')
    ranges = struct.unpack_from('<I', data, 12)[0]
    fields = struct.unpack_from('<I', data, 120)[0]
    start = 124 + ranges * 8
    if not 0 < ranges <= 4096 or fields > MAX_RESTORATION_FIELDS or start + fields * 44 != len(data):
        raise ValueError('Invalid restoration metadata extent')
    for match in matches:
        at, length = match['offset'], match['length']
        if (type(at) is not int or type(length) is not int or at < start or
                at % 4 or length not in (4, 8, 12) or at + length > len(data)):
            raise ValueError('Original-material match outside reviewed restoration format words')
        position = (at - start) % 44
        metadata = ((8 <= position and position + length <= 16) or
                    (32 <= position and position + length <= 44))
        if 24 <= position and position + length <= 32:
            row = at - position
            _, _, _, _, multiplier, addend, divisor, destination, width, _, projection = struct.unpack_from('<IIIIIiIIIII', data, row)
            metadata = (multiplier == 1 and addend == 0 and projection == 0 and
                        divisor <= 16 and 0 < width <= 24 and destination <= 32-width)
        if position == 28 and length == 8:
            row = at - position
            _, _, _, count, multiplier, addend, divisor, destination, width, stride, projection = struct.unpack_from('<IIIIIiIIIII', data, row)
            # LD1/LD2/LD3/LD4's four-bit operation selector occupies bits 12..15
            # regardless of the extracted interleave count. This exact pair
            # contains no selected source value or arithmetic adjustment.
            metadata = ((count, multiplier, addend, divisor, destination, width, stride, projection) ==
                        (1, 1, 0, 0, 12, 4, 1, PROJECTIONS['structure_load_opcode']))
        if not metadata:
            raise ValueError('Original-material match outside reviewed restoration format words')


def compressed_unwind_function_entries(data, symbols):
    """Authenticate linker-owned function records, never code or constant data.

    Mach-O compact_unwind_encoding.h defines these as a 24-bit function offset
    and an 8-bit encoding index. Short byte probes can span adjacent records.
    Require a bounded compressed-page format and independently known native
    symbol starts for every record. Encodings, padding, LSDA data and all other
    sections are deliberately outside the returned intervals.
    """
    signature, _ = layout(data)
    commands, size = struct.unpack_from('<II', data, 16)
    cursor, text, unwind = 32, None, None
    for _ in range(commands):
        command, length = struct.unpack_from('<II', data, cursor)
        if command == 0x19:
            segment = data[cursor+8:cursor+24].rstrip(b'\0')
            base, _, file_offset, _ = struct.unpack_from('<QQQQ', data, cursor+24)
            for index in range(struct.unpack_from('<I', data, cursor+64)[0]):
                at = cursor+72+index*80
                name = data[at:at+16].rstrip(b'\0')
                if name not in (b'__text', b'__unwind_info'):
                    continue
                address, extent = struct.unpack_from('<QQ', data, at+32)
                offset = struct.unpack_from('<I', data, at+48)[0]
                flags = struct.unpack_from('<I', data, at+64)[0]
                if (segment != b'__TEXT' or data[at+16:at+32].rstrip(b'\0') != segment or
                        file_offset != 0 or address-base != offset or offset < 32+size or
                        offset+extent > signature or not extent):
                    raise ValueError('Unreviewed native unwind section placement')
                if name == b'__text':
                    if text is not None or not flags & 0x80000400:
                        raise ValueError('Unreviewed native text section')
                    text = (offset,offset+extent)
                else:
                    if unwind is not None or flags != 0 or extent > 4*1024*1024:
                        raise ValueError('Unreviewed native unwind section')
                    unwind = (offset,offset+extent)
        cursor += length
    if unwind is None:
        return []
    if text is None or not (text[1] <= unwind[0] or unwind[1] <= text[0]):
        raise ValueError('Native unwind overlaps or lacks code')
    start, stop = unwind
    extent = stop-start
    def bounded(offset, count, width):
        if offset < 28 or offset % 4 or not 0 <= count <= 65536 or offset+count*width > extent:
            raise ValueError('Invalid compact unwind table extent')
    if extent < 28:
        raise ValueError('Truncated compact unwind header')
    version, common, common_count, personality, personalities, index, count = struct.unpack_from('<7I',data,start)
    if version != 1 or not 1 <= common_count <= 256 or personalities > 3 or not 2 <= count <= 65536:
        raise ValueError('Unsupported compact unwind index')
    bounded(common,common_count,4); bounded(personality,personalities,4); bounded(index,count,12)
    prefix_end = max(common+common_count*4,personality+personalities*4,index+count*12)
    rows = [struct.unpack_from('<III',data,start+index+i*12) for i in range(count)]
    if rows[-1][1] != 0 or any(a[0] >= b[0] for a,b in zip(rows,rows[1:])):
        raise ValueError('Invalid compact unwind sentinel/order')
    starts = set(symbols.values())
    intervals, pages = [], []
    for row, following in zip(rows,rows[1:]):
        base,page,lsda = row
        if (page < prefix_end or page % 4 or page+12 > extent or
                not text[0] <= base < min(following[0],text[1]) or following[0] > text[1]+1 or
                lsda < prefix_end or following[2] < lsda or (following[2]-lsda) % 8 or
                following[2] > page):
            raise ValueError('Invalid compact unwind page/index')
        kind, entries, entry_count, encodings, encoding_count = struct.unpack_from('<IHHHH',data,start+page)
        if (kind != 3 or entries < 12 or entries % 4 or not 1 <= entry_count <= 1021 or
                encodings < entries+entry_count*4 or encodings % 4 or common_count+encoding_count > 256 or
                encodings+encoding_count*4 > 4096 or page+encodings+encoding_count*4 > extent):
            raise ValueError('Unsupported compressed unwind page')
        end = page+encodings+encoding_count*4
        if any(page < old_end and old_start < end for old_start,old_end in pages):
            raise ValueError('Overlapping compact unwind pages')
        pages.append((page,end))
        previous = None
        for i in range(entry_count):
            value = struct.unpack_from('<I',data,start+page+entries+i*4)[0]
            function = base+(value & 0xffffff)
            # Apple's linker may place the final sentinel one byte beyond the
            # text extent. It is a range marker, never a function or admitted
            # byte interval; every actual function must still start in text.
            if (function % 4 or function not in starts or not base <= function < min(following[0],text[1]) or
                    (previous is None and function != base) or
                    (previous is not None and function <= previous) or value >> 24 >= common_count+encoding_count):
                raise ValueError('Unwind record lacks its native function or encoding')
            previous = function
        intervals.append((start+page+entries,start+page+entries+entry_count*4))
    return intervals


def chained_fixup_segment_metadata(data):
    """Validate pointer-format/segment-offset fields against Mach-O segments.

    Apple's mach-o/fixup-chains.h defines these linker-owned fields. Only their
    ten bytes qualify; page starts, imports, pointer payloads and adjacent bytes
    never do. This bounded 64-bit offset profile rejects other chain formats.
    """
    signature, _ = layout(data)
    commands, command_bytes = struct.unpack_from('<II', data, 16)
    cursor, segments, fixups = 32, [], None
    for _ in range(commands):
        command, length = struct.unpack_from('<II', data, cursor)
        if command == 0x19:
            name = data[cursor+8:cursor+24].rstrip(b'\0')
            vm, vm_size, file, file_size = struct.unpack_from('<QQQQ',data,cursor+24)
            limit = len(data) if name == b'__LINKEDIT' else signature
            if file_size and (file > limit or file_size > limit-file):
                raise ValueError('Fixup owner segment exceeds signed image')
            segments.append((name,vm,vm_size,file,file_size))
        elif command == 0x80000034:
            if fixups is not None or length != 16:
                raise ValueError('Invalid chained-fixups command')
            fixups = struct.unpack_from('<II',data,cursor+8)
        cursor += length
    if fixups is None:
        return []
    offset, extent = fixups
    owners = [s for s in segments if s[0] == b'__LINKEDIT']
    texts = [s for s in segments if s[0] == b'__TEXT']
    if (len(owners)!=1 or len(texts)!=1 or texts[0][3]!=0 or
            not 28 <= extent <= 4*1024*1024 or offset < 32+command_bytes or
            offset < owners[0][3] or offset+extent > owners[0][3]+owners[0][4] or
            offset+extent > signature):
        raise ValueError('Unbounded chained-fixups payload')
    version, starts, imports, names, count, import_format, name_format = struct.unpack_from('<7I',data,offset)
    if (version!=0 or import_format!=1 or name_format!=0 or
            not 28 <= starts < imports <= names < extent or starts%4 or imports%4 or
            count > 65536 or imports+count*4 != names):
        raise ValueError('Unsupported chained-fixups header')
    start = offset+starts
    n = struct.unpack_from('<I',data,start)[0]
    if n!=len(segments) or not 0<n<=1024 or starts+4+4*n > imports:
        raise ValueError('Chained-fixups segment directory differs')
    intervals, records = [], []
    for i, (name,vm,vm_size,file,file_size) in enumerate(segments):
        relative = struct.unpack_from('<I',data,start+4+4*i)[0]
        if not relative:
            continue
        at = start+relative
        if relative < 4+4*n or relative%4 or at+22 > offset+imports:
            raise ValueError('Invalid fixup segment record')
        size,page,kind,displacement,max_pointer,pages = struct.unpack_from('<IHHQIH',data,at)
        if (kind!=6 or page not in (4096,16384) or max_pointer!=0 or
                not pages or size!=22+2*pages or at+size > offset+imports or
                any(at<end and begin<at+size for begin,end in records) or
                name in (b'__TEXT',b'__LINKEDIT',b'__PAGEZERO') or not file_size or
                vm_size < file_size or vm < texts[0][1] or displacement!=vm-texts[0][1] or
                pages!=(file_size+page-1)//page):
            raise ValueError('Chained-fixups owner/format differs')
        for j in range(pages):
            first = struct.unpack_from('<H',data,at+22+2*j)[0]
            if first==65535:
                continue
            if first%4 or first>=page or j*page+first+8 > file_size:
                raise ValueError('Chained-fixup first pointer lies outside its page')
        records.append((at,at+size))
        intervals.append((at+6,at+16))
    return intervals


def extraction_spec_scalar_intervals(data, specs, symbols, sections):
    """Authenticate generated recipe coordinates, sizes and element widths.

    The ARM64 C++ layout is compile-time checked by rom_material.cpp. Only
    each record's five numeric format fields qualify; pointers, hashes, spans,
    container descriptors, padding and neighboring records never qualify.
    This is format validation, not a byte/hash allowlist for game material.
    """
    symbol = '__ZN3ff612_GLOBAL__N_114material_specsE'
    if symbol not in symbols:
        return []
    start = symbols[symbol]
    stride, prefix, extent = 168, 16, 20
    if (type(start) is not int or start < 0 or start % 8 or
            not 0 < len(specs) <= 4096 or
            not any(a <= start and start + stride*len(specs) <= z for a,z in sections)):
        raise ValueError('Invalid generated material-spec layout')
    intervals = []
    for index, row in enumerate(specs):
        keys = ('offset', 'count', 'source_width', 'native_width', 'source_count')
        if (any(type(row.get(k)) is not int or not 0 <= row[k] < 2**32 for k in keys) or
                row['count'] == 0 or row['source_count'] == 0 or
                row['source_width'] not in (1,2) or row['native_width'] not in (1,4)):
            raise ValueError('Invalid generated material-spec fields')
        at = start + index*stride + prefix
        expected = struct.pack('<5I', *(row[k] for k in keys))
        if data[at:at+extent] != expected:
            raise ValueError('Generated material-spec fields differ from recipes')
        intervals.append((at,at+extent))
    return intervals


def check_extraction_group_matches(data, matches, specs, symbols):
    """Identify incidental equality inside generated extraction-layout metadata.

    Recipe offsets/counts/widths, group cardinalities, bit positions, native-copy
    ABI, palette representation and admitted numbered-cell format
    fields describe extraction, not game values. Require the exact generator-owned
    symbol in a constant section and every generated word. This
    rule never admits instructions, extracted values, hashes or nearby bytes.
    The caller supplies validated recipes and symbols from the current image.
    Four-byte coincidences inside independently validated compact-unwind
    function records are also metadata, including cross-record byte matches.
    """
    _, sections = layout(data)
    intervals = extraction_spec_scalar_intervals(data, specs, symbols, sections)
    for row in specs:
        if row['transform'] not in ('grouped_equal_unsigned', 'unsigned_bit_fields', 'arm64_number_choice',
                                    'arm64_banked_word_copy', 'palette_group_index',
                                    'palette_pair_value', 'arm64_palette_boolean_value', 'arm64_palette_boolean_result', 'arm64_palette_optional_search',
                                    'arm64_inline_byte_append', 'arm64_stack_byte_argument'):
            continue
        groups = row['groups']
        if row['transform'] == 'grouped_equal_unsigned':
            if (len(groups) != row['count'] or not groups or
                    any(type(n) is not int or not 1 <= n <= 65536 for n in groups) or
                    sum(groups) != row['source_count']):
                raise ValueError('Invalid shared-group extraction metadata')
        elif row['transform'] == 'unsigned_bit_fields':
            if (type(row['count']) is not int or not 1 <= row['count'] <= 65536 or
                    type(row['source_count']) is not int or row['source_count'] != row['count'] or
                    type(row['source_width']) is not int or row['source_width'] not in (1, 2) or
                    type(row['native_width']) is not int or row['native_width'] != 4 or
                    len(groups) != row['count'] * 2 or
                    any(type(n) is not int for n in groups) or
                    any(shift < 0 or width < 1 or shift + width > row['source_width'] * 8
                        for shift, width in zip(groups[::2], groups[1::2]))):
                raise ValueError('Invalid bit-field extraction metadata')
        elif row['transform'] in ('arm64_inline_byte_append', 'arm64_stack_byte_argument'):
            validate_byte_publication(row)
        elif row['transform'] in ('palette_pair_value','arm64_palette_boolean_value', 'arm64_palette_boolean_result','arm64_palette_optional_search'):
            validate_palette_pair(row)
        elif row['transform'] == 'palette_group_index':
            # These words describe two palette layouts and the alias policy.
            # The selected bank and every color are ROM inputs, not group words.
            g = groups
            if (any(type(row[k]) is not int for k in ('source_count','count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 4 or row['count'] != 1 or
                    len(g) != 8 or any(type(n) is not int for n in g) or
                    not 1 <= g[0] <= 256 or not 0 <= g[1] < 8 or
                    not 0 < g[2] <= (255 >> g[1]) or not 1 <= g[3] <= 256 or
                    not 1 <= g[4] <= 256 or not 1 <= g[5] <= min(g[3],g[4]) or
                    not 1 <= g[6] <= 256 or g[7] not in (0,1) or
                    row['source_count'] <= g[0] or (row['source_count']-g[0]) % 2 or
                    not (g[6]-1)*g[4]+g[5] <= (row['source_count']-g[0])//2 <= 65536):
                raise ValueError('Invalid palette-group extraction metadata')
        elif row['transform'] == 'arm64_banked_word_copy':
            g = groups
            if (len(g) != 7 or any(type(n) is not int for n in g) or
                    any(type(row[k]) is not int for k in ('source_count','source_width','native_width','count')) or
                    (row['source_count'],row['source_width'],row['native_width']) != (2,2,1) or
                    row['native_representation'] != 'generated_native_block' or
                    not 12 <= row['count'] <= 140 or row['count'] % 4 or
                    not 0 <= g[0] < 65536 or g[0] % 2 or not 0 < g[1] <= 65536-g[0] or
                    not 2 <= g[2] <= 128 or g[2] & (g[2]-1) or g[1] % g[2] or
                    any(not 0 <= n < 31 for n in g[3:6]) or len(set(g[3:6])) != 3 or
                    not 0 <= g[6] <= 4095):
                raise ValueError('Invalid native word-copy ABI metadata')
        else:
            # Only the baseline and selected-cell index can participate here.
            # They are extraction/format coordinates, not the palette/position
            # outputs. Authenticate the entire generated metadata symbol below;
            # never extend this allowance into adjacent arguments or code.
            g = groups
            if (len(g) != 16 or any(type(n) is not int for n in g) or
                    any(type(row[k]) is not int for k in ('source_count','source_width','native_width')) or
                    row['source_width'] != 1 or row['native_width'] != 1 or
                    row['native_representation'] != 'generated_native_block' or
                    type(row['count']) is not int or row['count'] not in range(28,41,4) or
                    not 1 <= g[0] <= 256 or not 1 <= g[2] <= 3 or not 0 <= g[1] <= 8-g[2] or
                    not 0 <= g[3] <= 65535 or not 1 <= g[4] <= 4 or
                    any(not 1 <= n <= 256 for n in g[5:8]) or not 0 <= g[8] < g[7] or
                    not 2*g[0]+4 <= row['source_count'] <= 2*g[0]+514 or row['source_count'] % 2 or
                    not 0 <= g[9] < (row['source_count']-2*g[0]-2)//2 or
                    not 0 <= g[10] <= 4095 or g[10] % 8 or not 0 <= g[11] <= 65535 or
                    not 1 <= g[12] <= 10 or not 0 <= g[13] < 14 or
                    any(not 0 <= n < 2**32 or n % 4 for n in g[14:]) or
                    g[14]+row['count'] > 2**32 or g[14] <= g[15] < g[14]+row['count'] or
                    not -(1 << 27) <= g[15]-(g[14]+row['count']-4) < (1 << 27)):
                raise ValueError('Invalid numbered caller extraction metadata')
        name = row['id'] + '_groups'
        symbol = '__ZN3ff612_GLOBAL__N_1' + str(len(name)) + name + 'E'
        at = symbols.get(symbol)
        expected = struct.pack('<' + 'I' * len(groups), *groups)
        if (type(at) is not int or at < 0 or at % 4 or
                not any(start <= at and at + len(expected) <= end for start, end in sections) or
                data[at:at + len(expected)] != expected):
            raise ValueError('Generated extraction metadata differs from its reviewed recipe')
        # Only the four ABI words qualify for this copy profile. The selected
        # destination and extent are ROM operands, never metadata exceptions.
        # Paired palettes admit only the eight representation words. The
        # projection selector and native ABI/linkage suffix stay excluded.
        intervals.append((at, at + 32) if row['transform'] in (
                             'palette_pair_value','arm64_palette_boolean_value', 'arm64_palette_boolean_result','arm64_palette_optional_search')
                         else (at + 12, at + 28) if row['transform'] == 'arm64_banked_word_copy'
                         else (at + 8, at + len(expected) - 8)
                         if row['transform'] in ('arm64_inline_byte_append', 'arm64_stack_byte_argument')
                         else (at + 32, at + 40) if row['transform'] == 'arm64_number_choice'
                         else (at, at + len(expected)))
    # Distinct verified format arrays can be placed next to one another. A
    # coincidental probe may cross their boundary, but every byte must still
    # belong to authenticated metadata. Never bridge alignment gaps or extend
    # these intervals to hashes, pointers, selected values or instructions.
    covered = []
    for start, end in sorted(intervals):
        if covered and start <= covered[-1][1]:
            covered[-1] = (covered[-1][0], max(end, covered[-1][1]))
        else:
            covered.append((start, end))
    intervals = covered
    unwind = compressed_unwind_function_entries(data, symbols)
    fixups = chained_fixup_segment_metadata(data)
    for match in matches:
        at, length = match['offset'], match['length']
        if (type(at) is int and type(length) is int and length == 4 and
                any(start <= at and at+length <= end for start,end in unwind)):
            continue
        if (type(at) is int and type(length) is int and length in (4,8) and
                any(start <= at and at+length <= end for start,end in fixups)):
            continue
        if (type(at) is not int or type(length) is not int or at < 0 or at % 4 or
                length <= 0 or length % 4 or
                not any(start <= at and at + length <= end for start, end in intervals)):
            raise ValueError('Original-material match outside authenticated extraction layout')


def coalesce_material_ranges(ranges, materials):
    """Remove an already authenticated contained probe with its owning range.

    Equal or nested data need one restoration, not overlapping writes. Reject
    partial overlap and any disagreement rather than inventing replacement data.
    This never exempts bytes from stripping or scalar coverage requirements.
    """
    ordered = sorted(ranges, key=lambda row: (row[0], -len(materials[row[1]])))
    result = []
    for at, identity in ordered:
        value = materials[identity]
        if at < 0 or not value:
            raise ValueError('Invalid material location')
        if result:
            previous, owner = result[-1]
            parent = materials[owner]
            if at < previous + len(parent):
                relative = at - previous
                if relative + len(value) > len(parent) or parent[relative:relative + len(value)] != value:
                    raise ValueError('Overlapping material locations')
                continue
        result.append((at, identity))
    return result


def strip_image(path, materials, representations=None, specs=None, profiles=None):
    if representations is None:
        representations = ['constant_array'] * len(materials)
    if len(representations) != len(materials) or any(
            value not in ('constant_array', 'reviewed_fields', 'folded_pending',
                          'generated_native_block', 'generated_block_input') for value in representations):
        raise ValueError('Unreviewed native representation plan')
    path = Path(path)
    original = path.read_bytes()
    prefix, sections = layout(original)
    result = bytearray(original)
    ranges = block_plan(path, original, specs, materials, profiles) if profiles else []
    generated = {material for _, material in ranges}
    for material, representation in enumerate(representations):
        if representation == 'generated_native_block' and material not in generated:
            # The other executable has a distinct linked recipe. It may be
            # absent here only when the authenticated profile names that owner.
            if (specs is None or profiles is None or not any(
                    spec_id == specs[material]['id'] and binary != path.name
                    for block in profiles.get('blocks', [])
                    for binary, spec_id in block['material'].items())):
                raise ValueError('Missing generated native block removal plan')
        if representation == 'generated_block_input' and (specs is None or profiles is None or
                not any(specs[material]['id'] in block['inputs'] for block in profiles.get('blocks', []))):
            raise ValueError('Missing generated native input consumer')
    # An authenticated owner is not permission to leave a second compiled copy
    # elsewhere. Compare every generated fragment with the original image,
    # including the variant linked for the other executable.
    for material, representation in enumerate(representations):
        if representation != 'generated_native_block':
            continue
        value, cursor = materials[material], 0
        while (found := original.find(value, cursor)) >= 0:
            if not any(at == found and materials[owner] == value for at, owner in ranges):
                raise ValueError('Unreviewed duplicate generated native fragment')
            cursor = found + 1
    probes = material_probes(materials, representations, specs)
    for material, value in probes:
        # Count globally, then require each occurrence to be inside reviewed
        # constant sections. An optimizer moving it elsewhere must fail closed.
        positions, cursor = [], 0
        while (found := original.find(value, cursor)) >= 0:
            positions.append(found)
            cursor = found + 1
        if not positions:
            if representations[material] in ('reviewed_fields', 'folded_pending', 'generated_block_input'):
                # Scalar fields have their own authenticated removal pass below.
                # folded_pending additionally retains unresolved compiler shape
                # or other representations and continues to block publication.
                continue
            raise ValueError('Native material no longer has a reviewed contiguous representation')
        for at in positions:
            if not any(start <= at and at + len(value) <= stop for start, stop in sections):
                raise ValueError('Native material occurrence is outside reviewed data sections')
            ranges.append((at, material))
    ranges = coalesce_material_ranges(ranges, materials)
    end = 0
    for at, material in ranges:
        if at < end or at + len(materials[material]) > prefix:
            raise ValueError('Overlapping material locations')
        end = at + len(materials[material])
        result[at:end] = bytes(len(materials[material]))
    fields = field_plan(path, original, specs, materials, profiles) if profiles else []
    coverage = material_field_coverage(specs, profiles, path.name) if specs is not None and profiles else {}
    for material, representation in enumerate(representations):
        if representation == 'reviewed_fields' or (representation == 'generated_block_input' and
                                                   specs is not None and specs[material]['count'] == 1):
            covered = coverage.get(specs[material]['id'], set()) if specs is not None else set()
            if specs is None or covered != set(range(specs[material]['count'])):
                raise ValueError('Missing reviewed scalar removal plan')
    for field in fields:
        at, _, _, _, _, _, _, shift, width, _, _ = field
        if at + 4 > prefix or any(at < pos + len(materials[m]) and pos < at + 4 for pos, m in ranges):
            raise ValueError('Native field overlaps data/signature')
        mask = ((1 << width) - 1) << shift
        struct.pack_into('<I', result, at, struct.unpack_from('<I', result, at)[0] & ~mask)
    # Adjacent array and scalar removals can create transient byte matches.
    # Check the complete output: this also catches new matches introduced by
    # scalar clearing, which checking only the intermediate array pass misses.
    for _, value in probes:
        if value in result:
            raise ValueError('Original material survived stripping')
    path.write_bytes(result)
    return prefix, sha(original[:prefix]), ranges, fields


def write_map(path, image_path, recipe, stripped):
    prefix, restored_hash, ranges, fields = stripped
    image = Path(image_path).read_bytes()
    if layout(image)[0] != prefix:
        raise ValueError('Signing changed the native image layout')
    data = (b'FF6RST03' + struct.pack('<II', 3, len(ranges)) + recipe + sha(image) +
            restored_hash + struct.pack('<II', len(image), prefix) +
            struct.pack('<I', len(fields)) + b''.join(struct.pack('<II', *row) for row in ranges) +
            b''.join(struct.pack('<IIIIIiIIIII', *row) for row in fields))
    Path(path).write_bytes(data)
    # The artifact scan authenticates against these exact current-build bytes;
    # no previously reviewed filename/hash can authorize a changed map.
    return data
