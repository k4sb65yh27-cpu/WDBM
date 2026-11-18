// Dispatches mouse settings to the vendor drivers in src/settings/.
#include "settings.h"

namespace {
constexpr USHORT kRazer = 0x1532, kLogitech = 0x046D;
}

std::vector<SettingsDevice> EnumSettingsDevices() {
    std::vector<SettingsDevice> out;
    std::vector<HidColl> colls = EnumHid();
    EnumRazerSettings(colls, out);
    EnumLogitechSettings(colls, out);
    return out;
}

bool ApplyDpi(SettingsDevice& d, int dpi, std::wstring& err) {
    if (d.caps.dpiMax <= 0 || dpi < d.caps.dpiMin || dpi > d.caps.dpiMax) {
        err = L"DPI out of range";
        return false;
    }
    if (d.vid == kRazer) return RazerSetDpi(d, dpi, err);
    if (d.vid == kLogitech) return LogitechSetDpi(d, dpi, err);
    err = L"Unsupported device";
    return false;
}

bool ApplyPolling(SettingsDevice& d, int hz, std::wstring& err) {
    bool listed = false;
    for (int r : d.caps.pollingRates) listed |= r == hz;
    if (!listed) {
        err = L"Polling rate not supported";
        return false;
    }
    if (d.vid == kRazer) return RazerSetPolling(d, hz, err);
    if (d.vid == kLogitech) return LogitechSetPolling(d, hz, err);
    err = L"Unsupported device";
    return false;
}
