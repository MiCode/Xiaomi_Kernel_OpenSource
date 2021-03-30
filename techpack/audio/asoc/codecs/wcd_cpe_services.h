/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 */

#ifndef __CPE_SERVICES__
#define __CPE_SERVICES__

#define CPE_IRQ_OUTBOX_IRQ		0x01
#define CPE_IRQ_MEM_ACCESS_ERROR	0x02
#define CPE_IRQ_WDOG_BITE		0x04
#define CPE_IRQ_BUFFER_OVERFLOW		0x08
#define CPE_IRQ_LAB_OVFUNF		0x10
#define CPE_IRQ_FLL_LOCK_LOST		0x20
#define CPE_IRQ_RCO_WDOG_INT		0x40

#define EFAILED (MAX_ERRNO - 1)
#define ENOTREADY (MAX_ERRNO - 2)

#define MAX_SUPPORTED_CLKFREQ 8
#define CPE_SVC_INIT_PARAM_V1 1

enum cpe_svc_result {
	CPE_SVC_SUCCESS			= 0,
	CPE_SVC_FAILED			= -EFAILED,
	CPE_SVC_NO_MEMORY		= -ENOMEM,
	CPE_SVC_INVALID_HANDLE		= -EINVAL,
	CPE_SVC_NOT_READY		= -ENOTREADY,
	CPE_SVC_SHUTTING_DOWN		= -ESHUTDOWN,
	CPE_SVC_BUSY			= -EBUSY,
};

enum cpe_svc_event {
	CPE_SVC_CMI_MSG			= 0x01,
	CPE_SVC_OFFLINE			= 0x02,
	CPE_SVC_ONLINE			= 0x04,
	CPE_SVC_BOOT_FAILED		= 0x08,
	CPE_SVC_READ_COMPLETE		= 0x10,
	CPE_SVC_READ_ERROR		= 0x20,
	CPE_SVC_BOOT			= 0x40,
	CPE_SVC_CMI_CLIENTS_DEREG	= 0x100,
	CPE_SVC_EVENT_ANCHOR		= 0x7FFF
};

enum cpe_svc_module {
	CPE_SVC_LISTEN_PROC		= 1,
	CPE_SVC_MODULE_ANCHOR		= 0x7F
};

enum cpe_svc_route_dest {
	CPE_SVC_EXTERNAL		= 1,
	CPE_SVC_INTERNAL		= 2,
	CPE_SVC_ROUTE_ANCHOR		= 0x7F
};

enum cpe_svc_mem_type {
	CPE_SVC_DATA_MEM		= 1,
	CPE_SVC_INSTRUCTION_MEM		= 2,
	CPE_SVC_IPC_MEM			= 3,
	CPE_SVC_MEM_TYPE_ANCHOR		= 0x7F
};

enum cpe_svc_codec_id {
	CPE_SVC_CODEC_TOMTOM		= 5,
	CPE_SVC_CODEC_WCD9335		= 7,
	CPE_SVC_CODEC_WCD9326		= 8,
	CPE_SVC_CODEC_ID_ANCHOR		= 0x7ffffff
};

enum cpe_svc_codec_version {
	CPE_SVC_CODEC_V1P0		= 1,
	CPE_SVC_CODEC_VERSION_ANCHOR	= 0x7fffffff
};

struct cpe_svc_codec_info_v1 {
	u16			major_version;/*must be 1*/
	u16			minor_version;/*must be 0*/
	u32			id;
	u32			version;
	/*Add 1.1 version fields after this line*/
};

struct cpe_svc_notification {
	enum cpe_svc_event event;
	enum cpe_svc_result result;
	void *payload;
	void *private_data;
};

struct cpe_svc_msg_payload {
	u8    *cmi_msg;
};

struct cpe_svc_read_complete {
	u8    *buffer;
	size_t   size;
};

struct cpe_svc_boot_event {
	u32 debug_address;
	size_t debug_buffer_size;
	u32 status;
};

struct cpe_svc_mem_segment {
	enum cpe_svc_mem_type type;
	u32 cpe_addr;
	size_t size;
	u8 *data;
};

struct cpe_svc_hw_cfg {
	size_t DRAM_size;
	u32 DRAM_offset;
	size_t IRAM_size;
	u32 IRAM_offset;
	u8 inbox_size;
	u8 outbox_size;
};

struct cpe_svc_cfg_clk_plan {
	u32 current_clk_feq;
	u32 num_clk_freqs;
	u32 clk_freqs[MAX_SUPPORTED_CLKFREQ];
};

struct cpe_svc_init_param {
	void *context;
	u32 version;
	void (*query_freq_plans_cb)(void *cdc_priv,
			struct cpe_svc_cfg_clk_plan *clk_freq);
	void (*change_freq_plan_cb)(void *cdc_priv,
			u32 clk_freq);
};


void *cpe_svc_initialize(
		void irq_control_callback(u32 enable),
		const void *codec_info, void *context);
enum cpe_svc_result cpe_svc_deinitialize(void *cpe_handle);

void *cpe_svc_register(void *cpe_handle,
		void (*notification_callback)(
			const struct cpe_svc_notification *parameter),
		u32 mask, const char *name);

enum cpe_svc_result cpe_svc_deregister(void *cpe_handle, void *reg_handle);

enum cpe_svc_result cpe_svc_download_segment(void *cpe_handle,
		const struct cpe_svc_mem_segment *segment);

enum cpe_svc_result cpe_svc_boot(void *cpe_handle, int debug_mode);

enum cpe_svc_result cpe_svc_shutdown(void *cpe_handle);

enum cpe_svc_result cpe_svc_reset(void *cpe_handle);

enum cpe_svc_result cpe_svc_process_irq(void *cpe_handle, u32 cpe_irq);

enum cpe_svc_result
cpe_svc_route_notification(void *cpe_handle, enum cpe_svc_module module,
		enum cpe_svc_route_dest dest);

enum cpe_svc_result cpe_svc_ramdump(void *cpe_handle,
		struct cpe_svc_mem_segment *buffer);

enum cpe_svc_result cpe_svc_set_debug_mode(void *cpe_handle, u32 mode);

const struct cpe_svc_hw_cfg *cpe_svc_get_hw_cfg(void *cpe_handle);
enum cpe_svc_result cpe_svc_toggle_lab(void *cpe_handle, bool enable);
enum cpe_svc_result cpe_svc_ftm_test(void *cpe_handle, u32 *status);
#endif /*__CPE_SERVICES__*/
