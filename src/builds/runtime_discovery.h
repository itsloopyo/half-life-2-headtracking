// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include "builds/build_profile.h"
#include "builds/discovery_image.h"

namespace headtracking::builds {

struct DiscoveryResult {
    OffsetTable offsets{};
    uint32_t render_vtable = 0;
    uint32_t render_object = 0;
    uint32_t flashlight_vtable = 0;
    uint32_t trace_interface = 0;
    uint32_t local_player_global = 0;
    uint32_t convar_vtable = 0;
    uint32_t convar_parent = 0;
    std::vector<uint32_t> player_vtables;
};

DiscoveryResult Discover(const DiscoveryImage& image);
void DiscoverConVarFields(const DiscoveryImage& image, DiscoveryResult& result);

}
