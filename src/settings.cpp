// Dispatches mouse settings to the vendor drivers in src/settings/.
#include "settings.h"

#include "i18n.h"

namespace {
constexpr USHORT kRazer = 0x1532, kLogitech = 0x046D;
}

std::mutex& RazerIoLock() {
    static std::mutex m;
    return m;
}

std::mutex& LogitechIoLock() {
    static std::mutex m;
    return m;
}

std::vector<SettingsDevice> EnumSettingsDevices() {
    std::vector<SettingsDevice> out;
    std::vector<HidColl> colls = EnumHid();
    {
        std::lock_guard<std::mutex> lock(RazerIoLock());
        EnumRazerSettings(colls, out);
    }
    {
        std::lock_guard<std::mutex> lock(LogitechIoLock());
        EnumLogitechSettings(colls, out);
    }
    return out;
}

bool ApplyDpi(SettingsDevice& d, int dpi, std::wstring& err) {
    if (d.caps.dpiMax <= 0 || dpi < d.caps.dpiMin || dpi > d.caps.dpiMax) {
        err = TF(L"This value is outside the range of this mouse.", {});
        return false;
    }
    if (d.vid == kRazer) {
        std::lock_guard<std::mutex> lock(RazerIoLock());
        return RazerSetDpi(d, dpi, err);
    }
    if (d.vid == kLogitech) {
        std::lock_guard<std::mutex> lock(LogitechIoLock());
        return LogitechSetDpi(d, dpi, err);
    }
    err = TF(L"This mouse isn't supported.", {});
    return false;
}

bool ApplyPolling(SettingsDevice& d, int hz, std::wstring& err) {
    bool listed = false;
    for (int r : d.caps.pollingRates) listed |= r == hz;
    if (!listed) {
        err = TF(L"This mouse doesn't support this polling rate.", {});
        return false;
    }
    if (d.vid == kRazer) {
        std::lock_guard<std::mutex> lock(RazerIoLock());
        return RazerSetPolling(d, hz, err);
    }
    if (d.vid == kLogitech) {
        std::lock_guard<std::mutex> lock(LogitechIoLock());
        return LogitechSetPolling(d, hz, err);
    }
    err = TF(L"This mouse isn't supported.", {});
    return false;
}
