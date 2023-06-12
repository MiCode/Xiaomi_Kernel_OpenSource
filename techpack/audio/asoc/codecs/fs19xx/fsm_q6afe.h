/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-08-20 File created.
 */

#ifndef __FSM_AFE_H__
#define __FSM_AFE_H__

//#define CONFIG_QCOM_Q6AFE_OLD
#define FSM_PARAM_HDR_V3 // TODO
#define FSM_RUNIN_TEST

// PRIMARY/SECONDARY/TERTIARY/QUATERNARY/QUINARY/SENARY
#define FSM_RX_PORT (AFE_PORT_ID_SECONDARY_MI2S_RX) // TODO
#define FSM_TX_PORT (AFE_PORT_ID_SECONDARY_MI2S_TX) // TODO

#include "fsm-dev.h"
#include <linux/firmware.h>
#ifdef CONFIG_QCOM_Q6AFE_OLD
#include <linux/msm_audio_ion.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6audio-v2.h>
#else
#include <dsp/msm_audio_ion.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6audio-v2.h>
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

#ifndef FSM_PARAM_HDR_V3
#define param_hdr_v1 afe_port_param_data_v2
#define param_hdr_v3 afe_port_param_data_v2
union param_hdrs {
	struct param_hdr_v3 hdr;
};

struct mem_mapping_hdr {
	/*
	 * LSW of parameter data payload address. Supported values: any.
	 * - Must be set to zero for in-band data.
	 */
	u32 data_payload_addr_lsw;

	/*
	 * MSW of Parameter data payload address. Supported values: any.
	 * - Must be set to zero for in-band data.
	 * - In the case of 32 bit Shared memory address, MSW field must be
	 *   set to zero.
	 * - In the case of 36 bit shared memory address, bit 31 to bit 4 of
	 *   MSW must be set to zero.
	 */
	u32 data_payload_addr_msw;

	/*
	 * Memory map handle returned by DSP through
	 * ASM_CMD_SHARED_MEM_MAP_REGIONS command.
	 * Supported Values: Any.
	 * If memory map handle is NULL, the parameter data payloads are
	 * within the message payload (in-band).
	 * If memory map handle is non-NULL, the parameter data payloads begin
	 * at the address specified in the address MSW and LSW (out-of-band).
	 */
	u32 mem_map_handle;

} __packed;

/* Payload of the #AFE_PORT_CMD_GET_PARAM_V2 command,
 * which queries for one post/preprocessing parameter of a
 * stream.
 */
struct afe_port_fsm_get_param_v2 {
	struct apr_hdr apr_hdr;

	/* Port interface and direction (Rx or Tx) to start. */
	u16 port_id;

	/* Maximum data size of the parameter ID/module ID combination.
	 * This is a multiple of four bytes
	 * Supported values: > 0
	 */
	u16 payload_size;

	/* The memory mapping header to be used when requesting outband */
	struct mem_mapping_hdr mem_hdr;

	/* The module ID of the parameter data requested */
	u32 module_id;

	/* The parameter ID of the parameter data requested */
	u32 param_id;

	/* The header information for the parameter data */
	struct param_hdr_v1 param_hdr;
} __packed;

struct afe_port_fsm_set_param_v2 {
	/* APR Header */
	struct apr_hdr apr_hdr;

	/* Port interface and direction (Rx or Tx) to start. */
	u16 port_id;

	/*
	 * Actual size of the payload in bytes.
	 * This is used for parsing the parameter payload.
	 * Supported values: > 0
	 */
	u16 payload_size;

	/* The header detailing the memory mapping for out of band. */
	struct mem_mapping_hdr mem_hdr;

	/* The parameter data to be filled when sent inband */
	u8 param_data[0];
} __packed;
#endif

// FSADSP MODULE ID
#define AFE_MODULE_ID_FSADSP_TX (0x10001110)
#define AFE_MODULE_ID_FSADSP_RX (0x10001111)

// FSADSP PARAM ID
#define CAPI_V2_PARAM_FSADSP_TX_ENABLE     0x11111601
#define CAPI_V2_PARAM_FSADSP_RX_ENABLE     0x11111611
#define CAPI_V2_PARAM_FSADSP_BASE          0x10001FA0
#define CAPI_V2_PARAM_FSADSP_MODULE_ENABLE 0x10001FA1 // enable rx or tx module
#define CAPI_V2_PARAM_FSADSP_MODULE_PARAM  0x10001FA2 // send preset to adsp
#define CAPI_V2_PARAM_FSADSP_APP_MODE      0x10001FA3 // set rx effect mode(scene)
#define CAPI_V2_PARAM_FSADSP_CLIPPING_EST  0x10001FA4 // 
#define CAPI_V2_PARAM_FSADSP_TX_FMT        0x10001FA5 // tx format
#define CAPI_V2_PARAM_FSADSP_LIVEDATA      0x10001FA6 // livedata
#define CAPI_V2_PARAM_FSADSP_RE25          0x10001FA7 // re25
#define CAPI_V2_PARAM_FSADSP_CFG           0x10001FA8 // configuration
#define CAPI_V2_PARAM_FSADSP_VBAT          0x10001FA9 // Vbat
#define CAPI_V2_PARAM_FSADSP_VER           0x10001FAA // Version
#define CAPI_V2_PARAM_FSADSP_CALIB         0x10001FAB // Calibration
#define CAPI_V2_PARAM_FSADSP_ADPMODE       0x10001FAC // adp mode for spc1915
#define CAPI_V2_PARAM_FSADSP_SETTING       0x10001FAF
#define CAPI_V2_PARAM_FSADSP_PARAM_ACDB    0x10001FB0

#define APR_CHUNK_SIZE (256)
#define FSM_CALIB_PAYLOAD_SIZE (32)

enum fsm_testcase {
	FSM_TC_ENABLE_ALL = 0,
	FSM_TC_DISABLE_ALL = 1,
	FSM_TC_DISABLE_EQ = 2,
	FSM_TC_DISABLE_PROT = 3,
	FSM_TC_MAX,
};

struct fsm_resp_params {
	uint32_t size;
	uint32_t *params;
};

#define FSADSP_RE25_CMD_VERSION_V1 0xA001
struct fsadsp_cal_data {
	uint16_t rstrim;
	uint16_t channel;
	int re25;
} __packed;

struct fsadsp_cmd_re25 {
	uint16_t version;
	uint16_t ndev;
	struct fsadsp_cal_data cal_data[FSM_DEV_MAX];
} __packed;

struct fsm_afe {
	int module_id;
	int port_id;
	int param_id;
	bool op_set;
	int param_size;
	struct mem_mapping_hdr mem_hdr;
	struct param_hdr_v3 param_hdr;
	struct rtac_cal_block_data *cal_block;
};
typedef struct fsm_afe fsm_afe_t;

int fsm_afe_get_rx_port(void);
int fsm_afe_get_tx_port(void);
int fsm_afe_mod_ctrl(bool enable);
void fsm_reset_re25_data(void);
int fsm_set_re25_data(struct fsm_re25_data *data);
int fsm_afe_read_re25(uint32_t *re25, int count);
int fsm_afe_save_re25(struct fsadsp_cmd_re25 *cmd_re25);
int fsm_afe_get_livedata(void *ldata, int size);
int fsm_afe_send_apr(struct fsm_afe *afe, void *buf, uint32_t length);

void fsm_afe_set_callback(int32_t (*callback_func)(
		int opcode, void *payload, int size));
int afe_map_rtac_block(struct rtac_cal_block_data *cal_block);
int afe_unmap_rtac_block(uint32_t *mem_map_handle);
int afe_spk_prot_feed_back_cfg(int src_port, int dst_port,
	int l_ch, int r_ch, u32 enable);
int q6afe_send_fsm_pkt(struct fsm_afe *afe, uint8_t *param, uint32_t param_size);

#endif // __FSM_AFE_H__
