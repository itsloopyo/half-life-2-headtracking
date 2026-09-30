// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "builds/discovery_image.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>

namespace headtracking::builds {

void Require(bool valid, const char* reason) {
    if (!valid) throw std::runtime_error(reason);
}

uint32_t Unique(const std::vector<uint32_t>& candidates, const char* reason) {
    std::set<uint32_t> distinct(candidates.begin(), candidates.end());
    Require(distinct.size() == 1, reason);
    return *distinct.begin();
}

int Instruction::Base() const {
    if (!(code.flags & F_MODRM) || code.modrm_mod == 3 || code.p_67) return -1;
    if (code.flags & F_SIB) {
        if (code.sib_index != 4 || (code.modrm_mod == 0 && code.sib_base == 5)) return -1;
        return code.sib_base;
    }
    if (code.modrm_mod == 0 && code.modrm_rm == 5) return -1;
    return code.modrm_rm;
}

int32_t Instruction::Displacement() const {
    if (code.flags & F_DISP8) return static_cast<int8_t>(code.disp.disp8);
    if (code.flags & F_DISP32) return static_cast<int32_t>(code.disp.disp32);
    return 0;
}

uint32_t Instruction::Branch() const {
    const int32_t displacement = (code.flags & F_IMM8)
        ? static_cast<int8_t>(code.imm.imm8) : static_cast<int32_t>(code.imm.imm32);
    const int64_t result = static_cast<int64_t>(rva) + code.len + displacement;
    Require(result > 0 && result <= UINT32_MAX, "branch target overflows image address");
    return static_cast<uint32_t>(result);
}

DiscoveryImage::DiscoveryImage(const uint8_t* bytes, uint32_t size, uint32_t base)
    : m_bytes(bytes), m_size(size), m_base(base) {
    Require(bytes && size >= 0x100, "truncated DOS header");
    Require((Read(0) & 0xffff) == 0x5a4d, "invalid DOS signature");
    const uint32_t pe = Read(0x3c);
    Require(pe <= size - 0x100 && Read(pe) == 0x4550, "invalid PE header");
    Require((Read(pe + 4) & 0xffff) == 0x14c && (Read(pe + 24) & 0xffff) == 0x10b,
            "discovery requires a PE32 i386 image");
    Require(Read(pe + 24 + 56) == size, "mapped image size disagrees with PE header");
    const uint32_t count = Read(pe + 4) >> 16;
    const uint32_t optionalSize = Read(pe + 20) & 0xffff;
    Require(count > 0 && count <= 96 && optionalSize >= 96, "invalid PE section count or optional header");
    const uint64_t table = static_cast<uint64_t>(pe) + 24 + optionalSize;
    Require(table + count * 40 <= size, "truncated section table");
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t p = static_cast<uint32_t>(table) + i * 40;
        ImageSection section{Read(p + 12), Read(p + 8), Read(p + 36)};
        Require(section.begin <= size && section.size <= size - section.begin,
                "section exceeds mapped image");
        for (const auto& previous : m_sections)
            Require(section.begin >= previous.begin + previous.size ||
                    previous.begin >= section.begin + section.size, "overlapping PE sections");
        m_sections.push_back(section);
    }
}

uint32_t DiscoveryImage::Read(uint32_t rva) const {
    Require(rva <= m_size && 4 <= m_size - rva, "read exceeds mapped image");
    uint32_t result;
    std::memcpy(&result, m_bytes + rva, 4);
    return result;
}

uint32_t DiscoveryImage::Rva(uint32_t address) const {
    Require(address >= m_base && address - m_base < m_size, "pointer outside owning image");
    return address - m_base;
}

bool DiscoveryImage::Contains(uint32_t rva, uint32_t size, uint32_t flags) const {
    for (const auto& section : m_sections)
        if ((section.flags & flags) == flags && rva >= section.begin &&
            rva - section.begin <= section.size && size <= section.size - (rva - section.begin))
            return true;
    return false;
}

std::vector<uint32_t> DiscoveryImage::Find(const void* data, uint32_t size, uint32_t flags) const {
    std::vector<uint32_t> result;
    for (const auto& section : m_sections) {
        if ((section.flags & flags) != flags || size > section.size) continue;
        for (uint32_t i = 0; i <= section.size - size; ++i)
            if (std::memcmp(m_bytes + section.begin + i, data, size) == 0)
                result.push_back(section.begin + i);
    }
    return result;
}

std::vector<uint32_t> DiscoveryImage::References(uint32_t rva, uint32_t flags) const {
    Require(rva < m_size && m_base <= UINT32_MAX - rva, "absolute reference overflows");
    const uint32_t address = m_base + rva;
    return Find(&address, 4, flags);
}

uint32_t DiscoveryImage::String(const char* value) const {
    return Unique(Find(value, static_cast<uint32_t>(std::strlen(value) + 1), 0x40000000),
                  "missing or ambiguous named anchor");
}

std::vector<uint32_t> DiscoveryImage::Vtables(const char* name) const {
    const uint32_t text = String(name);
    Require(text >= 8 && Contains(text - 8, 8), "invalid RTTI type descriptor");
    const uint32_t type = text - 8;
    std::vector<uint32_t> result;
    for (uint32_t ref : References(type)) {
        if (ref < 12 || ref % 4) continue;
        const uint32_t col = ref - 12;
        if (!Contains(col, 20) || Read(col) != 0 || Read(col + 8) != 0) continue;
        const uint32_t hierarchyAddress = Read(col + 16);
        if (hierarchyAddress < m_base || !Contains(hierarchyAddress - m_base, 16)) continue;
        const uint32_t hierarchy = hierarchyAddress - m_base;
        const uint32_t count = Read(hierarchy + 8);
        if (Read(hierarchy) != 0 || count == 0 || count > 128) continue;
        const uint32_t arrayAddress = Read(hierarchy + 12);
        if (arrayAddress < m_base || !Contains(arrayAddress - m_base, count * 4)) continue;
        const uint32_t firstAddress = Read(arrayAddress - m_base);
        if (firstAddress < m_base || !Contains(firstAddress - m_base, 24) ||
            Read(firstAddress - m_base) != m_base + type) continue;
        for (uint32_t locator : References(col)) {
            if (locator % 4 || !Contains(locator + 4, 4)) continue;
            const uint32_t method = Read(locator + 4);
            if (method >= m_base && Contains(method - m_base, 1, 0x20000000))
                result.push_back(locator + 4);
        }
    }
    Require(!result.empty(), "no validated RTTI vtable");
    return result;
}

std::vector<uint32_t> DiscoveryImage::Methods(uint32_t table) const {
    std::vector<uint32_t> result;
    for (uint32_t i = 0; i < 512 && Contains(table + i * 4, 4); ++i) {
        const uint32_t value = Read(table + i * 4);
        if (value < m_base || !Contains(value - m_base, 1, 0x20000000)) break;
        result.push_back(value - m_base);
    }
    Require(!result.empty() && result.size() < 512, "invalid vtable extent");
    return result;
}

std::vector<uint32_t> DiscoveryImage::DerivedVtables(const char* name) const {
    const uint32_t text = String(name);
    Require(text >= 8 && Contains(text - 8, 8), "invalid base RTTI descriptor");
    const uint32_t type = text - 8;
    std::set<uint32_t> result;
    for (const auto& section : m_sections) {
        if (!(section.flags & 0x40000000) || (section.flags & 0x20000000)) continue;
        for (uint32_t offset = 0; offset + 20 <= section.size; offset += 4) {
            const uint32_t col = section.begin + offset;
            if (Read(col) != 0 || Read(col + 4) != 0 || Read(col + 8) != 0) continue;
            const uint32_t h = Read(col + 16);
            if (h < m_base || !Contains(h - m_base, 16) || Read(h - m_base) != 0) continue;
            const uint32_t count = Read(h - m_base + 8), array = Read(h - m_base + 12);
            if (!count || count > 128 || array < m_base || !Contains(array - m_base, count * 4)) continue;
            const uint32_t first = Read(array - m_base);
            if (first < m_base || !Contains(first - m_base, 24) ||
                Read(first - m_base) != Read(col + 12)) continue;
            bool contains = false;
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t descriptor = Read(array - m_base + i * 4);
                if (descriptor < m_base || !Contains(descriptor - m_base, 24)) { contains = false; break; }
                if (Read(descriptor - m_base) == m_base + type &&
                    Read(descriptor - m_base + 8) == 0 &&
                    Read(descriptor - m_base + 12) == UINT32_MAX) contains = true;
            }
            if (!contains) continue;
            for (uint32_t locator : References(col)) {
                if (locator % 4 || !Contains(locator + 4, 4)) continue;
                const uint32_t method = Read(locator + 4);
                if (method >= m_base && Contains(method - m_base, 1, 0x20000000)) result.insert(locator + 4);
            }
        }
    }
    Require(!result.empty(), "no primary vtables with the required player base type");
    return {result.begin(), result.end()};
}

Instruction DiscoveryImage::Decode(uint32_t rva) const {
    uint8_t padded[32]{};
    uint32_t available = 0;
    while (available < 15 && Contains(rva + available, 1, 0x20000000)) ++available;
    Require(available > 0, "instruction outside executable section");
    std::memcpy(padded, m_bytes + rva, available);
    Instruction result{rva, {}};
    hde32_disasm(padded, &result.code);
    Require(!(result.code.flags & F_ERROR) && result.code.len && result.code.len <= available &&
            !result.code.p_67, "invalid or truncated x86 instruction");
    return result;
}

Function DiscoveryImage::Walk(uint32_t entry, uint32_t limit) const {
    Function result;
    std::vector<uint32_t> pending{entry};
    uint32_t budget = 0;
    while (!pending.empty()) {
        uint32_t p = pending.back();
        pending.pop_back();
        while (!result.count(p)) {
            Require(p >= entry && p - entry < limit && ++budget <= limit,
                    "function control flow exceeds bounded extent");
            const Instruction ins = Decode(p);
            auto next = result.upper_bound(p);
            Require(next == result.end() || p + ins.code.len <= next->first,
                    "overlapping instruction boundaries");
            if (next != result.begin()) {
                const auto& prior = std::prev(next)->second;
                Require(prior.rva + prior.code.len <= p, "branch enters an instruction");
            }
            result.emplace(p, ins);
            const auto& h = ins.code;
            if (h.opcode == 0xc3 || h.opcode == 0xc2) break;
            Require(h.opcode != 0xcc && h.opcode != 0xca && h.opcode != 0xcb &&
                    !(h.opcode == 0xff && (h.modrm_reg == 4 || h.modrm_reg == 5)),
                    "unresolved indirect jump or invalid function boundary");
            if (h.opcode == 0xe9 || h.opcode == 0xeb) { p = ins.Branch(); continue; }
            if ((h.opcode >= 0x70 && h.opcode <= 0x7f) ||
                (h.opcode == 0x0f && h.opcode2 >= 0x80 && h.opcode2 <= 0x8f))
                pending.push_back(ins.Branch());
            p += h.len;
        }
    }
    return result;
}

uint32_t DiscoveryImage::EnclosingEntry(uint32_t instruction) const {
    for (uint32_t distance = 0; distance < 512 && distance < instruction; ++distance) {
        const uint32_t p = instruction - distance;
        if (m_bytes[p - 1] != 0xcc || m_bytes[p] == 0xcc) continue;
        const auto function = Walk(p);
        Require(function.count(instruction) != 0, "anchor is not on a reachable instruction boundary");
        return p;
    }
    throw std::runtime_error("no corroborated function entry for anchor");
}

std::vector<uint32_t> Calls(const DiscoveryImage& image, const Function& function) {
    std::vector<uint32_t> result;
    for (const auto& item : function) {
        if (item.second.code.opcode != 0xe8) continue;
        const uint32_t target = item.second.Branch();
        Require(image.Contains(target, 1, 0x20000000), "direct call outside executable image");
        result.push_back(target);
    }
    return result;
}

bool HasImmediate(const Function& function, uint32_t value) {
    for (const auto& item : function)
        if ((item.second.code.flags & F_IMM32) && item.second.code.imm.imm32 == value) return true;
    return false;
}

bool Returns(const Function& function, uint16_t stackBytes) {
    bool found = false;
    for (const auto& item : function) {
        const auto& h = item.second.code;
        if (h.opcode != 0xc3 && h.opcode != 0xc2) continue;
        if ((h.opcode == 0xc3 ? 0 : h.imm.imm16) != stackBytes) return false;
        found = true;
    }
    return found;
}

}
