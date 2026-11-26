#!/usr/bin/env python3
"""Verify and archive only the ROM-free Builder; generated games stay private."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import stat
import subprocess
import tempfile
import zipfile
from audit_builder_payload import inspect, private_pack
from check_repository import ROOT

VERSION = '0.5.2'
BUILD = '7'
NOTICES = 'FFVI Dependency Notices.txt'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def files(folder):
    result = {}
    for path in folder.rglob('*'):
        if path.is_symlink():
            raise ValueError('Unexpected symlink: ' + str(path))
        if path.is_file():
            result[str(path.relative_to(folder))] = path
    return result

def builder_files():
    """No generated game, icon, content pack or private input is distributable."""
    names = {'Contents/Info.plist', 'Contents/MacOS/FFVI Builder',
             'Contents/_CodeSignature/CodeResources'}
    resources = {'BuilderIcon.icns', NOTICES, 'LICENSE', 'THIRD_PARTY.md',
                 'SDL-LICENSE.txt', 'ff6-game.ff6restore', 'ff6-inspector.ff6restore'}
    names.update('Contents/Resources/' + name for name in resources)
    for app, exe in [('GameTemplate.app', 'ff6-game'), ('InspectorTemplate.app', 'ff6-inspector')]:
        base = 'Contents/Resources/' + app + '/Contents/'
        names.update(base + name for name in ['Info.plist', 'MacOS/' + exe,
                     '_CodeSignature/CodeResources', 'Resources/LICENSE',
                     'Resources/THIRD_PARTY.md', 'Resources/SDL-LICENSE.txt'])
        if exe == 'ff6-game':
            names.add(base + 'Resources/crt.metallib')
    return names

def signature(app):
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(app)], check=True)

def check_builder(builder):
    actual = files(builder)
    if set(actual) != builder_files():
        raise ValueError('Unexpected or missing Builder files: ' + str(sorted(set(actual) ^ builder_files())))
    info = plistlib.loads(actual['Contents/Info.plist'].read_bytes())
    if info['CFBundleShortVersionString'] != VERSION or info['CFBundleVersion'] != BUILD:
        raise ValueError('Builder version mismatch')
    review = json.loads((ROOT / 'config/artifact_probe_reviews.json').read_text())
    icon = 'Contents/Resources/BuilderIcon.icns'
    if sha(actual[icon]) != review['artifacts'][icon]['sha256']:
        raise ValueError('Release requires the original procedural Builder icon')
    if actual['Contents/Resources/' + NOTICES].read_bytes() != (ROOT / 'DEPENDENCY_NOTICES.txt').read_bytes():
        raise ValueError('Dependency notices mismatch')
    signature(builder)
    return actual

def check_generated(output):
    expected = json.loads((ROOT / 'config/game_output_identity.json').read_text())['bundles']
    if {p.name for p in output.iterdir()} != set(expected) | {NOTICES}:
        raise ValueError('Unexpected generated outputs')
    result = {}
    for name, hashes in expected.items():
        actual = {n: sha(p) for n, p in files(output / name).items()}
        if actual != hashes:
            raise ValueError('Generated app differs: ' + name + ': ' + str(sorted(
                n for n in actual.keys() | hashes.keys() if actual.get(n) != hashes.get(n))))
        signature(output / name)
        result[name] = {'files': len(actual), 'byte_identical': True, 'signature_valid': True}
    if (output / NOTICES).read_bytes() != (ROOT / 'DEPENDENCY_NOTICES.txt').read_bytes():
        raise ValueError('Generated dependency notices mismatch')
    return result

def archive_builder(builder, archive):
    actual = check_builder(builder)
    # Explicit allowlist above; never zip a build directory or generated output.
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as target:
        for name, path in sorted(actual.items()):
            info = zipfile.ZipInfo('FFVI Builder.app/' + name, (2026, 11, 20, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | (0o755 if os.access(path, os.X_OK) else 0o644)) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            target.writestr(info, path.read_bytes())

def verify_archive(archive, directory):
    with zipfile.ZipFile(archive) as source:
        expected = {'FFVI Builder.app/' + n for n in builder_files()}
        if set(source.namelist()) != expected or len(source.namelist()) != len(expected):
            raise ValueError('Unexpected release archive entries')
        for entry in source.infolist():
            mode = entry.external_attr >> 16
            if not stat.S_ISREG(mode):
                raise ValueError('Non-regular archive entry')
            path = directory / entry.filename
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(source.read(entry))
            path.chmod(mode & 0o777)
    app = directory / 'FFVI Builder.app'
    check_builder(app)
    return app

def run_logged(command, log, **kwargs):
    with log.open('w') as stream:
        subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                       check=True, timeout=300, **kwargs)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--builder', type=Path, required=True)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--smoke', action='store_true', help='Also open both generated apps for brief launch checks')
    args = parser.parse_args()
    subprocess.run([os.sys.executable, str(ROOT / 'tools/check_repository.py')], check=True)
    release = ROOT / 'build/releases'
    release.mkdir(parents=True, exist_ok=True)
    archive = release / ('FFVI-Builder-' + VERSION + '-macOS-arm64.zip')
    private = ROOT / '.local/release-verification'
    private.mkdir(parents=True, exist_ok=True)
    # Failed verification leaves the previous release intact.
    with tempfile.TemporaryDirectory(prefix='builder-', dir=private) as temp:
        work = Path(temp)
        pending = work / archive.name
        archive_builder(args.builder.resolve(), pending)
        builder = verify_archive(pending, work / 'Unpacked å')
        output = work / 'Generated apps å'
        output.mkdir()
        profile = work / 'offline.sb'
        profile.write_text('(version 1)\n(allow default)\n(deny network*)\n')
        run_logged(['/usr/bin/sandbox-exec', '-f', str(profile),
                    str(builder / 'Contents/MacOS/FFVI Builder'), '--build',
                    str(args.rom.resolve()), str(output)], private / 'builder.log',
                   env=dict(os.environ, PATH=''))
        generated = check_generated(output)
        content = private_pack(output / 'Final Fantasy VI.app/Contents/Resources/content.ff6pack')
        payload = inspect(content, builder)
        if payload['matches']:
            raise ValueError('Possible game material found in Builder: ' + str(payload['matches']))
        if args.smoke:
            run_logged([str(output / 'Final Fantasy VI.app/Contents/MacOS/ff6-game'),
                        '--smoke-test', '--save-folder', str(work / 'saves'),
                        '--preferences-folder', str(work / 'preferences')],
                       private / 'game-smoke.log')
            run_logged([str(output / 'FFVI Content Inspector.app/Contents/MacOS/ff6-inspector'),
                        '--smoke-test'], private / 'inspector-smoke.log')
        report = {'version': VERSION, 'bundles': generated, 'offline_empty_path': True,
                  'archive_files': len(builder_files()), 'payload_scan': payload,
                  'smoke_tests': args.smoke, 'archive_sha256': sha(pending)}
        (private / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
        pending.replace(archive)
    (release / 'SHA256SUMS').write_text(sha(archive) + '  ' + archive.name + '\n')
    print('Verified Builder release: ' + str(archive))
    print('Both generated apps match all expected files. Generated content was removed after checking.')

if __name__ == '__main__':
    main()
