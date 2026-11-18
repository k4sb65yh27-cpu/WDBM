// Razer mouse DPI and polling-rate control (native port).
//
// Protocol and per-model data follow OpenMouse's @openmouse/protocol (AGPL-3.0-or-later):
//   dist/razer/codec.js   90-byte report layout, razerSetDpiCommand (class 04 id 05, args
//                         [store, xHi, xLo, yHi, yLo, 0, 0]), RAZER_READ.dpi (04/85, args [store]),
//                         legacy polling 00/05 + 00/85 (divisor of 1000), extended polling 00/40 + 00/C0
//                         (args [selector, divisor of 8000]), RAZER_STORAGE = 0x01, status codes.
//   dist/razer/devices.js model table (see razer_models.h).
//   dist/drivers/razer/hid.js  request/response sequencing (re-send on a lost exchange, keep reading on
//                         "busy", 150 ms link settle after a polling change, read back after each write).
// and OpenRazer driver/razermouse_driver.c (GPL-2.0) razer_attr_write_poll_rate (selector 0x00 then
// selector 0x01 for models that persist the extended rate). The report framing, CRC and the status
// handling are the same as the battery code in drivers/razer.cpp.
//
// Nothing here runs unless the menu asks: no timers, no background polling.
#include "../settings.h"

#include <cstring>

#include "razer_models.h"

namespace {

constexpr USHORT kVid = 0x1532;
constexpr int kDpiMin = 100;   // DPI_MIN in drivers/razer/hid.js
constexpr int kDpiStep = 50;   // DPI_STEP in drivers/razer/hid.js (UI grid)

enum class Rz { Ok, Offline, Unsupported, NoReply };

const RazerModel* FindModel(USHORT pid) {
    for (int i = 0; i < kRazerModelCount; i++)
        if (kRazerModels[i].pid == pid) return &kRazerModels[i];
    return nullptr;
}

// Per timing group wait between the request and the reply (ms). The existing battery code uses 15 ms
// (then 40) on all models; the atheris receiver "needs a longer response window" (devices.js).
int GroupDelay(RzGroup g) {
    switch (g) {
        case RzGroup::AtherisReceiver: return 60;
        case RzGroup::NewReceiver:
        case RzGroup::ViperReceiver: return 30;
        default: return 15;
    }
}

// One request/reply exchange on the 90-byte feature report. `sends` bounds how often the request is
// sent again when the reply was lost (Synapse shares the mailbox) or the receiver timed out; "busy"
// only keeps reading. Worst case is a bit over a second, so a sleeping mouse never blocks for long.
Rz Exchange(HidDev& d, int delayMs, BYTE tid, BYTE cls, BYTE id, BYTE size, const std::vector<BYTE>& in,
            std::vector<BYTE>& out, int sends) {
    std::vector<BYTE> rep(91, 0);  // [0] = report id 0, then the 90-byte report
    BYTE* r = rep.data() + 1;
    r[1] = tid;
    r[5] = size;
    r[6] = cls;
    r[7] = id;
    for (size_t i = 0; i < in.size() && i < 80; i++) r[8 + i] = in[i];
    BYTE crc = 0;
    for (int i = 2; i < 88; i++) crc ^= r[i];
    r[88] = crc;
    std::vector<BYTE> resp;
    Rz res = Rz::NoReply;
    for (int s = 0; s < sends; s++) {
        if (!d.SetFeature(rep)) return Rz::NoReply;
        for (int rd = 0; rd < 6; rd++) {
            Sleep(delayMs);
            if (!d.GetFeature(0, resp) || resp.size() < 91) break;
            const BYTE* a = resp.data() + 1;
            BYTE replyCrc = 0;
            for (int i = 2; i < 88; i++) replyCrc ^= a[i];
            if (a[1] != tid || a[2] || a[3] || a[4] || a[6] != cls || a[7] != id || a[88] != replyCrc)
                break;  // somebody else's reply: ask again
            if (a[0] == 0x01) continue;  // busy: the same request is still being processed
            if (a[0] == 0x02) { out.assign(a + 8, a + 88); return Rz::Ok; }
            if (a[0] == 0x04) { res = Rz::Offline; break; }
            if (a[0] == 0x03 || a[0] == 0x05) return Rz::Unsupported;
            break;
        }
    }
    return res;
}

struct Link {
    HidDev dev;
    const RazerModel* m = nullptr;
    int delayMs = 15;
    BYTE tid = 0;
    bool ext = false;  // extended polling command in use

    Rz Cmd(BYTE cls, BYTE id, BYTE size, const std::vector<BYTE>& in, std::vector<BYTE>& out, int sends) {
        return Exchange(dev, delayMs, tid, cls, id, size, in, out, sends);
    }
    Rz ReadDpi(int& dpi, int sends) {
        std::vector<BYTE> a;
        Rz r = Cmd(0x04, 0x85, 0x07, { m->dpiStore }, a, sends);
        if (r == Rz::Ok) dpi = (a[1] << 8) | a[2];  // X; [3..4] is Y
        return r;
    }
    // Reads the polling rate with the command the model uses, then the other one (a wired model may
    // answer only the legacy command, a receiver only the extended one: see readPollingRateHz()).
    Rz ReadPolling(int& hz, int sends) {
        std::vector<BYTE> a;
        Rz first = Rz::NoReply;
        for (int pass = 0; pass < 2; pass++) {
            bool e = pass == 0 ? ext : !ext;
            Rz r = e ? Cmd(0x00, 0xC0, 0x02, { 0x00 }, a, sends) : Cmd(0x00, 0x85, 0x01, {}, a, sends);
            if (pass == 0) first = r;
            if (r == Rz::Ok) {
                int div = e ? a[1] : a[0];
                if (!div) continue;
                hz = (e ? 8000 : 1000) / div;
                ext = e;
                return Rz::Ok;
            }
            if (r == Rz::Offline) return r;  // asleep: do not wait for the second command too
        }
        return first == Rz::Unsupported ? Rz::NoReply : first;
    }
};

bool OpenLink(Link& l, const SettingsDevice& d) {
    int idx = d.variant & 0xFFFF;
    if (idx < 0 || idx >= kRazerModelCount || d.vid != kVid || kRazerModels[idx].pid != d.pid) return false;
    l.m = &kRazerModels[idx];
    l.delayMs = GroupDelay(l.m->group);
    l.tid = d.tid;
    l.ext = (d.variant & 0x10000) != 0;
    HidColl c;
    c.path = d.path;
    c.vid = d.vid;
    c.pid = d.pid;
    c.caps.FeatureReportByteLength = 91;
    return l.dev.OpenFeatureOnly(c);
}

const wchar_t* Why(Rz r) {
    switch (r) {
        case Rz::Offline: return L"The mouse is asleep or switched off.";
        case Rz::Unsupported: return L"The mouse rejected the command.";
        default: return L"The mouse did not answer (is Razer Synapse running?).";
    }
}

std::wstring BaseName(std::wstring n) {
    const wchar_t* suffix = L" (Wired)";
    size_t l = wcslen(suffix);
    if (n.size() > l && n.compare(n.size() - l, l, suffix) == 0) n.resize(n.size() - l);
    return n;
}

}  // namespace

void EnumRazerSettings(const std::vector<HidColl>& colls, std::vector<SettingsDevice>& out) {
    std::vector<USHORT> done;
    std::vector<SettingsDevice> found;
    for (const HidColl& c : colls) {
        if (c.vid != kVid || c.featLen() != 91) continue;
        const RazerModel* m = FindModel(c.pid);  // unknown products are never touched
        if (!m) continue;
        bool seen = false;
        for (USHORT p : done) seen |= p == c.pid;
        if (seen) continue;

        Link l;
        l.m = m;
        l.delayMs = GroupDelay(m->group);
        l.tid = m->tid;
        l.ext = m->extended;
        if (!l.dev.OpenFeatureOnly(c)) continue;
        int dpi = -1, hz = -1;
        Rz r = l.ReadDpi(dpi, 2);  // a wrong collection of the same product never answers: next one
        if (r != Rz::Ok && r != Rz::Offline) continue;
        done.push_back(c.pid);
        if (r == Rz::Ok) {
            if (l.ReadPolling(hz, 2) != Rz::Ok) hz = -1;
        }

        SettingsDevice d;
        d.name = m->name;
        d.vid = c.vid;
        d.pid = c.pid;
        d.path = c.path;
        d.tid = m->tid;
        d.verified = m->verified;
        d.dpi = dpi;
        d.pollingHz = hz;
        d.caps.dpiMin = kDpiMin;
        d.caps.dpiMax = m->maxDpi;
        d.caps.dpiStep = kDpiStep;
        for (int rate : m->rates)
            if (rate > 0) d.caps.pollingRates.push_back(rate);
        // The mouse told us which polling command it answers: follow it for rates and writes.
        if (hz > 0 && (l.ext != m->extended || m->probeRates)) {
            if (l.ext) {
                if (m->probeRates) d.caps.pollingRates = { 125, 500, 1000, 2000, 4000, 8000 };
            } else {
                std::vector<int> keep;
                for (int rate : d.caps.pollingRates)
                    if (rate <= 1000) keep.push_back(rate);
                d.caps.pollingRates = keep;
            }
        }
        d.variant = (int)(m - kRazerModels) | (l.ext ? 0x10000 : 0);
        found.push_back(std::move(d));
    }
    // A wireless mouse plugged in for charging shows up twice (cable + receiver). Drop an entry that
    // could not be read when the other one of the same mouse answered.
    for (size_t i = 0; i < found.size(); i++) {
        bool drop = false;
        if (found[i].dpi < 0)
            for (size_t j = 0; j < found.size(); j++)
                drop |= j != i && found[j].dpi >= 0 && BaseName(found[j].name) == BaseName(found[i].name);
        if (!drop) out.push_back(found[i]);
    }
}

bool RazerSetDpi(SettingsDevice& d, int dpi, std::wstring& err) {
    if (d.caps.dpiMax <= 0 || dpi < d.caps.dpiMin || dpi > d.caps.dpiMax ||
        (d.caps.dpiStep > 0 && (dpi - d.caps.dpiMin) % d.caps.dpiStep != 0)) {
        err = L"This DPI value is outside the range of the mouse.";
        return false;
    }
    Link l;
    if (!OpenLink(l, d)) { err = L"Cannot open the mouse (not a known Razer model, or in use).";  return false; }
    // Persistent store (RAZER_STORAGE = 0x01) so the value survives a power cycle.
    std::vector<BYTE> args = { l.m->dpiStore, (BYTE)(dpi >> 8), (BYTE)dpi, (BYTE)(dpi >> 8), (BYTE)dpi, 0, 0 };
    std::vector<BYTE> reply;
    Rz r = l.Cmd(0x04, 0x05, 0x07, args, reply, 2);
    if (r != Rz::Ok) { err = Why(r); return false; }
    int now = -1;
    r = l.ReadDpi(now, 3);
    if (r != Rz::Ok) { err = L"DPI was sent but could not be confirmed."; return false; }
    d.dpi = now;
    if (now != dpi) {
        err = L"The mouse kept " + std::to_wstring(now) + L" DPI instead of " + std::to_wstring(dpi) + L".";
        return false;
    }
    return true;
}

bool RazerSetPolling(SettingsDevice& d, int hz, std::wstring& err) {
    bool allowed = false;
    for (int r : d.caps.pollingRates) allowed |= r == hz;
    if (!allowed) { err = L"This polling rate is not supported by the mouse."; return false; }
    Link l;
    if (!OpenLink(l, d)) { err = L"Cannot open the mouse (not a known Razer model, or in use).";  return false; }
    int ceiling = l.ext ? 8000 : 1000;
    if (hz <= 0 || ceiling % hz != 0) { err = L"This polling rate cannot be encoded."; return false; }
    BYTE div = (BYTE)(ceiling / hz);
    std::vector<BYTE> reply;
    Rz r = l.ext ? l.Cmd(0x00, 0x40, 0x02, { 0x00, div }, reply, 2)  // selector 0x00: live rate
                 : l.Cmd(0x00, 0x05, 0x01, { div }, reply, 2);
    if (r != Rz::Ok) { err = Why(r); return false; }
    if (l.ext && l.m->commitTid) {
        // Selector 0x01 stores the rate (devices.js extendedPollingCommitTransactionId; OpenRazer).
        Exchange(l.dev, l.delayMs, l.m->commitTid, 0x00, 0x40, 0x02, { 0x01, div }, reply, 2);
    }
    // The wireless link reconfigures for a moment (POLLING_LINK_SETTLE_MS in drivers/razer/hid.js).
    if (l.m->wireless) Sleep(150);
    int now = -1;
    r = l.ReadPolling(now, 3);
    if (r != Rz::Ok) { err = L"Polling rate was sent but could not be confirmed."; return false; }
    d.pollingHz = now;
    if (now != hz) {
        err = L"The mouse kept " + std::to_wstring(now) + L" Hz instead of " + std::to_wstring(hz) + L" Hz.";
        return false;
    }
    return true;
}
