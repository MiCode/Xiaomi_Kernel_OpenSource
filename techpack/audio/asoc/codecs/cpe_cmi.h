/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 */

#ifndef __CPE_CMI_H__
#define __CPE_CMI_H__

#include <linux/types.h>

#define CPE_AFE_PORT_1_TX 1
#define CPE_AFE_PORT_3_TX 3
#define CPE_AFE_PORT_ID_2_OUT 0x02
#define CMI_INBAND_MESSAGE_SIZE 127

/*
 * Multiple mad types can be supported at once.
 * these values can be OR'ed to form the set of
 * supported mad types
 */
#define MAD_TYPE_AUDIO (1 << 0)
#define MAD_TYPE_BEACON (1 << 1)
#define MAD_TYPE_ULTRASND (1 << 2)

/* Core service command opcodes */
#define CPE_CORE_SVC_CMD_SHARED_MEM_ALLOC	(0x3001)
#define CPE_CORE_SVC_CMDRSP_SHARED_MEM_ALLOC	(0x3002)
#define CPE_CORE_SVC_CMD_SHARED_MEM_DEALLOC	(0x3003)
#define CPE_CORE_SVC_CMD_DRAM_ACCESS_REQ	(0x3004)
#define CPE_CORE_SVC_EVENT_SYSTEM_BOOT		(0x3005)
/* core service command opcodes for WCD9335 */
#define CPE_CORE_SVC_CMD_CFG_CLK_PLAN		(0x3006)
#define CPE_CORE_SVC_CMD_CLK_FREQ_REQUEST	(0x3007)

#define CPE_BOOT_SUCCESS 0x00
#define CPE_BOOT_FAILED 0x01

#define CPE_CORE_VERSION_SYSTEM_BOOT_EVENT 0x01

/* LSM Service command opcodes */
#define CPE_LSM_SESSION_CMD_OPEN_TX		(0x2000)
#define CPE_LSM_SESSION_CMD_SET_PARAMS		(0x2001)
#define CPE_LSM_SESSION_CMD_REGISTER_SOUND_MODEL (0x2002)
#define CPE_LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL (0x2003)
#define CPE_LSM_SESSION_CMD_START		(0x2004)
#define CPE_LSM_SESSION_CMD_STOP		(0x2005)
#define CPE_LSM_SESSION_EVENT_DETECTION_STATUS_V2 (0x2006)
#define CPE_LSM_SESSION_CMD_CLOSE_TX		(0x2007)
#define CPE_LSM_SESSION_CMD_SHARED_MEM_ALLOC	(0x2008)
#define CPE_LSM_SESSION_CMDRSP_SHARED_MEM_ALLOC (0x2009)
#define CPE_LSM_SESSION_CMD_SHARED_MEM_DEALLOC	(0x200A)
#define CPE_LSM_SESSION_CMD_TX_BUFF_OUTPUT_CONFIG (0x200f)
#define CPE_LSM_SESSION_CMD_OPEN_TX_V2		(0x200D)
#define CPE_LSM_SESSION_CMD_SET_PARAMS_V2	(0x200E)

/* LSM Service module and param IDs */
#define CPE_LSM_MODULE_ID_VOICE_WAKEUP		(0x00012C00)
#define CPE_LSM_MODULE_ID_VOICE_WAKEUP_V2	(0x00012C0D)
#define CPE_LSM_MODULE_FRAMEWORK		(0x00012C0E)

#define CPE_LSM_PARAM_ID_ENDPOINT_DETECT_THRESHOLD (0x00012C01)
#define CPE_LSM_PARAM_ID_OPERATION_MODE		(0x00012C02)
#define CPE_LSM_PARAM_ID_GAIN			(0x00012C03)
#define CPE_LSM_PARAM_ID_CONNECT_TO_PORT	(0x00012C04)
#define CPE_LSM_PARAM_ID_MIN_CONFIDENCE_LEVELS	(0x00012C07)

/* LSM LAB command opcodes */
#define CPE_LSM_SESSION_CMD_EOB		0x0000200B
#define CPE_LSM_MODULE_ID_LAB		0x00012C08
/* used for enable/disable lab*/
#define CPE_LSM_PARAM_ID_LAB_ENABLE	0x00012C09
/* used for T in LAB config DSP internal buffer*/
#define CPE_LSM_PARAM_ID_LAB_CONFIG	0x00012C0A
#define CPE_LSM_PARAM_ID_REGISTER_SOUND_MODEL	(0x00012C14)
#define CPE_LSM_PARAM_ID_DEREGISTER_SOUND_MODEL	(0x00012C15)
#define CPE_LSM_PARAM_ID_MEDIA_FMT		(0x00012C1E)

/* AFE Service command opcodes */
#define CPE_AFE_PORT_CMD_START			(0x1001)
#define CPE_AFE_PORT_CMD_STOP			(0x1002)
#define CPE_AFE_PORT_CMD_SUSPEND		(0x1003)
#define CPE_AFE_PORT_CMD_RESUME			(0x1004)
#define CPE_AFE_PORT_CMD_SHARED_MEM_ALLOC	(0x1005)
#define CPE_AFE_PORT_CMDRSP_SHARED_MEM_ALLOC	(0x1006)
#define CPE_AFE_PORT_CMD_SHARED_MEM_DEALLOC	(0x1007)
#define CPE_AFE_PORT_CMD_GENERIC_CONFIG		(0x1008)
#define CPE_AFE_SVC_CMD_LAB_MODE		(0x1009)

/* AFE Service module and param IDs */
#define CPE_AFE_CMD_SET_PARAM			(0x1000)
#define CPE_AFE_MODULE_ID_SW_MAD		(0x0001022D)
#define CPE_AFE_PARAM_ID_SW_MAD_CFG		(0x0001022E)
#define CPE_AFE_PARAM_ID_SVM_MODEL		(0x0001022F)

#define CPE_AFE_MODULE_HW_MAD			(0x00010230)
#define CPE_AFE_PARAM_ID_HW_MAD_CTL		(0x00010232)
#define CPE_AFE_PARAM_ID_HW_MAD_CFG		(0x00010231)

#define CPE_AFE_MODULE_AUDIO_DEV_INTERFACE	(0x0001020C)
#define CPE_AFE_PARAM_ID_GENERIC_PORT_CONFIG	(0x00010253)

#define CPE_CMI_BASIC_RSP_OPCODE	(0x0001)
#define CPE_HDR_MAX_PLD_SIZE	(0x7F)

#define CMI_OBM_FLAG_IN_BAND	0
#define CMI_OBM_FLAG_OUT_BAND	1

#define CMI_SHMEM_ALLOC_FAILED 0xff

/*
 * Future Service ID's can be added one line
 * before the CMI_CPE_SERVICE_ID_MAX
 */
enum {
	CMI_CPE_SERVICE_ID_MIN = 0,
	CMI_CPE_CORE_SERVICE_ID,
	CMI_CPE_AFE_SERVICE_ID,
	CMI_CPE_LSM_SERVICE_ID,
	CMI_CPE_SERVICE_ID_MAX,
};

#define CPE_LSM_SESSION_ID_MAX 2

#define IS_VALID_SESSION_ID(s_id) \
	(s_id <= CPE_LSM_SESSION_ID_MAX)

#define IS_VALID_SERVICE_ID(s_id) \
	(s_id > CMI_CPE_SERVICE_ID_MIN && \
	 s_id < CMI_CPE_SERVICE_ID_MAX)

#define IS_VALID_PLD_SIZE(p_size) \
	(p_size <= CPE_HDR_MAX_PLD_SIZE)

#define CMI_HDR_SET_OPCODE(hdr, cmd) (hdr->opcode = cmd)


#define CMI_HDR_SET(hdr_info, mask, shift, value) \
		(hdr_info = (((hdr_info) & ~(mask)) | \
			((value << shift) & mask)))

#define SVC_ID_SHIFT 4
#define SVC_ID_MASK (0x07 << SVC_ID_SHIFT)

#define SESSION_ID_SHIFT 0
#define SESSION_ID_MASK (0x0F << SESSION_ID_SHIFT)

#define PAYLD_SIZE_SHIFT 0
#define PAYLD_SIZE_MASK (0x7F << PAYLD_SIZE_SHIFT)

#define OBM_FLAG_SHIFT 7
#define OBM_FLAG_MASK (1 << OBM_FLAG_SHIFT)

#define VERSION_SHIFT 7
#define VERSION_MASK (1 << VERSION_SHIFT)

#define CMI_HDR_SET_SERVICE(hdr, s_id) \
		CMI_HDR_SET(hdr->hdr_info, SVC_ID_MASK,\
			    SVC_ID_SHIFT, s_id)
#define CMI_HDR_GET_SERVICE(hdr) \
		((hdr->hdr_info >> SVC_ID_SHIFT) & \
			(SVC_ID_MASK >> SVC_ID_SHIFT))


#define CMI_HDR_SET_SESSION(hdr, s_id) \
		CMI_HDR_SET(hdr->hdr_info, SESSION_ID_MASK,\
			    SESSION_ID_SHIFT, s_id)

#define CMI_HDR_GET_SESSION_ID(hdr) \
		((hdr->hdr_info >> SESSION_ID_SHIFT) & \
			 (SESSION_ID_MASK >> SESSION_ID_SHIFT))

#define CMI_GET_HEADER(msg)	((struct cmi_hdr *)(msg))
#define CMI_GET_PAYLOAD(msg)	((void *)(CMI_GET_HEADER(msg) + 1))
#define CMI_GET_OPCODE(msg)	(CMI_GET_HEADER(msg)->opcode)

#define CMI_HDR_SET_VERSION(hdr, ver) \
		CMI_HDR_SET(hdr->hdr_info, VERSION_MASK, \
				VERSION_SHIFT, ver)

#define CMI_HDR_SET_PAYLOAD_SIZE(hdr, p_size) \
		CMI_HDR_SET(hdr->pld_info, PAYLD_SIZE_MASK, \
			    PAYLD_SIZE_SHIFT, p_size)

#define CMI_HDR_GET_PAYLOAD_SIZE(hdr) \
		((hdr->pld_info >> PAYLD_SIZE_SHIFT) & \
			(PAYLD_SIZE_MASK >> PAYLD_SIZE_SHIFT))

#define CMI_HDR_SET_OBM(hdr, obm_flag) \
		CMI_HDR_SET(hdr->pld_info, OBM_FLAG_MASK, \
			    OBM_FLAG_SHIFT, obm_flag)

#define CMI_HDR_GET_OBM_FLAG(hdr) \
	((hdr->pld_info >> OBM_FLAG_SHIFT) & \
		(OBM_FLAG_MASK >> OBM_FLAG_SHIFT))

struct cmi_hdr {
	/*
	 * bits 0:3 is session id
	 * bits 4:6 is service id
	 * bit 7 is the version flag
	 */
	u8 hdr_info;

	/*
	 * bits 0:6 is payload size in case of in-band message
	 * bits 0:6 is size (OBM message size)
	 * bit 7 is the OBM flag
	 */
	u8 pld_info;

	/* 16 bit command opcode */
	u16 opcode;
} __packed;

union cpe_addr {
	u64 msw_lsw;
	void *kvaddr;
} __packed;

struct cmi_obm {
	u32 version;
	u32 size;
	union cpe_addr data_ptr;
	u32 mem_handle;
} __packed;

struct cmi_obm_msg {
	struct cmi_hdr hdr;
	struct cmi_obm pld;
} __packed;

struct cmi_core_svc_event_system_boot {
	u8 status;
	u8 version;
	u16 sfr_buff_size;
	u32 sfr_buff_address;
} __packed;

struct cmi_core_svc_cmd_shared_mem_alloc {
	u32 size;
} __packed;

struct cmi_core_svc_cmdrsp_shared_mem_alloc {
	u32 addr;
} __packed;

struct cmi_core_svc_cmd_clk_freq_request {
	u32 clk_freq;
} __packed;

struct cmi_msg_transport {
	u32 size;
	u32 addr;
} __packed;

struct cmi_basic_rsp_result {
	u8 status;
} __packed;

struct cpe_lsm_cmd_open_tx {
	struct cmi_hdr	hdr;
	u16 app_id;
	u16 reserved;
	u32 sampling_rate;
} __packed;

struct cpe_lsm_cmd_open_tx_v2 {
	struct cmi_hdr hdr;
	u32 topology_id;
} __packed;

struct cpe_cmd_shmem_alloc {
	struct cmi_hdr hdr;
	u32 size;
} __packed;

struct cpe_cmdrsp_shmem_alloc {
	struct cmi_hdr hdr;
	u32 addr;
} __packed;

struct cpe_cmd_shmem_dealloc {
	struct cmi_hdr	hdr;
	u32 addr;
} __packed;

struct cpe_lsm_event_detect_v2 {
	struct cmi_hdr	hdr;
	u8 detection_status;
	u8 size;
	u8 payload[0];
} __packed;

struct cpe_lsm_psize_res {
	u16 param_size;
	u16 reserved;
} __packed;

union cpe_lsm_param_size {
	u32 param_size;
	struct cpe_lsm_psize_res sr;
} __packed;

struct cpe_param_data {
	u32 module_id;
	u32 param_id;
	union cpe_lsm_param_size p_size;
} __packed;

struct cpe_lsm_param_epd_thres {
	struct cmi_hdr hdr;
	struct cpe_param_data param;
	u32 minor_version;
	u32 epd_begin;
	u32 epd_end;
} __packed;

struct cpe_lsm_param_gain {
	struct cmi_hdr hdr;
	struct cpe_param_data param;
	u32 minor_version;
	u16 gain;
	u16 reserved;
} __packed;

struct cpe_afe_hw_mad_ctrl {
	struct cpe_param_data param;
	u32 minor_version;
	u16 mad_type;
	u16 mad_enable;
} __packed;

struct cpe_afe_port_cfg {
	struct cpe_param_data param;
	u32 minor_version;
	u16 bit_width;
	u16 num_channels;
	u32 sample_rate;
} __packed;

struct cpe_afe_cmd_port_cfg {
	struct cmi_hdr hdr;
	u8 bit_width;
	u8 num_channels;
	u16 buffer_size;
	u32 sample_rate;
} __packed;

struct cpe_afe_params {
	struct cmi_hdr hdr;
	struct cpe_afe_hw_mad_ctrl hw_mad_ctrl;
	struct cpe_afe_port_cfg port_cfg;
} __packed;

struct cpe_afe_svc_cmd_mode {
	struct cmi_hdr hdr;
	u8 mode;
} __packed;

struct cpe_lsm_param_opmode {
	struct cmi_hdr hdr;
	struct cpe_param_data param;
	u32 minor_version;
	u16 mode;
	u16 reserved;
} __packed;

struct cpe_lsm_param_connectport {
	struct cmi_hdr hdr;
	struct cpe_param_data param;
	u32 minor_version;
	u16 afe_port_id;
	u16 reserved;
} __packed;

/*
 * This cannot be sent to CPE as is,
 * need to append the conf_levels dynamically
 */
struct cpe_lsm_conf_level {
	struct cmi_hdr hdr;
	struct cpe_param_data param;
	u8 num_active_models;
} __packed;

struct cpe_lsm_output_format_cfg {
	struct cmi_hdr hdr;
	u8 format;
	u8 packing;
	u8 data_path_events;
} __packed;

struct cpe_lsm_lab_enable {
	struct cpe_param_data param;
	u16 enable;
	u16 reserved;
} __packed;

struct cpe_lsm_control_lab {
	struct cmi_hdr hdr;
	struct cpe_lsm_lab_enable lab_enable;
} __packed;

struct cpe_lsm_lab_config {
	struct cpe_param_data param;
	u32 minor_ver;
	u32 latency;
} __packed;

struct cpe_lsm_lab_latency_config {
	struct cmi_hdr hdr;
	struct cpe_lsm_lab_config latency_cfg;
} __packed;

struct cpe_lsm_media_fmt_param {
	struct cmi_hdr hdr;
	struct cpe_param_data param;
	u32 minor_version;
	u32 sample_rate;
	u16 num_channels;
	u16 bit_width;
} __packed;


#define CPE_PARAM_LSM_LAB_LATENCY_SIZE (\
				sizeof(struct cpe_lsm_lab_latency_config) - \
				sizeof(struct cmi_hdr))
#define PARAM_SIZE_LSM_LATENCY_SIZE (\
					sizeof(struct cpe_lsm_lab_config) - \
					sizeof(struct cpe_param_data))
#define CPE_PARAM_SIZE_LSM_LAB_CONTROL (\
				sizeof(struct cpe_lsm_control_lab) - \
				sizeof(struct cmi_hdr))
#define PARAM_SIZE_LSM_CONTROL_SIZE (sizeof(struct cpe_lsm_lab_enable) - \
					sizeof(struct cpe_param_data))
#define PARAM_SIZE_AFE_HW_MAD_CTRL (sizeof(struct cpe_afe_hw_mad_ctrl) - \
				sizeof(struct cpe_param_data))
#define PARAM_SIZE_AFE_PORT_CFG (sizeof(struct cpe_afe_port_cfg) - \
				 sizeof(struct cpe_param_data))
#define CPE_AFE_PARAM_PAYLOAD_SIZE (sizeof(struct cpe_afe_params) - \
				sizeof(struct cmi_hdr))

#define OPEN_CMD_PAYLOAD_SIZE (sizeof(struct cpe_lsm_cmd_open_tx) - \
			       sizeof(struct cmi_hdr))
#define OPEN_V2_CMD_PAYLOAD_SIZE (sizeof(struct cpe_lsm_cmd_open_tx_v2) - \
			       sizeof(struct cmi_hdr))
#define SHMEM_ALLOC_CMD_PLD_SIZE (sizeof(struct cpe_cmd_shmem_alloc) - \
				      sizeof(struct cmi_hdr))

#define SHMEM_DEALLOC_CMD_PLD_SIZE (sizeof(struct cpe_cmd_shmem_dealloc) - \
				      sizeof(struct cmi_hdr))
#define OUT_FMT_CFG_CMD_PAYLOAD_SIZE ( \
		sizeof(struct cpe_lsm_output_format_cfg) - \
		sizeof(struct cmi_hdr))

#define CPE_AFE_CMD_PORT_CFG_PAYLOAD_SIZE \
		(sizeof(struct cpe_afe_cmd_port_cfg) - \
		 sizeof(struct cmi_hdr))

#define CPE_AFE_CMD_MODE_PAYLOAD_SIZE \
		(sizeof(struct cpe_afe_svc_cmd_mode) - \
		 sizeof(struct cmi_hdr))
#define CPE_CMD_EPD_THRES_PLD_SIZE (sizeof(struct cpe_lsm_param_epd_thres) - \
				    sizeof(struct cmi_hdr))
#define CPE_EPD_THRES_PARAM_SIZE ((CPE_CMD_EPD_THRES_PLD_SIZE) - \
				  sizeof(struct cpe_param_data))
#define CPE_CMD_OPMODE_PLD_SIZE (sizeof(struct cpe_lsm_param_opmode) - \
				 sizeof(struct cmi_hdr))
#define CPE_OPMODE_PARAM_SIZE ((CPE_CMD_OPMODE_PLD_SIZE) -\
			       sizeof(struct cpe_param_data))
#define CPE_CMD_CONNECTPORT_PLD_SIZE \
	(sizeof(struct cpe_lsm_param_connectport) - \
	 sizeof(struct cmi_hdr))
#define CPE_CONNECTPORT_PARAM_SIZE ((CPE_CMD_CONNECTPORT_PLD_SIZE) - \
				    sizeof(struct cpe_param_data))
#define CPE_CMD_GAIN_PLD_SIZE (sizeof(struct cpe_lsm_param_gain) - \
			       sizeof(struct cmi_hdr))
#define CPE_GAIN_PARAM_SIZE ((CPE_CMD_GAIN_PLD_SIZE) - \
			     sizeof(struct cpe_param_data))
#define CPE_MEDIA_FMT_PLD_SIZE (sizeof(struct cpe_lsm_media_fmt_param) - \
				sizeof(struct cmi_hdr))
#define CPE_MEDIA_FMT_PARAM_SIZE ((CPE_MEDIA_FMT_PLD_SIZE) - \
				  sizeof(struct cpe_param_data))
#endif /* __CPE_CMI_H__ */
