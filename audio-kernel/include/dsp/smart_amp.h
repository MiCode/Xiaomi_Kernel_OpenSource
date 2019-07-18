#ifndef _SMART_AMP_H
#define _SMART_AMP_H

#include <linux/types.h>
#include <dsp/apr_audio-v2.h>
#include <sound/soc.h>


/* Below 3 should be same as in aDSP code */
#define AFE_PARAM_ID_SMARTAMP_DEFAULT   0x10001166
#define AFE_SMARTAMP_MODULE_RX          0x11111112  /*Rx module*/
#define AFE_SMARTAMP_MODULE_TX          0x11111111  /*Tx module*/

#define CAPI_V2_TAS_TX_ENABLE 		0x10012D14
#define CAPI_V2_TAS_TX_CFG    		0x10012D16
#define CAPI_V2_TAS_RX_ENABLE 		0x10012D13
#define CAPI_V2_TAS_RX_CFG    		0x10012D15

#define MAX_DSP_PARAM_INDEX		600

#define TAS_PAYLOAD_SIZE 	14
#define TAS_GET_PARAM		1
#define TAS_SET_PARAM		0

#define TAS_RX_PORT		MI2S_RX
#define TAS_TX_PORT		MI2S_TX

#define CHANNEL0	1
#define CHANNEL1	2

#define TRUE		1
#define FALSE		0

#define TAS_SA_GET_F0          3810
#define TAS_SA_GET_Q           3811
#define TAS_SA_GET_TV          3812
#define TAS_SA_GET_RE          3813
#define TAS_SA_CALIB_INIT      3814
#define TAS_SA_CALIB_DEINIT    3815
#define TAS_SA_SET_RE          3816
#define TAS_SA_SET_PROFILE     3819
#define TAS_SA_GET_STATUS      3821
#define TAS_SA_SET_SPKID       3822
#define TAS_SA_SET_TCAL        3823

#define PROFILE_COUNT	5
#define CALIB_COUNT     5
#define STATUS_COUNT   2

#define CALIB_START		1
#define CALIB_STOP		2
#define TEST_START		3
#define TEST_STOP		4

#define SLAVE1		0x98
#define SLAVE2		0x9A
#define SLAVE3		0x9C
#define SLAVE4		0x9E

#define TAS_SA_IS_SPL_IDX(X)	((((X) >= 3810) && ((X) < 3899)) ? 1 : 0)
#define TAS_CALC_PARAM_IDX(INDEX, LENGTH, CHANNEL)		((INDEX ) | (LENGTH << 16) | (CHANNEL << 24))

/*Random Numbers is to handle global data corruption*/
#define TAS_FALSE 	0x01010101
#define TAS_TRUE 	0x10101010
	
struct afe_smartamp_set_params_t {
	uint32_t  payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_get_params_t {
    uint32_t payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_config_command {
        struct apr_hdr                      hdr;
        struct afe_port_cmd_set_param_v2    param;
        struct afe_port_param_data_v2       pdata;
        struct afe_smartamp_set_params_t  prot_config;
} __packed;

struct afe_smartamp_get_calib {
	struct apr_hdr hdr;
	//struct mem_mapping_hdr mem_hdr;
	struct afe_port_cmd_get_param_v2 get_param;
	struct afe_port_param_data_v2 pdata;
	struct afe_smartamp_get_params_t res_cfg;
} __packed;

struct afe_smartamp_calib_get_resp {
	uint32_t status;
	struct afe_port_param_data_v2 pdata;
	struct afe_smartamp_get_params_t res_cfg;
} __packed;

void msm_smartamp_add_controls(struct snd_soc_platform *platform);

int afe_smartamp_get_calib_data(struct afe_smartamp_get_calib *calib_resp,
		uint32_t param_id, uint32_t module_id);

int afe_smartamp_set_calib_data(uint32_t param_id,struct afe_smartamp_set_params_t *prot_config,
		uint8_t length, uint32_t module_id);

int afe_smartamp_algo_ctrl(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id);

#endif
