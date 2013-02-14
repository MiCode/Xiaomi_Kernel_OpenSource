/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

/*
 * SDIO-Abstraction-Layer Module.
 *
 * To be used with Qualcomm's SDIO-Client connected to this host.
 */
#include "sdio_al_private.h"

#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/spinlock.h>

#include <mach/dma.h>
#include <mach/gpio.h>
#include <mach/subsystem_notif.h>

#include "../../../drivers/mmc/host/msm_sdcc.h"

/**
 *  Func#0 has SDIO standard registers
 *  Func#1 is for Mailbox.
 *  Functions 2..7 are for channels.
 *  Currently only functions 2..5 are active due to SDIO-Client
 *  number of pipes.
 *
 */
#define SDIO_AL_MAX_CHANNELS 6

/** Func 1..5 */
#define SDIO_AL_MAX_FUNCS    (SDIO_AL_MAX_CHANNELS+1)
#define SDIO_AL_WAKEUP_FUNC  6

/** Number of SDIO-Client pipes */
#define SDIO_AL_MAX_PIPES    16
#define SDIO_AL_ACTIVE_PIPES 8

/** CMD53/CMD54 Block size */
#define SDIO_AL_BLOCK_SIZE   256

/** Func#1 hardware Mailbox base address	 */
#define HW_MAILBOX_ADDR			0x1000

/** Func#1 peer sdioc software version.
 *  The header is duplicated also to the mailbox of the other
 *  functions. It can be used before other functions are enabled. */
#define SDIOC_SW_HEADER_ADDR		0x0400

/** Func#2..7 software Mailbox base address at 16K */
#define SDIOC_SW_MAILBOX_ADDR			0x4000

/** Some Mailbox registers address, written by host for
 control */
#define PIPES_THRESHOLD_ADDR		0x01000

#define PIPES_0_7_IRQ_MASK_ADDR 	0x01048

#define PIPES_8_15_IRQ_MASK_ADDR	0x0104C

#define FUNC_1_4_MASK_IRQ_ADDR		0x01040
#define FUNC_5_7_MASK_IRQ_ADDR		0x01044
#define FUNC_1_4_USER_IRQ_ADDR		0x01050
#define FUNC_5_7_USER_IRQ_ADDR		0x01054

#define EOT_PIPES_ENABLE		0x00

/** Maximum read/write data available is SDIO-Client limitation */
#define MAX_DATA_AVAILABLE   		(16*1024)
#define INVALID_DATA_AVAILABLE  	(0x8000)

/** SDIO-Client HW threshold to generate interrupt to the
 *  SDIO-Host on write available bytes.
 */
#define DEFAULT_WRITE_THRESHOLD 	(1024)

/** SDIO-Client HW threshold to generate interrupt to the
 *  SDIO-Host on read available bytes, for streaming (non
 *  packet) rx data.
 */
#define DEFAULT_READ_THRESHOLD  	(1024)
#define LOW_LATENCY_THRESHOLD		(1)

/* Extra bytes to ensure getting the rx threshold interrupt on stream channels
   when restoring the threshold after sleep */
#define THRESHOLD_CHANGE_EXTRA_BYTES (100)

/** SW threshold to trigger reading the mailbox. */
#define DEFAULT_MIN_WRITE_THRESHOLD 	(1024)
#define DEFAULT_MIN_WRITE_THRESHOLD_STREAMING	(1600)

#define THRESHOLD_DISABLE_VAL  		(0xFFFFFFFF)

/** Mailbox polling time for packet channels */
#define DEFAULT_POLL_DELAY_MSEC		10
/** Mailbox polling time for streaming channels */
#define DEFAULT_POLL_DELAY_NOPACKET_MSEC 30

/** The SDIO-Client prepares N buffers of size X per Tx pipe.
 *  Even when the transfer fills a partial buffer,
 *  that buffer becomes unusable for the next transfer. */
#define DEFAULT_PEER_TX_BUF_SIZE	(128)

#define ROUND_UP(x, n) (((x + n - 1) / n) * n)

/** Func#2..7 FIFOs are r/w via
 sdio_readsb() & sdio_writesb(),when inc_addr=0 */
#define PIPE_RX_FIFO_ADDR   0x00
#define PIPE_TX_FIFO_ADDR   0x00

/** Inactivity time to go to sleep in mseconds */
#define INACTIVITY_TIME_MSEC 30
#define INITIAL_INACTIVITY_TIME_MSEC 5000

/** Context validity check */
#define SDIO_AL_SIGNATURE 0xAABBCCDD

/* Vendor Specific Command */
#define SD_IO_RW_EXTENDED_QCOM 54

#define TIME_TO_WAIT_US 500
#define SDIO_CLOSE_FLUSH_TIMEOUT_MSEC   (10000)
#define RX_FLUSH_BUFFER_SIZE (16*1024)

#define SDIO_TEST_POSTFIX "_TEST"

#define DATA_DEBUG(x, y...)						\
	do {								\
		if (sdio_al->debug.debug_data_on)			\
			pr_info(y);					\
		sdio_al_log(x, y);					\
	} while (0)

#define LPM_DEBUG(x, y...)						\
	do {								\
		if (sdio_al->debug.debug_lpm_on)			\
			pr_info(y);					\
		sdio_al_log(x, y);					\
	} while (0)

#define sdio_al_loge(x, y...)						\
	do {								\
		pr_err(y);						\
		sdio_al_log(x, y);					\
	} while (0)

#define sdio_al_logi(x, y...)						\
	do {								\
		pr_info(y);						\
		sdio_al_log(x, y);					\
	} while (0)

#define CLOSE_DEBUG(x, y...)						\
	do {								\
		if (sdio_al->debug.debug_close_on)			\
			pr_info(y);					\
		sdio_al_log(x, y);					\
	} while (0)

/* The index of the SDIO card used for the sdio_al_dloader */
#define SDIO_BOOTLOADER_CARD_INDEX 1


/* SDIO card state machine */
enum sdio_al_device_state {
	CARD_INSERTED,
	CARD_REMOVED,
	MODEM_RESTART
};

struct sdio_al_debug {
	u8 debug_lpm_on;
	u8 debug_data_on;
	u8 debug_close_on;
	struct dentry *sdio_al_debug_root;
	struct dentry *sdio_al_debug_lpm_on;
	struct dentry *sdio_al_debug_data_on;
	struct dentry *sdio_al_debug_close_on;
	struct dentry *sdio_al_debug_info;
	struct dentry *sdio_al_debug_log_buffers[MAX_NUM_OF_SDIO_DEVICES + 1];
};

/* Polling time for the inactivity timer for devices that doesn't have
 * a streaming channel
 */
#define SDIO_AL_POLL_TIME_NO_STREAMING 30

#define CHAN_TO_FUNC(x) ((x) + 2 - 1)

/**
 *  Mailbox structure.
 *  The Mailbox is located on the SDIO-Client Function#1.
 *  The mailbox size is 128 bytes, which is one block.
 *  The mailbox allows the host ton:
 *  1. Get the number of available bytes on the pipes.
 *  2. Enable/Disable SDIO-Client interrupt, related to pipes.
 *  3. Set the Threshold for generating interrupt.
 *
 */
struct sdio_mailbox {
	u32 pipe_bytes_threshold[SDIO_AL_MAX_PIPES]; /* Addr 0x1000 */

	/* Mask USER interrupts generated towards host - Addr 0x1040 */
	u32 mask_irq_func_1:8; /* LSB */
	u32 mask_irq_func_2:8;
	u32 mask_irq_func_3:8;
	u32 mask_irq_func_4:8;

	u32 mask_irq_func_5:8;
	u32 mask_irq_func_6:8;
	u32 mask_irq_func_7:8;
	u32 mask_mutex_irq:8;

	/* Mask PIPE interrupts generated towards host - Addr 0x1048 */
	u32 mask_eot_pipe_0_7:8;
	u32 mask_thresh_above_limit_pipe_0_7:8;
	u32 mask_overflow_pipe_0_7:8;
	u32 mask_underflow_pipe_0_7:8;

	u32 mask_eot_pipe_8_15:8;
	u32 mask_thresh_above_limit_pipe_8_15:8;
	u32 mask_overflow_pipe_8_15:8;
	u32 mask_underflow_pipe_8_15:8;

	/* Status of User interrupts generated towards host - Addr 0x1050 */
	u32 user_irq_func_1:8;
	u32 user_irq_func_2:8;
	u32 user_irq_func_3:8;
	u32 user_irq_func_4:8;

	u32 user_irq_func_5:8;
	u32 user_irq_func_6:8;
	u32 user_irq_func_7:8;
	u32 user_mutex_irq:8;

	/* Status of PIPE interrupts generated towards host */
	/* Note: All sources are cleared once they read. - Addr 0x1058 */
	u32 eot_pipe_0_7:8;
	u32 thresh_above_limit_pipe_0_7:8;
	u32 overflow_pipe_0_7:8;
	u32 underflow_pipe_0_7:8;

	u32 eot_pipe_8_15:8;
	u32 thresh_above_limit_pipe_8_15:8;
	u32 overflow_pipe_8_15:8;
	u32 underflow_pipe_8_15:8;

	u16 pipe_bytes_avail[SDIO_AL_MAX_PIPES];
};

/** Track pending Rx Packet size */
struct rx_packet_size {
	u32 size; /* in bytes */
	struct list_head	list;
};

#define PEER_SDIOC_SW_MAILBOX_SIGNATURE 0xFACECAFE
#define PEER_SDIOC_SW_MAILBOX_UT_SIGNATURE 0x5D107E57
#define PEER_SDIOC_SW_MAILBOX_BOOT_SIGNATURE 0xDEADBEEF

/* Allow support in old sdio version */
#define PEER_SDIOC_OLD_VERSION_MAJOR	0x0002
#define INVALID_SDIO_CHAN		0xFF

/**
 * Peer SDIO-Client software header.
 */
struct peer_sdioc_sw_header {
	u32 signature;
	u32 version;
	u32 max_channels;
	char channel_names[SDIO_AL_MAX_CHANNELS][PEER_CHANNEL_NAME_SIZE];
	u32 reserved[23];
};

struct peer_sdioc_boot_sw_header {
	u32 signature;
	u32 version;
	u32 boot_ch_num;
	u32 reserved[29]; /* 32 - previous fields */
};

/**
 * Peer SDIO-Client software mailbox.
 */
struct peer_sdioc_sw_mailbox {
	struct peer_sdioc_sw_header sw_header;
	struct peer_sdioc_channel_config ch_config[SDIO_AL_MAX_CHANNELS];
};

#define SDIO_AL_DEBUG_LOG_SIZE 3000
struct sdio_al_local_log {
	char buffer[SDIO_AL_DEBUG_LOG_SIZE];
	unsigned int buf_cur_pos;
	spinlock_t log_lock;
};

#define SDIO_AL_DEBUG_TMP_LOG_SIZE 250
static int sdio_al_log(struct sdio_al_local_log *, const char *fmt, ...);

/**
 *  SDIO Abstraction Layer driver context.
 *
 *  @pdata -
 *  @debug -
 *  @devices - an array of the the devices claimed by sdio_al
 *  @unittest_mode - a flag to indicate if sdio_al is in
 *		   unittest mode
 *  @bootloader_dev - the device which is used for the
 *                 bootloader
 *  @subsys_notif_handle - handle for modem restart
 *                 notifications
 *
 */
struct sdio_al {
	struct sdio_al_local_log gen_log;
	struct sdio_al_local_log device_log[MAX_NUM_OF_SDIO_DEVICES];
	struct sdio_al_platform_data *pdata;
	struct sdio_al_debug debug;
	struct sdio_al_device *devices[MAX_NUM_OF_SDIO_DEVICES];
	int unittest_mode;
	struct sdio_al_device *bootloader_dev;
	void *subsys_notif_handle;
	int sdioc_major;
	int skip_print_info;
};

struct sdio_al_work {
	struct work_struct work;
	struct sdio_al_device *sdio_al_dev;
};


/**
 *  SDIO Abstraction Layer device context.
 *
 *  @card - card claimed.
 *
 *  @mailbox - A shadow of the SDIO-Client mailbox.
 *
 *  @channel - Channels context.
 *
 *  @workqueue - workqueue to read the mailbox and handle
 *     pending requests. Reading the mailbox should not happen
 *     in interrupt context.
 *
 *  @work - work to submit to workqueue.
 *
 *  @is_ready - driver is ready.
 *
 *  @ask_mbox - Flag to request reading the mailbox,
 *					  for different reasons.
 *
 *  @wake_lock - Lock when can't sleep.
 *
 *  @lpm_chan - Channel to use for LPM (low power mode)
 *            communication.
 *
 *  @is_ok_to_sleep - Mark if driver is OK with going to sleep
 * 			(no pending transactions).
 *
 *  @inactivity_time - time allowed to be in inactivity before
 * 		going to sleep
 *
 *  @timer - timer to use for polling the mailbox.
 *
 *  @poll_delay_msec - timer delay for polling the mailbox.
 *
 *  @is_err - error detected.
 *
 *  @signature - Context Validity Check.
 *
 *  @flashless_boot_on - flag to indicate if sdio_al is in
 *    flshless boot mode
 *
 */
struct sdio_al_device {
	struct sdio_al_local_log *dev_log;
	struct mmc_card *card;
	struct mmc_host *host;
	struct sdio_mailbox *mailbox;
	struct sdio_channel channel[SDIO_AL_MAX_CHANNELS];

	struct peer_sdioc_sw_header *sdioc_sw_header;
	struct peer_sdioc_boot_sw_header *sdioc_boot_sw_header;

	struct workqueue_struct *workqueue;
	struct sdio_al_work sdio_al_work;
	struct sdio_al_work boot_work;

	int is_ready;

	wait_queue_head_t   wait_mbox;
	int ask_mbox;
	int bootloader_done;

	struct wake_lock wake_lock;
	int lpm_chan;
	int is_ok_to_sleep;
	unsigned long inactivity_time;

	struct timer_list timer;
	u32 poll_delay_msec;
	int is_timer_initialized;

	int is_err;

	u32 signature;

	unsigned int is_suspended;

	int flashless_boot_on;
	int ch_close_supported;
	int state;
	int (*lpm_callback)(void *, int);

	int print_after_interrupt;

	u8 *rx_flush_buf;
};

/*
 * Host operation:
 *   lower 16bits are operation code
 *   upper 16bits are operation state
 */
#define PEER_OPERATION(op_code , op_state) ((op_code) | ((op_state) << 16))
#define GET_PEER_OPERATION_CODE(op) ((op) & 0xffff)
#define GET_PEER_OPERATION_STATE(op) ((op) >> 16)

enum peer_op_code {
	PEER_OP_CODE_CLOSE = 1
};

enum peer_op_state {
	PEER_OP_STATE_INIT = 0,
	PEER_OP_STATE_START = 1
};


/*
 * On the kernel command line specify
 * sdio_al.debug_lpm_on=1 to enable the LPM debug messages
 * By default the LPM debug messages are turned off
 */
static int debug_lpm_on;
module_param(debug_lpm_on, int, 0);

/*
 * On the kernel command line specify
 * sdio_al.debug_data_on=1 to enable the DATA debug messages
 * By default the DATA debug messages are turned off
 */
static int debug_data_on;
module_param(debug_data_on, int, 0);

/*
 * Enables / disables open close debug messages
 */
static int debug_close_on = 1;
module_param(debug_close_on, int, 0);

/** The driver context */
static struct sdio_al *sdio_al;

/* Static functions declaration */
static int enable_eot_interrupt(struct sdio_al_device *sdio_al_dev,
				int pipe_index, int enable);
static int enable_threshold_interrupt(struct sdio_al_device *sdio_al_dev,
				      int pipe_index, int enable);
static void sdio_func_irq(struct sdio_func *func);
static void sdio_al_timer_handler(unsigned long data);
static int get_min_poll_time_msec(struct sdio_al_device *sdio_al_dev);
static u32 check_pending_rx_packet(struct sdio_channel *ch, u32 eot);
static u32 remove_handled_rx_packet(struct sdio_channel *ch);
static int set_pipe_threshold(struct sdio_al_device *sdio_al_dev,
			      int pipe_index, int threshold);
static int sdio_al_wake_up(struct sdio_al_device *sdio_al_dev,
			   u32 not_from_int, struct sdio_channel *ch);
static int sdio_al_client_setup(struct sdio_al_device *sdio_al_dev);
static int enable_mask_irq(struct sdio_al_device *sdio_al_dev,
			   int func_num, int enable, u8 bit_offset);
static int sdio_al_enable_func_retry(struct sdio_func *func, const char *name);
static void sdio_al_print_info(void);
static int sdio_read_internal(struct sdio_channel *ch, void *data, int len);
static int sdio_read_from_closed_ch(struct sdio_channel *ch, int len);
static void stop_and_del_timer(struct sdio_al_device *sdio_al_dev);

#define SDIO_AL_ERR(func)					\
	do {							\
		printk_once(KERN_ERR MODULE_NAME		\
			":In Error state, ignore %s\n",		\
			func);					\
		sdio_al_print_info();				\
	} while (0)

#ifdef CONFIG_DEBUG_FS
static int debug_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_info_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	sdio_al_print_info();
	return 1;
}

const struct file_operations debug_info_ops = {
	.open = debug_info_open,
	.write = debug_info_write,
};

struct debugfs_blob_wrapper sdio_al_dbgfs_log[MAX_NUM_OF_SDIO_DEVICES + 1];

/*
*
* Trigger on/off for debug messages
* for trigger off the data messages debug level use:
* echo 0 > /sys/kernel/debugfs/sdio_al/debug_data_on
* for trigger on the data messages debug level use:
* echo 1 > /sys/kernel/debugfs/sdio_al/debug_data_on
* for trigger off the lpm messages debug level use:
* echo 0 > /sys/kernel/debugfs/sdio_al/debug_lpm_on
* for trigger on the lpm messages debug level use:
* echo 1 > /sys/kernel/debugfs/sdio_al/debug_lpm_on
*/
static int sdio_al_debugfs_init(void)
{
	int i, blob_errs = 0;

	sdio_al->debug.sdio_al_debug_root = debugfs_create_dir("sdio_al", NULL);
	if (!sdio_al->debug.sdio_al_debug_root)
		return -ENOENT;

	sdio_al->debug.sdio_al_debug_lpm_on = debugfs_create_u8("debug_lpm_on",
					S_IRUGO | S_IWUGO,
					sdio_al->debug.sdio_al_debug_root,
					&sdio_al->debug.debug_lpm_on);

	sdio_al->debug.sdio_al_debug_data_on = debugfs_create_u8(
					"debug_data_on",
					S_IRUGO | S_IWUGO,
					sdio_al->debug.sdio_al_debug_root,
					&sdio_al->debug.debug_data_on);

	sdio_al->debug.sdio_al_debug_close_on = debugfs_create_u8(
					"debug_close_on",
					S_IRUGO | S_IWUGO,
					sdio_al->debug.sdio_al_debug_root,
					&sdio_al->debug.debug_close_on);

	sdio_al->debug.sdio_al_debug_info = debugfs_create_file(
					"sdio_debug_info",
					S_IRUGO | S_IWUGO,
					sdio_al->debug.sdio_al_debug_root,
					NULL,
					&debug_info_ops);

	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES; ++i) {
		char temp[18];

		scnprintf(temp, 18, "sdio_al_log_dev_%d", i + 1);
		sdio_al->debug.sdio_al_debug_log_buffers[i] =
			debugfs_create_blob(temp,
					S_IRUGO | S_IWUGO,
					sdio_al->debug.sdio_al_debug_root,
					&sdio_al_dbgfs_log[i]);
	}

	sdio_al->debug.sdio_al_debug_log_buffers[MAX_NUM_OF_SDIO_DEVICES] =
			debugfs_create_blob("sdio_al_gen_log",
				S_IRUGO | S_IWUGO,
				sdio_al->debug.sdio_al_debug_root,
				&sdio_al_dbgfs_log[MAX_NUM_OF_SDIO_DEVICES]);

	for (i = 0; i < (MAX_NUM_OF_SDIO_DEVICES + 1); ++i) {
		if (!sdio_al->debug.sdio_al_debug_log_buffers[i]) {
			pr_err(MODULE_NAME ": Failed to create debugfs buffer"
				   " entry for "
				   "sdio_al->debug.sdio_al_debug_log_buffers[%d]",
				   i);
			blob_errs = 1;
		}
	}

	if (blob_errs) {
		for (i = 0; i < (MAX_NUM_OF_SDIO_DEVICES + 1); ++i)
			if (sdio_al->debug.sdio_al_debug_log_buffers[i])
				debugfs_remove(
					sdio_al->
					debug.sdio_al_debug_log_buffers[i]);
	}


	if ((!sdio_al->debug.sdio_al_debug_data_on) &&
	    (!sdio_al->debug.sdio_al_debug_lpm_on) &&
	    (!sdio_al->debug.sdio_al_debug_close_on) &&
	    (!sdio_al->debug.sdio_al_debug_info) &&
		blob_errs) {
		debugfs_remove(sdio_al->debug.sdio_al_debug_root);
		sdio_al->debug.sdio_al_debug_root = NULL;
		return -ENOENT;
	}

	sdio_al_dbgfs_log[MAX_NUM_OF_SDIO_DEVICES].data =
						sdio_al->gen_log.buffer;
	sdio_al_dbgfs_log[MAX_NUM_OF_SDIO_DEVICES].size =
						SDIO_AL_DEBUG_LOG_SIZE;

	return 0;
}

static void sdio_al_debugfs_cleanup(void)
{
	int i;

	debugfs_remove(sdio_al->debug.sdio_al_debug_lpm_on);
	debugfs_remove(sdio_al->debug.sdio_al_debug_data_on);
	debugfs_remove(sdio_al->debug.sdio_al_debug_close_on);
	debugfs_remove(sdio_al->debug.sdio_al_debug_info);

	for (i = 0; i < (MAX_NUM_OF_SDIO_DEVICES + 1); ++i)
		debugfs_remove(sdio_al->debug.sdio_al_debug_log_buffers[i]);

	debugfs_remove(sdio_al->debug.sdio_al_debug_root);
}
#endif

static int sdio_al_log(struct sdio_al_local_log *log, const char *fmt, ...)
{
	va_list args;
	int r;
	char *tp, *log_buf;
	unsigned int *log_cur_pos;
	struct timeval kt;
	unsigned long flags;
	static char sdio_al_log_tmp[SDIO_AL_DEBUG_TMP_LOG_SIZE];

	spin_lock_irqsave(&log->log_lock, flags);

	kt = ktime_to_timeval(ktime_get());
	r = scnprintf(sdio_al_log_tmp, SDIO_AL_DEBUG_TMP_LOG_SIZE,
			"[%8ld.%6ld] ", kt.tv_sec, kt.tv_usec);

	va_start(args, fmt);
	r += vscnprintf(&sdio_al_log_tmp[r], (SDIO_AL_DEBUG_TMP_LOG_SIZE - r),
			fmt, args);
	va_end(args);

	log_buf = log->buffer;
	log_cur_pos = &(log->buf_cur_pos);

	for (tp = sdio_al_log_tmp; tp < (sdio_al_log_tmp + r); tp++) {
		log_buf[(*log_cur_pos)++] = *tp;
		if ((*log_cur_pos) == SDIO_AL_DEBUG_LOG_SIZE)
			*log_cur_pos = 0;
	}

	spin_unlock_irqrestore(&log->log_lock, flags);

	return r;
}

static int sdio_al_verify_func1(struct sdio_al_device *sdio_al_dev,
				char const *func)
{
	if (sdio_al_dev == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
				"sdio_al_dev\n", func);
		return -ENODEV;
	}

	if (sdio_al_dev->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: Invalid "
				"signature\n", func);
		return -ENODEV;
	}

	if (!sdio_al_dev->card) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: NULL "
				"card\n", func);
		return -ENODEV;
	}
	if (!sdio_al_dev->card->sdio_func[0]) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: NULL "
				"func1\n", func);
		return -ENODEV;
	}
	return 0;
}

static int sdio_al_claim_mutex(struct sdio_al_device *sdio_al_dev,
			       char const *func)
{
	if (!sdio_al_dev) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
					"device\n", func);
		return -ENODEV;
	}

	if (sdio_al_dev->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: Invalid "
					"device signature\n", func);
		return -ENODEV;
	}

	if (!sdio_al_dev->host) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: NULL "
					"host\n", func);
		return -ENODEV;
	}

	mmc_claim_host(sdio_al_dev->host);

	return 0;
}

static int sdio_al_release_mutex(struct sdio_al_device *sdio_al_dev,
			       char const *func)
{
	if (!sdio_al_dev) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
					"device\n", func);
		return -ENODEV;
	}

	if (sdio_al_dev->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: Invalid "
					"device signature\n", func);
		return -ENODEV;
	}

	if (!sdio_al_dev->host) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: NULL "
					"host\n", func);
		return -ENODEV;
	}

	mmc_release_host(sdio_al_dev->host);

	return 0;
}

static int sdio_al_claim_mutex_and_verify_dev(
	struct sdio_al_device *sdio_al_dev,
	char const *func)
{
	if (sdio_al_claim_mutex(sdio_al_dev, __func__))
		return -ENODEV;

	if (sdio_al_dev->state != CARD_INSERTED) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": %s: Invalid "
				"device state %d\n", func, sdio_al_dev->state);
		sdio_al_release_mutex(sdio_al_dev, __func__);
		return -ENODEV;
	}

	return 0;
}

static void sdio_al_get_into_err_state(struct sdio_al_device *sdio_al_dev)
{
	if ((!sdio_al) || (!sdio_al_dev))
		return;

	sdio_al_dev->is_err = true;
	sdio_al->debug.debug_data_on = 0;
	sdio_al->debug.debug_lpm_on = 0;
	sdio_al_print_info();
}

void sdio_al_register_lpm_cb(void *device_handle,
				       int(*lpm_callback)(void *, int))
{
	struct sdio_al_device *sdio_al_dev =
		(struct sdio_al_device *) device_handle;

	if (!sdio_al_dev) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s - "
				"device_handle is NULL\n", __func__);
		return;
	}

	if (lpm_callback) {
		sdio_al_dev->lpm_callback = lpm_callback;
		lpm_callback((void *)sdio_al_dev,
					   sdio_al_dev->is_ok_to_sleep);
	}

	LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ": %s - device %d "
			"registered for wakeup callback\n", __func__,
			sdio_al_dev->host->index);
}

void sdio_al_unregister_lpm_cb(void *device_handle)
{
	struct sdio_al_device *sdio_al_dev =
		(struct sdio_al_device *) device_handle;

	if (!sdio_al_dev) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s - "
				"device_handle is NULL\n", __func__);
		return;
	}

	sdio_al_dev->lpm_callback = NULL;
	LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ": %s - device %d "
		"unregister for wakeup callback\n", __func__,
		sdio_al_dev->host->index);
}

static void sdio_al_vote_for_sleep(struct sdio_al_device *sdio_al_dev,
				   int is_vote_for_sleep)
{
	pr_debug(MODULE_NAME ": %s()", __func__);

	if (!sdio_al_dev) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s - sdio_al_dev"
				" is NULL\n", __func__);
		return;
	}

	if (is_vote_for_sleep) {
		pr_debug(MODULE_NAME ": %s - sdio vote for Sleep", __func__);
		wake_unlock(&sdio_al_dev->wake_lock);
	} else {
		pr_debug(MODULE_NAME ": %s - sdio vote against sleep",
			  __func__);
		wake_lock(&sdio_al_dev->wake_lock);
	}

	if (sdio_al_dev->lpm_callback != NULL) {
		LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ": %s - "
				"is_vote_for_sleep=%d for card#%d, "
				"calling callback...", __func__,
				is_vote_for_sleep,
				sdio_al_dev->host->index);
		sdio_al_dev->lpm_callback((void *)sdio_al_dev,
					   is_vote_for_sleep);
	}
}

/**
 *  Write SDIO-Client lpm information
 *  Should only be called with host claimed.
 */
static int write_lpm_info(struct sdio_al_device *sdio_al_dev)
{
	struct sdio_func *lpm_func = NULL;
	int offset = offsetof(struct peer_sdioc_sw_mailbox, ch_config)+
		sizeof(struct peer_sdioc_channel_config) *
		sdio_al_dev->lpm_chan+
		offsetof(struct peer_sdioc_channel_config, is_host_ok_to_sleep);
	int ret;

	if (sdio_al_dev->lpm_chan == INVALID_SDIO_CHAN) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Invalid "
				"lpm_chan for card %d\n",
				sdio_al_dev->host->index);
		return -EINVAL;
	}

	if (!sdio_al_dev->card ||
		!sdio_al_dev->card->sdio_func[sdio_al_dev->lpm_chan+1]) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": NULL card or lpm_func\n");
		return -ENODEV;
	}
	lpm_func = sdio_al_dev->card->sdio_func[sdio_al_dev->lpm_chan+1];

	pr_debug(MODULE_NAME ":write_lpm_info is_ok_to_sleep=%d, device %d\n",
		 sdio_al_dev->is_ok_to_sleep,
		 sdio_al_dev->host->index);

	ret = sdio_memcpy_toio(lpm_func, SDIOC_SW_MAILBOX_ADDR+offset,
				&sdio_al_dev->is_ok_to_sleep, sizeof(u32));
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":failed to "
				"write lpm info for card %d\n",
				sdio_al_dev->host->index);
		return ret;
	}

	return 0;
}

/* Set inactivity counter to intial value to allow clients come up */
static inline void start_inactive_time(struct sdio_al_device *sdio_al_dev)
{
	sdio_al_dev->inactivity_time = jiffies +
		msecs_to_jiffies(INITIAL_INACTIVITY_TIME_MSEC);
}

static inline void restart_inactive_time(struct sdio_al_device *sdio_al_dev)
{
	sdio_al_dev->inactivity_time = jiffies +
		msecs_to_jiffies(INACTIVITY_TIME_MSEC);
}

static inline int is_inactive_time_expired(struct sdio_al_device *sdio_al_dev)
{
	return time_after(jiffies, sdio_al_dev->inactivity_time);
}


static int is_user_irq_enabled(struct sdio_al_device *sdio_al_dev,
			       int func_num)
{
	int ret = 0;
	struct sdio_func *func1;
	u32 user_irq = 0;
	u32 addr = 0;
	u32 offset = 0;
	u32 masked_user_irq = 0;

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return 0;
	func1 = sdio_al_dev->card->sdio_func[0];

	if (func_num < 4) {
		addr = FUNC_1_4_USER_IRQ_ADDR;
		offset = func_num * 8;
	} else {
		addr = FUNC_5_7_USER_IRQ_ADDR;
		offset = (func_num - 4) * 8;
	}

	user_irq = sdio_readl(func1, addr, &ret);
	if (ret) {
		pr_debug(MODULE_NAME ":read_user_irq fail\n");
		return 0;
	}

	masked_user_irq = (user_irq >> offset) && 0xFF;
	if (masked_user_irq == 0x1) {
		sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":user_irq "
				"enabled\n");
		return 1;
	}

	return 0;
}

static void sdio_al_sleep(struct sdio_al_device *sdio_al_dev,
			  struct mmc_host *host)
{
	int i;

	/* Go to sleep */
	pr_debug(MODULE_NAME  ":Inactivity timer expired."
		" Going to sleep\n");
	/* Stop mailbox timer */
	stop_and_del_timer(sdio_al_dev);
	/* Make sure we get interrupt for non-packet-mode right away */
	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
		struct sdio_channel *ch = &sdio_al_dev->channel[i];
		if ((ch->state != SDIO_CHANNEL_STATE_OPEN) &&
		    (ch->state != SDIO_CHANNEL_STATE_CLOSED)) {
			pr_debug(MODULE_NAME  ":continue for channel %s in"
					" state %d\n", ch->name, ch->state);
			continue;
		}
		if (ch->is_packet_mode == false) {
			ch->read_threshold = LOW_LATENCY_THRESHOLD;
			set_pipe_threshold(sdio_al_dev,
					   ch->rx_pipe_index,
					   ch->read_threshold);
		}
	}
	/* Prevent modem to go to sleep until we get the PROG_DONE on
	   the dummy CMD52 */
	msmsdcc_set_pwrsave(sdio_al_dev->host, 0);
	/* Mark HOST_OK_TOSLEEP */
	sdio_al_dev->is_ok_to_sleep = 1;
	write_lpm_info(sdio_al_dev);

	msmsdcc_lpm_enable(host);
	LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":Finished sleep sequence"
			" for card %d. Sleep now.\n",
		sdio_al_dev->host->index);
	/* Release wakelock */
	sdio_al_vote_for_sleep(sdio_al_dev, 1);
}


/**
 *  Read SDIO-Client Mailbox from Function#1.thresh_pipe
 *
 *  The mailbox contain the bytes available per pipe,
 *  and the End-Of-Transfer indication per pipe (if available).
 *
 * WARNING: Each time the Mailbox is read from the client, the
 * read_bytes_avail is incremented with another pending
 * transfer. Therefore, a pending rx-packet should be added to a
 * list before the next read of the mailbox.
 *
 * This function should run from a workqueue context since it
 * notifies the clients.
 *
 * This function assumes that sdio_al_claim_mutex was called before
 * calling it.
 *
 */
static int read_mailbox(struct sdio_al_device *sdio_al_dev, int from_isr)
{
	int ret;
	struct sdio_func *func1 = NULL;
	struct sdio_mailbox *mailbox = sdio_al_dev->mailbox;
	struct mmc_host *host = sdio_al_dev->host;
	u32 new_write_avail = 0;
	u32 old_write_avail = 0;
	u32 any_read_avail = 0;
	u32 any_write_pending = 0;
	int i;
	u32 rx_notify_bitmask = 0;
	u32 tx_notify_bitmask = 0;
	u32 eot_pipe = 0;
	u32 thresh_pipe = 0;
	u32 overflow_pipe = 0;
	u32 underflow_pipe = 0;
	u32 thresh_intr_mask = 0;
	int is_closing = 0;

	if (sdio_al_dev->is_err) {
		SDIO_AL_ERR(__func__);
		return 0;
	}

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return -ENODEV;
	func1 = sdio_al_dev->card->sdio_func[0];

	pr_debug(MODULE_NAME ":start %s from_isr = %d for card %d.\n"
		 , __func__, from_isr, sdio_al_dev->host->index);

	pr_debug(MODULE_NAME ":before sdio_memcpy_fromio.\n");
	memset(mailbox, 0, sizeof(struct sdio_mailbox));
	ret = sdio_memcpy_fromio(func1, mailbox,
			HW_MAILBOX_ADDR, sizeof(*mailbox));
	pr_debug(MODULE_NAME ":after sdio_memcpy_fromio.\n");
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Fail to read "
				"Mailbox for card %d, goto error state\n",
				sdio_al_dev->host->index);
		sdio_al_get_into_err_state(sdio_al_dev);
		goto exit_err;
	}

	eot_pipe =	(mailbox->eot_pipe_0_7) |
			(mailbox->eot_pipe_8_15<<8);
	thresh_pipe = 	(mailbox->thresh_above_limit_pipe_0_7) |
			(mailbox->thresh_above_limit_pipe_8_15<<8);

	overflow_pipe = (mailbox->overflow_pipe_0_7) |
			(mailbox->overflow_pipe_8_15<<8);
	underflow_pipe = mailbox->underflow_pipe_0_7 |
			(mailbox->underflow_pipe_8_15<<8);
	thresh_intr_mask =
		(mailbox->mask_thresh_above_limit_pipe_0_7) |
		(mailbox->mask_thresh_above_limit_pipe_8_15<<8);

	if (overflow_pipe || underflow_pipe)
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Mailbox ERROR "
				"overflow=0x%x, underflow=0x%x\n",
				overflow_pipe, underflow_pipe);

	/* In case of modem reset we would like to read the daya from the modem
	   to clear the interrupts but do not process it */
	if (sdio_al_dev->state != CARD_INSERTED) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":sdio_al_device"
				" (card %d) is in invalid state %d\n",
				sdio_al_dev->host->index,
				sdio_al_dev->state);
		return -ENODEV;
	}

	pr_debug(MODULE_NAME ":card %d: eot=0x%x, thresh=0x%x\n",
			sdio_al_dev->host->index,
			eot_pipe, thresh_pipe);

	/* Scan for Rx Packets available and update read available bytes */
	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
		struct sdio_channel *ch = &sdio_al_dev->channel[i];
		u32 old_read_avail;
		u32 read_avail;
		u32 new_packet_size = 0;

		if (ch->state == SDIO_CHANNEL_STATE_CLOSING)
			is_closing = true; /* used to prevent sleep */

		old_read_avail = ch->read_avail;
		read_avail = mailbox->pipe_bytes_avail[ch->rx_pipe_index];

		if ((ch->state == SDIO_CHANNEL_STATE_CLOSED) &&
			(read_avail > 0)) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				 ":%s: Invalid read_avail 0x%x, for CLOSED ch %s\n",
				 __func__, read_avail, ch->name);
			sdio_read_from_closed_ch(ch, read_avail);
		}
		if ((ch->state != SDIO_CHANNEL_STATE_OPEN) &&
		    (ch->state != SDIO_CHANNEL_STATE_CLOSING))
			continue;

		if (read_avail > INVALID_DATA_AVAILABLE) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				 ":Invalid read_avail 0x%x for pipe %d\n",
				 read_avail, ch->rx_pipe_index);
			continue;
		}
		any_read_avail |= read_avail | old_read_avail;
		ch->statistics.last_any_read_avail = any_read_avail;
		ch->statistics.last_read_avail = read_avail;
		ch->statistics.last_old_read_avail = old_read_avail;

		if (ch->is_packet_mode) {
			if ((eot_pipe & (1<<ch->rx_pipe_index)) &&
			    sdio_al_dev->print_after_interrupt) {
				LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME
					":Interrupt on ch %s, "
					"card %d", ch->name,
					sdio_al_dev->host->index);
			}
			new_packet_size = check_pending_rx_packet(ch, eot_pipe);
		} else {
			if ((thresh_pipe & (1<<ch->rx_pipe_index)) &&
			    sdio_al_dev->print_after_interrupt) {
				LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME
					":Interrupt on ch %s, "
					"card %d", ch->name,
					sdio_al_dev->host->index);
			}
			ch->read_avail = read_avail;

			/*
			 * Restore default thresh for non packet channels.
			 * in case it IS low latency channel then read_threshold
			 * and def_read_threshold are both
			 * LOW_LATENCY_THRESHOLD
			 */
			if ((ch->read_threshold != ch->def_read_threshold) &&
			    (read_avail >= ch->threshold_change_cnt)) {
				if (!ch->is_low_latency_ch) {
					ch->read_threshold =
						ch->def_read_threshold;
					set_pipe_threshold(sdio_al_dev,
							   ch->rx_pipe_index,
							   ch->read_threshold);
				}
			}
		}

		if ((ch->is_packet_mode) && (new_packet_size > 0)) {
			rx_notify_bitmask |= (1<<ch->num);
			ch->statistics.total_notifs++;
		}

		if ((!ch->is_packet_mode) && (ch->read_avail > 0) &&
		    (old_read_avail == 0)) {
			rx_notify_bitmask |= (1<<ch->num);
			ch->statistics.total_notifs++;
		}
	}
	sdio_al_dev->print_after_interrupt = 0;

	/* Update Write available */
	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
		struct sdio_channel *ch = &sdio_al_dev->channel[i];

		if ((ch->state != SDIO_CHANNEL_STATE_OPEN) &&
		    (ch->state != SDIO_CHANNEL_STATE_CLOSING))
			continue;

		new_write_avail = mailbox->pipe_bytes_avail[ch->tx_pipe_index];

		if (new_write_avail > INVALID_DATA_AVAILABLE) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				 ":Invalid write_avail 0x%x for pipe %d\n",
				 new_write_avail, ch->tx_pipe_index);
			continue;
		}

		old_write_avail = ch->write_avail;
		ch->write_avail = new_write_avail;

		if ((old_write_avail <= ch->min_write_avail) &&
			(new_write_avail >= ch->min_write_avail))
			tx_notify_bitmask |= (1<<ch->num);

		/* There is not enough write avail for this channel.
		   We need to keep reading mailbox to wait for the appropriate
		   write avail and cannot sleep. Ignore SMEM channel that has
		   only one direction. */
		if (strncmp(ch->name, "SDIO_SMEM", CHANNEL_NAME_SIZE))
			any_write_pending |=
			(new_write_avail < ch->ch_config.max_tx_threshold);
	}
	/* notify clients */
	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
		struct sdio_channel *ch = &sdio_al_dev->channel[i];

		if ((ch->state != SDIO_CHANNEL_STATE_OPEN) ||
				(ch->notify == NULL))
			continue;

		if (rx_notify_bitmask & (1<<ch->num))
			ch->notify(ch->priv,
					   SDIO_EVENT_DATA_READ_AVAIL);

		if (tx_notify_bitmask & (1<<ch->num))
			ch->notify(ch->priv,
					   SDIO_EVENT_DATA_WRITE_AVAIL);
	}


	if ((rx_notify_bitmask == 0) && (tx_notify_bitmask == 0) &&
	    !any_read_avail && !any_write_pending) {
		DATA_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":Nothing to "
				"Notify for card %d, is_closing=%d\n",
				sdio_al_dev->host->index, is_closing);
		if (is_closing)
			restart_inactive_time(sdio_al_dev);
		else if (is_inactive_time_expired(sdio_al_dev))
			sdio_al_sleep(sdio_al_dev, host);
	} else {
		DATA_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":Notify bitmask"
				" for card %d rx=0x%x, tx=0x%x.\n",
				sdio_al_dev->host->index,
				rx_notify_bitmask, tx_notify_bitmask);
		/* Restart inactivity timer if any activity on the channel */
		restart_inactive_time(sdio_al_dev);
	}

	pr_debug(MODULE_NAME ":end %s.\n", __func__);

exit_err:
	return ret;
}

/**
 *  Check pending rx packet when reading the mailbox.
 */
static u32 check_pending_rx_packet(struct sdio_channel *ch, u32 eot)
{
	u32 rx_pending;
	u32 rx_avail;
	u32 new_packet_size = 0;
	struct sdio_al_device *sdio_al_dev = ch->sdio_al_dev;


	if (sdio_al_dev == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": NULL sdio_al_dev"
				" for channel %s\n", ch->name);
		return -EINVAL;
	}

	mutex_lock(&ch->ch_lock);

	rx_pending = ch->rx_pending_bytes;
	rx_avail = sdio_al_dev->mailbox->pipe_bytes_avail[ch->rx_pipe_index];

	pr_debug(MODULE_NAME ":pipe %d of card %d rx_avail=0x%x, "
			     "rx_pending=0x%x\n",
	   ch->rx_pipe_index, sdio_al_dev->host->index, rx_avail,
		 rx_pending);


	/* new packet detected */
	if (eot & (1<<ch->rx_pipe_index)) {
		struct rx_packet_size *p = NULL;
		new_packet_size = rx_avail - rx_pending;

		if ((rx_avail <= rx_pending)) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": Invalid new packet size."
					" rx_avail=%d.\n", rx_avail);
			new_packet_size = 0;
			goto exit_err;
		}

		p = kzalloc(sizeof(*p), GFP_KERNEL);
		if (p == NULL) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": failed to allocate item for "
					"rx_pending list. rx_avail=%d, "
					"rx_pending=%d.\n",
					rx_avail, rx_pending);
			new_packet_size = 0;
			goto exit_err;
		}
		p->size = new_packet_size;
		/* Add new packet as last */
		list_add_tail(&p->list, &ch->rx_size_list_head);
		ch->rx_pending_bytes += new_packet_size;

		if (ch->read_avail == 0)
			ch->read_avail = new_packet_size;
	}

exit_err:
	mutex_unlock(&ch->ch_lock);

	return new_packet_size;
}



/**
 *  Remove first pending packet from the list.
 */
static u32 remove_handled_rx_packet(struct sdio_channel *ch)
{
	struct rx_packet_size *p = NULL;

	mutex_lock(&ch->ch_lock);

	ch->rx_pending_bytes -= ch->read_avail;

	if (!list_empty(&ch->rx_size_list_head)) {
		p = list_first_entry(&ch->rx_size_list_head,
			struct rx_packet_size, list);
		list_del(&p->list);
		kfree(p);
	} else {
		sdio_al_loge(ch->sdio_al_dev->dev_log, MODULE_NAME ":%s: ch "
				"%s: unexpected empty list!!\n",
				__func__, ch->name);
	}

	if (list_empty(&ch->rx_size_list_head))	{
		ch->read_avail = 0;
	} else {
		p = list_first_entry(&ch->rx_size_list_head,
			struct rx_packet_size, list);
		ch->read_avail = p->size;
	}

	mutex_unlock(&ch->ch_lock);

	return ch->read_avail;
}


/**
 *  Bootloader worker function.
 *
 *  @note: clear the bootloader_done flag only after reading the
 *  mailbox, to ignore more requests while reading the mailbox.
 */
static void boot_worker(struct work_struct *work)
{
	int ret = 0;
	int func_num = 0;
	int i;
	struct sdio_al_device *sdio_al_dev = NULL;
	struct sdio_al_work *sdio_al_work = container_of(work,
							 struct sdio_al_work,
							 work);

	if (sdio_al_work == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
				"sdio_al_work\n", __func__);
		return;
	}

	sdio_al_dev = sdio_al_work->sdio_al_dev;
	if (sdio_al_dev == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
				"sdio_al_dev\n", __func__);
		return;
	}
	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":Bootloader Worker Started"
			", wait for bootloader_done event..\n");
	wait_event(sdio_al_dev->wait_mbox,
		   sdio_al_dev->bootloader_done);
	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":Got bootloader_done "
			"event..\n");
	/* Do polling until MDM is up */
	for (i = 0; i < 5000; ++i) {
		if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
			return;
		if (is_user_irq_enabled(sdio_al_dev, func_num)) {
			sdio_al_release_mutex(sdio_al_dev, __func__);
			sdio_al_dev->bootloader_done = 0;
			ret = sdio_al_client_setup(sdio_al_dev);
			if (ret) {
				sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": sdio_al_client_setup failed, "
					"for card %d ret=%d\n",
					sdio_al_dev->host->index, ret);
				sdio_al_get_into_err_state(sdio_al_dev);
			}
			goto done;
		}
		sdio_al_release_mutex(sdio_al_dev, __func__);
		msleep(100);
	}
	sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Timeout waiting for "
			"user_irq for card %d\n",
			sdio_al_dev->host->index);
	sdio_al_get_into_err_state(sdio_al_dev);

done:
	pr_debug(MODULE_NAME ":Boot Worker for card %d Exit!\n",
		sdio_al_dev->host->index);
}

/**
 *  Worker function.
 *
 *  @note: clear the ask_mbox flag only after
 *  	 reading the mailbox, to ignore more requests while
 *  	 reading the mailbox.
 */
static void worker(struct work_struct *work)
{
	int ret = 0;
	struct sdio_al_device *sdio_al_dev = NULL;
	struct sdio_al_work *sdio_al_work = container_of(work,
							 struct sdio_al_work,
							 work);
	if (sdio_al_work == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": worker: NULL "
				"sdio_al_work\n");
		return;
	}

	sdio_al_dev = sdio_al_work->sdio_al_dev;
	if (sdio_al_dev == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": worker: NULL "
				"sdio_al_dev\n");
		return;
	}
	pr_debug(MODULE_NAME ":Worker Started..\n");
	while ((sdio_al_dev->is_ready) && (ret == 0)) {
		pr_debug(MODULE_NAME ":Wait for read mailbox request..\n");
		wait_event(sdio_al_dev->wait_mbox, sdio_al_dev->ask_mbox);
		if (!sdio_al_dev->is_ready)
			break;
		if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
			break;
		if (sdio_al_dev->is_ok_to_sleep) {
			ret = sdio_al_wake_up(sdio_al_dev, 1, NULL);
			if (ret) {
				sdio_al_release_mutex(sdio_al_dev, __func__);
				return;
			}
		}
		ret = read_mailbox(sdio_al_dev, false);
		sdio_al_release_mutex(sdio_al_dev, __func__);
		sdio_al_dev->ask_mbox = false;
	}

	pr_debug(MODULE_NAME ":Worker Exit!\n");
}

/**
 *  Write command using CMD54 rather than CMD53.
 *  Writing with CMD54 generate EOT interrupt at the
 *  SDIO-Client.
 *  Based on mmc_io_rw_extended()
 */
static int sdio_write_cmd54(struct mmc_card *card, unsigned fn,
	unsigned addr, const u8 *buf,
	unsigned blocks, unsigned blksz)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data data;
	struct scatterlist sg;
	int incr_addr = 1; /* MUST */
	int write = 1;

	BUG_ON(!card);
	BUG_ON(fn > 7);
	BUG_ON(blocks == 1 && blksz > 512);
	WARN_ON(blocks == 0);
	WARN_ON(blksz == 0);

	write = true;
	pr_debug(MODULE_NAME ":sdio_write_cmd54()"
		"fn=%d,buf=0x%x,blocks=%d,blksz=%d\n",
		fn, (u32) buf, blocks, blksz);

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = SD_IO_RW_EXTENDED_QCOM;

	cmd.arg = write ? 0x80000000 : 0x00000000;
	cmd.arg |= fn << 28;
	cmd.arg |= incr_addr ? 0x04000000 : 0x00000000;
	cmd.arg |= addr << 9;
	if (blocks == 1 && blksz <= 512)
		cmd.arg |= (blksz == 512) ? 0 : blksz;  /* byte mode */
	else
		cmd.arg |= 0x08000000 | blocks; 	/* block mode */
	cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;

	data.blksz = blksz;
	data.blocks = blocks;
	data.flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, buf, blksz * blocks);

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	if (mmc_host_is_spi(card->host)) {
		/* host driver already reported errors */
	} else {
		if (cmd.resp[0] & R5_ERROR) {
			sdio_al_loge(&sdio_al->gen_log, MODULE_NAME
						":%s: R5_ERROR for card %d",
						__func__, card->host->index);
			return -EIO;
		}
		if (cmd.resp[0] & R5_FUNCTION_NUMBER) {
			sdio_al_loge(&sdio_al->gen_log, MODULE_NAME
						":%s: R5_FUNCTION_NUMBER for card %d",
						__func__, card->host->index);
			return -EINVAL;
		}
		if (cmd.resp[0] & R5_OUT_OF_RANGE) {
			sdio_al_loge(&sdio_al->gen_log, MODULE_NAME
						":%s: R5_OUT_OF_RANGE for card %d",
						__func__, card->host->index);
			return -ERANGE;
		}
	}

	return 0;
}


/**
 *  Write data to channel.
 *  Handle different data size types.
 *
 */
static int sdio_ch_write(struct sdio_channel *ch, const u8 *buf, u32 len)
{
	int ret = 0;
	unsigned blksz = ch->func->cur_blksize;
	int blocks = len / blksz;
	int remain_bytes = len % blksz;
	struct mmc_card *card = NULL;
	u32 fn = ch->func->num;

	if (!ch) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
				"channel\n", __func__);
		return -ENODEV;
	}

	if (!ch->sdio_al_dev) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
				"sdio_al_dev\n", __func__);
		return -ENODEV;
	}

	if (len == 0) {
		sdio_al_loge(ch->sdio_al_dev->dev_log, MODULE_NAME ":channel "
				"%s trying to write 0 bytes\n", ch->name);
		return -EINVAL;
	}

	card = ch->func->card;

	if (remain_bytes) {
		/* CMD53 */
		if (blocks) {
			ret = sdio_memcpy_toio(ch->func, PIPE_TX_FIFO_ADDR,
					       (void *) buf, blocks*blksz);
			if (ret != 0) {
				sdio_al_loge(ch->sdio_al_dev->dev_log,
					MODULE_NAME ":%s: sdio_memcpy_toio "
					"failed for channel %s\n",
					__func__, ch->name);
				sdio_al_get_into_err_state(ch->sdio_al_dev);
				return ret;
			}
		}

		buf += (blocks*blksz);

		ret = sdio_write_cmd54(card, fn, PIPE_TX_FIFO_ADDR,
				buf, 1, remain_bytes);
	} else {
		ret = sdio_write_cmd54(card, fn, PIPE_TX_FIFO_ADDR,
				buf, blocks, blksz);
	}

	if (ret != 0) {
		sdio_al_loge(ch->sdio_al_dev->dev_log, MODULE_NAME ":%s: "
				"sdio_write_cmd54 failed for channel %s\n",
				__func__, ch->name);
		ch->sdio_al_dev->is_err = true;
		return ret;
	}

	return ret;
}

static int sdio_al_bootloader_completed(void)
{
	int i;

	pr_debug(MODULE_NAME ":sdio_al_bootloader_completed was called\n");

	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES; ++i) {
		struct sdio_al_device *dev = NULL;
		if (sdio_al->devices[i] == NULL)
			continue;
		dev = sdio_al->devices[i];
		dev->bootloader_done = 1;
		wake_up(&dev->wait_mbox);
	}

	return 0;
}

static int sdio_al_wait_for_bootloader_comp(struct sdio_al_device *sdio_al_dev)
{
	int ret = 0;

	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	/*
	 * Enable function 0 interrupt mask to allow 9k to raise this interrupt
	 * in power-up. When sdio_downloader will notify its completion
	 * we will poll on this interrupt to wait for 9k power-up
	 */
	ret = enable_mask_irq(sdio_al_dev, 0, 1, 0);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": Enable_mask_irq for card %d failed, "
				"ret=%d\n",
				sdio_al_dev->host->index, ret);
		sdio_al_release_mutex(sdio_al_dev, __func__);
		return ret;
	}

	sdio_al_release_mutex(sdio_al_dev, __func__);

	/*
	 * Start bootloader worker that will wait for the bootloader
	 * completion
	 */
	sdio_al_dev->boot_work.sdio_al_dev = sdio_al_dev;
	INIT_WORK(&sdio_al_dev->boot_work.work, boot_worker);
	sdio_al_dev->bootloader_done = 0;
	queue_work(sdio_al_dev->workqueue, &sdio_al_dev->boot_work.work);

	return 0;
}

static int sdio_al_bootloader_setup(void)
{
	int ret = 0;
	struct sdio_al_device *bootloader_dev = sdio_al->bootloader_dev;
	struct sdio_func *func1 = NULL;

	if (sdio_al_claim_mutex_and_verify_dev(bootloader_dev, __func__))
		return -ENODEV;

	if (bootloader_dev->flashless_boot_on) {
		sdio_al_loge(bootloader_dev->dev_log, MODULE_NAME ":Already "
			"in boot process.\n");
		sdio_al_release_mutex(bootloader_dev, __func__);
		return 0;
	}

	bootloader_dev->sdioc_boot_sw_header
		= kzalloc(sizeof(*bootloader_dev->sdioc_boot_sw_header),
			  GFP_KERNEL);
	if (bootloader_dev->sdioc_boot_sw_header == NULL) {
		sdio_al_loge(bootloader_dev->dev_log, MODULE_NAME ":fail to "
			"allocate sdioc boot sw header.\n");
		sdio_al_release_mutex(bootloader_dev, __func__);
		return -ENOMEM;
	}

	if (sdio_al_verify_func1(bootloader_dev, __func__)) {
		sdio_al_release_mutex(bootloader_dev, __func__);
		goto exit_err;
	}
	func1 = bootloader_dev->card->sdio_func[0];

	ret = sdio_memcpy_fromio(func1,
				 bootloader_dev->sdioc_boot_sw_header,
				 SDIOC_SW_HEADER_ADDR,
				 sizeof(struct peer_sdioc_boot_sw_header));
	if (ret) {
		sdio_al_loge(bootloader_dev->dev_log, MODULE_NAME ":fail to "
			"read sdioc boot sw header.\n");
		sdio_al_release_mutex(bootloader_dev, __func__);
		goto exit_err;
	}

	if (bootloader_dev->sdioc_boot_sw_header->signature !=
	    (u32) PEER_SDIOC_SW_MAILBOX_BOOT_SIGNATURE) {
		sdio_al_loge(bootloader_dev->dev_log, MODULE_NAME ":invalid "
			"mailbox signature 0x%x.\n",
			bootloader_dev->sdioc_boot_sw_header->signature);
		sdio_al_release_mutex(bootloader_dev, __func__);
		ret = -EINVAL;
		goto exit_err;
	}

	/* Upper byte has to be equal - no backward compatibility for unequal */
	if ((bootloader_dev->sdioc_boot_sw_header->version >> 16) !=
	    (sdio_al->pdata->peer_sdioc_boot_version_major)) {
		sdio_al_loge(bootloader_dev->dev_log, MODULE_NAME ": HOST(0x%x)"
			" and CLIENT(0x%x) SDIO_AL BOOT VERSION don't match\n",
			((sdio_al->pdata->peer_sdioc_boot_version_major<<16)+
			sdio_al->pdata->peer_sdioc_boot_version_minor),
			bootloader_dev->sdioc_boot_sw_header->version);
		sdio_al_release_mutex(bootloader_dev, __func__);
		ret = -EIO;
		goto exit_err;
	}

	sdio_al_logi(bootloader_dev->dev_log, MODULE_NAME ": SDIOC BOOT SW "
			"version 0x%x\n",
			bootloader_dev->sdioc_boot_sw_header->version);

	bootloader_dev->flashless_boot_on = true;

	sdio_al_release_mutex(bootloader_dev, __func__);

	ret = sdio_al_wait_for_bootloader_comp(bootloader_dev);
	if (ret) {
		sdio_al_loge(bootloader_dev->dev_log, MODULE_NAME
				": sdio_al_wait_for_bootloader_comp failed, "
				"err=%d\n", ret);
		goto exit_err;
	}

	ret = sdio_downloader_setup(bootloader_dev->card, 1,
			bootloader_dev->sdioc_boot_sw_header->boot_ch_num,
			sdio_al_bootloader_completed);

	if (ret) {
		sdio_al_loge(bootloader_dev->dev_log, MODULE_NAME
			": sdio_downloader_setup failed, err=%d\n", ret);
		goto exit_err;
	}

	sdio_al_logi(bootloader_dev->dev_log, MODULE_NAME ":In Flashless boot,"
		" waiting for its completion\n");


exit_err:
	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":free "
			"sdioc_boot_sw_header.\n");
	kfree(bootloader_dev->sdioc_boot_sw_header);
	bootloader_dev->sdioc_boot_sw_header = NULL;
	bootloader_dev = NULL;

	return ret;
}


/**
 *  Read SDIO-Client software header
 *
 */
static int read_sdioc_software_header(struct sdio_al_device *sdio_al_dev,
				      struct peer_sdioc_sw_header *header)
{
	int ret;
	int i;
	int test_version = 0;
	int sdioc_test_version = 0;
	struct sdio_func *func1 = NULL;

	pr_debug(MODULE_NAME ":reading sdioc sw header.\n");

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return -ENODEV;

	func1 = sdio_al_dev->card->sdio_func[0];

	ret = sdio_memcpy_fromio(func1, header,
			SDIOC_SW_HEADER_ADDR, sizeof(*header));
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":fail to read "
				"sdioc sw header.\n");
		goto exit_err;
	}

	if (header->signature == (u32)PEER_SDIOC_SW_MAILBOX_UT_SIGNATURE) {
		sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":SDIOC SW "
				"unittest signature. 0x%x\n",
				header->signature);
		sdio_al->unittest_mode = true;
		/* Verify test code compatibility with the modem */
		sdioc_test_version = (header->version & 0xFF00) >> 8;
		test_version = sdio_al->pdata->peer_sdioc_version_minor >> 8;
		if (test_version != sdioc_test_version) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": HOST(0x%x) and CLIENT(0x%x) "
				"testing VERSION don't match\n",
				test_version,
				sdioc_test_version);
			msleep(500);
			BUG();
		}
	}

	if ((header->signature != (u32) PEER_SDIOC_SW_MAILBOX_SIGNATURE) &&
	    (header->signature != (u32) PEER_SDIOC_SW_MAILBOX_UT_SIGNATURE)) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":SDIOC SW "
				"invalid signature. 0x%x\n", header->signature);
		goto exit_err;
	}
	/* Upper byte has to be equal - no backward compatibility for unequal */
	sdio_al->sdioc_major = header->version >> 16;
	if (sdio_al->pdata->allow_sdioc_version_major_2) {
		if ((sdio_al->sdioc_major !=
		    sdio_al->pdata->peer_sdioc_version_major) &&
		    (sdio_al->sdioc_major != PEER_SDIOC_OLD_VERSION_MAJOR)) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": HOST(0x%x) and CLIENT(0x%x) "
				"SDIO_AL VERSION don't match\n",
				((sdio_al->pdata->peer_sdioc_version_major<<16)+
				sdio_al->pdata->peer_sdioc_version_minor),
				header->version);
			goto exit_err;
		}
	} else {
		if (sdio_al->sdioc_major !=
		    sdio_al->pdata->peer_sdioc_version_major) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": HOST(0x%x) and CLIENT(0x%x) "
				"SDIO_AL VERSION don't match\n",
				((sdio_al->pdata->peer_sdioc_version_major<<16)+
				sdio_al->pdata->peer_sdioc_version_minor),
				header->version);
			goto exit_err;
		}
	}
	sdio_al_dev->ch_close_supported = (header->version & 0x000F) >=
		(sdio_al->pdata->peer_sdioc_version_minor & 0xF);

	sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":SDIOC SW version 0x%x,"
			" sdio_al major 0x%x minor 0x%x\n", header->version,
			sdio_al->sdioc_major,
			sdio_al->pdata->peer_sdioc_version_minor);

	sdio_al_dev->flashless_boot_on = false;
	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
		struct sdio_channel *ch = &sdio_al_dev->channel[i];

		/* Set default values */
		ch->read_threshold  = DEFAULT_READ_THRESHOLD;
		ch->write_threshold = DEFAULT_WRITE_THRESHOLD;
		ch->min_write_avail = DEFAULT_MIN_WRITE_THRESHOLD;
		ch->is_packet_mode = true;
		ch->peer_tx_buf_size = DEFAULT_PEER_TX_BUF_SIZE;
		ch->poll_delay_msec = 0;

		ch->num = i;
		ch->func = NULL;
		ch->rx_pipe_index = ch->num*2;
		ch->tx_pipe_index = ch->num*2+1;

		memset(ch->name, 0, sizeof(ch->name));

		if (header->channel_names[i][0]) {
			memcpy(ch->name, SDIO_PREFIX,
			       strlen(SDIO_PREFIX));
			memcpy(ch->name + strlen(SDIO_PREFIX),
			       header->channel_names[i],
			       PEER_CHANNEL_NAME_SIZE);

			ch->state = SDIO_CHANNEL_STATE_IDLE;
			ch->sdio_al_dev = sdio_al_dev;
			if (sdio_al_dev->card->sdio_func[ch->num+1]) {
				ch->func =
				sdio_al_dev->card->sdio_func[ch->num+1];
			} else {
				sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": NULL func for channel %s\n",
					ch->name);
				goto exit_err;
			}
		} else {
			ch->state = SDIO_CHANNEL_STATE_INVALID;
		}

		sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":Channel=%s, "
				"state=%d\n", ch->name,	ch->state);
	}

	return 0;

exit_err:
	sdio_al_get_into_err_state(sdio_al_dev);
	memset(header, 0, sizeof(*header));

	return -EIO;
}

/**
 *  Read SDIO-Client channel configuration
 *
 */
static int read_sdioc_channel_config(struct sdio_channel *ch)
{
	int ret;
	struct peer_sdioc_sw_mailbox *sw_mailbox = NULL;
	struct peer_sdioc_channel_config *ch_config = NULL;
	struct sdio_al_device *sdio_al_dev = ch->sdio_al_dev;

	if (sdio_al_dev == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": NULL sdio_al_dev"
				" for channel %s\n", ch->name);
		return -EINVAL;
	}

	if (sdio_al_dev->sdioc_sw_header->version == 0)
		return -1;

	pr_debug(MODULE_NAME ":reading sw mailbox %s channel.\n", ch->name);

	sw_mailbox = kzalloc(sizeof(*sw_mailbox), GFP_KERNEL);
	if (sw_mailbox == NULL)
		return -ENOMEM;

	ret = sdio_memcpy_fromio(ch->func, sw_mailbox,
			SDIOC_SW_MAILBOX_ADDR, sizeof(*sw_mailbox));
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":fail to read "
				"sw mailbox.\n");
		goto exit_err;
	}

	ch_config = &sw_mailbox->ch_config[ch->num];
	memcpy(&ch->ch_config, ch_config,
		sizeof(struct peer_sdioc_channel_config));

	if (!ch_config->is_ready) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":sw mailbox "
				"channel not ready.\n");
		goto exit_err;
	}

	ch->read_threshold = LOW_LATENCY_THRESHOLD;
	ch->is_low_latency_ch = ch_config->is_low_latency_ch;
	/* Threshold on 50% of the maximum size , sdioc uses double-buffer */
	ch->write_threshold = (ch_config->max_tx_threshold * 5) / 10;
	ch->threshold_change_cnt = ch->ch_config.max_rx_threshold -
			ch->read_threshold + THRESHOLD_CHANGE_EXTRA_BYTES;

	if (ch->is_low_latency_ch)
		ch->def_read_threshold = LOW_LATENCY_THRESHOLD;
	else /* Aggregation up to 90% of the maximum size */
		ch->def_read_threshold = (ch_config->max_rx_threshold * 9) / 10;

	ch->is_packet_mode = ch_config->is_packet_mode;
	if (!ch->is_packet_mode) {
		ch->poll_delay_msec = DEFAULT_POLL_DELAY_NOPACKET_MSEC;
		ch->min_write_avail = DEFAULT_MIN_WRITE_THRESHOLD_STREAMING;
	}
	/* The max_packet_size is set by the modem in version 3 and on */
	if (sdio_al->sdioc_major > PEER_SDIOC_OLD_VERSION_MAJOR)
		ch->min_write_avail = ch_config->max_packet_size;

	if (ch->min_write_avail > ch->write_threshold)
		ch->min_write_avail = ch->write_threshold;

	CLOSE_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":ch %s "
			"read_threshold=%d, write_threshold=%d,"
			" min_write_avail=%d, max_rx_threshold=%d,"
			" max_tx_threshold=%d\n", ch->name, ch->read_threshold,
			ch->write_threshold, ch->min_write_avail,
			ch_config->max_rx_threshold,
			ch_config->max_tx_threshold);

	ch->peer_tx_buf_size = ch_config->tx_buf_size;

	kfree(sw_mailbox);

	return 0;

exit_err:
	sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":Reading SW Mailbox "
			"error.\n");
	kfree(sw_mailbox);

	return -1;
}


/**
 *  Enable/Disable EOT interrupt of a pipe.
 *
 */
static int enable_eot_interrupt(struct sdio_al_device *sdio_al_dev,
				int pipe_index, int enable)
{
	int ret = 0;
	struct sdio_func *func1;
	u32 mask;
	u32 pipe_mask;
	u32 addr;

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return -ENODEV;
	func1 = sdio_al_dev->card->sdio_func[0];

	if (pipe_index < 8) {
		addr = PIPES_0_7_IRQ_MASK_ADDR;
		pipe_mask = (1<<pipe_index);
	} else {
		addr = PIPES_8_15_IRQ_MASK_ADDR;
		pipe_mask = (1<<(pipe_index-8));
	}

	mask = sdio_readl(func1, addr, &ret);
	if (ret) {
		pr_debug(MODULE_NAME ":enable_eot_interrupt fail\n");
		goto exit_err;
	}

	if (enable)
		mask &= (~pipe_mask); /* 0 = enable */
	else
		mask |= (pipe_mask);  /* 1 = disable */

	sdio_writel(func1, mask, addr, &ret);

exit_err:
	return ret;
}


/**
 *  Enable/Disable mask interrupt of a function.
 *
 */
static int enable_mask_irq(struct sdio_al_device *sdio_al_dev,
			   int func_num, int enable, u8 bit_offset)
{
	int ret = 0;
	struct sdio_func *func1 = NULL;
	u32 mask = 0;
	u32 func_mask = 0;
	u32 addr = 0;
	u32 offset = 0;

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return -ENODEV;
	func1 = sdio_al_dev->card->sdio_func[0];

	if (func_num < 4) {
		addr = FUNC_1_4_MASK_IRQ_ADDR;
		offset = func_num * 8 + bit_offset;
	} else {
		addr = FUNC_5_7_MASK_IRQ_ADDR;
		offset = (func_num - 4) * 8 + bit_offset;
	}

	func_mask = 1<<offset;

	mask = sdio_readl(func1, addr, &ret);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": "
				"enable_mask_irq fail\n");
		goto exit_err;
	}

	if (enable)
		mask &= (~func_mask); /* 0 = enable */
	else
		mask |= (func_mask);  /* 1 = disable */

	pr_debug(MODULE_NAME ":enable_mask_irq,  writing mask = 0x%x\n", mask);

	sdio_writel(func1, mask, addr, &ret);

exit_err:
	return ret;
}

/**
 *  Enable/Disable Threshold interrupt of a pipe.
 *
 */
static int enable_threshold_interrupt(struct sdio_al_device *sdio_al_dev,
				      int pipe_index, int enable)
{
	int ret = 0;
	struct sdio_func *func1;
	u32 mask;
	u32 pipe_mask;
	u32 addr;

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return -ENODEV;
	func1 = sdio_al_dev->card->sdio_func[0];

	if (pipe_index < 8) {
		addr = PIPES_0_7_IRQ_MASK_ADDR;
		pipe_mask = (1<<pipe_index);
	} else {
		addr = PIPES_8_15_IRQ_MASK_ADDR;
		pipe_mask = (1<<(pipe_index-8));
	}

	mask = sdio_readl(func1, addr, &ret);
	if (ret) {
		pr_debug(MODULE_NAME ":enable_threshold_interrupt fail\n");
		goto exit_err;
	}

	pipe_mask = pipe_mask<<8; /* Threshold bits 8..15 */
	if (enable)
		mask &= (~pipe_mask); /* 0 = enable */
	else
		mask |= (pipe_mask);  /* 1 = disable */

	sdio_writel(func1, mask, addr, &ret);

exit_err:
	return ret;
}

/**
 *  Set the threshold to trigger interrupt from SDIO-Card on
 *  pipe available bytes.
 *
 */
static int set_pipe_threshold(struct sdio_al_device *sdio_al_dev,
			      int pipe_index, int threshold)
{
	int ret = 0;
	struct sdio_func *func1;

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return -ENODEV;
	func1 = sdio_al_dev->card->sdio_func[0];

	sdio_writel(func1, threshold,
			PIPES_THRESHOLD_ADDR+pipe_index*4, &ret);
	if (ret)
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": "
				"set_pipe_threshold err=%d\n", -ret);

	return ret;
}

/**
 *  Enable func w/ retries
 *
 */
static int sdio_al_enable_func_retry(struct sdio_func *func, const char *name)
{
	int ret, i;
	for (i = 0; i < 200; i++) {
		ret = sdio_enable_func(func);
		if (ret) {
			pr_debug(MODULE_NAME ":retry enable %s func#%d "
					     "ret=%d\n",
					 name, func->num, ret);
			msleep(10);
		} else
			break;
	}

	return ret;
}

/**
 *  Open Channel
 *
 *  1. Init Channel Context.
 *  2. Init the Channel SDIO-Function.
 *  3. Init the Channel Pipes on Mailbox.
 */
static int open_channel(struct sdio_channel *ch)
{
	int ret = 0;
	struct sdio_al_device *sdio_al_dev = ch->sdio_al_dev;

	if (sdio_al_dev == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": NULL "
				"sdio_al_dev for channel %s\n", ch->name);
		return -EINVAL;
	}

	/* Init channel Context */
	/** Func#1 is reserved for mailbox */
	ch->func = sdio_al_dev->card->sdio_func[ch->num+1];
	ch->rx_pipe_index = ch->num*2;
	ch->tx_pipe_index = ch->num*2+1;
	ch->signature = SDIO_AL_SIGNATURE;

	ch->total_rx_bytes = 0;
	ch->total_tx_bytes = 0;

	ch->write_avail = 0;
	ch->read_avail = 0;
	ch->rx_pending_bytes = 0;

	mutex_init(&ch->ch_lock);

	pr_debug(MODULE_NAME ":open_channel %s func#%d\n",
			 ch->name, ch->func->num);

	INIT_LIST_HEAD(&(ch->rx_size_list_head));

	/* Init SDIO Function */
	ret = sdio_al_enable_func_retry(ch->func, ch->name);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": "
				"sdio_enable_func() err=%d\n", -ret);
		goto exit_err;
	}

	/* Note: Patch Func CIS tuple issue */
	ret = sdio_set_block_size(ch->func, SDIO_AL_BLOCK_SIZE);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": "
				"sdio_set_block_size()failed, err=%d\n", -ret);
		goto exit_err;
	}

	ch->func->max_blksize = SDIO_AL_BLOCK_SIZE;

	sdio_set_drvdata(ch->func, ch);

	/* Get channel parameters from the peer SDIO-Client */
	read_sdioc_channel_config(ch);

	/* Set Pipes Threshold on Mailbox */
	ret = set_pipe_threshold(sdio_al_dev,
				 ch->rx_pipe_index, ch->read_threshold);
	if (ret)
		goto exit_err;
	ret = set_pipe_threshold(sdio_al_dev,
				 ch->tx_pipe_index, ch->write_threshold);
	if (ret)
		goto exit_err;

	/* Set flag before interrupts are enabled to allow notify */
	ch->state = SDIO_CHANNEL_STATE_OPEN;
	pr_debug(MODULE_NAME ":channel %s is in OPEN state now\n", ch->name);

	sdio_al_dev->poll_delay_msec = get_min_poll_time_msec(sdio_al_dev);

	/* lpm mechanism lives under the assumption there is always a timer */
	/* Check if need to start the timer */
	if  ((sdio_al_dev->poll_delay_msec) &&
	     (sdio_al_dev->is_timer_initialized == false)) {

		init_timer(&sdio_al_dev->timer);
		sdio_al_dev->timer.data = (unsigned long) sdio_al_dev;
		sdio_al_dev->timer.function = sdio_al_timer_handler;
		sdio_al_dev->timer.expires = jiffies +
			msecs_to_jiffies(sdio_al_dev->poll_delay_msec);
		add_timer(&sdio_al_dev->timer);
		sdio_al_dev->is_timer_initialized = true;
	}

	/* Enable Pipes Interrupts */
	enable_eot_interrupt(sdio_al_dev, ch->rx_pipe_index, true);
	enable_eot_interrupt(sdio_al_dev, ch->tx_pipe_index, true);

	enable_threshold_interrupt(sdio_al_dev, ch->rx_pipe_index, true);
	enable_threshold_interrupt(sdio_al_dev, ch->tx_pipe_index, true);

exit_err:

	return ret;
}

/**
 *  Ask the worker to read the mailbox.
 */
static void ask_reading_mailbox(struct sdio_al_device *sdio_al_dev)
{
	if (!sdio_al_dev->ask_mbox) {
		pr_debug(MODULE_NAME ":ask_reading_mailbox for card %d\n",
			 sdio_al_dev->host->index);
		sdio_al_dev->ask_mbox = true;
		wake_up(&sdio_al_dev->wait_mbox);
	}
}

/**
 *  Start the timer
 */
static void start_timer(struct sdio_al_device *sdio_al_dev)
{
	if ((sdio_al_dev->poll_delay_msec)  &&
		(sdio_al_dev->state == CARD_INSERTED)) {
		sdio_al_dev->timer.expires = jiffies +
			msecs_to_jiffies(sdio_al_dev->poll_delay_msec);
		add_timer(&sdio_al_dev->timer);
	}
}

/**
 *  Restart(postpone) the already working timer
 */
static void restart_timer(struct sdio_al_device *sdio_al_dev)
{
	if ((sdio_al_dev->poll_delay_msec) &&
		(sdio_al_dev->state == CARD_INSERTED)) {
		ulong expires =	jiffies +
			msecs_to_jiffies(sdio_al_dev->poll_delay_msec);
		mod_timer(&sdio_al_dev->timer, expires);
	}
}

/**
 *  Stop and delete the timer
 */
static void stop_and_del_timer(struct sdio_al_device *sdio_al_dev)
{
	if (sdio_al_dev->is_timer_initialized) {
		sdio_al_dev->poll_delay_msec = 0;
		del_timer_sync(&sdio_al_dev->timer);
	}
}

/**
 *  Do the wakup sequence.
 *  This function should be called after claiming the host!
 *  The caller is responsible for releasing the host.
 *
 *  Wake up sequence
 *  1. Get lock
 *  2. Enable wake up function if needed
 *  3. Mark NOT OK to sleep and write it
 *  4. Restore default thresholds
 *  5. Start the mailbox and inactivity timer again
 */
static int sdio_al_wake_up(struct sdio_al_device *sdio_al_dev,
			   u32 not_from_int, struct sdio_channel *ch)
{
	int ret = 0;
	struct sdio_func *wk_func = NULL;
	unsigned long time_to_wait;
	struct mmc_host *host = sdio_al_dev->host;

	if (sdio_al_dev->is_err) {
		SDIO_AL_ERR(__func__);
		return -ENODEV;
	}

	if (!sdio_al_dev->is_ok_to_sleep) {
		LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":card %d "
				"already awake, no need to wake up\n",
				sdio_al_dev->host->index);
		return 0;
	}

	/* Wake up sequence */
	if (not_from_int) {
		if (ch) {
			LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ": Wake up"
					" card %d (not by interrupt), ch %s",
					sdio_al_dev->host->index,
					ch->name);
		} else {
			LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ": Wake up"
					  " card %d (not	by interrupt)",
					  sdio_al_dev->host->index);
		}
	} else {
		LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ": Wake up card "
				"%d by interrupt",
				sdio_al_dev->host->index);
		sdio_al_dev->print_after_interrupt = 1;
	}

	sdio_al_vote_for_sleep(sdio_al_dev, 0);

	msmsdcc_lpm_disable(host);
	msmsdcc_set_pwrsave(host, 0);
	/* Poll the GPIO */
	time_to_wait = jiffies + msecs_to_jiffies(1000);
	while (time_before(jiffies, time_to_wait)) {
		if (sdio_al->pdata->get_mdm2ap_status())
			break;
		udelay(TIME_TO_WAIT_US);
	}

	pr_debug(MODULE_NAME ":GPIO mdm2ap_status=%d\n",
		       sdio_al->pdata->get_mdm2ap_status());

	/* Here get_mdm2ap_status() returning 0 is not an error condition */
	if (sdio_al->pdata->get_mdm2ap_status() == 0)
		LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ": "
				"get_mdm2ap_status() is 0\n");

	/* Enable Wake up Function */
	if (!sdio_al_dev->card ||
	    !sdio_al_dev->card->sdio_func[SDIO_AL_WAKEUP_FUNC-1]) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": NULL card or wk_func\n");
		return -ENODEV;
	}
	wk_func = sdio_al_dev->card->sdio_func[SDIO_AL_WAKEUP_FUNC-1];
	ret = sdio_al_enable_func_retry(wk_func, "wakeup func");
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": "
				"sdio_enable_func() err=%d\n", -ret);
		goto error_exit;
	}
	/* Mark NOT OK_TOSLEEP */
	sdio_al_dev->is_ok_to_sleep = 0;
	ret = write_lpm_info(sdio_al_dev);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": "
				"write_lpm_info() failed, err=%d\n", -ret);
		sdio_al_dev->is_ok_to_sleep = 1;
		sdio_disable_func(wk_func);
		goto error_exit;
	}
	sdio_disable_func(wk_func);

	/* Start the timer again*/
	restart_inactive_time(sdio_al_dev);
	sdio_al_dev->poll_delay_msec = get_min_poll_time_msec(sdio_al_dev);
	start_timer(sdio_al_dev);

	LPM_DEBUG(sdio_al_dev->dev_log, MODULE_NAME "Finished Wake up sequence"
			" for card %d", sdio_al_dev->host->index);

	msmsdcc_set_pwrsave(host, 1);
	pr_debug(MODULE_NAME ":Turn clock off\n");

	return ret;
error_exit:
	sdio_al_vote_for_sleep(sdio_al_dev, 1);
	msmsdcc_set_pwrsave(host, 1);
	WARN_ON(ret);
	sdio_al_get_into_err_state(sdio_al_dev);
	return ret;
}


/**
 *  SDIO Function Interrupt handler.
 *
 *  Interrupt shall be triggered by SDIO-Client when:
 *  1. End-Of-Transfer (EOT) detected in packet mode.
 *  2. Bytes-available reached the threshold.
 *
 *  Reading the mailbox clears the EOT/Threshold interrupt
 *  source.
 *  The interrupt source should be cleared before this ISR
 *  returns. This ISR is called from IRQ Thread and not
 *  interrupt, so it may sleep.
 *
 */
static void sdio_func_irq(struct sdio_func *func)
{
	struct sdio_al_device *sdio_al_dev = sdio_get_drvdata(func);

	pr_debug(MODULE_NAME ":start %s.\n", __func__);

	if (sdio_al_dev == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": NULL device");
		return;
	}

	if (sdio_al_dev->is_ok_to_sleep)
		sdio_al_wake_up(sdio_al_dev, 0, NULL);
	else
		restart_timer(sdio_al_dev);

	read_mailbox(sdio_al_dev, true);

	pr_debug(MODULE_NAME ":end %s.\n", __func__);
}

/**
 *  Timer Expire Handler
 *
 */
static void sdio_al_timer_handler(unsigned long data)
{
	struct sdio_al_device *sdio_al_dev = (struct sdio_al_device *)data;
	if (sdio_al_dev == NULL) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": NULL "
				"sdio_al_dev for data %lu\n", data);
		return;
	}
	if (sdio_al_dev->state != CARD_INSERTED) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": sdio_al_dev "
				"is in invalid state %d\n", sdio_al_dev->state);
		return;
	}
	pr_debug(MODULE_NAME " Timer Expired\n");

	ask_reading_mailbox(sdio_al_dev);

	restart_timer(sdio_al_dev);
}

/**
 *  Driver Setup.
 *
 */
static int sdio_al_setup(struct sdio_al_device *sdio_al_dev)
{
	int ret = 0;
	struct mmc_card *card = sdio_al_dev->card;
	struct sdio_func *func1 = NULL;
	int i = 0;
	int fn = 0;

	if (sdio_al_verify_func1(sdio_al_dev, __func__))
		return -ENODEV;
	func1 = card->sdio_func[0];

	sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":sdio_al_setup for "
			"card %d\n", sdio_al_dev->host->index);

	ret = sdio_al->pdata->config_mdm2ap_status(1);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME "Could not "
				"request GPIO\n");
		return ret;
	}

	INIT_WORK(&sdio_al_dev->sdio_al_work.work, worker);
	/* disable all pipes interrupts before claim irq.
	   since all are enabled by default. */
	for (i = 0 ; i < SDIO_AL_MAX_PIPES; i++) {
		enable_eot_interrupt(sdio_al_dev, i, false);
		enable_threshold_interrupt(sdio_al_dev, i, false);
	}

	/* Disable all SDIO Functions before claim irq. */
	for (fn = 1 ; fn <= card->sdio_funcs; fn++)
		sdio_disable_func(card->sdio_func[fn-1]);

	sdio_set_drvdata(func1, sdio_al_dev);
	sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":claim IRQ for card "
			"%d\n",	sdio_al_dev->host->index);

	ret = sdio_claim_irq(func1, sdio_func_irq);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Fail to claim"
				" IRQ for card %d\n",
				sdio_al_dev->host->index);
		return ret;
	}

	sdio_al_dev->is_ready = true;

	/* Start worker before interrupt might happen */
	queue_work(sdio_al_dev->workqueue, &sdio_al_dev->sdio_al_work.work);

	start_inactive_time(sdio_al_dev);

	pr_debug(MODULE_NAME ":Ready.\n");

	return 0;
}

/**
 *  Driver Tear-Down.
 *
 */
static void sdio_al_tear_down(void)
{
	int i, j;
	struct sdio_al_device *sdio_al_dev = NULL;
	struct sdio_func *func1;

	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES; ++i) {
		if (sdio_al->devices[i] == NULL)
			continue;
		sdio_al_dev = sdio_al->devices[i];

		if (sdio_al_dev->is_ready) {
			sdio_al_dev->is_ready = false; /* Flag worker to exit */
			sdio_al_dev->ask_mbox = false;
			ask_reading_mailbox(sdio_al_dev); /* Wakeup worker */
			/* allow gracefully exit of the worker thread */
			msleep(100);

			flush_workqueue(sdio_al_dev->workqueue);
			destroy_workqueue(sdio_al_dev->workqueue);

			sdio_al_vote_for_sleep(sdio_al_dev, 1);

			if (!sdio_al_claim_mutex_and_verify_dev(sdio_al_dev,
								__func__)) {
				if (!sdio_al_dev->card ||
				    !sdio_al_dev->card->sdio_func[0]) {
					sdio_al_loge(sdio_al_dev->dev_log,
						     MODULE_NAME
							": %s: Invalid func1",
							__func__);
					return;
				}
				func1 = sdio_al_dev->card->sdio_func[0];
				sdio_release_irq(func1);
				sdio_disable_func(func1);
				sdio_al_release_mutex(sdio_al_dev, __func__);
			}
		}

		for (j = 0; j < SDIO_AL_MAX_CHANNELS; j++)
			sdio_al_dev->channel[j].signature = 0x0;
		sdio_al_dev->signature = 0;

		kfree(sdio_al_dev->sdioc_sw_header);
		kfree(sdio_al_dev->mailbox);
		kfree(sdio_al_dev->rx_flush_buf);
		kfree(sdio_al_dev);
	}

	sdio_al->pdata->config_mdm2ap_status(0);
}

/**
 *  Find channel by name.
 *
 */
static struct sdio_channel *find_channel_by_name(const char *name)
{
	struct sdio_channel *ch = NULL;
	int i, j;
	struct sdio_al_device *sdio_al_dev = NULL;

	for (j = 0; j < MAX_NUM_OF_SDIO_DEVICES; ++j) {
		if (sdio_al->devices[j] == NULL)
			continue;
		sdio_al_dev = sdio_al->devices[j];
		for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
			if (sdio_al_dev->channel[i].state ==
					SDIO_CHANNEL_STATE_INVALID)
				continue;
			if (strncmp(sdio_al_dev->channel[i].name, name,
					CHANNEL_NAME_SIZE) == 0) {
				ch = &sdio_al_dev->channel[i];
				break;
			}
		}
		if (ch != NULL)
			break;
	}

	return ch;
}

/**
 *  Find the minimal poll time.
 *
 */
static int get_min_poll_time_msec(struct sdio_al_device *sdio_sl_dev)
{
	int i;
	int poll_delay_msec = 0x0FFFFFFF;

	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++)
		if ((sdio_sl_dev->channel[i].state ==
					SDIO_CHANNEL_STATE_OPEN) &&
		(sdio_sl_dev->channel[i].poll_delay_msec > 0) &&
		(sdio_sl_dev->channel[i].poll_delay_msec < poll_delay_msec))
			poll_delay_msec =
				sdio_sl_dev->channel[i].poll_delay_msec;

	if (poll_delay_msec == 0x0FFFFFFF)
		poll_delay_msec = SDIO_AL_POLL_TIME_NO_STREAMING;

	pr_debug(MODULE_NAME ":poll delay time is %d msec\n", poll_delay_msec);

	return poll_delay_msec;
}

/**
 *  Open SDIO Channel.
 *
 *  Enable the channel.
 *  Set the channel context.
 *  Trigger reading the mailbox to check available bytes.
 *
 */
int sdio_open(const char *name, struct sdio_channel **ret_ch, void *priv,
		 void (*notify)(void *priv, unsigned ch_event))
{
	int ret = 0;
	struct sdio_channel *ch = NULL;
	struct sdio_al_device *sdio_al_dev = NULL;

	*ret_ch = NULL; /* default */

	ch = find_channel_by_name(name);
	if (ch == NULL) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":Can't find "
			"channel name %s\n", name);
		return -EINVAL;
	}

	sdio_al_dev = ch->sdio_al_dev;
	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	if ((ch->state != SDIO_CHANNEL_STATE_IDLE) &&
		(ch->state != SDIO_CHANNEL_STATE_CLOSED)) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Wrong ch %s "
				"state %d\n", name, ch->state);
		ret = -EPERM;
		goto exit_err;
	}

	if (sdio_al_dev->is_err) {
		SDIO_AL_ERR(__func__);
		ret = -ENODEV;
		goto exit_err;
	}

	ret = sdio_al_wake_up(sdio_al_dev, 1, ch);
	if (ret)
		goto exit_err;

	ch->notify = notify;
	ch->priv = priv;

	/* Note: Set caller returned context before interrupts are enabled */
	*ret_ch = ch;

	ret = open_channel(ch);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":sdio_open %s "
				"err=%d\n", name, -ret);
		goto exit_err;
	}

	CLOSE_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":sdio_open %s "
							"completed OK\n", name);
	if (sdio_al_dev->lpm_chan == INVALID_SDIO_CHAN) {
		if (sdio_al->sdioc_major == PEER_SDIOC_OLD_VERSION_MAJOR) {
			if (!ch->is_packet_mode) {
				sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME
						":setting channel %s as "
						"lpm_chan\n", name);
				sdio_al_dev->lpm_chan = ch->num;
			}
		} else {
			sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ": "
					"setting channel %s as lpm_chan\n",
					name);
			sdio_al_dev->lpm_chan = ch->num;
		}
	}

exit_err:
	sdio_al_release_mutex(sdio_al_dev, __func__);
	return ret;
}
EXPORT_SYMBOL(sdio_open);

/**
 *  Request peer operation
 *  note: sanity checks of parameters done by caller
 *        called under bus locked
 */
static int peer_set_operation(u32 opcode,
		struct sdio_al_device *sdio_al_dev,
		struct sdio_channel *ch)
{
	int ret;
	int offset;
	struct sdio_func *wk_func = NULL;
	u32 peer_operation;
	int loop_count = 0;

	if (!sdio_al_dev->card ||
	    !sdio_al_dev->card->sdio_func[SDIO_AL_WAKEUP_FUNC-1]) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": NULL card or wk_func\n");
		ret = -ENODEV;
		goto exit;
	}
	wk_func = sdio_al_dev->card->sdio_func[SDIO_AL_WAKEUP_FUNC-1];

	/* calculate offset of peer_operation field in sw mailbox struct */
	offset = offsetof(struct peer_sdioc_sw_mailbox, ch_config) +
		sizeof(struct peer_sdioc_channel_config) * ch->num +
		offsetof(struct peer_sdioc_channel_config, peer_operation);

	ret = sdio_al_wake_up(sdio_al_dev, 1, ch);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Fail to "
				"wake up\n");
		goto exit;
	}
	/* request operation from MDM peer */
	peer_operation = PEER_OPERATION(opcode, PEER_OP_STATE_INIT);
	ret = sdio_memcpy_toio(ch->func, SDIOC_SW_MAILBOX_ADDR+offset,
			&peer_operation, sizeof(u32));
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":failed to "
				"request close operation\n");
		goto exit;
	}
	ret = sdio_al_enable_func_retry(wk_func, "wk_func");
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Fail to enable"
				" Func#%d\n", wk_func->num);
		goto exit;
	}
	pr_debug(MODULE_NAME ":%s: wk_func enabled on ch %s\n",
			__func__, ch->name);
	/* send "start" operation to MDM */
	peer_operation = PEER_OPERATION(opcode, PEER_OP_STATE_START);
	ret  =  sdio_memcpy_toio(ch->func, SDIOC_SW_MAILBOX_ADDR+offset,
			&peer_operation, sizeof(u32));
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":failed to "
				"send start close operation\n");
		goto exit;
	}
	ret = sdio_disable_func(wk_func);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Fail to "
				"disable Func#%d\n", wk_func->num);
		goto exit;
	}
	/* poll for peer operation ack */
	while (peer_operation != 0) {
		ret  =  sdio_memcpy_fromio(ch->func,
				&peer_operation,
				SDIOC_SW_MAILBOX_ADDR+offset,
				sizeof(u32));
		if (ret) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					":failed to request ack on close"
					" operation, loop_count = %d\n",
					loop_count);
			goto exit;
		}
		loop_count++;
		if (loop_count > 10) {
			sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":%s: "
					"peer_operation=0x%x wait loop"
					" %d on ch %s\n", __func__,
					peer_operation, loop_count, ch->name);
		}
	}
exit:
	return ret;
}

static int channel_close(struct sdio_channel *ch, int flush_flag)
{
	int ret;
	struct sdio_al_device *sdio_al_dev = NULL;
	int flush_len;
	ulong flush_expires;

	if (!ch) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL "
				"channel\n",  __func__);
		return -ENODEV;
	}

	if (!ch->func) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":%s: NULL func"
				" on channel:%d\n", __func__, ch->num);
		return -ENODEV;
	}

	sdio_al_dev = ch->sdio_al_dev;
	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	if (!sdio_al_dev->ch_close_supported) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":%s: Not "
			"supported by mdm, ch %s\n",
			__func__, ch->name);
		ret = -ENOTSUPP;
		goto error_exit;
	}

	if (sdio_al_dev->is_err) {
		SDIO_AL_ERR(__func__);
		ret = -ENODEV;
		goto error_exit;
	}
	if (ch->state != SDIO_CHANNEL_STATE_OPEN) {
		sdio_al_loge(sdio_al_dev->dev_log,
				MODULE_NAME ":%s: ch %s is not in "
				"open state (%d)\n",
				__func__, ch->name, ch->state);
		ret = -ENODEV;
		goto error_exit;
	}
	ch->state = SDIO_CHANNEL_STATE_CLOSING;
	ret = peer_set_operation(PEER_OP_CODE_CLOSE, sdio_al_dev, ch);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":%s: "
				"peer_set_operation() failed: %d\n",
				__func__, ret);
		ret = -ENODEV;
		goto error_exit;
	}
	/* udate poll time for opened channels */
	if  (ch->poll_delay_msec > 0) {
		sdio_al_dev->poll_delay_msec =
			get_min_poll_time_msec(sdio_al_dev);
	}
	sdio_al_release_mutex(ch->sdio_al_dev, __func__);

	flush_expires = jiffies +
		msecs_to_jiffies(SDIO_CLOSE_FLUSH_TIMEOUT_MSEC);
	/* flush rx packets of the channel */
	if (flush_flag) {
		do {
			while (ch->read_avail > 0) {
				flush_len = ch->read_avail;
				ret = sdio_read_internal(ch,
						sdio_al_dev->rx_flush_buf,
						flush_len);
				if (ret) {
					sdio_al_loge(&sdio_al->gen_log,
						MODULE_NAME ":%s sdio_read"
						" failed: %d, ch %s\n",
						__func__, ret,
						ch->name);
					return ret;
				}

				if (time_after(jiffies, flush_expires) != 0) {
					sdio_al_loge(&sdio_al->gen_log,
						MODULE_NAME ":%s flush rx "
						"packets timeout: ch %s\n",
						__func__, ch->name);
					sdio_al_get_into_err_state(sdio_al_dev);
					return -EBUSY;
				}
			}
			msleep(100);
			if (ch->signature != SDIO_AL_SIGNATURE) {
					sdio_al_loge(&sdio_al->gen_log,
						MODULE_NAME ":%s: after sleep,"
						" invalid signature"
						" 0x%x\n", __func__,
						ch->signature);
				return -ENODEV;
			}
			if (sdio_al_claim_mutex_and_verify_dev(ch->sdio_al_dev,
							       __func__))
				return -ENODEV;

			ret = read_mailbox(sdio_al_dev, false);
			if (ret) {
				sdio_al_loge(&sdio_al->gen_log,
						MODULE_NAME ":%s: failed to"
						" read mailbox", __func__);
				goto error_exit;
			}
			sdio_al_release_mutex(ch->sdio_al_dev, __func__);
		} while (ch->read_avail > 0);
	}
	if (sdio_al_claim_mutex_and_verify_dev(ch->sdio_al_dev,
					       __func__))
		return -ENODEV;
	/* disable function to be able to open the channel again */
	ret = sdio_disable_func(ch->func);
	if (ret) {
		sdio_al_loge(&sdio_al->gen_log,
			MODULE_NAME ":Fail to disable Func#%d\n",
			ch->func->num);
		goto error_exit;
	}
	ch->state = SDIO_CHANNEL_STATE_CLOSED;
	CLOSE_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":%s: Ch %s closed "
				"successfully\n", __func__, ch->name);

error_exit:
	sdio_al_release_mutex(ch->sdio_al_dev, __func__);

	return ret;
}

/**
 *  Close SDIO Channel.
 *
 */
int sdio_close(struct sdio_channel *ch)
{
	return channel_close(ch, true);
}
EXPORT_SYMBOL(sdio_close);

/**
 *  Get the number of available bytes to write.
 *
 */
int sdio_write_avail(struct sdio_channel *ch)
{
	if (!ch) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL "
				"channel\n", __func__);
		return -ENODEV;
	}
	if (ch->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: "
				"Invalid signature 0x%x\n",  __func__,
				ch->signature);
		return -ENODEV;
	}
	if (ch->state != SDIO_CHANNEL_STATE_OPEN) {
		sdio_al_loge(ch->sdio_al_dev->dev_log, MODULE_NAME ":%s: "
				"channel %s state is not open (%d)\n",
				__func__, ch->name, ch->state);
		return -ENODEV;
	}
	pr_debug(MODULE_NAME ":sdio_write_avail %s 0x%x\n",
			 ch->name, ch->write_avail);

	return ch->write_avail;
}
EXPORT_SYMBOL(sdio_write_avail);

/**
 *  Get the number of available bytes to read.
 *
 */
int sdio_read_avail(struct sdio_channel *ch)
{
	if (!ch) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL "
				"channel\n", __func__);
		return -ENODEV;
	}
	if (ch->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: "
				"Invalid signature 0x%x\n",  __func__,
				ch->signature);
		return -ENODEV;
	}
	if (ch->state != SDIO_CHANNEL_STATE_OPEN) {
		sdio_al_loge(ch->sdio_al_dev->dev_log, MODULE_NAME ":%s: "
				"channel %s state is not open (%d)\n",
				__func__, ch->name, ch->state);
		return -ENODEV;
	}
	pr_debug(MODULE_NAME ":sdio_read_avail %s 0x%x\n",
			 ch->name, ch->read_avail);

	return ch->read_avail;
}
EXPORT_SYMBOL(sdio_read_avail);

static int sdio_read_from_closed_ch(struct sdio_channel *ch, int len)
{
	int ret = 0;
	struct sdio_al_device *sdio_al_dev = NULL;

	if (!ch) {
		sdio_al_loge(ch->sdio_al_dev->dev_log,
			MODULE_NAME ":%s: NULL channel\n",  __func__);
		return -ENODEV;
	}

	sdio_al_dev = ch->sdio_al_dev;
	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	ret = sdio_memcpy_fromio(ch->func, sdio_al_dev->rx_flush_buf,
				 PIPE_RX_FIFO_ADDR, len);

	if (ret) {
		sdio_al_loge(ch->sdio_al_dev->dev_log,
				MODULE_NAME ":ch %s: %s err=%d, len=%d\n",
				ch->name, __func__, -ret, len);
		sdio_al_dev->is_err = true;
		sdio_al_release_mutex(sdio_al_dev, __func__);
		return ret;
	}

	restart_inactive_time(sdio_al_dev);

	sdio_al_release_mutex(sdio_al_dev, __func__);

	return 0;
}

/**
 *  Internal read from SDIO Channel.
 *
 *  Reading from the pipe will trigger interrupt if there are
 *  other pending packets on the SDIO-Client.
 *
 */
static int sdio_read_internal(struct sdio_channel *ch, void *data, int len)
{
	int ret = 0;
	struct sdio_al_device *sdio_al_dev = NULL;

	if (!ch) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL "
				"channel\n",  __func__);
		return -ENODEV;
	}
	if (!data) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL data\n",
				__func__);
		return -ENODEV;
	}
	if (len == 0) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":channel %s trying"
				" to read 0 bytes\n", ch->name);
		return -EINVAL;
	}

	if (ch->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: Invalid "
				"signature 0x%x\n",  __func__, ch->signature);
		return -ENODEV;
	}

	sdio_al_dev = ch->sdio_al_dev;
	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	if (sdio_al_dev->is_err) {
		SDIO_AL_ERR(__func__);
		ret = -ENODEV;
		goto exit;
	}

	/* lpm policy says we can't go to sleep when we have pending rx data,
	   so either we had rx interrupt and woken up, or we never went to
	   sleep */
	if (sdio_al_dev->is_ok_to_sleep) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":%s: called "
				"when is_ok_to_sleep is set for ch %s, len=%d,"
				" last_any_read_avail=%d, last_read_avail=%d, "
				"last_old_read_avail=%d", __func__, ch->name,
				len, ch->statistics.last_any_read_avail,
				ch->statistics.last_read_avail,
				ch->statistics.last_old_read_avail);
	}
	BUG_ON(sdio_al_dev->is_ok_to_sleep);

	if ((ch->state != SDIO_CHANNEL_STATE_OPEN) &&
			(ch->state != SDIO_CHANNEL_STATE_CLOSING)) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":%s wrong "
				"channel %s state %d\n",
				__func__, ch->name, ch->state);
		ret = -EINVAL;
		goto exit;
	}

	DATA_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":start ch %s read %d "
			"avail %d.\n", ch->name, len, ch->read_avail);

	restart_inactive_time(sdio_al_dev);

	if ((ch->is_packet_mode) && (len != ch->read_avail)) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":sdio_read ch "
				"%s len != read_avail\n", ch->name);
		ret = -EINVAL;
		goto exit;
	}

	if (len > ch->read_avail) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":ERR ch %s: "
				"reading more bytes (%d) than the avail(%d).\n",
				ch->name, len, ch->read_avail);
		ret = -ENOMEM;
		goto exit;
	}

	ret = sdio_memcpy_fromio(ch->func, data, PIPE_RX_FIFO_ADDR, len);

	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":ch %s: "
				"sdio_read err=%d, len=%d, read_avail=%d, "
				"last_read_avail=%d, last_old_read_avail=%d\n",
				ch->name, -ret, len, ch->read_avail,
				ch->statistics.last_read_avail,
				ch->statistics.last_old_read_avail);
		sdio_al_get_into_err_state(sdio_al_dev);
		goto exit;
	}

	ch->statistics.total_read_times++;

	/* Remove handled packet from the list regardless if ret is ok */
	if (ch->is_packet_mode)
		remove_handled_rx_packet(ch);
	else
		ch->read_avail -= len;

	ch->total_rx_bytes += len;
	DATA_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":end ch %s read %d "
			"avail %d total %d.\n", ch->name, len,
			ch->read_avail, ch->total_rx_bytes);

	if ((ch->read_avail == 0) && !(ch->is_packet_mode))
		ask_reading_mailbox(sdio_al_dev);

exit:
	sdio_al_release_mutex(sdio_al_dev, __func__);

	return ret;
}

/**
 *  Read from SDIO Channel.
 *
 *  Reading from the pipe will trigger interrupt if there are
 *  other pending packets on the SDIO-Client.
 *
 */
int sdio_read(struct sdio_channel *ch, void *data, int len)
{
	if (!ch) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL "
				"channel\n", __func__);
		return -ENODEV;
	}
	if (ch->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: "
			"Invalid signature 0x%x\n",  __func__, ch->signature);
		return -ENODEV;
	}
	if (ch->state == SDIO_CHANNEL_STATE_OPEN) {
		return sdio_read_internal(ch, data, len);
	} else {
		sdio_al_loge(ch->sdio_al_dev->dev_log, MODULE_NAME
				":%s: Invalid channel %s state %d\n",
				__func__, ch->name, ch->state);
	}
	return -ENODEV;
}
EXPORT_SYMBOL(sdio_read);

/**
 *  Write to SDIO Channel.
 *
 */
int sdio_write(struct sdio_channel *ch, const void *data, int len)
{
	int ret = 0;
	struct sdio_al_device *sdio_al_dev = NULL;

	if (!ch) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL "
				"channel\n",  __func__);
		return -ENODEV;
	}
	if (!data) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: NULL data\n",
				__func__);
		return -ENODEV;
	}
	if (len == 0) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":channel %s trying"
				" to write 0 bytes\n", ch->name);
		return -EINVAL;
	}

	if (ch->signature != SDIO_AL_SIGNATURE) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":%s: Invalid "
				"signature 0x%x\n",  __func__, ch->signature);
		return -ENODEV;
	}

	sdio_al_dev = ch->sdio_al_dev;
	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	WARN_ON(len > ch->write_avail);

	if (sdio_al_dev->is_err) {
		SDIO_AL_ERR(__func__);
		ret = -ENODEV;
		goto exit;
	}

	if (ch->state != SDIO_CHANNEL_STATE_OPEN) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":writing to "
				"closed channel %s\n", ch->name);
		ret = -EINVAL;
		goto exit;
	}

	if (sdio_al_dev->is_ok_to_sleep) {
		ret = sdio_al_wake_up(sdio_al_dev, 1, ch);
		if (ret)
			goto exit;
	} else {
		restart_inactive_time(sdio_al_dev);
	}

	DATA_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":start ch %s write %d "
			"avail %d.\n", ch->name, len, ch->write_avail);

	if (len > ch->write_avail) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":ERR ch %s: "
				"write more bytes (%d) than  available %d.\n",
				ch->name, len, ch->write_avail);
		ret = -ENOMEM;
		goto exit;
	}

	ret = sdio_ch_write(ch, data, len);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":sdio_write "
				"on channel %s err=%d\n", ch->name, -ret);
		goto exit;
	}

	ch->total_tx_bytes += len;
	DATA_DEBUG(sdio_al_dev->dev_log, MODULE_NAME ":end ch %s write %d "
			"avail %d total %d.\n", ch->name, len,
			ch->write_avail, ch->total_tx_bytes);

	/* Round up to whole buffer size */
	len = ROUND_UP(len, ch->peer_tx_buf_size);
	/* Protect from wraparound */
	len = min(len, (int) ch->write_avail);
	ch->write_avail -= len;

	if (ch->write_avail < ch->min_write_avail)
		ask_reading_mailbox(sdio_al_dev);

exit:
	sdio_al_release_mutex(sdio_al_dev, __func__);

	return ret;
}
EXPORT_SYMBOL(sdio_write);

static int __devinit msm_sdio_al_probe(struct platform_device *pdev)
{
	if (!sdio_al) {
		pr_err(MODULE_NAME ": %s: NULL sdio_al\n", __func__);
		return -ENODEV;
	}

	sdio_al->pdata = pdev->dev.platform_data;
	return 0;
}

static int __devexit msm_sdio_al_remove(struct platform_device *pdev)
{
	return 0;
}

static void sdio_al_close_all_channels(struct sdio_al_device *sdio_al_dev)
{
	int j;
	int ret;
	struct sdio_channel *ch = NULL;

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s", __func__);

	if (!sdio_al_dev) {
		sdio_al_loge(sdio_al_dev->dev_log,
			MODULE_NAME ": %s: NULL device", __func__);
		return;
	}
	for (j = 0; j < SDIO_AL_MAX_CHANNELS; j++) {
		ch = &sdio_al_dev->channel[j];

		if (ch->state == SDIO_CHANNEL_STATE_OPEN) {
			sdio_al_loge(sdio_al_dev->dev_log,
				MODULE_NAME ": %s: Call to sdio_close() for"
				" ch %s\n", __func__, ch->name);
			ret = channel_close(ch, false);
			if (ret) {
				sdio_al_loge(sdio_al_dev->dev_log,
					MODULE_NAME ": %s: failed sdio_close()"
					" for ch %s (%d)\n",
					__func__, ch->name, ret);
			}
		} else {
			pr_debug(MODULE_NAME ": %s: skip sdio_close() ch %s"
					" (state=%d)\n", __func__,
					ch->name, ch->state);
		}
	}
}

static void sdio_al_invalidate_sdio_clients(struct sdio_al_device *sdio_al_dev,
					    struct platform_device **pdev_arr)
{
	int j;

	pr_debug(MODULE_NAME ": %s: Notifying SDIO clients for card %d",
			__func__, sdio_al_dev->host->index);
	for (j = 0; j < SDIO_AL_MAX_CHANNELS; ++j) {
		if (sdio_al_dev->channel[j].state ==
			SDIO_CHANNEL_STATE_INVALID)
			continue;
		pdev_arr[j] = sdio_al_dev->channel[j].pdev;
		sdio_al_dev->channel[j].signature = 0x0;
		sdio_al_dev->channel[j].state =
			SDIO_CHANNEL_STATE_INVALID;
	}
}

static void sdio_al_modem_reset_operations(struct sdio_al_device
							*sdio_al_dev)
{
	int ret = 0;
	struct platform_device *pdev_arr[SDIO_AL_MAX_CHANNELS];
	int j;

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s", __func__);

	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return;

	if (sdio_al_dev->state == CARD_REMOVED) {
		sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s: "
			"card %d is already removed", __func__,
			sdio_al_dev->host->index);
		goto exit_err;
	}

	if (sdio_al_dev->state == MODEM_RESTART) {
		sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ": %s: "
			"card %d was already notified for modem reset",
			__func__, sdio_al_dev->host->index);
		goto exit_err;
	}

	sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ": %s: Set the "
		"state to MODEM_RESTART for card %d",
		__func__, sdio_al_dev->host->index);
	sdio_al_dev->state = MODEM_RESTART;
	sdio_al_dev->is_ready = false;

	/* Stop mailbox timer */
	stop_and_del_timer(sdio_al_dev);

	if ((sdio_al_dev->is_ok_to_sleep) &&
	    (!sdio_al_dev->is_err)) {
		pr_debug(MODULE_NAME ": %s: wakeup modem for "
				    "card %d", __func__,
			sdio_al_dev->host->index);
		ret = sdio_al_wake_up(sdio_al_dev, 1, NULL);
	}

	if (!ret && (!sdio_al_dev->is_err) && sdio_al_dev->card &&
		sdio_al_dev->card->sdio_func[0]) {
			sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME
			": %s: sdio_release_irq for card %d",
			__func__,
			sdio_al_dev->host->index);
			sdio_release_irq(sdio_al_dev->card->sdio_func[0]);
	}

	memset(pdev_arr, 0, sizeof(pdev_arr));
	sdio_al_invalidate_sdio_clients(sdio_al_dev, pdev_arr);

	sdio_al_release_mutex(sdio_al_dev, __func__);

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s: Notifying SDIO "
						    "clients for card %d",
			__func__, sdio_al_dev->host->index);
	for (j = 0; j < SDIO_AL_MAX_CHANNELS; j++) {
		if (!pdev_arr[j])
			continue;
		platform_device_unregister(pdev_arr[j]);
	}
	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s: Finished Notifying "
						    "SDIO clients for card %d",
			__func__, sdio_al_dev->host->index);

	return;

exit_err:
	sdio_al_release_mutex(sdio_al_dev, __func__);
	return;
}

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
static void sdio_al_reset(void)
{
	int i;
	struct sdio_al_device *sdio_al_dev;

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s", __func__);

	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES; i++) {
		if (sdio_al->devices[i] == NULL) {
			pr_debug(MODULE_NAME ": %s: NULL device in index %d",
					__func__, i);
			continue;
		}
		sdio_al_dev = sdio_al->devices[i];
		sdio_al_modem_reset_operations(sdio_al->devices[i]);
	}

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s completed", __func__);
}
#endif

static void msm_sdio_al_shutdown(struct platform_device *pdev)
{
	int i;
	struct sdio_al_device *sdio_al_dev;

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME
			"Initiating msm_sdio_al_shutdown...");

	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES; i++) {
		if (sdio_al->devices[i] == NULL) {
			pr_debug(MODULE_NAME ": %s: NULL device in index %d",
					__func__, i);
			continue;
		}
		sdio_al_dev = sdio_al->devices[i];

		if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
			return;

		if (sdio_al_dev->ch_close_supported)
			sdio_al_close_all_channels(sdio_al_dev);

		sdio_al_release_mutex(sdio_al_dev, __func__);

		sdio_al_modem_reset_operations(sdio_al_dev);
	}
	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s: "
		"msm_sdio_al_shutdown complete.", __func__);
}

static struct platform_driver msm_sdio_al_driver = {
	.probe          = msm_sdio_al_probe,
	.remove         = __exit_p(msm_sdio_al_remove),
	.shutdown	= msm_sdio_al_shutdown,
	.driver         = {
		.name   = "msm_sdio_al",
	},
};

/**
 *  Initialize SDIO_AL channels.
 *
 */
static int init_channels(struct sdio_al_device *sdio_al_dev)
{
	int ret = 0;
	int i;

	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	ret = read_sdioc_software_header(sdio_al_dev,
					 sdio_al_dev->sdioc_sw_header);
	if (ret)
		goto exit;

	ret = sdio_al_setup(sdio_al_dev);
	if (ret)
		goto exit;

	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
		int ch_name_size;
		if (sdio_al_dev->channel[i].state == SDIO_CHANNEL_STATE_INVALID)
			continue;
		if (sdio_al->unittest_mode) {
			memset(sdio_al_dev->channel[i].ch_test_name, 0,
				sizeof(sdio_al_dev->channel[i].ch_test_name));
			ch_name_size = strnlen(sdio_al_dev->channel[i].name,
				       CHANNEL_NAME_SIZE);
			strncpy(sdio_al_dev->channel[i].ch_test_name,
			       sdio_al_dev->channel[i].name,
			       ch_name_size);
			strncat(sdio_al_dev->channel[i].ch_test_name +
			       ch_name_size,
			       SDIO_TEST_POSTFIX,
			       SDIO_TEST_POSTFIX_SIZE);
			pr_debug(MODULE_NAME ":pdev.name = %s\n",
				sdio_al_dev->channel[i].ch_test_name);
			sdio_al_dev->channel[i].pdev = platform_device_alloc(
				sdio_al_dev->channel[i].ch_test_name, -1);
		} else {
			pr_debug(MODULE_NAME ":pdev.name = %s\n",
				sdio_al_dev->channel[i].name);
			sdio_al_dev->channel[i].pdev = platform_device_alloc(
				sdio_al_dev->channel[i].name, -1);
		}
		if (!sdio_al_dev->channel[i].pdev) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					":NULL platform device for ch %s",
					sdio_al_dev->channel[i].name);
			sdio_al_dev->channel[i].state =
				SDIO_CHANNEL_STATE_INVALID;
			continue;
		}
		ret = platform_device_add(sdio_al_dev->channel[i].pdev);
		if (ret) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					":platform_device_add failed, "
					"ret=%d\n", ret);
			sdio_al_dev->channel[i].state =
				SDIO_CHANNEL_STATE_INVALID;
		}
	}

exit:
	sdio_al_release_mutex(sdio_al_dev, __func__);
	return ret;
}

/**
 *  Initialize SDIO_AL channels according to the client setup.
 *  This function also check if the client is in boot mode and
 *  flashless boot is required to be activated or the client is
 *  up and running.
 *
 */
static int sdio_al_client_setup(struct sdio_al_device *sdio_al_dev)
{
	int ret = 0;
	struct sdio_func *func1;
	int signature = 0;

	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	if (!sdio_al_dev->card || !sdio_al_dev->card->sdio_func[0]) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":NULL card or "
							       "func1\n");
		sdio_al_release_mutex(sdio_al_dev, __func__);
		return -ENODEV;
	}
	func1 = sdio_al_dev->card->sdio_func[0];

	/* Read the header signature to determine the status of the MDM
	 * SDIO Client
	 */
	signature = sdio_readl(func1, SDIOC_SW_HEADER_ADDR, &ret);
	sdio_al_release_mutex(sdio_al_dev, __func__);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":fail to read "
				"signature from sw header.\n");
		return ret;
	}

	switch (signature) {
	case PEER_SDIOC_SW_MAILBOX_BOOT_SIGNATURE:
		if (sdio_al_dev == sdio_al->bootloader_dev) {
			sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":setup "
					"bootloader on card %d\n",
					sdio_al_dev->host->index);
			return sdio_al_bootloader_setup();
		} else {
			sdio_al_logi(sdio_al_dev->dev_log, MODULE_NAME ":wait "
					"for bootloader completion "
					"on card %d\n",
					sdio_al_dev->host->index);
			return sdio_al_wait_for_bootloader_comp(sdio_al_dev);
		}
	case PEER_SDIOC_SW_MAILBOX_SIGNATURE:
	case PEER_SDIOC_SW_MAILBOX_UT_SIGNATURE:
		return init_channels(sdio_al_dev);
	default:
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Invalid "
				"signature 0x%x\n", signature);
		return -EINVAL;
	}

	return 0;
}

static void clean_sdio_al_device_data(struct sdio_al_device *sdio_al_dev)
{
	sdio_al_dev->is_ready = 0;
	sdio_al_dev->bootloader_done = 0;
	sdio_al_dev->lpm_chan = 0;
	sdio_al_dev->is_ok_to_sleep = 0;
	sdio_al_dev->inactivity_time = 0;
	sdio_al_dev->poll_delay_msec = 0;
	sdio_al_dev->is_timer_initialized = 0;
	sdio_al_dev->is_err = 0;
	sdio_al_dev->is_suspended = 0;
	sdio_al_dev->flashless_boot_on = 0;
	sdio_al_dev->ch_close_supported = 0;
	sdio_al_dev->print_after_interrupt = 0;
	memset(sdio_al_dev->sdioc_sw_header, 0,
	       sizeof(*sdio_al_dev->sdioc_sw_header));
	memset(sdio_al_dev->mailbox, 0, sizeof(*sdio_al_dev->mailbox));
	memset(sdio_al_dev->rx_flush_buf, 0,
	       sizeof(*sdio_al_dev->rx_flush_buf));
}

/*
 * SDIO driver functions
 */
static int sdio_al_sdio_probe(struct sdio_func *func,
		const struct sdio_device_id *sdio_dev_id)
{
	int ret = 0;
	struct sdio_al_device *sdio_al_dev = NULL;
	int i;
	struct mmc_card *card = NULL;

	if (!func) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL func\n",
				__func__);
		return -ENODEV;
	}
	card = func->card;

	if (!card) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL card\n",
				__func__);
		return -ENODEV;
	}

	if (!card->sdio_func[0]) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
							    "func1\n",
				__func__);
		return -ENODEV;
	}

	if (card->sdio_funcs < SDIO_AL_MAX_FUNCS) {
		dev_info(&card->dev,
			 "SDIO-functions# %d less than expected.\n",
			 card->sdio_funcs);
		return -ENODEV;
	}

	/* Check if there is already a device for this card */
	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES; ++i) {
		if (sdio_al->devices[i] == NULL)
			continue;
		if (sdio_al->devices[i]->host == card->host) {
			sdio_al_dev = sdio_al->devices[i];
			if (sdio_al_dev->state == CARD_INSERTED)
				return 0;
			clean_sdio_al_device_data(sdio_al_dev);
			break;
		}
	}

	if (!sdio_al_dev) {
		sdio_al_dev = kzalloc(sizeof(struct sdio_al_device),
				      GFP_KERNEL);
		if (sdio_al_dev == NULL)
			return -ENOMEM;

		for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES ; ++i)
			if (sdio_al->devices[i] == NULL) {
				sdio_al->devices[i] = sdio_al_dev;
				sdio_al_dev->dev_log = &sdio_al->device_log[i];
				spin_lock_init(&sdio_al_dev->dev_log->log_lock);
	#ifdef CONFIG_DEBUG_FS
				sdio_al_dbgfs_log[i].data =
						sdio_al_dev->dev_log->buffer;
				sdio_al_dbgfs_log[i].size =
					SDIO_AL_DEBUG_LOG_SIZE;
	#endif
				break;
			}
		if (i == MAX_NUM_OF_SDIO_DEVICES) {
			sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ":No space "
					"in devices array for the device\n");
			return -ENOMEM;
		}
	}

	dev_info(&card->dev, "SDIO Card claimed.\n");
	sdio_al->skip_print_info = 0;

	sdio_al_dev->state = CARD_INSERTED;

	if (card->host->index == SDIO_BOOTLOADER_CARD_INDEX)
		sdio_al->bootloader_dev = sdio_al_dev;

	sdio_al_dev->is_ready = false;

	sdio_al_dev->signature = SDIO_AL_SIGNATURE;

	sdio_al_dev->is_suspended = 0;
	sdio_al_dev->is_timer_initialized = false;

	sdio_al_dev->lpm_chan = INVALID_SDIO_CHAN;

	sdio_al_dev->card = card;
	sdio_al_dev->host = card->host;

	if (!sdio_al_dev->mailbox) {
		sdio_al_dev->mailbox = kzalloc(sizeof(struct sdio_mailbox),
					       GFP_KERNEL);
		if (sdio_al_dev->mailbox == NULL)
			return -ENOMEM;
	}

	if (!sdio_al_dev->sdioc_sw_header) {
		sdio_al_dev->sdioc_sw_header
			= kzalloc(sizeof(*sdio_al_dev->sdioc_sw_header),
				  GFP_KERNEL);
		if (sdio_al_dev->sdioc_sw_header == NULL)
			return -ENOMEM;
	}

	if (!sdio_al_dev->rx_flush_buf) {
		sdio_al_dev->rx_flush_buf = kzalloc(RX_FLUSH_BUFFER_SIZE,
						    GFP_KERNEL);
		if (sdio_al_dev->rx_flush_buf == NULL) {
			sdio_al_loge(&sdio_al->gen_log,
					MODULE_NAME ":Fail to allocate "
					   "rx_flush_buf for card %d\n",
			       card->host->index);
			return -ENOMEM;
		}
	}

	sdio_al_dev->timer.data = (unsigned long)sdio_al_dev;

	wake_lock_init(&sdio_al_dev->wake_lock, WAKE_LOCK_SUSPEND, MODULE_NAME);
	/* Don't allow sleep until all required clients register */
	sdio_al_vote_for_sleep(sdio_al_dev, 0);

	if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
		return -ENODEV;

	/* Init Func#1 */
	ret = sdio_al_enable_func_retry(card->sdio_func[0], "Init Func#1");
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Fail to "
				"enable Func#%d\n", card->sdio_func[0]->num);
		goto exit;
	}

	/* Patch Func CIS tuple issue */
	ret = sdio_set_block_size(card->sdio_func[0], SDIO_AL_BLOCK_SIZE);
	if (ret) {
		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ":Fail to set "
			"block size, Func#%d\n", card->sdio_func[0]->num);
		goto exit;
	}
	sdio_al_dev->card->sdio_func[0]->max_blksize = SDIO_AL_BLOCK_SIZE;

	sdio_al_dev->workqueue = create_singlethread_workqueue("sdio_al_wq");
	sdio_al_dev->sdio_al_work.sdio_al_dev = sdio_al_dev;
	init_waitqueue_head(&sdio_al_dev->wait_mbox);

	ret = sdio_al_client_setup(sdio_al_dev);

exit:
	sdio_al_release_mutex(sdio_al_dev, __func__);
	return ret;
}

static void sdio_al_sdio_remove(struct sdio_func *func)
{
	struct sdio_al_device *sdio_al_dev = NULL;
	int i;
	struct mmc_card *card = NULL;
	struct platform_device *pdev_arr[SDIO_AL_MAX_CHANNELS];

	if (!func) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL func\n",
				__func__);
		return;
	}
	card = func->card;

	if (!card) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL card\n",
				__func__);
		return;
	}

	/* Find the sdio_al_device of this card */
	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES; ++i) {
		if (sdio_al->devices[i] == NULL)
			continue;
		if (sdio_al->devices[i]->card == card) {
			sdio_al_dev = sdio_al->devices[i];
			break;
		}
	}
	if (sdio_al_dev == NULL) {
		pr_debug(MODULE_NAME ":%s :NULL sdio_al_dev for card %d\n",
				 __func__, card->host->index);
		return;
	}

	if (sdio_al_claim_mutex(sdio_al_dev, __func__))
		return;

	if (sdio_al_dev->state == CARD_REMOVED) {
		sdio_al_release_mutex(sdio_al_dev, __func__);
		return;
	}

	if (!card->sdio_func[0]) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: NULL "
						"func1\n", __func__);
		sdio_al_release_mutex(sdio_al_dev, __func__);
		return;
	}

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":%s for card %d\n",
			 __func__, card->host->index);

	sdio_al_dev->state = CARD_REMOVED;

	memset(pdev_arr, 0, sizeof(pdev_arr));
	sdio_al_invalidate_sdio_clients(sdio_al_dev, pdev_arr);

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":%s: ask_reading_mailbox "
			"for card %d\n", __func__, card->host->index);
	sdio_al_dev->is_ready = false; /* Flag worker to exit */
	sdio_al_dev->ask_mbox = false;
	ask_reading_mailbox(sdio_al_dev); /* Wakeup worker */

	stop_and_del_timer(sdio_al_dev);

	sdio_al_release_mutex(sdio_al_dev, __func__);

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s: Notifying SDIO "
						    "clients for card %d",
			__func__, sdio_al_dev->host->index);
	for (i = 0; i < SDIO_AL_MAX_CHANNELS; i++) {
		if (!pdev_arr[i])
			continue;
		platform_device_unregister(pdev_arr[i]);
	}
	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s: Finished Notifying "
						    "SDIO clients for card %d",
			__func__, sdio_al_dev->host->index);

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":%s: vote for sleep for "
			"card %d\n", __func__, card->host->index);
	sdio_al_vote_for_sleep(sdio_al_dev, 1);

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":%s: flush_workqueue for "
			"card %d\n", __func__, card->host->index);
	flush_workqueue(sdio_al_dev->workqueue);
	destroy_workqueue(sdio_al_dev->workqueue);
	wake_lock_destroy(&sdio_al_dev->wake_lock);

	sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ":%s: sdio card %d removed."
			"\n", __func__,	card->host->index);
}

static void sdio_print_mailbox(char *prefix_str, struct sdio_mailbox *mailbox)
{
	int k = 0;
	char buf[256];
	char buf1[10];

	if (!mailbox) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": mailbox is "
				"NULL\n");
		return;
	}

	sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s: pipes 0_7: eot=0x%x,"
		" thresh=0x%x, overflow=0x%x, "
		"underflow=0x%x, mask_thresh=0x%x\n",
		 prefix_str, mailbox->eot_pipe_0_7,
		 mailbox->thresh_above_limit_pipe_0_7,
		 mailbox->overflow_pipe_0_7,
		 mailbox->underflow_pipe_0_7,
		 mailbox->mask_thresh_above_limit_pipe_0_7);

	memset(buf, 0, sizeof(buf));
	strncat(buf, ": bytes_avail:", sizeof(buf));

	for (k = 0 ; k < SDIO_AL_ACTIVE_PIPES ; ++k) {
		snprintf(buf1, sizeof(buf1), "%d, ",
			 mailbox->pipe_bytes_avail[k]);
		strncat(buf, buf1, sizeof(buf));
	}

	sdio_al_loge(&sdio_al->gen_log, MODULE_NAME "%s", buf);
}

static void sdio_al_print_info(void)
{
	int i = 0;
	int j = 0;
	int ret = 0;
	struct sdio_mailbox *mailbox = NULL;
	struct sdio_mailbox *hw_mailbox = NULL;
	struct peer_sdioc_channel_config *ch_config = NULL;
	struct sdio_func *func1 = NULL;
	struct sdio_func *lpm_func = NULL;
	int offset = 0;
	int is_ok_to_sleep = 0;
	char buf[50];

	if (sdio_al->skip_print_info == 1)
		return;

	sdio_al->skip_print_info = 1;

	sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s - SDIO DEBUG INFO\n",
			__func__);

	if (!sdio_al) {
		sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": %s - ERROR - "
				"sdio_al is NULL\n",  __func__);
		return;
	}

	sdio_al_loge(&sdio_al->gen_log, MODULE_NAME ": GPIO mdm2ap_status=%d\n",
				sdio_al->pdata->get_mdm2ap_status());

	for (j = 0 ; j < MAX_NUM_OF_SDIO_DEVICES ; ++j) {
		struct sdio_al_device *sdio_al_dev = sdio_al->devices[j];

		if (sdio_al_dev == NULL) {
			continue;
		}

		if (!sdio_al_dev->host) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": Host"
					" is NULL\n);");
			continue;
		}

		snprintf(buf, sizeof(buf), "Card#%d: Shadow HW MB",
		       sdio_al_dev->host->index);

		/* printing Shadowing HW Mailbox*/
		mailbox = sdio_al_dev->mailbox;
		sdio_print_mailbox(buf, mailbox);

		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": Card#%d: "
			"is_ok_to_sleep=%d\n",
			sdio_al_dev->host->index,
			sdio_al_dev->is_ok_to_sleep);


		sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME ": Card#%d: "
				   "Shadow channels SW MB:",
		       sdio_al_dev->host->index);

		/* printing Shadowing SW Mailbox per channel*/
		for (i = 0 ; i < SDIO_AL_MAX_CHANNELS ; ++i) {
			struct sdio_channel *ch = &sdio_al_dev->channel[i];

			if (ch == NULL) {
				continue;
			}

			if (ch->state == SDIO_CHANNEL_STATE_INVALID)
				continue;

			ch_config = &sdio_al_dev->channel[i].ch_config;

			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": Ch %s: max_rx_thres=0x%x, "
				"max_tx_thres=0x%x, tx_buf=0x%x, "
				"is_packet_mode=%d, "
				"max_packet=0x%x, min_write=0x%x",
				ch->name, ch_config->max_rx_threshold,
				ch_config->max_tx_threshold,
				ch_config->tx_buf_size,
				ch_config->is_packet_mode,
				ch_config->max_packet_size,
				ch->min_write_avail);

			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": total_rx=0x%x, total_tx=0x%x, "
				"read_avail=0x%x, write_avail=0x%x, "
				"rx_pending=0x%x, num_reads=0x%x, "
				"num_notifs=0x%x", ch->total_rx_bytes,
				ch->total_tx_bytes, ch->read_avail,
				ch->write_avail, ch->rx_pending_bytes,
				ch->statistics.total_read_times,
				ch->statistics.total_notifs);
		} /* end loop over all channels */

	} /* end loop over all devices */

	/* reading from client and printing is_host_ok_to_sleep per device */
	for (j = 0 ; j < MAX_NUM_OF_SDIO_DEVICES ; ++j) {
		struct sdio_al_device *sdio_al_dev = sdio_al->devices[j];

		if (sdio_al_verify_func1(sdio_al_dev, __func__))
			continue;

		if (!sdio_al_dev->host) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": Host is NULL");
			continue;
		}

		if (sdio_al_dev->lpm_chan == INVALID_SDIO_CHAN) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
				": %s - for Card#%d, is lpm_chan=="
				"INVALID_SDIO_CHAN. continuing...",
				__func__, sdio_al_dev->host->index);
			continue;
		}

		offset = offsetof(struct peer_sdioc_sw_mailbox, ch_config)+
		sizeof(struct peer_sdioc_channel_config) *
		sdio_al_dev->lpm_chan+
		offsetof(struct peer_sdioc_channel_config, is_host_ok_to_sleep);

		lpm_func = sdio_al_dev->card->sdio_func[sdio_al_dev->
								lpm_chan+1];
		if (!lpm_func) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": %s - lpm_func is NULL for card#%d"
					" continuing...\n", __func__,
					sdio_al_dev->host->index);
			continue;
		}

		if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
			return;
		ret  =  sdio_memcpy_fromio(lpm_func,
					    &is_ok_to_sleep,
					    SDIOC_SW_MAILBOX_ADDR+offset,
					    sizeof(int));
		sdio_al_release_mutex(sdio_al_dev, __func__);

		if (ret)
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": %s - fail to read "
				"is_HOST_ok_to_sleep from mailbox for card %d",
				__func__, sdio_al_dev->host->index);
		else
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": Card#%d: "
				"is_HOST_ok_to_sleep=%d\n",
				sdio_al_dev->host->index,
				is_ok_to_sleep);
	}

	for (j = 0 ; j < MAX_NUM_OF_SDIO_DEVICES ; ++j) {
		struct sdio_al_device *sdio_al_dev = sdio_al->devices[j];

		if (!sdio_al_dev)
			continue;

		/* Reading HW Mailbox */
		hw_mailbox = sdio_al_dev->mailbox;

		if (sdio_al_claim_mutex_and_verify_dev(sdio_al_dev, __func__))
			return;

		if (!sdio_al_dev->card || !sdio_al_dev->card->sdio_func[0]) {
			sdio_al_release_mutex(sdio_al_dev, __func__);
			return;
		}
		func1 = sdio_al_dev->card->sdio_func[0];
		ret = sdio_memcpy_fromio(func1, hw_mailbox,
			HW_MAILBOX_ADDR, sizeof(*hw_mailbox));
		sdio_al_release_mutex(sdio_al_dev, __func__);

		if (ret) {
			sdio_al_loge(sdio_al_dev->dev_log, MODULE_NAME
					": fail to read "
			       "mailbox for card#%d. "
			       "continuing...\n",
			       sdio_al_dev->host->index);
			continue;
		}

		snprintf(buf, sizeof(buf), "Card#%d: Current HW MB",
		       sdio_al_dev->host->index);

		/* Printing HW Mailbox */
		sdio_print_mailbox(buf, hw_mailbox);
	}
}

static struct sdio_device_id sdio_al_sdioid[] = {
    {.class = 0, .vendor = 0x70, .device = 0x2460},
    {.class = 0, .vendor = 0x70, .device = 0x0460},
    {.class = 0, .vendor = 0x70, .device = 0x23F1},
    {.class = 0, .vendor = 0x70, .device = 0x23F0},
    {}
};

static struct sdio_driver sdio_al_sdiofn_driver = {
    .name      = "sdio_al_sdiofn",
    .id_table  = sdio_al_sdioid,
    .probe     = sdio_al_sdio_probe,
    .remove    = sdio_al_sdio_remove,
};

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
/*
 *  Callback for notifications from restart mudule.
 *  This function handles only the BEFORE_RESTART notification.
 *  Stop all the activity on the card and notify our clients.
 */
static int sdio_al_subsys_notifier_cb(struct notifier_block *this,
				  unsigned long notif_type,
				  void *data)
{
	if (notif_type != SUBSYS_BEFORE_SHUTDOWN) {
		sdio_al_logi(&sdio_al->gen_log, MODULE_NAME ": %s: got "
				"notification %ld", __func__, notif_type);
		return NOTIFY_DONE;
	}

	sdio_al_reset();
	return NOTIFY_OK;
}

static struct notifier_block sdio_al_nb = {
	.notifier_call = sdio_al_subsys_notifier_cb,
};
#endif

/**
 *  Module Init.
 *
 *  @warn: allocate sdio_al context before registering driver.
 *
 */
static int __init sdio_al_init(void)
{
	int ret = 0;
	int i;

	pr_debug(MODULE_NAME ":sdio_al_init\n");

	pr_info(MODULE_NAME ":SDIO-AL SW version %s\n",
		DRV_VERSION);

	sdio_al = kzalloc(sizeof(struct sdio_al), GFP_KERNEL);
	if (sdio_al == NULL)
		return -ENOMEM;

	for (i = 0; i < MAX_NUM_OF_SDIO_DEVICES ; ++i)
		sdio_al->devices[i] = NULL;

	sdio_al->unittest_mode = false;

	sdio_al->debug.debug_lpm_on = debug_lpm_on;
	sdio_al->debug.debug_data_on = debug_data_on;
	sdio_al->debug.debug_close_on = debug_close_on;

#ifdef CONFIG_DEBUG_FS
	sdio_al_debugfs_init();
#endif


#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	sdio_al->subsys_notif_handle = subsys_notif_register_notifier(
		"external_modem", &sdio_al_nb);
#endif

	ret = platform_driver_register(&msm_sdio_al_driver);
	if (ret) {
		pr_err(MODULE_NAME ": platform_driver_register failed: %d\n",
		       ret);
		goto exit;
	}

	sdio_register_driver(&sdio_al_sdiofn_driver);

	spin_lock_init(&sdio_al->gen_log.log_lock);

exit:
	if (ret)
		kfree(sdio_al);
	return ret;
}

/**
 *  Module Exit.
 *
 *  Free allocated memory.
 *  Disable SDIO-Card.
 *  Unregister driver.
 *
 */
static void __exit sdio_al_exit(void)
{
	if (sdio_al == NULL)
		return;

	pr_debug(MODULE_NAME ":sdio_al_exit\n");

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	subsys_notif_unregister_notifier(
		sdio_al->subsys_notif_handle, &sdio_al_nb);
#endif

	sdio_al_tear_down();

	sdio_unregister_driver(&sdio_al_sdiofn_driver);

	kfree(sdio_al);

#ifdef CONFIG_DEBUG_FS
	sdio_al_debugfs_cleanup();
#endif

	platform_driver_unregister(&msm_sdio_al_driver);

	pr_debug(MODULE_NAME ":sdio_al_exit complete\n");
}

module_init(sdio_al_init);
module_exit(sdio_al_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SDIO Abstraction Layer");
MODULE_AUTHOR("Amir Samuelov <amirs@codeaurora.org>");
MODULE_VERSION(DRV_VERSION);

