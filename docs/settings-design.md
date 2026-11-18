# Mouse settings (DPI, polling rate): design

This fork adds native, read/write mouse settings next to the battery monitor. Goals, in order:

1. Zero idle cost. No timer, no polling for settings: capabilities and current values are read once when a
   device arrives (or the menu asks for a refresh), and written only when the user picks a value.
2. One exe, no runtime. Plain C++/Win32 like the rest of the app.
3. Settings stored on the mouse (onboard memory / storage byte), so nothing has to be re-applied at boot.
4. Protocol knowledge is ported from OpenMouse's `@openmouse/protocol` (AGPL-3.0-or-later), OpenRazer
   (GPL-2.0) and Solaar (GPL-2.0) as documentation; each driver cites the file it follows.
   The combined work is distributed under AGPL-3.0-or-later (GPL-3.0 section 13).

## Interface (`src/settings.h`)

```cpp
struct MouseCaps {
    int dpiMin = 0, dpiMax = 0, dpiStep = 0;   // 0 = DPI not settable
    std::vector<int> pollingRates;             // Hz, ascending; empty = not settable
};

struct SettingsDevice {
    std::wstring name;        // human readable model name
    USHORT vid = 0, pid = 0;  // of the collection used for settings
    std::wstring path;        // HID path of the control collection
    int hidppIndex = -1;      // Logitech device index (0xFF direct, 1..6 behind a receiver)
    BYTE featDpi = 0, featRate = 0, featOnboard = 0;  // Logitech feature indexes (0 = absent)
    int variant = 0;          // driver specific (Razer timing group, Logitech feature flavour ...)
    BYTE tid = 0;             // Razer transaction id
    MouseCaps caps;
    int dpi = -1;             // current active DPI, -1 unknown
    int pollingHz = -1;       // current rate, -1 unknown
    bool verified = false;    // model confirmed on hardware by the source project
};

std::vector<SettingsDevice> EnumSettingsDevices();          // blocking, worker thread only
bool ApplyDpi(SettingsDevice& d, int dpi, std::wstring& err);      // updates d.dpi on success
bool ApplyPolling(SettingsDevice& d, int hz, std::wstring& err);   // updates d.pollingHz on success
```

Per vendor, in `src/settings/`:

```cpp
void EnumRazerSettings(const std::vector<HidColl>&, std::vector<SettingsDevice>&);
bool RazerSetDpi(SettingsDevice&, int dpi, std::wstring& err);
bool RazerSetPolling(SettingsDevice&, int hz, std::wstring& err);
void EnumLogitechSettings(const std::vector<HidColl>&, std::vector<SettingsDevice>&);
bool LogitechSetDpi(SettingsDevice&, int dpi, std::wstring& err);
bool LogitechSetPolling(SettingsDevice&, int hz, std::wstring& err);
```

Rules for drivers:
- Only the HID layer in `src/hid.h` (`HidDev`, `EnumHid`). No new dependency.
- A write is attempted only for a model in the driver's table; unknown products are never written to.
- Read back after each write; report a failure in `err` (short English sentence, shown in a balloon).
- Validate the requested value against `caps` before sending anything.
- No process access, no hooks, no driver: vendor HID reports to the user's own mouse only.

## User interface

The tray / widget menu gets, under each supported mouse's submenu:
- `DPI` submenu: radio items for 400, 800, 1600, 3200 (clamped to the mouse's range) plus the current value
  if it is not one of them, then `Other value...` (small dialog, validated against the range and step).
- `Polling rate` submenu: radio items for each supported rate.
Values come from a cache filled by the worker thread on device arrival; opening the menu never blocks.
