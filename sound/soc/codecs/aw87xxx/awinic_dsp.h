#ifndef __AWINIC_DSP_H__
#define __AWINIC_DSP_H__

/*#define AW_MTK_OPEN_DSP_PLATFORM*/

/**********************************************************
 * aw87xxx dsp
***********************************************************/
#define AWINIC_DSP_MSG_HDR_VER (1)

/*dsp params id*/
#define AFE_PARAM_ID_AWDSP_RX_VMAX_L			(0X10013D17)
#define AFE_PARAM_ID_AWDSP_RX_VMAX_R			(0X10013D18)

enum aw_channel {
	AW_CHANNEL_LEFT = 0,
	AW_CHANNEL_RIGHT = 1,
};


enum aw_dsp_msg_type {
	DSP_MSG_TYPE_DATA = 0,
	DSP_MSG_TYPE_CMD = 1,
};

struct aw_dsp_msg_hdr {
	int32_t type;
	int32_t opcode_id;
	int32_t version;
	int32_t reserver[3];
};

bool aw87xx_platform_init(void);
int aw_get_vmax_from_dsp(uint32_t *vmax, int32_t channel);
int aw_set_vmax_to_dsp(uint32_t vmax, int32_t channel);

#endif
