#pragma once
// Razer mouse model table for DPI / polling-rate control. GENERATED from OpenMouse's
// @openmouse/protocol 0.60.0 (AGPL-3.0-or-later), dist/razer/devices.js (RAZER_PRODUCTS, the
// PRODUCT_DEFINITIONS rows with their audited transaction ids from TRANSACTION_3F / TRANSACTION_1F,
// everything else 0xFF), flattened to one row per PID. 111 models.
// Product list and ids originate from OpenRazer driver/razermouse_driver.h (GPL-2.0).
//
// Deliberately absent, as in devices.js: legacy pre-90-byte-report mice (Orochi 2011 0x0013,
// DeathAdder 3.5G 0x0016/0x0029), Orochi V2 Bluetooth 0x0095, and Viper Mini 0x008A, Viper V4 Pro
// 0x00E5/0x00E6 and Cobra 0x00A3, which OpenMouse drives with their own protocol code.
//
// Columns: pid, name, transaction id, timing group, wireless receiver, max DPI (from published
// specs, the mouse cannot report it), DPI store byte of the DPI read (0x01 = "VARSTORE", 0x00 =
// NOSTORE for DeathAdder V2 per OpenRazer; writes always use VARSTORE, as OpenRazer does),
// polling rates (0 = unused slot), extended polling command
// (class 00 id 40/C0, divisor of 8000) instead of legacy (00/05 and 00/85, divisor of 1000),
// commit transaction id (0 = none; second extended write with selector 0x01 that makes the rate
// persist: devices.js extendedPollingCommitTransactionId, plus OpenRazer razer_attr_write_poll_rate,
// see below), probe flag (Mouse Dock Pro: the paired mouse decides
// between the 1 kHz and 8 kHz ladders, discovered at read time), verified on hardware by OpenMouse.
#include <cstdint>

enum class RzGroup : uint8_t { Standard, Index3, AtherisReceiver, NewReceiver, ViperReceiver };

struct RazerModel {
    uint16_t pid;
    const wchar_t* name;
    uint8_t tid;
    RzGroup group;
    bool wireless;
    int maxDpi;
    uint8_t dpiStore;
    int rates[6];
    bool extended;
    uint8_t commitTid;
    bool probeRates;
    bool verified;
};

// Commit transaction ids beyond devices.js, from OpenRazer razer_attr_write_poll_rate (second write
// with selector 0x01 after the 0x00 write): 0x1F for 0x009E/0x009F, 0x00BE/0x00BF and 0x00EF/0x00F0;
// 0xFF for Viper 8KHz 0x0091 and DeathAdder V3 Pro 0x00C3.
inline const RazerModel kRazerModels[] = {
    { 0x0015, L"Naga", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x001F, L"Naga Epic", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0020, L"Abyssus 1800", 0xFF, RzGroup::Standard, false, 1800, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0024, L"Mamba 2012 (Wired)", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0025, L"Mamba 2012", 0xFF, RzGroup::Standard, true, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x002E, L"Naga 2012", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x002F, L"Imperator 2012", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0032, L"Ouroboros 2012", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0034, L"Taipan", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0036, L"Naga Hex (Red)", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0037, L"DeathAdder 2013", 0xFF, RzGroup::Standard, false, 6400, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0038, L"DeathAdder 1800", 0xFF, RzGroup::Standard, false, 1800, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0039, L"Orochi 2013", 0xFF, RzGroup::Standard, true, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x003E, L"Naga Epic Chroma (Wired)", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x003F, L"Naga Epic Chroma", 0xFF, RzGroup::Standard, true, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0040, L"Naga 2014", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0041, L"Naga Hex", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0042, L"Abyssus 2014", 0xFF, RzGroup::Standard, false, 1800, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0043, L"DeathAdder Chroma", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0044, L"Mamba (Wired)", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0045, L"Mamba", 0xFF, RzGroup::Standard, true, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0046, L"Mamba Tournament Edition", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0048, L"Orochi (Wired)", 0xFF, RzGroup::Standard, false, 8200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x004C, L"Diamondback Chroma", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, true },
    { 0x004F, L"DeathAdder 2000", 0xFF, RzGroup::Standard, false, 2000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0050, L"Naga Hex V2", 0x3F, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0053, L"Naga Chroma", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0054, L"DeathAdder 3500", 0xFF, RzGroup::Standard, false, 3500, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0059, L"Lancehead (Wired)", 0x3F, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x005A, L"Lancehead", 0x3F, RzGroup::Standard, true, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x005B, L"Abyssus V2", 0xFF, RzGroup::Standard, false, 5000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x005C, L"DeathAdder Elite", 0x3F, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x005E, L"Abyssus 2000", 0xFF, RzGroup::Standard, false, 2000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0060, L"Lancehead Tournament Edition", 0x3F, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0062, L"Atheris", 0x1F, RzGroup::AtherisReceiver, true, 7200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0064, L"Basilisk", 0x3F, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0065, L"Basilisk Essential", 0x3F, RzGroup::Standard, false, 6400, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0067, L"Naga Trinity", 0xFF, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x006A, L"Abyssus Elite (D.Va Edition)", 0xFF, RzGroup::Standard, false, 7200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x006B, L"Abyssus Essential", 0xFF, RzGroup::Standard, false, 7200, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x006C, L"Mamba Elite", 0x1F, RzGroup::Standard, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x006E, L"DeathAdder Essential", 0x3F, RzGroup::Standard, false, 6400, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x006F, L"Lancehead Wireless", 0x3F, RzGroup::NewReceiver, true, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0070, L"Lancehead Wireless (Wired)", 0x3F, RzGroup::NewReceiver, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0071, L"DeathAdder Essential White Edition", 0x3F, RzGroup::Standard, false, 6400, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0072, L"Mamba Wireless", 0x3F, RzGroup::NewReceiver, true, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0073, L"Mamba Wireless (Wired)", 0x3F, RzGroup::NewReceiver, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0077, L"Pro Click", 0x1F, RzGroup::NewReceiver, true, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x007A, L"Viper Ultimate (Wired)", 0x3F, RzGroup::ViperReceiver, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x007B, L"Viper Ultimate", 0x3F, RzGroup::ViperReceiver, true, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0x00, false, false },
    { 0x007C, L"DeathAdder V2 Pro (Wired)", 0x3F, RzGroup::ViperReceiver, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x007D, L"DeathAdder V2 Pro", 0x3F, RzGroup::ViperReceiver, true, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0x00, false, false },
    { 0x0080, L"Pro Click (Wired)", 0x1F, RzGroup::NewReceiver, false, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0083, L"Basilisk X HyperSpeed", 0xFF, RzGroup::NewReceiver, true, 16000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0084, L"DeathAdder V2", 0x3F, RzGroup::Standard, false, 20000, 0x00, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0085, L"Basilisk V2", 0x1F, RzGroup::Standard, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0086, L"Basilisk Ultimate (Wired)", 0x1F, RzGroup::NewReceiver, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0088, L"Basilisk Ultimate", 0x1F, RzGroup::NewReceiver, true, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x008C, L"DeathAdder V2 Mini", 0x3F, RzGroup::Standard, false, 8500, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x008D, L"Naga Left-Handed Edition 2020", 0x1F, RzGroup::Standard, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x008F, L"Naga Pro (Wired)", 0x1F, RzGroup::NewReceiver, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0090, L"Naga Pro", 0x1F, RzGroup::NewReceiver, true, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0091, L"Viper 8KHz", 0xFF, RzGroup::Standard, false, 20000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0xFF, false, false },
    { 0x0094, L"Orochi V2", 0x1F, RzGroup::AtherisReceiver, true, 18000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0096, L"Naga X", 0x1F, RzGroup::Index3, false, 18000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0098, L"DeathAdder Essential (2021)", 0x3F, RzGroup::Standard, false, 6400, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x0099, L"Basilisk V3", 0x1F, RzGroup::Index3, false, 26000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x009A, L"Pro Click Mini", 0x1F, RzGroup::NewReceiver, true, 12000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x009C, L"DeathAdder V2 X HyperSpeed", 0x1F, RzGroup::NewReceiver, true, 14000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x009E, L"Viper Mini Signature Edition (Wired)", 0x1F, RzGroup::ViperReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x1F, false, false },
    { 0x009F, L"Viper Mini Signature Edition", 0x1F, RzGroup::ViperReceiver, true, 30000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x1F, false, false },
    { 0x00A1, L"DeathAdder V2 Lite", 0x1F, RzGroup::Standard, false, 8500, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00A4, L"Mouse Dock Pro", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0x00, true, true },
    { 0x00A5, L"Viper V2 Pro", 0x1F, RzGroup::NewReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, true },
    { 0x00A6, L"Viper V2 Pro", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0x00, false, true },
    { 0x00A7, L"Naga V2 Pro (Wired)", 0x1F, RzGroup::NewReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, true },
    { 0x00A8, L"Naga V2 Pro", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0x00, false, true },
    { 0x00AA, L"Basilisk V3 Pro (Wired)", 0x1F, RzGroup::NewReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00AB, L"Basilisk V3 Pro", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0x00, false, false },
    { 0x00AF, L"Cobra Pro (Wired)", 0x1F, RzGroup::NewReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00B0, L"Cobra Pro", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0x00, false, false },
    { 0x00B2, L"DeathAdder V3", 0x1F, RzGroup::Standard, false, 30000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x1F, false, false },
    { 0x00B3, L"HyperPolling Wireless Dongle", 0x1F, RzGroup::ViperReceiver, true, 30000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0xFF, false, false },
    { 0x00B4, L"Naga V2 HyperSpeed", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00B6, L"DeathAdder V3 Pro (Wired)", 0x1F, RzGroup::NewReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, true },
    { 0x00B7, L"DeathAdder V3 Pro", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, true },
    { 0x00B8, L"Viper V3 HyperSpeed", 0x1F, RzGroup::ViperReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, true },
    { 0x00B9, L"Basilisk V3 X HyperSpeed", 0x1F, RzGroup::NewReceiver, true, 18000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00BE, L"DeathAdder V4 Pro (Wired)", 0x1F, RzGroup::NewReceiver, false, 45000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x1F, false, false },
    { 0x00BF, L"DeathAdder V4 Pro", 0x1F, RzGroup::NewReceiver, true, 45000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x1F, false, false },
    { 0x00C0, L"Viper V3 Pro", 0x1F, RzGroup::ViperReceiver, false, 35000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, true },
    { 0x00C1, L"Viper V3 Pro", 0x1F, RzGroup::ViperReceiver, true, 35000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x1F, false, true },
    { 0x00C2, L"DeathAdder V3 Pro (Wired)", 0x1F, RzGroup::NewReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00C3, L"DeathAdder V3 Pro", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, true, 0xFF, false, false },
    { 0x00C4, L"DeathAdder V3 HyperSpeed (Wired)", 0x1F, RzGroup::NewReceiver, false, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00C5, L"DeathAdder V3 HyperSpeed", 0x1F, RzGroup::NewReceiver, true, 30000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00C7, L"Pro Click V2 Vertical Edition (Wired)", 0x1F, RzGroup::NewReceiver, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00C8, L"Pro Click V2 Vertical Edition", 0x1F, RzGroup::NewReceiver, true, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00CB, L"Basilisk V3 35K", 0x1F, RzGroup::Index3, false, 35000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00CC, L"Basilisk V3 Pro 35K (Wired)", 0x1F, RzGroup::NewReceiver, false, 35000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00CD, L"Basilisk V3 Pro 35K", 0x1F, RzGroup::NewReceiver, true, 35000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x00, false, false },
    { 0x00D0, L"Pro Click V2 (Wired)", 0x1F, RzGroup::NewReceiver, false, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00D1, L"Pro Click V2", 0x1F, RzGroup::NewReceiver, true, 20000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00D3, L"Basilisk Mobile (Wired)", 0x1F, RzGroup::NewReceiver, false, 18000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00D4, L"Basilisk Mobile", 0x1F, RzGroup::NewReceiver, true, 18000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00D6, L"Basilisk V3 Pro 35K Phantom Green (Wired)", 0x1F, RzGroup::NewReceiver, false, 35000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00D7, L"Basilisk V3 Pro 35K Phantom Green", 0x1F, RzGroup::NewReceiver, true, 35000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x00, false, false },
    { 0x00DE, L"Viper V3 Pro SE (Wired)", 0x1F, RzGroup::ViperReceiver, false, 35000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00DF, L"Viper V3 Pro SE", 0x1F, RzGroup::ViperReceiver, true, 35000, 0x01, { 125, 500, 1000, 0, 0, 0 }, false, 0x00, false, false },
    { 0x00EF, L"DeathAdder V4 Pro Carbon Fiber Edition (Wired)", 0x1F, RzGroup::NewReceiver, false, 45000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x1F, false, false },
    { 0x00F0, L"DeathAdder V4 Pro Carbon Fiber Edition", 0x1F, RzGroup::NewReceiver, true, 45000, 0x01, { 125, 500, 1000, 2000, 4000, 8000 }, true, 0x1F, false, false },
};
inline constexpr int kRazerModelCount = (int)(sizeof(kRazerModels) / sizeof(kRazerModels[0]));
