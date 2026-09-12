# Ghostcontrol (PS5 USB Controller Patcher)

A PlayStation 5 payload that enables third-party USB and 2.4GHz wireless dongle controllers on jailbroken PS5 consoles. It reads raw USB HID / XInput reports from connected controllers and injects them into a virtual DualSense controller via the PS5's `scePadVirtualDeviceInsertData` (VDI) subsystem.

This repository is a fork of [StonedModder's Ghostcontrol](https://github.com/StonedModder/Ghostcontrol---PS5-USB-Controller-Patcher), extending compatibility to **GameSir Cyclone 2** (and standard XInput / Xbox 360 wireless controllers) with improved 125Hz continuous input injection.

---

## Supported Controllers

| Controller | Connection / Mode | VID:PID | Status |
|---|---|---|---|
| **GameSir Cyclone 2** | 2.4GHz Wireless Dongle (PC / XInput mode) | `3537:100b` | ✅ Fully Working (Sticks, Triggers, D-Pad, Buttons, Home) |
| **8BitDo Ultimate 2** | Nintendo Switch Pro mode | `057e:2009` | ✅ Working |
| **8BitDo Ultimate 2** | Native mode | `2dc8:310b` | Untested |
| **Xbox One S / Series** | Wired USB | `045e:02ea` / `045e:0b12` | ✅ Supported |

See `othercontrollersGuide.md` for adding new controllers.

---

## Features

- **High-Frequency Continuous Injection**: 125Hz dedicated injection thread ensuring zero dropped inputs, responsive navigation, and smooth analog stick control across games and system menus.
- **XInput & Xbox 360 Wireless Protocol**: Native packet parsing with deadzone calibration, analog trigger mapping, and Guide/Home button handling.
- **PS5 On-Screen Notifications**: Informative notifications on controller detection, user assignment, and disconnect.
- **User Assignment Dialog**: Virtual DualSense binds cleanly to the foreground profile on startup or interactive user assignment.
- **Auto-Reconnect**: Seamless re-initialization upon controller or dongle unplug/replug.

---

## Requirements

- PS5 on a compatible jailbreakable firmware (with kernel exploit / elf loader support).
- [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk) to compile from source.
- Compatible USB controller or wireless dongle.

> **Note:** In compliance with copyright guidelines, pre-compiled payload binaries (`.elf`) are not distributed in this repository. You can compile the payload using the PS5 Payload SDK.

---

## Building from Source

Ensure you have installed the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) and have the environment configured:

```sh
cd payload
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
make clean all
```

This will produce the compiled payload: `ghost-control-xbox-ps5.elf`.

---

## Deployment

Send the compiled ELF to your PS5 running an ELF loader (e.g. on port 9021 or 9020):

```sh
# Replace with your PS5's IP address
nc -w 5 192.168.1.xxx 9021 < payload/ghost-control-xbox-ps5.elf
```

Or deploy directly via Makefile:

```sh
cd payload
make deploy PS5_HOST=192.168.1.xxx PORT=9021
```

---

## How It Works

1. **Virtual Device Creation (VDA)**: Creates a virtual DualSense pad via `scePadVirtualDeviceAddDevice(type=3)`.
2. **Handle Acquisition**: Captures kernel logging events (`klog`) to obtain the virtual device handle.
3. **User Binding**: Associates the virtual controller with the foreground user session via ShellUI MBus IPC (`shellui_pad.c`).
4. **USB HID / XInput Engine**: Detaches the console's default kernel HID driver from the device endpoint, opens raw USB FS endpoints, runs any required initialization handshakes, and reads controller reports.
5. **Continuous Injection (VDI)**: Decodes controller packets into native `ScePadData` structures and feeds them continuously at 125Hz through `scePadVirtualDeviceInsertData`.

See `ProControllerResearch.md` for research documentation on the USB HID protocol.

---

## File Structure

| File / Directory | Description |
|---|---|
| `payload/gc_main.c` | Core payload: virtual device lifecycle, USB FS listener, continuous injection thread |
| `payload/controller_xbox360.c` | Xbox 360 / XInput driver (GameSir Cyclone 2, wireless dongles) |
| `payload/controller_xbox360.h` | Header for Xbox 360 / XInput driver |
| `payload/controller_xbox.c` | Xbox One / Series S wired driver |
| `payload/controller_nintendo.c` | Nintendo Switch Pro / 8BitDo driver |
| `payload/shellui_pad.c` | ShellUI PT_ATTACH helper for user binding |
| `payload/usb_helpers.c` | USB FS transfer helpers |
| `payload/Makefile` | Compilation and deployment recipes |
| `ProControllerResearch.md` | Full USB protocol research for Nintendo Switch Pro Controller |
| `othercontrollersGuide.md` | Guide for adding other USB HID controllers |

---

## Credits & Acknowledgements

- **[StonedModder](https://github.com/StonedModder)** — Creator of the original [Ghostcontrol](https://github.com/StonedModder/Ghostcontrol---PS5-USB-Controller-Patcher) project and research into the PS5 Virtual Device Interface.
- **[H4zeyaf](https://github.com/H4zeyaf)** — GameSir Cyclone 2 support, continuous 125Hz injection implementation, and XInput driver integration.
- Contributors and developers of the [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk).

---

## License

GPL-3.0-or-later
