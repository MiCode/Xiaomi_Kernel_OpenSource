/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
 * SDIO-Downloader
 *
 * To be used with Qualcomm's SDIO-Client connected to this host.
 */

/* INCLUDES */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/mmc/card.h>
#include <linux/dma-mapping.h>
#include <mach/dma.h>
#include <linux/mmc/sdio_func.h>
#include "sdio_al_private.h"
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/debugfs.h>

/* DEFINES AND MACROS */
#define MAX_NUM_DEVICES		1
#define TTY_SDIO_DEV			"tty_sdio_0"
#define TTY_SDIO_DEV_TEST		"tty_sdio_test_0"
#define SDIOC_MAILBOX_ADDRESS		0
#define SDIO_DL_BLOCK_SIZE		512
#define SDIO_DL_MAIN_THREAD_NAME	"sdio_tty_main_thread"
#define SDIOC_DL_BUFF_ADDRESS		0
#define SDIOC_UP_BUFF_ADDRESS		0x4
#define SDIOC_DL_BUFF_SIZE_OFFSET	0x8
#define SDIOC_UP_BUFF_SIZE_OFFSET	0xC
#define SDIOC_DL_WR_PTR		0x10
#define SDIOC_DL_RD_PTR		0x14
#define SDIOC_UL_WR_PTR		0x18
#define SDIOC_UL_RD_PTR		0x1C
#define SDIOC_EXIT_PTR			0x20
#define SDIOC_OP_MODE_PTR		0x24
#define SDIOC_PTRS_OFFSET		0x10
#define SDIOC_PTR_REGS_SIZE		0x10
#define SDIOC_CFG_REGS_SIZE		0x10
#define WRITE_RETRIES			0xFFFFFFFF
#define INPUT_SPEED			4800
#define OUTPUT_SPEED			4800
#define SDIOC_EXIT_CODE		0xDEADDEAD
#define SLEEP_MS			10
#define PRINTING_GAP			200
#define TIMER_DURATION			10
#define PUSH_TIMER_DURATION		5000
#define MULTIPLE_RATIO			1
#define MS_IN_SEC			1000
#define BITS_IN_BYTE			8
#define BYTES_IN_KB			1024
#define WRITE_TILL_END_RETRIES		5
#define SDIO_DLD_NORMAL_MODE_NAME	"SDIO DLD NORMAL MODE"
#define SDIO_DLD_BOOT_TEST_MODE_NAME	"SDIO DLD BOOT TEST MODE"
#define SDIO_DLD_AMSS_TEST_MODE_NAME	"SDIO DLD AMSS TEST MODE"
#define TEST_NAME_MAX_SIZE		30
#define PUSH_STRING
#define SDIO_DLD_OUTGOING_BUFFER_SIZE	(48*1024*MULTIPLE_RATIO)

/* FORWARD DECLARATIONS */
static int sdio_dld_open(struct tty_struct *tty, struct file *file);
static void sdio_dld_close(struct tty_struct *tty, struct file *file);
static int sdio_dld_write_callback(struct tty_struct *tty,
				   const unsigned char *buf, int count);
static int sdio_dld_write_room(struct tty_struct *tty);
static int sdio_dld_main_task(void *card);
static void sdio_dld_print_info(void);
#ifdef CONFIG_DEBUG_FS
static int sdio_dld_debug_info_open(struct inode *inode, struct file *file);
static ssize_t sdio_dld_debug_info_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
#endif


/* STRUCTURES AND TYPES */
enum sdio_dld_op_mode {
	 SDIO_DLD_NO_MODE = 0,
	 SDIO_DLD_NORMAL_MODE = 1,
	 SDIO_DLD_BOOT_TEST_MODE = 2,
	 SDIO_DLD_AMSS_TEST_MODE = 3,
	 SDIO_DLD_NUM_OF_MODES,
};

struct sdioc_reg_sequential_chunk_ptrs {
	unsigned int dl_wr_ptr;
	unsigned int dl_rd_ptr;
	unsigned int up_wr_ptr;
	unsigned int up_rd_ptr;
};

struct sdioc_reg_sequential_chunk_cfg {
	unsigned int dl_buff_address;
	unsigned int up_buff_address;
	unsigned int dl_buff_size;
	unsigned int ul_buff_size;
};

struct sdioc_reg {
	unsigned int reg_val;
	unsigned int reg_offset;
};

struct sdioc_reg_chunk {
	struct sdioc_reg dl_buff_address;
	struct sdioc_reg up_buff_address;
	struct sdioc_reg dl_buff_size;
	struct sdioc_reg ul_buff_size;
	struct sdioc_reg dl_wr_ptr;
	struct sdioc_reg dl_rd_ptr;
	struct sdioc_reg up_wr_ptr;
	struct sdioc_reg up_rd_ptr;
	struct sdioc_reg good_to_exit_ptr;
};

struct sdio_data {
	char *data;
	int offset_read_p;
	int offset_write_p;
	int buffer_size;
	int num_of_bytes_in_use;
};

struct sdio_dld_data {
	struct sdioc_reg_chunk sdioc_reg;
	struct sdio_data incoming_data;
	struct sdio_data outgoing_data;
};

struct sdio_dld_wait_event {
	wait_queue_head_t wait_event;
	int wake_up_signal;
};

struct sdio_dld_task {
	struct task_struct *dld_task;
	const char *task_name;
	struct sdio_dld_wait_event exit_wait;
	atomic_t please_close;
};

#ifdef CONFIG_DEBUG_FS
struct sdio_dloader_debug {
	struct dentry *sdio_dld_debug_root;
	struct dentry *sdio_al_dloader;
};

const struct file_operations sdio_dld_debug_info_ops = {
	.open = sdio_dld_debug_info_open,
	.write = sdio_dld_debug_info_write,
};
#endif

struct sdio_downloader {
	int sdioc_boot_func;
	struct sdio_dld_wait_event write_callback_event;
	struct sdio_dld_task dld_main_thread;
	struct tty_driver *tty_drv;
	struct tty_struct *tty_str;
	struct sdio_dld_data sdio_dloader_data;
	struct mmc_card *card;
	int(*done_callback)(void);
	struct sdio_dld_wait_event main_loop_event;
	struct timer_list timer;
	unsigned int poll_ms;
	struct timer_list push_timer;
	unsigned int push_timer_ms;
	enum sdio_dld_op_mode op_mode;
	char op_mode_name[TEST_NAME_MAX_SIZE];
};

struct sdio_dld_global_info {
	int global_bytes_write_toio;
	int global_bytes_write_tty;
	int global_bytes_read_fromio;
	int global_bytes_push_tty;
	u64 start_time;
	u64 end_time;
	u64 delta_jiffies;
	unsigned int time_msec;
	unsigned int throughput;
	int cl_dl_wr_ptr;
	int cl_dl_rd_ptr;
	int cl_up_wr_ptr;
	int cl_up_rd_ptr;
	int host_read_ptr;
	int host_write_ptr;
	int cl_dl_buffer_size;
	int cl_up_buffer_size;
	int host_outgoing_buffer_size;
	int cl_dl_buffer_address;
	int cl_up_buffer_address;
};

static const struct tty_operations sdio_dloader_tty_ops = {
	.open = sdio_dld_open,
	.close = sdio_dld_close,
	.write = sdio_dld_write_callback,
	.write_room = sdio_dld_write_room,
};

/* GLOBAL VARIABLES */
struct sdio_downloader *sdio_dld;
struct sdio_dld_global_info sdio_dld_info;
static char outgoing_data_buffer[SDIO_DLD_OUTGOING_BUFFER_SIZE];

static DEFINE_SPINLOCK(lock1);
static unsigned long lock_flags1;
static DEFINE_SPINLOCK(lock2);
static unsigned long lock_flags2;

/*
 * sdio_op_mode sets the operation mode of the sdio_dloader -
 * it may be in NORMAL_MODE, BOOT_TEST_MODE or AMSS_TEST_MODE
 */
static int sdio_op_mode = (int)SDIO_DLD_NORMAL_MODE;
module_param(sdio_op_mode, int, 0);

#ifdef CONFIG_DEBUG_FS

struct sdio_dloader_debug sdio_dld_debug;

#define ARR_SIZE 30000
#define SDIO_DLD_DEBUGFS_INIT_VALUE	87654321
#define SDIO_DLD_DEBUGFS_CASE_1_CODE	11111111
#define SDIO_DLD_DEBUGFS_CASE_2_CODE	22222222
#define SDIO_DLD_DEBUGFS_CASE_3_CODE	33333333
#define SDIO_DLD_DEBUGFS_CASE_4_CODE	44444444
#define SDIO_DLD_DEBUGFS_CASE_5_CODE	55555555
#define SDIO_DLD_DEBUGFS_CASE_6_CODE	66666666
#define SDIO_DLD_DEBUGFS_CASE_7_CODE	77777777
#define SDIO_DLD_DEBUGFS_CASE_8_CODE	88888888
#define SDIO_DLD_DEBUGFS_CASE_9_CODE	99999999
#define SDIO_DLD_DEBUGFS_CASE_10_CODE	10101010
#define SDIO_DLD_DEBUGFS_CASE_11_CODE	11001100
#define SDIO_DLD_DEBUGFS_CASE_12_CODE	12001200
#define SDIO_DLD_DEBUGFS_LOOP_WAIT	7
#define SDIO_DLD_DEBUGFS_LOOP_WAKEUP	8
#define SDIO_DLD_DEBUGFS_CB_WAIT	3
#define SDIO_DLD_DEBUGFS_CB_WAKEUP	4

static int curr_index;
struct ptrs {
	int h_w_ptr;
	int h_r_ptr;
	int c_u_w_ptr;
	int c_u_r_ptr;
	int code;
	int h_has_to_send;
	int c_has_to_receive;
	int min_of;
	int reserve2;
	int tty_count;
	int write_tty;
	int write_toio;
	int loop_wait_wake;
	int cb_wait_wake;
	int c_d_w_ptr;
	int c_d_r_ptr;
	int to_read;
	int push_to_tty;
	int global_tty_send;
	int global_sdio_send;
	int global_tty_received;
	int global_sdio_received;
	int reserve22;
	int reserve23;
	int reserve24;
	int reserve25;
	int reserve26;
	int reserve27;
	int reserve28;
	int reserve29;
	int reserve30;
	int reserve31;
};

struct global_data {
	int curr_i;
	int duration_ms;
	int global_bytes_sent;
	int throughput_Mbs;
	int host_outgoing_buffer_size_KB;
	int client_up_buffer_size_KB;
	int client_dl_buffer_size_KB;
	int client_dl_buffer_address;
	int client_up_buffer_address;
	int global_bytes_received;
	int global_bytes_pushed;
	int reserve11;
	int reserve12;
	int reserve13;
	int reserve14;
	int reserve15;
	int reserve16;
	int reserve17;
	int reserve18;
	int reserve19;
	int reserve20;
	int reserve21;
	int reserve22;
	int reserve23;
	int reserve24;
	int reserve25;
	int reserve26;
	int reserve27;
	int reserve28;
	int reserve29;
	int reserve30;
	int reserve31;
	struct ptrs ptr_array[ARR_SIZE];
};

static struct global_data gd;
static struct debugfs_blob_wrapper blob;
static struct dentry *root;
static struct dentry *dld;

struct debugfs_global {
	int global_8k_has;
	int global_9k_has;
	int global_min;
	int global_count;
	int global_write_tty;
	int global_write_toio;
	int global_bytes_cb_tty;
	int global_to_read;
	int global_push_to_tty;
	int global_tty_send;
	int global_sdio_send;
	int global_sdio_received;
	int global_tty_push;
};

static struct debugfs_global debugfs_glob;

static void update_standard_fields(int index)
{

	gd.ptr_array[index].global_tty_send =
		sdio_dld_info.global_bytes_write_tty;
	gd.ptr_array[index].global_sdio_send =
		sdio_dld_info.global_bytes_write_toio;
	gd.ptr_array[index].global_tty_received =
		sdio_dld_info.global_bytes_push_tty;
	gd.ptr_array[index].global_sdio_received =
		sdio_dld_info.global_bytes_read_fromio;
}

static void update_gd(int code)
{
	struct sdioc_reg_chunk *reg_str =
					&sdio_dld->sdio_dloader_data.sdioc_reg;
	struct sdio_data *outgoing = &sdio_dld->sdio_dloader_data.outgoing_data;
	int index = curr_index%ARR_SIZE;

	gd.curr_i = curr_index;
	gd.duration_ms = 0;
	gd.global_bytes_sent = 0;
	gd.throughput_Mbs = 0;
	gd.host_outgoing_buffer_size_KB = 0;
	gd.client_up_buffer_size_KB = 0;
	gd.client_dl_buffer_size_KB = 0;
	gd.client_dl_buffer_address = 0;
	gd.client_up_buffer_address = 0;
	gd.global_bytes_received = 0;
	gd.global_bytes_pushed = 0;
	gd.reserve11 = 0;
	gd.reserve12 = 0;
	gd.reserve13 = 0;
	gd.reserve14 = 0;
	gd.reserve15 = 0;
	gd.reserve16 = 0;
	gd.reserve17 = 0;
	gd.reserve18 = 0;
	gd.reserve19 = 0;
	gd.reserve20 = 0;
	gd.reserve21 = 0;
	gd.reserve22 = 0;
	gd.reserve23 = 0;
	gd.reserve24 = 0;
	gd.reserve25 = 0;
	gd.reserve26 = 0;
	gd.reserve27 = 0;
	gd.reserve28 = 0;
	gd.reserve29 = 0;
	gd.reserve30 = 0;
	gd.reserve31 = 0;

	gd.ptr_array[index].h_w_ptr = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*0*/
	gd.ptr_array[index].h_r_ptr = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*1*/
	gd.ptr_array[index].c_u_w_ptr =	SDIO_DLD_DEBUGFS_INIT_VALUE;	/*2*/
	gd.ptr_array[index].c_u_r_ptr =	SDIO_DLD_DEBUGFS_INIT_VALUE;	/*3*/
	gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_INIT_VALUE;		/*4*/
	gd.ptr_array[index].h_has_to_send = SDIO_DLD_DEBUGFS_INIT_VALUE;/*5*/
	gd.ptr_array[index].c_has_to_receive =
		SDIO_DLD_DEBUGFS_INIT_VALUE;				/*6*/
	gd.ptr_array[index].min_of = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*7*/
	gd.ptr_array[index].reserve2 = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*8*/
	gd.ptr_array[index].tty_count = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*9*/
	gd.ptr_array[index].write_tty = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*A*/
	gd.ptr_array[index].write_toio = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*B*/
	gd.ptr_array[index].loop_wait_wake =
		SDIO_DLD_DEBUGFS_INIT_VALUE;				/*C*/
	gd.ptr_array[index].cb_wait_wake = SDIO_DLD_DEBUGFS_INIT_VALUE;	/*D*/
	gd.ptr_array[index].c_d_w_ptr =	SDIO_DLD_DEBUGFS_INIT_VALUE;	/*E*/
	gd.ptr_array[index].c_d_r_ptr =	SDIO_DLD_DEBUGFS_INIT_VALUE;	/*F*/
	gd.ptr_array[index].to_read =
		SDIO_DLD_DEBUGFS_INIT_VALUE;			/*0x10*/
	gd.ptr_array[index].push_to_tty =
		SDIO_DLD_DEBUGFS_INIT_VALUE;			/*0x11*/
	gd.ptr_array[index].global_tty_send =
		SDIO_DLD_DEBUGFS_INIT_VALUE;			/*0x12*/
	gd.ptr_array[index].global_sdio_send =
		SDIO_DLD_DEBUGFS_INIT_VALUE;			/*0x13*/
	gd.ptr_array[index].global_tty_received =
		SDIO_DLD_DEBUGFS_INIT_VALUE;			/*0x14*/
	gd.ptr_array[index].global_sdio_received =
		SDIO_DLD_DEBUGFS_INIT_VALUE;			/*0x15*/
	gd.ptr_array[index].reserve22 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve23 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve24 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve25 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve26 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve27 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve28 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve29 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve30 = SDIO_DLD_DEBUGFS_INIT_VALUE;
	gd.ptr_array[index].reserve31 = SDIO_DLD_DEBUGFS_INIT_VALUE;

	switch (code) {
	case SDIO_DLD_DEBUGFS_CASE_1_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_1_CODE;
		gd.ptr_array[index].h_w_ptr = outgoing->offset_write_p;
		gd.ptr_array[index].h_r_ptr = outgoing->offset_read_p;
		gd.ptr_array[index].c_u_w_ptr =	reg_str->up_wr_ptr.reg_val;
		gd.ptr_array[index].c_u_r_ptr =	reg_str->up_rd_ptr.reg_val;
		gd.ptr_array[index].c_d_w_ptr =	reg_str->dl_wr_ptr.reg_val;
		gd.ptr_array[index].c_d_r_ptr =	reg_str->dl_rd_ptr.reg_val;
		break;

	case SDIO_DLD_DEBUGFS_CASE_2_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_2_CODE;
		gd.ptr_array[index].c_u_r_ptr = reg_str->up_rd_ptr.reg_val;
		gd.ptr_array[index].c_u_w_ptr = reg_str->up_wr_ptr.reg_val;
		gd.ptr_array[index].h_has_to_send = debugfs_glob.global_8k_has;
		gd.ptr_array[index].c_has_to_receive =
			debugfs_glob.global_9k_has;
		gd.ptr_array[index].min_of = debugfs_glob.global_min;
		break;

	case SDIO_DLD_DEBUGFS_CASE_3_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_3_CODE;
		gd.ptr_array[index].h_w_ptr = outgoing->offset_write_p;
		gd.ptr_array[index].h_r_ptr = outgoing->offset_read_p;
		gd.ptr_array[index].write_tty = debugfs_glob.global_write_tty;
		break;

	case SDIO_DLD_DEBUGFS_CASE_4_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_4_CODE;
		gd.ptr_array[index].h_w_ptr = outgoing->offset_write_p;
		gd.ptr_array[index].h_r_ptr = outgoing->offset_read_p;
		gd.ptr_array[index].c_u_r_ptr = reg_str->up_rd_ptr.reg_val;
		gd.ptr_array[index].c_u_w_ptr = reg_str->up_wr_ptr.reg_val;
		gd.ptr_array[index].write_toio =
			debugfs_glob.global_write_toio;
		break;

	case SDIO_DLD_DEBUGFS_CASE_5_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_5_CODE;
		gd.ptr_array[index].tty_count = debugfs_glob.global_count;
		break;

	case SDIO_DLD_DEBUGFS_CASE_6_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_6_CODE;
		gd.ptr_array[index].loop_wait_wake = 7;
		break;

	case SDIO_DLD_DEBUGFS_CASE_7_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_7_CODE;
		gd.ptr_array[index].loop_wait_wake = 8;
		break;

	case SDIO_DLD_DEBUGFS_CASE_8_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_8_CODE;
		gd.ptr_array[index].cb_wait_wake = 3;
		break;

	case SDIO_DLD_DEBUGFS_CASE_9_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_9_CODE;
		gd.ptr_array[index].cb_wait_wake = 4;
		break;

	case SDIO_DLD_DEBUGFS_CASE_10_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_10_CODE;
		gd.ptr_array[index].cb_wait_wake =
			debugfs_glob.global_bytes_cb_tty;
		break;

	case SDIO_DLD_DEBUGFS_CASE_11_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_11_CODE;
		gd.ptr_array[index].to_read = debugfs_glob.global_to_read;
		break;

	case SDIO_DLD_DEBUGFS_CASE_12_CODE:
		gd.ptr_array[index].code = SDIO_DLD_DEBUGFS_CASE_12_CODE;
		gd.ptr_array[index].push_to_tty =
			debugfs_glob.global_push_to_tty;
		break;

	default:
		break;
	}
	update_standard_fields(index);
	curr_index++;
}

static int bootloader_debugfs_init(void)
{
	/* /sys/kernel/debug/bootloader there will be dld_arr file */
	root = debugfs_create_dir("bootloader", NULL);
	if (!root) {
		pr_info(MODULE_NAME ": %s - creating root dir "
			"failed\n", __func__);
		return -ENODEV;
	}

	blob.data = &gd;
	blob.size = sizeof(struct global_data);
	dld = debugfs_create_blob("dld_arr", S_IRUGO, root, &blob);
	if (!dld) {
		debugfs_remove_recursive(root);
		pr_err(MODULE_NAME ": %s, failed to create debugfs entry\n",
		       __func__);
		return -ENODEV;
	}

	return 0;
}

/*
* for triggering the sdio_dld info use:
* echo 1 > /sys/kernel/debug/sdio_al_dld/sdio_al_dloader_info
*/
static int sdio_dld_debug_init(void)
{
	sdio_dld_debug.sdio_dld_debug_root =
				debugfs_create_dir("sdio_al_dld", NULL);
	if (!sdio_dld_debug.sdio_dld_debug_root) {
		pr_err(MODULE_NAME ": %s - Failed to create folder. "
		       "sdio_dld_debug_root is NULL",
		       __func__);
		return -ENOENT;
	}

	sdio_dld_debug.sdio_al_dloader = debugfs_create_file(
					"sdio_al_dloader_info",
					S_IRUGO | S_IWUGO,
					sdio_dld_debug.sdio_dld_debug_root,
					NULL,
					&sdio_dld_debug_info_ops);

	if (!sdio_dld_debug.sdio_al_dloader) {
		pr_err(MODULE_NAME ": %s - Failed to create a file. "
		       "sdio_al_dloader is NULL",
		       __func__);
		debugfs_remove(sdio_dld_debug.sdio_dld_debug_root);
		sdio_dld_debug.sdio_dld_debug_root = NULL;
		return -ENOENT;
	}

	return 0;
}

static int sdio_dld_debug_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t sdio_dld_debug_info_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	sdio_dld_print_info();
	return count;
}
#endif /* CONFIG_DEBUG_FS */

static void sdio_dld_print_info(void)
{

	sdio_dld_info.end_time = get_jiffies_64(); /* read the current time */
	sdio_dld_info.delta_jiffies =
		sdio_dld_info.end_time - sdio_dld_info.start_time;
	sdio_dld_info.time_msec = jiffies_to_msecs(sdio_dld_info.delta_jiffies);

	sdio_dld_info.throughput = sdio_dld_info.global_bytes_write_toio *
		BITS_IN_BYTE / sdio_dld_info.time_msec;
	sdio_dld_info.throughput /= MS_IN_SEC;

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - DURATION IN MSEC = %d\n",
		__func__,
		sdio_dld_info.time_msec);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - BYTES WRITTEN ON SDIO BUS "
			    "= %d...BYTES SENT BY TTY = %d",
		__func__,
	       sdio_dld_info.global_bytes_write_toio,
	       sdio_dld_info.global_bytes_write_tty);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - BYTES RECEIVED ON SDIO BUS "
			    "= %d...BYTES SENT TO TTY = %d",
		__func__,
		sdio_dld_info.global_bytes_read_fromio,
		sdio_dld_info.global_bytes_push_tty);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - THROUGHPUT=%d Mbit/Sec",
		__func__, sdio_dld_info.throughput);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - CLIENT DL_BUFFER_SIZE=%d"
		" KB..CLIENT UL_BUFFER=%d KB\n",
		__func__,
		sdio_dld_info.cl_dl_buffer_size/BYTES_IN_KB,
		sdio_dld_info.cl_up_buffer_size/BYTES_IN_KB);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - HOST OUTGOING BUFFER_SIZE"
			    "=%d KB",
		__func__,
		sdio_dld_info.host_outgoing_buffer_size/BYTES_IN_KB);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - CLIENT DL BUFFER "
		 "ADDRESS = 0x%x", __func__,
		sdio_dld_info.cl_dl_buffer_address);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - CLIENT UP BUFFER "
		"ADDRESS = 0x%x",
		__func__,
		sdio_dld_info.cl_up_buffer_address);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - CLIENT - UPLINK BUFFER - "
		"READ POINTER = %d", __func__,
		sdio_dld_info.cl_up_rd_ptr);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - CLIENT - UPLINK BUFFER - "
		"WRITE POINTER = %d", __func__,
		sdio_dld_info.cl_up_wr_ptr);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - CLIENT - DOWNLINK BUFFER - "
		"READ POINTER = %d", __func__,
		sdio_dld_info.cl_dl_rd_ptr);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - CLIENT - DOWNLINK BUFFER - "
		"WRITE POINTER = %d", __func__,
		sdio_dld_info.cl_dl_wr_ptr);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - HOST - OUTGOING BUFFER - "
		"READ POINTER = %d", __func__,
		sdio_dld_info.host_read_ptr);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - HOST - OUTGOING BUFFER - "
		"WRITE POINTER = %d", __func__,
		sdio_dld_info.host_write_ptr);

	pr_info(MODULE_NAME ": %s, FLASHLESS BOOT - END DEBUG INFO", __func__);
}

/**
  * sdio_dld_set_op_mode
  * sets the op_mode and the name of the op_mode. Also, in case
  * it's invalid mode sets op_mode to SDIO_DLD_NORMAL_MODE
  *
  * @op_mode: the operation mode to be set
  * @return NONE
  */
static void sdio_dld_set_op_mode(enum sdio_dld_op_mode op_mode)
{
	sdio_dld->op_mode = op_mode;

	switch (op_mode) {
	case SDIO_DLD_NORMAL_MODE:
		memcpy(sdio_dld->op_mode_name,
		       SDIO_DLD_NORMAL_MODE_NAME, TEST_NAME_MAX_SIZE);
		break;
	case SDIO_DLD_BOOT_TEST_MODE:
		memcpy(sdio_dld->op_mode_name,
		       SDIO_DLD_BOOT_TEST_MODE_NAME, TEST_NAME_MAX_SIZE);
		break;
	case SDIO_DLD_AMSS_TEST_MODE:
		memcpy(sdio_dld->op_mode_name,
		       SDIO_DLD_AMSS_TEST_MODE_NAME, TEST_NAME_MAX_SIZE);
		break;
	default:
		sdio_dld->op_mode = SDIO_DLD_NORMAL_MODE;
		pr_err(MODULE_NAME ": %s - Invalid Op_Mode = %d. Settings "
		       "Op_Mode to default - NORMAL_MODE\n",
		       __func__, op_mode);
		memcpy(sdio_dld->op_mode_name,
		       SDIO_DLD_NORMAL_MODE_NAME, TEST_NAME_MAX_SIZE);
		break;
	}

	if (sdio_dld->op_mode_name != NULL) {
		pr_info(MODULE_NAME ": %s - FLASHLESS BOOT - Op_Mode is set to "
			"%s\n", __func__, sdio_dld->op_mode_name);
	} else {
		pr_info(MODULE_NAME ": %s - FLASHLESS BOOT - op_mode_name is "
			"NULL\n", __func__);
	}
}

/**
  * sdio_dld_allocate_local_buffers
  * allocates local outgoing and incoming buffers and also sets
  * threshold for outgoing data.
  *
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_allocate_local_buffers(void)
{
	struct sdioc_reg_chunk *reg_str = &sdio_dld->sdio_dloader_data.
		sdioc_reg;
	struct sdio_data *outgoing = &sdio_dld->sdio_dloader_data.outgoing_data;
	struct sdio_data *incoming = &sdio_dld->sdio_dloader_data.incoming_data;

	incoming->data =
		kzalloc(reg_str->dl_buff_size.reg_val, GFP_KERNEL);

	if (!incoming->data) {
		pr_err(MODULE_NAME ": %s - param ""incoming->data"" is NULL. "
		       "Couldn't allocate incoming_data local buffer\n",
		       __func__);
		return -ENOMEM;
	}

	incoming->buffer_size = reg_str->dl_buff_size.reg_val;

	outgoing->data = outgoing_data_buffer;

	outgoing->buffer_size = SDIO_DLD_OUTGOING_BUFFER_SIZE;

	if (outgoing->buffer_size !=
	    reg_str->ul_buff_size.reg_val*MULTIPLE_RATIO) {
		pr_err(MODULE_NAME ": %s - HOST outgoing buffer size (%d bytes)"
		       "must be a multiple of ClIENT uplink buffer size (%d "
		       "bytes). HOST_SIZE == n*CLIENT_SIZE.(n=1,2,3...)\n",
		       __func__,
		       SDIO_DLD_OUTGOING_BUFFER_SIZE,
		       reg_str->ul_buff_size.reg_val);
		kfree(incoming->data);
		return -EINVAL;
	}

	/* keep sdio_dld_info up to date */
	sdio_dld_info.host_outgoing_buffer_size = outgoing->buffer_size;

	return 0;
}

/**
  * sdio_dld_dealloc_local_buffers frees incoming and outgoing
  * buffers.
  *
  * @return None.
  */
static void sdio_dld_dealloc_local_buffers(void)
{
	kfree((void *)sdio_dld->sdio_dloader_data.incoming_data.data);
}

/**
  * mailbox_to_seq_chunk_read_cfg
  * reads 4 configuration registers of mailbox from str_func, as
  * a sequentail chunk in memory, and updates global struct
  * accordingly.
  *
  * @str_func: a pointer to func struct.
  * @return 0 on success or negative value on error.
  */
static int mailbox_to_seq_chunk_read_cfg(struct sdio_func *str_func)
{
	struct sdioc_reg_sequential_chunk_cfg seq_chunk;
	struct sdioc_reg_chunk *reg = &sdio_dld->sdio_dloader_data.sdioc_reg;
	int status = 0;

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	sdio_claim_host(str_func);

	/* reading SDIOC_MAILBOX_SIZE bytes from SDIOC_MAILBOX_ADDRESS */
	status = sdio_memcpy_fromio(str_func,
				    (void *)&seq_chunk,
				    SDIOC_MAILBOX_ADDRESS,
				    SDIOC_CFG_REGS_SIZE);
	if (status) {
		pr_err(MODULE_NAME ": %s - sdio_memcpy_fromio()"
		       " READING CFG MAILBOX failed. status=%d.\n",
		       __func__, status);
	}

	sdio_release_host(str_func);

	reg->dl_buff_address.reg_val = seq_chunk.dl_buff_address;
	reg->up_buff_address.reg_val = seq_chunk.up_buff_address;
	reg->dl_buff_size.reg_val = seq_chunk.dl_buff_size;
	reg->ul_buff_size.reg_val = seq_chunk.ul_buff_size;

	/* keep sdio_dld_info up to date */
	sdio_dld_info.cl_dl_buffer_size = seq_chunk.dl_buff_size;
	sdio_dld_info.cl_up_buffer_size = seq_chunk.ul_buff_size;
	sdio_dld_info.cl_dl_buffer_address = seq_chunk.dl_buff_address;
	sdio_dld_info.cl_up_buffer_address = seq_chunk.up_buff_address;

	return status;
}

/**
  * mailbox_to_seq_chunk_read_ptrs
  * reads 4 pointers registers of mailbox from str_func, as a
  * sequentail chunk in memory, and updates global struct
  * accordingly.
  *
  * @str_func: a pointer to func struct.
  * @return 0 on success or negative value on error.
  */
static int mailbox_to_seq_chunk_read_ptrs(struct sdio_func *str_func)
{
	struct sdioc_reg_sequential_chunk_ptrs seq_chunk;
	struct sdioc_reg_chunk *reg = &sdio_dld->sdio_dloader_data.sdioc_reg;
	int status = 0;

	struct sdio_data *outgoing = &sdio_dld->sdio_dloader_data.outgoing_data;
	static int counter = 1;
	static int offset_write_p;
	static int offset_read_p;
	static int up_wr_ptr;
	static int up_rd_ptr;
	static int dl_wr_ptr;
	static int dl_rd_ptr;

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	sdio_claim_host(str_func);

	/* reading SDIOC_MAILBOX_SIZE bytes from SDIOC_MAILBOX_ADDRESS */
	status = sdio_memcpy_fromio(str_func,
				    (void *)&seq_chunk,
				    SDIOC_PTRS_OFFSET,
				    SDIOC_PTR_REGS_SIZE);
	if (status) {
		pr_err(MODULE_NAME ": %s - sdio_memcpy_fromio()"
		       " READING PTRS MAILBOX failed. status=%d.\n",
		       __func__, status);
	}

	sdio_release_host(str_func);

	reg->dl_rd_ptr.reg_val = seq_chunk.dl_rd_ptr;
	reg->dl_wr_ptr.reg_val = seq_chunk.dl_wr_ptr;
	reg->up_rd_ptr.reg_val = seq_chunk.up_rd_ptr;
	reg->up_wr_ptr.reg_val = seq_chunk.up_wr_ptr;

	/* keeping sdio_dld_info up to date */
	sdio_dld_info.cl_dl_rd_ptr = seq_chunk.dl_rd_ptr;
	sdio_dld_info.cl_dl_wr_ptr = seq_chunk.dl_wr_ptr;
	sdio_dld_info.cl_up_rd_ptr = seq_chunk.up_rd_ptr;
	sdio_dld_info.cl_up_wr_ptr = seq_chunk.up_wr_ptr;


	/* DEBUG - if there was a change in value */
	if ((offset_write_p != outgoing->offset_write_p) ||
	    (offset_read_p != outgoing->offset_read_p) ||
	    (up_wr_ptr != reg->up_wr_ptr.reg_val) ||
	    (up_rd_ptr != reg->up_rd_ptr.reg_val) ||
	    (dl_wr_ptr != reg->dl_wr_ptr.reg_val) ||
	    (dl_rd_ptr != reg->dl_rd_ptr.reg_val) ||
	    (counter % PRINTING_GAP == 0)) {
		counter = 1;
		pr_debug(MODULE_NAME ": %s MailBox pointers: BLOCK_SIZE=%d, "
			 "hw=%d, hr=%d, cuw=%d, cur=%d, cdw=%d, cdr=%d\n",
			 __func__,
			 SDIO_DL_BLOCK_SIZE,
			 outgoing->offset_write_p,
			 outgoing->offset_read_p,
			 reg->up_wr_ptr.reg_val,
			 reg->up_rd_ptr.reg_val,
			 reg->dl_wr_ptr.reg_val,
			 reg->dl_rd_ptr.reg_val);

#ifdef CONFIG_DEBUG_FS
		update_gd(SDIO_DLD_DEBUGFS_CASE_1_CODE);
#endif
		/* update static variables */
		offset_write_p = outgoing->offset_write_p;
		offset_read_p =	outgoing->offset_read_p;
		up_wr_ptr = reg->up_wr_ptr.reg_val;
		up_rd_ptr = reg->up_rd_ptr.reg_val;
		dl_wr_ptr = reg->dl_wr_ptr.reg_val;
		dl_rd_ptr = reg->dl_rd_ptr.reg_val;
	} else {
		counter++;
	}
	return status;
}

/**
  * sdio_dld_init_func
  * enables the sdio func, and sets the func block size.
  *
  * @str_func: a pointer to func struct.
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_init_func(struct sdio_func *str_func)
{
	int status1 = 0;
	int status2 = 0;

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	sdio_claim_host(str_func);

	status1 = sdio_enable_func(str_func);
	if (status1) {
		sdio_release_host(str_func);
		pr_err(MODULE_NAME ": %s - sdio_enable_func() failed. "
		       "status=%d\n", __func__, status1);
		return status1;
	}

	status2 = sdio_set_block_size(str_func, SDIO_DL_BLOCK_SIZE);
	if (status2) {
		pr_err(MODULE_NAME ": %s - sdio_set_block_size() failed. "
		       "status=%d\n", __func__, status2);
		status1 = sdio_disable_func(str_func);
		if (status1) {
			pr_err(MODULE_NAME ": %s - sdio_disable_func() "
		       "failed. status=%d\n", __func__, status1);
		}
		sdio_release_host(str_func);
		return status2;
	}

	sdio_release_host(str_func);
	str_func->max_blksize = SDIO_DL_BLOCK_SIZE;
	return 0;
}

/**
  * sdio_dld_allocate_buffers
  * initializes the sdio func, and then reads the mailbox, in
  * order to allocate incoming and outgoing buffers according to
  * the size that was read from the mailbox.
  *
  * @str_func: a pointer to func struct.
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_allocate_buffers(struct sdio_func *str_func)
{
	int status = 0;

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	status = mailbox_to_seq_chunk_read_cfg(str_func);
	if (status) {
		pr_err(MODULE_NAME ": %s - Failure in Function "
		       "mailbox_to_seq_chunk_read_cfg(). status=%d\n",
		       __func__, status);
		return status;
	}

	status = sdio_dld_allocate_local_buffers();
	if (status) {
		pr_err(MODULE_NAME ": %s - Failure in Function "
		       "sdio_dld_allocate_local_buffers(). status=%d\n",
		       __func__, status);
		return status;
	}
	return 0;
}

/**
  * sdio_dld_create_thread
  * creates thread and wakes it up.
  *
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_create_thread(void)
{
	sdio_dld->dld_main_thread.task_name = SDIO_DL_MAIN_THREAD_NAME;

	sdio_dld->dld_main_thread.dld_task =
		kthread_create(sdio_dld_main_task,
			       (void *)(sdio_dld->card),
			       sdio_dld->dld_main_thread.task_name);

	if (IS_ERR(sdio_dld->dld_main_thread.dld_task)) {
		pr_err(MODULE_NAME ": %s - kthread_create() failed\n",
			__func__);
		return -ENOMEM;
	}
	wake_up_process(sdio_dld->dld_main_thread.dld_task);
	return 0;
}

/**
  * start_timer
  * sets the timer and starts.
  *
  * @timer: the timer to configure and add
  * @ms: the ms until it expires
  * @return None.
  */
static void start_timer(struct timer_list *timer, unsigned int ms)
{
	if ((ms == 0) || (timer == NULL)) {
		pr_err(MODULE_NAME ": %s - invalid parameter", __func__);
	} else {
		timer->expires = jiffies +
			msecs_to_jiffies(ms);
		add_timer(timer);
	}
}

/**
  * sdio_dld_timer_handler
  * this is the timer handler. whenever it is invoked, it wakes
  * up the main loop task, and the write callback, and starts
  * the timer again.
  *
  * @data: a pointer to the tty device driver structure.
  * @return None.
  */

static void sdio_dld_timer_handler(unsigned long data)
{
	pr_debug(MODULE_NAME " Timer Expired\n");
	spin_lock_irqsave(&lock2, lock_flags2);
	if (sdio_dld->main_loop_event.wake_up_signal == 0) {
		sdio_dld->main_loop_event.wake_up_signal = 1;
		wake_up(&sdio_dld->main_loop_event.wait_event);
	}
	spin_unlock_irqrestore(&lock2, lock_flags2);

	sdio_dld->write_callback_event.wake_up_signal = 1;
	wake_up(&sdio_dld->write_callback_event.wait_event);

	start_timer(&sdio_dld->timer, sdio_dld->poll_ms);
}

/**
  * sdio_dld_push_timer_handler
  * this is a timer handler of the push_timer.
  *
  * @data: a pointer to the tty device driver structure.
  * @return None.
  */
static void sdio_dld_push_timer_handler(unsigned long data)
{
	pr_err(MODULE_NAME " %s - Push Timer Expired... Trying to "
		"push data to TTY Core for over then %d ms.\n",
		__func__, sdio_dld->push_timer_ms);
}

/**
  * sdio_dld_open
  * this is the open callback of the tty driver.
  * it initializes the sdio func, allocates the buffers, and
  * creates the main thread.
  *
  * @tty: a pointer to the tty struct.
  * @file: file descriptor.
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_open(struct tty_struct *tty, struct file *file)
{
	int status = 0;
	int func_in_array =
		REAL_FUNC_TO_FUNC_IN_ARRAY(sdio_dld->sdioc_boot_func);
	struct sdio_func *str_func = sdio_dld->card->sdio_func[func_in_array];

	sdio_dld->tty_str = tty;
	sdio_dld->tty_str->low_latency = 1;
	sdio_dld->tty_str->icanon = 0;
	set_bit(TTY_NO_WRITE_SPLIT, &sdio_dld->tty_str->flags);

	pr_info(MODULE_NAME ": %s, TTY DEVICE FOR FLASHLESS BOOT OPENED\n",
	       __func__);
	sdio_dld_info.start_time = get_jiffies_64(); /* read the current time */

	if (!tty) {
		pr_err(MODULE_NAME ": %s - param ""tty"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	atomic_set(&sdio_dld->dld_main_thread.please_close, 0);
	sdio_dld->dld_main_thread.exit_wait.wake_up_signal = 0;

	status = sdio_dld_allocate_buffers(str_func);
	if (status) {
		pr_err(MODULE_NAME ": %s, failed in "
		       "sdio_dld_allocate_buffers(). status=%d\n",
		       __func__, status);
		return status;
	}

	/* init waiting event of the write callback */
	init_waitqueue_head(&sdio_dld->write_callback_event.wait_event);

	/* init waiting event of the main loop */
	init_waitqueue_head(&sdio_dld->main_loop_event.wait_event);

	/* configure and init the timer */
	sdio_dld->poll_ms = TIMER_DURATION;
	init_timer(&sdio_dld->timer);
	sdio_dld->timer.data = (unsigned long) sdio_dld;
	sdio_dld->timer.function = sdio_dld_timer_handler;
	sdio_dld->timer.expires = jiffies +
		msecs_to_jiffies(sdio_dld->poll_ms);
	add_timer(&sdio_dld->timer);

	sdio_dld->push_timer_ms = PUSH_TIMER_DURATION;
	init_timer(&sdio_dld->push_timer);
	sdio_dld->push_timer.data = (unsigned long) sdio_dld;
	sdio_dld->push_timer.function = sdio_dld_push_timer_handler;

	status = sdio_dld_create_thread();
	if (status) {
		del_timer_sync(&sdio_dld->timer);
		del_timer_sync(&sdio_dld->push_timer);
		sdio_dld_dealloc_local_buffers();
		pr_err(MODULE_NAME ": %s, failed in sdio_dld_create_thread()."
				   "status=%d\n", __func__, status);
		return status;
	}
	return 0;
}

/**
  * sdio_dld_close
  * this is the close callback of the tty driver. it requests
  * the main thread to exit, and waits for notification of it.
  * it also de-allocates the buffers, and unregisters the tty
  * driver and device.
  *
  * @tty: a pointer to the tty struct.
  * @file: file descriptor.
  * @return None.
  */
static void sdio_dld_close(struct tty_struct *tty, struct file *file)
{
	int status = 0;
	struct sdioc_reg_chunk *reg = &sdio_dld->sdio_dloader_data.sdioc_reg;

	/* informing the SDIOC that it can exit boot phase */
	sdio_dld->sdio_dloader_data.sdioc_reg.good_to_exit_ptr.reg_val =
		SDIOC_EXIT_CODE;

	atomic_set(&sdio_dld->dld_main_thread.please_close, 1);

	pr_debug(MODULE_NAME ": %s - CLOSING - WAITING...", __func__);

	wait_event(sdio_dld->dld_main_thread.exit_wait.wait_event,
		   sdio_dld->dld_main_thread.exit_wait.wake_up_signal);
	pr_debug(MODULE_NAME ": %s - CLOSING - WOKE UP...", __func__);

	del_timer_sync(&sdio_dld->timer);
	del_timer_sync(&sdio_dld->push_timer);

	sdio_dld_dealloc_local_buffers();

	tty_unregister_device(sdio_dld->tty_drv, 0);

	status = tty_unregister_driver(sdio_dld->tty_drv);

	if (status) {
		pr_err(MODULE_NAME ": %s - tty_unregister_driver() failed\n",
		       __func__);
	}

#ifdef CONFIG_DEBUG_FS
	gd.curr_i = curr_index;
	gd.duration_ms = sdio_dld_info.time_msec;
	gd.global_bytes_sent = sdio_dld_info.global_bytes_write_toio;
	gd.global_bytes_received = 0;
	gd.throughput_Mbs = sdio_dld_info.throughput;
	gd.host_outgoing_buffer_size_KB = sdio_dld->sdio_dloader_data.
		outgoing_data.buffer_size/BYTES_IN_KB;
	gd.client_up_buffer_size_KB = reg->ul_buff_size.reg_val/BYTES_IN_KB;
	gd.client_dl_buffer_size_KB = reg->dl_buff_size.reg_val/BYTES_IN_KB;
	gd.client_dl_buffer_address = reg->dl_buff_address.reg_val;
	gd.client_up_buffer_address = reg->up_buff_address.reg_val;
	gd.global_bytes_received = sdio_dld_info.global_bytes_read_fromio;
	gd.global_bytes_pushed = sdio_dld_info.global_bytes_push_tty;
#endif

	/* saving register values before deallocating sdio_dld
	   in order to use it in sdio_dld_print_info() through shell command */
	sdio_dld_info.cl_dl_rd_ptr = reg->dl_rd_ptr.reg_val;
	sdio_dld_info.cl_dl_wr_ptr = reg->dl_wr_ptr.reg_val;
	sdio_dld_info.cl_up_rd_ptr = reg->up_rd_ptr.reg_val;
	sdio_dld_info.cl_up_wr_ptr = reg->up_wr_ptr.reg_val;

	sdio_dld_info.host_read_ptr =
		sdio_dld->sdio_dloader_data.outgoing_data.offset_read_p;

	sdio_dld_info.host_write_ptr =
		sdio_dld->sdio_dloader_data.outgoing_data.offset_write_p;

	sdio_dld_info.cl_dl_buffer_size =
		sdio_dld->sdio_dloader_data.sdioc_reg.dl_buff_size.reg_val;

	sdio_dld_info.cl_up_buffer_size =
		sdio_dld->sdio_dloader_data.sdioc_reg.ul_buff_size.reg_val;

	sdio_dld_info.host_outgoing_buffer_size =
		sdio_dld->sdio_dloader_data.outgoing_data.buffer_size;

	sdio_dld_info.cl_dl_buffer_address =
		sdio_dld->sdio_dloader_data.sdioc_reg.dl_buff_address.reg_val;

	sdio_dld_info.cl_up_buffer_address =
		sdio_dld->sdio_dloader_data.sdioc_reg.up_buff_address.reg_val;

	sdio_dld_print_info();

	if (sdio_dld->done_callback)
		sdio_dld->done_callback();

	pr_info(MODULE_NAME ": %s - Freeing sdio_dld data structure, and "
		" returning...", __func__);
	kfree(sdio_dld);
}

/**
  * writing_size_to_buf
  * writes from src buffer into dest buffer. if dest buffer
  * reaches its end, rollover happens.
  *
  * @dest: destination buffer.
  * @src: source buffer.
  * @dest_wr_ptr: writing pointer in destination buffer.
  * @dest_size: destination buffer size.
  * @dest_rd_ptr: reading pointer in destination buffer.
  * @size_to_write: size of bytes to write.
  * @return -how many bytes actually written to destination
  * buffer.
  *
  * ONLY destination buffer is treated as cyclic buffer.
  */
static int writing_size_to_buf(char *dest,
			       const unsigned char *src,
			       int *dest_wr_ptr,
			       int dest_size,
			       int dest_rd_ptr,
			       int size_to_write)
{
	int actually_written = 0;
	int size_to_add = *dest_wr_ptr;

	if (!dest) {
		pr_err(MODULE_NAME ": %s - param ""dest"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!src) {
		pr_err(MODULE_NAME ": %s - param ""src"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!dest_wr_ptr) {
		pr_err(MODULE_NAME ": %s - param ""dest_wr_ptr"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	for (actually_written = 0 ;
	      actually_written < size_to_write ; ++actually_written) {
		/* checking if buffer is full */
		if (((size_to_add + 1) % dest_size) == dest_rd_ptr) {
			*dest_wr_ptr = size_to_add;
			return actually_written;
		}

		dest[size_to_add] = src[actually_written];
		size_to_add = (size_to_add+1)%dest_size;
	}

	*dest_wr_ptr = size_to_add;

	return actually_written;
}

/**
  * sdioc_bytes_till_end_of_buffer - this routine calculates how many bytes are
  * empty/in use. if calculation requires rap around - it will ignore the rap
  * around and will do the calculation untill the end of the buffer
  *
  * @write_ptr: writing pointer.
  * @read_ptr: reading pointer.
  * @total_size: buffer size.
  * @free_bytes: return value-how many free bytes.
  * @bytes_in_use: return value-how many bytes in use.
  * @return 0 on success or negative value on error.
  *
  * buffer is treated as a cyclic buffer.
  */
static int sdioc_bytes_till_end_of_buffer(int write_ptr,
					  int read_ptr,
					  int total_size,
					  int *free_bytes,
					  int *bytes_in_use)
{
	if (!free_bytes) {
		pr_err(MODULE_NAME ": %s - param ""free_bytes"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!bytes_in_use) {
		pr_err(MODULE_NAME ": %s - param ""bytes_in_use"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (write_ptr >= read_ptr) {
		if (read_ptr == 0)
			*free_bytes = total_size - write_ptr - 1;
		else
			*free_bytes = total_size - write_ptr;
		*bytes_in_use = write_ptr - read_ptr;
	} else {
		*bytes_in_use = total_size - read_ptr;
		*free_bytes = read_ptr - write_ptr - 1;
	}

	return  0;
}

/**
  * sdioc_bytes_free_in_buffer
  * this routine calculates how many bytes are free in a buffer
  * and how many are in use, according to its reading and
  * writing pointer offsets.
  *
  * @write_ptr: writing pointer.
  * @read_ptr: reading pointer.
  * @total_size: buffer size.
  * @free_bytes: return value-how many free bytes in buffer.
  * @bytes_in_use: return value-how many bytes in use in buffer.
  * @return 0 on success or negative value on error.
  *
  * buffer is treated as a cyclic buffer.
  */
static int sdioc_bytes_free_in_buffer(int write_ptr,
				      int read_ptr,
				      int total_size,
				      int *free_bytes,
				      int *bytes_in_use)
{
	if (!free_bytes) {
		pr_err(MODULE_NAME ": %s - param ""free_bytes"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!bytes_in_use) {
		pr_err(MODULE_NAME ": %s - param ""bytes_in_use"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	/* if pointers equel - buffers are empty. nothing to read/write */

	if (write_ptr >= read_ptr)
		*bytes_in_use = write_ptr - read_ptr;
	else
		*bytes_in_use = total_size - (read_ptr - write_ptr);

	*free_bytes = total_size - *bytes_in_use - 1;

	return 0;
}

/*
* sdio_dld_write_room
*
* This is the write_room function of the tty driver.
*
* @tty: pointer to tty struct.
* @return free bytes for write.
*
*/
static int sdio_dld_write_room(struct tty_struct *tty)
{
	return sdio_dld->sdio_dloader_data.outgoing_data.buffer_size;
}

/**
  * sdio_dld_write_callback
  * this is the write callback of the tty driver.
  *
  * @tty: pointer to tty struct.
  * @buf: buffer to write from.
  * @count: number of bytes to write.
  * @return bytes written or negative value on error.
  *
  * if destination buffer has not enough room for the incoming
  * data, returns an error.
  */
static int sdio_dld_write_callback(struct tty_struct *tty,
				   const unsigned char *buf, int count)
{
	struct sdio_data *outgoing = &sdio_dld->sdio_dloader_data.outgoing_data;
	int dst_free_bytes = 0;
	int dummy = 0;
	int status = 0;
	int bytes_written = 0;
	int total_written = 0;
	static int write_retry;
	int pending_to_write = count;

#ifdef CONFIG_DEBUG_FS
	debugfs_glob.global_count = count;
	update_gd(SDIO_DLD_DEBUGFS_CASE_5_CODE);
#endif

	pr_debug(MODULE_NAME ": %s - WRITING CALLBACK CALLED WITH %d bytes\n",
		 __func__, count);

	if (!outgoing->data) {
		pr_err(MODULE_NAME ": %s - param ""outgoing->data"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	pr_debug(MODULE_NAME ": %s - WRITE CALLBACK size to write to outgoing"
		 " buffer %d\n", __func__, count);

	/* as long as there is something to write to outgoing buffer */
	do {
		int bytes_to_write = 0;
		status = sdioc_bytes_free_in_buffer(
			outgoing->offset_write_p,
			outgoing->offset_read_p,
			outgoing->buffer_size,
			&dst_free_bytes,
			&dummy);

		if (status) {
			pr_err(MODULE_NAME ": %s - Failure in Function "
			       "sdioc_bytes_free_in_buffer(). status=%d\n",
			       __func__, status);
			return status;
		}

		/*
		 * if there is free room in outgoing buffer
		 * lock mutex and request trigger notification from the main
		 * task. unlock mutex, and wait for sinal
		 */
		if (dst_free_bytes > 0) {
			write_retry = 0;
			/*
			 * if there is more data to write to outgoing buffer
			 * than it can receive, wait for signal from main task
			 */
			if (pending_to_write > dst_free_bytes) {

				/* sampling updated dst_free_bytes */
				status = sdioc_bytes_free_in_buffer(
				outgoing->offset_write_p,
				outgoing->offset_read_p,
				outgoing->buffer_size,
				&dst_free_bytes,
				&dummy);

				if (status) {
					pr_err(MODULE_NAME ": %s - Failure in "
							   "Function "
					       "sdioc_bytes_free_in_buffer(). "
					       "status=%d\n", __func__, status);
					return status;
				}
			}

			bytes_to_write = min(pending_to_write, dst_free_bytes);
			bytes_written =
				writing_size_to_buf(outgoing->data,
						    buf+total_written,
						    &outgoing->offset_write_p,
						    outgoing->buffer_size,
						    outgoing->offset_read_p,
						    bytes_to_write);

			/* keeping sdio_dld_info up to date */
			sdio_dld_info.host_write_ptr =
				sdio_dld->sdio_dloader_data.
					    outgoing_data.offset_write_p;

#ifdef CONFIG_DEBUG_FS
			debugfs_glob.global_write_tty = bytes_written;
			update_gd(SDIO_DLD_DEBUGFS_CASE_3_CODE);
#endif
			sdio_dld_info.global_bytes_write_tty += bytes_written;

			spin_lock_irqsave(&lock2, lock_flags2);
			if (sdio_dld->main_loop_event.wake_up_signal == 0) {
				sdio_dld->main_loop_event.wake_up_signal = 1;
				wake_up(&sdio_dld->main_loop_event.wait_event);
			}
			spin_unlock_irqrestore(&lock2, lock_flags2);

			/*
			 * although outgoing buffer has enough room, writing
			 * failed
			 */
			if (bytes_written != bytes_to_write) {
				pr_err(MODULE_NAME ": %s - couldn't write "
				       "%d bytes to " "outgoing buffer."
				       "bytes_written=%d\n",
				       __func__, bytes_to_write,
				       bytes_written);
			       return -EIO;
			}

			total_written += bytes_written;
			pending_to_write -= bytes_written;
			outgoing->num_of_bytes_in_use += bytes_written;

			pr_debug(MODULE_NAME ": %s - WRITE CHUNK to outgoing "
					   "buffer. pending_to_write=%d, "
					   "outgoing_free_bytes=%d, "
					   "bytes_written=%d\n",
				 __func__,
				 pending_to_write,
				 dst_free_bytes,
				 bytes_written);

		} else {
			write_retry++;

			pr_debug(MODULE_NAME ": %s - WRITE CALLBACK - NO ROOM."
			       " pending_to_write=%d, write_retry=%d\n",
				 __func__,
				 pending_to_write,
				 write_retry);

			spin_lock_irqsave(&lock1, lock_flags1);
			sdio_dld->write_callback_event.wake_up_signal = 0;
			spin_unlock_irqrestore(&lock1, lock_flags1);

			pr_debug(MODULE_NAME ": %s - WRITE CALLBACK - "
					     "WAITING...", __func__);
#ifdef CONFIG_DEBUG_FS
			update_gd(SDIO_DLD_DEBUGFS_CASE_8_CODE);
#endif
			wait_event(sdio_dld->write_callback_event.wait_event,
				   sdio_dld->write_callback_event.
				   wake_up_signal);
#ifdef CONFIG_DEBUG_FS
			update_gd(SDIO_DLD_DEBUGFS_CASE_9_CODE);
#endif
			pr_debug(MODULE_NAME ": %s - WRITE CALLBACK - "
					     "WOKE UP...", __func__);
		}
	} while (pending_to_write > 0 && write_retry < WRITE_RETRIES);

	if (pending_to_write > 0) {

		pr_err(MODULE_NAME ": %s - WRITE CALLBACK - pending data is "
				   "%d out of %d > 0. total written in this "
				   "callback = %d\n",
		       __func__, pending_to_write, count, total_written);
	}

	if (write_retry == WRITE_RETRIES) {
		pr_err(MODULE_NAME ": %s, write_retry=%d= max\n",
		       __func__, write_retry);
	}

#ifdef CONFIG_DEBUG_FS
	debugfs_glob.global_bytes_cb_tty = total_written;
	update_gd(SDIO_DLD_DEBUGFS_CASE_10_CODE);
#endif

	return total_written;
}

/**
  * sdio_memcpy_fromio_wrapper -
  * reads from sdioc, and updats the sdioc registers according
  * to how many bytes were actually read.
  *
  * @str_func: a pointer to func struct.
  * @client_rd_ptr: sdioc value of downlink read ptr.
  * @client_wr_ptr: sdioc value of downlink write ptr.
  * @buffer_to_store: buffer to store incoming data.
  * @address_to_read: address to start reading from in sdioc.
  * @size_to_read: size of bytes to read.
  * @client_buffer_size: sdioc downlink buffer size.
  * @return 0 on success or negative value on error.
  */
static int sdio_memcpy_fromio_wrapper(struct sdio_func *str_func,
				      unsigned int client_rd_ptr,
				      unsigned int client_wr_ptr,
				      void *buffer_to_store,
				      unsigned int address_to_read_from,
				      int size_to_read,
				      int client_buffer_size)
{
	int status = 0;
	struct sdioc_reg_chunk *reg_str =
		&sdio_dld->sdio_dloader_data.sdioc_reg;

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!buffer_to_store) {
		pr_err(MODULE_NAME ": %s - param ""buffer_to_store"" is "
				   "NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (size_to_read < 0) {
		pr_err(MODULE_NAME ": %s - invalid size to read=%d\n",
			__func__, size_to_read);
		return -EINVAL;
	}

	sdio_claim_host(str_func);

	pr_debug(MODULE_NAME ": %s, READING DATA - from add %d, "
			   "size_to_read=%d\n",
	       __func__, address_to_read_from, size_to_read);

	status = sdio_memcpy_fromio(str_func,
				    (void *)buffer_to_store,
				    address_to_read_from,
				    size_to_read);
	if (status) {
		pr_err(MODULE_NAME ": %s - sdio_memcpy_fromio()"
		       " DATA failed. status=%d.\n",
		       __func__, status);
		sdio_release_host(str_func);
		return status;
	}

	/* updating an offset according to cyclic buffer size */
	reg_str->dl_rd_ptr.reg_val =
		(reg_str->dl_rd_ptr.reg_val + size_to_read) %
		client_buffer_size;
	/* keeping sdio_dld_info up to date */
	sdio_dld_info.cl_dl_rd_ptr = reg_str->dl_rd_ptr.reg_val;

	status = sdio_memcpy_toio(str_func,
				  reg_str->dl_rd_ptr.reg_offset,
				  (void *)&reg_str->dl_rd_ptr.reg_val,
				  sizeof(reg_str->dl_rd_ptr.reg_val));

	if (status) {
		pr_err(MODULE_NAME ": %s - sdio_memcpy_toio() "
		       "UPDATE PTR failed. status=%d.\n",
		       __func__, status);
	}

	sdio_release_host(str_func);
	return status;
}

/**
  * sdio_memcpy_toio_wrapper
  * writes to sdioc, and updats the sdioc registers according
  * to how many bytes were actually read.
  *
  * @str_func: a pointer to func struct.
  * @client_wr_ptr: sdioc downlink write ptr.
  * @h_read_ptr: host incoming read ptrs
  * @buf_write_from: buffer to write from.
  * @bytes_to_write: number of bytes to write.
  * @return 0 on success or negative value on error.
  */
static int sdio_memcpy_toio_wrapper(struct sdio_func *str_func,
				    unsigned int client_wr_ptr,
				    unsigned int h_read_ptr,
				    void *buf_write_from,
				    int bytes_to_write)
{
	int status = 0;
	struct sdioc_reg_chunk *reg_str =
		&sdio_dld->sdio_dloader_data.sdioc_reg;
	struct sdio_data *outgoing = &sdio_dld->sdio_dloader_data.outgoing_data;

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!buf_write_from) {
		pr_err(MODULE_NAME ": %s - param ""buf_write_from"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	sdio_claim_host(str_func);

	pr_debug(MODULE_NAME ": %s, WRITING DATA TOIO to address 0x%x, "
		 "bytes_to_write=%d\n",
		 __func__,
		reg_str->up_buff_address.reg_val + reg_str->up_wr_ptr.reg_val,
		 bytes_to_write);

	status = sdio_memcpy_toio(str_func,
				  reg_str->up_buff_address.reg_val +
				  reg_str->up_wr_ptr.reg_val,
				  (void *) (outgoing->data + h_read_ptr),
				  bytes_to_write);

	if (status) {
		pr_err(MODULE_NAME ": %s - sdio_memcpy_toio() "
		       "DATA failed. status=%d.\n", __func__, status);
		sdio_release_host(str_func);
		return status;
	}

	sdio_dld_info.global_bytes_write_toio += bytes_to_write;
	outgoing->num_of_bytes_in_use -= bytes_to_write;

	/*
	 * if writing to client succeeded, then
	 * 1. update the client up_wr_ptr
	 * 2. update the host outgoing rd ptr
	 **/
	reg_str->up_wr_ptr.reg_val =
		((reg_str->up_wr_ptr.reg_val + bytes_to_write) %
		 reg_str->ul_buff_size.reg_val);

	/* keeping sdio_dld_info up to date */
	sdio_dld_info.cl_up_wr_ptr = reg_str->up_wr_ptr.reg_val;

	outgoing->offset_read_p =
		((outgoing->offset_read_p + bytes_to_write) %
		  outgoing->buffer_size);

	/* keeping sdio_dld_info up to date*/
	sdio_dld_info.host_read_ptr = outgoing->offset_read_p;

#ifdef CONFIG_DEBUG_FS
	debugfs_glob.global_write_toio = bytes_to_write;
	update_gd(SDIO_DLD_DEBUGFS_CASE_4_CODE);
#endif

	/* updating uplink write pointer according to size that was written */
	status = sdio_memcpy_toio(str_func,
				  reg_str->up_wr_ptr.reg_offset,
				  (void *)(&reg_str->up_wr_ptr.reg_val),
				  sizeof(reg_str->up_wr_ptr.reg_val));
	if (status) {
		pr_err(MODULE_NAME ": %s - sdio_memcpy_toio() "
				       "UPDATE PTR failed. status=%d.\n",
		       __func__, status);
	}

	sdio_release_host(str_func);
	return status;
}

/**
  * sdio_dld_read
  * reads from sdioc
  *
  * @client_rd_ptr: sdioc downlink read ptr.
  * @client_wr_ptr: sdioc downlink write ptr.
  * @reg_str: sdioc register shadowing struct.
  * @str_func: a pointer to func struct.
  * @bytes_read:how many bytes read.
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_read(unsigned int client_rd_ptr,
			 unsigned int client_wr_ptr,
			 struct sdioc_reg_chunk *reg_str,
			 struct sdio_func *str_func,
			 int *bytes_read)
{
	int status = 0;
	struct sdio_data *incoming = &sdio_dld->sdio_dloader_data.incoming_data;

	if (!reg_str) {
		pr_err(MODULE_NAME ": %s - param ""reg_str"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!bytes_read) {
		pr_err(MODULE_NAME ": %s - param ""bytes_read"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	/* there is data to read in ONE chunk */
	if (client_wr_ptr > client_rd_ptr) {
		status = sdio_memcpy_fromio_wrapper(
			str_func,
			client_rd_ptr,
			client_wr_ptr,
			(void *)incoming->data,
			reg_str->dl_buff_address.reg_val + client_rd_ptr,
			client_wr_ptr - client_rd_ptr,
			reg_str->dl_buff_size.reg_val);

		if (status) {
			pr_err(MODULE_NAME ": %s - Failure in Function "
			       "sdio_memcpy_fromio_wrapper(). "
			       "SINGLE CHUNK READ. status=%d\n",
			       __func__, status);
			return status;
		}

		incoming->num_of_bytes_in_use += client_wr_ptr - client_rd_ptr;
		*bytes_read = client_wr_ptr - client_rd_ptr;

#ifdef CONFIG_DEBUG_FS
			debugfs_glob.global_to_read =
				client_wr_ptr - client_rd_ptr;
			update_gd(SDIO_DLD_DEBUGFS_CASE_11_CODE);
#endif
	}

	/* there is data to read in TWO chunks */
	else {
		int dl_buf_size = reg_str->dl_buff_size.reg_val;
		int tail_size = dl_buf_size - client_rd_ptr;

		/* reading chunk#1: from rd_ptr to the end of the buffer */
		status = sdio_memcpy_fromio_wrapper(
			str_func,
			client_rd_ptr,
			dl_buf_size,
			(void *)incoming->data,
			reg_str->dl_buff_address.reg_val + client_rd_ptr,
			tail_size,
			dl_buf_size);

		if (status) {
			pr_err(MODULE_NAME ": %s - Failure in Function "
			       "sdio_memcpy_fromio_wrapper(). "
			       "1 of 2 CHUNKS READ. status=%d\n",
			       __func__, status);
			return status;
		}

		incoming->num_of_bytes_in_use += tail_size;
		*bytes_read = tail_size;

#ifdef CONFIG_DEBUG_FS
			debugfs_glob.global_to_read = tail_size;
			update_gd(SDIO_DLD_DEBUGFS_CASE_11_CODE);
#endif

		/* reading chunk#2: reading from beginning buffer */
		status = sdio_memcpy_fromio_wrapper(
			str_func,
			client_rd_ptr,
			client_wr_ptr,
			(void *)(incoming->data + tail_size),
			reg_str->dl_buff_address.reg_val,
			client_wr_ptr,
			reg_str->dl_buff_size.reg_val);

		if (status) {
			pr_err(MODULE_NAME ": %s - Failure in Function "
			       "sdio_memcpy_fromio_wrapper(). "
			       "2 of 2 CHUNKS READ. status=%d\n",
			       __func__, status);
			return status;
		}

		incoming->num_of_bytes_in_use += client_wr_ptr;
		*bytes_read += client_wr_ptr;

#ifdef CONFIG_DEBUG_FS
			debugfs_glob.global_to_read = client_wr_ptr;
			update_gd(SDIO_DLD_DEBUGFS_CASE_11_CODE);
#endif
	}
	return 0;
}

/**
  * sdio_dld_main_task
  * sdio downloader main task. reads mailboxf checks if there is
  * anything to read, checks if host has anything to
  * write.
  *
  * @card: a pointer to mmc_card.
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_main_task(void *card)
{
	int status = 0;
	struct tty_struct *tty = sdio_dld->tty_str;
	struct sdioc_reg_chunk *reg_str =
		&sdio_dld->sdio_dloader_data.sdioc_reg;
	int func = sdio_dld->sdioc_boot_func;
	struct sdio_func *str_func = NULL;
	struct sdio_data *outgoing = &sdio_dld->sdio_dloader_data.outgoing_data;
	struct sdio_data *incoming = &sdio_dld->sdio_dloader_data.incoming_data;
	struct sdio_dld_task *task = &sdio_dld->dld_main_thread;
	int retries = 0;
#ifdef PUSH_STRING
	int bytes_pushed = 0;
#endif

	msleep(SLEEP_MS);

	if (!card) {
		pr_err(MODULE_NAME ": %s - param ""card"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!tty) {
		pr_err(MODULE_NAME ": %s - param ""tty"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	str_func = ((struct mmc_card *)card)->
		sdio_func[REAL_FUNC_TO_FUNC_IN_ARRAY(func)];

	if (!str_func) {
		pr_err(MODULE_NAME ": %s - param ""str_func"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	while (true) {
		/* client pointers for both buffers */
		int client_ul_wr_ptr = 0;
		int client_ul_rd_ptr = 0;
		int client_dl_wr_ptr = 0;
		int client_dl_rd_ptr = 0;

		/* host pointer for outgoing buffer */
		int h_out_wr_ptr = 0;
		int h_out_rd_ptr = 0;

		int h_bytes_rdy_wr = 0;
		int c_bytes_rdy_rcve = 0;

		int need_to_write = 0;
		int need_to_read = 0;

		/*
		 * forever, checking for signal to die, then read MailBox.
		 * if nothing to read or nothing to write to client, sleep,
		 * and again read MailBox
		 */
		do {
			int dummy = 0;

			/*  checking if a signal to die was sent */
			if (atomic_read(&task->please_close) == 1) {

				pr_debug(MODULE_NAME ": %s - 0x%x was written "
					 "to 9K\n", __func__, SDIOC_EXIT_CODE);

				sdio_claim_host(str_func);

				/* returned value is not checked on purpose */
				sdio_memcpy_toio(
					str_func,
					reg_str->good_to_exit_ptr.reg_offset,
					(void *)&reg_str->good_to_exit_ptr.
					reg_val,
					sizeof(reg_str->good_to_exit_ptr.
					       reg_val));

				sdio_release_host(str_func);

				task->exit_wait.wake_up_signal = 1;
				wake_up(&task->exit_wait.wait_event);
				return 0;
			}

			status = mailbox_to_seq_chunk_read_ptrs(str_func);
			if (status) {
				pr_err(MODULE_NAME ": %s - Failure in Function "
				       "mailbox_to_seq_chunk_read_ptrs(). "
				       "status=%d\n", __func__, status);
				return status;
			}

			/* calculate how many bytes the host has send */
			h_out_wr_ptr = outgoing->offset_write_p;
			h_out_rd_ptr = outgoing->offset_read_p;

			status = sdioc_bytes_till_end_of_buffer(
				h_out_wr_ptr,
				h_out_rd_ptr,
				outgoing->buffer_size,
				&dummy,
				&h_bytes_rdy_wr);

			if (status) {
				pr_err(MODULE_NAME ": %s - Failure in Function "
				       "sdioc_bytes_till_end_of_buffer(). "
				       "status=%d\n", __func__, status);
				return status;
			}

			/* is there something to read from client */
			client_dl_wr_ptr = reg_str->dl_wr_ptr.reg_val;
			client_dl_rd_ptr = reg_str->dl_rd_ptr.reg_val;

			if (client_dl_rd_ptr != client_dl_wr_ptr)
				need_to_read = 1;

			/*
			 *  calculate how many bytes the client can receive
			 *  from host
			 */
			client_ul_wr_ptr = reg_str->up_wr_ptr.reg_val;
			client_ul_rd_ptr = reg_str->up_rd_ptr.reg_val;

			status = sdioc_bytes_till_end_of_buffer(
				client_ul_wr_ptr,
				client_ul_rd_ptr,
				reg_str->ul_buff_size.reg_val,
				&c_bytes_rdy_rcve,
				&dummy);

			if (status) {
				pr_err(MODULE_NAME ": %s - Failure in Function "
				       "sdioc_bytes_till_end_of_buffer(). "
				       "status=%d\n", __func__, status);
				return status;
			}

			/* if host has anything to write */
			if (h_bytes_rdy_wr > 0)
				need_to_write = 1;

			if (need_to_write || need_to_read)
				break;

			spin_lock_irqsave(&lock2, lock_flags2);
			sdio_dld->main_loop_event.wake_up_signal = 0;
			spin_unlock_irqrestore(&lock2, lock_flags2);

			pr_debug(MODULE_NAME ": %s - MAIN LOOP - WAITING...\n",
				 __func__);
#ifdef CONFIG_DEBUG_FS
			update_gd(SDIO_DLD_DEBUGFS_CASE_6_CODE);
#endif
			wait_event(sdio_dld->main_loop_event.wait_event,
				   sdio_dld->main_loop_event.wake_up_signal);
#ifdef CONFIG_DEBUG_FS
			update_gd(SDIO_DLD_DEBUGFS_CASE_7_CODE);
#endif

			pr_debug(MODULE_NAME ": %s - MAIN LOOP - WOKE UP...\n",
				 __func__);

		} while (1);

		/* CHECK IF THERE IS ANYTHING TO READ IN CLIENT */
		if (need_to_read) {
#ifdef PUSH_STRING
			int num_push = 0;
			int left = 0;
			int bytes_read;
#else
			int i;
#endif
			need_to_read = 0;

			status = sdio_dld_read(client_dl_rd_ptr,
					       client_dl_wr_ptr,
					       reg_str,
					       str_func,
					       &bytes_read);

			if (status) {
				pr_err(MODULE_NAME ": %s - Failure in Function "
				       "sdio_dld_read(). status=%d\n",
				       __func__, status);
				return status;
			}

			sdio_dld_info.global_bytes_read_fromio +=
				bytes_read;

			bytes_pushed = 0;
#ifdef PUSH_STRING
			left = incoming->num_of_bytes_in_use;
			start_timer(&sdio_dld->push_timer,
				    sdio_dld->push_timer_ms);
			do {
				num_push = tty_insert_flip_string(
					tty,
					incoming->data+bytes_pushed,
					left);

				bytes_pushed += num_push;
				left -= num_push;
				tty_flip_buffer_push(tty);
			} while (left != 0);

			del_timer(&sdio_dld->push_timer);

			if (bytes_pushed != incoming->num_of_bytes_in_use) {
				pr_err(MODULE_NAME ": %s - failed\n",
				       __func__);
			}
#else
			pr_debug(MODULE_NAME ": %s - NEED TO READ %d\n",
			       __func__, incoming->num_of_bytes_in_use);

			for (i = 0 ; i < incoming->num_of_bytes_in_use ; ++i) {
				int err = 0;
				err = tty_insert_flip_char(tty,
							   incoming->data[i],
							   TTY_NORMAL);
				tty_flip_buffer_push(tty);
			}

			pr_debug(MODULE_NAME ": %s - JUST READ\n", __func__);
#endif /*PUSH_STRING*/
			sdio_dld_info.global_bytes_push_tty +=
				incoming->num_of_bytes_in_use;
#ifdef CONFIG_DEBUG_FS
			debugfs_glob.global_push_to_tty = bytes_read;
			update_gd(SDIO_DLD_DEBUGFS_CASE_12_CODE);
#endif
			incoming->num_of_bytes_in_use = 0;
			tty_flip_buffer_push(tty);
		}

		/* CHECK IF THERE IS ANYTHING TO WRITE IN HOST AND HOW MUCH */
		if (need_to_write) {
			int dummy = 0;

			do {
				int bytes_to_write = min(c_bytes_rdy_rcve,
							 h_bytes_rdy_wr);

				/*
				 * in case nothing to send or no room to
				 * receive
				 */
				if (bytes_to_write == 0)
					break;

				if (client_ul_rd_ptr == 0 &&
				    (client_ul_rd_ptr != client_ul_wr_ptr))
					break;

				/*
				 * if client_rd_ptr points to start, but there
				 * is data to read wait until WRITE_TILL_END
				 * before writing a chunk of data, to avoid
				 * writing until (BUF_SIZE - 1), because it will
				 * yield an extra write of "1" bytes
				 */
				if (client_ul_rd_ptr == 0 &&
				    (client_ul_rd_ptr != client_ul_wr_ptr) &&
				    retries < WRITE_TILL_END_RETRIES) {
					retries++;
					break;
				}
				retries = 0;

#ifdef CONFIG_DEBUG_FS
				debugfs_glob.global_8k_has = h_bytes_rdy_wr;
				debugfs_glob.global_9k_has = c_bytes_rdy_rcve;
				debugfs_glob.global_min = bytes_to_write;
				update_gd(SDIO_DLD_DEBUGFS_CASE_2_CODE);
#endif
				need_to_write = 0;

				pr_debug(MODULE_NAME ": %s - NEED TO WRITE "
					 "TOIO %d\n",
					 __func__, bytes_to_write);

				status = sdio_memcpy_toio_wrapper(
					str_func,
					reg_str->up_wr_ptr.reg_val,
					outgoing->offset_read_p,
					(void *)((char *)outgoing->data +
						 outgoing->offset_read_p),
					bytes_to_write);

				if (status) {
					pr_err(MODULE_NAME ": %s - Failure in "
					       "Function "
					       "sdio_memcpy_toio_wrapper(). "
					       "SINGLE CHUNK WRITE. "
					       "status=%d\n",
					       __func__, status);
					return status;
				}

				sdio_claim_host(str_func);

				status = sdio_memcpy_fromio(
					str_func,
					(void *)&reg_str->up_rd_ptr.reg_val,
					SDIOC_UL_RD_PTR,
					sizeof(reg_str->up_rd_ptr.reg_val));

				if (status) {
					pr_err(MODULE_NAME ": %s - "
					       "sdio_memcpy_fromio() "
					       "failed. status=%d\n",
					       __func__, status);
					sdio_release_host(str_func);

					return status;
				}

				sdio_release_host(str_func);

				spin_lock_irqsave(&lock1, lock_flags1);
				if (sdio_dld->write_callback_event.
				    wake_up_signal == 0) {
					sdio_dld->write_callback_event.
						wake_up_signal = 1;
					wake_up(&sdio_dld->
						write_callback_event.
						wait_event);
				}

				spin_unlock_irqrestore(&lock1, lock_flags1);
				client_ul_wr_ptr = reg_str->up_wr_ptr.reg_val;
				client_ul_rd_ptr = reg_str->up_rd_ptr.reg_val;

				status = sdioc_bytes_till_end_of_buffer(
					client_ul_wr_ptr,
					client_ul_rd_ptr,
					reg_str->ul_buff_size.reg_val,
					&c_bytes_rdy_rcve,
					&dummy);

				/* calculate how many bytes host has to send */
				h_out_wr_ptr = outgoing->offset_write_p;
				h_out_rd_ptr = outgoing->offset_read_p;

				status = sdioc_bytes_till_end_of_buffer(
					h_out_wr_ptr,
					h_out_rd_ptr,
					outgoing->buffer_size,
					&dummy,
					&h_bytes_rdy_wr);

			} while (h_out_wr_ptr != h_out_rd_ptr);
		}
	}
	return 0;
}

/**
  * sdio_dld_init_global
  * initialization of sdio_dld global struct
  *
  * @card: a pointer to mmc_card.
  * @return 0 on success or negative value on error.
  */
static int sdio_dld_init_global(struct mmc_card *card,
				int(*done)(void))
{
	if (!card) {
		pr_err(MODULE_NAME ": %s - param ""card"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!done) {
		pr_err(MODULE_NAME ": %s - param ""done"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	sdio_dld->done_callback = done;
	sdio_dld->card = card;
	init_waitqueue_head(&sdio_dld->dld_main_thread.exit_wait.wait_event);
	sdio_dld->write_callback_event.wake_up_signal = 1;
	sdio_dld->main_loop_event.wake_up_signal = 1;

	sdio_dld->sdio_dloader_data.sdioc_reg.dl_buff_size.reg_offset =
		SDIOC_DL_BUFF_SIZE_OFFSET;

	sdio_dld->sdio_dloader_data.sdioc_reg.dl_rd_ptr.reg_offset =
		SDIOC_DL_RD_PTR;

	sdio_dld->sdio_dloader_data.sdioc_reg.dl_wr_ptr.reg_offset =
		SDIOC_DL_WR_PTR;

	sdio_dld->sdio_dloader_data.sdioc_reg.ul_buff_size.reg_offset =
		SDIOC_UP_BUFF_SIZE_OFFSET;

	sdio_dld->sdio_dloader_data.sdioc_reg.up_rd_ptr.reg_offset =
		SDIOC_UL_RD_PTR;

	sdio_dld->sdio_dloader_data.sdioc_reg.up_wr_ptr.reg_offset =
		SDIOC_UL_WR_PTR;

	sdio_dld->sdio_dloader_data.sdioc_reg.good_to_exit_ptr.reg_offset =
		SDIOC_EXIT_PTR;

	sdio_dld->sdio_dloader_data.sdioc_reg.dl_buff_address.reg_offset =
		SDIOC_DL_BUFF_ADDRESS;

	sdio_dld->sdio_dloader_data.sdioc_reg.up_buff_address.reg_offset =
		SDIOC_UP_BUFF_ADDRESS;

	sdio_dld_set_op_mode(SDIO_DLD_NORMAL_MODE);

	return 0;
}

/**
 * sdio_downloader_setup
 * initializes the TTY driver
 *
 * @card: a pointer to mmc_card.
 * @num_of_devices: number of devices.
 * @channel_number: channel number.
 * @return 0 on success or negative value on error.
 *
 * The TTY stack needs to know in advance how many devices it should
 * plan to manage. Use this call to set up the ports that will
 * be exported through SDIO.
 */
int sdio_downloader_setup(struct mmc_card *card,
			  unsigned int num_of_devices,
			  int channel_number,
			  int(*done)(void))
{
	int status = 0;
	int result = 0;
	int func_in_array = 0;
	struct sdio_func *str_func = NULL;
	struct device *tty_dev;

	if (num_of_devices == 0 || num_of_devices > MAX_NUM_DEVICES) {
		pr_err(MODULE_NAME ": %s - invalid number of devices\n",
		       __func__);
		return -EINVAL;
	}

	if (!card) {
		pr_err(MODULE_NAME ": %s - param ""card"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	if (!done) {
		pr_err(MODULE_NAME ": %s - param ""done"" is NULL.\n",
		       __func__);
		return -EINVAL;
	}

	sdio_dld = kzalloc(sizeof(struct sdio_downloader), GFP_KERNEL);
	if (!sdio_dld) {
		pr_err(MODULE_NAME ": %s - couldn't allocate sdio_dld data "
		       "structure.", __func__);
		return -ENOMEM;
	}

#ifdef CONFIG_DEBUG_FS
	bootloader_debugfs_init();
#endif /* CONFIG_DEBUG_FS */

	status = sdio_dld_init_global(card, done);

	if (status) {
		pr_err(MODULE_NAME ": %s - Failure in Function "
		       "sdio_dld_init_global(). status=%d\n",
		       __func__, status);
		kfree(sdio_dld);
		return status;
	}

	sdio_dld->tty_drv = alloc_tty_driver(num_of_devices);

	if (!sdio_dld->tty_drv) {
		pr_err(MODULE_NAME ": %s - param ""sdio_dld->tty_drv"" is "
				   "NULL.\n", __func__);
		kfree(sdio_dld);
		return -EINVAL;
	}

	sdio_dld_set_op_mode((enum sdio_dld_op_mode)sdio_op_mode);

	/* according to op_mode, a different tty device is created */
	if (sdio_dld->op_mode == SDIO_DLD_BOOT_TEST_MODE)
		sdio_dld->tty_drv->name = TTY_SDIO_DEV_TEST;
	else
	    sdio_dld->tty_drv->name = TTY_SDIO_DEV;

	sdio_dld->tty_drv->owner = THIS_MODULE;
	sdio_dld->tty_drv->driver_name = "SDIO_Dloader";

	/* uses dynamically assigned dev_t values */
	sdio_dld->tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	sdio_dld->tty_drv->subtype = SERIAL_TYPE_NORMAL;
	sdio_dld->tty_drv->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV
				| TTY_DRIVER_RESET_TERMIOS;

	/* initializing the tty driver */
	sdio_dld->tty_drv->init_termios = tty_std_termios;
	sdio_dld->tty_drv->init_termios.c_cflag =
		B4800 | CS8 | CREAD | HUPCL | CLOCAL;
	sdio_dld->tty_drv->init_termios.c_ispeed = INPUT_SPEED;
	sdio_dld->tty_drv->init_termios.c_ospeed = OUTPUT_SPEED;

	tty_set_operations(sdio_dld->tty_drv, &sdio_dloader_tty_ops);

	status = tty_register_driver(sdio_dld->tty_drv);
	if (status) {
		put_tty_driver(sdio_dld->tty_drv);
		pr_err(MODULE_NAME ": %s - tty_register_driver() failed\n",
			__func__);

		sdio_dld->tty_drv = NULL;
		kfree(sdio_dld);
		return status;
	}

	tty_dev = tty_register_device(sdio_dld->tty_drv, 0, NULL);
	if (IS_ERR(tty_dev)) {
		pr_err(MODULE_NAME ": %s - tty_register_device() "
			"failed\n", __func__);
		tty_unregister_driver(sdio_dld->tty_drv);
		kfree(sdio_dld);
		return PTR_ERR(tty_dev);
	}

	sdio_dld->sdioc_boot_func = SDIOC_CHAN_TO_FUNC_NUM(channel_number);
	func_in_array = REAL_FUNC_TO_FUNC_IN_ARRAY(sdio_dld->sdioc_boot_func);
	str_func = sdio_dld->card->sdio_func[func_in_array];
	status = sdio_dld_init_func(str_func);
	if (status) {
		pr_err(MODULE_NAME ": %s - Failure in Function "
		       "sdio_dld_init_func(). status=%d\n",
		       __func__, status);
		goto exit_err;
	}

#ifdef CONFIG_DEBUG_FS
	sdio_dld_debug_init();
#endif

	sdio_claim_host(str_func);

	/*
	 * notifing the client by writing what mode we are by writing
	 * to a special register
	 */
	status = sdio_memcpy_toio(str_func,
				  SDIOC_OP_MODE_PTR,
				  (void *)&sdio_dld->op_mode,
				  sizeof(sdio_dld->op_mode));

	sdio_release_host(str_func);

	if (status) {
		pr_err(MODULE_NAME ": %s - sdio_memcpy_toio() "
		       "writing to OP_MODE_REGISTER failed. "
		       "status=%d.\n",
		       __func__, status);
		goto exit_err;
	}

	return 0;

exit_err:
	tty_unregister_device(sdio_dld->tty_drv, 0);
	result = tty_unregister_driver(sdio_dld->tty_drv);
	if (result)
		pr_err(MODULE_NAME ": %s - tty_unregister_driver() "
		       "failed. result=%d\n", __func__, -result);
	kfree(sdio_dld);
	return status;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SDIO Downloader");
MODULE_AUTHOR("Yaniv Gardi <ygardi@codeaurora.org>");
MODULE_VERSION(DRV_VERSION);

