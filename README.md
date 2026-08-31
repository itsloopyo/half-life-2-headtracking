# Half-Life 2 Head Tracking

![Half-Life 2 running with this mod](https://raw.githubusercontent.com/itsloopyo/half-life-2-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Half-Life 2 that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - your head moves the camera the frame is rendered from, while the mouse or controller still controls where you shoot. The player's own eye angles and position are never written, so weapon fire, traces, physics and NPC behavior are unchanged.
- **6DOF tracking** - yaw, pitch and roll, plus positional lean, peek and duck.
- **Any OpenTrack compatible source** - webcam, phone app, TrackIR, or anything else that sends the OpenTrack UDP protocol.

## Requirements

- [Half-Life 2](https://store.steampowered.com/app/220/HalfLife_2/) on Steam (app 220), on the build whose `hl2\bin\client.dll` is dated 27 June 2025. On any other build the mod stays dormant and the game runs vanilla.
- A tracking source that sends the OpenTrack UDP protocol, such as [OpenTrack](https://github.com/opentrack/opentrack/releases) with a webcam.
- 64-bit Windows 10 or 11. The game itself is a 32-bit process, so the mod and its loader are both x86.

## Installation

1. Download the installer ZIP (`HalfLife2HeadTracking-v<version>-installer.zip`) from the [Releases page](https://github.com/itsloopyo/half-life-2-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`. It finds Half-Life 2, places the loader and the mod in `<game>\bin\`, and reports what it did.
4. Configure OpenTrack (or your tracker app) to send UDP to `127.0.0.1:4242`.
5. Launch the game.

If the installer cannot find your copy of the game, point it at the folder
yourself. Either pass the path as an argument:

```powershell
install.cmd "D:\Games\steamapps\common\Half-Life 2"
```

or set the override environment variable before running it:

```powershell
$env:HALF_LIFE_2_PATH = "D:\Games\steamapps\common\Half-Life 2"
.\install.cmd
```

Both expect the folder that contains `hl2.exe`.

### Manual Installation

For placing the files by hand, or when using the Nexus ZIP
(`HalfLife2HeadTracking-v<version>-nexus.zip`), which contains the deploy
subtree only and no loader.

1. Download [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) and take `dinput8.dll` out of `Ultimate-ASI-Loader.zip`. That asset is the x86 build; the x64 one cannot load into Half-Life 2.
2. Rename it to `winmm.dll` and put it in `<game>\Half-Life 2\bin\`. Source loads its DLLs out of `bin\`, so a copy next to `hl2.exe` is never loaded and nothing happens. If another mod already put an Ultimate ASI Loader proxy in that folder, leave it alone and skip this step.
3. Put `HalfLife2HeadTracking.asi` in that same `bin\` folder. The Nexus ZIP mirrors the game folder, so extracting it over `<game>\Half-Life 2\` lands it there for you.
4. Launch the game once. The mod writes `HeadTracking.ini` and `HeadTracking.log` next to `hl2.exe`.

## Setting Up OpenTrack

In OpenTrack, set **Output** to `UDP over network` and enter host `127.0.0.1`,
port `4242`. Map yaw, pitch and roll, and X, Y and Z as well if you want
positional tracking. Press **Start**, then launch the game.

Centering is done in your tracker. Use OpenTrack's Center bind, SteamVR's reset,
or the CENTER button in your phone app.

### VR Headset Setup

1. Connect the headset to the PC over Air Link, Virtual Desktop or a link cable.
2. Start SteamVR and let it finish setting the headset up.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on `UDP over network`, host `127.0.0.1`, port `4242`.

### Webcam Setup

Set OpenTrack's **Input** to `neuralnet tracker`. It tracks your face from a
plain webcam, with no markers, clip or IR hardware. Even lighting on your face
is what it needs most.

### Phone App Setup

The mod accepts one thing: OpenTrack UDP packets on port `4242`. A phone app is
usable here if it sends that protocol itself, or ships a PC-side companion that
does. Check your app against that first.

For an app that does send it, what decides the wiring is how much filtering it
does before the packet leaves the phone. An app that filters on-device can point
straight at this PC's LAN address (run `ipconfig` to find it) on UDP port
`4242`. A raw or lightly filtered feed sent direct will jitter, because the
mod's smoothing is sized to take the edge off a clean signal rather than to
rescue a noisy one. That app should send into OpenTrack instead, on a spare port
such as `5252`, with OpenTrack's output going to `127.0.0.1:4242` so its filters
and curves clean the feed up first.

The test is quicker than the reading: send direct, hold your head still, and if
the view drifts or shakes, route it through OpenTrack.

I made [Headcam](https://headcam.app) so decent tracking was free for anybody
with a phone already in their pocket. It filters on-device, so it can send
direct. Any other app that filters enough works exactly the same way.

A phone on WiFi is a remote connection and gets `RemoteSmoothing`. So does a
tracker running on this same PC that sends to the machine's LAN address instead
of `127.0.0.1`, because the classifier sees a transport and not a machine. Only
loopback counts as local.

## Controls

Two equivalent binding sets, use whichever your keyboard has:

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. 6DOF, rotation and position together
2. Rotation only, positional tracking off
3. Position only, rotational tracking off
4. Back to 6DOF

`Page Down` / `Ctrl+Shift+H` switches yaw between horizon-locked (yaw around the
world up axis, the default) and camera-local (yaw composed with the camera's
current pitch and roll). That switch is not written back to the ini, so the next
launch starts from whatever `WorldSpaceYaw` says.

The nav-cluster keys are rebindable under `[Hotkeys]` in `HeadTracking.ini`. The
chords are fixed. Hotkeys only fire while the Half-Life 2 window has focus.

## Configuration

`HeadTracking.ini` is written next to `hl2.exe` on first launch. It is read once
at startup, so restart the game after editing it. Any key you leave out falls
back to its default, so an ini from an older version keeps working, and deleting
the file regenerates a clean default set.

```ini
; Half-Life 2 head tracking - default config

[Network]
Port=4242
EnableOnStartup=1

[Sensitivity]
Yaw=1
Pitch=1
Roll=1
InvertYaw=0
InvertPitch=0
InvertRoll=0

[Smoothing]
; Picked per connection from the tracker's source address, and applied
; to both rotation and position. 0 = no smoothing, 1 = heavy.
; LocalSmoothing: tracker runs on this machine (loopback)
LocalSmoothing=0
; RemoteSmoothing: tracker is a remote device on the network
RemoteSmoothing=0.15

[Deadzone]
Yaw=0
Pitch=0
Roll=0

[Position]
; 6DOF head position, applied to the render view origin only
Enabled=1
; WorldScale = Source units per metre of head movement (1 unit = 1 inch; 39.37 = 1:1)
WorldScale=39.37
SensX=1
SensY=1
SensZ=1
; Flip an axis if leaning moves the view the wrong way. Trackers
; disagree on whether they report in your frame or the camera's
; mirrored view of it. Inversion is applied after the limits below,
; so flipping Z keeps the generous forward allowance on leaning in.
InvertX=0
InvertY=0
InvertZ=0
; Movement envelope in metres before world scaling
LimitX=0.3
LimitY=0.2
LimitZ=0.4
LimitZBack=0.1

[Hotkeys]
Toggle=0x23
YawMode=0x22
; Page Up: cycle 6DOF -> rotation-only -> position-only
ModeCycle=0x21

[View]
; true = horizon-locked yaw (default), false = camera-local yaw
WorldSpaceYaw=1
; Field of view, same units as the game's fov_desired cvar (horizontal
; degrees at 4:3; the mod widens it for your real aspect ratio as the
; engine does). Written into the render view rather than the cvar, so it
; is not bound by fov_desired's own range. 0 = leave the game's FOV
; alone. Applies only while tracking is enabled (End).
Fov=0
; The weapon is drawn with its own FOV. Widening Fov leaves the gun
; looking oversized against the wider world: LOWER this to shrink it.
; 0 = leave the game's viewmodel FOV alone.
FovViewmodel=0

[Debug]
; Writes HeadTracking.log next to hl2.exe, fresh every launch (the
; previous session is kept as HeadTracking.prev.log, and nothing else). It
; records the build profile, the tracker connection and the pose being
; applied. That is the file to attach to a bug report - leave it on.
LogToFile=1
```

`Fov` accepts 30 to 150, or 0 for off, and is applied as a ratio against your
current FOV rather than written flat over it, so weapon zooms, the suit zoom and
scripted camera moves still work and scale by the same factor.

## Troubleshooting

Read `HeadTracking.log`, next to `hl2.exe`, first. It is rewritten from empty
every launch, so it holds the session you just played and nothing older; the
launch before it is kept as `HeadTracking.prev.log`, which is where a crashed
session ends up once you relaunch. The log records the build profile that
matched, whether the hooks installed, the tracker connection and which smoothing
is in force. Attach it to a bug report.

**Mod not loading (no log file at all)**

- Confirm `winmm.dll` is in `<game>\Half-Life 2\bin\`, not next to `hl2.exe`. Source only loads DLLs out of `bin\`.
- Confirm you took the x86 Ultimate ASI Loader. The x64 build cannot load into a 32-bit process.
- Confirm `HalfLife2HeadTracking.asi` is in that same `bin\` folder, spelled as shipped, and that `[Debug] LogToFile` has not been set to `0`.

**Log says the mod is staying dormant**

- Your `client.dll` is not in the mod's build profile registry. The log line names the direction: newer than the mod knows about (the game patched, check the Releases page), older (let Steam finish updating), or a repacked binary the mod will not engage on.
- The episodes, Lost Coast and Deathmatch each load their own `client.dll` and are not in the registry. Only the base Half-Life 2 campaign is supported today.

**No tracking response**

- Check the tracker is running with its output set to UDP `127.0.0.1:4242`, and that your firewall is not blocking that port.
- Press `End` (or `Ctrl+Shift+Y`) to toggle tracking on, and check `[Network] EnableOnStartup` has not been set to `0`.
- If the log says `UDP port 4242 busy, receiver will retry in background`, another app is holding the port. Close it and keep playing; the mod retries twice a second.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` if the tracker is a phone or another device on the network, or route it through OpenTrack so its filters can clean the feed up.
- For webcam tracking, improve the lighting on your face.
- If the view flicks between two positions, two apps are both sending to port 4242. The log names both addresses; close the one you are not using.

**Leaning or turning moves the view the wrong way**

- Flip the axis with `InvertX`, `InvertY` or `InvertZ` under `[Position]`, or `InvertYaw` / `InvertPitch` / `InvertRoll` under `[Sensitivity]`, then restart. The log states the inversions in force at startup.
- If yaw feels wrong only when looking steeply up or down, toggle between horizon-locked and camera-local yaw with `Page Down` (or `Ctrl+Shift+H`).

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLLs. The mod loader (Ultimate ASI
Loader) is only removed if the installer put it there. Use `uninstall.cmd /force`
to remove it anyway.

## Building from Source

Requires [pixi](https://pixi.sh), CMake 3.20 or newer, and Visual Studio with
the x86 C++ toolset. Half-Life 2 is a 32-bit process, so the build targets Win32.

```powershell
git clone --recursive https://github.com/itsloopyo/half-life-2-headtracking.git
cd half-life-2-headtracking
pixi run build-release
pixi run test
pixi run package
```

`pixi run package` produces the installer and Nexus ZIPs in `release\`. No copy
of the game is needed to build.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

The mod statically links and redistributes third-party components under their
own licenses. Each is listed with its version and full upstream notice in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md), and both release ZIPs carry
`LICENSE` and `THIRD-PARTY-NOTICES.md` alongside the binary. The demo clip at
the top of this page contains Half-Life 2 footage, which remains the property of
Valve and is not covered by the MIT license above.

## Credits

- Valve, developer and publisher of Half-Life 2
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) - ASI loader (MIT)
- [OpenTrack](https://github.com/opentrack/opentrack) - head tracking protocol and software (ISC)
- [MinHook](https://github.com/TsudaKageyu/minhook) - function hooking library (BSD-2-Clause)
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core) - shared tracking pipeline (MIT)

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Valve. It requires
a legitimately purchased copy of the game. Use at your own risk.
