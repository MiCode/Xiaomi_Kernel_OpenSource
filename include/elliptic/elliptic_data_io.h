/**
* Copyright Elliptic Labs 2015-2016
* Copyright (C) 2019 XiaoMi, Inc.
*
*/

#pragma once

#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#define ELLIPTIC_DATA_IO_AP_TO_DSP 0
#define ELLIPTIC_DATA_IO_DSP_TO_AP 1

#define ELLIPTIC_DATA_IO_READ_OK 0
#define ELLIPTIC_DATA_IO_READ_BUSY 1
#define ELLIPTIC_DATA_IO_READ_CANCEL 2

#define ELLIPTIC_MSG_BUF_SIZE 512

/* wake source timeout in ms*/
#define ELLIPTIC_WAKEUP_TIMEOUT 250

#define ELLIPTIC_DATA_FIFO_SIZE (PAGE_SIZE)

#define ULTRASOUND_RX_PORT_ID  0
#define ULTRASOUND_TX_PORT_ID  1

/* Elliptic Labs UltraSound Module */
#define ELLIPTIC_ULTRASOUND_DISABLE         0
#define ELLIPTIC_ULTRASOUND_ENABLE          1
#define ELLIPTIC_ULTRASOUND_SET_PARAMS          2
#define ELLIPTIC_ULTRASOUND_GET_PARAMS          3
#define ELLIPTIC_ULTRASOUND_RAMP_DOWN           4

/** Param ID definition */
#define ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_DATA           3
#define ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_DATA     11
#define ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_VERSION       12
#define ELLIPTIC_ULTRASOUND_PARAM_ID_BUILD_BRANCH         14
#define ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_V2_DATA  15
#define ELLIPTIC_ULTRASOUND_PARAM_ID_SENSORHUB            16
#define ELLIPTIC_ULTRASOUND_PARAM_ID_DIAGNOSTICS_DATA     17
#define ELLIPTIC_ULTRASOUND_PARAM_ID_TAG                  18
#define ELLIPTIC_ULTRASOUND_PARAM_ID_ML_DATA              19

#define ELLIPTIC_DATA_READ_BUSY             0
#define ELLIPTIC_DATA_READ_OK               1
#define ELLIPTIC_DATA_READ_CANCEL           2

#define ELLIPTIC_ALL_DEVICES        -1
#define ELLIPTIC_DEVICE_0            0
#define ELLIPTIC_DEVICE_1            1

/** 512 byte APR payload */
#define ELLIPTIC_GET_PARAMS_SIZE            128
/** System config size is 96 bytes */
/* #define ELLIPTIC_SET_PARAMS_SIZE         128 */
#define ELLIPTIC_SET_PARAMS_SIZE            114

enum elliptic_message_id {
	ELLIPTIC_MESSAGE_PAYLOAD,   /* Input to AP*/
	ELLIPTIC_MESSAGE_RAW,       /* Output from AP*/
	ELLIPTIC_MESSAGE_CALIBRATION,
	ELLIPTIC_MESSAGE_CALIBRATION_V2,
	ELLIPTIC_MESSAGE_DIAGNOSTICS,
	ELLIPTIC_MAX_MESSAGE_IDS
};

typedef enum {
    ELLIPTIC_DATA_PUSH_FROM_KERNEL,
    ELLIPTIC_DATA_PUSH_FROM_USERSPACE
} elliptic_data_push_t;

struct elliptic_data {
    /* wake lock timeout */
    unsigned int wakeup_timeout;

    /* members for top half interrupt handling */
    struct kfifo fifo_isr;
    spinlock_t fifo_isr_spinlock;
    wait_queue_head_t fifo_isr_not_empty;
    struct mutex user_buffer_lock;

    /* buffer to swap data from isr fifo to userspace */
    uint8_t isr_swap_buffer[ELLIPTIC_MSG_BUF_SIZE];

    atomic_t abort_io;

    /* debug counters, reset between open/close */
    uint32_t isr_fifo_discard;

    /* debug counters, persistent */
    uint32_t isr_fifo_discard_total;
    uint32_t userspace_read_total;
    uint32_t isr_write_total;

};

/* Elliptic IO module API (implemented by IO module)*/

int elliptic_data_io_initialize(void);
int elliptic_data_io_cleanup(void);

int32_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size);

int32_t elliptic_data_io_transact(uint32_t message_id, const char *data,
	size_t data_size, char *output_data, size_t output_data_size);


/* Elliptic driver API (implemented by main driver)*/
int elliptic_data_initialize(struct elliptic_data *,
	size_t max_queue_size, unsigned int wakeup_timeout, int id);

int elliptic_data_cleanup(struct elliptic_data *);

void elliptic_data_reset_debug_counters(struct elliptic_data *);
void elliptic_data_update_debug_counters(struct elliptic_data *);
void elliptic_data_print_debug_counters(struct elliptic_data *);

/* Called from elliptic device read */
size_t elliptic_data_pop(struct elliptic_data *,
	char __user *buffer, size_t buffer_size);

/* Used for cancelling a blocking read */
void elliptic_data_cancel(struct elliptic_data *);

/* Called from IO module*/
int elliptic_data_push(int deviceid, const char *buffer, size_t buffer_size, elliptic_data_push_t);

/* Writes to io module and user space control */
int32_t elliptic_data_write(uint32_t message_id,
    const char *data, size_t data_size);

/* Opens port */
int elliptic_open_port(int portid);

/* Closes port */
int elliptic_close_port(int portid);

/* Opens port */
int elliptic_io_open_port(int portid);

/* Closes port */
int elliptic_io_close_port(int portid);

/* Create device node for userspace io driver*/
int elliptic_userspace_io_driver_init(void);
void elliptic_userspace_io_driver_exit(void);

/* Create device node for userspace io driver*/
int elliptic_userspace_ctrl_driver_init(void);
void elliptic_userspace_ctrl_driver_exit(void);
int32_t elliptic_userspace_ctrl_write(uint32_t message_id,
    const char *data, size_t data_size);

