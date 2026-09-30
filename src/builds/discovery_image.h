// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <hde32.h>

namespace headtracking::builds {

struct ImageSection {
    uint32_t begin, size, flags;
};

struct Instruction {
    uint32_t rva;
    hde32s code;
    int Base() const;
    int32_t Displacement() const;
    uint32_t Branch() const;
};

using Function = std::map<uint32_t, Instruction>;

class DiscoveryImage {
public:
    DiscoveryImage(const uint8_t* bytes, uint32_t size, uint32_t base);
    bool Contains(uint32_t rva, uint32_t size, uint32_t flags = 0x40000000) const;
    uint32_t Read(uint32_t rva) const;
    uint32_t Rva(uint32_t address) const;
    uint32_t Base() const { return m_base; }
    std::vector<uint32_t> Find(const void* data, uint32_t size, uint32_t flags) const;
    std::vector<uint32_t> References(uint32_t rva, uint32_t flags = 0x40000000) const;
    uint32_t String(const char* value) const;
    std::vector<uint32_t> Vtables(const char* name) const;
    std::vector<uint32_t> DerivedVtables(const char* name) const;
    std::vector<uint32_t> Methods(uint32_t table) const;
    Instruction Decode(uint32_t rva) const;
    Function Walk(uint32_t entry, uint32_t limit = 16384) const;
    uint32_t EnclosingEntry(uint32_t instruction) const;
private:
    const uint8_t* m_bytes;
    uint32_t m_size, m_base;
    std::vector<ImageSection> m_sections;
};

void Require(bool valid, const char* reason);
uint32_t Unique(const std::vector<uint32_t>& candidates, const char* reason);
std::vector<uint32_t> Calls(const DiscoveryImage& image, const Function& function);
bool HasImmediate(const Function& function, uint32_t value);
bool Returns(const Function& function, uint16_t stackBytes);

}
