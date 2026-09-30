// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "builds/build_registry.h"

#include <cstring>
#include <exception>

namespace headtracking::builds {

namespace {

const BuildProfile* const kKnownProfiles[] = {
    &kSteamProfile_Hl2Complete_20250627,
    &kSteamProfile_20250627,
};

const BuildProfile* g_active = nullptr;
BuildProfile g_dynamic{};
DiscoveryResult g_discovery{};

bool SameOffsets(const OffsetTable& a, const OffsetTable& b) {
    return a.render_view_rva == b.render_view_rva &&
        std::memcmp(&a.view_setup, &b.view_setup, sizeof(a.view_setup)) == 0 &&
        std::memcmp(&a.aim, &b.aim, sizeof(a.aim)) == 0 &&
        std::memcmp(&a.fov, &b.fov, sizeof(a.fov)) == 0 &&
        a.engine.engine_ptr_rva == b.engine.engine_ptr_rva &&
        std::strcmp(a.engine.interface_version, b.engine.interface_version) == 0 &&
        a.engine.slot_is_in_game == b.engine.slot_is_in_game &&
        a.engine.slot_is_paused == b.engine.slot_is_paused &&
        a.engine.slot_is_menu_background == b.engine.slot_is_menu_background &&
        a.engine.slot_is_drawing_loading_image == b.engine.slot_is_drawing_loading_image &&
        a.engine.slot_get_max_clients == b.engine.slot_get_max_clients &&
        a.engine.slot_get_level_name == b.engine.slot_get_level_name &&
        a.flashlight_update_rva == b.flashlight_update_rva && a.player_flashlight == b.player_flashlight;
}

}  // namespace

const BuildProfile* MatchProfile(const cameraunlock::memory::PeFingerprint& fp) {
    g_active = nullptr;
    for (const BuildProfile* p : kKnownProfiles) {
        if (p->fingerprint.Matches(fp)) {
            g_active = p;
            return p;
        }
    }
    return nullptr;
}

const BuildProfile* ActiveProfile() { return g_active; }

const DiscoveryResult* ActiveDiscovery() {
    return g_active == &g_dynamic ? &g_discovery : nullptr;
}

const BuildProfile* SelectProfile(const DiscoveryImage& image,
    const cameraunlock::memory::PeFingerprint& fingerprint, std::string& error) {
    g_active = nullptr;
    g_dynamic = {};
    g_discovery = {};
    error.clear();
    try {
        const auto discovered = Discover(image);
        for (const auto* known : kKnownProfiles)
            if (known->fingerprint.Matches(fingerprint))
                Require(SameOffsets(known->offsets, discovered.offsets),
                        "runtime discovery disagrees with the exact historical profile");
        g_discovery = discovered;
        g_dynamic = {"runtime-source-win32", fingerprint, discovered.offsets};
        g_active = &g_dynamic;
        return g_active;
    } catch (const std::exception& failure) {
        error = failure.what();
        return nullptr;
    }
}

}  // namespace headtracking::builds
