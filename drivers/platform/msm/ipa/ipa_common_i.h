/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/ipa_mhi.h>
#include <linux/ipa_qmi_service_v01.h>

#ifndef _IPA_COMMON_I_H_
#define _IPA_COMMON_I_H_
#include <linux/errno.h>
#include <linux/ipc_logging.h>
#include <linux/ipa.h>
#include <linux/ipa_uc_offload.h>
#include <linux/ipa_wdi3.h>
#include <linux/ipa_wigig.h>
#include <linux/ratelimit.h>

#define WARNON_RATELIMIT_BURST 1
#define IPA_RATELIMIT_BURST 1

#define __FILENAME__ \
	(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = EP; \
		log_info.id_string = (client < 0 || client >= IPA_CLIENT_MAX) \
			? "Invalid Client" : ipa_clients_strings[client]

#define IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = SIMPLE; \
		log_info.id_string = __func__

#define IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = RESOURCE; \
		log_info.id_string = resource_name

#define IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = SPECIAL; \
		log_info.id_string = id_str

#define IPA_ACTIVE_CLIENTS_INC_EP(client) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_EP(client) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_SIMPLE() \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_SIMPLE() \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_RESOURCE(resource_name) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_RESOURCE(resource_name) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_SPECIAL(id_str) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_SPECIAL(id_str) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

/*
 * Printing one warning message in 5 seconds if multiple warning messages
 * are coming back to back.
 */

#define WARN_ON_RATELIMIT_IPA(condition)				\
({								\
	static DEFINE_RATELIMIT_STATE(_rs,			\
				DEFAULT_RATELIMIT_INTERVAL,	\
				WARNON_RATELIMIT_BURST);	\
	int rtn = !!(condition);				\
								\
	if (unlikely(rtn && __ratelimit(&_rs)))			\
		WARN_ON(rtn);					\
})

/*
 * Printing one error message in 5 seconds if multiple error messages
 * are coming back to back.
 */

#define pr_err_ratelimited_ipa(fmt, args...)				\
({									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      IPA_RATELIMIT_BURST);		\
									\
	if (__ratelimit(&_rs))						\
		pr_err(fmt, ## args);					\
})

#define ipa_assert_on(condition)\
do {\
	if (unlikely(condition))\
		ipa_assert();\
} while (0)

#define IPA_CLIENT_IS_PROD(x) \
	(x < IPA_CLIENT_MAX && (x & 0x1) == 0)
#define IPA_CLIENT_IS_CONS(x) \
	(x < IPA_CLIENT_MAX && (x & 0x1) == 1)

#define IPA_GSI_CHANNEL_STOP_SLEEP_MIN_USEC (3000)
#define IPA_GSI_CHANNEL_STOP_SLEEP_MAX_USEC (5000)

enum ipa_active_client_log_type {
	EP,
	SIMPLE,
	RESOURCE,
	SPECIAL,
	INVALID
};

struct ipa_active_client_logging_info {
	const char *id_string;
	char *file;
	int line;
	enum ipa_active_client_log_type type;
};

/**
 * struct ipa_mem_buffer - IPA memory buffer
 * @base: base
 * @phys_base: physical base address
 * @size: size of memory buffer
 */
struct ipa_mem_buffer {
	void *base;
	dma_addr_t phys_base;
	u32 size;
};

/**
 * enum ipa3_mhi_burst_mode - MHI channel burst mode state
 *
 * Values are according to MHI specification
 * @IPA_MHI_BURST_MODE_DEFAULT: burst mode enabled for HW channels,
 * disabled for SW channels
 * @IPA_MHI_BURST_MODE_RESERVED:
 * @IPA_MHI_BURST_MODE_DISABLE: Burst mode is disabled for this channel
 * @IPA_MHI_BURST_MODE_ENABLE: Burst mode is enabled for this channel
 *
 */
enum ipa3_mhi_burst_mode {
	IPA_MHI_BURST_MODE_DEFAULT,
	IPA_MHI_BURST_MODE_RESERVED,
	IPA_MHI_BURST_MODE_DISABLE,
	IPA_MHI_BURST_MODE_ENABLE,
};

/**
 * enum ipa_hw_mhi_channel_states - MHI channel state machine
 *
 * Values are according to MHI specification
 * @IPA_HW_MHI_CHANNEL_STATE_DISABLE: Channel is disabled and not processed by
 *	the host or device.
 * @IPA_HW_MHI_CHANNEL_STATE_ENABLE: A channel is enabled after being
 *	initialized and configured by host, including its channel context and
 *	associated transfer ring. While this state, the channel is not active
 *	and the device does not process transfer.
 * @IPA_HW_MHI_CHANNEL_STATE_RUN: The device processes transfers and doorbell
 *	for channels.
 * @IPA_HW_MHI_CHANNEL_STATE_SUSPEND: Used to halt operations on the channel.
 *	The device does not process transfers for the channel in this state.
 *	This state is typically used to synchronize the transition to low power
 *	modes.
 * @IPA_HW_MHI_CHANNEL_STATE_STOP: Used to halt operations on the channel.
 *	The device does not process transfers for the channel in this state.
 * @IPA_HW_MHI_CHANNEL_STATE_ERROR: The device detected an error in an element
 *	from the transfer ring associated with the channel.
 * @IPA_HW_MHI_CHANNEL_STATE_INVALID: Invalid state. Shall not be in use in
 *	operational scenario.
 */
enum ipa_hw_mhi_channel_states {
	IPA_HW_MHI_CHANNEL_STATE_DISABLE	= 0,
	IPA_HW_MHI_CHANNEL_STATE_ENABLE		= 1,
	IPA_HW_MHI_CHANNEL_STATE_RUN		= 2,
	IPA_HW_MHI_CHANNEL_STATE_SUSPEND	= 3,
	IPA_HW_MHI_CHANNEL_STATE_STOP		= 4,
	IPA_HW_MHI_CHANNEL_STATE_ERROR		= 5,
	IPA_HW_MHI_CHANNEL_STATE_INVALID	= 0xFF
};

/**
 * Structure holding the parameters for IPA_CPU_2_HW_CMD_MHI_DL_UL_SYNC_INFO
 * command. Parameters are sent as 32b immediate parameters.
 * @isDlUlSyncEnabled: Flag to indicate if DL UL Syncronization is enabled
 * @UlAccmVal: UL Timer Accumulation value (Period after which device will poll
 *	for UL data)
 * @ulMsiEventThreshold: Threshold at which HW fires MSI to host for UL events
 * @dlMsiEventThreshold: Threshold at which HW fires MSI to host for DL events
 */
union IpaHwMhiDlUlSyncCmdData_t {
	struct IpaHwMhiDlUlSyncCmdParams_t {
		u32 isDlUlSyncEnabled:8;
		u32 UlAccmVal:8;
		u32 ulMsiEventThreshold:8;
		u32 dlMsiEventThreshold:8;
	} params;
	u32 raw32b;
};

struct ipa_mhi_ch_ctx {
	u8 chstate;/*0-7*/
	u8 brstmode:2;/*8-9*/
	u8 pollcfg:6;/*10-15*/
	u16 rsvd;/*16-31*/
	u32 chtype;
	u32 erindex;
	u64 rbase;
	u64 rlen;
	u64 rp;
	u64 wp;
} __packed;

struct ipa_mhi_ev_ctx {
	u32 intmodc:16;
	u32 intmodt:16;
	u32 ertype;
	u32 msivec;
	u64 rbase;
	u64 rlen;
	u64 rp;
	u64 wp;
} __packed;

struct ipa_mhi_init_uc_engine {
	struct ipa_mhi_msi_info *msi;
	u32 mmio_addr;
	u32 host_ctrl_addr;
	u32 host_data_addr;
	u32 first_ch_idx;
	u32 first_er_idx;
	union IpaHwMhiDlUlSyncCmdData_t *ipa_cached_dl_ul_sync_info;
};

struct ipa_mhi_init_gsi_engine {
	u32 first_ch_idx;
};

struct ipa_mhi_init_engine {
	struct ipa_mhi_init_uc_engine uC;
	struct ipa_mhi_init_gsi_engine gsi;
};

struct start_gsi_channel {
	enum ipa_hw_mhi_channel_states state;
	struct ipa_mhi_msi_info *msi;
	struct ipa_mhi_ev_ctx *ev_ctx_host;
	u64 event_context_addr;
	struct ipa_mhi_ch_ctx *ch_ctx_host;
	u64 channel_context_addr;
	void (*ch_err_cb)(struct gsi_chan_err_notify *notify);
	void (*ev_err_cb)(struct gsi_evt_err_notify *notify);
	void *channel;
	bool assert_bit40;
	struct gsi_mhi_channel_scratch *mhi;
	unsigned long *cached_gsi_evt_ring_hdl;
	uint8_t evchid;
};

struct start_uc_channel {
	enum ipa_hw_mhi_channel_states state;
	u8 index;
	u8 id;
};

struct start_mhi_channel {
	struct start_uc_channel uC;
	struct start_gsi_channel gsi;
};

struct ipa_mhi_connect_params_internal {
	struct ipa_sys_connect_params *sys;
	u8 channel_id;
	struct start_mhi_channel start;
};

/**
 * struct ipa_hdr_offset_entry - IPA header offset entry
 * @link: entry's link in global header offset entries list
 * @offset: the offset
 * @bin: bin
 * @ipacm_installed: indicate if installed by ipacm
 */
struct ipa_hdr_offset_entry {
	struct list_head link;
	u32 offset;
	u32 bin;
	bool ipacm_installed;
};

extern const char *ipa_clients_strings[];

#define IPA_IPC_LOGGING(buf, fmt, args...) \
	do { \
		if (buf) \
			ipc_log_string((buf), fmt, __func__, __LINE__, \
				## args); \
	} while (0)

void ipa_inc_client_enable_clks(struct ipa_active_client_logging_info *id);
void ipa_dec_client_disable_clks(struct ipa_active_client_logging_info *id);
int ipa_inc_client_enable_clks_no_block(
	struct ipa_active_client_logging_info *id);
int ipa_suspend_resource_no_block(enum ipa_rm_resource_name resource);
int ipa_resume_resource(enum ipa_rm_resource_name name);
int ipa_suspend_resource_sync(enum ipa_rm_resource_name resource);
int ipa_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
	u32 bandwidth_mbps);
void *ipa_get_ipc_logbuf(void);
void *ipa_get_ipc_logbuf_low(void);
void ipa_assert(void);

/* MHI */
int ipa_mhi_init_engine(struct ipa_mhi_init_engine *params);
int ipa_connect_mhi_pipe(struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl);
int ipa_disconnect_mhi_pipe(u32 clnt_hdl);
bool ipa_mhi_stop_gsi_channel(enum ipa_client_type client);
int ipa_qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req);
int ipa_qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req);
int ipa_generate_tag_process(void);
int ipa_disable_sps_pipe(enum ipa_client_type client);
int ipa_mhi_reset_channel_internal(enum ipa_client_type client);
int ipa_mhi_start_channel_internal(enum ipa_client_type client);
bool ipa_mhi_sps_channel_empty(enum ipa_client_type client);
int ipa_mhi_resume_channels_internal(enum ipa_client_type client,
		bool LPTransitionRejected, bool brstmode_enabled,
		union __packed gsi_channel_scratch ch_scratch, u8 index);
int ipa_mhi_handle_ipa_config_req(struct ipa_config_req_msg_v01 *config_req);
int ipa_mhi_query_ch_info(enum ipa_client_type client,
		struct gsi_chan_info *ch_info);
int ipa_mhi_destroy_channel(enum ipa_client_type client);
int ipa_mhi_is_using_dma(bool *flag);
const char *ipa_mhi_get_state_str(int state);

/* MHI uC */
int ipa_uc_mhi_send_dl_ul_sync_info(union IpaHwMhiDlUlSyncCmdData_t *cmd);
int ipa_uc_mhi_init
	(void (*ready_cb)(void), void (*wakeup_request_cb)(void));
void ipa_uc_mhi_cleanup(void);
int ipa_uc_mhi_reset_channel(int channelHandle);
int ipa_uc_mhi_suspend_channel(int channelHandle);
int ipa_uc_mhi_stop_event_update_channel(int channelHandle);
int ipa_uc_mhi_print_stats(char *dbg_buff, int size);

/* uC */
int ipa_uc_state_check(void);

/* general */
void ipa_get_holb(int ep_idx, struct ipa_ep_cfg_holb *holb);
void ipa_set_tag_process_before_gating(bool val);
bool ipa_has_open_aggr_frame(enum ipa_client_type client);
int ipa_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *in,
	ipa_notify_cb notify, void *priv, u8 hdr_len,
	struct ipa_ntn_conn_out_params *outp);

int ipa_tear_down_uc_offload_pipes(int ipa_ep_idx_ul, int ipa_ep_idx_dl,
	struct ipa_ntn_conn_in_params *params);
u8 *ipa_write_64(u64 w, u8 *dest);
u8 *ipa_write_32(u32 w, u8 *dest);
u8 *ipa_write_16(u16 hw, u8 *dest);
u8 *ipa_write_8(u8 b, u8 *dest);
u8 *ipa_pad_to_64(u8 *dest);
u8 *ipa_pad_to_32(u8 *dest);
int ipa_ntn_uc_reg_rdyCB(void (*ipauc_ready_cb)(void *user_data),
			      void *user_data);
void ipa_ntn_uc_dereg_rdyCB(void);

int ipa_conn_wdi_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out,
	ipa_wdi_meter_notifier_cb wdi_notify);

int ipa_disconn_wdi_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);

int ipa_enable_wdi_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);

int ipa_disable_wdi_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);

const char *ipa_get_version_string(enum ipa_hw_type ver);
int ipa_start_gsi_channel(u32 clnt_hdl);

bool ipa_pm_is_used(void);

int ipa_smmu_store_sgt(struct sg_table **out_ch_ptr,
		struct sg_table *in_sgt_ptr);
int ipa_smmu_free_sgt(struct sg_table **out_sgt_ptr);

int ipa_ut_module_init(void);
void ipa_ut_module_exit(void);

int ipa_wigig_internal_init(
	struct ipa_wdi_uc_ready_params *inout,
	ipa_wigig_misc_int_cb int_notify,
	phys_addr_t *uc_db_pa);

int ipa_conn_wigig_rx_pipe_i(void *in, struct ipa_wigig_conn_out_params *out,
	struct dentry **parent);

int ipa_conn_wigig_client_i(void *in, struct ipa_wigig_conn_out_params *out,
	ipa_notify_cb tx_notify,
	void *priv);

int ipa_wigig_uc_msi_init(
	bool init,
	phys_addr_t periph_baddr_pa,
	phys_addr_t pseudo_cause_pa,
	phys_addr_t int_gen_tx_pa,
	phys_addr_t int_gen_rx_pa,
	phys_addr_t dma_ep_misc_pa);

int ipa_disconn_wigig_pipe_i(enum ipa_client_type client,
	struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu,
	void *dbuff);

int ipa_enable_wigig_pipe_i(enum ipa_client_type client);

int ipa_disable_wigig_pipe_i(enum ipa_client_type client);

int ipa_wigig_send_msg(int msg_type,
	const char *netdev_name, u8 *mac,
	enum ipa_client_type client, bool to_wigig);

int ipa_wigig_save_regs(void);

void ipa_register_client_callback(int (*client_cb)(bool is_lock),
			bool (*teth_port_state)(void), u32 ipa_ep_idx);

void ipa_deregister_client_callback(u32 ipa_ep_idx);

#endif /* _IPA_COMMON_I_H_ */
