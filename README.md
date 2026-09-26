# Half-Life 2 Head Tracking

![Half-Life 2 running with this mod](https://raw.githubusercontent.com/itsloopyo/half-life-2-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Half-Life 2 that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - your head moves the camera the frame is rendered from, while the mouse or controller still controls where you shoot. The player's own eye angles and position are never written, so weapon fire, traces, physics and NPC behavior are unchanged.
- **6DOF tracking** - yaw, pitch and roll, plus positional lean, peek and duck.
- **Flashlight follows your head** - the flashlight turns with your head rather than your aim, and a little further than the view does, so the beam lands past the centre of the screen where your eyes are. `LightMultiplier` (1.5 by default) sets how far; `LightFollowsHead=false` leaves the beam on your aim.
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [Half-Life 2](https://store.steampowered.com/app/220/HalfLife_2/) on Steam (app 220), on the build whose `client.dll` is dated 27 June 2025. Both of app 220's branches are covered: the default one, which launches `-game hl2_complete` and plays Half-Life 2, Episode One and Episode Two as a single campaign, and `steam_legacy`, which launches `-game hl2`. On any other build the mod stays dormant and the game runs vanilla.
- A tracking source that sends the OpenTrack UDP protocol, such as [OpenTrack](https://github.com/opentrack/opentrack/releases) with a webcam.
- 64-bit Windows 10 or 11. The game itself is a 32-bit process, so the mod and its loader are both x86.

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **Half-Life 2**, and click
**Play with head tracking**.

### Standalone Installer

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
4. Launch the game once. The mod writes `CameraUnlock.ini` and `HeadTracking.log` next to `hl2.exe`.

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

Each action has a list of keys, and any key in it fires the action. By default
each list holds a nav-cluster key and a chord, so use whichever your keyboard has:

| Action              | Default keys               | Setting                |
|---------------------|----------------------------|------------------------|
| Toggle tracking     | `End`, `Ctrl+Shift+Y`      | `ToggleKey`            |
| Cycle tracking mode | `PageUp`, `Ctrl+Shift+G`   | `CycleTrackingModeKey` |
| Toggle yaw mode     | `PageDown`, `Ctrl+Shift+H` | `YawModeKey`           |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. 6DOF, rotation and position together
2. Rotation only, positional tracking off
3. Position only, rotational tracking off
4. Back to 6DOF

`Page Down` / `Ctrl+Shift+H` switches yaw between horizon-locked (yaw around the
world up axis, the default) and camera-local (yaw composed with the camera's
current pitch and roll).

The tracking mode and the yaw mode are saved to `CameraUnlock.ini` the moment
they change, so the next launch starts in the mode you left it in. `End` turns
tracking on and off for the session only and saves nothing: whether tracking is
on at launch is `EnableOnStartup`.

Every key in the three lists, the chords included, can be changed or removed
under `[Hotkeys]` in `CameraUnlock.ini`, for example `ToggleKey=F8, Ctrl+Shift+Y`.
Hotkeys only fire while the Half-Life 2 window has focus.

## Configuration

<!-- cameraunlock:config -->
The mod reads its settings from `CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`
- `LightFollowsHead=true`
- `LightMultiplier=1.5`

With every setting at its default, the file reads:

```ini
; Half-Life 2 head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default

[Light]
; true: a light you carry points where you look instead of where you aim.
LightFollowsHead=default
; How far the light turns for each degree your head turns.
; 1 matches the view, 0 keeps the light on your aim.
LightMultiplier=default

[View]
; Field of view in degrees, as the game's fov_desired: horizontal, at 4:3, and the mod
; widens it for your screen as the game does. 0 leaves the game's own. Otherwise 30 to
; 150, which fov_desired's own 75 to 120 does not bound. A zoom still narrows the view by
; the factor it always did. Applies only while head tracking is on.
Fov=0.0
; Field of view the weapon in your hands is drawn with, in the same degrees. A wider Fov
; leaves the weapon looking oversized: lower this to shrink it. 0 leaves the game's own.
FovViewmodel=0.0

[Debug]
; true: write HeadTracking.log beside hl2.exe, new at every launch, with the launch before
; kept as HeadTracking.prev.log. It records the game build, the tracker connection and
; the view the mod draws. Attach it to a bug report.
LogToFile=true
```
<!-- /cameraunlock:config -->

`Fov` accepts 30 to 150, or 0 for off, and is applied as a ratio against your
current FOV rather than written flat over it, so weapon zooms, the suit zoom and
scripted camera moves still work and scale by the same factor.

The settings are read once at startup, so restart the game after editing the
file.

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
- Confirm `HalfLife2HeadTracking.asi` is in that same `bin\` folder, spelled as shipped, and that `LogToFile` under `[Debug]` in `CameraUnlock.ini` has not been set to `false`.

**Log says the mod is staying dormant**

- Your `client.dll` is not in the mod's build profile registry. The log line names the direction: newer than the mod knows about (the game patched, check the Releases page), older (let Steam finish updating), or a repacked binary the mod will not engage on.
- The Episode One (app 380) and Episode Two (app 420) entries in your Steam library both load `episodic\bin\client.dll`, and Lost Coast and Deathmatch each load their own. None of those are in the registry. The episodes are head-tracked when you play them inside app 220, whose default campaign loads `hl2_complete\bin\client.dll`.

**No tracking response**

- Check the tracker is running with its output set to UDP `127.0.0.1:4242`, and that your firewall is not blocking that port.
- Press `End` (or `Ctrl+Shift+Y`) to toggle tracking on, and check `EnableOnStartup` in `CameraUnlock.ini` has not been set to `false`.
- If the log says `UDP port 4242 busy, receiver will retry in background`, another app is holding the port. Close it and keep playing; the mod retries twice a second.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` in `CameraUnlock.ini` if the tracker is a phone or another device on the network, or route it through OpenTrack so its filters can clean the feed up.
- For webcam tracking, improve the lighting on your face.
- If the view flicks between two positions, two apps are both sending to port 4242. The log names both addresses; close the one you are not using.

**Leaning or turning moves the view the wrong way**

- The mod applies the head pose as your tracker sends it, with no inversion of its own. Flip the axis in your tracker app.
- If yaw feels wrong only when looking steeply up or down, toggle between horizon-locked and camera-local yaw with `Page Down` (or `Ctrl+Shift+H`).

## Updating

Download the new release and run `install.cmd` again. Your settings in `CameraUnlock.ini` are kept, and the installer does not touch `HeadTracking.ini`.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLLs and its log files, and leaves
`CameraUnlock.ini` and `HeadTracking.ini` in place, so a reinstall keeps your
settings. The mod loader (Ultimate ASI Loader) is only removed if the installer
put it there. Use `uninstall.cmd /force` to remove it anyway.

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
