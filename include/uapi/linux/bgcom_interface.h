#ifndef LINUX_BG_CHAR_H
#define LINUX_BG_CHAR_H
#define BGCOM_REG_READ  0
#define BGCOM_AHB_READ  1
#define BGCOM_AHB_WRITE 2
#define EXCHANGE_CODE 'V'

struct bg_ui_data {
	uint64_t  __user write;
	uint64_t  __user result;
	uint32_t  bg_address;
	uint32_t  cmd;
	uint32_t  num_of_words;
};

#define REG_READ \
	_IOWR(EXCHANGE_CODE, BGCOM_REG_READ, \
	struct bg_ui_data)
#define AHB_READ \
	_IOWR(EXCHANGE_CODE, BGCOM_AHB_READ, \
	struct bg_ui_data)
#define AHB_WRITE \
	_IOW(EXCHANGE_CODE, BGCOM_AHB_WRITE, \
	struct bg_ui_data)
#endif
