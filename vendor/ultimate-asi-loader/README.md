# Ultimate ASI Loader (vendored)

Bundled copy of Ultimate ASI Loader (x86), the install-time source of truth.
install.cmd copies it straight out of here and never reaches out to the network.
Refresh manually with `pixi run update-deps`, then commit.

## Snapshot

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Tag: `v9.7.4`
- Commit: `6b440669144c4a0bef5718ab155df160d231cd42`
- Asset: `Ultimate-ASI-Loader.zip`
- Asset URL: https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader.zip
- dinput8.dll SHA-256: `d5a059aa467a7a7127c8f6169f79fa63ff0f55986ee9eb2fd9a281bebf2aa2e6`
- Fetched at: 2026-08-31T13:17:36.0082340+01:00

`dinput8.dll` is extracted from the upstream x86 zip untouched. install.cmd copies it
to <game>\bin\winmm.dll, the proxy slot Half-Life 2 loads ASI plugins through.
