# The radio (2.1.9): Bluetooth MIDI, Wi-Fi, Ableton Link

These files are copied unchanged from [fm1-base](https://github.com/charlesvestal/fm1-base) at commit `3039770`
(2026-11-26), with felucca-bsp `fdd709f`. `firmware/src/radio_arctic.c` wires them to Arctic Scream's platform, and
`firmware/src/net.c` is the NETWORK page and Link's clock. Nothing of JieLi's binary libraries is linked.

| Here | From fm1-base | Who, licence |
|---|---|---|
| `ble/` | `bt/ble/` | Henri David ([Optimist](https://github.com/w0ts/optimist)), `GPL-3.0-only OR MPL-2.0`: taken here under GPL-3.0-only |
| `wifi/`, `device/wifi_own.c`, `device/wifi_own_ap.c`, `device/wifi_own_tune.c` | the same paths | Henri David (Optimist); `wifi_usbnet.*` w0ts. `GPL-3.0-only OR MPL-2.0`: GPL-3.0-only here |
| `link/` | `link/` | Henri David, GPL-3.0-only (the protocol: Ableton Link, Copyright 2016 Ableton AG) |
| `chip/fm1_ble.h`, `chip/fm1_ble_rf.h`, `chip/fm1_wifi.h`, `chip/fm1_rf_regs.h`, `chip/usb_ncm.h` | felucca-bsp `chip/` | Optimist / Leo Kuroshita (Hügelton Instruments), MPL-2.0 or `GPL-3.0-only OR MPL-2.0` as each header says |
| `chip/fm1_rf_cal.h` | felucca-bsp `chip/` | JieLi's calibration rewritten in C by Charles Vestal, Apache-2.0 |
| `chip/ble_rf_tables_v15.h` (and its copy `firmware/gen/ble_rf_tables.h`), `device/fm1_wifi_wlc.h` | felucca-bsp `chip/`, fm1-base `device/` | JieLi (AC79 SDK) and M-VAVE's start-up as packed by Optimist, Apache-2.0 (`LICENSES/Apache-2.0-radio.txt`, `NOTICE-felucca-bsp`, `NOTICE-fm1-base`) |
| `device/radio.c`, `device/bt_own.c`, `device/bt_own_central.c`, `device/bt_own_stream.h`, `device/ble_peers_none.c`, `device/shim_lock.c`, `include/plat.h`, `include/plat_uid.h` | fm1-base `device/`, `include/` | Charles Vestal, MPL-2.0 (`LICENSES/MPL-2.0-fm1-base.txt`): MPL files may be carried in a GPL-3.0 work (MPL-2.0 §3.3); they stay MPL-2.0 |

Each file keeps its own SPDX header. To update: copy the same files again from a newer fm1-base and rebuild
(`tools/build.py` checks that the Wi-Fi frame buffers sit in main RAM below 0x01C20000, and lists any 64-bit
multiply inlined beside a load in the radio's functions).
