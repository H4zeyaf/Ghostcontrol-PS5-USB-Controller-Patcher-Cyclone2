#include "controller_xbox360.h"
#include "usb_helpers.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __PROSPERO__
#include <ps5/klog.h>
#define LOG(...) klog_printf("[GC] " __VA_ARGS__)
#else
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#endif

/* ── helpers ──────────────────────────────────────────────────────────── */

#define DEADZONE 4000

static inline uint8_t stick_x(int16_t v) {
    return (v > DEADZONE || v < -DEADZONE) ? (uint8_t)((v + 32768) >> 8) : 128u;
}

static inline uint8_t stick_y(int16_t v) {
    return (v > DEADZONE || v < -DEADZONE) ? (uint8_t)(255 - ((v + 32768) >> 8)) : 128u;
}

/* ── input parsing ────────────────────────────────────────────────────── */

/*
 * Xbox 360 wired / XInput packet layout (official Linux xpad.c):
 *
 * b[0] = 0x00 (report ID)
 * b[1] = 0x14 (length = 20)
 *
 * b[2] (dpad + system buttons):
 *   bit 0 (0x01): DPad Up
 *   bit 1 (0x02): DPad Down
 *   bit 2 (0x04): DPad Left
 *   bit 3 (0x08): DPad Right
 *   bit 4 (0x10): Start (Options)
 *   bit 5 (0x20): Back / View (Share)
 *   bit 6 (0x40): Left Stick click (L3)
 *   bit 7 (0x80): Right Stick click (R3)
 *
 * b[3] (bumpers + Guide + face buttons):
 *   bit 0 (0x01): LB (L1)
 *   bit 1 (0x02): RB (R1)
 *   bit 2 (0x04): Guide / Home (PS button)
 *   bit 3 (0x08): sync / unused
 *   bit 4 (0x10): A (Cross)
 *   bit 5 (0x20): B (Circle)
 *   bit 6 (0x40): X (Square)
 *   bit 7 (0x80): Y (Triangle)
 *
 * b[4] = Left Trigger (0 - 255)
 * b[5] = Right Trigger (0 - 255)
 * b[6..7]   = Left Stick X  (int16 LE, center=0)
 * b[8..9]   = Left Stick Y  (int16 LE, center=0, inverted)
 * b[10..11] = Right Stick X (int16 LE, center=0)
 * b[12..13] = Right Stick Y (int16 LE, center=0, inverted)
 */

void xbox360_parse_input(const uint8_t *b, ScePadData *o) {
    uint8_t b2 = b[2];
    uint8_t b3 = b[3];
    uint8_t lt = b[4];
    uint8_t rt = b[5];
    int16_t lx = (int16_t)((uint16_t)b[6]  | ((uint16_t)b[7]  << 8));
    int16_t ly = (int16_t)((uint16_t)b[8]  | ((uint16_t)b[9]  << 8));
    int16_t rx = (int16_t)((uint16_t)b[10] | ((uint16_t)b[11] << 8));
    int16_t ry = (int16_t)((uint16_t)b[12] | ((uint16_t)b[13] << 8));

    uint32_t btn = 0;

    /* Dpad: b[2] bits 0-3 (standard Xbox 360 bitmask) */
    if (b2 & 0x01u) btn |= SCE_PAD_BUTTON_UP;
    if (b2 & 0x02u) btn |= SCE_PAD_BUTTON_DOWN;
    if (b2 & 0x04u) btn |= SCE_PAD_BUTTON_LEFT;
    if (b2 & 0x08u) btn |= SCE_PAD_BUTTON_RIGHT;

    /* Start / Back: b[2] bits 4-5 */
    if (b2 & 0x10u) btn |= SCE_PAD_BUTTON_OPTIONS;  /* Start -> Options */
    if (b2 & 0x20u) btn |= SCE_PAD_BUTTON_SHARE;    /* Back -> Share / Create */

    /* Thumbstick clicks: b[2] bits 6-7 */
    if (b2 & 0x40u) btn |= SCE_PAD_BUTTON_L3;
    if (b2 & 0x80u) btn |= SCE_PAD_BUTTON_R3;

    /* Bumpers: b[3] bits 0-1 */
    if (b3 & 0x01u) btn |= SCE_PAD_BUTTON_L1;
    if (b3 & 0x02u) btn |= SCE_PAD_BUTTON_R1;

    /* Guide / Home: b[3] bit 2 */
    if (b3 & 0x04u) btn |= SCE_PAD_BUTTON_PS;

    /* Face buttons: b[3] bits 4-7 */
    if (b3 & 0x10u) btn |= SCE_PAD_BUTTON_CROSS;    /* A -> Cross */
    if (b3 & 0x20u) btn |= SCE_PAD_BUTTON_CIRCLE;   /* B -> Circle */
    if (b3 & 0x40u) btn |= SCE_PAD_BUTTON_SQUARE;   /* X -> Square */
    if (b3 & 0x80u) btn |= SCE_PAD_BUTTON_TRIANGLE; /* Y -> Triangle */

    /* Analog Triggers + digital threshold */
    o->analogButtons.l2 = lt;
    o->analogButtons.r2 = rt;
    if (lt > 16u) btn |= SCE_PAD_BUTTON_L2;
    if (rt > 16u) btn |= SCE_PAD_BUTTON_R2;

    o->buttons = btn;

    /* Analog Sticks with deadzone filter */
    o->leftStick.x  = stick_x(lx);
    o->leftStick.y  = stick_y(ly);
    o->rightStick.x = stick_x(rx);
    o->rightStick.y = stick_y(ry);

    o->connected = 1;
    o->quat.w    = 1.0f;
}

/* ── XInput protocol ──────────────────────────────────────────────────── */

void xbox360_handshake(int fd, struct usb_fs_endpoint *eps) {
    /* Send single OUT enable packet to start XInput data stream */
    uint8_t enable[] = {0x01, 0x03, 0x0E};
    usb_send_out(fd, &eps[1], enable, sizeof(enable), "xbox360_enable");
    LOG("Xbox 360 handshake sent\n");
}

int xbox360_handle_packet(int fd, struct usb_fs_endpoint *eps,
                          const uint8_t *buf, uint32_t len,
                          ScePadData *out_pad) {
    (void)fd; (void)eps;

    if (len < 3) return 0;

    /* Guide / Home button short packet: [0x01, 0x03, <pressed>] or [0x00, 0x03, <pressed>] */
    if ((buf[0] == 0x01 || buf[0] == 0x00) && buf[1] == 0x03 && len >= 3) {
        out_pad->leftStick.x = 128;
        out_pad->leftStick.y = 128;
        out_pad->rightStick.x = 128;
        out_pad->rightStick.y = 128;
        out_pad->connected = 1;
        out_pad->quat.w = 1.0f;
        if (buf[2] & 0x01u) out_pad->buttons = SCE_PAD_BUTTON_PS;
        return 1;
    }

    if (len < 14) return 0;

    const uint8_t *pkt = NULL;

    /* 1. Direct Xbox 360 controller report starting at buf[0] */
    if (buf[0] == 0x00 && buf[1] == 0x14 && len >= 14) {
        pkt = buf;
    }
    /* 2. Wireless receiver dongle with 4-byte radio header (length >= 24) */
    else if (len >= 24 && buf[4] == 0x00 && buf[5] == 0x14) {
        pkt = &buf[4];
    }
    /* 3. Search for [0x00, 0x14] signature anywhere */
    else {
        for (uint32_t i = 0; i + 14 <= len; i++) {
            if (buf[i] == 0x00 && buf[i+1] == 0x14) {
                pkt = &buf[i];
                break;
            }
        }
    }

    /* Reject all packets without valid [0x00, 0x14] Xbox 360 input signature!
     * Never fall back to parsing an all-zero or unknown buffer. */
    if (pkt) {
        xbox360_parse_input(pkt, out_pad);
        return 1;
    }

    return 0;
}
