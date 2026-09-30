// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include "builds/build_profile.h"
#include "builds/runtime_discovery.h"

namespace headtracking::builds {

// Append-only: never edit a shipped profile's offsets in place (that strands
// every user still on that build) - add a new entry and put it on top of
// kKnownProfiles in build_registry.cpp. Definitions live in steam_offsets.cpp.
extern const BuildProfile kSteamProfile_Hl2Complete_20250627;
extern const BuildProfile kSteamProfile_20250627;

// The profile whose fingerprint matches this client.dll, or nullptr.
const BuildProfile* MatchProfile(const cameraunlock::memory::PeFingerprint& fp);

// Selection publishes only after validation and clears the result on failure.
const BuildProfile* ActiveProfile();
const BuildProfile* SelectProfile(const DiscoveryImage& image,
    const cameraunlock::memory::PeFingerprint& fingerprint, std::string& error);
const DiscoveryResult* ActiveDiscovery();

}  // namespace headtracking::builds
