#!/usr/bin/env python3
"""Look for packaged content copies using a private, canonical generated pack.

This is an additional detection layer, not a provenance certificate. It reads
shared-extractor output instead of implementing a second ROM extractor. Reports
contain resource identities and locations only, never matched content bytes.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def private_pack(path):
    raw_inventory = (ROOT / 'config/rom_inventory.json').read_bytes()
    inventory = json.loads(raw_inventory)
    specs = list(inventory['resources'])
    for source in inventory.get('decoded_sources', []):
        specs.extend(source['resources'])
    specs.extend(inventory.get('derived_resources', []))
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError('Content pack exceeds limit')
    data = path.read_bytes()
    if len(data) > 16 * 1024 * 1024:
        raise ValueError('Content pack grew past limit')
    position = 0

    def take(size):
        nonlocal position
        if size < 0 or size > len(data) - position:
            raise ValueError('Truncated pack')
        out = data[position:position + size]
        position += size
        return out

    def integer(size):
        return int.from_bytes(take(size), 'little')

    if take(8) != b'FF6PACK1' or integer(4) != 1:
        raise ValueError('Invalid content pack header')
    if (take(32).hex() != inventory['rom_sha256'] or
            take(32) != hashlib.sha256(raw_inventory).digest() or integer(4) != len(specs)):
        raise ValueError('Pack does not match canonical extraction metadata')
    resources = []
    for spec in specs:
        id_size, size = integer(2), integer(4)
        expected = take(32).hex()
        identity = take(id_size).decode('ascii')
        if identity != spec['id'] or size != spec['size'] or expected != spec['sha256']:
            raise ValueError('Unexpected content resource')
        payload = take(size)
        if hashlib.sha256(payload).hexdigest() != expected:
            raise ValueError('Corrupt private comparison input')
        resources.append((identity, payload))
    if position != len(data):
        raise ValueError('Trailing pack bytes')
    return resources


def probes(payload):
    # Very short/repetitive spans match ordinary constants and zero-fill. Leave
    # them explicitly untested instead of granting a misleading clean result.
    if len(payload) < 32 or len(set(payload)) < 8:
        return []
    positions = {0, max(0, (len(payload) - 64) // 2), max(0, len(payload) - 64)}
    return [payload[at:at + 64] for at in sorted(positions)
            if len(set(payload[at:at + 64])) >= 8]


def inspect(resources, bundle):
    needles, skipped = [], []
    for identity, payload in resources:
        samples = probes(payload)
        if not samples:
            skipped.append(identity)
        needles.extend((identity, value) for value in samples)
    matches, files = [], 0
    for path in sorted(bundle.rglob('*')):
        if path.is_symlink():
            raise ValueError('Unexpected symlink in Builder')
        if not path.is_file():
            continue
        if path.stat().st_size > 32 * 1024 * 1024:
            raise ValueError('Builder resource exceeds audit limit')
        data = path.read_bytes()
        files += 1
        if b'FF6PACK1' in data and path.suffix == '.ff6pack':
            raise ValueError('Content pack packaged inside Builder')
        for identity, value in needles:
            at = data.find(value)
            if at >= 0:
                matches.append({'resource': identity, 'file': str(path.relative_to(bundle)),
                                'offset': at, 'matched_bytes': len(value)})
    return {'schema': 1, 'files': files, 'resources': len(resources), 'probes': len(needles),
            'skipped_small_or_repetitive': skipped, 'matches': matches,
            'limitation': 'Bounded raw-byte probes do not cover every partial, transformed, '
                          'compressed or compiler-folded representation; source review remains required.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', type=Path, required=True)
    parser.add_argument('--builder', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    output = args.report.resolve()
    if not any(output.is_relative_to(ROOT / name) for name in ('.local', 'build')):
        raise ValueError('Keep reports in ignored storage')
    if not args.builder.is_dir():
        raise ValueError('Missing Builder bundle')
    report = inspect(private_pack(args.pack), args.builder)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: report[k] for k in ('files', 'resources', 'probes', 'matches')}))
    if report['matches']:
        raise SystemExit('Possible original content found in Builder; inspect private report')


if __name__ == '__main__':
    main()
