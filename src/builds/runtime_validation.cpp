// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "builds/runtime_validation.h"
#include "builds/build_registry.h"
#include "debug_log.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <exception>
#include <vector>

namespace headtracking::builds {
namespace {
uint32_t g_base = 0;
std::atomic<bool> g_healthy{false};

bool ReadMemory(const void* address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, out, size, &read) && read == size;
}

uint32_t ReadPointer(uint32_t address) {
    uint32_t result = 0;
    Require(address % 4 == 0 && ReadMemory(reinterpret_cast<const void*>(address), &result, sizeof(result)),
            "live pointer is unaligned or unreadable");
    return result;
}

bool Executable(uint32_t pointer) {
    MEMORY_BASIC_INFORMATION region{};
    return VirtualQuery(reinterpret_cast<void*>(pointer), &region, sizeof(region)) == sizeof(region) &&
        region.State == MEM_COMMIT && !(region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        (region.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}

void* Interface(HMODULE module, const char* name) {
    using Factory = void*(__cdecl*)(const char*, int*);
    Require(module != nullptr, "interface module not loaded");
    auto factory = reinterpret_cast<Factory>(GetProcAddress(module, "CreateInterface"));
    Require(factory != nullptr, "interface module does not export CreateInterface");
    void* object = factory(name, nullptr);
    Require(object != nullptr, name);
    return object;
}

void CheckSlots(uint32_t object, std::initializer_list<unsigned> slots) {
    const uint32_t table = ReadPointer(object);
    for (unsigned slot : slots)
        Require(table <= UINT32_MAX - slot * 4 && Executable(ReadPointer(table + slot * 4)),
                "live interface slot is not executable");
}

void RejectLive(const char* error) {
    if (g_healthy.exchange(false)) HT_LOG("[discovery] live contract rejected: %s; tracking writes disabled", error);
}
}

const BuildProfile* ResolveRuntimeBuild(HMODULE client) {
    try {
        wchar_t path[MAX_PATH];
        const DWORD count = GetModuleFileNameW(client, path, MAX_PATH);
        Require(count && count < MAX_PATH, "cannot determine client.dll campaign path");
        const std::wstring full(path);
        const std::wstring legacy = L"\\hl2\\bin\\client.dll", complete = L"\\hl2_complete\\bin\\client.dll";
        Require((full.size() >= legacy.size() && _wcsicmp(full.c_str() + full.size() - legacy.size(), legacy.c_str()) == 0) ||
                (full.size() >= complete.size() && _wcsicmp(full.c_str() + full.size() - complete.size(), complete.c_str()) == 0),
                "this campaign route is outside hl2/hl2_complete support");
        cameraunlock::memory::PeFingerprint fp{};
        Require(cameraunlock::memory::ReadPeFingerprint(client, fp), "cannot read client.dll fingerprint");
        Require(fp.SizeOfImage > 0 && fp.SizeOfImage < 0x10000000, "invalid client.dll image size");
        HT_LOG("[discovery] client.dll fingerprint=%08X/%08X/%08X", fp.TimeDateStamp, fp.SizeOfImage, fp.CheckSum);
        const auto base = reinterpret_cast<uintptr_t>(client);
        Require(base <= UINT32_MAX - fp.SizeOfImage, "client.dll image address overflows");
        std::vector<uint8_t> bytes(fp.SizeOfImage);
        size_t offset = 0;
        while (offset < bytes.size()) {
            MEMORY_BASIC_INFORMATION region{};
            Require(VirtualQuery(reinterpret_cast<void*>(base + offset), &region, sizeof(region)) == sizeof(region),
                    "cannot query client.dll pages");
            const size_t length = std::min<size_t>(bytes.size() - offset,
                reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize - (base + offset));
            Require(length > 0, "invalid client.dll page extent");
            if (region.State == MEM_COMMIT && !(region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                Require(ReadMemory(reinterpret_cast<void*>(base + offset), bytes.data() + offset, length),
                        "cannot snapshot readable client.dll pages");
            offset += length;
        }
        DiscoveryImage image(bytes.data(), fp.SizeOfImage, static_cast<uint32_t>(base));
        std::string error;
        const auto* profile = SelectProfile(image, fp, error);
        Require(profile != nullptr, error.c_str());
        g_base = static_cast<uint32_t>(base);
        const auto& o = profile->offsets;
        HT_LOG("[discovery] runtime route validated: render=%X reticle=%X trace=%X projection=%X viewport=%X "
               "local=%X engine=%X fov=%X/%X light=%X owner=%X", o.render_view_rva, o.aim.draw_position_rva,
               o.aim.trace_line_rva, o.aim.screen_transform_rva, o.aim.viewport_rva, o.aim.local_player_rva,
               o.engine.engine_ptr_rva, o.fov.fov_desired_rva, o.fov.viewmodel_fov_rva,
               o.flashlight_update_rva, o.player_flashlight);
        return profile;
    } catch (const std::exception& error) {
        HT_LOG("[discovery] rejected before hooks: %s", error.what());
        return nullptr;
    }
}

bool ValidateRuntimeObjects(HMODULE client) {
    try {
        const auto* result = ActiveDiscovery();
        Require(result != nullptr, "no complete discovery result");
        const auto& o = result->offsets;
        const auto engineModule = GetModuleHandleW(L"engine.dll");
        const uint32_t engine = reinterpret_cast<uint32_t>(Interface(engineModule, "VEngineClient014"));
        Require(ReadPointer(g_base + o.engine.engine_ptr_rva) == engine, "engine interface ownership mismatch");
        CheckSlots(engine, {26, 84, 87, 28, 21, 51, 36});
        const uint32_t trace = reinterpret_cast<uint32_t>(Interface(engineModule, "EngineTraceClient003"));
        Require(ReadPointer(g_base + result->trace_interface) == trace, "trace interface ownership mismatch");
        CheckSlots(trace, {4});
        const uint32_t clientInterface = reinterpret_cast<uint32_t>(Interface(client, "VClient017"));
        CheckSlots(clientInterface, {27});
        Require(ReadPointer(g_base + result->render_object) == g_base + result->render_vtable,
                "render singleton/vtable mismatch");
        for (const auto& named : {std::pair<uint32_t, const char*>{o.fov.fov_desired_rva, "fov_desired"},
                                 {o.fov.viewmodel_fov_rva, "viewmodel_fov"}}) {
            const uint32_t object = g_base + named.first;
            Require(ReadPointer(object) == g_base + result->convar_vtable &&
                    ReadPointer(object + result->convar_parent) == object, "FOV ConVar type/parent mismatch");
            char name[32]{};
            const auto namePointer = ReadPointer(object + o.fov.convar_name);
            Require(ReadMemory(reinterpret_cast<void*>(namePointer), name, std::strlen(named.second) + 1) &&
                    std::strcmp(name, named.second) == 0, "FOV ConVar name mismatch");
        }
        g_healthy.store(true);
        HT_LOG("[discovery] live render, engine, trace, client and FOV identities validated");
        return true;
    } catch (const std::exception& error) {
        HT_LOG("[discovery] live initialization rejected before hooks: %s", error.what());
        return false;
    }
}

bool RuntimeHealthy() { return g_healthy.load(); }

bool ValidateRenderObject(void* object) {
    if (!RuntimeHealthy()) return false;
    try {
        const auto& result = *ActiveDiscovery();
        Require(reinterpret_cast<uint32_t>(object) == g_base + result.render_object &&
                ReadPointer(reinterpret_cast<uint32_t>(object)) == g_base + result.render_vtable,
                "unexpected render object");
        ValidatedLocalFlashlight();
        return RuntimeHealthy();
    } catch (const std::exception& error) { RejectLive(error.what()); return false; }
}

void* ValidatedLocalFlashlight() {
    if (!RuntimeHealthy()) return nullptr;
    try {
        const auto& result = *ActiveDiscovery();
        const uint32_t player = ReadPointer(g_base + result.local_player_global);
        if (!player) return nullptr;
        const uint32_t table = ReadPointer(player);
        Require(table >= g_base && std::binary_search(result.player_vtables.begin(), result.player_vtables.end(), table - g_base),
                "local player lacks the validated primary C_BasePlayer identity");
        Require(player <= UINT32_MAX - result.offsets.player_flashlight, "player flashlight field overflows");
        const uint32_t light = ReadPointer(player + result.offsets.player_flashlight);
        if (light) Require(ReadPointer(light) == g_base + result.flashlight_vtable, "local flashlight type mismatch");
        return reinterpret_cast<void*>(light);
    } catch (const std::exception& error) { RejectLive(error.what()); return nullptr; }
}

}
