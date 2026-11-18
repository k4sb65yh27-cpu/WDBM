// Host-side checks for src/settings/logitech_onboard.cpp against the factory profile images OpenMouse
// ships (dist/drivers/logitech/onboard-profiles.js FACTORY_PROFILE_FORMAT_7 / _8, CRC 0x84db / 0x2a38).
// Build: g++ -std=c++17 -Wall -Wextra tests/logitech_onboard_test.cpp src/settings/logitech_onboard.cpp
#include "../src/settings/logitech_onboard.h"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <vector>

static std::vector<uint8_t> Hex(const char* s) {
    std::vector<uint8_t> out;
    std::istringstream in(s);
    std::string tok;
    while (in >> tok) out.push_back((uint8_t)std::stoul(tok, nullptr, 16));
    return out;
}

static const char* kFormat8 = R"(
  03 03 00 00 20 03 20 03 02 b0 04 b0 04 02 40 06
  40 06 02 60 09 60 09 02 80 0c 80 0c 02 00 00 00
  00 ff 00 ff ff ff 14 08 0c 14 08 0c 3c 00 2c 01
  80 01 00 01 80 01 00 02 80 01 00 04 80 01 00 08
  80 01 00 10 ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  03 00 00 00 00 00 1f 40 00 00 00 03 00 00 00 00
  00 1f 40 00 00 00 03 00 00 00 00 00 1f 40 32 00
  00 03 00 00 00 00 00 1f 40 32 00 00 03 2a 38)";

static const char* kFormat7 = R"(
  03 03 00 00 20 03 20 03 02 b0 04 b0 04 02 40 06
  40 06 02 60 09 60 09 02 80 0c 80 0c 02 00 00 00
  00 ff 00 ff ff ff ff ff ff ff ff ff 3c 00 2c 01
  80 01 00 01 80 01 00 02 80 01 00 04 80 01 00 08
  80 01 00 10 ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
  03 00 00 00 00 00 1f 40 00 00 00 03 00 00 00 00
  00 1f 40 00 00 00 03 00 00 00 00 00 1f 40 32 00
  00 03 00 00 00 00 00 1f 40 32 00 00 03 84 db)";

int main() {
    using namespace logi;

    // CRC-16/CCITT-FALSE check value ("123456789" -> 0x29B1), with two trailing CRC bytes.
    std::vector<uint8_t> check = { '1', '2', '3', '4', '5', '6', '7', '8', '9', 0, 0 };
    assert(ProfileCrc(check) == 0x29B1);

    for (int format : { 8, 7 }) {
        std::vector<uint8_t> f = Hex(format == 8 ? kFormat8 : kFormat7);
        assert(f.size() == 255);
        assert(ProfileCrc(f) == StoredCrc(f));
        assert(StoredCrc(f) == (format == 8 ? 0x2a38 : 0x84db));

        DpiTable t;
        assert(DecodeDpiTable(f, format, t));
        assert(t.defaultIndex == 0);
        const int expected[] = { 800, 1200, 1600, 2400, 3200 };
        for (int i = 0; i < 5; ++i) assert(t.slots[i].x == expected[i] && t.slots[i].y == expected[i] && t.slots[i].lod == 2);
        assert(DecodeProfileRate(f, format, false) == 1000 && DecodeProfileRate(f, format, true) == 1000);

        // Default slot to 1600: only bytes 4..7 and the CRC change, lift-off untouched.
        std::vector<uint8_t> g = f;
        std::string err;
        assert(EncodeSlotDpi(g, format, t.defaultIndex, 1600, err));
        for (size_t i = 0; i < g.size(); ++i)
            if (g[i] != f[i]) assert((i >= 4 && i <= 7) || i >= 253);
        assert(g[4] == 0x40 && g[5] == 0x06 && g[6] == 0x40 && g[7] == 0x06 && g[8] == 0x02);
        assert(ProfileCrc(g) == StoredCrc(g));
        DpiTable t2;
        assert(DecodeDpiTable(g, format, t2) && t2.slots[0].x == 1600 && t2.slots[1].x == 1200);
        // Writing the original value back restores the factory image byte for byte.
        assert(EncodeSlotDpi(g, format, 0, 800, err) && g == f);

        // Report rate: wireless byte 0, wired byte 1, rate codes; 8 kHz only over Lightspeed.
        FormatCaps caps = CapsForDevice(format, 0);
        g = f;
        assert(EncodeProfileRate(g, format, false, 8000, caps, err) && g[0] == 6 && g[1] == 3);
        assert(ProfileCrc(g) == StoredCrc(g) && DecodeProfileRate(g, format, false) == 8000);
        assert(!EncodeProfileRate(g, format, true, 2000, caps, err));
        if (format == 8) {  // PRO X 3 SUPERSTRIKE cable (USB transport C0A9) runs the full range
            FormatCaps x3 = CapsForDevice(8, 0xC0A9);
            assert(EncodeProfileRate(g, 8, true, 4000, x3, err) && g[1] == 5);
        }
        // An unused slot is refused.
        g = f;
        for (int i = 24; i < 29; ++i) g[i] = 0;  // slot 4
        ApplyCrc(g);
        assert(!EncodeSlotDpi(g, format, 4, 1600, err));
    }

    // Base v1 (format 4): interval in ms at byte 0, scalar slots at 0x03.
    {
        std::vector<uint8_t> v1(255, 0xFF);
        v1[0] = 1;  // 1 ms = 1000 Hz
        v1[1] = 1;  // default slot 1
        v1[2] = 0;
        const int dpis[] = { 400, 800, 1600, 3200, 6400 };
        for (int i = 0; i < 5; ++i) {
            v1[3 + i * 2] = (uint8_t)(dpis[i] & 0xFF);
            v1[4 + i * 2] = (uint8_t)(dpis[i] >> 8);
        }
        ApplyCrc(v1);
        DpiTable t;
        assert(DecodeDpiTable(v1, 4, t) && t.defaultIndex == 1 && t.slots[1].x == 800 && t.slots[4].x == 6400);
        std::string err;
        assert(EncodeSlotDpi(v1, 4, 1, 1200, err) && v1[5] == 0xB0 && v1[6] == 0x04);
        assert(DecodeProfileRate(v1, 4, false) == 1000);
        assert(EncodeProfileRate(v1, 4, false, 500, CapsForFormat(4), err) && v1[0] == 2);
        assert(DecodeProfileRate(v1, 4, true) == 500 && ProfileCrc(v1) == StoredCrc(v1));
    }

    // Writability per OpenMouse WRITABLE_FORMATS.
    assert(FormatWritable(8) && FormatWritable(7) && FormatWritable(2) && !FormatWritable(1) && !FormatWritable(6) &&
           !FormatWritable(5));
    assert(FormatHardwareConfirmed(8) && !FormatHardwareConfirmed(2));

    // DPI list: 100, then +50 steps to 25600, then 26000.
    {
        std::vector<uint8_t> b = { 0x00, 0x64, 0xE0, 0x32, 0x64, 0x00, 0x65, 0x90, 0x00, 0x00 };
        std::vector<DpiRun> runs;
        assert(DecodeDpiList(b, runs));
        int lo, hi, step;
        assert(DpiRangeOf(runs, lo, hi, step) && lo == 100 && hi == 26000 && step == 50);
        assert(DpiListed(runs, 100) && DpiListed(runs, 150) && DpiListed(runs, 25600) && DpiListed(runs, 26000));
        assert(!DpiListed(runs, 125) && !DpiListed(runs, 25650));
        assert(NearestListedDpi(runs, 25800) == 25600 || NearestListedDpi(runs, 25800) == 26000);
        std::vector<uint8_t> bad = { 0xE0, 0x32, 0x64, 0x00 };  // range without a start
        assert(!DecodeDpiList(bad, runs));
    }

    // Report-rate decoders.
    assert((DecodeRateMask(0x7F) == std::vector<int>{ 125, 250, 500, 1000, 2000, 4000, 8000 }));
    assert((DecodeRateMask(0x0F) == std::vector<int>{ 125, 250, 500, 1000 }));
    assert((DecodeRateBitmap(0x8B) == std::vector<int>{ 125, 250, 500, 1000 }));  // G402: 1, 2, 4, 8 ms

    std::puts("logitech_onboard_test: ok");
    return 0;
}
