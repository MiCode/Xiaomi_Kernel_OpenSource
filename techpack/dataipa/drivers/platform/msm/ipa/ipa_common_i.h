/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_COMMON_I_H_
#define _IPA_COMMON_I_H_
#include <linux/ipa_mhi.h>
#include <linux/ipa_qmi_service_v01.h>
#include <linux/errno.h>
#include <linux/ipc_logging.h>
#include <linux/ipa.h>
#include <linux/ipa_uc_offload.h>
#include <linux/ipa_wdi3.h>
#include <linux/ipa_wigig.h>
#include <linux/ratelimit.h>
#include "gsi.h"

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
		ipa3_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_EP(client) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client); \
		ipa3_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_SIMPLE() \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info); \
		ipa3_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_SIMPLE() \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info); \
		ipa3_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_RESOURCE(resource_name) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name); \
		ipa3_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_RESOURCE(resource_name) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name); \
		ipa3_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_SPECIAL(id_str) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str); \
		ipa3_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_SPECIAL(id_str) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str); \
		ipa3_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_EP_NO_BLOCK(client) ({\
	int __ret = 0; \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client); \
		__ret = ipa3_inc_client_enable_clks_no_block(&log_info); \
	} while (0); \
	(__ret); \
})

#define IPA_ACTIVE_CLIENTS_DEC_EP_NO_BLOCK(client) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client); \
		ipa3_dec_client_disable_clks_no_block(&log_info); \
	} while (0)

/*
 * Printing one warning message in 5 seconds if multiple warning messages
 * are coming back to back.
 */
#ifdef CONFIG_IPA_DEBUG
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
#else
#define WARN_ON_RATELIMIT_IPA(condition)
#endif

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

enum ipa_mhi_state {
	IPA_MHI_STATE_INITIALIZED,
	IPA_MHI_STATE_READY,
	IPA_MHI_STATE_STARTED,
	IPA_MHI_STATE_SUSPEND_IN_PROGRESS,
	IPA_MHI_STATE_SUSPENDED,
	IPA_MHI_STATE_RESUME_IN_PROGRESS,
	IPA_MHI_STATE_MAX
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

/**
 * enum teth_tethering_mode - Tethering mode (Rmnet / MBIM)
 */
enum teth_tethering_mode {
	TETH_TETHERING_MODE_RMNET,
	TETH_TETHERING_MODE_MBIM,
	TETH_TETHERING_MODE_MAX,
};

/**
 * teth_bridge_init_params - Parameters used for in/out USB API
 * @usb_notify_cb:	Callback function which should be used by the caller.
 * Output parameter.
 * @private_data:	Data for the callback function. Should be used by the
 * caller. Output parameter.
 * @skip_ep_cfg: boolean field that determines if Apps-processor
 *  should or should not confiugre this end-point.
 */
struct teth_bridge_init_params {
	ipa_notify_cb usb_notify_cb;
	void *private_data;
	enum ipa_client_type client;
	bool skip_ep_cfg;
};

/**
 * struct teth_bridge_connect_params - Parameters used in teth_bridge_connect()
 * @ipa_usb_pipe_hdl:	IPA to USB pipe handle, returned from ipa_connect()
 * @usb_ipa_pipe_hdl:	USB to IPA pipe handle, returned from ipa_connect()
 * @tethering_mode:	Rmnet or MBIM
 * @ipa_client_type:    IPA "client" name (IPA_CLIENT_USB#_PROD)
 */
struct teth_bridge_connect_params {
	u32 ipa_usb_pipe_hdl;
	u32 usb_ipa_pipe_hdl;
	enum teth_tethering_mode tethering_mode;
	enum ipa_client_type client_type;
};

/**
 * struct IpaOffloadStatschannel_info - channel info for uC
 * stats
 * @dir: Direction of the channel ID DIR_CONSUMER =0,
 * DIR_PRODUCER = 1
 * @ch_id: GSI ch_id of the IPA endpoint for which stats need
 * to be calculated, 0xFF means invalid channel or disable stats
 * on already stats enabled channel
 */
struct IpaOffloadStatschannel_info {
	u8 dir;
	u8 ch_id;
} __packed;

/**
 * struct IpaHwOffloadStatsAllocCmdData_t - protocol info for uC
 * stats start
 * @protocol: Enum that indicates the protocol type
 * @ch_id_info: GSI ch_id and dir of the IPA endpoint for which stats
 * need to be calculated
 */
struct IpaHwOffloadStatsAllocCmdData_t {
	u32 protocol;
	struct IpaOffloadStatschannel_info
		ch_id_info[IPA_MAX_CH_STATS_SUPPORTED];
} __packed;

/**
 * struct ipa_uc_dbg_ring_stats - uC dbg stats info for each
 * offloading protocol
 * @ring: ring stats for each channel
 * @ch_num: number of ch supported for given protocol
 */
struct ipa_uc_dbg_ring_stats {
	struct IpaHwRingStats_t ring[IPA_MAX_CH_STATS_SUPPORTED];
	u8 num_ch;
};

/**
 * struct ipa_tz_unlock_reg_info - Used in order unlock regions of memory by TZ
 * @reg_addr - Physical address of the start of the region
 * @size - Size of the region in bytes
 */
struct ipa_tz_unlock_reg_info {
	u64 reg_addr;
	u64 size;
};

/**
 * struct ipa_tx_suspend_irq_data - interrupt data for IPA_TX_SUSPEND_IRQ
 * @endpoints: bitmask of endpoints which case IPA_TX_SUSPEND_IRQ interrupt
 * @dma_addr: DMA address of this Rx packet
 */
struct ipa_tx_suspend_irq_data {
	u32 endpoints;
};

extern const char *ipa_clients_strings[];

#define IPA_IPC_LOGGING(buf, fmt, args...) \
	do { \
		if (buf) \
			ipc_log_string((buf), fmt, __func__, __LINE__, \
				## args); \
	} while (0)

void ipa3_inc_client_enable_clks(struct ipa_active_client_logging_info *id);
void ipa3_dec_client_disable_clks(struct ipa_active_client_logging_info *id);
int ipa3_inc_client_enable_clks_no_block(
	struct ipa_active_client_logging_info *id);
int ipa3_suspend_resource_no_block(enum ipa_rm_resource_name resource);
int ipa3_resume_resource(enum ipa_rm_resource_name name);
int ipa3_suspend_resource_sync(enum ipa_rm_resource_name resource);
int ipa3_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
	u32 bandwidth_mbps);
void *ipa3_get_ipc_logbuf(void);
void *ipa3_get_ipc_logbuf_low(void);
void ipa_assert(void);

/* MHI */
int ipa3_mhi_init_engine(struct ipa_mhi_init_engine *params);
int ipa3_connect_mhi_pipe(struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl);
int ipa3_disconnect_mhi_pipe(u32 clnt_hdl);
bool ipa3_mhi_stop_gsi_channel(enum ipa_client_type client);
int ipa3_generate_tag_process(void);
int ipa3_disable_sps_pipe(enum ipa_client_type client);
int ipa3_mhi_reset_channel_internal(enum ipa_client_type client);
int ipa3_mhi_start_channel_internal(enum ipa_client_type client);
bool ipa3_mhi_sps_channel_empty(enum ipa_client_type client);
int ipa3_mhi_resume_channels_internal(enum ipa_client_type client,
		bool LPTransitionRejected, bool brstmode_enabled,
		union __packed gsi_channel_scratch ch_scratch, u8 index);
int ipa3_mhi_query_ch_info(enum ipa_client_type client,
		struct gsi_chan_info *ch_info);
int ipa3_mhi_destroy_channel(enum ipa_client_type client);
int ipa_mhi_is_using_dma(bool *flag);

/* MHI uC */
int ipa3_uc_mhi_send_dl_ul_sync_info(union IpaHwMhiDlUlSyncCmdData_t *cmd);
int ipa3_uc_mhi_init
	(void (*ready_cb)(void), void (*wakeup_request_cb)(void));
void ipa3_uc_mhi_cleanup(void);
int ipa3_uc_mhi_reset_channel(int channelHandle);
int ipa3_uc_mhi_suspend_channel(int channelHandle);
int ipa3_uc_mhi_stop_event_update_channel(int channelHandle);
int ipa3_uc_mhi_print_stats(char *dbg_buff, int size);

/* uC */
int ipa3_uc_state_check(void);

/* general */
void ipa3_get_holb(int ep_idx, struct ipa_ep_cfg_holb *holb);
void ipa3_set_tag_process_before_gating(bool val);
bool ipa3_has_open_aggr_frame(enum ipa_client_type client);
int ipa3_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *in,
	ipa_notify_cb notify, void *priv, u8 hdr_len,
	struct ipa_ntn_conn_out_params *outp);

int ipa3_tear_down_uc_offload_pipes(int ipa_ep_idx_ul, int ipa_ep_idx_dl,
	struct ipa_ntn_conn_in_params *params);
u8 *ipa_write_64(u64 w, u8 *dest);
u8 *ipa_write_32(u32 w, u8 *dest);
u8 *ipa_write_16(u16 hw, u8 *dest);
u8 *ipa_write_8(u8 b, u8 *dest);
u8 *ipa_pad_to_64(u8 *dest);
u8 *ipa_pad_to_32(u8 *dest);
int ipa3_ntn_uc_reg_rdyCB(void (*ipauc_ready_cb)(void *user_data),
			      void *user_data);
void ipa3_ntn_uc_dereg_rdyCB(void);

int ipa3_conn_wdi3_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out,
	ipa_wdi_meter_notifier_cb wdi_notify);

int ipa3_disconn_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);

int ipa3_enable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);

int ipa3_disable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);

const char *ipa_get_version_string(enum ipa_hw_type ver);
int ipa3_start_gsi_channel(u32 clnt_hdl);

int ipa_smmu_store_sgt(struct sg_table **out_ch_ptr,
		struct sg_table *in_sgt_ptr);
int ipa_smmu_free_sgt(struct sg_table **out_sgt_ptr);

#ifdef CONFIG_IPA_UT
int ipa_ut_module_init(void);
void ipa_ut_module_exit(void);
#else
static inline int ipa_ut_module_init(void)
{
	return -EPERM;
}
static inline void ipa_ut_module_exit(void)
{
}
#endif

int ipa3_wigig_internal_init(
	struct ipa_wdi_uc_ready_params *inout,
	ipa_wigig_misc_int_cb int_notify,
	phys_addr_t *uc_db_pa);

int ipa3_conn_wigig_rx_pipe_i(void *in, struct ipa_wigig_conn_out_params *out,
	struct dentry **parent);

int ipa3_conn_wigig_client_i(void *in, struct ipa_wigig_conn_out_params *out,
	ipa_notify_cb tx_notify,
	void *priv);

int ipa3_wigig_uc_msi_init(
	bool init,
	phys_addr_t periph_baddr_pa,
	phys_addr_t pseudo_cause_pa,
	phys_addr_t int_gen_tx_pa,
	phys_addr_t int_gen_rx_pa,
	phys_addr_t dma_ep_misc_pa);

int ipa3_disconn_wigig_pipe_i(enum ipa_client_type client,
	struct ipa_wigig_pipe_setup_info_smmu *pipe_smmu,
	void *dbuff);

int ipa3_enable_wigig_pipe_i(enum ipa_client_type client);

int ipa3_disable_wigig_pipe_i(enum ipa_client_type client);

int ipa_wigig_send_msg(int msg_type,
	const char *netdev_name, u8 *mac,
	enum ipa_client_type client, bool to_wigig);

int ipa_wigig_send_wlan_msg(enum ipa_wlan_event msg_type,
	const char *netdev_name, u8 *mac);

void ipa3_register_client_callback(int (*client_cb)(bool is_lock),
			bool (*teth_port_state)(void), u32 ipa_ep_idx);

void ipa3_deregister_client_callback(u32 ipa_ep_idx);

/*
* Configuration
*/
int ipa3_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg);

int ipa3_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ipa_ep_cfg);

int ipa3_cfg_ep_conn_track(u32 clnt_hdl,
	const struct ipa_ep_cfg_conn_track *ep_conn_track);

int ipa_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ipa_ep_cfg);

int ipa_cfg_ep_hdr_ext(u32 clnt_hdl,
	const struct ipa_ep_cfg_hdr_ext *ipa_ep_cfg);

int ipa_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ipa_ep_cfg);

int ipa_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ipa_ep_cfg);

int ipa_cfg_ep_deaggr(u32 clnt_hdl,
	const struct ipa_ep_cfg_deaggr *ipa_ep_cfg);

int ipa_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ipa_ep_cfg);

int ipa_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *ipa_ep_cfg);

int ipa_cfg_ep_metadata_mask(u32 clnt_hdl, const struct ipa_ep_cfg_metadata_mask
	*ipa_ep_cfg);

int ipa_cfg_ep_holb_by_client(enum ipa_client_type client,
	const struct ipa_ep_cfg_holb *ipa_ep_cfg);

/*
* Header removal / addition
*/
int ipa3_add_hdr(struct ipa_ioc_add_hdr *hdrs);

int ipa3_del_hdr(struct ipa_ioc_del_hdr *hdls);

int ipa3_add_hdr_usr(struct ipa_ioc_add_hdr *hdrs, bool user_only);

int ipa3_reset_hdr(bool user_only);

/*
* Header Processing Context
*/
int ipa3_add_hdr_proc_ctx(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs,
	bool user_only);

int ipa3_del_hdr_proc_ctx(struct ipa_ioc_del_hdr_proc_ctx *hdls);

/*
* Routing
*/

int ipa3_add_rt_rule_v2(struct ipa_ioc_add_rt_rule_v2 *rules);

int ipa3_add_rt_rule_usr(struct ipa_ioc_add_rt_rule *rules, bool user_only);

int ipa3_add_rt_rule_usr_v2(struct ipa_ioc_add_rt_rule_v2 *rules,
	bool user_only);

int ipa3_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls);

int ipa3_commit_rt(enum ipa_ip_type ip);

int ipa3_reset_rt(enum ipa_ip_type ip, bool user_only);

/*
* Filtering
*/

int ipa3_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls);

int ipa3_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *rules);

int ipa3_mdfy_flt_rule_v2(struct ipa_ioc_mdfy_flt_rule_v2 *rules);

int ipa3_reset_flt(enum ipa_ip_type ip, bool user_only);

/*
* NAT\IPv6CT
*/
int ipa3_allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem);
int ipa3_allocate_nat_table(struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc);
int ipa3_allocate_ipv6ct_table(
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc);

int ipa3_nat_init_cmd(struct ipa_ioc_v4_nat_init *init);
int ipa3_ipv6ct_init_cmd(struct ipa_ioc_ipv6ct_init *init);

int ipa3_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma);
int ipa3_table_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma);

int ipa3_nat_del_cmd(struct ipa_ioc_v4_nat_del *del);
int ipa3_del_nat_table(struct ipa_ioc_nat_ipv6ct_table_del *del);
int ipa3_del_ipv6ct_table(struct ipa_ioc_nat_ipv6ct_table_del *del);

int ipa3_nat_mdfy_pdn(struct ipa_ioc_nat_pdn_entry *mdfy_pdn);

/*
* Data path
*/
int ipa3_rx_poll(u32 clnt_hdl, int budget);
void ipa3_recycle_wan_skb(struct sk_buff *skb);

/*
* System pipes
*/
int ipa3_set_wlan_tx_info(struct ipa_wdi_tx_info *info);

/*
* Tethering bridge (Rmnet / MBIM)
*/
int ipa3_teth_bridge_init(struct teth_bridge_init_params *params);

int ipa3_teth_bridge_disconnect(enum ipa_client_type client);

int ipa3_teth_bridge_connect(struct teth_bridge_connect_params *connect_params);

/*
* Tethering client info
*/
void ipa3_set_client(int index, enum ipacm_client_enum client, bool uplink);

enum ipacm_client_enum ipa3_get_client(int pipe_idx);

bool ipa3_get_client_uplink(int pipe_idx);

/*
* mux id
*/
int ipa3_write_qmap_id(struct ipa_ioc_write_qmapid *param_in);

/*
* interrupts
*/

int ipa3_remove_interrupt_handler(enum ipa_irq_type interrupt);

/*
* Interface
*/
int ipa3_register_intf(const char *name, const struct ipa_tx_intf *tx,
	const struct ipa_rx_intf *rx);
int ipa3_register_intf_ext(const char *name, const struct ipa_tx_intf *tx,
	const struct ipa_rx_intf *rx,
	const struct ipa_ext_intf *ext);
int ipa3_deregister_intf(const char *name);

/*
* Miscellaneous
*/

int ipa3_uc_debug_stats_alloc(
	struct IpaHwOffloadStatsAllocCmdData_t cmdinfo);
int ipa3_uc_debug_stats_dealloc(uint32_t protocol);
void ipa3_get_gsi_stats(int prot_id,
	struct ipa_uc_dbg_ring_stats *stats);
int ipa3_get_prot_id(enum ipa_client_type client);
bool ipa_is_client_handle_valid(u32 clnt_hdl);
int ipa3_get_smmu_params(struct ipa_smmu_in_params *in,
	struct ipa_smmu_out_params *out);

/**
* ipa_tz_unlock_reg - Unlocks memory regions so that they become accessible
*	from AP.
* @reg_info - Pointer to array of memory regions to unlock
* @num_regs - Number of elements in the array
*
* Converts the input array of regions to a struct that TZ understands and
* issues an SCM call.
* Also flushes the memory cache to DDR in order to make sure that TZ sees the
* correct data structure.
*
* Returns: 0 on success, negative on failure
*/
int ipa3_tz_unlock_reg(struct ipa_tz_unlock_reg_info *reg_info, u16 num_regs);

#endif /* _IPA_COMMON_I_H_ */
