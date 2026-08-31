// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
// Steam Win32 build profiles for Half-Life 2's client.dll. The install ships
// one per campaign dir (hl2, hl2_complete, episodic, lostcoast, hl2mp), each a
// different binary, so several profiles are live at once and a player moving
// between campaigns moves between them. Append-only:
// a new patch gets a new entry here and a new line at the top of kKnownProfiles
// in build_registry.cpp. Nothing in this file is ever edited in place.
//
// Rederive by locating CViewRender::RenderView in a disassembler, and read the
// running DLL's fingerprint with `pixi run check-fingerprint`.

#include "builds/build_registry.h"

namespace headtracking::builds {

// CViewSetup is 200 bytes on this branch - CViewRender::RenderView copies it as
// 50 dwords, which is what fixes the tail and so the whole layout.
//
// The head of the struct is read straight off RenderView's own use of it: it
// passes (+0x00, +0x08, +0x10, +0x18) to the viewport calls, so the rect ints
// are doubled (x/unscaledX, y/unscaledY, width/unscaledWidth), and it compares
// +0x1C against 2 and passes it as an eye index, which makes that field
// m_eStereoEye rather than the fourth unscaled int - the unscaled height is
// pushed out to +0x20. From there the declaration order runs m_bOrtho (padded
// to four), the four ortho floats, fov, fovViewmodel, origin, angles.
//
// The two FOVs are pinned by measurement, not by that count alone: in game at
// 1280x800 with the default fov_desired of 75, +0x38 reads 85.28, which is 75
// widened by (1280/800)/(4/3) to the last decimal place, and +0x34 reads 0.0
// (m_OrthoBottom, unused outside an ortho view). Everything after it follows.
constexpr ViewSetupOffsets kViewSetupLayout_2025 = {
    0x40u,  // origin (Vector x, y, z)
    0x4Cu,  // angles (QAngle pitch, yaw, roll)
    0x38u,  // fov, horizontal degrees, already widened for this viewport
    0x3Cu,  // fovViewmodel - the float straight after fov
    0x10u,  // rect width
    0x18u,  // rect height
};

// Every function here is one GetDrawPosition itself calls, found by decompiling
// it: its VR branch traces MASK_SHOT along the aim and projects the impact
// point, which is exactly the shape the head-tracked reticle needs. (That
// branch is gated on a live HMD, so on a normal session the vanilla crosshair
// is at hard screen centre and the mod's detour supplies the whole answer.)
// trace_t::endpos at 12 and fraction at 44 are CBaseTrace's, read off the same
// decompilation, and the mod checks them against the ray it asked for before
// trusting either.
//
// GetDrawPosition is __cdecl here, not __thiscall - it ends in a bare `ret` and
// never touches ecx - and it takes its QAngle by value, so its detour sees six
// stack arguments and no `this`.
constexpr AimOffsets kAimLayout_20250627 = {
    0x155420u,  // CHudCrosshair::GetDrawPosition
    0x087950u,  // UTIL_TraceLine
    0x1D18E0u,  // ScreenTransform
    0x1C56E0u,  // GetFullscreenViewport(&w, &h)
    0x0C2F30u,  // C_BasePlayer::GetLocalPlayer
    12u,        // trace_t::endpos
    44u,        // trace_t::fraction
};

// The IVEngineClient* client.dll itself calls through, at client.dll+0x4AE0B4:
// ScreenTransform reads WorldToScreenMatrix from it (slot 36) and
// CHudCrosshair::ShouldDraw reads IsDrawingLoadingImage (28) and IsPaused (84),
// which is where those two slot numbers were read off. IsInGame (26) and
// IsConnected (27) are `signon == 6` and `signon > 1` in engine.dll;
// GetMaxClients (21) and GetLevelName (51) were identified the same way.
constexpr EngineStateOffsets kEngineState_20250627 = {
    0x4AE0B4u,
    "VEngineClient014",
    26u,  // IsInGame
    84u,  // IsPaused
    87u,  // IsLevelMainMenuBackground
    28u,  // IsDrawingLoadingImage
    21u,  // GetMaxClients
    51u,  // GetLevelName
};

// The FOV ConVars, located from their name strings: each name is pushed as the
// first argument of its ConVar constructor with the object itself in ecx, so
// the instruction pair names the object outright. Read out of the same
// registrations, fov_desired is default 75, min 75, max 120, flags 0x280
// (FCVAR_ARCHIVE | FCVAR_USERINFO) and viewmodel_fov is default 54, flags
// 0x4000 (FCVAR_CHEAT) - the reason the mod carries its own override at all.
//
// The two field offsets are the standard ConCommandBase / ConVar layout for
// 32-bit MSVC: vtable, m_pNext, m_bRegistered, then m_pszName at 0x0C, and past
// ConVar's second vtable and its parent/default/string members to m_fValue at
// 0x2C. Confirmed at load by reading the name back off the object.
constexpr FovConVarOffsets kFovConVars_20250627 = {
    0x4E2A80u,  // fov_desired
    0x4D9218u,  // viewmodel_fov
    0x0Cu,      // ConCommandBase::m_pszName
    0x2Cu,      // ConVar::m_fValue
};

// hl2\bin\client.dll dated 2025-06-27 (Steam buildid 19307283). The RTTI
// vftable for CViewRender sits at rva 0x3AF540 and slot 6 is RenderView, at rva
// 0x1E21C0 - confirmed by the telemetry marker inside it, which names both
// "CViewRender::RenderView" and game\client\viewrender.cpp:2016.
extern const BuildProfile kSteamProfile_20250627 = {
    "steam-win32-20250627",
    { 0x685DE804u, 0x00598000u, 0x00000000u },
    { 0x1E21C0u, kViewSetupLayout_2025, kAimLayout_20250627, kEngineState_20250627,
      kFovConVars_20250627 },
};

// hl2_complete\bin\client.dll dated 2025-06-27, four seconds after the hl2 one
// above and from the same source tree, but a distinct binary: the campaign dirs
// each ship their own client.dll and the player picks one at launch, so both
// profiles have to be live at once rather than superseding each other.
//
// Everything kViewSetupLayout_2025 rests on reproduces here, so it is reused
// rather than duplicated: RenderView copies the struct as 50 dwords, passes
// (+0x00, +0x08, +0x10, +0x18) to the viewport calls, compares +0x1C against 2
// as the stereo eye index, and hands +0x40 to the area-portal call as origin.
constexpr AimOffsets kAimLayout_Hl2Complete_20250627 = {
    0x155750u,  // CHudCrosshair::GetDrawPosition
    0x087A80u,  // UTIL_TraceLine
    0x1D1CB0u,  // ScreenTransform
    0x1C5AF0u,  // GetFullscreenViewport(&w, &h)
    0x0C31A0u,  // C_BasePlayer::GetLocalPlayer
    12u,        // trace_t::endpos
    44u,        // trace_t::fraction
};

// Same VEngineClient014 in the same shared bin\engine.dll - there is only one
// engine.dll in the install and every campaign dir loads it - so the slot
// numbers are not a re-derivation but the same vtable. Only client.dll's own
// pointer to it moved. Slots 28 and 84 were read back off this build anyway,
// out of CHudCrosshair::ShouldDraw, which calls both.
constexpr EngineStateOffsets kEngineState_Hl2Complete_20250627 = {
    0x4B00D4u,
    "VEngineClient014",
    26u,  // IsInGame
    84u,  // IsPaused
    87u,  // IsLevelMainMenuBackground
    28u,  // IsDrawingLoadingImage
    21u,  // GetMaxClients
    51u,  // GetLevelName
};

// Located the same way as the hl2 pair, and carrying the same registrations:
// fov_desired with flags 0x280 (FCVAR_ARCHIVE | FCVAR_USERINFO), viewmodel_fov
// with 0x4000 (FCVAR_CHEAT). The ConVar field offsets are the MSVC layout the
// mod checks by reading the name back off the object at load.
constexpr FovConVarOffsets kFovConVars_Hl2Complete_20250627 = {
    0x4E4AA0u,  // fov_desired
    0x4DB238u,  // viewmodel_fov
    0x0Cu,      // ConCommandBase::m_pszName
    0x2Cu,      // ConVar::m_fValue
};

// RenderView is slot 6 of the CViewRender vftable at rva 0x3B1590, and its own
// telemetry marker names "CViewRender::RenderView" at viewrender.cpp:2016 -
// the same source line the hl2 profile's marker gives.
extern const BuildProfile kSteamProfile_Hl2Complete_20250627 = {
    "steam-win32-hl2complete-20250627",
    { 0x685DE808u, 0x0059A000u, 0x00000000u },
    { 0x1E25D0u, kViewSetupLayout_2025, kAimLayout_Hl2Complete_20250627,
      kEngineState_Hl2Complete_20250627, kFovConVars_Hl2Complete_20250627 },
};

}  // namespace headtracking::builds
