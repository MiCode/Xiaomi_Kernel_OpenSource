#pragma once

#include <linux/types.h>
#include <dsp/apr_audio-v2.h>
#include <mius/mius_data_io.h>
#include <linux/delay.h>

#define MIUS_SET_PARAMS_SIZE			114
#define MIUS_ULTRASOUND_MODULE_RX			0x1000A210
#define MIUS_ULTRASOUND_MODULE_TX			0x1000A211

#define MI_ULTRASOUND_OPCODE				0x0FF10208

/* This need to be updated for all platforms */
#define MIUS_PORT_ID				AFE_PORT_ID_TX_CODEC_DMA_TX_4

/** Sequence of MI Ultrasound module parameters */
struct afe_mi_ultrasound_set_params_t {
	uint32_t  payload[MIUS_SET_PARAMS_SIZE];
} __packed;

/** Sequence of MI Ultrasound module parameters */

/** Elliptic APR public  */

int32_t mi_ultrasound_apr_set_parameter(int32_t port_id, uint32_t param_id,
	u8 *user_params, int32_t length);

int32_t mius_process_apr_payload(uint32_t *payload);

int mius_notify_gain_change_msg(int component_id, int gaindb);

typedef struct afe_mi_ultrasound_state {
	atomic_t us_apr_state;
	void **ptr_apr;
	atomic_t *ptr_status;
	atomic_t *ptr_state;
	wait_queue_head_t *ptr_wait;
	int timeout_ms;
} afe_mi_ultrasound_state_t;

extern afe_mi_ultrasound_state_t mius_afe;
