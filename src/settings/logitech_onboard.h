#pragma once
// Logitech HID++ 2.0 helpers with no I/O: onboard profile (feature 0x8100) layout, CRC and the
// DPI / report-rate fields, plus the decoders for the DPI (0x2201 / 0x2202) and report-rate
// (0x8060 / 0x8061) replies. Kept free of Windows APIs so tests/logitech_onboard_test.cpp can
// run them on the host.
//
// Ported from OpenMouse @openmouse/protocol 0.60.0 (AGPL-3.0-or-later):
//   dist/drivers/logitech/onboard-profiles.js  formats, layouts, CRC, encodeDpiStages,
//                                              encodeReportRate, reportRateCapabilitiesFor
//   dist/drivers/logitech/hidpp.js             getDpiOptions / readLegacyDpiList (DPI list and
//                                              range encoding), decodeExtendedReportRateMask
//   dist/logitech/index.js                     decodeReportRateBitmap (0x8060)
#include <cstdint>
#include <string>
#include <vector>

namespace logi {

// Rate codes shared by 0x8061 and the base-v6 profile byte (REPORT_RATE_HZ).
extern const int kRateHz[7];

// ---- onboard profile (0x8100) ----------------------------------------------------------------

// CRC-16/CCITT-FALSE over everything but the trailing two bytes (profileCrc), stored big-endian.
uint16_t ProfileCrc(const std::vector<uint8_t>& sector);
uint16_t StoredCrc(const std::vector<uint8_t>& sector);
void ApplyCrc(std::vector<uint8_t>& sector);

// Formats OpenMouse writes profile content to (WRITABLE_FORMATS = 2, 3, 4, 7, 8).
bool FormatWritable(int format);
// Formats whose profile DPI / rate write OpenMouse reports as confirmed on real hardware
// (3: G703, 4: G102, 7: PRO X SUPERLIGHT 2 probe, 8: PRO X 3 SUPERSTRIKE DPI write).
// Format 2 (LOGAN) is writable there but still "requires a hardware sanity check".
bool FormatHardwareConfirmed(int format);

// Storage limits per format (FORMAT_CAPABILITIES). `ok` is false for a format with no
// established limits, which then offers no profile edits.
struct FormatCaps {
    bool ok = false;
    int maxStages = 0, minDpi = 0, maxDpi = 0, stepDpi = 0;
    int wirelessMaxHz = 0, wiredMaxHz = 0;
};
FormatCaps CapsForFormat(int format);
// reportRateCapabilitiesFor: the cable of a product whose USB transport id is listed (C0A9, the
// PRO X 3 SUPERSTRIKE) runs the full wireless range.
FormatCaps CapsForDevice(int format, uint16_t usbTransportId);

// Byte offsets (layoutForFormat): base v1 for formats 1-5, base v6 from 6 on.
struct Layout {
    bool v6 = false;
    int rateWireless = 0, rateWired = 0, dpi = 0;
};
Layout LayoutForFormat(int format);

struct DpiStage {
    int x = 0, y = 0, lod = 0;  // x == 0: unused slot
};
struct DpiTable {
    int defaultIndex = -1;  // -1 when the byte is 0xFF
    std::vector<DpiStage> slots;  // always 5 slots, positional
};
// decodeDpi / decodeLegacyDpi, keeping slot positions. False if the sector is too short.
bool DecodeDpiTable(const std::vector<uint8_t>& sector, int format, DpiTable& out);

// Sets X and Y of one slot to `dpi` (lift-off byte and every other byte untouched), then
// reapplies the CRC. This is encodeDpiStages restricted to one slot: with the other slots
// carried through unchanged it produces the same bytes. Returns false with `err` set when the
// slot is unused or out of range.
bool EncodeSlotDpi(std::vector<uint8_t>& sector, int format, int slot, int dpi, std::string& err);

// decodeReportRate / decodeLegacyReportRate for one link; 0 when unset or unknown.
int DecodeProfileRate(const std::vector<uint8_t>& sector, int format, bool wired);
// encodeReportRate: base v1 stores the interval in ms (one shared byte), base v6 the rate code
// per link. Reapplies the CRC. Returns false with `err` set when the rate cannot be stored.
bool EncodeProfileRate(std::vector<uint8_t>& sector, int format, bool wired, int hz, const FormatCaps& caps,
                       std::string& err);

// ---- DPI lists -------------------------------------------------------------------------------

// One run of DPI values: lo, lo + step, ..., hi (step 0 for a single value).
struct DpiRun {
    int lo = 0, hi = 0, step = 0;
};
// Decodes the big-endian value list of getSensorDpiList: plain values, and (0xE000 | step)
// markers that extend the previous value up to the next one. Stops at a 0 value. Returns false
// on a malformed range (OpenMouse throws for 0x2202 and stops for 0x2201).
bool DecodeDpiList(const std::vector<uint8_t>& bytes, std::vector<DpiRun>& runs);
bool DpiListed(const std::vector<DpiRun>& runs, int dpi);
// Range and step the settings menu can offer: min, max and the GCD of every listed value and step
// (dpiStageCapabilitiesForOptions). False when the list is empty.
bool DpiRangeOf(const std::vector<DpiRun>& runs, int& lo, int& hi, int& step);
// The listed value nearest to `dpi` (for error messages); 0 when the list is empty.
int NearestListedDpi(const std::vector<DpiRun>& runs, int dpi);

// ---- report rates ----------------------------------------------------------------------------

// 0x8061: bit N of the big-endian mask is rate code N.
std::vector<int> DecodeRateMask(uint16_t mask);
// 0x8060: bit i marks an interval of (i + 1) ms. Ascending Hz.
std::vector<int> DecodeRateBitmap(uint8_t bits);
int RateCode(int hz);  // index in kRateHz, -1 if none

}  // namespace logi
