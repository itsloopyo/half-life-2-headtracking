// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "aim_state.h"

namespace headtracking {

namespace {

AimState g_state;
unsigned g_frame = 0;

}  // namespace

void PublishAimState(const AimState& state) {
    g_state = state;
    g_state.frame = ++g_frame;
}

const AimState& CurrentAimState() { return g_state; }

}  // namespace headtracking
