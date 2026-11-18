// Logitech HID++ 2.0 mouse DPI and report-rate control (native port).
//
// Protocol logic follows OpenMouse's @openmouse/protocol 0.60.0 (AGPL-3.0-or-later):
//   dist/drivers/logitech/hidpp.js
//     FEATURE ids (ADJUSTABLE_DPI 0x2201, EXTENDED_ADJUSTABLE_DPI 0x2202, REPORT_RATE 0x8060,
//     EXTENDED_ADJUSTABLE_REPORT_RATE 0x8061, ONBOARD_PROFILES 0x8100, DEVICE_INFORMATION 0x0003),
//     resolveDpiFeature / resolveReportRateFeature (extended first, legacy fallback),
//     getDpiOptions (0x2202 fn 2 pages), readDpiConfiguration (fn 5), readDpiCapabilities (fn 1),
//     setDpi (fn 6 [0, x, y or 0, lod]), readLegacyDpi / readLegacyDpiList / setLegacyDpi (0x2201
//     fn 2 / 1 / 3, short report on a direct endpoint), readSupportedPollingRates /
//     readPollingRate / setExtendedReportRate (0x8061 fn 1 or 0 / 2 / 3 plus the rate-change
//     event), readLegacyReportRates / readLegacyReportRate / setLegacyReportRate (0x8060 fn 0/1/2),
//     reportRateConnection (0 cable, 1 Lightspeed), readIdentity + isWiredHidppConnection
//     (transport ids decide the active link), readProfileState, openActiveProfile,
//     writeActiveProfile, writeProfileSector (fn 6 / 7 / 8), readProfileSector (fn 5, 16-byte
//     chunks, overlapping tail). Requests use the long report whenever the long collection is
//     open (as drivers/logitech.cpp does); only setLegacyDpi on a direct endpoint uses the short
//     report, which OpenMouse found three direct endpoints require.
//   dist/drivers/logitech/onboard-profiles.js  see logitech_onboard.h.
//   dist/drivers/logitech/mode-status.js       not used: nothing here touches 0x8090.
// The HID++ framing, collections, receiver indexes and the C54F pass-through dongle are the ones
// src/drivers/logitech.cpp uses for the battery.
//
// What is written, and where (docs/settings-design.md goal 3: settings live on the mouse):
//  * Mouse in onboard mode (0x8100 getMode = 1), the normal state without G HUB: the value goes
//    into the active onboard profile sector, as OpenMouse's writeActiveProfile does, so it
//    survives a power cycle. DPI replaces the X/Y of the profile's default DPI slot (the one the
//    mouse loads at power-on); the report rate replaces the byte of the active link (Lightspeed or
//    cable). The sector is read, CRC-checked, patched, CRC'd, written, read back byte for byte, and
//    restored on any failure. Then the same value is sent live (no mode change) so it applies
//    at once; if the mouse refuses the live write it applies at the next power-on.
//    Only profile formats OpenMouse marks writable (2, 3, 4, 7, 8) are written; any other
//    format is refused with an error, never written live (a live-only write would silently revert).
//  * Host mode (G HUB holds the mouse): the live feature is the only setting in force; it is
//    written and read back, and `err` says it is not stored on the mouse.
//  * No 0x8100 feature (MX / productivity mice): the live feature write, as OpenMouse does.
// The host/onboard mode itself is never changed (unlike OpenMouse's ensureHostControl).
//
// PRO X 2 / PRO X 3 SUPERSTRIKE (profile format 8, USB transport ids C0A8 / C0A9, the PRO X 3
// Lightspeed dongle 046D:C54F on index 1, or the cable on 0xFF): the onboard path above. OpenMouse
// confirms format 8's layout (CRC-matching dumps) and a DPI profile write on a PRO X 3; the cable
// of a C0A9 runs 8 kHz, a C0A8 cable is capped at 1 kHz.
//
// Nothing here runs unless the menu asks: no timers, no background polling. Every exchange has a
// timeout, and the time spent waiting on silent requests is capped per device (kBudgetMs), so a
// sleeping mouse costs about a second.
#include "../settings.h"

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <set>

#include "logitech_onboard.h"

namespace {

constexpr USHORT kVid = 0x046D;
constexpr BYTE kSwId = 0x0B;      // differs from the battery poller's 0x0D, so replies never cross
constexpr int kBudgetMs = 1000;   // total wait on unanswered requests per device and operation
constexpr int kReqMs = 500;

constexpr USHORT kFeatRoot = 0x0000, kFeatInfo = 0x0003, kFeatName = 0x0005;
constexpr USHORT kFeatDpi = 0x2201, kFeatDpiExt = 0x2202, kFeatRate = 0x8060, kFeatRateExt = 0x8061;
constexpr USHORT kFeatOnboard = 0x8100;

// SettingsDevice::variant bits.
constexpr int kVarExtDpi = 1, kVarExtRate = 2, kVarWired = 4, kVarLongOnly = 8, kVarFullRateUsb = 16;
constexpr int kVarFormatShift = 8;  // bits 8..15: onboard profile format id (0 = none / unknown)

bool IsPassthroughDongle(USHORT pid) { return pid == 0xC54F; }  // PRO X3 SUPERSTRIKE dongle
bool IsBolt(USHORT pid) { return pid == 0xC548; }

// Receivers (OpenMouse vendors.js LOGITECH_RECEIVER_PRODUCT_IDS without the C0A8 mouse interface
// and the C54F pass-through dongle, which are handled on their own).
bool IsKnownReceiver(USHORT pid) {
    static const USHORT ids[] = { 0xC54D, 0xC543, 0xC539, 0xC547, 0x40BD, 0xC52B, 0xC532, 0xC53A, 0xC53D,
                                  0xC53F, 0xC541, 0xC545, 0xC518, 0xC51A, 0xC51B, 0xC521, 0xC525, 0xC526,
                                  0xC52E, 0xC52F, 0xC531, 0xC534, 0xC537, 0xC548 };
    for (USHORT id : ids)
        if (id == pid) return true;
    return false;
}

bool IsShortColl(const HidColl& c) {
    return c.outLen() == 7 && ((c.page() == 0xFF00 && c.usage() == 0x0001) || (c.page() == 0xFF43 && c.usage() == 0x0301));
}
bool IsLongColl(const HidColl& c) {
    return c.outLen() == 20 &&
           ((c.page() == 0xFF00 && c.usage() == 0x0002) ||
            (c.page() == 0xFF43 && (c.usage() == 0x0202 || c.usage() == 0x0302)));
}

std::wstring Lower(std::wstring s) {
    for (auto& ch : s) ch = (wchar_t)towlower(ch);
    return s;
}

bool IsBluetoothPath(const std::wstring& path) {
    std::wstring p = Lower(path);
    return p.find(L"{00001812-") != std::wstring::npos || p.find(L"{00001124-") != std::wstring::npos;
}

// Same grouping as PhysicalKey() in drivers/logitech.cpp: the collections of one interface differ
// only in "&colNN" and the last "&NNNN" of the instance part.
std::wstring PhysicalKey(const HidColl& c) {
    std::wstring p = Lower(c.path);
    size_t a = p.find(L'#');
    size_t b = a == std::wstring::npos ? a : p.find(L'#', a + 1);
    size_t e = b == std::wstring::npos ? b : p.find(L'#', b + 1);
    if (e == std::wstring::npos) return p;
    std::wstring hw = p.substr(a + 1, b - a - 1), inst = p.substr(b + 1, e - b - 1);
    size_t col = hw.find(L"&col");
    if (col != std::wstring::npos) hw.erase(col, 6);
    size_t amp = inst.rfind(L'&');
    if (amp != std::wstring::npos) inst.erase(amp);
    return hw + L"#" + inst;
}

std::wstring Widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// ---- transport ---------------------------------------------------------------------------------

enum class Res { Ok, Err20, Err10, Timeout };

class Link {
public:
    bool Open(const HidColl* s, const HidColl* l, bool longOnly) {
        if (s) hasS_ = s_.Open(*s);
        if (l) hasL_ = l_.Open(*l);
        longOnly_ = longOnly;
        return hasS_ || hasL_;
    }
    bool dead() const { return budget_ <= 0; }

    // Watches for the 0x8061 rate-change event [idx][feature][0x00][rate code] while requests run.
    void Watch(BYTE idx, BYTE feat) { watchIdx_ = idx; watchFeat_ = feat; event_ = -1; }
    int event() const { return event_; }

    // One request; `p` receives the reply parameters, `code` the error code of an error reply.
    Res Req(BYTE idx, BYTE feat, BYTE fn, const std::vector<BYTE>& params, std::vector<BYTE>& p, BYTE* code = nullptr,
            int timeoutMs = kReqMs, bool forceShort = false) {
        if (dead()) return Res::Timeout;
        bool useLong = hasL_ && (longOnly_ || !hasS_ || !forceShort || params.size() > 3);
        if (!useLong && !hasS_) return Res::Timeout;
        if (useLong && params.size() > 16) return Res::Timeout;
        const BYTE b3 = (BYTE)((fn << 4) | kSwId);
        std::vector<BYTE> msg(useLong ? 20 : 7, 0);
        msg[0] = useLong ? 0x11 : 0x10;
        msg[1] = idx;
        msg[2] = feat;
        msg[3] = b3;
        for (size_t i = 0; i < params.size() && 4 + i < msg.size(); ++i) msg[4 + i] = params[i];
        if (!(useLong ? l_ : s_).Write(msg)) return Res::Timeout;

        int wait = std::min(timeoutMs, budget_);
        ULONGLONG start = GetTickCount64(), end = start + (ULONGLONG)wait;
        std::vector<BYTE> r;
        while ((LONGLONG)end - (LONGLONG)GetTickCount64() > 0) {
            for (HidDev* d : { &l_, &s_ }) {
                if (!d->ok() || d->Read(r, 15) < 7) continue;
                if ((r[0] != 0x10 && r[0] != 0x11) || r[1] != idx) continue;
                if (watchFeat_ && r[1] == watchIdx_ && r[2] == watchFeat_ && r[3] == 0x00) event_ = r[4];
                if (r[2] == feat && r[3] == b3) {
                    p.assign(r.begin() + 4, r.end());
                    return Res::Ok;
                }
                if ((r[2] == 0xFF || r[2] == 0x8F) && r[3] == feat && r[4] == b3) {
                    if (code) *code = r[5];
                    return r[2] == 0xFF ? Res::Err20 : Res::Err10;
                }
            }
        }
        budget_ -= (int)(GetTickCount64() - start);
        return Res::Timeout;
    }

    bool Call(BYTE idx, BYTE feat, BYTE fn, const std::vector<BYTE>& params, std::vector<BYTE>& p,
              int timeoutMs = kReqMs, bool forceShort = false) {
        return Req(idx, feat, fn, params, p, nullptr, timeoutMs, forceShort) == Res::Ok;
    }

    // Waits for the watched event only (after the write was acknowledged).
    void WaitEvent(BYTE idx, int wanted, int timeoutMs) {
        ULONGLONG end = GetTickCount64() + (ULONGLONG)timeoutMs;
        std::vector<BYTE> r;
        while (event_ != wanted && (LONGLONG)end - (LONGLONG)GetTickCount64() > 0) {
            for (HidDev* d : { &l_, &s_ }) {
                if (!d->ok() || d->Read(r, 15) < 7) continue;
                if ((r[0] == 0x10 || r[0] == 0x11) && r[1] == idx && r[2] == watchFeat_ && r[3] == 0x00) event_ = r[4];
            }
        }
    }

    BYTE Feature(BYTE idx, USHORT id) {
        std::vector<BYTE> p;
        if (Call(idx, kFeatRoot, 0, { (BYTE)(id >> 8), (BYTE)id }, p) && !p.empty()) return p[0];
        return 0;
    }

    // ROOT getProtocolVersion: true for a HID++ 2.0 device on `idx`.
    bool Ping20(BYTE idx, int timeoutMs) {
        std::vector<BYTE> p;
        return Req(idx, kFeatRoot, 1, { 0, 0, 0x5A }, p, nullptr, timeoutMs) == Res::Ok && p.size() >= 3 && p[0] >= 2;
    }

private:
    HidDev s_, l_;
    bool hasS_ = false, hasL_ = false, longOnly_ = false;
    int budget_ = kBudgetMs;
    BYTE watchIdx_ = 0, watchFeat_ = 0;
    int event_ = -1;
};

// ---- device model --------------------------------------------------------------------------------

struct Mouse {
    Link* h = nullptr;
    BYTE idx = 0xFF;
    BYTE fDpi = 0, fRate = 0, fOnb = 0, fInfo = 0;
    bool extDpi = false, extRate = false;
    bool wired = false;
    USHORT usbTid = 0;
    std::string unitId;
};

void ResolveFeatures(Mouse& m) {
    Link& h = *m.h;
    if ((m.fDpi = h.Feature(m.idx, kFeatDpiExt))) m.extDpi = true;
    else m.fDpi = h.Feature(m.idx, kFeatDpi);
    if ((m.fRate = h.Feature(m.idx, kFeatRateExt))) m.extRate = true;
    else m.fRate = h.Feature(m.idx, kFeatRate);
    m.fOnb = h.Feature(m.idx, kFeatOnboard);
    m.fInfo = h.Feature(m.idx, kFeatInfo);
}

// readIdentity + isWiredHidppConnection: the transport whose id equals the USB product id in use
// is the active link; without a match a direct (0xFF, not Bluetooth) endpoint counts as wired.
void ReadIdentity(Mouse& m, USHORT pid, bool bluetooth) {
    m.wired = m.idx == 0xFF && !bluetooth;
    if (!m.fInfo) return;
    std::vector<BYTE> p;
    if (!m.h->Call(m.idx, m.fInfo, 0, {}, p) || p.size() < 13) return;
    char buf[16];
    snprintf(buf, sizeof buf, "%02X%02X%02X%02X", p[1], p[2], p[3], p[4]);
    m.unitId = buf == std::string("00000000") ? "" : buf;
    static const BYTE flags[] = { 0x01, 0x02, 0x04, 0x08 };  // Bluetooth, BLE, Wireless, USB
    size_t off = 7;
    for (BYTE f : flags) {
        if (!(p[6] & f) || off + 2 > 13) continue;
        USHORT tid = (USHORT)((p[off] << 8) | p[off + 1]);
        off += 2;
        if (f == 0x08) m.usbTid = tid;
        if (tid == pid) m.wired = f == 0x08;
    }
}

std::wstring ReadName(Mouse& m) {
    BYTE f = m.h->Feature(m.idx, kFeatName);
    if (!f) return L"";
    std::vector<BYTE> p;
    if (!m.h->Call(m.idx, f, 0, {}, p) || p.empty()) return L"";
    int len = p[0];
    std::string name;
    while ((int)name.size() < len) {
        if (!m.h->Call(m.idx, f, 1, { (BYTE)name.size() }, p) || p.empty()) break;
        size_t before = name.size();
        for (BYTE ch : p) {
            if ((int)name.size() >= len || !ch) break;
            name += (char)ch;
        }
        if (name.size() == before) break;
    }
    if (name.empty()) return L"";
    std::wstring w = Widen(name);
    return w.rfind(L"Logitech", 0) == 0 ? w : L"Logitech " + w;
}

// getDpiOptions (0x2202) / readLegacyDpiList (0x2201).
bool ReadDpiRuns(Mouse& m, std::vector<logi::DpiRun>& runs) {
    std::vector<BYTE> p, bytes;
    if (m.extDpi) {
        for (int page = 0; page < 32; ++page) {
            if (!m.h->Call(m.idx, m.fDpi, 2, { 0, 0, (BYTE)page }, p)) return false;
            if (p.size() <= 3) break;
            bytes.insert(bytes.end(), p.begin() + 3, p.end());
            bool end = false;
            for (size_t i = 0; i + 1 < bytes.size(); i += 2)
                if (!bytes[i] && !bytes[i + 1]) end = true;
            if (end) break;
        }
    } else {
        if (!m.h->Call(m.idx, m.fDpi, 1, { 0 }, p) || p.size() < 2) return false;
        bytes.assign(p.begin() + 1, p.end());
    }
    if (!logi::DecodeDpiList(bytes, runs)) {
        if (m.extDpi) return false;  // OpenMouse throws on a bad 0x2202 range, stops on 0x2201
    }
    return true;
}

struct DpiNow {
    int x = -1, y = -1, lod = 0;
};
bool ReadDpi(Mouse& m, DpiNow& d) {
    std::vector<BYTE> p;
    if (m.extDpi) {
        if (!m.h->Call(m.idx, m.fDpi, 5, {}, p) || p.size() < 10) return false;
        d.x = (p[1] << 8) | p[2];
        d.y = (p[5] << 8) | p[6];
        d.lod = p[9];
    } else {
        if (!m.h->Call(m.idx, m.fDpi, 2, { 0 }, p) || p.size() < 3) return false;
        d.x = d.y = (p[1] << 8) | p[2];
    }
    return d.x > 0;
}

BYTE RateConnection(const Mouse& m) { return m.wired ? 0x00 : 0x01; }

std::vector<int> ReadRates(Mouse& m) {
    std::vector<BYTE> p;
    if (!m.fRate) return {};
    if (!m.extRate) {
        if (!m.h->Call(m.idx, m.fRate, 0, {}, p) || p.empty()) return {};
        return logi::DecodeRateBitmap(p[0]);
    }
    BYTE conn = RateConnection(m);
    if (!m.h->Call(m.idx, m.fRate, 1, { conn }, p) || p.size() < 2) return {};
    std::vector<int> listed = logi::DecodeRateMask((USHORT)((p[0] << 8) | p[1]));
    if (conn == 0 || (!listed.empty() && listed.back() > 1000)) return listed;
    // Some firmware keeps the capability mask on function 0 (readSupportedPollingRates).
    BYTE code = 0;
    if (m.h->Req(m.idx, m.fRate, 0, { conn }, p, &code) == Res::Ok && p.size() >= 2) {
        std::vector<int> caps = logi::DecodeRateMask((USHORT)((p[0] << 8) | p[1]));
        if (!caps.empty() && caps.back() > 1000) return caps;
    }
    return listed;
}

int ReadRate(Mouse& m) {
    std::vector<BYTE> p;
    if (!m.fRate) return -1;
    if (!m.extRate) {
        if (!m.h->Call(m.idx, m.fRate, 1, {}, p) || p.empty() || !p[0]) return -1;
        return (2000 / p[0] + 1) / 2;
    }
    if (!m.h->Call(m.idx, m.fRate, 2, { RateConnection(m) }, p) || p.empty() || p[0] >= 7) return -1;
    return logi::kRateHz[p[0]];
}

struct Onboard {
    int format = 0, mode = 0, sectorSize = 255;
};
bool ReadOnboard(Mouse& m, Onboard& o) {
    std::vector<BYTE> p;
    if (!m.h->Call(m.idx, m.fOnb, 0, {}, p) || p.size() < 9) return false;
    o.format = p[1];
    int ss = (p[7] << 8) | p[8];
    o.sectorSize = ss > 0 && ss <= 1024 ? ss : 255;
    if (!m.h->Call(m.idx, m.fOnb, 2, {}, p) || p.empty()) return false;
    o.mode = p[0];  // 1 onboard, 2 host
    return true;
}

bool ReadSector(Mouse& m, int sector, int len, std::vector<BYTE>& out) {
    out.assign((size_t)len, 0);
    auto chunk = [&](int off) {
        std::vector<BYTE> p;
        if (!m.h->Call(m.idx, m.fOnb, 5, { (BYTE)(sector >> 8), (BYTE)sector, (BYTE)(off >> 8), (BYTE)off }, p) ||
            p.size() < 16)
            return false;
        std::copy(p.begin(), p.begin() + std::min(16, len - off), out.begin() + off);
        return true;
    };
    if (len < 16) return false;
    int off = 0;
    for (; off + 16 <= len; off += 16)
        if (!chunk(off)) return false;
    if (off < len && !chunk(len - 16)) return false;
    return true;
}

bool WriteSector(Mouse& m, int sector, const std::vector<BYTE>& bytes) {
    std::vector<BYTE> p;
    int n = (int)bytes.size();
    if (!m.h->Call(m.idx, m.fOnb, 6, { (BYTE)(sector >> 8), (BYTE)sector, 0, 0, (BYTE)(n >> 8), (BYTE)n }, p))
        return false;
    for (int off = 0; off < n; off += 16) {
        std::vector<BYTE> chunk(16, 0xFF);
        for (int i = 0; i < 16 && off + i < n; ++i) chunk[i] = bytes[off + i];
        if (!m.h->Call(m.idx, m.fOnb, 7, chunk, p)) return false;
    }
    return m.h->Call(m.idx, m.fOnb, 8, {}, p, 1500);
}

// ---- enumeration ---------------------------------------------------------------------------------

struct Group {
    const HidColl* s = nullptr;
    const HidColl* l = nullptr;
};

void ProbeGroup(USHORT pid, const Group& g, std::set<std::string>& units, std::vector<SettingsDevice>& out) {
    Link h;
    const bool longOnly = IsBolt(pid) || IsBluetoothPath((g.l ? g.l : g.s)->path);
    if (!h.Open(g.s, g.l, longOnly)) return;
    const bool bluetooth = IsBluetoothPath((g.l ? g.l : g.s)->path);
    std::wstring product = HidProduct(g.l ? *g.l : *g.s);
    const bool receiver = IsKnownReceiver(pid) || product.find(L"Receiver") != std::wstring::npos;

    std::vector<BYTE> indices;
    if (IsPassthroughDongle(pid)) indices = { 1 };
    else if (bluetooth) indices = { 0xFF };
    else if (receiver) indices = { 1, 2, 3, 4, 5, 6 };
    else indices = { 0xFF, 1 };  // hidppDeviceIndexCandidates for a non-receiver

    for (BYTE idx : indices) {
        if (h.dead()) break;
        if (!h.Ping20(idx, 400)) continue;
        Mouse m;
        m.h = &h;
        m.idx = idx;
        ResolveFeatures(m);
        if (!m.fDpi) continue;  // no sensor: keyboard, headset or the receiver itself
        ReadIdentity(m, pid, bluetooth);
        if (!m.unitId.empty() && !units.insert(m.unitId).second) continue;  // same mouse on another link

        SettingsDevice d;
        d.vid = kVid;
        d.pid = pid;
        d.path = (g.l ? g.l : g.s)->path;
        d.hidppIndex = idx;
        d.featDpi = m.fDpi;
        d.featRate = m.fRate;
        d.featOnboard = m.fOnb;
        d.name = ReadName(m);
        if (d.name.empty()) d.name = (idx == 0xFF && !product.empty()) ? product : L"Logitech mouse";

        Onboard o;
        bool haveOnb = m.fOnb && ReadOnboard(m, o);
        int format = haveOnb ? o.format : 0;
        d.variant = (m.extDpi ? kVarExtDpi : 0) | (m.extRate ? kVarExtRate : 0) | (m.wired ? kVarWired : 0) |
                    (longOnly ? kVarLongOnly : 0) | (m.usbTid == 0xC0A9 ? kVarFullRateUsb : 0) |
                    ((format & 0xFF) << kVarFormatShift);

        std::vector<logi::DpiRun> runs;
        if (ReadDpiRuns(m, runs)) {
            logi::FormatCaps fc = logi::CapsForFormat(format);
            if (runs.empty() && !m.extDpi && fc.ok) runs.push_back({ fc.minDpi, fc.maxDpi, fc.stepDpi });  // empty legacy list
            logi::DpiRangeOf(runs, d.caps.dpiMin, d.caps.dpiMax, d.caps.dpiStep);
        }
        DpiNow now;
        if (ReadDpi(m, now)) d.dpi = now.x;

        std::vector<int> rates = ReadRates(m);
        logi::FormatCaps rc = logi::CapsForDevice(format, m.usbTid);
        if (rc.ok) {
            int ceiling = m.wired ? rc.wiredMaxHz : rc.wirelessMaxHz;
            rates.erase(std::remove_if(rates.begin(), rates.end(), [ceiling](int r) { return r > ceiling; }), rates.end());
        }
        d.caps.pollingRates = rates;
        d.pollingHz = ReadRate(m);
        if (rc.ok && d.pollingHz > 0) {
            int ceiling = m.wired ? rc.wiredMaxHz : rc.wirelessMaxHz;
            d.pollingHz = std::min(d.pollingHz, ceiling);
        }
        d.verified = haveOnb && logi::FormatHardwareConfirmed(format);
        out.push_back(d);
        if (idx == 0xFF) break;  // a direct endpoint is one mouse
    }
}

// ---- writes --------------------------------------------------------------------------------------

// Reopens the collections of a device found by EnumLogitechSettings and checks it still answers.
struct Session {
    Link h;
    Mouse m;
    Onboard o;
    bool haveOnb = false;
};

bool OpenSession(const SettingsDevice& d, Session& s, std::wstring& err) {
    std::vector<HidColl> colls = EnumHid();
    std::wstring key;
    for (const HidColl& c : colls)
        if (c.path == d.path) key = PhysicalKey(c);
    if (key.empty()) {
        err = L"The mouse is no longer connected.";
        return false;
    }
    const HidColl *sc = nullptr, *lc = nullptr;
    for (const HidColl& c : colls) {
        if (c.vid != d.vid || c.pid != d.pid || PhysicalKey(c) != key) continue;
        if (!sc && IsShortColl(c)) sc = &c;
        if (!lc && IsLongColl(c)) lc = &c;
    }
    if (!s.h.Open(sc, lc, (d.variant & kVarLongOnly) != 0)) {
        err = L"Could not open the mouse's control interface.";
        return false;
    }
    s.m.h = &s.h;
    s.m.idx = (BYTE)d.hidppIndex;
    if (!s.h.Ping20(s.m.idx, 400)) {
        err = L"The mouse did not answer. Move it or click a button, then try again.";
        return false;
    }
    ResolveFeatures(s.m);
    s.m.wired = (d.variant & kVarWired) != 0;
    s.m.usbTid = (d.variant & kVarFullRateUsb) ? 0xC0A9 : 0;
    if (s.m.fOnb) {
        s.haveOnb = ReadOnboard(s.m, s.o);
        if (!s.haveOnb) {
            err = L"Could not read the mouse's onboard memory state.";
            return false;
        }
    }
    return true;
}

// openActiveProfile + the write/verify/restore part of writeActiveProfile, with `patch` changing
// only the requested field. Returns false with `err` set; the sector is left as it was.
template <class Patch>
bool WriteActiveProfile(Session& s, Patch patch, std::wstring& err) {
    Mouse& m = s.m;
    if (!logi::FormatWritable(s.o.format)) {
        err = L"This mouse runs from its onboard memory (profile format " + std::to_wstring(s.o.format) +
              L"), which cannot be written safely yet. Change it in G HUB.";
        return false;
    }
    std::vector<BYTE> p;
    if (!m.h->Call(m.idx, m.fOnb, 4, {}, p) || p.size() < 2) {
        err = L"Could not read the active onboard profile.";
        return false;
    }
    int sector = (p[0] << 8) | p[1];
    if (sector < 1 || sector > 0xFF) {  // 0 is the directory; 0x01xx are read-only factory (ROM) profiles
        err = L"The mouse runs a read-only factory profile. Enable a writable profile in G HUB first.";
        return false;
    }
    std::vector<BYTE> before;
    if (!ReadSector(m, sector, s.o.sectorSize, before)) {
        err = L"Could not read the onboard profile.";
        return false;
    }
    if (logi::ProfileCrc(before) != logi::StoredCrc(before)) {
        err = L"The onboard profile failed its checksum; nothing was written.";
        return false;
    }
    std::vector<BYTE> after = before;
    std::string why;
    if (!patch(after, why)) {
        err = Widen(why);
        return false;
    }
    if (after == before) return true;  // never spend a flash cycle on a no-op
    std::vector<BYTE> check;
    if (WriteSector(m, sector, after) && ReadSector(m, sector, s.o.sectorSize, check) && check == after) return true;
    WriteSector(m, sector, before);  // best effort restore
    err = L"The mouse did not store the onboard profile as written; the previous profile was restored.";
    return false;
}

bool LiveSetDpi(Mouse& m, int dpi) {
    std::vector<BYTE> p;
    BYTE code = 0;
    if (m.extDpi) {
        DpiNow cur;
        ReadDpi(m, cur);
        bool separate = false, liftOff = false;
        if (m.h->Call(m.idx, m.fDpi, 1, { 0 }, p) && p.size() >= 3) {
            separate = p[2] & 0x01;
            liftOff = (p[2] & 0x02) != 0;
        }
        int y = separate ? dpi : 0;
        BYTE lod = liftOff && cur.lod > 0 ? (BYTE)cur.lod : 0;
        if (m.h->Req(m.idx, m.fDpi, 6, { 0, (BYTE)(dpi >> 8), (BYTE)dpi, (BYTE)(y >> 8), (BYTE)y, lod }, p, &code) != Res::Ok)
            return false;
    } else {
        // setLegacyDpi: short report on a direct endpoint, long through a receiver.
        if (m.h->Req(m.idx, m.fDpi, 3, { 0, (BYTE)(dpi >> 8), (BYTE)dpi }, p, &code, kReqMs, m.idx == 0xFF) != Res::Ok)
            return false;
    }
    DpiNow now;
    return ReadDpi(m, now) && now.x == dpi;
}

bool LiveSetRate(Mouse& m, int hz) {
    std::vector<BYTE> p;
    BYTE code = 0;
    if (m.extRate) {
        int rc = logi::RateCode(hz);
        if (rc < 0) return false;
        m.h->Watch(m.idx, m.fRate);
        if (m.h->Req(m.idx, m.fRate, 3, { (BYTE)rc }, p, &code) != Res::Ok) return false;
        m.h->WaitEvent(m.idx, rc, 1000);  // the mouse confirms the active rate with an event
        if (m.h->event() == rc) return true;
        return ReadRate(m) == hz;
    }
    int ms = (2000 / hz + 1) / 2;
    if (ms < 1 || ms > 8) return false;
    if (m.h->Req(m.idx, m.fRate, 2, { (BYTE)ms }, p, &code) != Res::Ok) return false;
    return ReadRate(m) == hz;
}

const wchar_t* kHostNote = L"The mouse is in host mode (G HUB): the new value is not stored on the mouse.";
const wchar_t* kLaterNote = L"Saved in the mouse's onboard memory; it applies the next time the mouse powers on.";

}  // namespace

void EnumLogitechSettings(const std::vector<HidColl>& colls, std::vector<SettingsDevice>& out) {
    std::set<USHORT> pids;
    for (const HidColl& c : colls)
        if (c.vid == kVid && (IsShortColl(c) || IsLongColl(c))) pids.insert(c.pid);
    std::set<std::string> units;
    for (USHORT pid : pids) {
        std::vector<std::wstring> keys;
        std::vector<Group> groups;
        for (const HidColl& c : colls) {
            if (c.vid != kVid || c.pid != pid) continue;
            bool sh = IsShortColl(c), lg = IsLongColl(c);
            if (!sh && !lg) continue;
            std::wstring k = PhysicalKey(c);
            size_t i = std::find(keys.begin(), keys.end(), k) - keys.begin();
            if (i == keys.size()) {
                keys.push_back(k);
                groups.push_back({});
            }
            if (sh && !groups[i].s) groups[i].s = &c;
            if (lg && !groups[i].l) groups[i].l = &c;
        }
        for (const Group& g : groups) ProbeGroup(pid, g, units, out);
    }
}

bool LogitechSetDpi(SettingsDevice& d, int dpi, std::wstring& err) {
    if (d.caps.dpiMax <= 0 || dpi < d.caps.dpiMin || dpi > d.caps.dpiMax) {
        err = L"DPI out of range.";
        return false;
    }
    Session s;
    if (!OpenSession(d, s, err)) return false;
    Mouse& m = s.m;
    if (!m.fDpi) {
        err = L"This mouse does not expose DPI controls.";
        return false;
    }
    std::vector<logi::DpiRun> runs;
    if (!ReadDpiRuns(m, runs)) {
        err = L"Could not read the DPI values the mouse supports.";
        return false;
    }
    if (runs.empty() && !m.extDpi) {
        logi::FormatCaps fc = logi::CapsForFormat(s.haveOnb ? s.o.format : 0);
        if (fc.ok) runs.push_back({ fc.minDpi, fc.maxDpi, fc.stepDpi });
    }
    if (!logi::DpiListed(runs, dpi)) {
        int closest = logi::NearestListedDpi(runs, dpi);
        err = std::to_wstring(dpi) + L" DPI is not offered by this mouse" +
              (closest ? L" (nearest: " + std::to_wstring(closest) + L")." : L".");
        return false;
    }

    if (s.haveOnb && s.o.mode == 1) {
        const int format = s.o.format;
        auto patch = [format, dpi](std::vector<BYTE>& sector, std::string& why) {
            logi::DpiTable t;
            if (!logi::DecodeDpiTable(sector, format, t)) {
                why = "The onboard profile has no DPI table.";
                return false;
            }
            int slot = t.defaultIndex;  // the slot the mouse loads at power-on
            if (slot < 0 || slot >= (int)t.slots.size() || !t.slots[slot].x) {
                why = "The onboard profile has no default DPI slot.";
                return false;
            }
            return logi::EncodeSlotDpi(sector, format, slot, dpi, why);
        };
        if (!WriteActiveProfile(s, patch, err)) return false;
        d.dpi = dpi;
        if (!LiveSetDpi(m, dpi)) err = kLaterNote;
        return true;
    }
    if (s.haveOnb && s.o.mode != 2) {
        err = L"The mouse reported an unknown onboard memory mode; nothing was written.";
        return false;
    }
    if (!LiveSetDpi(m, dpi)) {
        DpiNow now;
        err = ReadDpi(m, now) ? L"The mouse kept " + std::to_wstring(now.x) + L" DPI." : L"The mouse rejected the DPI change.";
        return false;
    }
    d.dpi = dpi;
    if (s.haveOnb) err = kHostNote;
    return true;
}

bool LogitechSetPolling(SettingsDevice& d, int hz, std::wstring& err) {
    if (std::find(d.caps.pollingRates.begin(), d.caps.pollingRates.end(), hz) == d.caps.pollingRates.end()) {
        err = L"Polling rate not supported.";
        return false;
    }
    Session s;
    if (!OpenSession(d, s, err)) return false;
    Mouse& m = s.m;
    if (!m.fRate) {
        err = L"This mouse does not expose report-rate controls.";
        return false;
    }
    std::vector<int> live = ReadRates(m);
    if (std::find(live.begin(), live.end(), hz) == live.end()) {
        err = L"The mouse does not offer " + std::to_wstring(hz) + L" Hz on this connection.";
        return false;
    }

    if (s.haveOnb && s.o.mode == 1) {
        const int format = s.o.format;
        const bool wired = m.wired;
        const logi::FormatCaps caps = logi::CapsForDevice(format, m.usbTid);
        auto patch = [format, wired, hz, caps](std::vector<BYTE>& sector, std::string& why) {
            return logi::EncodeProfileRate(sector, format, wired, hz, caps, why);
        };
        if (!WriteActiveProfile(s, patch, err)) return false;
        d.pollingHz = hz;
        if (!LiveSetRate(m, hz)) err = kLaterNote;
        return true;
    }
    if (s.haveOnb && s.o.mode != 2) {
        err = L"The mouse reported an unknown onboard memory mode; nothing was written.";
        return false;
    }
    if (!LiveSetRate(m, hz)) {
        int now = ReadRate(m);
        err = now > 0 ? L"The mouse kept " + std::to_wstring(now) + L" Hz." : L"The mouse rejected the polling rate change.";
        return false;
    }
    d.pollingHz = hz;
    if (s.haveOnb) err = kHostNote;
    return true;
}
