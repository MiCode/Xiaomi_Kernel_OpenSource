/* arch/arm/mach-msm/qdsp5/adsp.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2010, 2012 Code Aurora Forum. All rights reserved.
 * Author: Iliyan Malchev <ibm@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ARCH_ARM_MACH_MSM_ADSP_H
#define _ARCH_ARM_MACH_MSM_ADSP_H

#include <linux/types.h>
#include <linux/msm_adsp.h>
#include <linux/msm_ion.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_adsp.h>

int adsp_pmem_fixup(struct msm_adsp_module *module, void **addr,
		    unsigned long len);
int adsp_ion_do_cache_op(struct msm_adsp_module *module, void *addr,
			void *paddr, unsigned long len,
			unsigned long offset, int cmd);
int adsp_ion_fixup_kvaddr(struct msm_adsp_module *module, void **addr,
			   unsigned long *kvaddr, unsigned long len,
			   struct file **filp, unsigned long *offset);
int adsp_pmem_paddr_fixup(struct msm_adsp_module *module, void **addr);

int adsp_vfe_verify_cmd(struct msm_adsp_module *module,
			unsigned int queue_id, void *cmd_data,
			size_t cmd_size);
int adsp_jpeg_verify_cmd(struct msm_adsp_module *module,
			 unsigned int queue_id, void *cmd_data,
			 size_t cmd_size);
int adsp_lpm_verify_cmd(struct msm_adsp_module *module,
			unsigned int queue_id, void *cmd_data,
			size_t cmd_size);
int adsp_video_verify_cmd(struct msm_adsp_module *module,
			  unsigned int queue_id, void *cmd_data,
			  size_t cmd_size);
int adsp_videoenc_verify_cmd(struct msm_adsp_module *module,
			  unsigned int queue_id, void *cmd_data,
			  size_t cmd_size);
void q5audio_dsp_not_responding(void);

struct adsp_event;

int adsp_vfe_patch_event(struct msm_adsp_module *module,
			struct adsp_event *event);

int adsp_jpeg_patch_event(struct msm_adsp_module *module,
			struct adsp_event *event);


struct adsp_module_info {
	const char *name;
	const char *pdev_name;
	uint32_t id;
	const char *clk_name;
	unsigned long clk_rate;
	int (*verify_cmd) (struct msm_adsp_module*, unsigned int, void *,
			   size_t);
	int (*patch_event) (struct msm_adsp_module*, struct adsp_event *);
};

#define ADSP_EVENT_MAX_SIZE 496
#define EVENT_LEN       12
#define EVENT_MSG_ID ((uint16_t)~0)

struct adsp_event {
	struct list_head list;
	uint32_t size; /* always in bytes */
	uint16_t msg_id;
	uint16_t type; /* 0 for msgs (from aDSP), -1 for events (from ARM9) */
	int is16; /* always 0 (msg is 32-bit) when the event type is 1(ARM9) */
	union {
		uint16_t msg16[ADSP_EVENT_MAX_SIZE / 2];
		uint32_t msg32[ADSP_EVENT_MAX_SIZE / 4];
	} data;
};

struct adsp_info {
	uint32_t send_irq;
	uint32_t read_ctrl;
	uint32_t write_ctrl;

	uint32_t max_msg16_size;
	uint32_t max_msg32_size;

	uint32_t max_task_id;
	uint32_t max_module_id;
	uint32_t max_queue_id;
	uint32_t max_image_id;

	/* for each image id, a map of queue id to offset */
	uint32_t **queue_offset;

	/* for each image id, a map of task id to module id */
	uint32_t **task_to_module;

	/* for each module id, map of module id to module */
	struct msm_adsp_module **id_to_module;

	uint32_t module_count;
	struct adsp_module_info *module;

	/* stats */
	uint32_t events_received;
	uint32_t event_backlog_max;

	/* rpc_client for init_info */
	struct msm_rpc_endpoint	*init_info_rpc_client;
	struct adsp_rtos_mp_mtoa_init_info_type	*init_info_ptr;
	wait_queue_head_t	init_info_wait;
	unsigned 		init_info_state;
	struct mutex lock;

	/* Interrupt value */
	int int_adsp;
};

#define RPC_ADSP_RTOS_ATOM_NULL_PROC 0
#define RPC_ADSP_RTOS_MTOA_NULL_PROC 0
#define RPC_ADSP_RTOS_APP_TO_MODEM_PROC 2
#define RPC_ADSP_RTOS_MODEM_TO_APP_PROC 2
#define RPC_ADSP_RTOS_MTOA_EVENT_INFO_PROC 3
#define RPC_ADSP_RTOS_MTOA_INIT_INFO_PROC 4

enum rpc_adsp_rtos_proc_type {
	RPC_ADSP_RTOS_PROC_NONE = 0,
	RPC_ADSP_RTOS_PROC_MODEM = 1,
	RPC_ADSP_RTOS_PROC_APPS = 2,
};

enum {
	RPC_ADSP_RTOS_CMD_REGISTER_APP,
	RPC_ADSP_RTOS_CMD_ENABLE,
	RPC_ADSP_RTOS_CMD_DISABLE,
	RPC_ADSP_RTOS_CMD_KERNEL_COMMAND,
	RPC_ADSP_RTOS_CMD_16_COMMAND,
	RPC_ADSP_RTOS_CMD_32_COMMAND,
	RPC_ADSP_RTOS_CMD_DISABLE_EVENT_RSP,
	RPC_ADSP_RTOS_CMD_REMOTE_EVENT,
	RPC_ADSP_RTOS_CMD_SET_STATE,
	RPC_ADSP_RTOS_CMD_REMOTE_INIT_INFO_EVENT,
	RPC_ADSP_RTOS_CMD_GET_INIT_INFO,
};

enum rpc_adsp_rtos_mod_status_type {
	RPC_ADSP_RTOS_MOD_READY,
	RPC_ADSP_RTOS_MOD_DISABLE,
	RPC_ADSP_RTOS_SERVICE_RESET,
	RPC_ADSP_RTOS_CMD_FAIL,
	RPC_ADSP_RTOS_CMD_SUCCESS,
	RPC_ADSP_RTOS_INIT_INFO,
	RPC_ADSP_RTOS_DISABLE_FAIL,
};

struct rpc_adsp_rtos_app_to_modem_args_t {
	struct rpc_request_hdr hdr;
	uint32_t gotit; /* if 1, the next elements are present */
	uint32_t cmd; /* e.g., RPC_ADSP_RTOS_CMD_REGISTER_APP */
	uint32_t proc_id; /* e.g., RPC_ADSP_RTOS_PROC_APPS */
	uint32_t module; /* e.g., QDSP_MODULE_AUDPPTASK */
};

enum qdsp_image_type {
	QDSP_IMAGE_COMBO,
	QDSP_IMAGE_GAUDIO,
	QDSP_IMAGE_QTV_LP,
	QDSP_IMAGE_MAX,
	/* DO NOT USE: Force this enum to be a 32bit type to improve speed */
	QDSP_IMAGE_32BIT_DUMMY = 0x10000
};

struct adsp_rtos_mp_mtoa_header_type {
	enum rpc_adsp_rtos_mod_status_type  event;
	enum rpc_adsp_rtos_proc_type        proc_id;
};

/* ADSP RTOS MP Communications - Modem to APP's  Event Info*/
struct adsp_rtos_mp_mtoa_type {
	uint32_t	module;
	uint32_t	image;
	uint32_t	apps_okts;
};

/* ADSP RTOS MP Communications - Modem to APP's Init Info  */
#if CONFIG_ADSP_RPC_VER > 0x30001
#define IMG_MAX         2
#define ENTRIES_MAX     36
#define MODULES_MAX     64
#else
#define IMG_MAX         6
#define ENTRIES_MAX     48
#endif
#define QUEUES_MAX      64

struct queue_to_offset_type {
	uint32_t	queue;
	uint32_t	offset;
};

struct mod_to_queue_offsets {
	uint32_t        module;
	uint32_t        q_type;
	uint32_t        q_max_len;
};

struct adsp_rtos_mp_mtoa_init_info_type {
	uint32_t	image_count;
	uint32_t	num_queue_offsets;
	struct queue_to_offset_type	queue_offsets_tbl[IMG_MAX][ENTRIES_MAX];
	uint32_t	num_task_module_entries;
	uint32_t	task_to_module_tbl[IMG_MAX][ENTRIES_MAX];

	uint32_t	module_table_size;
#if CONFIG_ADSP_RPC_VER > 0x30001
	uint32_t	module_entries[MODULES_MAX];
#else
	uint32_t	module_entries[ENTRIES_MAX];
#endif
	uint32_t	mod_to_q_entries;
	struct mod_to_queue_offsets	mod_to_q_tbl[ENTRIES_MAX];
	/*
	 * queue_offsets[] is to store only queue_offsets
	 */
	uint32_t	queue_offsets[IMG_MAX][QUEUES_MAX];
};

struct adsp_rtos_mp_mtoa_s_type {
	struct adsp_rtos_mp_mtoa_header_type mp_mtoa_header;
#if CONFIG_ADSP_RPC_VER == 0x30001
	uint32_t desc_field;
#endif
	union {
		struct adsp_rtos_mp_mtoa_init_info_type mp_mtoa_init_packet;
		struct adsp_rtos_mp_mtoa_type mp_mtoa_packet;
	} adsp_rtos_mp_mtoa_data;
};

struct rpc_adsp_rtos_modem_to_app_args_t {
	struct rpc_request_hdr hdr;
	uint32_t gotit; /* if 1, the next elements are present */
	struct adsp_rtos_mp_mtoa_s_type mtoa_pkt;
};

#define ADSP_STATE_DISABLED   0
#define ADSP_STATE_ENABLING   1
#define ADSP_STATE_ENABLED    2
#define ADSP_STATE_DISABLING  3
#define ADSP_STATE_INIT_INFO  4

struct msm_adsp_module {
	struct mutex lock;
	const char *name;
	unsigned id;
	struct adsp_info *info;

	struct msm_rpc_endpoint *rpc_client;
	struct msm_adsp_ops *ops;
	void *driver_data;

	/* statistics */
	unsigned num_commands;
	unsigned num_events;

	wait_queue_head_t state_wait;
	unsigned state;

	struct platform_device pdev;
	struct clk *clk;
	int open_count;

	struct mutex ion_regions_lock;
	struct hlist_head ion_regions;
	int (*verify_cmd) (struct msm_adsp_module*, unsigned int, void *,
			   size_t);
	int (*patch_event) (struct msm_adsp_module*, struct adsp_event *);
};

extern void msm_adsp_publish_cdevs(struct msm_adsp_module *, unsigned);
extern int adsp_init_info(struct adsp_info *info);
extern void rmtask_init(void);

/* Value to indicate that a queue is not defined for a particular image */
#define QDSP_RTOS_NO_QUEUE  0xfffffffe

/*
 * Constants used to communicate with the ADSP RTOS
 */
#define ADSP_RTOS_WRITE_CTRL_WORD_MUTEX_M            0x80000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_MUTEX_NAVAIL_V     0x80000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_MUTEX_AVAIL_V      0x00000000U

#define ADSP_RTOS_WRITE_CTRL_WORD_CMD_M              0x70000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_CMD_WRITE_REQ_V    0x00000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_CMD_WRITE_DONE_V   0x10000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_CMD_NO_CMD_V       0x70000000U

#define ADSP_RTOS_WRITE_CTRL_WORD_STATUS_M           0x0E000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_NO_ERR_V           0x00000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_NO_FREE_BUF_V      0x02000000U

#define ADSP_RTOS_WRITE_CTRL_WORD_KERNEL_FLG_M       0x01000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_HTOD_MSG_WRITE_V   0x00000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_HTOD_CMD_V         0x01000000U

#define ADSP_RTOS_WRITE_CTRL_WORD_DSP_ADDR_M         0x00FFFFFFU
#define ADSP_RTOS_WRITE_CTRL_WORD_HTOD_CMD_ID_M      0x00FFFFFFU

/* Combination of MUTEX and CMD bits to check if the DSP is busy */
#define ADSP_RTOS_WRITE_CTRL_WORD_READY_M            0xF0000000U
#define ADSP_RTOS_WRITE_CTRL_WORD_READY_V            0x70000000U

/* RTOS to Host processor command mask values */
#define ADSP_RTOS_READ_CTRL_WORD_FLAG_M              0x80000000U
#define ADSP_RTOS_READ_CTRL_WORD_FLAG_UP_WAIT_V      0x00000000U
#define ADSP_RTOS_READ_CTRL_WORD_FLAG_UP_CONT_V      0x80000000U

#define ADSP_RTOS_READ_CTRL_WORD_CMD_M               0x60000000U
#define ADSP_RTOS_READ_CTRL_WORD_READ_DONE_V         0x00000000U
#define ADSP_RTOS_READ_CTRL_WORD_READ_REQ_V          0x20000000U
#define ADSP_RTOS_READ_CTRL_WORD_NO_CMD_V            0x60000000U

/* Combination of FLAG and COMMAND bits to check if MSG ready */
#define ADSP_RTOS_READ_CTRL_WORD_READY_M             0xE0000000U
#define ADSP_RTOS_READ_CTRL_WORD_READY_V             0xA0000000U
#define ADSP_RTOS_READ_CTRL_WORD_CONT_V              0xC0000000U
#define ADSP_RTOS_READ_CTRL_WORD_DONE_V              0xE0000000U

#define ADSP_RTOS_READ_CTRL_WORD_STATUS_M            0x18000000U
#define ADSP_RTOS_READ_CTRL_WORD_NO_ERR_V            0x00000000U

#define ADSP_RTOS_READ_CTRL_WORD_IN_PROG_M           0x04000000U
#define ADSP_RTOS_READ_CTRL_WORD_NO_READ_IN_PROG_V   0x00000000U
#define ADSP_RTOS_READ_CTRL_WORD_READ_IN_PROG_V      0x04000000U

#define ADSP_RTOS_READ_CTRL_WORD_CMD_TYPE_M          0x03000000U
#define ADSP_RTOS_READ_CTRL_WORD_CMD_TASK_TO_H_V     0x00000000U
#define ADSP_RTOS_READ_CTRL_WORD_CMD_KRNL_TO_H_V     0x01000000U
#define ADSP_RTOS_READ_CTRL_WORD_CMD_H_TO_KRNL_CFM_V 0x02000000U

#define ADSP_RTOS_READ_CTRL_WORD_DSP_ADDR_M          0x00FFFFFFU

#define ADSP_RTOS_READ_CTRL_WORD_MSG_ID_M            0x000000FFU
#define ADSP_RTOS_READ_CTRL_WORD_TASK_ID_M           0x0000FF00U

/* Base address of DSP and DSP hardware registers */
#define QDSP_RAMC_OFFSET  0x400000

#endif
