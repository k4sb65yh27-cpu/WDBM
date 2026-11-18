// Logitech onboard profile and HID++ reply decoding without I/O. See logitech_onboard.h for the
// OpenMouse (AGPL-3.0-or-later) files each function follows.
#include "logitech_onboard.h"

#include <algorithm>
#include <cstdlib>

namespace logi {

const int kRateHz[7] = { 125, 250, 500, 1000, 2000, 4000, 8000 };

namespace {

constexpr int kSlots = 5;  // DPI_STAGE_SLOTS

int ReadU16LE(const std::vector<uint8_t>& b, size_t off) {
    if (off + 1 >= b.size()) return -1;
    int v = b[off] | (b[off + 1] << 8);
    return v == 0xFFFF ? -1 : v;  // erased flash
}

void WriteU16LE(std::vector<uint8_t>& b, size_t off, int v) {
    b[off] = (uint8_t)(v & 0xFF);
    b[off + 1] = (uint8_t)((v >> 8) & 0xFF);
}

int Gcd(int a, int b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

}  // namespace

uint16_t ProfileCrc(const std::vector<uint8_t>& s) {
    unsigned crc = 0xFFFF;
    for (size_t i = 0; i + 2 < s.size(); ++i) {
        crc ^= (unsigned)s[i] << 8;
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
    }
    return (uint16_t)crc;
}

uint16_t StoredCrc(const std::vector<uint8_t>& s) {
    if (s.size() < 2) return 0;
    return (uint16_t)((s[s.size() - 2] << 8) | s[s.size() - 1]);
}

void ApplyCrc(std::vector<uint8_t>& s) {
    if (s.size() < 2) return;
    uint16_t crc = ProfileCrc(s);
    s[s.size() - 2] = (uint8_t)(crc >> 8);
    s[s.size() - 1] = (uint8_t)crc;
}

bool FormatWritable(int f) { return f == 2 || f == 3 || f == 4 || f == 7 || f == 8; }
bool FormatHardwareConfirmed(int f) { return f == 3 || f == 4 || f == 7 || f == 8; }

FormatCaps CapsForFormat(int f) {
    FormatCaps c;
    switch (f) {
        case 1: c = { true, 5, 252, 4032, 84, 1000, 1000 }; break;
        case 2: c = { true, 5, 100, 25600, 50, 1000, 1000 }; break;
        case 3: c = { true, 5, 50, 12000, 50, 1000, 1000 }; break;
        case 4: c = { true, 5, 50, 8000, 50, 1000, 1000 }; break;
        case 7: c = { true, 5, 100, 32000, 50, 8000, 1000 }; break;
        case 8: c = { true, 5, 100, 48000, 50, 8000, 1000 }; break;
        default: break;
    }
    return c;
}

FormatCaps CapsForDevice(int f, uint16_t usbTransportId) {
    FormatCaps c = CapsForFormat(f);
    if (c.ok && usbTransportId == 0xC0A9) c.wiredMaxHz = c.wirelessMaxHz;  // FULL_RATE_USB_TRANSPORT_IDS
    return c;
}

Layout LayoutForFormat(int f) {
    Layout l;
    l.v6 = f >= 6;
    if (l.v6) {
        l.rateWireless = 0x00;
        l.rateWired = 0x01;
        l.dpi = 0x02;
    } else {
        l.rateWireless = l.rateWired = 0x00;  // one shared interval byte
        l.dpi = 0x01;
    }
    return l;
}

bool DecodeDpiTable(const std::vector<uint8_t>& s, int format, DpiTable& out) {
    Layout l = LayoutForFormat(format);
    size_t need = (size_t)l.dpi + 2 + (l.v6 ? kSlots * 5 : kSlots * 2);
    if (s.size() < need + 2) return false;
    out.defaultIndex = s[l.dpi] == 0xFF ? -1 : s[l.dpi];
    out.slots.assign(kSlots, DpiStage{});
    for (int i = 0; i < kSlots; ++i) {
        DpiStage& st = out.slots[i];
        if (l.v6) {
            size_t base = (size_t)l.dpi + 2 + i * 5;
            int x = ReadU16LE(s, base), y = ReadU16LE(s, base + 2);
            if (x <= 0 || y < 0) continue;
            st = { x, y, s[base + 4] };
        } else {
            int v = ReadU16LE(s, (size_t)l.dpi + 2 + i * 2);
            if (v <= 0) continue;
            st = { v, v, 0 };
        }
    }
    return true;
}

bool EncodeSlotDpi(std::vector<uint8_t>& s, int format, int slot, int dpi, std::string& err) {
    DpiTable t;
    if (!DecodeDpiTable(s, format, t)) {
        err = "The mouse's onboard profile has an unexpected layout, so nothing was changed.";
        return false;
    }
    if (slot < 0 || slot >= kSlots || t.slots[slot].x == 0) {
        err = "The mouse's onboard profile has an unexpected layout, so nothing was changed.";
        return false;
    }
    if (dpi <= 0 || dpi > 0xFFFE) {
        err = "This value is outside the range of this mouse.";
        return false;
    }
    Layout l = LayoutForFormat(format);
    if (l.v6) {
        // validateDpiStagePlan: a used v6 stage carries a lift-off level 1..3 (0 = unused).
        if (t.slots[slot].lod < 1 || t.slots[slot].lod > 3) {
            err = "The mouse's onboard profile has an unexpected layout, so nothing was changed.";
            return false;
        }
        size_t base = (size_t)l.dpi + 2 + slot * 5;
        WriteU16LE(s, base, dpi);
        WriteU16LE(s, base + 2, dpi);
    } else {
        WriteU16LE(s, (size_t)l.dpi + 2 + slot * 2, dpi);
    }
    ApplyCrc(s);
    return true;
}

int RateCode(int hz) {
    for (int i = 0; i < 7; ++i)
        if (kRateHz[i] == hz) return i;
    return -1;
}

int DecodeProfileRate(const std::vector<uint8_t>& s, int format, bool wired) {
    Layout l = LayoutForFormat(format);
    size_t off = (size_t)(wired ? l.rateWired : l.rateWireless);
    if (off >= s.size()) return 0;
    int raw = s[off];
    if (raw == 0xFF) return 0;
    if (l.v6) return raw < 7 ? kRateHz[raw] : 0;
    if (raw == 0 || 1000 % raw) return 0;
    return 1000 / raw;
}

bool EncodeProfileRate(std::vector<uint8_t>& s, int format, bool wired, int hz, const FormatCaps& caps,
                       std::string& err) {
    int ceiling = wired ? caps.wiredMaxHz : caps.wirelessMaxHz;
    if (!caps.ok || ceiling <= 0) {
        err = "This mouse's onboard memory isn't supported yet, so nothing was changed. Change this setting in G HUB.";
        return false;
    }
    int code = RateCode(hz);
    if (code < 0 || hz > ceiling) {
        err = "This connection stores polling rates up to {hz} Hz.";
        return false;
    }
    Layout l = LayoutForFormat(format);
    size_t off = (size_t)(wired ? l.rateWired : l.rateWireless);
    if (off + 2 >= s.size()) {
        err = "The mouse's onboard profile has an unexpected layout, so nothing was changed.";
        return false;
    }
    if (l.v6) {
        s[off] = (uint8_t)code;
    } else {
        if (1000 % hz) {
            err = "This polling rate can't be saved in the mouse's onboard memory.";
            return false;
        }
        s[off] = (uint8_t)(1000 / hz);
    }
    ApplyCrc(s);
    return true;
}

bool DecodeDpiList(const std::vector<uint8_t>& b, std::vector<DpiRun>& runs) {
    runs.clear();
    for (size_t i = 0; i + 1 < b.size();) {
        int v = (b[i] << 8) | b[i + 1];
        if (v == 0) break;
        if ((v >> 13) == 0x7) {
            int step = v & 0x1FFF;
            int last = i + 3 < b.size() ? ((b[i + 2] << 8) | b[i + 3]) : 0;
            if (runs.empty() || !step || !last || last <= runs.back().hi) return false;
            DpiRun r;
            r.lo = runs.back().hi + step;
            if (r.lo <= last) {
                r.step = step;
                r.hi = r.lo + (last - r.lo) / step * step;
                runs.push_back(r);
            }
            i += 4;
        } else {
            runs.push_back({ v, v, 0 });
            i += 2;
        }
    }
    return true;
}

bool DpiListed(const std::vector<DpiRun>& runs, int dpi) {
    for (const DpiRun& r : runs) {
        if (dpi < r.lo || dpi > r.hi) continue;
        if (r.step == 0 ? dpi == r.lo : (dpi - r.lo) % r.step == 0) return true;
    }
    return false;
}

bool DpiRangeOf(const std::vector<DpiRun>& runs, int& lo, int& hi, int& step) {
    if (runs.empty()) return false;
    lo = runs[0].lo;
    hi = runs[0].hi;
    int g = 0;
    for (const DpiRun& r : runs) {
        lo = std::min(lo, r.lo);
        hi = std::max(hi, r.hi);
        g = Gcd(g, r.lo);
        if (r.step) g = Gcd(g, r.step);
    }
    step = g ? g : 1;
    return true;
}

int NearestListedDpi(const std::vector<DpiRun>& runs, int dpi) {
    int best = 0;
    auto consider = [&](int v) {
        if (!best || std::abs(v - dpi) < std::abs(best - dpi)) best = v;
    };
    for (const DpiRun& r : runs) {
        if (r.step == 0 || dpi <= r.lo) {
            consider(r.lo);
        } else if (dpi >= r.hi) {
            consider(r.hi);
        } else {
            int below = r.lo + (dpi - r.lo) / r.step * r.step;
            consider(below);
            if (below + r.step <= r.hi) consider(below + r.step);
        }
    }
    return best;
}

std::vector<int> DecodeRateMask(uint16_t mask) {
    std::vector<int> out;
    for (int i = 0; i < 7; ++i)
        if (mask & (1u << i)) out.push_back(kRateHz[i]);
    return out;
}

std::vector<int> DecodeRateBitmap(uint8_t bits) {
    std::vector<int> out;
    for (int bit = 0; bit < 8; ++bit)
        if (bits & (1u << bit)) {
            // Math.round(1000 / (bit + 1)): 3, 5, 6, 7 ms give non-round rates; keep them as is.
            int hz = (2000 / (bit + 1) + 1) / 2;
            out.push_back(hz);
        }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace logi
