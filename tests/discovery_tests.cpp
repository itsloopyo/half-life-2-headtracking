// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include <Windows.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstring>
#include <exception>
#include <functional>
#include "builds/runtime_discovery.h"
#include "builds/build_registry.h"

namespace {
using namespace headtracking::builds;
struct Fixture {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x4000);
    uint32_t base;
    explicit Fixture(uint32_t address = 0x10000000) : base(address) {
        Put(0, 0x5a4d); Put(0x3c, 0x80); Put(0x80, 0x4550);
        Put(0x84, 0x3014c); Put(0x94, 0xe0); Put(0x98, 0x10b); Put(0xd0, 0x4000);
        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t header = 0x178 + i * 40;
            Put(header + 8, 0x1000); Put(header + 12, (i + 1) * 0x1000);
            Put(header + 36, i == 0 ? 0x60000000 : i == 1 ? 0x40000000 : 0xc0000000);
        }
        bytes[0x1000] = 0xc3;
        const char name[] = ".?AVFixture@@";
        std::memcpy(bytes.data() + 0x3008, name, sizeof(name));
        Put(0x210c, base + 0x3000); Put(0x2110, base + 0x2120);
        Put(0x2128, 1); Put(0x212c, base + 0x2140);
        Put(0x2140, base + 0x2160); Put(0x2160, base + 0x3000);
        Put(0x216c, UINT32_MAX);
        Put(0x2200, base + 0x2100); Put(0x2204, base + 0x1000);
    }
    void Put(uint32_t offset, uint32_t value) { std::memcpy(bytes.data() + offset, &value, 4); }
    void Code(uint32_t offset, std::initializer_list<uint8_t> code) {
        std::copy(code.begin(), code.end(), bytes.begin() + offset);
    }
    DiscoveryImage Image() const { return {bytes.data(), static_cast<uint32_t>(bytes.size()), base}; }
    void ConVar(uint8_t parent, uint8_t name, uint8_t value) {
        for (uint32_t i = 0; i < 6; ++i) Put(0x2204 + i * 4, base + 0x1000);
        Put(0x2204 + 4 * 4, base + 0x1100);
        Put(0x2204 + 5 * 4, base + 0x1200);
        Code(0x1100, {0x8b,0x41,parent,0x8b,0x40,name,0xc3});
        Code(0x1200, {0x55,0x8b,0xec,0x56,0x8b,0xf1,
            0xf3,0x0f,0x10,0x5d,8,0x0f,0x2e,0x5e,value,
            0xf3,0x0f,0x11,0x5e,value,0x5e,0x5d,0xc2,4,0});
    }
};
}

int RunDiscoveryTests() {
    using namespace headtracking::builds;
    int failures = 0;
    const auto check = [&](bool pass, const char* label) {
        std::printf("  [%s] %s\n", pass ? "PASS" : "FAIL", label);
        if (!pass) ++failures;
    };
    const auto rejects = [&](const std::function<void()>& action, const char* label) {
        bool rejected = false;
        try { action(); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, label);
    };
    std::puts("\nDiscovery contracts\n===================");
    for (uint32_t base : {0x10000000u, 0x90000000u}) {
        Fixture f(base);
        check(f.Image().Vtables(".?AVFixture@@") == std::vector<uint32_t>{0x2204},
              "RTTI resolves at both low and large-address-aware bases");
        check(f.Image().DerivedVtables(".?AVFixture@@") == std::vector<uint32_t>{0x2204},
              "primary owner relationship resolves");
        f.Put(0x2160, base + 0x3010);
        rejects([&] { f.Image().Vtables(".?AVFixture@@"); }, "wrong RTTI hierarchy owner rejected");
        rejects([&] { f.Image().DerivedVtables(".?AVFixture@@"); }, "wrong primary hierarchy owner rejected");
        f.Put(0x2160, base + 0x3000);
        f.Put(0x2168, 4);
        rejects([&] { f.Image().DerivedVtables(".?AVFixture@@"); }, "displaced player base rejected");
    }
    {
        Fixture f;
        std::memcpy(f.bytes.data() + 0x3100, f.bytes.data() + 0x3008, 14);
        rejects([&] { f.Image().String(".?AVFixture@@"); }, "duplicate named anchor rejected");
        f.bytes[0x3008] = '!'; f.bytes[0x3100] = '!';
        rejects([&] { f.Image().String(".?AVFixture@@"); }, "missing named anchor rejected");
        f.Put(0x178 + 8, 0xffffffff);
        rejects([&] { f.Image(); }, "overflowing section extent rejected");
    }
    {
        Fixture f;
        f.bytes[0x1fff] = 0xe8;
        rejects([&] { f.Image().Decode(0x1fff); }, "truncated call at executable boundary rejected");
        f.Code(0x1000, {0xeb, 0xff});
        rejects([&] { f.Image().Walk(0x1000); }, "branch into instruction rejected");
        f.Code(0x1000, {0xeb, 0x02, 0xcc, 0xcc, 0xc2, 0x14, 0});
        check(Returns(f.Image().Walk(0x1000), 20), "bounded tail branch preserves stack contract");
        check(!Returns(f.Image().Walk(0x1000), 12), "wrong argument byte count rejected");
        rejects([&] { f.Image().Walk(0x2000); }, "non-executable function rejected");
    }
    for (uint8_t shift : {uint8_t{0}, uint8_t{4}}) {
        Fixture f;
        f.ConVar(static_cast<uint8_t>(28 + shift), static_cast<uint8_t>(12 + shift), static_cast<uint8_t>(44 + shift));
        DiscoveryResult result; result.convar_vtable = 0x2204;
        DiscoverConVarFields(f.Image(), result);
        check(result.convar_parent == 28u + shift && result.offsets.fov.convar_name == 12u + shift &&
              result.offsets.fov.convar_value == 44u + shift, "ConVar fields follow moved operands");
        f.bytes[0x1206] = 0xf2;
        rejects([&] { DiscoverConVarFields(f.Image(), result); }, "double-width ConVar argument rejected");
        f.bytes[0x1206] = 0xf3;
        f.bytes[0x1213] = 0x70;
        rejects([&] { DiscoverConVarFields(f.Image(), result); }, "mismatched float comparison/store rejected");
    }
    {
        Fixture f;
        f.ConVar(28, 12, 44);
        std::copy_n(f.bytes.data() + 0x1100, 7, f.bytes.data() + 0x1400);
        f.Put(0x2204 + 4 * 4, f.base + 0x1400);
        f.Put(0x2204 + 5 * 4, f.base + 0x1000);
        f.Put(0x2204 + 6 * 4, f.base + 0x1200);
        DiscoveryResult result; result.convar_vtable = 0x2204;
        DiscoverConVarFields(f.Image(), result);
        check(result.offsets.fov.convar_value == 44 && result.offsets.fov.convar_name == 12,
              "moved function address and float setter virtual slot resolve");
        f.Put(0x216c, 0);
        rejects([&] { f.Image().DerivedVtables(".?AVFixture@@"); }, "virtual player base rejected");
    }
    {
        Fixture f;
        f.ConVar(28, 12, 44);
        f.Put(0x2204 + 6 * 4, f.base + 0x1300);
        std::copy_n(f.bytes.data() + 0x1200, 25, f.bytes.data() + 0x1300);
        f.bytes[0x130e] = 48; f.bytes[0x1313] = 48;
        DiscoveryResult result; result.convar_vtable = 0x2204;
        rejects([&] { DiscoverConVarFields(f.Image(), result); }, "ambiguous float fields rejected");
        std::string error;
        check(!SelectProfile(f.Image(), {1,0x4000,0}, error) && !ActiveProfile() && !ActiveDiscovery(),
              "incomplete image cannot publish a discovery result");
    }
    return failures;
}

int RunDiscoveryImageProbe(const char* path) {
    using namespace headtracking::builds;
    try {
        std::ifstream file(path, std::ios::binary);
        Require(file.good(), "cannot read image file");
        const std::vector<uint8_t> raw{std::istreambuf_iterator<char>(file), {}};
        Require(raw.size() >= sizeof(IMAGE_DOS_HEADER), "truncated image file");
        IMAGE_DOS_HEADER dos;
        std::memcpy(&dos, raw.data(), sizeof(dos));
        Require(dos.e_lfanew > 0 && static_cast<size_t>(dos.e_lfanew) + sizeof(IMAGE_NT_HEADERS32) <= raw.size(),
                "invalid file PE offset");
        IMAGE_NT_HEADERS32 nt;
        std::memcpy(&nt, raw.data() + dos.e_lfanew, sizeof(nt));
        Require(nt.OptionalHeader.SizeOfImage > 0 && nt.OptionalHeader.SizeOfImage < 0x10000000 &&
                nt.OptionalHeader.SizeOfHeaders <= raw.size() &&
                nt.OptionalHeader.SizeOfHeaders <= nt.OptionalHeader.SizeOfImage, "invalid mapped file size");
        std::vector<uint8_t> mapped(nt.OptionalHeader.SizeOfImage);
        std::memcpy(mapped.data(), raw.data(), nt.OptionalHeader.SizeOfHeaders);
        const size_t table = static_cast<size_t>(dos.e_lfanew) + 24 + nt.FileHeader.SizeOfOptionalHeader;
        Require(table + nt.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER) <= raw.size(), "truncated file sections");
        for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
            IMAGE_SECTION_HEADER section;
            std::memcpy(&section, raw.data() + table + i * sizeof(section), sizeof(section));
            Require(section.PointerToRawData <= raw.size() && section.SizeOfRawData <= raw.size() - section.PointerToRawData &&
                    section.VirtualAddress <= mapped.size() && section.SizeOfRawData <= mapped.size() - section.VirtualAddress,
                    "file section out of bounds");
            std::memcpy(mapped.data() + section.VirtualAddress, raw.data() + section.PointerToRawData, section.SizeOfRawData);
        }
        DiscoveryImage image(mapped.data(), static_cast<uint32_t>(mapped.size()), nt.OptionalHeader.ImageBase);
        std::string error;
        cameraunlock::memory::PeFingerprint fingerprint{nt.FileHeader.TimeDateStamp,
            nt.OptionalHeader.SizeOfImage, nt.OptionalHeader.CheckSum};
        if (!SelectProfile(image, fingerprint, error)) throw std::runtime_error(error);
        const auto result = *ActiveDiscovery();
        const auto& o = result.offsets;
        std::printf("render=%X reticle=%X trace=%X projection=%X viewport=%X local=%X engine=%X fov=%X/%X light=%X owner=%X\n",
            o.render_view_rva, o.aim.draw_position_rva, o.aim.trace_line_rva, o.aim.screen_transform_rva,
            o.aim.viewport_rva, o.aim.local_player_rva, o.engine.engine_ptr_rva,
            o.fov.fov_desired_rva, o.fov.viewmodel_fov_rva, o.flashlight_update_rva, o.player_flashlight);
        fingerprint.CheckSum ^= 1;
        if (!SelectProfile(image, fingerprint, error)) throw std::runtime_error(error);
        std::puts("PASS: real registry accepts unlisted fingerprint through discovery");
        const uint32_t relocatedBase = 0x90000000;
        const uint32_t delta = relocatedBase - nt.OptionalHeader.ImageBase;
        const auto relocations = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        Require(relocations.VirtualAddress <= mapped.size() && relocations.Size <= mapped.size() - relocations.VirtualAddress,
                "relocation directory exceeds image");
        uint32_t cursor = relocations.VirtualAddress;
        while (cursor < relocations.VirtualAddress + relocations.Size) {
            IMAGE_BASE_RELOCATION block;
            std::memcpy(&block, mapped.data() + cursor, sizeof(block));
            Require(block.SizeOfBlock >= sizeof(block) && block.SizeOfBlock <= relocations.VirtualAddress + relocations.Size - cursor,
                    "invalid relocation block");
            for (uint32_t offset = sizeof(block); offset < block.SizeOfBlock; offset += 2) {
                uint16_t fixup;
                std::memcpy(&fixup, mapped.data() + cursor + offset, 2);
                if ((fixup >> 12) == IMAGE_REL_BASED_ABSOLUTE) continue;
                Require((fixup >> 12) == IMAGE_REL_BASED_HIGHLOW, "unexpected relocation type");
                const uint32_t location = block.VirtualAddress + (fixup & 0xfff);
                Require(location <= mapped.size() - 4, "relocation exceeds image");
                uint32_t value;
                std::memcpy(&value, mapped.data() + location, 4);
                value += delta;
                std::memcpy(mapped.data() + location, &value, 4);
            }
            cursor += block.SizeOfBlock;
        }
        DiscoveryImage relocated(mapped.data(), static_cast<uint32_t>(mapped.size()), relocatedBase);
        if (!SelectProfile(relocated, fingerprint, error)) throw std::runtime_error(error);
        Require(ActiveProfile()->offsets.render_view_rva == o.render_view_rva &&
                ActiveProfile()->offsets.player_flashlight == o.player_flashlight,
                "relocation changed discovered RVAs or fields");
        std::puts("PASS: real registry discovers relocated image above 2 GB");
        const uint32_t marker = relocated.String("CViewRender::RenderView");
        mapped[marker] = '!';
        Require(SelectProfile(relocated, fingerprint, error) == nullptr && !ActiveProfile() && !ActiveDiscovery(),
                "missing anchor retained active discovery state");
        std::printf("PASS: missing anchor rejects after success (%s)\n", error.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "discovery rejected: %s\n", error.what());
        return 1;
    }
}
