/**
* Copyright MI 2015-2016
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

#define MIUS_DATA_IO_AP_TO_DSP 0
#define MIUS_DATA_IO_DSP_TO_AP 1

#define MIUS_DATA_IO_READ_OK 0
#define MIUS_DATA_IO_READ_BUSY 1
#define MIUS_DATA_IO_READ_CANCEL 2

#define MIUS_MSG_BUF_SIZE 512

/* wake source timeout in ms*/
#define MIUS_WAKEUP_TIMEOUT 250

#define MIUS_DATA_FIFO_SIZE (PAGE_SIZE)

#define ULTRASOUND_RX_PORT_ID  0
#define ULTRASOUND_TX_PORT_ID  1

/* MI UltraSound Module */
#define MIUS_ULTRASOUND_DISABLE             0
#define MIUS_ULTRASOUND_ENABLE              1
#define MIUS_ULTRASOUND_SET_PARAMS          2
#define MIUS_ULTRASOUND_GET_PARAMS          3
#define MIUS_ULTRASOUND_DEBUG_LEVEL         4
#define MIUS_ULTRASOUND_RAMP_DOWN           5
#define MIUS_ULTRASOUND_SUSPEND             6
#define MIUS_ULTRASOUND_CL_DATA             7
#define MIUS_ULTRASOUND_MODE                8
#define MIUS_ULTRASOUND_UPLOAD_NONE         9

/** Param ID definition */
#define MIUS_ULTRASOUND_PARAM_ID_ENGINE_DATA           3
#define MIUS_ULTRASOUND_PARAM_ID_CALIBRATION_DATA     11
#define MIUS_ULTRASOUND_PARAM_ID_ENGINE_VERSION       12
#define MIUS_ULTRASOUND_PARAM_ID_BUILD_BRANCH         14
#define MIUS_ULTRASOUND_PARAM_ID_CALIBRATION_V2_DATA  15
#define MIUS_ULTRASOUND_PARAM_ID_SENSORHUB            16
#define MIUS_ULTRASOUND_PARAM_ID_DIAGNOSTICS_DATA     17
#define MIUS_ULTRASOUND_PARAM_ID_TAG                  18
#define MIUS_ULTRASOUND_PARAM_ID_ML_DATA              19

#define MIUS_DATA_READ_BUSY             0
#define MIUS_DATA_READ_OK               1
#define MIUS_DATA_READ_CANCEL           2

#define MIUS_ALL_DEVICES        -1
#define MIUS_DEVICE_0            0
#define MIUS_DEVICE_1            1

enum mius_message_id {
	MIUS_MESSAGE_PAYLOAD,   /* Input to AP*/
	MIUS_MESSAGE_RAW,       /* Output from AP*/
	MIUS_MESSAGE_CALIBRATION,
	MIUS_MESSAGE_CALIBRATION_V2,
	MIUS_MESSAGE_DIAGNOSTICS,
	MIUS_MAX_MESSAGE_IDS
};

typedef enum {
    MIUS_DATA_PUSH_FROM_KERNEL,
    MIUS_DATA_PUSH_FROM_USERSPACE
} mius_data_push_t;

struct mius_data {
    /* wake lock timeout */
    unsigned int wakeup_timeout;

    /* members for top half interrupt handling */
    struct kfifo fifo_isr;
    spinlock_t fifo_isr_spinlock;
    wait_queue_head_t fifo_isr_not_empty;
    struct mutex user_buffer_lock;

    /* buffer to swap data from isr fifo to userspace */
    uint8_t isr_swap_buffer[MIUS_MSG_BUF_SIZE];

    atomic_t abort_io;

    /* debug counters, reset between open/close */
    uint32_t isr_fifo_discard;

    /* debug counters, persistent */
    uint32_t isr_fifo_discard_total;
    uint32_t userspace_read_total;
    uint32_t isr_write_total;

};

/* Elliptic IO module API (implemented by IO module)*/

int mius_data_io_initialize(void);
int mius_data_io_cleanup(void);

int32_t mius_data_io_write(uint32_t message_id, const char *data,
	size_t data_size);

int32_t mius_data_io_transact(uint32_t message_id, const char *data,
	size_t data_size, char *output_data, size_t output_data_size);


/* Elliptic driver API (implemented by main driver)*/
int mius_data_initialize(struct mius_data *,
	size_t max_queue_size, unsigned int wakeup_timeout, int id);

int mius_data_cleanup(struct mius_data *);

void mius_data_reset_debug_counters(struct mius_data *);
void mius_data_update_debug_counters(struct mius_data *);
void mius_data_print_debug_counters(struct mius_data *);

/* Called from mius device read */
size_t mius_data_pop(struct mius_data *,
	char __user *buffer, size_t buffer_size);

/* Used for cancelling a blocking read */
void mius_data_cancel(struct mius_data *);

/* Called from IO module*/
int mius_data_push(int deviceid, const char *buffer, size_t buffer_size, mius_data_push_t);

/* Writes to io module and user space control */
int32_t mius_data_write(uint32_t message_id,
    const char *data, size_t data_size);

/* Opens port */
int mius_open_port(int portid);

/* Closes port */
int mius_close_port(int portid);

/* Opens port */
int mius_io_open_port(int portid);

/* Closes port */
int mius_io_close_port(int portid);

/* Create device node for userspace io driver*/
int mius_userspace_io_driver_init(void);
void mius_userspace_io_driver_exit(void);

/* Create device node for userspace io driver*/
int mius_userspace_ctrl_driver_init(void);
void mius_userspace_ctrl_driver_exit(void);
int32_t mius_userspace_ctrl_write(uint32_t message_id,
    const char *data, size_t data_size);

