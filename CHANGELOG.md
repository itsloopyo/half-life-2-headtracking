# Changelog

## [Unreleased]

### Changed

- The vendored Ultimate ASI Loader no longer carries the three third-party DLLs
  the upstream 32-bit build embeds as resources. `binkw32.dll` (RAD Game Tools'
  Bink and Smacker 1.994i, proprietary middleware licensed per title),
  `wndmode.dll` (DirectX Windower Embedded, (C) 2008 VEG and (C) 2004 menopem,
  no licence) and `vorbisfile.dll` (Xiph.Org, BSD-3-Clause) ride along so that a
  user who renames the loader over one of those libraries still gets the
  original exports, and the installer ZIP ships that binary, so it was
  redistributing all three. `scripts/strip-loader-payload.ps1` now zeroes them,
  `pixi run update-deps` runs it on every refresh, and `pixi run package`
  refuses to build a ZIP from a loader that still has them. Only the `.rsrc`
  section changes: the loader's code, imports, relocations and appended PDB are
  byte-identical to upstream, and nothing in this mod could reach the stripped
  resources anyway.
- `THIRD-PARTY-NOTICES.md` recorded cameraunlock-core at a commit the submodule
  no longer points at; it is restamped to the commit the mod compiles.

## [0.1.0] - 2026-08-31

First release.

