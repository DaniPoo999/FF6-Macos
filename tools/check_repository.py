#!/usr/bin/env python3
"""Check source release inputs and, optionally, the exact staged Git snapshot."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = {'.gitignore', 'CMakeLists.txt', 'DEPENDENCY_NOTICES.txt', 'LICENSE',
              'README.md', 'THIRD_PARTY.md', 'build.sh'}
CONFIG_FILES = {'BuilderInfo.plist.in', 'artifact_probe_reviews.json',
                'dependency_notices.json', 'game_output_identity.json',
                'metal_identity.json', 'native_fields.json', 'native_link_identity.json',
                'rom_inventory.json', 'rom_materials.json'}
TOOL_FILES = {'audit_builder_payload.py', 'build_metal.py', 'builder_icon_main.cpp',
              'check_repository.py', 'dependency_notices.py', 'generate_inventory.py',
              'generate_material_metadata.py', 'native_fields.py', 'native_template.py',
              'prepare_builder.py', 'release_builder.py', 'rom_input_source.hpp',
              'rom_inputs_main.cpp'}


def inputs():
    """Explicit source boundary, also usable in a downloaded source archive.

    Local build/evidence directories are never publication inputs. This is a
    hygiene check, not a substitute for reviewing embedded material or licenses.
    """
    names = set()
    for path in ROOT.iterdir():
        if path.name in {'.git', '.local', 'build', '.DS_Store'}:
            continue
        if path.name in {'src', 'include', 'config', 'tools'} and path.is_dir() and not path.is_symlink():
            for child in path.rglob('*'):
                if '__pycache__' in child.parts or child.name == '.DS_Store':
                    continue
                if child.is_symlink():
                    raise ValueError('Source symlink: ' + str(child.relative_to(ROOT)))
                if child.is_file():
                    names.add(str(child.relative_to(ROOT)))
        else:
            names.add(path.name)
    required = ROOT_FILES | {'config/' + n for n in CONFIG_FILES} | {'tools/' + n for n in TOOL_FILES}
    missing = required - names
    if missing:
        raise ValueError('Missing source inputs: ' + ', '.join(sorted(missing)))
    for name in names:
        p = Path(name)
        allowed = (name in ROOT_FILES or name == 'src/gaussian_coefficients.inc' or
                   (len(p.parts) == 2 and p.parts[0] == 'config' and p.name in CONFIG_FILES) or
                   (len(p.parts) == 2 and p.parts[0] == 'tools' and p.name in TOOL_FILES) or
                   (p.parts[0] in {'src', 'include'} and p.suffix in {'.cpp', '.hpp', '.h', '.mm', '.metal'}))
        if not allowed:
            raise ValueError('Unexpected publication input: ' + name)
    return sorted(names)

CREDENTIALS = {
    'private_key': re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----'),
    'github_token': re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})\b'),
    'aws_access_key': re.compile(rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b'),
    'api_token': re.compile(rb'\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{40,}\b'),
}


def credential_findings(files):
    # Report only categories and paths; never print a matched credential.
    return [name + ': ' + kind for name, data in files.items()
            for kind, expression in CREDENTIALS.items() if expression.search(data)]


def inspect_index(root, expected):
    """Reject content/mode differences and anything outside the reviewed set."""
    records = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=root)
    seen, errors = set(), []
    for record in records.split(b'\0'):
        if not record:
            continue
        header, raw_path = record.split(b'\t', 1)
        mode, object_id, stage = header.decode('ascii').split()
        name = raw_path.decode('utf-8')
        if name in seen or stage != '0':
            errors.append(name + ': unresolved index entry')
        seen.add(name)
        if mode not in ('100644', '100755') or name not in expected:
            errors.append(name + ': unexpected file or Git mode')
            continue
        data = subprocess.check_output(['git', 'cat-file', 'blob', object_id], cwd=root)
        wanted = expected[name]
        if hashlib.sha256(data).hexdigest() != wanted['sha256'] or mode != wanted['mode']:
            errors.append(name + ': staged bytes/mode differ from reviewed worktree')
    errors.extend(name + ': reviewed source is not staged' for name in sorted(set(expected) - seen))
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true',
                        help='Require the index to equal the reviewed source snapshot exactly')
    args = parser.parse_args()
    errors = []
    content, expected = {}, {}
    for name in inputs():
        path = ROOT / name
        if not path.is_file() or path.is_symlink():
            errors.append(name + ': non-regular source input')
            continue
        data = path.read_bytes()
        try:
            data.decode('utf-8')
        except UnicodeDecodeError:
            errors.append(name + ': binary data is not a source input')
        if b'\x00' in data:
            errors.append(name + ': embedded NUL bytes')
        content[name] = data
        expected[name] = {'sha256': hashlib.sha256(data).hexdigest(),
                          'mode': '100755' if path.stat().st_mode & 0o111 else '100644'}
    errors.extend(credential_findings(content))
    if args.staged:
        errors.extend(inspect_index(ROOT, expected))
    print(json.dumps({'source_files': len(expected), 'staged_checked': args.staged,
                      'errors': errors, 'legal_clearance': False}))
    if errors:
        raise SystemExit('Repository source checks failed')


if __name__ == '__main__':
    main()
