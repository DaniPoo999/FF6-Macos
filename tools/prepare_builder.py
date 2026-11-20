#!/usr/bin/env python3
"""Developer-only packaging: the resulting builder invokes no Python or build tools."""
from pathlib import Path
import json
import plistlib
import shutil
import subprocess
import sys
from native_template import (check_artifact_probe_matches, check_extraction_group_matches,
                             check_restoration_format_matches,
                             load_artifact_probe_reviews,
                             material_probes, normalize_link_identity, read_materials, strip_image, write_map)
from native_fields import symbol_offsets
from dependency_notices import checked_notices, NOTICE_NAME

ROOT = Path(__file__).resolve().parents[1]
link_identity = json.loads((ROOT / 'config/native_link_identity.json').read_text())
if (set(link_identity) != {'schema', 'scope', 'images'} or link_identity['schema'] != 1 or
        set(link_identity['images']) != {'ff6-game', 'ff6-inspector'}):
    raise ValueError('Invalid frozen native link identities')
builder, inspector, game, sdl_license, crt_library, builder_icon, private_materials = map(Path, sys.argv[1:])
# SDL's own license does not cover every bundled third-party component. Keep
# their complete notices with the distributed templates, outside the frozen apps.
supplemental_notices = checked_notices(sdl_license.parent)
recipe, materials, representations, specs, profiles = read_materials(ROOT / 'config/rom_materials.json', private_materials)
probes = material_probes(materials, representations, specs)
probe_reviews = load_artifact_probe_reviews(ROOT / 'config/artifact_probe_reviews.json', ROOT, specs)
resources = builder / 'Contents/Resources'
resources.mkdir(parents=True, exist_ok=True)
(resources / NOTICE_NAME).write_bytes(supplemental_notices)
shutil.copy2(builder_icon, resources / 'BuilderIcon.icns')
templates = [
    ('InspectorTemplate.app', inspector, 'ff6-inspector', 'org.ff6native.inspector', 'FFVI Content Inspector'),
    ('GameTemplate.app', game, 'ff6-game', 'org.ff6native.game', 'Final Fantasy VI'),
]
license_destinations = [resources]
for name, executable, binary, identifier, title in templates:
    template = resources / name
    if template.exists():
        shutil.rmtree(template)
    macos = template / 'Contents/MacOS'
    macos.mkdir(parents=True)
    contents = template / 'Contents/Resources'
    contents.mkdir()
    shutil.copy2(executable, macos / binary)
    if binary == 'ff6-game':
        shutil.copy2(crt_library, contents / 'crt.metallib')
    plist = dict(CFBundleExecutable=binary, CFBundleIdentifier=identifier,
                 CFBundleName=title, CFBundleDisplayName=title, CFBundlePackageType='APPL',
                 CFBundleShortVersionString='0.1.0', CFBundleVersion='1',
                 LSMinimumSystemVersion='13.0', NSHighResolutionCapable=True,
                 NSPrincipalClass='NSApplication', SDL_FILESYSTEM_BASE_DIR_TYPE='resource')
    (template / 'Contents/Info.plist').write_bytes(plistlib.dumps(plist, sort_keys=True))
    license_destinations.append(contents)
for dest in license_destinations:
    shutil.copy2(ROOT / 'THIRD_PARTY.md', dest / 'THIRD_PARTY.md')
    shutil.copy2(sdl_license, dest / 'SDL-LICENSE.txt')
    if (ROOT / 'LICENSE').exists():
        shutil.copy2(ROOT / 'LICENSE', dest / 'LICENSE')
generated_maps = {}
for name, _, binary, *_ in templates:
    template = resources / name
    executable = template / 'Contents/MacOS' / binary
    # First establish the complete app's native signature layout, then strip
    # content and re-sign the incomplete template. Restoration maps stay outside
    # the template so they never become part of the generated game bundle.
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(template)], check=True)
    # Canonicalize platform build metadata only after the entire runtime prefix
    # matches the frozen native result. This does not accept changed code/data.
    executable.write_bytes(normalize_link_identity(executable.read_bytes(), link_identity['images'][binary]))
    stripped = strip_image(executable, materials, representations, specs, profiles)
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(template)], check=True)
    map_path = resources / (binary + '.ff6restore')
    generated_maps[map_path.relative_to(builder).as_posix()] = write_map(
        map_path, executable, recipe, stripped)
seen = set()
for path in resources.rglob('*'):
    if path.is_file():
        content = path.read_bytes()
        name = path.relative_to(builder).as_posix()
        matches = []
        for material, value in probes:
            cursor = 0
            while (at := content.find(value, cursor)) >= 0:
                matches.append(dict(material=specs[material]['id'], offset=at, length=len(value)))
                cursor = at + 1
        if name in generated_maps:
            check_restoration_format_matches(content, matches, generated_maps[name])
            if matches:
                print('Reviewed restoration format-word matches:', name, matches)
        else:
            check_artifact_probe_matches(name, content, matches, probe_reviews)
        seen.add(name)
if set(probe_reviews) - seen:
    raise ValueError('Reviewed artifact absent from Builder')
executable = builder / 'Contents/MacOS/FFVI Builder'
content = executable.read_bytes()
matches = []
for material, value in probes:
    cursor = 0
    while (at := content.find(value, cursor)) >= 0:
        matches.append(dict(material=specs[material]['id'], offset=at, length=len(value)))
        cursor = at + 1
check_extraction_group_matches(content, matches, specs, symbol_offsets(executable, content))
if matches:
    print('Reviewed extraction group-count matches:', matches)
subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(builder)], check=True)
subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(builder)], check=True)
print('Packaged and signed ROM-dependent templates; material and dependency checks passed.')
