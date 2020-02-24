/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux foundation. All rights reserved.
 */

#ifndef __WIGIG_SENSING_H__
#define __WIGIG_SENSING_H__
#include <linux/cdev.h>
#include <linux/circ_buf.h>
#include <linux/kfifo.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <uapi/misc/wigig_sensing_uapi.h>

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "[wigig_sensing]: " fmt

/* Registers */
#define RGF_USER_SPI_SPI_MBOX_FILL_STATUS (0x880080)
#define RGF_USER_SPI_SPI_EXT_MBOX_OUTB    (0x880084)
#define RGF_USER_SPI_SPI_MBOX_INB         (0x880088)
#define RGF_SPI_FIFO_CONTROL_ADDR         (0x8800A0)
#define RGF_SPI_FIFO_WR_PTR_ADDR          (0x88009C)
#define RGF_SPI_FIFO_RD_PTR_ADDR          (0x880098)
#define RGF_SPI_FIFO_BASE_ADDR_ADDR       (0x880094)
#define RGF_SPI_CONTROL_ADDR              (0x880090)
#define RGF_SPI_CONFIG_ADDR               (0x88008C)

#define SPIS_TRNS_LEN_REG_ADDR          (0x50)
#define ADDR_WIDTH                      (3)
#define DUMMY_BYTES_WIDTH               (4)
#define OPCODE_WIDTH                    (1)
#define SPIS_SANITY_REG_ADDR            (0x0)
#define SPIS_SANITY_REG_VAL             (0xDEADBEEF)
#define SPIS_CFG_REG_ADDR               (0xC)
#define JTAG_ID_REG_ADDR                (0x880000)
#define JTAG_ID                         (0x1007E0E1)
/* optimized configuration with 4 dummy bytes */
#define SPIS_CONFIG_REG_OPT_VAL         (0x44200800)
#define SPIS_EXTENDED_RESET_COMMAND_LEN (225)

#define MAX_SPI_READ_CHUNKS (10)
#define SPI_MIN_TRANSACTION_SIZE (512)
#define SPI_MAX_TRANSACTION_SIZE (8*1024)
#define SPI_CMD_TRANSACTION_SIZE (512)
#define SPI_BUFFER_SIZE (SPI_MAX_TRANSACTION_SIZE + OPCODE_WIDTH + \
			 ADDR_WIDTH + DUMMY_BYTES_WIDTH)
#define SPI_CMD_BUFFER_SIZE (SPI_CMD_TRANSACTION_SIZE + OPCODE_WIDTH + \
			     ADDR_WIDTH + DUMMY_BYTES_WIDTH)

#define INT_FW_READY          BIT(24)
#define INT_DATA_READY        BIT(25)
#define INT_FIFO_READY        BIT(26)
#define INT_DONT_DEASSERT     BIT(27)
#define INT_SYSASSERT         BIT(29)
#define INT_DEEP_SLEEP_EXIT   BIT(30)
union user_rgf_spi_status {
	struct {
		u16 fill_level:16;
		int reserved1:3;
		u8 spi_fifo_thr_status:1;
		u8 spi_fifo_empty_status:1;
		u8 spi_fifo_full_status:1;
		u8 spi_fifo_underrun_status:1;
		u8 spi_fifo_overrun_status:1;

		/* mbox_outb */
		u8 int_fw_ready:1; /* FW MBOX ready */
		u8 int_data_ready:1; /* data available on FIFO */
		u8 int_fifo_ready:1; /* FIFO status update */
		u8 int_dont_deassert:1; /* Don't deassert DRI */
		u8 reserved3:1;
		u8 int_sysassert:1; /* SYSASSERT occurred */
		u8 int_deep_sleep_exit:1;
		u8 reserved4:1;
	} __packed b;
	u32 v;
} __packed;

union user_rgf_spi_mbox_inb {
	struct {
		u8 mode:4;
		u8 channel_request:4;
		u32 reserved:23;
		u8 deassert_dri:1;
	}  __packed b;
	u32 v;
} __packed;

union rgf_spi_config {
	struct {
		u16 size:16;
		u8 reserved1:8;
		u8 mbox_auto_clear_disable:1;
		u8 status_auto_clear_disable:1;
		u8 reserved2:5;
		u8 enable:1;
	} __packed b;
	u32 v;
} __packed;

union rgf_spi_control {
	struct {
		u8 read_ptr_clear:1;
		u8 fill_level_clear:1;
		u8 thresh_reach_clear:1;
		u8 status_field_clear:1;
		u8 mbox_field_clear:1;
		u32 reserved:27;
	} __packed b;
	u32 v;
} __packed;

struct cir_data {
	struct circ_buf b;
	u32 size_bytes;
	struct mutex lock;
};

struct spi_fifo {
	u32 wr_ptr;
	u32 rd_ptr;
	u32 base_addr;
	union rgf_spi_config config;
	union rgf_spi_control control;
};

/**
 * State machine states
 * TODO: Document states
 */
enum wigig_sensing_stm_e {
	WIGIG_SENSING_STATE_MIN = 0,
	WIGIG_SENSING_STATE_INITIALIZED,
	WIGIG_SENSING_STATE_READY_STOPPED,
	WIGIG_SENSING_STATE_SEARCH,
	WIGIG_SENSING_STATE_FACIAL,
	WIGIG_SENSING_STATE_GESTURE,
	WIGIG_SENSING_STATE_CUSTOM,
	WIGIG_SENSING_STATE_GET_PARAMS,
	WIGIG_SENSING_STATE_SYS_ASSERT,
	WIGIG_SENSING_STATE_MAX,
};

struct wigig_sensing_stm {
	bool auto_recovery;
	bool fw_is_ready;
	bool spi_malfunction;
	bool spi_ready;
	bool waiting_for_deep_sleep_exit;
	bool waiting_for_deep_sleep_exit_first_pass;
	bool burst_size_ready;
	bool change_mode_in_progress;
	enum wigig_sensing_stm_e state;
	enum wigig_sensing_mode mode;
	u32 burst_size;
	u32 channel;
	u32 channel_request;
	enum wigig_sensing_stm_e state_request;
	enum wigig_sensing_mode mode_request;
};

enum spi_stats_meas {
	SPI_STATS_MEAS_MIN,
	SPI_STATS_MEAS_SANITY = SPI_STATS_MEAS_MIN,
	SPI_STATS_MEAS_DEASSERT,
	SPI_STATS_MEAS_DRI_PROC,
	SPI_STATS_MEAS_MBOX_FILL_STATUS,
	SPI_STATS_MEAS_CHANGE_MODE,
	SPI_STATS_MEAS_DATA_READY,
	SPI_STATS_MEAS_MAX,
};

#define SPI_STATS_MAX_NAME_LEN (20)
struct spi_stats {
	char name[SPI_STATS_MAX_NAME_LEN];
	atomic64_t min;
	atomic64_t max;
	atomic64_t acc;
	atomic_t num_meas;
	ktime_t start, delta;
};

struct wigig_sensing_ctx {
	dev_t wigig_sensing_dev;
	struct cdev cdev;
	struct class *class;
	struct device *dev;
	struct spi_device *spi_dev;
	struct dentry *debugfs_dent;

	 /* Locks */
	struct mutex ioctl_lock;
	struct mutex file_lock;
	struct mutex spi_lock;
	struct mutex dri_lock;
	wait_queue_head_t cmd_wait_q;
	wait_queue_head_t data_wait_q;

	/* DRI */
	struct gpio_desc *dri_gpio;
	int dri_irq;
	bool opened;

	/* Memory buffers for SPI transactions */
	u8 *tx_buf;
	u8 *rx_buf;
	u8 *cmd_buf;
	u8 *cmd_reply_buf;

	/* SPI FIFO parameters */
	struct spi_fifo spi_fifo;
	struct wigig_sensing_stm stm;
	u32 last_read_length;
	union user_rgf_spi_mbox_inb inb_cmd;
	u32 spi_transaction_size;

	/* CIR buffer */
	struct cir_data cir_data;
	u8 *temp_buffer;
	bool event_pending;
	DECLARE_KFIFO(events_fifo, enum wigig_sensing_event, 8);
	u32 dropped_bursts;

	/* Statistics */
	struct spi_stats spi_stats[SPI_STATS_MEAS_MAX];
};

#endif /* __WIGIG_SENSING_H__ */

