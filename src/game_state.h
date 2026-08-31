// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include "builds/build_profile.h"

namespace headtracking {

// Whether the head pose should reach the camera at all this frame.
//
// The render-view hook fires for everything the engine draws a 3D world for,
// which includes the animated map behind the main menu, the frame under the
// pause menu and the last frame held on screen while a save loads. Tracking in
// any of those is wrong: nothing the player does with their head is meant to
// move a menu backdrop, and a paused game that keeps drifting looks broken.
//
// It also fires in multiplayer. Half-Life 2 ships Deathmatch alongside the
// single-player campaign, and a mod that moves one player's view where other
// players are watching is a mod that has to be off. Deathmatch runs a different
// client.dll whose fingerprint this mod does not know, so it is already dormant
// there; the max-clients test below is the second latch, for any multiplayer
// session reached from a client.dll that IS recognised.
class GameState {
public:
    // Resolves the engine interface the gate reads. Returns false when it
    // cannot be trusted, which is what leaves the whole mod dormant - a gate
    // that cannot say "not in gameplay" is not a gate.
    bool Resolve();

    // Re-evaluated once per rendered frame; the individual reads are a handful
    // of loads each, and the level name is only fetched when the cheaper tests
    // have already passed.
    bool IsGameplayActive();

private:
    void LogTransition(bool active, const char* why);

    // Set together by Resolve, and only on success: the slot numbers are only
    // meaningful for the interface version the same profile named, so a gate
    // holding one without the other could not be read at all.
    void* m_engine = nullptr;
    const builds::EngineStateOffsets* m_offsets = nullptr;

    bool m_lastActive = false;
    bool m_everLogged = false;
};

GameState& GetGameState();

}  // namespace headtracking
