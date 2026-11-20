#!/usr/bin/env python3
"""Compile reviewed metadata into the extractor; never reads a ROM or reference repo."""
import hashlib
import json
from pathlib import Path
import re
import sys


def validate(doc):
    if doc['schema'] != 1 or doc['rom_size'] != 3145728:
        raise ValueError('Unsupported inventory schema/profile')
    ids = set()
    end = 0
    for row in doc['resources']:
        if not re.fullmatch(r'[a-z0-9_]{1,160}', row['id']) or row['id'] in ids:
            raise ValueError('Invalid or duplicate resource ID')
        ids.add(row['id'])
        if row['offset'] < end or row['size'] <= 0:
            raise ValueError('Overlapping, unordered or empty resource')
        end = row['offset'] + row['size']
        if end > doc['rom_size'] or not re.fullmatch(r'[0-9a-f]{64}', row['sha256']):
            raise ValueError('Invalid resource range/hash')
        for excluded in doc['excluded']:
            if max(row['offset'], excluded['offset']) < min(end, excluded['offset'] + excluded['size']):
                raise ValueError('Resource intersects excluded code/header/padding: ' + row['id'])
    spans = sorted((x['offset'], x['offset'] + x['size']) for x in doc['resources'] + doc['excluded'])
    cursor = 0
    for start, stop in spans:
        if start != cursor or stop <= start:
            raise ValueError('Classification must partition ROM storage exactly')
        cursor = stop
    if cursor != doc['rom_size']:
        raise ValueError('Classification does not cover ROM')
    containers = set()
    for source in doc.get('decoded_sources', []):
        if not re.fullmatch(r'[a-z0-9_]{1,160}', source['id']) or source['id'] in containers:
            raise ValueError('Invalid/duplicate decoded source')
        containers.add(source['id'])
        if not any(row['offset'] == source['offset'] and row['size'] == source['size'] and
                   row['classification'] == 'compressed_code_and_data' for row in doc['excluded']):
            raise ValueError('Decoded source must match an excluded mixed container')
        if source['codec'] != 'lzss' or not 0 < source['decoded_size'] <= 65536:
            raise ValueError('Unsupported decoded source')
        for key in ('sha256', 'decoded_sha256'):
            if not re.fullmatch(r'[0-9a-f]{64}', source[key]):
                raise ValueError('Invalid decoded-source hash')
        end = 0
        for row in source['resources']:
            if not re.fullmatch(r'[a-z0-9_]{1,160}', row['id']) or row['id'] in ids:
                raise ValueError('Invalid/duplicate decoded resource ID')
            ids.add(row['id'])
            if row['offset'] < end or row['size'] <= 0 or not row.get('reference_owner'):
                raise ValueError('Unreviewed/overlapping decoded resource')
            end = row['offset'] + row['size']
            if end > source['decoded_size'] or not re.fullmatch(r'[0-9a-f]{64}', row['sha256']):
                raise ValueError('Invalid decoded resource bounds/hash')
        cursor = 0
        for row in sorted(source['resources'] + source['excluded'], key=lambda row: row['offset']):
            if row['offset'] != cursor or row['size'] <= 0:
                raise ValueError('Decoded classification must partition the container')
            cursor += row['size']
        if cursor != source['decoded_size']:
            raise ValueError('Incomplete decoded classification')

    for row in doc.get('derived_resources', []):
        if (row['id'] in ids or not re.fullmatch(r'[a-z0-9_]{1,160}', row['id']) or
                row['transform'] != 'airship_smoke_v1' or row['size'] != 32768 * 6 or
                not row.get('reference_owner') or not re.fullmatch(r'[0-9a-f]{64}', row['sha256'])):
            raise ValueError('Invalid semantic resource transform')
        ids.add(row['id'])


def main():
    source = Path(sys.argv[1])
    raw = source.read_bytes()
    doc = json.loads(raw)
    validate(doc)
    if sys.argv[2] == '--check':
        derived = sum(len(source['resources']) for source in doc.get('decoded_sources', []))
        print(f"Inventory: {len(doc['resources'])} ROM spans, {derived} decoded data spans, "
              f"{len(doc.get('derived_resources', []))} semantic transforms; excluded ranges remain explicit")
        return
    lines = ['// Generated metadata only. No original game content.',
             f'constexpr std::string_view inventory_hash = "{hashlib.sha256(raw).hexdigest()}";',
             'constexpr ResourceSpec resource_specs[] = {']
    for row in doc['resources']:
        lines.append('    {' + ', '.join((json.dumps(row['id']), json.dumps(row['family']),
                     str(row['offset']) + 'U', str(row['size']) + 'U', json.dumps(row['sha256']))) + '},')
    for source in doc.get('decoded_sources', []):
        for row in source['resources']:
            lines.append('    {' + ', '.join((json.dumps(row['id']), json.dumps(row['family']),
                         str(row['offset'])+'U', str(row['size'])+'U', json.dumps(row['sha256']),
                         json.dumps(source['id']))) + '},')
    for row in doc.get('derived_resources', []):
        lines.append('    {' + ', '.join((json.dumps(row['id']), json.dumps(row['family']),
                     '0U', str(row['size'])+'U', json.dumps(row['sha256']), '""',
                     json.dumps(row['transform']))) + '},')
    lines.append('};\n')
    sources = doc.get('decoded_sources', [])
    lines.append(f'constexpr std::array<DecodedSourceSpec, {len(sources)}> decoded_sources = {{{{')
    for source in sources:
        lines.append('    {' + ', '.join((json.dumps(source['id']), str(source['offset'])+'U',
                     str(source['size'])+'U', str(source['decoded_size'])+'U', json.dumps(source['sha256']),
                     json.dumps(source['decoded_sha256']))) + '},')
    lines.append('}};\n')
    out = Path(sys.argv[2]); out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('\n'.join(lines))


if __name__ == '__main__':
    main()
