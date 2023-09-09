#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "common/tusb_common.h"
#include "device/usbd.h"

enum {
    REPORT_ID_JOYSTICK = 1,
    REPORT_ID_LED_SLIDER_16 = 4,
    REPORT_ID_LED_SLIDER_15 = 5,
    REPORT_ID_LED_TOWER_6 = 6,
    REPORT_ID_LED_COMPRESSED = 11,
};

// because they are missing from tusb_hid.h
#define HID_STRING_INDEX(x) HID_REPORT_ITEM(x, 7, RI_TYPE_LOCAL, 1)
#define HID_STRING_INDEX_N(x, n) HID_REPORT_ITEM(x, 7, RI_TYPE_LOCAL, n)
#define HID_STRING_MINIMUM(x) HID_REPORT_ITEM(x, 8, RI_TYPE_LOCAL, 1)
#define HID_STRING_MINIMUM_N(x, n) HID_REPORT_ITEM(x, 8, RI_TYPE_LOCAL, n)
#define HID_STRING_MAXIMUM(x) HID_REPORT_ITEM(x, 9, RI_TYPE_LOCAL, 1)
#define HID_STRING_MAXIMUM_N(x, n) HID_REPORT_ITEM(x, 9, RI_TYPE_LOCAL, n)

// Joystick Report Descriptor Template - Based off Drewol/rp2040-gamecon
// Button Map | X | Y
#define GAMECON_REPORT_DESC_JOYSTICK                                           \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                    \
    HID_USAGE(HID_USAGE_DESKTOP_JOYSTICK),                                     \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                                \
        HID_REPORT_ID(REPORT_ID_JOYSTICK)                                      \
        HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON),                                 \
        HID_USAGE_MIN(1), HID_USAGE_MAX(16),                                   \
        HID_LOGICAL_MIN(0), HID_LOGICAL_MAX(1),                                \
        HID_REPORT_COUNT(16), HID_REPORT_SIZE(1),                              \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                     \
                                                                               \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                \
        HID_USAGE(HID_USAGE_DESKTOP_HAT_SWITCH),                               \
        HID_LOGICAL_MIN(1), HID_LOGICAL_MAX(8),                                \
        HID_PHYSICAL_MIN(0), HID_PHYSICAL_MAX_N(315, 2),                       \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(1),                               \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                     \
                                                                               \
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                \
        HID_USAGE(HID_USAGE_DESKTOP_X), HID_USAGE(HID_USAGE_DESKTOP_Y),        \
        HID_USAGE(HID_USAGE_DESKTOP_Z), HID_USAGE(HID_USAGE_DESKTOP_RX),       \
        HID_LOGICAL_MIN(0x00), HID_LOGICAL_MAX(0xff), /* Analog */             \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(4),                               \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                     \
                                                                               \
        HID_USAGE_PAGE_N(HID_USAGE_PAGE_VENDOR, 2),                            \
        HID_USAGE(0),                                                          \
        HID_LOGICAL_MIN(0x00), HID_LOGICAL_MAX(0xff),                          \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(1),                               \
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                     \
    HID_COLLECTION_END

#define GAMECON_LED_HEADER \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), HID_USAGE(0x00),                   \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                                \
        HID_REPORT_COUNT(1), HID_REPORT_SIZE(8),                                \
        HID_INPUT(HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE)

#define GAMECON_LED_FOOTER \
    HID_COLLECTION_END

// Slider First 16 LEDs (48 rgb zones, BRG order)
#define GAMECON_REPORT_DESC_LED_SLIDER_16                                      \
        HID_REPORT_ID(REPORT_ID_LED_SLIDER_16)                                 \
        HID_REPORT_COUNT(48), HID_REPORT_SIZE(8),                              \
        HID_LOGICAL_MIN(0x00), HID_LOGICAL_MAX_N(0x00ff, 2),                   \
        HID_USAGE_PAGE(HID_USAGE_PAGE_ORDINAL),                                \
        HID_USAGE_MIN(1), HID_USAGE_MAX(48),                                   \
        HID_STRING_MINIMUM(7), HID_STRING_MAXIMUM(54),                         \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

// Slider Remaining 15 LEDs (45 rgb zones, BRG order)
#define GAMECON_REPORT_DESC_LED_SLIDER_15                                      \
        HID_REPORT_ID(REPORT_ID_LED_SLIDER_15)                                 \
        HID_REPORT_COUNT(45), HID_REPORT_SIZE(8),                              \
        HID_LOGICAL_MIN(0x00), HID_LOGICAL_MAX_N(0x00ff, 2),                   \
        HID_USAGE_PAGE(HID_USAGE_PAGE_ORDINAL),                                \
        HID_USAGE_MIN(49), HID_USAGE_MAX(93),                                  \
        HID_STRING_MINIMUM(7), HID_STRING_MAXIMUM(51), /* Delta to previous */ \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

// Tower LEDs (18 rgb zones, BRG order)
#define GAMECON_REPORT_DESC_LED_TOWER_6                                        \
        HID_REPORT_ID(REPORT_ID_LED_TOWER_6)                                   \
        HID_REPORT_COUNT(18), HID_REPORT_SIZE(8),                              \
        HID_LOGICAL_MIN(0x00), HID_LOGICAL_MAX_N(0x00ff, 2),                   \
        HID_USAGE_PAGE(HID_USAGE_PAGE_ORDINAL),                                \
        HID_USAGE_MIN(94), HID_USAGE_MAX(111),                                 \
        HID_STRING_MINIMUM(7), HID_STRING_MAXIMUM(24), /* Delta to previous */ \
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

/*
        HID_STRING_MINIMUM(56), HID_STRING_MAXIMUM(100),                       \
        HID_STRING_MINIMUM(101), HID_STRING_MAXIMUM(117),                      \
*/

// LEDs Compressed
#define GAMECON_REPORT_DESC_LED_COMPRESSED                                     \
        HID_REPORT_ID(REPORT_ID_LED_COMPRESSED)                                \
        HID_USAGE_PAGE(HID_USAGE_PAGE_ORDINAL),                                \
        HID_USAGE(0x00),                                                      \
        HID_LOGICAL_MIN(0x00), HID_LOGICAL_MAX_N(0x00ff, 2),                   \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(63),                              \
        HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

#endif /* USB_DESCRIPTORS_H_ */
