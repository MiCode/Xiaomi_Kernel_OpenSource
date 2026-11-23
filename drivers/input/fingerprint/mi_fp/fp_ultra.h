#ifndef FP_ULTRA_H
#define FP_ULTRA_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/time.h>

#define DTS_IPC_GPIO				"xiaomi,gpio_ipc"
#define DTS_INTR2_GPIO				"xiaomi,gpio_intr2"

#define FP_IPC_DEV_NAME "qbt_ipc"

#define QBT_INPUT_DEV_VERSION 0x0100

#define MAX_FW_EVENTS 128

/*
 * enum qbt_fw_event -
 *      enumeration of firmware events
 * @FW_EVENT_FINGER_DOWN - finger down detected
 * @FW_EVENT_FINGER_UP - finger up detected
 * @FW_EVENT_IPC - an IPC from the firmware is pending
 */
enum qbt_fw_event {
	FW_EVENT_IPC = 3,
};


struct fw_ipc_info {
	int gpio;
	int irq;
	bool irq_enabled;
	struct work_struct work;
    bool work_init;
};

struct ipc_event {
	enum qbt_fw_event ev;
};

#endif