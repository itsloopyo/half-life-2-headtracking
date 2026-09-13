// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

namespace headtracking {
namespace builds { struct BuildProfile; }
struct AimState;

bool InstallFlashlightHook(void* client, const builds::BuildProfile& profile);
void BeginFlashlightView(const AimState& aim);
void EndFlashlightView();
}
