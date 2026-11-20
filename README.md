# Final Fantasy VI — native macOS port

A native C++20 port of the SNES game for **Apple Silicon and macOS 13 or later**,
using SDL3 for input, audio and display. It runs native game logic rather than a
SNES emulator. This is an unofficial fan project, not affiliated with Square Enix.

**Version 0.5 is a prerelease.** Stability is not established. Bugs, incomplete
behavior and audiovisual differences remain; keep backups of your saves.

## Build your game

1. Download **FFVI-Builder-0.5-macOS-arm64.zip** from the
   [0.5 prerelease](https://github.com/DaniPoo999/FF6-Macos/releases/tag/v0.5).
2. Unzip it and open **FFVI Builder.app**.
3. Select your legally obtained **Final Fantasy III (USA) (Rev 1)** SNES ROM.
4. Choose an empty output folder and click **Build Apps**.
5. Open **Final Fantasy VI.app** to play.

The Builder works offline, with no Xcode, compiler, Python or Homebrew required.
It extracts the content, assembles and locally signs two self-contained apps:

- **Final Fantasy VI.app** — the game.
- **FFVI Content Inspector.app** — debugging and content inspection tools.

It also writes **FFVI Dependency Notices.txt** alongside them. Existing output is
never overwritten. Saves and preferences live separately in Application Support.

The prerelease is ad-hoc signed, **not Developer ID signed or notarized**. macOS
may block a downloaded app. After verifying the download's checksum, use the
per-app **Open Anyway** option in System Settings → Privacy & Security if needed.
Do not disable Gatekeeper globally.

**No ROM is included. Never upload or distribute either generated app, extracted
content or ROM-derived icons through this repository or its releases.** Only the
source and the ROM-free Builder are published. The Builder's icon is procedural;
the game's Terra icon is generated locally from your ROM.

### Supported ROM

The headerless payload must be 3,145,728 bytes, with SHA-256:

```text
10eccc5d2fab81346dd759f6be478dcb682eef981e8d3d662da176e1f9a996bc
```

An optional 512-byte copier header is accepted only when its removal yields that
payload. Other revisions, translations and patched ROMs are rejected.

### Controls

Use an SDL-compatible controller, including a Nintendo Switch Pro Controller,
or the keyboard:

| Action | Key |
| --- | --- |
| Move / select | Arrow keys |
| Confirm | Return |
| Cancel | Backspace |
| Menu / SNES X | X |
| SNES Y | Y |
| L / R | Q / W |
| Start / Select | Right Shift / Left Control |

## Compile the Builder yourself

Developer builds require an Apple Silicon Mac, Git, **CMake 3.25+**, **Python
3.9+**, full **Xcode 27.2 (27B52)** with Apple Clang **21.0.0
(clang-2100.4.1.1)**, and Apple Metal **32023.921 (metalfe-32023.921.3)**.
These pins preserve the expected game executable and shader output. Other
compiler versions may be rejected; do not disable the identity checks.

```sh
git clone https://github.com/DaniPoo999/FF6-Macos.git
cd FF6-Macos
./build.sh --rom "/absolute/path/to/Final Fantasy III (USA) (Rev 1).sfc"
open "build/FFVI Builder.app"
```

The first build downloads hash-pinned **SDL 3.4.16**. The ROM is needed to generate
private compilation inputs; packaging strips these from the executable templates
and includes validated restoration recipes. The downloaded Builder restores them
from each user's ROM. No disassembly repository or private development cache is
required to compile. Generated files remain in ignored `build/` or `.local/`.

<details>
<summary>Installing the pinned Metal component</summary>

Xcode updates can select a different Metal version. Obtain the matching component
using Apple's Xcode tools, then select it for this build:

```sh
mkdir -p .local/toolchains
xcodebuild -downloadComponent metalToolchain -buildVersion 27A5237l \
  -exportPath "$PWD/.local/toolchains"
diskutil image attach --readOnly --nobrowse \
  --mountPoint "$PWD/.local/toolchains/metal-27A5237l" \
  "$PWD/.local/toolchains/MetalToolchain-27A5237l.exportedBundle/Restore/022-22115-084.dmg"
export FF6_METAL_COMPILER="$PWD/.local/toolchains/metal-27A5237l/Metal.xctoolchain/usr/bin/metal"
./build.sh --rom "/absolute/path/to/Final Fantasy III (USA) (Rev 1).sfc"
```

Keep the read-only mount available until the build finishes, then run
`diskutil eject "$PWD/.local/toolchains/metal-27A5237l"` and unset
`FF6_METAL_COMPILER`. Set `DEVELOPER_DIR` to your full Xcode's `Contents/Developer`
directory if necessary. Apple toolchains are not included in this repository.

</details>

### Verify and package a Builder release

```sh
./build.sh --release --rom "/absolute/path/to/Final Fantasy III (USA) (Rev 1).sfc"
```

This checks the source boundary, packages only the Builder, unpacks the archive,
runs it with networking denied and an empty command PATH, checks signatures and
compares every generated app file against the expected hashes. A bounded content
scan also checks the Builder. Temporary generated apps are deleted afterward.
The archive and `SHA256SUMS` appear in `build/releases/`; verification logs stay
private in `.local/release-verification/`.

For brief launch checks of both generated apps, run:

```sh
python3 tools/release_builder.py --builder "build/FFVI Builder.app" \
  --rom "/absolute/path/to/Final Fantasy III (USA) (Rev 1).sfc" --smoke
```

These verify packaging and startup, not complete gameplay or clean-machine
compatibility. Before committing, stage the intended source and run
`python3 tools/check_repository.py --staged`. Never force-add ignored content.

## Source and licenses

Original graphics, text, names, scripts, music, samples and gameplay data are
derived from the supplied ROM. The repository contains native implementations,
extraction algorithms and functional metadata such as offsets and hashes.

Keep [LICENSE](LICENSE), [THIRD_PARTY.md](THIRD_PARTY.md) and
[DEPENDENCY_NOTICES.txt](DEPENDENCY_NOTICES.txt). Source is provided under
GPL-3.0; dependencies retain their own licenses. These notices grant no rights
to original game material. The historical attribution file mentions private
research documents and comparison harnesses that are not part of this source
distribution. Reconstructed routines' independent authorship has not been
established comprehensively; the content checks are not legal clearance.

The FFVI disassembly reference is
[everything8215/ff6](https://github.com/everything8215/ff6) at
`813013276c952fdd27edcf7b8b86f17291542cbf`. The Gaussian-kernel adaptation retains
the [Ares](https://github.com/ares-emulator/ares) ISC notice (reference revision
`9408cb43d4948fc3ea6e152a307a34348df3fe04`). Mesen and other emulators were research
references, not bundled runtime dependencies.
