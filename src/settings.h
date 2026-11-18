#pragma once
// Native mouse settings (DPI, polling rate). See docs/settings-design.md.
// Read once when a device arrives or on request, written only when the user picks a value:
// nothing here runs while the app is idle. Drivers run on the settings worker thread: their `err`
// texts use TF() / the std::wstring T() (the const wchar_t* T() is for the UI thread only).
#include <mutex>
#include <string>
#include <vector>

#include "hid.h"

struct MouseCaps {
    int dpiMin = 0, dpiMax = 0, dpiStep = 0;  // 0 = DPI not settable
    std::vector<int> pollingRates;            // Hz, ascending; empty = not settable
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
    std::string unitId;       // Logitech unit id (memory only): a write checks the slot still holds this mouse
};

// One lock per vendor, held by the battery poller (PollRazer / PollLogitech) and by the settings
// enumeration and writes, so the two threads never talk to the same vendor's devices at once.
// Each holder takes one lock at a time, never both, so they cannot deadlock.
std::mutex& RazerIoLock();
std::mutex& LogitechIoLock();

std::vector<SettingsDevice> EnumSettingsDevices();                // blocking, worker thread only
bool ApplyDpi(SettingsDevice& d, int dpi, std::wstring& err);      // updates d.dpi on success
bool ApplyPolling(SettingsDevice& d, int hz, std::wstring& err);   // updates d.pollingHz on success

void EnumRazerSettings(const std::vector<HidColl>& colls, std::vector<SettingsDevice>& out);
bool RazerSetDpi(SettingsDevice& d, int dpi, std::wstring& err);
bool RazerSetPolling(SettingsDevice& d, int hz, std::wstring& err);
void EnumLogitechSettings(const std::vector<HidColl>& colls, std::vector<SettingsDevice>& out);
bool LogitechSetDpi(SettingsDevice& d, int dpi, std::wstring& err);
bool LogitechSetPolling(SettingsDevice& d, int hz, std::wstring& err);
