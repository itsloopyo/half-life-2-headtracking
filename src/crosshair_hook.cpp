// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "crosshair_hook.h"

#include <Windows.h>
#include <cstdint>

#include "aim_point.h"
#include "builds/build_registry.h"
#include "debug_log.h"
#include "detour.h"

namespace headtracking {

namespace {

// void GetDrawPosition(float* pX, float* pY, bool* pbBehindCamera,
//                      QAngle angleCrosshairOffset)   [__cdecl]
//
// __cdecl, not __thiscall: the function ends in a bare `ret`, so the caller
// cleans the stack and there is no `this` in ecx - it takes the crosshair's
// state from globals rather than from an object. The QAngle is taken by value,
// which on x86 means its three floats are pushed individually, so they arrive
// here as three separate parameters. Both details are load-bearing; a detour
// with the wrong convention or arity unbalances the stack on every HUD paint.
using GetDrawPositionFn = void(__cdecl*)(float* x, float* y, bool* behindCamera,
                                         float offsetPitch, float offsetYaw, float offsetRoll);

GetDrawPositionFn g_originalGetDrawPosition = nullptr;

// The crosshair offset angle is a weapon's own aim correction, expressed
// relative to the view angles. Vanilla adds it to CurrentViewAngles(), which is
// the head-tracked camera; the decoupled version has to add it to the clean aim
// instead, or a weapon that uses one would have its correction measured from
// the wrong camera.
void __cdecl Hook_GetDrawPosition(float* x, float* y, bool* behindCamera,
                                  float offsetPitch, float offsetYaw, float offsetRoll) {
    g_originalGetDrawPosition(x, y, behindCamera, offsetPitch, offsetYaw, offsetRoll);

    // Guarded for the same reason the render-view detour is (see
    // camera_hook.cpp): the correction allocates - it logs through
    // std::string - and an exception unwinding out of a detour, through the
    // trampoline and into client.dll's HUD paint, ends the process. A frame
    // drawn with the engine's own centred crosshair does not.
    try {
        const float offset[3] = { offsetPitch, offsetYaw, offsetRoll };
        float px = 0.0f, py = 0.0f;
        bool behind = false;
        // False is the untracked frame and the unresolvable trace, and both
        // mean the same thing here: leave the engine's own answer alone.
        if (!ComputeReticlePosition(offset, px, py, behind)) return;

        if (x) *x = px;
        if (y) *y = py;
        if (behindCamera) *behindCamera = behind;
    } catch (...) {
        // Deliberately silent: logging from here could throw again, and the
        // engine's own answer is already in place.
    }
}

}  // namespace

bool CrosshairHook::Install() {
    // ResolveAimPoint is the gate on both the build profile and its aim
    // addresses, so a profile is guaranteed by the time it returns true.
    if (!ResolveAimPoint()) return false;
    const builds::BuildProfile* profile = builds::ActiveProfile();

    HMODULE client = GetModuleHandleA("client.dll");
    if (!client) {
        HT_LOG("[crosshair] client.dll not loaded");
        return false;
    }
    void* target = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(client)
                                           + profile->offsets.aim.draw_position_rva);

    return InstallDetour("crosshair", "GetDrawPosition", target,
                         reinterpret_cast<void*>(&Hook_GetDrawPosition),
                         reinterpret_cast<void**>(&g_originalGetDrawPosition),
                         " - the crosshair stays centred (head tracking is unaffected)");
}

}  // namespace headtracking
