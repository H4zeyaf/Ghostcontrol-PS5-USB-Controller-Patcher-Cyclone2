#pragma once
#include <stdint.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include "gc_types.h"

/*
 * Xbox 360 (Wired) / XInput Protocol
 * Used by GameSir Cyclone 2 and others.
 *
 * Endpoints: IN=0x81 OUT=0x02 (varies, check descriptor)
 */

#define XBOX360_EP_IN   0x81
#define XBOX360_EP_OUT  0x02

/* Parse Xbox 360 input report into ScePadData */
void xbox360_parse_input(const uint8_t *buf, ScePadData *o);

/* Xbox 360 / XInput initialization handshake */
void xbox360_handshake(int fd, struct usb_fs_endpoint *eps);

/* Handle one IN packet. Returns 1 if pad updated, 0 to skip. */
int  xbox360_handle_packet(int fd, struct usb_fs_endpoint *eps,
                           const uint8_t *buf, uint32_t len,
                           ScePadData *out_pad);

void notify(const char *fmt, ...);
