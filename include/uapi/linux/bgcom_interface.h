#ifndef LINUX_BG_CHAR_H

#include <linux/types.h>

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
#define BGCOM_BG_APP_RUNNING 9
#define BGCOM_ADSP_DOWN2_BG  10
#define BGCOM_BG_WEAR_LOAD 11
#define BGCOM_BG_WEAR_TWM_LOAD 12
#define BGCOM_BG_WEAR_UNLOAD 13
#define BGCOM_BG_FETCH_TWM_DATA 14
#define BGCOM_BG_READ_TWM_DATA 15
#define EXCHANGE_CODE  'V'

struct bg_ui_data {
	__u64  __user write;
	__u64  __user result;
	__u32  bg_address;
	__u32  cmd;
	__u32  num_of_words;
	__u8 __user *buffer;
};

enum bg_event_type {
	BG_BEFORE_POWER_DOWN = 1,
	BG_AFTER_POWER_DOWN,
	BG_BEFORE_POWER_UP,
	BG_AFTER_POWER_UP,
	MODEM_BEFORE_POWER_DOWN,
	MODEM_AFTER_POWER_UP,
	ADSP_BEFORE_POWER_DOWN,
	ADSP_AFTER_POWER_UP,
	TWM_BG_AFTER_POWER_UP,
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
#define BG_APP_RUNNING \
	_IOWR(EXCHANGE_CODE, BGCOM_BG_APP_RUNNING, \
	struct bg_ui_data)
#define BG_MODEM_DOWN2_BG_DONE \
	_IOWR(EXCHANGE_CODE, BGCOM_MODEM_DOWN2_BG, \
	struct bg_ui_data)
#define BG_WEAR_LOAD \
	_IOWR(EXCHANGE_CODE, BGCOM_BG_WEAR_LOAD, \
	struct bg_ui_data)
#define BG_WEAR_TWM_LOAD \
	_IOWR(EXCHANGE_CODE, BGCOM_BG_WEAR_TWM_LOAD, \
	struct bg_ui_data)
#define BG_WEAR_UNLOAD \
	_IOWR(EXCHANGE_CODE, BGCOM_BG_WEAR_UNLOAD, \
	struct bg_ui_data)
#define BG_ADSP_DOWN2_BG_DONE \
	_IOWR(EXCHANGE_CODE, BGCOM_ADSP_DOWN2_BG, \
	struct bg_ui_data)
#define BG_FETCH_TWM_DATA \
	_IOWR(EXCHANGE_CODE, BGCOM_BG_FETCH_TWM_DATA, \
	struct bg_ui_data)
#define BG_READ_TWM_DATA \
	_IOWR(EXCHANGE_CODE, BGCOM_BG_READ_TWM_DATA, \
	struct bg_ui_data)
#endif
