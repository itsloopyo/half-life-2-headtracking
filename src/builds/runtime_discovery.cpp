// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "builds/runtime_discovery.h"

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>

namespace headtracking::builds {
namespace {

bool TryWalk(const DiscoveryImage& image, uint32_t entry, Function& out, uint32_t limit = 4096) {
    try { out = image.Walk(entry, limit); return true; }
    catch (const std::runtime_error&) { out.clear(); return false; }
}

std::vector<uint32_t> Methods(const DiscoveryImage& image, const char* name) {
    std::set<uint32_t> all;
    for (uint32_t table : image.Vtables(name)) {
        const auto methods = image.Methods(table);
        all.insert(methods.begin(), methods.end());
    }
    return {all.begin(), all.end()};
}

bool Memory(const Instruction& ins, uint8_t opcode, int base, int32_t offset, int reg) {
    return ins.code.opcode == opcode && ins.Base() == base &&
           ins.Displacement() == offset && ins.code.modrm_reg == reg && !ins.code.p_66;
}

std::vector<Instruction> Ordered(const Function& function) {
    std::vector<Instruction> result;
    for (const auto& item : function) result.push_back(item.second);
    return result;
}

uint32_t MarkerMethod(const DiscoveryImage& image, const char* type, const char* marker,
                      uint32_t& tableOut) {
    const uint32_t text = image.Base() + image.String(marker);
    std::vector<uint32_t> candidates;
    for (uint32_t table : image.Vtables(type)) {
        for (uint32_t method : image.Methods(table)) {
            Function function;
            if (TryWalk(image, method, function, 16384) && HasImmediate(function, text)) {
                candidates.push_back(method);
                tableOut = table;
            }
        }
    }
    return Unique(candidates, "missing or ambiguous RTTI/telemetry presentation method");
}

void ValidateViewCopy(const DiscoveryImage& image, const Function& render) {
    std::vector<uint32_t> copies;
    for (uint32_t target : Calls(image, render)) {
        Function function;
        if (!TryWalk(image, target, function, 1024) || !Returns(function, 4)) continue;
        const auto code = Ordered(function);
        std::set<int32_t> ints, floats;
        for (size_t i = 0; i + 1 < code.size(); ++i) {
            const auto& a = code[i];
            const auto& b = code[i + 1];
            if (a.rva + a.code.len != b.rva) continue;
            if (a.Base() != 2 || b.Base() != 6 || a.Displacement() != b.Displacement()) continue;
            if (a.code.opcode == 0x8b && a.code.modrm_reg == 0 &&
                b.code.opcode == 0x89 && b.code.modrm_reg == 0) ints.insert(a.Displacement());
            if (a.code.opcode == 0xd9 && a.code.modrm_reg == 0 &&
                b.code.opcode == 0xd9 && b.code.modrm_reg == 3) floats.insert(a.Displacement());
        }
        bool valid = true;
        for (int32_t offset : {0, 4, 8, 12, 16, 20, 24, 28, 32}) valid &= ints.count(offset) != 0;
        for (int32_t offset = 0x28; offset <= 0x64; offset += 4) valid &= floats.count(offset) != 0;
        bool argument = false;
        for (const auto& ins : code) argument |= Memory(ins, 0x8b, 5, 8, 2);
        if (valid && argument) copies.push_back(target);
    }
    Unique(copies, "CViewSetup public ABI copy types/widths disagree with render boundary");
    bool extent = false;
    const auto code = Ordered(render);
    for (size_t i = 0; i + 1 < code.size(); ++i)
        if (code[i].code.opcode == 0xb9 && code[i].code.imm.imm32 == 50) {
            for (size_t j = i + 1; j < std::min(i + 5, code.size()); ++j)
                extent |= code[j].code.opcode == 0xa5 && code[j].code.p_rep == 0xf3;
        }
    Require(extent, "CViewSetup 200-byte public ABI extent not corroborated");
}

uint32_t Reticle(const DiscoveryImage& image) {
    std::vector<uint32_t> candidates;
    for (uint32_t method : Methods(image, ".?AVCHudCrosshair@@")) {
        Function caller;
        if (!TryWalk(image, method, caller)) continue;
        for (uint32_t target : Calls(image, caller)) {
            Function function;
            if (TryWalk(image, target, function, 2048) && Returns(function, 0) &&
                HasImmediate(function, 0x46004003)) candidates.push_back(target);
        }
    }
    return Unique(candidates, "CHudCrosshair trace/projection callee missing or ambiguous");
}

struct Value {
    enum Kind { Unknown, Constant, Stack } kind = Unknown;
    int32_t value = 0;
};

struct CallSite {
    uint32_t target, instruction;
    std::vector<Value> arguments;
};

// The tracked expressions are deliberately limited to the stack-address and
// constant arguments of the HUD helper calls. Unsupported writes kill a value.
std::vector<CallSite> StackCalls(const Function& function) {
    std::array<Value, 8> regs{};
    std::vector<Value> pushes;
    std::vector<CallSite> calls;
    std::set<uint32_t> joins;
    for (const auto& item : function) {
        const auto& ins = item.second;
        const auto& h = ins.code;
        if (h.opcode == 0xe9 || h.opcode == 0xeb || (h.opcode >= 0x70 && h.opcode <= 0x7f) ||
            (h.opcode == 0x0f && h.opcode2 >= 0x80 && h.opcode2 <= 0x8f)) joins.insert(ins.Branch());
    }
    for (const auto& item : function) {
        const auto& ins = item.second;
        const auto& h = ins.code;
        if (joins.count(ins.rva)) { regs = {}; pushes.clear(); }
        if (h.opcode == 0x8d) regs[h.modrm_reg] = ins.Base() == 5 ? Value{Value::Stack, ins.Displacement()} : Value{};
        else if (h.opcode >= 0xb8 && h.opcode <= 0xbf)
            regs[h.opcode - 0xb8] = {Value::Constant, static_cast<int32_t>(h.imm.imm32)};
        else if (h.opcode == 0x8b && h.modrm_mod == 3) regs[h.modrm_reg] = regs[h.modrm_rm];
        else if (h.opcode == 0x8b) regs[h.modrm_reg] = {};
        else if (h.opcode == 0x89 && h.modrm_mod == 3) regs[h.modrm_rm] = regs[h.modrm_reg];
        else if (h.opcode == 0x31 && h.modrm_mod == 3) regs[h.modrm_rm] = {};
        else if (h.opcode == 0xa1) regs[0] = {};
        else if (h.opcode >= 0x58 && h.opcode <= 0x5f) regs[h.opcode - 0x58] = {};
        else if (!((h.opcode >= 0x50 && h.opcode <= 0x57) || h.opcode == 0x68 || h.opcode == 0x6a ||
                   (h.opcode == 0xff && (h.modrm_reg == 2 || h.modrm_reg == 6)) || h.opcode == 0xe8 || h.opcode == 0x90 ||
                   (h.opcode == 0x89 && h.modrm_mod != 3) ||
                   (h.opcode == 0xc7 && h.modrm_mod != 3) ||
                   (h.opcode == 0x0f && (h.opcode2 == 0x10 || h.opcode2 == 0x11 || h.opcode2 == 0x28 ||
                    h.opcode2 == 0x2a || h.opcode2 == 0x2e || h.opcode2 == 0x58 || h.opcode2 == 0x59 || h.opcode2 == 0x5c)) ||
                   (h.opcode >= 0xd8 && h.opcode <= 0xdf) ||
                   h.opcode == 0x85 || h.opcode == 0x84 || h.opcode == 0x3b || h.opcode == 0x39 ||
                   ((h.opcode == 0x81 || h.opcode == 0x83) && h.modrm_reg == 7))) regs = {};
        if (h.p_66) regs = {};
        if (h.opcode >= 0x50 && h.opcode <= 0x57) pushes.push_back(regs[h.opcode - 0x50]);
        if (h.opcode == 0x68) pushes.push_back({Value::Constant, static_cast<int32_t>(h.imm.imm32)});
        if (h.opcode == 0x6a) pushes.push_back({Value::Constant, static_cast<int8_t>(h.imm.imm8)});
        if (h.opcode == 0xff && h.modrm_reg == 6) pushes.push_back({});
        if (h.opcode == 0xe8 || (h.opcode == 0xff && h.modrm_reg == 2)) {
            if (h.opcode == 0xe8) calls.push_back({ins.Branch(), ins.rva,
                                                {pushes.rbegin(), pushes.rend()}});
            pushes.clear();
            regs[0] = regs[1] = regs[2] = {};
        }
        if (h.opcode == 0xe9 || h.opcode == 0xeb ||
            (h.opcode >= 0x70 && h.opcode <= 0x7f) ||
            (h.opcode == 0x0f && h.opcode2 >= 0x80 && h.opcode2 <= 0x8f)) {
            pushes.clear();
            regs = {};
        }
    }
    return calls;
}

uint32_t AbsoluteThis(const DiscoveryImage& image, const Function& function, uint32_t virtualSlot) {
    std::vector<uint32_t> matches;
    const auto code = Ordered(function);
    for (size_t i = 0; i + 2 < code.size(); ++i) {
        const auto& load = code[i];
        if (load.code.opcode != 0x8b || load.code.modrm != 0x0d) continue;
        const uint32_t global = image.Rva(load.code.disp.disp32);
        if (!image.Contains(global, 4, 0xc0000000) || global % 4) continue;
        bool tableLoaded = false;
        for (size_t j = i + 1; j < std::min(i + 10, code.size()); ++j) {
            const auto& ins = code[j];
            if (ins.code.opcode == 0xe8) break;
            if (ins.code.opcode == 0x8b && ins.code.modrm_reg == 1) break;
            if (ins.code.opcode == 0x8b && ins.code.modrm_reg == 0)
                tableLoaded = Memory(ins, 0x8b, 1, 0, 0);
            if (ins.code.opcode == 0xff && ins.code.modrm_reg == 2) {
                if (tableLoaded && ins.Base() == 0 && ins.Displacement() == static_cast<int32_t>(virtualSlot * 4))
                    matches.push_back(global);
                break;
            }
            if (ins.code.opcode == 0xe9 || ins.code.opcode == 0xeb ||
                (ins.code.opcode >= 0x70 && ins.code.opcode <= 0x7f)) break;
        }
    }
    return Unique(matches, "interface global/virtual-call relationship missing or ambiguous");
}

uint32_t RegisteredInterface(const DiscoveryImage& image, const char* name) {
    const uint32_t text = image.String(name);
    std::vector<uint32_t> matches;
    for (uint32_t ref : image.References(text, 0x20000000)) {
        if (ref == 0) continue;
        const auto push = image.Decode(ref - 1);
        if (push.code.opcode != 0x68 || push.code.imm.imm32 != image.Base() + text) continue;
        uint32_t p = ref - 1 + push.code.len;
        bool called = false;
        for (int i = 0; i < 12; ++i) {
            const auto ins = image.Decode(p);
            if (ins.code.opcode == 0xff && ins.code.modrm_reg == 2) called = true;
            if (called && ins.code.opcode == 0xa3) {
                const uint32_t slot = image.Rva(ins.code.imm.imm32);
                Require(slot % 4 == 0 && image.Contains(slot, 4, 0xc0000000), "interface storage is invalid");
                matches.push_back(slot);
                break;
            }
            if (ins.code.opcode == 0xc3 || ins.code.opcode == 0xe9) break;
            p += ins.code.len;
        }
    }
    return Unique(matches, "named interface factory result not uniquely stored");
}

uint32_t ConVar(const DiscoveryImage& image, const char* name, uint32_t& vtable) {
    const uint32_t text = image.String(name);
    const auto tables = image.Vtables(".?AVConVar@@");
    std::vector<uint32_t> objects;
    for (uint32_t ref : image.References(text, 0x20000000)) {
        if (!ref) continue;
        const auto push = image.Decode(ref - 1);
        if (push.code.opcode != 0x68 || push.code.imm.imm32 != image.Base() + text) continue;
        const uint32_t entry = image.EnclosingEntry(ref - 1);
        const Function initializer = image.Walk(entry, 512);
        Require(Returns(initializer, 0), "ConVar initializer stack contract mismatch");
        const auto object = image.Decode(ref - 1 + push.code.len);
        if (object.code.opcode != 0xb9) continue;
        const auto call = image.Decode(object.rva + object.code.len);
        if (call.code.opcode != 0xe8) continue;
        const auto ctor = image.Walk(call.Branch(), 1024);
        std::vector<uint32_t> matchingTables;
        for (uint32_t table : tables) {
            for (const auto& item : ctor) {
                const auto& ins = item.second;
                if (ins.code.opcode == 0xc7 && ins.Base() >= 0 && ins.Displacement() == 0 &&
                    ins.code.imm.imm32 == image.Base() + table) matchingTables.push_back(table);
            }
        }
        const uint32_t selected = Unique(matchingTables, "named ConVar constructor lacks primary RTTI identity");
        const uint32_t rva = image.Rva(object.code.imm.imm32);
        Require(rva % 4 == 0 && image.Contains(rva, 0x48, 0xc0000000), "ConVar object extent outside writable image");
        Require(!vtable || vtable == selected, "FOV ConVars have different types");
        vtable = selected;
        objects.push_back(rva);
    }
    return Unique(objects, "missing or ambiguous named ConVar construction");
}

void Flashlight(const DiscoveryImage& image, DiscoveryResult& result) {
    const uint32_t table = Unique(image.Vtables(".?AVCFlashlightEffect@@"), "ambiguous flashlight RTTI");
    const auto methods = image.Methods(table);
    const uint32_t marker = image.Base() + image.String("CFlashlightEffect::UpdateLightNew");
    std::vector<uint32_t> updates;
    uint32_t updateSlot = 0;
    for (size_t slot = 0; slot < methods.size(); ++slot) {
        Function function;
        if (!TryWalk(image, methods[slot], function, 512) || !Returns(function, 20)) continue;
        for (uint32_t target : Calls(image, function)) {
            Function child;
            if (TryWalk(image, target, child, 8192) && HasImmediate(child, marker)) {
                updates.push_back(methods[slot]);
                updateSlot = static_cast<uint32_t>(slot);
            }
        }
    }
    result.offsets.flashlight_update_rva = Unique(updates, "flashlight update ABI/telemetry relationship missing or ambiguous");
    result.flashlight_vtable = table;
    std::vector<int32_t> forwarded;
    bool vectors = false, distance = false;
    for (const auto& item : image.Walk(result.offsets.flashlight_update_rva, 512)) {
        const auto& ins = item.second;
        if (ins.code.opcode == 0xff && ins.code.modrm_reg == 6 && ins.Base() == 5)
            forwarded.push_back(ins.Displacement());
        if (ins.code.opcode != 0xe8) continue;
        const auto child = image.Walk(ins.Branch(), 8192);
        if (HasImmediate(child, marker)) {
            Require(forwarded == std::vector<int32_t>{20, 16, 12, 8} && Returns(child, 16),
                    "flashlight vector argument forwarding mismatch");
            vectors = true;
        } else {
            Require(forwarded == std::vector<int32_t>{24, 12, 8} && Returns(child, 12),
                    "flashlight distance argument forwarding mismatch");
            bool frame = false, integer = false;
            for (const auto& instruction : child) {
                const auto& value = instruction.second;
                frame |= value.code.opcode == 0x8b && value.code.modrm == 0xdc;
                integer |= value.code.opcode == 0x0f && value.code.opcode2 == 0x2a &&
                           value.code.p_rep == 0xf3 && value.Base() == 3 && value.Displacement() == 16;
            }
            Require(frame && integer, "flashlight distance is not a 32-bit integer argument");
            distance = true;
        }
        forwarded.clear();
    }
    Require(vectors && distance, "flashlight vector/distance routes not corroborated");
    std::vector<uint32_t> constructors;
    for (uint32_t ref : image.References(table, 0x20000000)) {
        if (ref < 2) continue;
        const auto store = image.Decode(ref - 2);
        if (store.code.opcode != 0xc7 || store.Displacement() != 0 || store.Base() < 0 ||
            store.code.imm.imm32 != image.Base() + table) continue;
        uint32_t entry;
        Function function;
        try { entry = image.EnclosingEntry(store.rva); function = image.Walk(entry, 1024); }
        catch (const std::runtime_error&) { continue; }
        if (!Returns(function, 4)) continue;
        bool argument = false, stored = false, initialized = false;
        for (const auto& item : function) {
            const auto& ins = item.second;
            argument |= Memory(ins, 0x8b, 5, 8, 0);
            stored |= Memory(ins, 0x89, store.Base(), 8, 0);
            initialized |= Memory(ins, 0xc6, store.Base(), 4, 0) && ins.code.imm.imm8 == 0;
        }
        if (argument && stored && initialized) constructors.push_back(entry);
    }
    const uint32_t constructor = Unique(constructors, "flashlight constructor identity missing or ambiguous");
    std::set<uint32_t> playerFunctions;
    for (uint32_t method : Methods(image, ".?AVC_BasePlayer@@")) {
        playerFunctions.insert(method);
        Function function;
        if (!TryWalk(image, method, function)) continue;
        const auto calls = Calls(image, function);
        playerFunctions.insert(calls.begin(), calls.end());
    }
    std::vector<uint32_t> fields;
    for (uint32_t target : playerFunctions) {
        Function function;
        if (!TryWalk(image, target, function, 1024)) continue;
        const auto calls = Calls(image, function);
        if (std::find(calls.begin(), calls.end(), constructor) == calls.end() || !Returns(function, 0)) continue;
        const auto code = Ordered(function);
        bool ownsThis = false, dispatch = false;
        for (const auto& ins : code) {
            ownsThis |= ins.code.opcode == 0x8b && ins.code.modrm == 0xd9;
            dispatch |= ins.code.opcode == 0xff && ins.code.modrm_reg == 2 && ins.Base() == 7 &&
                        ins.Displacement() == static_cast<int32_t>(updateSlot * 4);
        }
        if (!ownsThis || !dispatch) continue;
        for (size_t i = 0; i < code.size(); ++i) {
            const auto& store = code[i];
            if (store.code.opcode != 0x89 || store.Base() != 3 || store.code.modrm_reg != 0) continue;
            const int32_t field = store.Displacement();
            if (field < 4 || field % 4) continue;
            bool clear = false, load = false, test = false;
            for (const auto& ins : code) {
                clear |= Memory(ins, 0xc7, 3, field, 0) && ins.code.imm.imm32 == 0;
                load |= Memory(ins, 0x8b, 3, field, 0);
                test |= Memory(ins, 0x83, 3, field, 7) && ins.code.imm.imm8 == 0;
            }
            if (clear && load && test) fields.push_back(static_cast<uint32_t>(field));
        }
    }
    result.offsets.player_flashlight = Unique(fields, "player flashlight allocation/store/dispatch ownership not unique");
    result.player_vtables = image.DerivedVtables(".?AVC_BasePlayer@@");
}

void ValidateProjection(const Function& function) {
    bool input = false, output = false, z = false;
    std::set<int32_t> matrix, components, stores;
    for (const auto& item : function) {
        const auto& ins = item.second;
        const auto& h = ins.code;
        input |= Memory(ins, 0x8b, 5, 8, 1);
        output |= Memory(ins, 0x8b, 5, 12, 2);
        if (h.opcode == 0x0f && h.p_rep == 0xf3 && !h.p_66) {
            if (ins.Base() == 0 && (h.opcode2 == 0x10 || h.opcode2 == 0x58)) matrix.insert(ins.Displacement());
            if (ins.Base() == 1 && h.opcode2 == 0x59) components.insert(ins.Displacement());
            if (ins.Base() == 2 && h.opcode2 == 0x11) stores.insert(ins.Displacement());
        }
        z |= Memory(ins, 0xc7, 2, 8, 0) && h.imm.imm32 == 0;
    }
    Require(input && output && z && components == std::set<int32_t>{0, 4, 8} &&
            stores == std::set<int32_t>{0, 4} &&
            matrix == std::set<int32_t>{0, 4, 8, 12, 16, 20, 24, 28, 48, 52, 56, 60},
            "projection input/output float widths or matrix contract mismatch");
}

void ValidateHudOutput(const Function& function) {
    const auto code = Ordered(function);
    std::set<int32_t> floats, byteOutputs, angles;
    for (const auto& ins : code)
        if (ins.code.opcode == 0x0f && ins.code.opcode2 == 0x10 && ins.code.p_rep == 0xf3 &&
            ins.Base() == 5 && ins.Displacement() >= 20) angles.insert(ins.Displacement());
    for (size_t i = 0; i + 3 < code.size(); ++i) {
        if (!Memory(code[i], 0x8b, 5, code[i].Displacement(), 0)) continue;
        for (size_t j = i + 1; j < i + 4; ++j) {
            const auto& ins = code[j];
            if (ins.code.opcode >= 0x59 && ins.code.opcode <= 0x5f) continue;
            if (ins.Base() != 0 || ins.Displacement() != 0) break;
            if (ins.code.opcode == 0x0f && ins.code.opcode2 == 0x11 && ins.code.p_rep == 0xf3)
                floats.insert(code[i].Displacement());
            if (ins.code.opcode == 0x88) byteOutputs.insert(code[i].Displacement());
            break;
        }
    }
    Require(floats == std::set<int32_t>{8, 12} && byteOutputs == std::set<int32_t>{16} &&
            angles == std::set<int32_t>{20, 24, 28}, "HUD argument positions or scalar widths mismatch");
}

void ValidateViewport(const DiscoveryImage& image, uint32_t entry) {
    const auto function = image.Walk(entry, 256);
    Require(Returns(function, 0), "viewport calling convention mismatch");
    const auto code = Ordered(function);
    std::set<int32_t> forwarded;
    for (const auto& ins : code)
        if (ins.code.opcode == 0xff && ins.code.modrm_reg == 6 && ins.Base() == 5)
            forwarded.insert(ins.Displacement());
    Require(forwarded == std::set<int32_t>{8, 12} && Calls(image, function).size() == 1,
            "viewport width/height pointer forwarding mismatch");
}

void ValidateTraceArguments(const DiscoveryImage& image, const Function& function) {
    bool frame = false, output = false;
    std::vector<int32_t> pushes;
    std::vector<std::vector<int32_t>> forwarded;
    bool traceCall = false;
    for (const auto& item : function) {
        const auto& ins = item.second;
        const auto& h = ins.code;
        frame |= h.opcode == 0x8b && h.modrm == 0xdc;
        output |= Memory(ins, 0x8b, 3, 28, 6);
        if (h.opcode == 0xff && h.modrm_reg == 6 && ins.Base() == 3)
            pushes.push_back(ins.Displacement());
        if (h.opcode == 0xe8) {
            forwarded.push_back(pushes);
            pushes.clear();
        }
        if (h.opcode == 0xff && h.modrm_reg == 2 && ins.Base() == 0 && ins.Displacement() == 16) {
            traceCall = pushes == std::vector<int32_t>{16};
            pushes.clear();
        }
    }
    const auto calls = Calls(image, function);
    Require(frame && output && traceCall && forwarded.size() >= 2 && calls.size() >= 2 &&
            forwarded[0] == std::vector<int32_t>{12, 8} && forwarded[1] == std::vector<int32_t>{24, 20},
            "trace start/end/mask/filter/output argument positions mismatch");
    Require(Returns(image.Walk(calls[0], 1024), 8) && Returns(image.Walk(calls[1], 1024), 12),
            "trace ray/filter constructor calling convention mismatch");
}

}

void DiscoverConVarFields(const DiscoveryImage& image, DiscoveryResult& result) {
    const auto methods = image.Methods(result.convar_vtable);
    Require(methods.size() > 4, "ConCommandBase name accessor missing");
    const auto getter = Ordered(image.Walk(methods[4], 64));
    Require(getter.size() == 3 && Memory(getter[0], 0x8b, 1, getter[0].Displacement(), 0) &&
            Memory(getter[1], 0x8b, 0, getter[1].Displacement(), 0) && getter[2].code.opcode == 0xc3,
            "ConVar name accessor does not follow the parent/name contract");
    result.convar_parent = static_cast<uint32_t>(getter[0].Displacement());
    result.offsets.fov.convar_name = static_cast<uint32_t>(getter[1].Displacement());
    std::vector<uint32_t> values;
    for (uint32_t method : methods) {
        Function function;
        if (!TryWalk(image, method, function, 1024) || !Returns(function, 4)) continue;
        bool input = false, ownsThis = false;
        std::set<int32_t> compared, stored;
        for (const auto& item : function) {
            const auto& ins = item.second;
            const auto& h = ins.code;
            ownsThis |= h.opcode == 0x8b && h.modrm == 0xf1;
            if (h.opcode != 0x0f || h.p_66) continue;
            input |= h.opcode2 == 0x10 && h.p_rep == 0xf3 && ins.Base() == 5 && ins.Displacement() == 8;
            if (h.opcode2 == 0x2e && h.p_rep == 0 && ins.Base() == 6) compared.insert(ins.Displacement());
            if (h.opcode2 == 0x11 && h.p_rep == 0xf3 && ins.Base() == 6) stored.insert(ins.Displacement());
        }
        if (!input || !ownsThis) continue;
        for (int32_t field : compared)
            if (stored.count(field)) values.push_back(static_cast<uint32_t>(field));
    }
    result.offsets.fov.convar_value = Unique(values, "ConVar float setter width/field identity missing or ambiguous");
    for (uint32_t field : {result.convar_parent, result.offsets.fov.convar_name, result.offsets.fov.convar_value})
        Require(field % 4 == 0 && field >= 4 && field <= 0x48 - 4, "ConVar field exceeds corroborated object extent");
}

DiscoveryResult Discover(const DiscoveryImage& image) {
    DiscoveryResult result;
    auto& out = result.offsets;
    image.String("VClient017");
    out.render_view_rva = MarkerMethod(image, ".?AVCViewRender@@", "CViewRender::RenderView",
                                       result.render_vtable);
    const auto render = image.Walk(out.render_view_rva);
    Require(Returns(render, 12), "RenderView stack argument contract mismatch");
    ValidateViewCopy(image, render);
    out.view_setup = {0x40, 0x4c, 0x38, 0x3c, 0x10, 0x18};
    std::vector<uint32_t> renderObjects;
    for (uint32_t ref : image.References(result.render_vtable, 0x20000000)) {
        if (ref < 6) continue;
        const auto ins = image.Decode(ref - 6);
        if (ins.code.opcode != 0xc7 || ins.code.modrm != 0x05 ||
            ins.code.imm.imm32 != image.Base() + result.render_vtable) continue;
        const uint32_t object = image.Rva(ins.code.disp.disp32);
        Require(image.Contains(object, 4, 0xc0000000), "render singleton outside writable image");
        renderObjects.push_back(object);
    }
    result.render_object = Unique(renderObjects, "render singleton constructor missing or ambiguous");

    out.aim.draw_position_rva = Reticle(image);
    const auto hud = image.Walk(out.aim.draw_position_rva, 2048);
    ValidateHudOutput(hud);
    const auto calls = StackCalls(hud);
    std::vector<uint32_t> traces;
    int32_t traceBuffer = 0;
    uint32_t traceCall = 0;
    for (const auto& call : calls) {
        const auto& args = call.arguments;
        if (args.size() < 6 || args[2].kind != Value::Constant || args[2].value != 0x46004003) continue;
        Require(args[0].kind == Value::Stack && args[1].kind == Value::Stack &&
                args[4].kind == Value::Constant && args[4].value == 0 && args[5].kind == Value::Stack,
                "HUD trace argument contract mismatch");
        traces.push_back(call.target);
        traceBuffer = args[5].value;
        traceCall = call.instruction;
    }
    out.aim.trace_line_rva = Unique(traces, "HUD trace call missing or ambiguous");
    const auto trace = image.Walk(out.aim.trace_line_rva, 1024);
    Require(Returns(trace, 0), "trace helper calling convention mismatch");
    ValidateTraceArguments(image, trace);
    result.trace_interface = AbsoluteThis(image, trace, 4);
    Require(result.trace_interface == RegisteredInterface(image, "EngineTraceClient003"),
            "trace helper does not use EngineTraceClient003");

    std::vector<uint32_t> projections, viewports, locals;
    for (const auto& call : calls) {
        const auto& args = call.arguments;
        if (call.instruction > traceCall && args.size() == 2 && args[0].kind == Value::Stack &&
            args[0].value == traceBuffer + 12 && args[1].kind == Value::Stack)
            projections.push_back(call.target);
        if (call.instruction < traceCall && args.size() == 2 && args[0].kind == Value::Stack &&
            args[1].kind == Value::Stack && args[1].value == args[0].value + 4)
            viewports.push_back(call.target);
        const auto first = image.Decode(call.target);
        if (first.code.opcode == 0xa1 && image.Decode(call.target + first.code.len).code.opcode == 0xc3) {
            const uint32_t slot = image.Rva(first.code.imm.imm32);
            Require(image.Contains(slot, 4, 0xc0000000), "local-player global outside writable image");
            locals.push_back(call.target);
            result.local_player_global = slot;
        }
    }
    out.aim.screen_transform_rva = Unique(projections, "HUD trace endpos/projection relationship missing or ambiguous");
    out.aim.viewport_rva = Unique(viewports, "HUD viewport output contract missing or ambiguous");
    out.aim.local_player_rva = Unique(locals, "HUD local-player getter missing or ambiguous");
    ValidateViewport(image, out.aim.viewport_rva);
    const auto projection = image.Walk(out.aim.screen_transform_rva, 1024);
    Require(Returns(projection, 0), "projection calling convention mismatch");
    ValidateProjection(projection);
    const uint32_t engine = AbsoluteThis(image, projection, 36);
    Require(engine == RegisteredInterface(image, "VEngineClient014"), "projection uses a different engine interface");
    out.engine = {engine, "VEngineClient014", 26, 84, 87, 28, 21, 51};
    out.aim.trace_endpos = 12;
    out.aim.trace_fraction = 44;

    out.fov.fov_desired_rva = ConVar(image, "fov_desired", result.convar_vtable);
    out.fov.viewmodel_fov_rva = ConVar(image, "viewmodel_fov", result.convar_vtable);
    DiscoverConVarFields(image, result);
    Flashlight(image, result);
    return result;
}

}
