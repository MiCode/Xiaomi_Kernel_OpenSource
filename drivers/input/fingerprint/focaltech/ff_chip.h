/**
 * ${ANDROID_BUILD_TOP}/vendor/focaltech/src/chips/protocol.h
 *
 * Copyright (C) 2014-2017 FocalTech Systems Co., Ltd. All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
**/

#ifndef __FF_CHIPS_PROTOCOL_H__
#define __FF_CHIPS_PROTOCOL_H__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/string.h>

typedef enum {
    FF_POWER_MODE_WAKEUP = 0, // Wake up any sleep mode.
    FF_POWER_MODE_INIT_SLEEP, // Init the sleeping procedure for the first time, ft9348 only.
    FF_POWER_MODE_AUTO_SLEEP, // Smart mode, wake and sleep periodically.
    FF_POWER_MODE_DEEP_SLEEP, // Low power mode, can be waked up as soon as possible.
    FF_POWER_MODE_LOST_POWER, // Very low power mode.
} ff_power_mode_t;

typedef enum {
    FF_DEVICE_MODE_IDLE = 0,
    FF_DEVICE_MODE_WAIT_TOUCH, // Wait for finger touching.
    FF_DEVICE_MODE_WAIT_LEAVE, // Wait for finger leaving.
    FF_DEVICE_MODE_DET_FINGER, // Detect the finger, no waiting.
    FF_DEVICE_MODE_WAIT_IMAGE, // Wait for the finger to touch and then scan the image.
    FF_DEVICE_MODE_SCAN_IMAGE, // Scan the image regardless the finger.
    FF_DEVICE_MODE_GESTURE,    // Wait for the finger to touch and decide the gesture.
} ff_device_mode_t;

typedef enum {
    FF_DEVICE_STAT_IDLE = 0,
    FF_DEVICE_STAT_BUSY = 1,
} ff_device_status_t;
typedef enum
{
    e_WorkMode_Idle = 0x00,
    e_WorkMode_Sleep,
    e_WorkMode_Fdt,
    e_WorkMode_Img,
    e_WorkMode_Nav,
    e_WorkMode_SysRst,
    e_WorkMode_AfeRst,
    e_WorkMode_FdtRst,
    e_WorkMode_FifoRst,
    e_WorkMode_OscOn,
    e_WorkMode_OscOff,
    e_WorkMode_SpiWakeUp,
    e_WorkMode_Max,
    e_Dummy
}E_WORKMODE_FW;
typedef struct {
    int cols;
    int rows;
} ff_sensor_t;

typedef struct {
    uint16_t device_id;
    uint16_t chip_id;
    ff_sensor_t sensor;
} ff_device_info_t;

typedef struct {
    /*
     * Write the device's Special Function Register.
     *
     * @params
     *  addr: Register address.
     *  data: Register data.
     *
     * @return
     *  ff_err_t code.
     */
    int (*write_info)(uint8_t addr, uint8_t data);

    /*
     * Write the device's Special Function Register.
     *
     * @params
     *  addr: Register address.
     *  data: Register data.
     *
     * @return
     *  ff_err_t code.
     */
    int (*write_sfr)(uint8_t addr, uint8_t data);

    /*
     * Read the device's Special Function Register.
     *
     * @params
     *  addr: Register address.
     *  data: Pointer of register buffer.
     *
     * @return
     *  ff_err_t code. And the register value is updated to $data if success.
     */
    int (*read_sfr)(uint8_t addr, uint8_t *data);

    /*
     * Write data to the the device SRAM.
     *
     * @params
     *  addr: The start address of SRAM to be written.
     *  data: Data to be written.
     *  dlen: Data length of $data.
     *
     * @return
     *  ff_err_t code.
     */
    int (*write_sram)(uint16_t addr, const void *data, uint16_t dlen);

    /*
     * Read data from the the device SRAM.
     *
     * @params
     *  addr: The start address of SRAM to be read.
     *  data: The buffer to store the read data.
     *  dlen: Data length to be read.
     *
     * @return
     *  ff_err_t code.
     */
    int (*read_sram)(uint16_t addr, void *data, uint16_t dlen);

    /*
     * Configure the device into certain power mode.
     *
     * @params
     *  mode: ff_power_mode_t.
     *
     * @return
     *  ff_err_t code.
     */
    int (*config_power_mode)(ff_power_mode_t mode);

    /*
     * Configure the device into certain working mode.
     *
     * @params
     *  mode: ff_device_mode_t.
     *
     * @return
     *  ff_err_t code.
     */
    int (*config_device_mode)(ff_device_mode_t mode);

    /*
     * Query the event source status (actually the interrupt status register).
     *
     * @return
     *  A ff_event_status_t code, otherwise ff_err_t code.
     */
    int (*query_event_status)(void);

    /*
     * Query the device (actually the MCU) status.
     *
     * @return
     *  A ff_device_status_t code, otherwise ff_err_t code.
     */
    int (*query_device_status)(void);

    /*
     * Query the finger status, whether the finger is pressed on the sensor
     * in another words.
     *
     * @return
     *  A ff_finger_status_t code, otherwise ff_err_t code.
     */
    int (*query_finger_status)(void);

    /*
     * Query the gesture status while the device is in FF_DEVICE_MODE_GESTURE mode.
     *
     * @return
     *  A ff_gesture_status_t code, otherwise ff_err_t code.
     */
    int (*query_gesture_status)(void);

    /*
     * Device health checking.
     *
     * @return
     *  ff_err_t code.
     */
    int (*check_alive)(void);

    /*
     * Initialize the fingerprint chip/sensor.
     *
     * @return
     *  ff_err_t code.
     */
    int (*init_chip)(void);

    /*
     * Probe the device id.
     *
     * @return
     *  The device id or 0x0000 on failure.
     */
    uint16_t (*probe_id)(void);

    /*
     * Reset the fingerprint device through software command.
     * The device is reset to a standby state.
     *
     * @return
     *  ff_err_t code.
     */
    int (*sw_reset)(void);

    /*
     * Reset the fingerprint device through RST pin.
     * The device is reset to a standby state.
     *
     * @return
     *  ff_err_t code.
     */
    int (*hw_reset)(void);

} ff_chip_operation_t;

typedef struct {
    struct list_head chiplist;
    ff_device_info_t info;
    ff_chip_operation_t chip;
} ff_device_t;

/*
 * Registered chips list.
 */
extern struct list_head *g_chiplist;

/*
 * The singleton ff_device_t object.
 */
extern ff_device_t *g_device;

typedef struct __attribute__((__packed__)) {
    uint8_t cmd[2];
    uint8_t addr;
    uint8_t tx_byte;
    uint8_t rx_byte;
} ff_sfr_buf_t;

typedef struct __attribute__((__packed__)) {
    uint8_t cmd[2];
    uint16_t addr;  /* Big-endian */
    uint16_t dlen;  /* Big-endian */
    uint8_t data[]; /* 16 bits big-endian data buffer. */
} ff_sram_buf_t;

typedef struct __attribute__((__packed__)) {
    uint8_t cmd[2];
    uint8_t addr;
    union {
        uint8_t dlen; /* For reading. */
        uint8_t data; /* For writing. */
    };
} ff_info_buf_t;

#define MAX_XFER_BUF_SIZE 2048

/*
 * Convenient macro to convert buffer to special format.
 */
#define TYPE_OF(type, buf) (type *)(buf)

/* Unused variables */
#define UNUSED_VAR(v) ((void)v)

/* Endian swapping (16|32|64 bits). */
#define u16_swap_endian(A) ((uint16_t)( \
        (((A) & 0xff00) >> 8) | \
        (((A) & 0x00ff) << 8)  ))
#define u32_swap_endian(A) ((uint32_t)( \
        (((A) & 0xff000000) >> 24) | \
        (((A) & 0x00ff0000) >> 8 ) | \
        (((A) & 0x0000ff00) << 8 ) | \
        (((A) & 0x000000ff) << 24)  ))
#define u64_swap_endian(A) ((uint64_t)( \
        (((A) & 0xff00000000000000ull) >> 56) | \
        (((A) & 0x00ff000000000000ull) >> 40) | \
        (((A) & 0x0000ff0000000000ull) >> 24) | \
        (((A) & 0x000000ff00000000ull) >> 8 ) | \
        (((A) & 0x00000000ff000000ull) << 8 ) | \
        (((A) & 0x0000000000ff0000ull) << 24) | \
        (((A) & 0x000000000000ff00ull) << 40) | \
        (((A) & 0x00000000000000ffull) << 56)  ))
/* End of Endian swapping. */

#endif /* __FF_CHIPS_PROTOCOL_H__ */
