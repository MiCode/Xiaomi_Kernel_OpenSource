#ifndef __PARSE_RTP_H__
#define __PARSE_RTP_H__

#define RTP_BIN_NAME "drv2624.rtp"
#define RTP_HEAD_SIZE 16

#define MAX_RTP_FILE_SIZE (1024 * 80)
#define INDEX_NUM  512
#define INVALID 1
#define SUCCESS 0

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
	uint16_t length;
	uint16_t duration;
};

struct instruction_t {
	char word;
	uint32_t value;
};

int set_running_effect_id(int id);
int pop_running_effect_command(char *command, int *data);
//int drv2624_load_rtp(char *filename);
int rtp_parse(unsigned char *pData, unsigned int nSize);
#endif
