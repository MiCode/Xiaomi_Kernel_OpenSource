#ifndef __AW87XXX_H__
#define __AW87XXX_H__

#include "aw87xxx_monitor.h"

#define AWINIC_CFG_UPDATE_DELAY
#define AW_I2C_RETRIES			(5)
#define AW_I2C_RETRY_DELAY		(2)

#define AW_READ_CHIPID_RETRIES		(5)
#define AW_READ_CHIPID_RETRY_DELAY	(2)

#define AW87XXX_CFG_NAME_MAX		(64)

#define AW87XXX_LOAD_CFG_RETRIES	(3)

#define AW87XXX_ADDR_BYTE_LEN		(1)
#define AW87XXX_DATA_BYTE_LEN		(1)
#define AW87XXX_BIN_TYPE_REG		(0)

#define AW87XXX_REG_CHIPID		(0x00)

#define AW87XXX_REG_REG_SYSCTRL		(0x01)

#define AW87XXX_REG_REG_SYSCTRL_DISABLE	(0x01)

#define AW87XXX_REG_NONE_ACCESS		(0)
#define AW87XXX_REG_RD_ACCESS		(1 << 0)
#define AW87XXX_REG_WR_ACCESS		(1 << 1)

#define AW87XXX_CFG_NUM_INIT		(0)
#define AW87XXX_ADD_CHAN_NAME_SHIFT	(2)
#define AW87XXX_MODE_CFG_NUM_MAX	(3)

#define KTHREAD_SYNC_INTERVAL		(100)
#define WAIT_DSP_OPEN_TIME		(3000)

enum aw87xxx_scene_mode {
	AW87XXX_OFF_MODE = 0,
	AW87XXX_MUSIC_MODE = 1,
	AW87XXX_VOICE_MODE = 2,
	AW87XXX_FM_MODE = 3,
	AW87XXX_RCV_MODE = 4,
	AW87XXX_MODE_MAX = 5,
};

enum aw87xxx_chip_type {
	AW87XXX_339 = 0,
	AW87XXX_359 = 1,
	AW87XXX_369 = 2,
	AW87XXX_519 = 3,
	AW87XXX_559 = 4,
	AW87XXX_CHIP_TYPE_MAX = 5,
};

enum aw87xxx_fm_version {
	AW87XXX_OLD_FIRMWARE = 0,
	AW87XXX_FH_FIRMWARE = 1,
};

enum aw87xxx_chipid {
	AW87XXX_CHIPID_39 = 0x39,
	AW87XXX_CHIPID_59 = 0x59,
	AW87XXX_CHIPID_69 = 0x69,
	AW87XXX_CHIPID_5A = 0x5A,
};

enum aw87xxx_cfg_update_flag {
	AW87XXX_CFG_NG = 0,
	AW87XXX_CFG_WAIT,
	AW87XXX_CFG_OK,
};

struct ui_frame_header {
	uint32_t check_sum;
	uint32_t bin_fh_ver;
	uint32_t bin_data_type;
	uint32_t bin_data_ver;
	uint32_t bin_data_len;
	uint32_t ui_ver;
	uint32_t chip_name[2];
	uint32_t addr_bit_wide;
	uint32_t data_bit_wide;
	uint32_t i2c_addr;
	uint32_t reserved[4];
};

struct aw87xxx_container {
	int len;
	unsigned char data[];
};

struct aw87xxx_scene_param {
	uint8_t scene_mode;
	uint8_t update_num;
	uint8_t cfg_update_flag;
	struct ui_frame_header *frame_header;
	struct aw87xxx_container *scene_cnt;
	struct aw_bin *bin_file;
};

struct aw87xxx_scene_list {
	struct aw87xxx_scene_param aw_off;
	struct aw87xxx_scene_param aw_music;
	struct aw87xxx_scene_param aw_voice;
	struct aw87xxx_scene_param aw_fm;
	struct aw87xxx_scene_param aw_rcv;
};

struct aw87xxx_reg_param {
	int reg_max;
	const unsigned char *reg_access;
};

/********************************************
 *
 * aw87xxx struct
 *
 *******************************************/

struct aw87xxx {
	bool open_dsp_en;

	uint8_t i2c_seq;
	uint8_t i2c_addr;
	uint8_t chipid;
	uint8_t product;
	uint8_t hwen_flag;
	uint8_t current_mode;
	char cfg_name[AW87XXX_MODE_MAX][AW87XXX_CFG_NAME_MAX];
	char vmax_cfg_name[AW87XXX_CFG_NAME_MAX];
	char chip_name[10];
	char bus_num[2];

	int reset_gpio;

	int32_t pa_channel;

	char *name_suffix;
	char channel[2];
	char *end;

	spinlock_t bin_parse_lock;
	struct mutex lock;
	struct mutex cfg_lock;
	struct delayed_work ram_work;
	struct i2c_client *i2c_client;
	struct device *dev;
	struct list_head list;
	struct aw87xxx_scene_list aw87xxx_scene_ls;
	struct aw87xxx_reg_param reg_param;
	struct aw87xxx_monitor monitor;
};

/********************************************
 *
 * print information control
 *
 *******************************************/
#define aw_dev_err(dev, format, ...) \
			pr_err("[%s]" format, dev_name(dev), ##__VA_ARGS__)

#define aw_dev_info(dev, format, ...) \
			pr_info("[%s]" format, dev_name(dev), ##__VA_ARGS__)

#define aw_dev_dbg(dev, format, ...) \
			pr_debug("[%s]" format, dev_name(dev), ##__VA_ARGS__)

/******************************************************************
* aw87xxx functions
*******************************************************************/
unsigned char aw87xxx_show_current_mode(int32_t channel);
int aw87xxx_audio_scene_load(uint8_t mode, int32_t channel);

#endif
