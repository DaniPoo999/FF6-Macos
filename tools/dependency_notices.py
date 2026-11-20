#!/usr/bin/env python3
"""Check the supplemental notices against the pinned, unmodified SDL sources.

Only license text is retained here. This never changes a dependency, game
template or frozen application; the Builder publishes the text beside the apps.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NOTICE_NAME = 'FFVI Dependency Notices.txt'


def normalized(text):
    return '\n'.join(line.rstrip() for line in text.strip().splitlines()) + '\n'


def checked_notices(sdl_source, root=ROOT):
    manifest = json.loads((root / 'config/dependency_notices.json').read_text())
    data = (root / 'DEPENDENCY_NOTICES.txt').read_bytes()
    if manifest['schema'] != 1 or hashlib.sha256(data).hexdigest() != manifest['notice_sha256']:
        raise ValueError('Supplemental dependency notice identity changed; review required')
    text = data.decode('utf-8')
    source = sdl_source.resolve()
    for entry in manifest['sources']:
        path = (source / entry['path']).resolve()
        if not path.is_relative_to(source):
            raise ValueError('Dependency notice source escapes SDL directory')
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != entry['sha256']:
            raise ValueError('Dependency notice source changed: ' + entry['path'])
        for section in entry['sections']:
            start, end = section['start'], section['end']
            if not 0 <= start < end <= len(raw):
                raise ValueError('Invalid dependency notice span: ' + entry['path'])
            fragment = normalized(raw[start:end].decode('utf-8'))
            if hashlib.sha256(fragment.encode()).hexdigest() != section['sha256'] or fragment not in text:
                raise ValueError('Missing or changed dependency notice: ' + entry['path'])
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdl-source', type=Path, required=True)
    args = parser.parse_args()
    data = checked_notices(args.sdl_source)
    print(f'Supplemental dependency notices verified ({len(data)} bytes)')


if __name__ == '__main__':
    main()
