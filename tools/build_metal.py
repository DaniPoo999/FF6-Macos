#!/usr/bin/env python3
"""Build the native CRT shader with the frozen compiler/SDK identity.

Only toolchain versions and hashes are pinned. No precompiled shader or game
material is used as a fallback; a different output fails before packaging.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def validate_identity(profile):
    if (set(profile) != {'schema', 'compiler', 'sdk', 'source_sha256', 'output_sha256'} or
            profile['schema'] != 1 or
            not isinstance(profile['compiler'], str) or
            not re.fullmatch(r'Apple metal version [0-9.]+ \(metalfe-[0-9.]+\)', profile['compiler']) or
            not isinstance(profile['sdk'], str) or not re.fullmatch(r'[0-9]+\.[0-9]+', profile['sdk']) or
            any(not isinstance(profile[k], str) or not re.fullmatch(r'[0-9a-f]{64}', profile[k])
                for k in ('source_sha256', 'output_sha256'))):
        raise ValueError('Invalid frozen Metal build identity')
    return profile


def require_hash(data, expected, description):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f'{description} differs from the frozen build identity')


def find_compiler(profile, developer_dir):
    override = os.environ.get('FF6_METAL_COMPILER')
    candidates = [Path(override)] if override else []
    if not override:
        env = dict(os.environ, DEVELOPER_DIR=developer_dir)
        found = subprocess.run(['xcrun', '--find', 'metal'], env=env, capture_output=True,
                               text=True, timeout=15)
        if found.returncode == 0:
            candidates.append(Path(found.stdout.strip()))
        # Apple's downloaded toolchains can outlive an Xcode update. Their
        # mount suffix is transient, so select by compiler identity, not path.
        candidates.extend(sorted(Path('/var/run/com.apple.security.cryptexd/mnt').glob(
            '*MetalToolchain*/Metal.xctoolchain/usr/bin/metal')))
        candidates.extend(sorted((Path.home() / 'Library/Developer/DVTDownloads/MetalToolchain/mounts').glob(
            '*/Metal.xctoolchain/usr/bin/metal')))
    for compiler in dict.fromkeys(candidates):
        if not compiler.is_file() or not compiler.with_name('metallib').is_file():
            continue
        version = subprocess.run([str(compiler), '--version'], capture_output=True,
                                 text=True, timeout=15)
        if version.returncode == 0 and version.stdout.splitlines()[:1] == [profile['compiler']]:
            return compiler
    raise RuntimeError('The frozen Metal compiler is unavailable: ' + profile['compiler'] +
                       '. See README.md for the pinned Apple component and set '
                       'FF6_METAL_COMPILER to its metal executable.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity', required=True, type=Path)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--developer-dir', required=True)
    parser.add_argument('--check', action='store_true',
                        help='Check compiler availability without creating output')
    args = parser.parse_args()
    if not args.check and (args.source is None or args.output is None):
        parser.error('--source and --output are required when compiling')
    profile = validate_identity(json.loads(args.identity.read_text()))
    compiler = find_compiler(profile, args.developer_dir)
    if args.check:
        print('Verified frozen Metal compiler: ' + str(compiler))
        return
    require_hash(args.source.read_bytes(), profile['source_sha256'], 'CRT source')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='metal-build-', dir=args.output.parent) as temporary:
        intermediate = Path(temporary) / 'crt.air'
        result = Path(temporary) / 'crt.metallib'
        # The shader uses Metal's own standard library. Explicit SDK metadata
        # preserves its build identity without depending on the active macOS SDK.
        subprocess.run([str(compiler), '-std=macos-metal2.4', '-mmacosx-version-min=13.0',
                        '-Xclang', '-target-sdk-version=' + profile['sdk'], '-c',
                        str(args.source), '-o', str(intermediate)], check=True)
        subprocess.run([str(compiler.with_name('metallib')), str(intermediate), '-o', str(result)],
                       check=True)
        require_hash(result.read_bytes(), profile['output_sha256'], 'Compiled CRT library')
        os.replace(result, args.output)
    print('Verified frozen Metal output with ' + profile['compiler'])


if __name__ == '__main__':
    main()
