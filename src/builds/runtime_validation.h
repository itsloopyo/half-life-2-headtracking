// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once
#include <Windows.h>
#include "builds/build_profile.h"

namespace headtracking::builds {
const BuildProfile* ResolveRuntimeBuild(HMODULE client);
bool ValidateRuntimeObjects(HMODULE client);
bool ValidateRenderObject(void* object);
void* ValidatedLocalFlashlight();
bool RuntimeHealthy();
}
