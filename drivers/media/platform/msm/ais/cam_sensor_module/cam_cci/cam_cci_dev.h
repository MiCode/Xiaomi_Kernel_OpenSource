/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAM_CCI_DEV_H_
#define _CAM_CCI_DEV_H_

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/cam_sensor.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <cam_sensor_cmn_header.h>
#include <cam_io_util.h>
#include <cam_sensor_util.h>
#include <cam_subdev.h>
#include <cam_cpas_api.h>
#include "cam_cci_hwreg.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"

#define V4L2_IDENT_CCI 50005
#define CCI_I2C_QUEUE_0_SIZE 128
#define CCI_I2C_QUEUE_1_SIZE 32
#define CYCLES_PER_MICRO_SEC_DEFAULT 4915
#define CCI_MAX_DELAY 1000000

#define CCI_TIMEOUT msecs_to_jiffies(1500)

#define NUM_MASTERS 2
#define NUM_QUEUES 2

#define TRUE  1
#define FALSE 0

#define CCI_PINCTRL_STATE_DEFAULT "cci_default"
#define CCI_PINCTRL_STATE_SLEEP "cci_suspend"

#define CCI_NUM_CLK_MAX 16
#define CCI_NUM_CLK_CASES 5
#define CCI_CLK_SRC_NAME "cci_src_clk"
#define MSM_CCI_WRITE_DATA_PAYLOAD_SIZE_10 10
#define MSM_CCI_WRITE_DATA_PAYLOAD_SIZE_11 11
#define BURST_MIN_FREE_SIZE 8
#define MAX_LRME_V4l2_EVENTS 30

/* Max bytes that can be read per CCI read transaction */
#define CCI_READ_MAX 256
#define CCI_I2C_READ_MAX_RETRIES 3
#define CCI_I2C_MAX_READ 8192
#define CCI_I2C_MAX_WRITE 8192
#define CCI_I2C_MAX_BYTE_COUNT 65535

#define CAMX_CCI_DEV_NAME "cam-cci-driver"

#define MAX_CCI 2

#define PRIORITY_QUEUE (QUEUE_0)
#define SYNC_QUEUE (QUEUE_1)

enum cci_i2c_sync {
	MSM_SYNC_DISABLE,
	MSM_SYNC_ENABLE,
};

enum cam_cci_cmd_type {
	MSM_CCI_INIT,
	MSM_CCI_RELEASE,
	MSM_CCI_SET_SID,
	MSM_CCI_SET_FREQ,
	MSM_CCI_SET_SYNC_CID,
	MSM_CCI_I2C_READ,
	MSM_CCI_I2C_WRITE,
	MSM_CCI_I2C_WRITE_SEQ,
	MSM_CCI_I2C_WRITE_BURST,
	MSM_CCI_I2C_WRITE_ASYNC,
	MSM_CCI_GPIO_WRITE,
	MSM_CCI_I2C_WRITE_SYNC,
	MSM_CCI_I2C_WRITE_SYNC_BLOCK,
};

enum cci_i2c_queue_t {
	QUEUE_0,
	QUEUE_1,
	QUEUE_INVALID,
};

struct cam_cci_wait_sync_cfg {
	uint16_t cid;
	int16_t csid;
	uint16_t line;
	uint16_t delay;
};

struct cam_cci_gpio_cfg {
	uint16_t gpio_queue;
	uint16_t i2c_queue;
};

struct cam_cci_read_cfg {
	uint32_t addr;
	uint16_t addr_type;
	uint8_t *data;
	uint16_t num_byte;
	uint16_t data_type;
};

struct cam_cci_i2c_queue_info {
	uint32_t max_queue_size;
	uint32_t report_id;
	uint32_t irq_en;
	uint32_t capture_rep_data;
};

struct cam_cci_master_info {
	uint32_t status;
	atomic_t q_free[NUM_QUEUES];
	uint8_t q_lock[NUM_QUEUES];
	uint8_t reset_pending;
	struct mutex mutex;
	struct completion reset_complete;
	struct completion th_complete;
	struct mutex mutex_q[NUM_QUEUES];
	struct completion report_q[NUM_QUEUES];
	atomic_t done_pending[NUM_QUEUES];
	spinlock_t lock_q[NUM_QUEUES];
};

struct cam_cci_clk_params_t {
	uint16_t hw_thigh;
	uint16_t hw_tlow;
	uint16_t hw_tsu_sto;
	uint16_t hw_tsu_sta;
	uint16_t hw_thd_dat;
	uint16_t hw_thd_sta;
	uint16_t hw_tbuf;
	uint8_t hw_scl_stretch_en;
	uint8_t hw_trdhld;
	uint8_t hw_tsp;
	uint32_t cci_clk_src;
};

enum cam_cci_state_t {
	CCI_STATE_ENABLED,
	CCI_STATE_DISABLED,
};

/**
 * struct cci_device
 * @pdev: Platform device
 * @subdev: V4L2 sub device
 * @base: Base address of CCI device
 * @hw_version: Hardware version
 * @ref_count: Reference Count
 * @cci_state: CCI state machine
 * @num_clk: Number of CCI clock
 * @cci_clk: CCI clock structure
 * @cci_clk_info: CCI clock information
 * @cam_cci_i2c_queue_info: CCI queue information
 * @i2c_freq_mode: I2C frequency of operations
 * @cci_clk_params: CCI hw clk params
 * @cci_gpio_tbl: CCI GPIO table
 * @cci_gpio_tbl_size: GPIO table size
 * @cci_pinctrl: Pinctrl structure
 * @cci_pinctrl_status: CCI pinctrl status
 * @cci_clk_src: CCI clk src rate
 * @cci_vreg: CCI regulator structure
 * @cci_reg_ptr: CCI individual regulator structure
 * @regulator_count: Regulator count
 * @support_seq_write:
 *     Set this flag when sequential write is enabled
 * @write_wq: Work queue structure
 * @valid_sync: Is it a valid sync with CSID
 * @v4l2_dev_str: V4L2 device structure
 * @cci_wait_sync_cfg: CCI sync config
 * @cycles_per_us: Cycles per micro sec
 * @payload_size: CCI packet payload size
 * @irq_status1: Store irq_status1 to be cleared after
 *               draining FIFO buffer for burst read
 * @lock_status: to protect changes to irq_status1
 * @is_burst_read: Flag to determine if we are performing
 *                 a burst read operation or not
 */
struct cci_device {
	struct v4l2_subdev subdev;
	struct cam_hw_soc_info soc_info;
	uint32_t hw_version;
	uint8_t ref_count;
	enum cam_cci_state_t cci_state;
	struct cam_cci_i2c_queue_info
		cci_i2c_queue_info[NUM_MASTERS][NUM_QUEUES];
	struct cam_cci_master_info cci_master_info[NUM_MASTERS];
	enum i2c_freq_mode i2c_freq_mode[NUM_MASTERS];
	struct cam_cci_clk_params_t cci_clk_params[I2C_MAX_MODES];
	struct msm_pinctrl_info cci_pinctrl;
	uint8_t cci_pinctrl_status;
	uint8_t support_seq_write;
	struct workqueue_struct *write_wq[MASTER_MAX];
	struct cam_cci_wait_sync_cfg cci_wait_sync_cfg;
	uint8_t valid_sync;
	struct cam_subdev v4l2_dev_str;
	uint32_t cycles_per_us;
	int32_t clk_level_index;
	uint8_t payload_size;
	char device_name[20];
	uint32_t cpas_handle;
	uint32_t irq_status1;
	spinlock_t lock_status;
	bool is_burst_read;
};

enum cam_cci_i2c_cmd_type {
	CCI_I2C_SET_PARAM_CMD = 1,
	CCI_I2C_WAIT_CMD,
	CCI_I2C_WAIT_SYNC_CMD,
	CCI_I2C_WAIT_GPIO_EVENT_CMD,
	CCI_I2C_TRIG_I2C_EVENT_CMD,
	CCI_I2C_LOCK_CMD,
	CCI_I2C_UNLOCK_CMD,
	CCI_I2C_REPORT_CMD,
	CCI_I2C_WRITE_CMD,
	CCI_I2C_READ_CMD,
	CCI_I2C_WRITE_DISABLE_P_CMD,
	CCI_I2C_READ_DISABLE_P_CMD,
	CCI_I2C_WRITE_CMD2,
	CCI_I2C_WRITE_CMD3,
	CCI_I2C_REPEAT_CMD,
	CCI_I2C_INVALID_CMD,
};

enum cam_cci_gpio_cmd_type {
	CCI_GPIO_SET_PARAM_CMD = 1,
	CCI_GPIO_WAIT_CMD,
	CCI_GPIO_WAIT_SYNC_CMD,
	CCI_GPIO_WAIT_GPIO_IN_EVENT_CMD,
	CCI_GPIO_WAIT_I2C_Q_TRIG_EVENT_CMD,
	CCI_GPIO_OUT_CMD,
	CCI_GPIO_TRIG_EVENT_CMD,
	CCI_GPIO_REPORT_CMD,
	CCI_GPIO_REPEAT_CMD,
	CCI_GPIO_CONTINUE_CMD,
	CCI_GPIO_INVALID_CMD,
};

struct cam_sensor_cci_client {
	struct v4l2_subdev *cci_subdev;
	uint32_t freq;
	enum i2c_freq_mode i2c_freq_mode;
	enum cci_i2c_master_t cci_i2c_master;
	uint16_t sid;
	uint16_t cid;
	uint32_t timeout;
	uint16_t retries;
	uint16_t id_map;
	uint16_t cci_device;
};

struct cam_cci_ctrl {
	int32_t status;
	struct cam_sensor_cci_client *cci_info;
	enum cam_cci_cmd_type cmd;
	union {
		struct cam_sensor_i2c_reg_setting cci_i2c_write_cfg;
		struct cam_cci_read_cfg cci_i2c_read_cfg;
		struct cam_cci_wait_sync_cfg cci_wait_sync_cfg;
		struct cam_cci_gpio_cfg gpio_cfg;
	} cfg;
};

struct cci_write_async {
	struct cci_device *cci_dev;
	struct cam_cci_ctrl c_ctrl;
	enum cci_i2c_queue_t queue;
	struct work_struct work;
	enum cci_i2c_sync sync_en;
};

irqreturn_t cam_cci_irq(int irq_num, void *data);

#ifdef CONFIG_MSM_AIS
extern struct v4l2_subdev *cam_cci_get_subdev(int cci_dev_index);
#else
static inline struct v4l2_subdev *cam_cci_get_subdev(int cci_dev_index)
{
	return NULL;
}
#endif

#define VIDIOC_MSM_CCI_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 23, struct cam_cci_ctrl *)

#endif /* _CAM_CCI_DEV_H_ */
