#ifndef _SMART_AMP_H_
#define _SMART_AMP_H_

#include <linux/types.h>
#include <sound/apr_audio-v2.h>
#include <linux/delay.h>

/* Below 3 should be same as in aDSP code */
#define AFE_PARAM_ID_SMARTAMP_DEFAULT	0x10001166
#define AFE_SMARTAMP_MODULE		0x0F010209
#define MAX_DSP_PARAM_INDEX		432

#define TAS_GET_PARAM			1
#define TAS_SET_PARAM			0
#define TAS_PAYLOAD_SIZE		14

#define SLAVE1		0x98
#define SLAVE2		0x9A
#define SLAVE3		0x9C
#define SLAVE4		0x9E

struct afe_smartamp_set_params_t {
	uint32_t payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_set_calib {
	struct apr_hdr                      hdr;
	struct afe_port_cmd_set_param_v2    param;
	struct afe_port_param_data_v2       pdata;
	struct afe_smartamp_set_params_t	prot_config;
} __packed;

struct afe_smartamp_get_params_t {
	uint32_t payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_get_calib {
	struct apr_hdr						hdr;
	struct afe_port_cmd_get_param_v2	get_param;
	struct afe_port_param_data_v2		pdata;
	struct afe_smartamp_get_params_t	res_cfg;
} __packed;

struct afe_smartamp_calib_get_resp {
	uint32_t status;
	struct afe_port_param_data_v2 pdata;
	struct afe_smartamp_get_params_t res_cfg;
} __packed;

int afe_smartamp_algo_ctrl(u8 *data, u32 param_id, u8 dir, u8 size, u8 slave_id);

#endif
