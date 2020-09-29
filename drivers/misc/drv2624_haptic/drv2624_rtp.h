#ifndef __DRV2624_RTP_H__
#define __DRV2624_RTP_H__

struct rtp_head {
	uint32_t magic_num;
	uint32_t ver;
	uint16_t effect_num;
	uint16_t reserve1;
	uint16_t reserve2;
	uint16_t reserve3;
};

struct idx_element {
	uint16_t offset;
	uint16_t longth;
};

struct instruction_t {
	char word;
	uint32_t value;
};

#endif
