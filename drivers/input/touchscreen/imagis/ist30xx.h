/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __IST30XX_H__
#define __IST30XX_H__

/*
 * Support F/W ver : IST30xxB v3.3 (included tag)
 * Support IC : IST3026B, IST3032B, IST3038
 * Release : 2014.01.07 by Ian Bae
 */

#define IMAGIS_IST30XX          (1)             /* 3026, 3032 */
#define IMAGIS_IST30XXB         (2)             /* 3026B, 3032B */
#define IMAGIS_IST3038          (3)             /* 3038 */
#define IMAGIS_IST3044          (4)             /* 3044 */

#define IMAGIS_TSP_IC           IMAGIS_IST3038

#if ((IMAGIS_TSP_IC == IMAGIS_IST30XX) || (IMAGIS_TSP_IC == IMAGIS_IST30XXB))
#define IST30XX_EXTEND_COORD    (0)     /* IST3026, IST3032, IST3038 */
#elif ((IMAGIS_TSP_IC == IMAGIS_IST3038) || (IMAGIS_TSP_IC == IMAGIS_IST3044))
#define IST30XX_EXTEND_COORD    (1)     /* IST3038, IST3044 */
#endif

#define I2C_BURST_MODE          (1)
#define I2C_MONOPOLY_MODE       (0)

#define IST30XX_EVENT_MODE      (1)
#if IST30XX_EVENT_MODE
# define IST30XX_NOISE_MODE     (1)
# define IST30XX_TRACKING_MODE  (1)
# define IST30XX_ALGORITHM_MODE (1)
#else
# define IST30XX_NOISE_MODE     (0)
# define IST30XX_TRACKING_MODE  (0)
# define IST30XX_ALGORITHM_MODE (0)
#endif

#define IST30XX_USE_KEY         (1)
#define IST30XX_DEBUG           (1)
#define IST30XX_CMCS_TEST       (1)

#define IST30XX_DEV_NAME        "IST30XX"
#define IST30XX_CHIP_ID         (0x30003000)
#define IST30XXA_CHIP_ID        (0x300a300a)
#define IST30XXB_CHIP_ID        (0x300b300b)
#define IST3038_CHIP_ID         (0x30383038)

#define IST30XX_DEV_ID          (0xA0 >> 1)
#define IST30XX_FW_DEV_ID       (0xA4 >> 1)

#define IST30XX_ADDR_LEN        (4)
#define IST30XX_DATA_LEN        (4)

#define IST30XX_ISP_CMD_LEN     (3)

#define IST30XX_MAX_MT_FINGERS  (10)
#define IST30XX_MAX_KEYS        (5)

#define IST30XX_MAX_X           (720)
#define IST30XX_MAX_Y           (1280)
#define IST30XX_MAX_W           (15)

#define IST30XX                 (1)
#define IST30XXB                (2)

/* I2C Transfer msg number */
#define WRITE_CMD_MSG_LEN       (1)
#define READ_CMD_MSG_LEN        (2)

#define NORMAL_TEMPERATURE      (0)
#define LOW_TEMPERATURE         (1)
#define HIGH_TEMPERATURE        (2)

/* Local */
#define TSP_LOCAL_GENERAL	(0)
#define TSP_LOCAL_CODE          TSP_LOCAL_GENERAL

/* Debug message */
#define DEV_ERR	(1)
#define DEV_WARN	(2)
#define DEV_INFO	(3)
#define DEV_NOTICE	(4)
#define DEV_DEBUG	(5)
#define DEV_VERB	(6)

#define IST30XX_DEBUG_TAG       "[ Imagis ]"
#define IST30XX_DEBUG_LEVEL     DEV_INFO

#define IST30XX_MAX_LOG_SIZE    (4 * 100 * 1024)    // 4bytes * 100Kbytes
#define IST30XX_INTERNAL_BIN    (1)

#define IST30XX_CMCS_LOAD_END       (0x8FFFFCAB)
#if (IMAGIS_TSP_IC == IMAGIS_IST30XXB)
#define IST30XX_CMCS_BUF_SIZE       (16 * 16)
#elif (IMAGIS_TSP_IC == IMAGIS_IST3038)
#define IST30XX_CMCS_BUF_SIZE       (((19 * 19) / 2 + 1) * 2)
#else
#error "Unknown TSP_IC"
#endif

#if IST30XX_INTERNAL_BIN
#define IST30XX_UPDATE_BY_WORKQUEUE     (0)
#define IST30XX_UPDATE_DELAY    (3 * HZ)
#endif

#define tsp_err(fmt, ...)   tsp_printk(DEV_ERR, fmt, ## __VA_ARGS__)
#define tsp_warn(fmt, ...)  tsp_printk(DEV_WARN, fmt, ## __VA_ARGS__)
#define tsp_info(fmt, ...)  tsp_printk(DEV_INFO, fmt, ## __VA_ARGS__)
#define tsp_notc(fmt, ...)  tsp_printk(DEV_NOTICE, fmt, ## __VA_ARGS__)
#define tsp_debug(fmt, ...) tsp_printk(DEV_DEBUG, fmt, ## __VA_ARGS__)
#define tsp_verb(fmt, ...)  tsp_printk(DEV_VERB, fmt, ## __VA_ARGS__)


enum ist30xx_commands {
	CMD_ENTER_UPDATE            = 0x02,
	CMD_EXIT_UPDATE             = 0x03,
	CMD_UPDATE_SENSOR           = 0x04,
	CMD_UPDATE_CONFIG           = 0x05,
	CMD_ENTER_REG_ACCESS        = 0x07,
	CMD_EXIT_REG_ACCESS         = 0x08,
	CMD_SET_NOISE_MODE          = 0x0A,
	CMD_START_SCAN              = 0x0B,
	CMD_ENTER_FW_UPDATE         = 0x0C,
	CMD_RUN_DEVICE              = 0x0D,
	CMD_EXEC_MEM_CODE           = 0x0E,
	CMD_SET_TEST_MODE           = 0x0F,

	CMD_CALIBRATE               = 0x11,
	CMD_USE_IDLE                = 0x12,
	CMD_USE_DEBUG               = 0x13,
	CMD_ZVALUE_MODE             = 0x15,
	CMD_SAME_POSITION           = 0x16,
	CMD_CHECK_CALIB             = 0x1A,
	CMD_SET_TEMPER_MODE         = 0x1B,
	CMD_USE_CORRECT_CP          = 0x1C,
	CMD_SET_REPORT_RATE         = 0x1D,
	CMD_SET_IDLE_TIME           = 0x1E,

	CMD_GET_COORD               = 0x20,

	CMD_GET_CHIP_ID             = 0x30,
	CMD_GET_FW_VER              = 0x31,
	CMD_GET_CHECKSUM            = 0x32,
	CMD_GET_LCD_RESOLUTION      = 0x33,
	CMD_GET_TSP_CHNUM1          = 0x34,
	CMD_GET_PARAM_VER           = 0x35,
	CMD_GET_SUB_VER             = 0x36,
	CMD_GET_CALIB_RESULT        = 0x37,
	CMD_GET_TSP_SWAP_INFO       = 0x38,
	CMD_GET_KEY_INFO1           = 0x39,
	CMD_GET_KEY_INFO2           = 0x3A,
	CMD_GET_KEY_INFO3           = 0x3B,
	CMD_GET_TSP_CHNUM2          = 0x3C,
	CMD_GET_TSP_DIRECTION       = 0x3D,

	CMD_GET_TSP_VENDOR          = 0x3E,
	CMD_GET_TSP_PANNEL_TYPE     = 0x40,

	CMD_GET_CHECKSUM_ALL        = 0x41,
	CMD_DEFAULT                 = 0xFF,
};

#define CMD_FW_UPDATE_MAGIC     (0x85FDAE8A)


typedef struct _ALGR_INFO {
	u32	scan_status;
	u8	touch_cnt;
	u8	intl_touch_cnt;
	u16	status_flag;

	u16	raw_peak_min;
	u16	raw_peak_max;
	u16	flt_peak_max;
	u16	adpt_threshold;

	u16	key_raw_data[6];
} ALGR_INFO;

#if IST30XX_EXTEND_COORD
#define EXTEND_COORD_CHECKSUM   (1)
#define IST30XX_INTR_STATUS1    (0x71000000)
#define IST30XX_INTR_STATUS2    (0x00000C00)
#define CHECK_INTR_STATUS1(n)   (((n & IST30XX_INTR_STATUS1) == IST30XX_INTR_STATUS1) ? 1 : 0)
#define CHECK_INTR_STATUS2(n)   (((n & IST30XX_INTR_STATUS2) > 0) ? 0 : 1)
#define CHECK_INTR_STATUS3(n)   (((n & IST30XX_INTR_STATUS2) == IST30XX_INTR_STATUS2) ? 1 : 0)

#define PARSE_FINGER_CNT(n)     ((n >> 12) & 0xF)
#define PARSE_KEY_CNT(n)        ((n >> 21) & 0x7)
#define PARSE_FINGER_STATUS(n)  (n & 0x3FF)             /* Finger status: [9:0] */
#define PARSE_KEY_STATUS(n)     ((n >> 16) & 0x1F)      /* Key status: [20:16] */

#define PRESSED_FINGER(s, id)    ((s & (1 << (id - 1))) ? true : false)
#define PRESSED_KEY(s, id)       ((s & (1 << (16 + id - 1))) ? true : false)
typedef union {
	struct {
		u32	y       : 12;
		u32	x       : 12;
		u32	area    : 4;
		u32	id      : 4;
	} bit_field;
	u32 full_field;
} finger_info;
#else  // IST30XX_EXTEND_COORD

typedef union {
	struct {
		u32	y       : 10;
		u32	w       : 6;
		u32	x       : 10;
		u32	id      : 4;
		u32	udmg    : 2;
	} bit_field;
	u32 full_field;
} finger_info;
#endif  // IST30XX_EXTEND_COORD


struct ist30xx_status {
	int	power;
	int	update;
	int	calib;
	int	calib_msg;
	bool	event_mode;
	bool	noise_mode;
};

struct ist30xx_fw {
	u32	prev_core_ver;
	u32	prev_param_ver;
	u32	core_ver;
	u32	param_ver;
	u32	sub_ver;
	u32	index;
	u32	size;
	u32	chksum;
	u32	buf_size;
	u8 *	buf;
	u32	fw_tsp_index;
};

#define IST30XX_TAG_MAGIC       "ISTV1TAG"
struct ist30xx_tags {
	char	magic1[8];
	u32	fw_addr;
	u32	fw_size;
	u32	flag_addr;
	u32	flag_size;
	u32	cfg_addr;
	u32	cfg_size;
	u32	sensor1_addr;
	u32	sensor1_size;
	u32	sensor2_addr;
	u32	sensor2_size;
	u32	sensor3_addr;
	u32	sensor3_size;
	u32	chksum;
	u32	reserved2;
	char	magic2[8];
};

#include "ist30xx_tsp.h"
#include <linux/earlysuspend.h>
#include <linux/power_supply.h>

struct ist30xx_config_info {
	int		tsp_type;
	const char *	tsp_name;
	int *		key_code;
	const char *	cmcs_name;
	const char *	fw_name;
};

struct ist30xx_platform_data {
	int				max_x;
	int				max_y;
	int				max_w;
	int				key_num;
	unsigned long			irqflags;
	u32				reset_gpio_flags;
	u32				irq_gpio_flags;
	u32				power_gpio_flags;
	int				config_array_size;
	struct ist30xx_config_info *	config_array;
	int				reset_gpio;
	int				power_gpio;
	int				irq_gpio;
};

/* for misc struct */
struct TSP_CH_NUM {
	u8	tx;
	u8	rx;
};
struct TSP_NODE_BUF {
	u16	raw[NODE_TX_NUM][NODE_RX_NUM];
	u16	base[NODE_TX_NUM][NODE_RX_NUM];
	u16	filter[NODE_TX_NUM][NODE_RX_NUM];
	u16	min_raw;
	u16	max_raw;
	u16	min_base;
	u16	max_base;
	u16	len;
};
struct TSP_DIRECTION {
	bool	swap_xy;
	bool	flip_x;
	bool	flip_y;
};
typedef struct _TSP_INFO {
	struct TSP_CH_NUM	ch_num;
	struct TSP_DIRECTION	dir;
	struct TSP_NODE_BUF	node;
	int			height;
	int			width;
	int			finger_num;
} TSP_INFO;
typedef struct _TKEY_INFO {
	int	key_num;
	bool	enable;
	bool	axis_rx;
	u8	axis_chnum;
	u8	ch_num[5];
} TKEY_INFO;
/* for misc struct */

/* for tracking struct */
typedef struct _IST30XX_RINGBUF {
	int	RingBufCtr; // Number of characters in the ring buffer
	u16	RingBufInIdx;
	u16	RingBufOutIdx;
	u8	LogBuf[IST30XX_MAX_LOG_SIZE];        // Ring buffer for status
} IST30XX_RING_BUF;
/* for tracking struct */

#define IST30XX_CMCS_MAGIC          "CMCS1TAG"
/* for cmcs struct */
struct CMCS_ADDR_INFO {
	u32	base_screen;
	u32	base_key;
	u32	start_cp;
	u32	vcmp;
	u32	sensor1;
	u32	sensor2;
	u32	sensor3;
};
struct CMCS_CH_INFO {
	u8	tx_num;
	u8	rx_num;
	u8	key_rx;
	u8	key1;
	u8	key2;
	u8	key3;
	u8	key4;
	u8	key5;
};
struct CMCS_SLOPE_INFO {
	s16	x_min;
	s16	x_max;
	s16	y_min;
	s16	y_max;
};
struct CMCS_SPEC_INFO {
	u16	screen_min;
	u16	screen_max;
	u16	key_min;
	u16	key_max;
};
struct CMCS_CMD {
	u16	mode;   // enable bit
	u16	cmcs_size;
	u16	base_screen;
	u16	base_key;
	u8	start_cp_cm;
	u8	start_cp_cs;
	u8	vcmp_cm;
	u8	vcmp_cs;
	u32	reserved; // for checksum of firmware
};
typedef struct _CMCS_INFO {
	u32			timeout;
	struct CMCS_ADDR_INFO	addr;
	struct CMCS_CH_INFO	ch;
	struct CMCS_SPEC_INFO	spec_cr;
	struct CMCS_SPEC_INFO	spec_cm;
	struct CMCS_SPEC_INFO	spec_cs0;
	struct CMCS_SPEC_INFO	spec_cs1;
	struct CMCS_SLOPE_INFO	slope;
	u16			sensor1_size;
	u16			sensor2_size;
	u16			sensor3_size;
	u16			reserved;
	u32			cmcs_chksum;
	u32			sensor_chksum;
	struct CMCS_CMD		cmd;
} CMCS_INFO;

typedef struct _CMCS_BIN_INFO {
	char		magic1[8];
	CMCS_INFO	cmcs;
	u8 *		buf_cmcs;
	u32 *		buf_sensor;
	u16 *		buf_node;
	char		magic2[8];
} CMCS_BIN_INFO;

typedef struct _CMCS_BUF {
	s16	cm[IST30XX_CMCS_BUF_SIZE];
	s16	spec[IST30XX_CMCS_BUF_SIZE];
	s16	slope0[IST30XX_CMCS_BUF_SIZE];
	s16	slope1[IST30XX_CMCS_BUF_SIZE];
	s16	cs0[IST30XX_CMCS_BUF_SIZE];
	s16	cs1[IST30XX_CMCS_BUF_SIZE];
} CMCS_BUF;
/* for cmcs struct */

struct ist30xx_data {
	struct i2c_client *		client;
	struct input_dev *		input_dev;
	struct early_suspend		early_suspend;
	struct ist30xx_status		status;
	struct ist30xx_fw		fw;
	struct ist30xx_tags		tags;
	u32				current_index;
	u32				chip_id;
	u32				tsp_type;
	u32				max_fingers;
	u32				max_keys;
	u32				irq_enabled;
#if IST30XX_EXTEND_COORD
	u32				t_status;
#else
	u32				num_fingers;
	u32				num_keys;
	finger_info			prev_fingers[IST30XX_MAX_MT_FINGERS];
	finger_info			prev_keys[IST30XX_MAX_MT_FINGERS];
#endif
	finger_info			fingers[IST30XX_MAX_MT_FINGERS];
	int				initialized;

#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
	struct delayed_work		work_fw_update;
#endif
	struct delayed_work		work_reset_check;
	struct delayed_work		work_noise_protect;
	struct delayed_work		work_debug_algorithm;
#if IST30XX_EVENT_MODE
	struct timer_list		event_timer;
	u32				scan_count;
	u32				algr_addr;
	u32				algr_size;
	u32				intr_debug_addr;
	u32				intr_debug_size;
#endif
	struct mutex			ist30xx_mutex;
	int				error_cnt;
	int				ta_status;
	int				noise_mode;
#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
	struct          delayed_work	work_fw_update;
#endif
	int				report_rate;
	int				idle_rate;
	u32				event_ms;
	u32				timer_ms;
	int				scan_retry;
	struct ist30xx_platform_data *	pdata;
	int				dbg_level;
	bool				tsp_touched[IST30XX_MAX_MT_FINGERS];
	bool				tkey_pressed[IST30XX_MAX_KEYS];
	char				is_fw_update[20];
	u32				fw_ver;
	u32				param_ver;
	u32				sub_ver;
	struct ist30xx_tags *		ts_tags;
	u32 *				frame_buf;
	u32 *				frame_rawbuf;
	u32 *				frame_fltbuf;
	struct				class *ist30xx_class;
	struct device *			sys_dev;
	struct device *			tunes_dev;
	struct device *			node_dev;
	struct				class *sec_class;
	struct device *			sec_touchscreen;
	struct device *			sec_touchkey;
	struct device *			sec_fac_dev;
	struct device *			tracking_dev;
	struct device *			fw_dev;
	struct device *			cmcs_dev;
	bool				tunes_cmd_done;
	bool				reg_mode;
	TSP_INFO			tsp_info;
	TKEY_INFO			tkey_info;
	IST30XX_RING_BUF		TrackBuf;
	IST30XX_RING_BUF *		pTrackBuf;
	bool				tracking_initialize;
	int				cmcs_ready;
	u8 *				cmcs_bin;
	u32				cmcs_bin_size;
	CMCS_BIN_INFO			ist30xx_cmcs_bin;
	CMCS_BUF			ist30xx_cmcs_buf;
	CMCS_BIN_INFO *			cmcs;
	CMCS_BUF *			cmcs_buf;
	struct regulator *		vdd;
	struct regulator *		vcc_i2c;
	bool				irq_working;
	u32				noise_ms;

	struct notifier_block 		power_supply_notifier;
	bool				is_usb_plug_in;
};

void tsp_printk(int level, const char *fmt, ...);
int ist30xx_intr_wait(struct ist30xx_data *data, long ms);

void ist30xx_enable_irq(struct ist30xx_data *data);
void ist30xx_disable_irq(struct ist30xx_data *data);

void ist30xx_start(struct ist30xx_data *data);
int ist30xx_get_ver_info(struct ist30xx_data *data);
int ist30xx_init_touch_driver(struct ist30xx_data *data);

int ist30xx_get_position(struct i2c_client *client, u32 *buf, u16 len);

int ist30xx_read_cmd(struct i2c_client *client, u32 cmd, u32 *buf);
int ist30xx_write_cmd(struct i2c_client *client, u32 cmd, u32 val);

int ist30xx_cmd_run_device(struct i2c_client *client, bool is_reset);
int ist30xx_cmd_start_scan(struct i2c_client *client);
int ist30xx_cmd_calibrate(struct i2c_client *client);
int ist30xx_cmd_check_calib(struct i2c_client *client);
int ist30xx_cmd_update(struct i2c_client *client, int cmd);
int ist30xx_cmd_reg(struct i2c_client *client, int cmd);

int ist30xx_power_on(struct ist30xx_data *data, bool download);
int ist30xx_power_off(struct ist30xx_data *data);
int ist30xx_reset(struct ist30xx_data *data, bool download);

int ist30xx_internal_suspend(struct ist30xx_data *data);
int ist30xx_internal_resume(struct ist30xx_data *data);

int __devinit ist30xx_init_system(struct ist30xx_data *data);

#endif  // __IST30XX_H__
