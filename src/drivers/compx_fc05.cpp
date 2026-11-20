// Wireless mice on the Compx 2.4 GHz receiver 3554:FC05 (product string "2.4G Receiver"), sold under
// several office-mouse brands. The receiver has no 17-byte channel, so the Kysona protocol does not
// apply. The level is read, never written, from an 8-byte feature report:
//   GET_FEATURE report id 06 on the FF02:0002 collection of interface 1:
//   [06][00][00][level][level][00][..][..]   level = 0..100 %
// Source: a capture on a user's receiver (2026-11): a full mouse read 06 00 00 64 64 00 C5 49, with and
// without its charging cable. No charging flag was seen, so charging is not reported. Bytes 3 and 4
// carried the same value; the driver uses byte 3 and only trusts the reply when both agree.
#include "drivers.h"

void PollCompxFc05(PollContext& ctx) {
    for (auto [vid, pid] : ctx.Products(0x3554)) {
        if (pid != 0xFC05) continue;
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
                 return c.page() == 0xFF02 && c.usage() == 0x0002 && c.featLen() >= 6;
             })) {
            HidDev d;
            if (!d.OpenFeatureOnly(*c)) continue;
            std::vector<BYTE> r;
            if (!d.GetFeature(0x06, r) || r.size() < 6 || r[0] != 0x06) continue;
            // 0 / 0: receiver without its mouse (off or asleep); a mismatch: not this layout.
            if (r[3] > 100 || r[3] != r[4] || r[3] == 0) {
                ctx.Claim(vid, pid);
                break;
            }
            DeviceStatus s;
            s.key = L"compx:" + Hex4(vid) + L":" + Hex4(pid);
            s.name = L"2.4G wireless mouse";
            s.kind = DeviceKind::Mouse;
            s.via = L"2.4 GHz receiver";
            s.percent = r[3];
            s.graceSec = 1800;
            ctx.Add(s, vid, pid);
            break;
        }
    }
}
