// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "builds/build_registry.h"

#include "debug_log.h"

namespace headtracking::builds {

namespace {

const BuildProfile* const kKnownProfiles[] = {
    &kSteamProfile_Hl2Complete_20250627,
    &kSteamProfile_20250627,
};

const BuildProfile* g_active = nullptr;

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

void LogUnrecognisedBuild(const cameraunlock::memory::PeFingerprint& fp) {
    HT_LOG("[hook] no build profile matches client.dll (TimeDateStamp=0x%08X "
           "SizeOfImage=0x%08X CheckSum=0x%08X); runtime discovery is unavailable "
           "- staying dormant", fp.TimeDateStamp, fp.SizeOfImage, fp.CheckSum);
    for (const BuildProfile* p : kKnownProfiles) {
        HT_LOG("[hook]   known profile '%s': TimeDateStamp=0x%08X SizeOfImage=0x%08X "
               "CheckSum=0x%08X",
               p->name, p->fingerprint.TimeDateStamp, p->fingerprint.SizeOfImage,
               p->fingerprint.CheckSum);
    }
}

}  // namespace headtracking::builds
