#ifndef LINUX_BG_CHAR_H
#define LINUX_BG_CHAR_H
#define BGCOM_REG_READ  0
#define BGCOM_AHB_READ  1
#define BGCOM_AHB_WRITE 2
#define BGCOM_SET_SPI_FREE  3
#define BGCOM_SET_SPI_BUSY  4
#define BGCOM_REG_WRITE  5
#define BGCOM_SOFT_RESET  6
#define BGCOM_MODEM_DOWN2_BG  7
#define BGCOM_TWM_EXIT  8
#define EXCHANGE_CODE  'V'

struct bg_ui_data {
	uint64_t  __user write;
	uint64_t  __user result;
	uint32_t  bg_address;
	uint32_t  cmd;
	uint32_t  num_of_words;
};

enum bg_event_type {
	BG_BEFORE_POWER_DOWN = 1,
	BG_AFTER_POWER_UP,
	MODEM_BEFORE_POWER_DOWN,
	MODEM_AFTER_POWER_UP,
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
#define SET_SPI_FREE \
	_IOR(EXCHANGE_CODE, BGCOM_SET_SPI_FREE, \
	struct bg_ui_data)
#define SET_SPI_BUSY \
	_IOR(EXCHANGE_CODE, BGCOM_SET_SPI_BUSY, \
	struct bg_ui_data)
#define REG_WRITE \
	_IOWR(EXCHANGE_CODE, BGCOM_REG_WRITE, \
	struct bg_ui_data)
#define BG_SOFT_RESET \
	_IOWR(EXCHANGE_CODE, BGCOM_SOFT_RESET, \
	struct bg_ui_data)
#define BG_TWM_EXIT \
	_IOWR(EXCHANGE_CODE, BGCOM_TWM_EXIT, \
	struct bg_ui_data)
#define BG_MODEM_DOWN2_BG_DONE \
	_IOWR(EXCHANGE_CODE, BGCOM_MODEM_DOWN2_BG, \
	struct bg_ui_data)
#endif
