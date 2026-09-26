// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "flashlight_hook.h"

#include <cstdint>
#include <cstring>

#include "aim_state.h"
#include "builds/build_profile.h"
#include "debug_log.h"
#include "detour.h"
#include "game_state.h"
#include "log_throttle.h"
#include "plugin.h"
#include "source_math.h"

namespace headtracking {
namespace {
// UpdateLight takes four Vector references and an integer distance, and pops 20 bytes.
using UpdateLightFn = void(__fastcall*)(void*, void*, const float*, const float*,
                                       const float*, const float*, int);
using LocalPlayerFn = void*(__cdecl*)();
UpdateLightFn g_original = nullptr;
LocalPlayerFn g_localPlayer = nullptr;
uint32_t g_flashlightOffset = 0;
const AimState* g_view = nullptr;

struct PendingLight {
    void* light = nullptr;
    float origin[3], forward[3], right[3], up[3];
    int distance = 0;
};
PendingLight g_pending;

void* LocalFlashlight() {
    void* player = g_localPlayer();
    return player ? *reinterpret_cast<void**>(static_cast<uint8_t*>(player)
                                             + g_flashlightOffset) : nullptr;
}

void UpdateTrackedLight(void* light, const float* origin, const float* forward,
                        const float* right, const float* up, int distance) {
    float trackedForward[3], trackedRight[3], trackedUp[3];
    if (g_view && g_view->applied) {
        source::AngleVectors(g_view->light_angles, trackedForward, trackedRight, trackedUp);
        origin = g_view->render_origin;
        forward = trackedForward;
        right = trackedRight;
        up = trackedUp;
        static LogThrottle throttle(2, 2, 600, 600);
        if (throttle.ShouldLog()) {
            HT_LOG("[flashlight] origin=(%.2f,%.2f,%.2f) forward=(%.3f,%.3f,%.3f)",
                   origin[0], origin[1], origin[2], forward[0], forward[1], forward[2]);
        }
    }
    g_original(light, nullptr, origin, forward, right, up, distance);
}

void __fastcall HookUpdateLight(void* light, void*, const float* origin,
                                const float* forward, const float* right,
                                const float* up, int distance) {
    // LightFollowsHead=false leaves the beam as the game aims it, from the game's own eye.
    if (light != LocalFlashlight() || !GetPlugin().GetConfig().light.follows_head) {
        g_original(light, nullptr, origin, forward, right, up, distance);
        return;
    }
    if (g_view) {
        UpdateTrackedLight(light, origin, forward, right, up, distance);
        return;
    }
    if (!GetPlugin().IsEnabled() || !GetGameState().IsGameplayActive()) {
        g_pending.light = nullptr;
        g_original(light, nullptr, origin, forward, right, up, distance);
        return;
    }
    // The player updates its light before RenderView samples the tracker. Defer
    // that update so the beam and camera consume the same frame's pose.
    g_pending.light = light;
    std::memcpy(g_pending.origin, origin, sizeof(g_pending.origin));
    std::memcpy(g_pending.forward, forward, sizeof(g_pending.forward));
    std::memcpy(g_pending.right, right, sizeof(g_pending.right));
    std::memcpy(g_pending.up, up, sizeof(g_pending.up));
    g_pending.distance = distance;
}
}

bool InstallFlashlightHook(void* client, const builds::BuildProfile& profile) {
    const auto& offsets = profile.offsets;
    if (!offsets.flashlight_update_rva || !offsets.player_flashlight ||
        !offsets.aim.local_player_rva) {
        HT_LOG("[flashlight] missing build offsets; beam follows the game's aim");
        return false;
    }
    auto* base = static_cast<uint8_t*>(client);
    g_localPlayer = reinterpret_cast<LocalPlayerFn>(base + offsets.aim.local_player_rva);
    g_flashlightOffset = offsets.player_flashlight;
    return InstallDetour("flashlight", "UpdateLight", base + offsets.flashlight_update_rva,
                         reinterpret_cast<void*>(&HookUpdateLight),
                         reinterpret_cast<void**>(&g_original));
}

void BeginFlashlightView(const AimState& aim) {
    g_view = &aim;
    if (!g_pending.light) return;
    // Switching the light off or changing levels may have destroyed this object.
    if (g_pending.light == LocalFlashlight()) {
        UpdateTrackedLight(g_pending.light, g_pending.origin, g_pending.forward,
                           g_pending.right, g_pending.up, g_pending.distance);
    }
    g_pending.light = nullptr;
}

void EndFlashlightView() { g_view = nullptr; }
}
