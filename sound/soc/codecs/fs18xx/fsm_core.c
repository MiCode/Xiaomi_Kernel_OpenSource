// SPDX-License-Identifier: GPL-2.0

/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-17 File created.
 */

#include "fsm_public.h"
#if defined(__KERNEL__)
#include <linux/slab.h>
#include <linux/delay.h>
#elif defined(FSM_HAL_SUPPORT)
#include <unistd.h>
#endif

#define CRC16_TABLE_SIZE      256
#define CRC16_POLY_NOMIAL     0xA001
#define OTP_OP_COUNTER_OFFSET 0xFF10

static int fsm_skip_device(fsm_dev_t *fsm_dev);
static int fsm_stub_check_stable(fsm_dev_t *fsm_dev, int type);
static int fsm_try_init(void);

static LIST_HEAD(fsm_dev_list);
#define fsm_list_init(fsm_dev) \
	do { \
		INIT_LIST_HEAD(&fsm_dev->list); \
		list_add(&fsm_dev->list, &fsm_dev_list); \
	} while (0)

#define fsm_list_entry(fsm_dev, ops) \
	do { \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev) && ops) { \
				ops(fsm_dev); \
			} \
		} \
	} while (0)

#define fsm_list_func(fsm_dev, func) \
	do { \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev)) { \
				func(fsm_dev); \
			} \
		} \
	} while (0)

#define fsm_list_check(fsm_dev, type, ret) \
	do { \
		ret = 0; \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev)) { \
				ret |= fsm_stub_check_stable(fsm_dev, type); \
				if (ret) { \
					break; \
				} \
			} \
		} \
	} while (0)

#define fsm_list_return(fsm_dev, func, ret) \
	do { \
		ret = 0; \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev)) { \
				ret |= func(fsm_dev); \
			} \
		} \
	} while (0)

#define fsm_list_func_arg(fsm_dev, func, argv) \
	do { \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev)) { \
				func(fsm_dev, argv); \
			} \
		} \
	} while (0)


static uint16_t g_crc16table[CRC16_TABLE_SIZE] = {
	0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
	0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
	0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
	0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
	0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
	0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
	0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
	0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
	0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
	0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
	0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
	0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
	0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
	0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
	0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
	0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
	0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
	0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
	0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
	0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
	0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
	0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
	0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
	0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
	0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
	0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
	0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
	0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
	0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
	0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
	0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
	0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040,
};

static struct fsm_config g_fsm_config = {
	.dev_count  = 0,
	.volume     = FSM_VOLUME_MAX,
	.next_scene = FSM_SCENE_MUSIC,
	.i2s_bclk   = 1536000,
	.i2s_srate  = 48000,
	.test_type  = TEST_NONE,
	.cur_angle  = 0,
	.next_angle = 0,

	// flags
	.vddd_on = 0,
	.codec_inited = 0,
	.force_fw = 0,
	.force_init = 0,
	.force_scene = 0,
	.force_calib = 0,
	.store_otp = 1,
	.force_mute = 0,
	.stop_test = 0,
	.skip_monitor = 0,
	.use_monitor = 1, // 0: close monitor
	.dev_suspend = 0,
	.stream_muted = 1,

	.fw_name = FSM_FW_NAME,
	.preset = NULL,
};

static const struct fsm_srate g_srate_tbl[] = {
	{  8000, 0 },
	{ 16000, 3 },
	{ 32000, 8 },
	{ 44100, 7 },
	{ 48000, 8 },
	// fs1860 support below srate:
	{ 88200, 9 },
	{ 96000, 10 },
};
static struct fsm_vbat_state g_vbat_state = { 0 };

static int fsm_skip_device(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!fsm_dev || !cfg) {
		return 1;
	}
	if (!fsm_dev->state.dev_inited) {
		return 1;
	}
	if ((fsm_dev->own_scene & cfg->next_scene) == 0) {
		return 1;
	}

	return 0;
}

static int fsm_cal_re25_zmdata(fsm_dev_t *fsm_dev,
				struct re25_data *re25, uint16_t zmdata)
{
	if (!fsm_dev || !re25) {
		return -EINVAL;
	}
	if (!fsm_dev->state.re25_runin) {
		return 0;
	}
	if (re25->count >= 10)
		return 0;

	if (abs(re25->pre_val - zmdata) > FSM_ZMDELTA_MAX
			|| re25->pre_val == 0) {
		re25->zmdata = 0;
		re25->pre_val = zmdata;
		re25->count = 1;
		re25->min_val = zmdata;
	} else {
		re25->count++;
		if (zmdata < re25->min_val) {
			re25->min_val = zmdata;
		}
	}
	pr_addr(info, "ZM[%2d]:%d", re25->count, zmdata);

	if (re25->count >= 10) {
		// finish calibration
		pr_addr(info, "ZM:%d", re25->min_val);
		re25->zmdata = re25->min_val;
		return 0;
	}

	return -EINVAL;
}

static int fsm_cal_f0_zmdata(fsm_dev_t *fsm_dev,
				struct f0_data *f0, int freq, uint16_t zmdata)
{
	fsm_config_t *cfg = fsm_get_config();
	int count;

	if (!fsm_dev || !f0 || !cfg) {
		return -EINVAL;
	}
	if (!fsm_dev->state.f0_runing) {
		return 0;
	}
	count = f0->count;
	pr_addr(info, "ZM[%4d]:%d", freq, zmdata);
	f0->freq[count] = freq;
	f0->zmdata[count] = zmdata;
	if (f0->count == 0 || zmdata < f0->min_zm) {
		f0->min_zm = zmdata;
		f0->min_idx = f0->count;
		fsm_dev->f0 = freq;
	}
	f0->count++;

	return 0;
}

static int fsm_cal_zmdata(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	uint16_t zmdata;
	int ret;

	if (!fsm_dev || !cfg) {
		return -EINVAL;
	}
	if (fsm_skip_device(fsm_dev)) {
		return 0;
	}
	ret = fsm_reg_multiread(fsm_dev, REG(FSM_ZMDATA), &zmdata);
	if (ret) {
		pr_addr(err, "get zmdata fail:%d", ret);
		return -EINVAL;
	}
	if (!fsm_dev->tdata) {
		fsm_dev->tdata = fsm_alloc_mem(sizeof(struct fsm_test_data));
		if (!fsm_dev->tdata) {
			pr_addr(err, "alloc test data fail");
			return -EINVAL;
		}
		memset(fsm_dev->tdata, 0, sizeof(struct fsm_test_data));
	}
	if (cfg->test_type == TEST_RE25) {
		ret = fsm_cal_re25_zmdata(fsm_dev, &fsm_dev->tdata->re25, zmdata);
	} else if (cfg->test_type == TEST_F0) {
		ret = fsm_cal_f0_zmdata(fsm_dev, &fsm_dev->tdata->f0,
			cfg->test_freq, zmdata);
	} else {
		pr_addr(err, "invalid test type:%d", cfg->test_type);
		ret = -EINVAL;
	}

	return ret;
}

static int fsm_list_wait(int type)
{
	fsm_dev_t *fsm_dev = NULL;
	int retry;
	int ret;

	for (retry = 1; retry <= FSM_WAIT_STABLE_RETRY; retry++) {
		fsm_delay_ms(2);
		fsm_list_check(fsm_dev, type, ret);
		if (!ret) {
			break;
		}
	}
	if (retry > FSM_WAIT_STABLE_RETRY) {
		pr_info("type:%d, wait timeout!", type);
	}
	pr_debug("type:%d, wait %d times", type, retry);

	return ret;
}

int zero_bit_counter(uint8_t byte)
{
	int count = 0;

	byte = ~byte;
	while (byte) {
		byte &= byte - 1;
		++count;
	}
	return count;
}

int get_otp_counter(uint16_t byte)
{
	// OTP offset start from 0x10 to 0x20(Totally 16)
	if (byte == OTP_OP_COUNTER_OFFSET) {
		return 0;
	}
	return (LOW8(byte) - LOW8(OTP_OP_COUNTER_OFFSET) + 1);
}

void convert_data_to_bytes(uint32_t val, uint8_t *buf)
{
	buf[0] = (val >> 8) & 0xFF;
	buf[1] = (val) & 0xFF;
	buf[2] = (val >> 24) & 0xFF;
	buf[3] = (val >> 16) & 0xFF;
}

fsm_config_t *fsm_get_config(void)
{
	return &g_fsm_config;
}
// EXPORT_SYMBOL(fsm_get_config);

void fsm_get_version(fsm_version_t *version)
{
	sprintf(version->git_branch, "%s", FSM_GIT_BRANCH);
	sprintf(version->git_commit, "%s", FSM_GIT_COMMIT);
	sprintf(version->code_date, "%s", FSM_CODE_DATE);
	sprintf(version->code_version, "%s", FSM_CODE_VERSION);
	pr_info("version %s", version->code_version);
}

void *fsm_alloc_mem(int size)
{
#if defined(__KERNEL__)
	return kzalloc(size, GFP_KERNEL);
#elif defined(FSM_HAL_SUPPORT)
	return malloc(size);
#else
	return NULL;
#endif
}

void fsm_free_mem(void *buf)
{
	if (buf == NULL) {
		return;
	}

#if defined(__KERNEL__)
	kfree(buf);
#elif defined(FSM_HAL_SUPPORT)
	free(buf);
#endif
	buf = NULL;
}

void fsm_delay_ms(uint32_t delay_ms)
{
	if (delay_ms == 0) {
		return;
	}
#if defined(__KERNEL__)
	usleep_range(delay_ms * 1000, delay_ms * 1000 + 1);
#elif defined(FSM_HAL_SUPPORT)
	usleep(delay_ms * 1000);
#endif
}

uint16_t set_bf_val(uint16_t *pval, const uint16_t bf, const uint16_t bf_val)
{
	uint8_t len = (bf >> 12) & 0x0F;
	uint8_t pos = (bf >> 8) & 0x0F;
	uint16_t new_val;
	uint16_t old_val;
	uint16_t msk;

	if (!pval) {
		return 0xFFFF;
	}
	old_val = new_val = *pval;
	msk = ((1 << (len + 1)) - 1) << pos;
	new_val &= ~msk;
	new_val |= bf_val << pos;
	*pval = new_val;

	return old_val;
}

uint16_t get_bf_val(const uint16_t bf, const uint16_t val)
{
	uint8_t len = (bf >> 12) & 0x0F;
	uint8_t pos = (bf >> 8) & 0x0F;
	uint16_t msk, value;

	msk = ((1 << (len + 1)) - 1) << pos;
	value = (val & msk) >> pos;

	return value;
}

int fsm_set_bf(fsm_dev_t *fsm_dev, const uint16_t bf, const uint16_t val)
{
	reg_unit_t reg;
	uint16_t oldval;
	uint16_t msk;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	reg.len = (bf >> 12) & 0x0F;
	reg.pos = (bf >> 8) & 0x0F;
	reg.addr = bf & 0xFF;
	if (reg.len == 15) {
		return fsm_reg_write(fsm_dev, reg.addr, val);
	}
	ret = fsm_reg_read(fsm_dev, reg.addr, &oldval);
	if (ret) {
		pr_info("get bf:%04X failed", bf);
		return ret;
	}

	msk = ((1 << (reg.len + 1)) - 1) << reg.pos;
	reg.value = oldval & (~msk);
	reg.value |= val << reg.pos;

	if (oldval == reg.value) {
		return 0;
	}
	ret = fsm_reg_write(fsm_dev, reg.addr, reg.value);
	if (ret) {
		pr_info("set bf:%04X failed", bf);
		return ret;
	}

	return ret;
}

int fsm_get_bf(fsm_dev_t *fsm_dev, const uint16_t bf, uint16_t *pval)
{
	reg_unit_t reg;
	uint16_t msk;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	reg.len = (bf >> 12) & 0x0F;
	reg.pos = (bf >> 8) & 0x0F;
	reg.addr = bf & 0xFF;
	ret = fsm_reg_multiread(fsm_dev, reg.addr, &reg.value);
	if (ret) {
		pr_info("get bf:%04X failed", bf);
		return ret;
	}
	msk = ((1 << (reg.len + 1)) - 1) << reg.pos;
	reg.value &= msk;
	if (pval) {
		*pval = reg.value >> reg.pos;
	}

	return ret;
}

struct fsm_dev *fsm_get_fsm_dev(uint8_t addr)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;

	if (!cfg || cfg->dev_count <= 0) {
		return NULL;
	}

	list_for_each_entry(fsm_dev, &fsm_dev_list, list) {
		if (fsm_dev && addr == fsm_dev->addr) {
			return fsm_dev;
		}
	}

	return NULL;
}

int fsm_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
#if defined(FSM_DEBUG_I2C)
	pr_addr(info, "%02X<-%04X", reg, val);
#endif
#if defined(CONFIG_FSM_REGMAP)
	ret = fsm_regmap_write(fsm_dev, reg, val);
#elif defined(CONFIG_FSM_I2C)
	ret = fsm_i2c_reg_write(fsm_dev, reg, val);
#elif defined(FSM_HAL_SUPPORT)
	ret = fsm_hal_reg_write(fsm_dev, reg, val);
#else
	ret = -EINVAL;
#endif
	if (ret) {
		pr_addr(err, "%02X<-%04X fail:%d", reg, val, ret);
	}

	return ret;
}

int fsm_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pval)
{
	uint16_t value;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
#if defined(CONFIG_FSM_REGMAP)
	ret = fsm_regmap_read(fsm_dev, reg, &value);
#elif defined(CONFIG_FSM_I2C)
	ret = fsm_i2c_reg_read(fsm_dev, reg, &value);
#elif defined(FSM_HAL_SUPPORT)
	ret = fsm_hal_reg_read(fsm_dev, reg, &value);
#else
	ret = -EINVAL;
#endif
#if defined(FSM_DEBUG_I2C)
	pr_addr(info, " %02X->%04X", reg, value);
#endif
	if (ret) {
		value = 0;
		pr_addr(err, " %02X->%04X fail:%d", reg, value, ret);
	}
	if (pval) {
		*pval = value;
	}

	return ret;
}

int fsm_burst_write(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *data, int len)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
#if defined(FSM_DEBUG_I2C)
	if (len >= 4) {
		pr_addr(info, "%02X<-%02X %02X %02X %02X", reg,
				data[0], data[1], data[2], data[3]);
	}
	// logprint("%s: %02X: %02X<-", __func__, fsm_dev->addr, reg);
	// for (ret = 0; ret < len; ret++) {
	// logprint("%02X ", data[ret]);
	// }
	// logprint("\n");
#endif

#if defined(CONFIG_FSM_REGMAP)
	ret = fsm_regmap_bulkwrite(fsm_dev, reg, data, len);
#elif defined(CONFIG_FSM_I2C)
	ret = fsm_i2c_bulkwrite(fsm_dev, reg, data, len);
#elif defined(FSM_HAL_SUPPORT)
	ret = fsm_hal_bulkwrite(fsm_dev, reg, data, len);
#else
	ret = -EINVAL;
#endif
	if (ret) {
		pr_addr(err, "BW %02X fail:%d", reg, ret);
	}

	return ret;
}

int fsm_reg_update_bits(fsm_dev_t *fsm_dev, reg_unit_t *reg)
{
	uint16_t temp;
	uint16_t mask;
	uint16_t val;
	uint8_t addr;
	int ret;

	if (!fsm_dev || !reg) {
		return -EINVAL;
	}
	addr = reg->addr;
	if (addr == REG(FSM_OTPACC)) {
		if (reg->value == 0 && fsm_dev->acc_count > 0) {
			fsm_dev->acc_count--;
		}
		if (reg->value != 0 && fsm_dev->acc_count < 0xFF) {
			fsm_dev->acc_count++;
		}
	}
	if (reg->pos == 0 && reg->len == 15) {
		return fsm_reg_write(fsm_dev, addr, reg->value);
	}
	mask = ((1 << (reg->len + 1)) - 1) << reg->pos;
	val = (reg->value << reg->pos);
	ret = fsm_reg_read(fsm_dev, addr, &temp);
	temp = ((~mask & temp) | (val & mask));
	ret |= fsm_reg_write(fsm_dev, addr, temp);

	return ret;
}

int fsm_reg_update(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val)
{
	uint16_t temp;
	int ret;

#if defined(FSM_DEBUG_I2C)
	pr_addr(info, "%02X<-%04X", reg, val);
#endif
	ret = fsm_reg_read(fsm_dev, reg, &temp);
	if (!ret && temp != val) {
		ret = fsm_reg_write(fsm_dev, reg, val);
	}
	if (ret) {
		pr_addr(err, "update reg:%02X failed:%d", reg, ret);
	}

	return ret;
}

int fsm_reg_multiread(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pval)
{
	uint16_t value;
	uint16_t old;
	int count = 0;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	while (count++ < FSM_I2C_RETRY) {
		ret = fsm_reg_read(fsm_dev, reg, &value);
		if (ret)
			continue;
		if (count > 1 && old == value) {
			break;
		}
		old = value;
	}
	if (ret) {
		value = 0;
#if !defined(BUILD_FSTOOL)
		pr_addr(err, " %02X->%04X fail:%d", reg, value, ret);
#endif
	}
	if (pval) {
		*pval = value;
	}

	return ret;
}

uint16_t fsm_calc_checksum(uint16_t *data, int len)
{
	uint16_t crc = 0;
	uint8_t index;
	uint8_t b;
	int i;

	if (len <= 0) {
		return 0;
	}

	for (i = 0; i < len; i++) {
		b = (uint8_t)(data[i] & 0xFF);
		index = (uint8_t)(crc ^ b);
		crc = (uint16_t)((crc >> 8) ^ g_crc16table[index]);
		b = (uint8_t)((data[i] >> 8) & 0xFF);
		index = (uint8_t)(crc ^ b);
		crc = (uint16_t)((crc >> 8) ^ g_crc16table[index]);
	}
	return crc;
}

int fsm_get_srate_bits(fsm_dev_t *fsm_dev, uint32_t srate)
{
	int size;
	int idx;

	if (srate > 48000 && HIGH8(fsm_dev->version) != FS1860_DEV_ID) {
		pr_addr(err, "invalid srate:%d", srate);
		return -EINVAL;
	}
	size = sizeof(g_srate_tbl) / sizeof(struct fsm_srate);
	for (idx = 0; idx < size; idx++) {
		if (srate == g_srate_tbl[idx].srate)
			return g_srate_tbl[idx].bf_val;
	}
	return -EINVAL;
}

int fsm_access_key(fsm_dev_t *fsm_dev, int access)
{
	int ret = 0;

	if (!fsm_dev) {
		return -EINVAL;
	}
	if (access) {
		if (fsm_dev->acc_count == 0) {
			ret = fsm_reg_write(fsm_dev, REG(FSM_OTPACC), FSM_OTP_ACC_KEY2);
			fsm_dev->acc_count = !ret ? 1 : 0;
		} else if (fsm_dev->acc_count < 0xFF) {
			fsm_dev->acc_count++;
		}
	} else {
		if (fsm_dev->acc_count == 1) {
			ret = fsm_reg_write(fsm_dev, REG(FSM_OTPACC), 0);
			fsm_dev->acc_count = !ret ? 0 : 1;
		} else if (fsm_dev->acc_count > 0) {
			fsm_dev->acc_count--;
		}
	}
	// pr_addr(debug, "acc:%d, count:%d", access, fsm_dev->acc_count);

	return ret;
}

int fsm_reg_dump(fsm_dev_t *fsm_dev)
{
	uint16_t value;
	int reg_addr;
	int ret;

	pr_info("%s: %02X: ", __func__, fsm_dev->addr);
	ret = fsm_access_key(fsm_dev, 1);
	for (reg_addr = 0; reg_addr < 0xFF; reg_addr++) {
		if (reg_addr == 0x0C)
			reg_addr = 0x89;
		else if (reg_addr == 0x8A)
			reg_addr = 0xA1;
		else if (reg_addr == 0xA2)
			reg_addr = 0xAF;
		else if (reg_addr == 0xB0)
			reg_addr = 0xBB;
		else if (reg_addr == 0xBC)
			reg_addr = 0xC0;
		else if (reg_addr == 0xC5)
			reg_addr = 0xC9;
		else if (reg_addr == 0xD0)
			reg_addr = 0xE8;
		ret |= fsm_reg_read(fsm_dev, reg_addr, &value);
		pr_info("%02X:%04X ", reg_addr, value);
		if (reg_addr == 0xE8) {
			break;
		}
	}
	ret |= fsm_access_key(fsm_dev, 0);
	pr_info("\n");

	return ret;
}

static int fsm_search_list(fsm_dev_t *fsm_dev, struct preset_file *pfile)
{
	dev_list_t *dev_list;
	uint8_t *preset;
	uint16_t offset;
	uint16_t type;
	int idx;

	preset = (uint8_t *)pfile;
	for (idx = 0; idx < pfile->hdr.ndev; idx++) {
		type = pfile->index[idx].type;
		if (FSM_DSC_DEV_INFO != type) {
			continue;
		}
		offset = pfile->index[idx].offset;
		pr_addr(debug, "offset[%d]: %d", idx, offset);
		dev_list = (struct dev_list *)&preset[offset];
		if (fsm_dev->addr == dev_list->addr) {
			pr_addr(info, "found dev_list");
			fsm_dev->dev_list = dev_list;
			break;
		}
	}
	if (idx == pfile->hdr.ndev) {
		pr_addr(err, "not found dev_list");
		fsm_dev->dev_list = NULL;
		return -EINVAL;
	}

	return 0;
}

static int fsm_check_dev_type(fsm_dev_t *fsm_dev)
{
	uint16_t dev_type;

	if (!fsm_dev || !fsm_dev->dev_list) {
		pr_addr(err, "bad parameter");
		return -EINVAL;
	}
	dev_type = fsm_dev->dev_list->dev_type;
	if (HIGH8(fsm_dev->version) == 0x06 || HIGH8(fsm_dev->version) == 0x0B) {
		// fs1801 series
		if (HIGH8(dev_type) != 0x0A) {
			return -EINVAL;
		}
		return 0;
	}
	// other series
	if (HIGH8(fsm_dev->version) != HIGH8(dev_type)) {
		pr_addr(err, "type:%02X not match version:%02X",
				HIGH8(dev_type), HIGH8(fsm_dev->version));
		return -EINVAL;
	}

	return 0;
}

int fsm_init_dev_list(fsm_dev_t *fsm_dev)
{
	struct preset_file *pfile;
	dev_list_t *dev_list;
	int ret;

	pfile = fsm_get_presets();
	if (!fsm_dev || !pfile) {
		pr_addr(err, "bad parameter or invalid FW");
		return -EINVAL;
	}

	pr_addr(debug, "ndev:%d", pfile->hdr.ndev);
	ret = fsm_search_list(fsm_dev, pfile);
	if (ret) {
		return ret;
	}
	dev_list = fsm_dev->dev_list;
	// assert(dev_list != NULL);
	ret = fsm_check_dev_type(fsm_dev);
	if (ret) {
		pr_addr(err, "type(%04X) not matched version(%04X)",
				dev_list->dev_type, fsm_dev->version);
		fsm_dev->dev_list = NULL;
		return ret;
	}
	pr_addr(info, "preset ver:%04X", dev_list->preset_ver);
	if ((dev_list->preset_ver & BIT(15)) == 0) { // BIT_15 must be 1
		pr_addr(err, "invalid preset version:%04X",
				dev_list->preset_ver);
		return -EINVAL;
	}
	pr_addr(debug, "bus  : %d, len: %d",
		dev_list->bus, dev_list->len);
	pr_addr(debug, "type : %04X, npreset: %d",
		dev_list->dev_type, dev_list->npreset);
	pr_addr(debug, "scene: eq:%04X, reg:%04X",
		dev_list->eq_scenes, dev_list->reg_scenes);
	fsm_dev->own_scene = dev_list->reg_scenes | dev_list->eq_scenes;

	return 0;
}

void *fsm_get_list_by_idx(fsm_dev_t *fsm_dev, int idx)
{
	dev_list_t *dev_list;
	fsm_index_t *index;
	uint8_t *pdata;

	if (fsm_dev == NULL) {
		return NULL;
	}
	dev_list = fsm_dev->dev_list;
	if (dev_list == NULL) {
		return NULL;
	}
	index = &dev_list->index[0];
	pdata = (uint8_t *)index;

	return (void *)(&pdata[index[idx].offset]);
}

void *fsm_get_data_list(fsm_dev_t *fsm_dev, int type)
{
	dev_list_t *dev_list;
	fsm_index_t *index;
	int i;

	if (fsm_dev == NULL || fsm_dev->dev_list == NULL) {
		return NULL;
	}
	dev_list = fsm_dev->dev_list;
	index = dev_list->index;
	if (index == NULL) {
		return NULL;
	}
	for (i = 0; i < dev_list->len; i++) {
		if (index[i].type == type) {
			break;
		}
	}
	if (i >= dev_list->len) {
		return NULL;
	}

	return (void *)((uint8_t *)index + index[i].offset);
}

int fsm_get_spk_info(fsm_dev_t *fsm_dev, uint16_t info_type)
{
	info_list_t *info;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	info = (info_list_t *)fsm_get_data_list(fsm_dev, FSM_DSC_SPK_INFO);
	if (!info || info_type > info->len) {
		return -EINVAL;
	}
	// pr_addr(debug, "spk info len: %d", info->len);

	return info->data[info_type];
}

int fsm_init_info(fsm_dev_t *fsm_dev)
{
	if (!fsm_dev || !fsm_dev->dev_list) {
		return -EINVAL;
	}
	fsm_dev->bus = fsm_dev->dev_list->bus;
	fsm_dev->addr = fsm_dev->dev_list->addr;
	fsm_dev->tmax = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_TMAX);
	fsm_dev->tcoef = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_TEMPR_COEF);
	fsm_dev->tsel = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_TEMPR_SEL);
	pr_addr(info, "tmax:%d, tcoef:%d, tsel:%d",
		fsm_dev->tmax, fsm_dev->tcoef, fsm_dev->tsel);
	fsm_dev->spkr = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_RES);
	fsm_dev->rapp = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_RAPP);
	if (IS_PRESET_V3(fsm_dev->dev_list->preset_ver)) {
		fsm_dev->pos_mask = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_POSITION);
		fsm_dev->compat.RS2RL_RATIO = \
				fsm_get_spk_info(fsm_dev, FSM_INFO_RSRL_RATIO);
	}
	pr_addr(info, "pos:%02X, rs_ratio:%d",
		fsm_dev->pos_mask, fsm_dev->compat.RS2RL_RATIO);
	pr_addr(info, "spkr:%d, rapp:%d",
		fsm_dev->spkr, fsm_dev->rapp);

	return 0;
}

struct preset_file *fsm_get_presets(void)
{
	return g_fsm_config.preset;
}

void fsm_set_presets(struct preset_file *file)
{
	fsm_config_t *cfg = fsm_get_config();

	if (cfg) {
		fsm_free_mem(cfg->preset);
		cfg->preset = file;
	}
}

/**
 * fsm device interface for external reference
 */

int fsm_parse_preset(const void *data, uint32_t size)
{
	struct preset_file *pfile;
	struct preset_header *hdr;
	uint16_t checksum;
	int crc_size;

	if (data == NULL || size == 0) {
		pr_info("bad parameter, size:%d", size);
		return -EINVAL;
	}
	pfile = fsm_get_presets();
	if (pfile) {
		pr_debug("already had presets, try init");
		fsm_try_init();
		return 0;
	}
	pfile = (struct preset_file *)fsm_alloc_mem(size);
	if (!pfile) {
		pr_info("alloc memery failed");
		return -ENOMEM;
	}
	memcpy(pfile, data, size);

	hdr = &pfile->hdr;
	pr_debug("version : %04X", hdr->version);
	pr_info("customer: %s", hdr->customer);
	pr_info("project : %s", hdr->project);
	pr_info("date    : %4d%02d%02d-%02d%02d", hdr->date.year, \
		hdr->date.month, hdr->date.day, hdr->date.hour, hdr->date.min);
	pr_debug("size    : %d", hdr->size);
	pr_debug("crc16   : %04X", hdr->crc16);

	crc_size = (size - sizeof(struct preset_header) + 2)/sizeof(uint16_t);
	if (hdr->size == 0 || hdr->size != size) {
		pr_info("invalid size: hdr:%d, fw:%d", hdr->size, size);
		return -EINVAL;
	}
	checksum = fsm_calc_checksum((uint16_t *)(&(pfile->hdr.ndev)), crc_size);
	if (checksum != hdr->crc16) {
		pr_info("checksum(%04X) not match(%04X)", checksum, hdr->crc16);
		return -EINVAL;
	} else {
		pr_info("checksum success!");
		fsm_set_presets(pfile);
	}
	fsm_try_init();

	return 0;
}

int fsm_swap_channel(fsm_dev_t *fsm_dev, int next_angle)
{
	uint16_t left_chn;
	uint16_t i2sctrl;
	uint16_t chs12;
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	switch (next_angle) {
	case 270:
		left_chn = (FSM_POS_LBTM | FSM_POS_RBTM);
		break;
	case 180:
		left_chn = (FSM_POS_RTOP | FSM_POS_RBTM);
		break;
	case 90:
		left_chn = (FSM_POS_LTOP | FSM_POS_RTOP);
		break;
	case 0:
	default:
		left_chn = (FSM_POS_LTOP | FSM_POS_LBTM);
		break;
	}
	left_chn = (left_chn & fsm_dev->pos_mask);
	chs12 = ((left_chn != 0) ? 1 : 2);
	if (fsm_dev->pos_mask == FSM_POS_MONO) {
		chs12 = 3;
	}
	ret = fsm_reg_read(fsm_dev, REG(FSM_I2SCTRL), &i2sctrl);
	if (get_bf_val(FSM_CHS12, i2sctrl) == chs12) {
		return ret;
	}
	ret |= fsm_set_bf(fsm_dev, FSM_DACMUTE, 1);
	set_bf_val(&i2sctrl, FSM_CHS12, chs12);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_I2SCTRL), i2sctrl);
	fsm_delay_ms(10);
	pr_addr(debug, "pos:%02X, CHS12:%d", fsm_dev->pos_mask, chs12);
	ret = fsm_set_bf(fsm_dev, FSM_DACMUTE, 0);

	return ret;
}

int fsm_wait_stable(fsm_dev_t *fsm_dev, int type)
{
	int retries = FSM_WAIT_STABLE_RETRY;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_skip_device(fsm_dev)) {
		return 0;
	}
	// fsm_delay_ms(10);
	while (retries-- > 0) {
		fsm_delay_ms(5); // delay 5 ms
		ret = fsm_stub_check_stable(fsm_dev, type);
		if (!ret)
			break;
	}
	if (retries <= 0) {
		pr_addr(err, "type: %d, wait timeout!", type);
		return -ETIMEDOUT;
	}
	return 0;
}

int fsm_set_spkset(fsm_dev_t *fsm_dev)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	switch (fsm_dev->spkr) {
	case 4:
		ret = fsm_set_bf(fsm_dev, FSM_SPKR, 0);
		break;
	case 6:
		ret = fsm_set_bf(fsm_dev, FSM_SPKR, 1);
		break;
	case 7:
		ret = fsm_set_bf(fsm_dev, FSM_SPKR, 2);
		break;
	case 8:
	default:
		ret = fsm_set_bf(fsm_dev, FSM_SPKR, 3);
		break;
	}

	return ret;
}

int fsm_read_vbat(fsm_dev_t *fsm_dev, uint16_t *vbat)
{
	uint16_t batv;
	uint8_t ver;
	int ret;

	if (fsm_dev == NULL || !vbat) {
		return -EINVAL;
	}
	ret = fsm_reg_multiread(fsm_dev, REG(FSM_BATV), &batv);
	if (ret) {
		pr_addr(err, "get batv fail:%d", ret);
		return ret;
	}
	ver = HIGH8(fsm_dev->version);
	switch (ver) {
	case FS1601S_DEV_ID:
	case FS1603_DEV_ID:
		*vbat = 10 * batv; // 10 mV per step
		break;
	case FS1818_DEV_ID:
	case FS1896_DEV_ID:
	case FS1860_DEV_ID:
		*vbat = 50 * batv / 4; // 50/4 mV per step
		break;
	default:
		pr_addr(err, "invalid dev_id:%02X", ver);
		return -EINVAL;
	}

	return ret;
}

int fsm_get_vbat(fsm_dev_t *fsm_dev)
{
	struct fsm_vbat_state *vbat_state = &g_vbat_state;
	fsm_config_t *cfg = fsm_get_config();
	uint16_t batv;
	int ret;

	if (!cfg || fsm_dev == NULL || !vbat_state) {
		return -EINVAL;
	}
	ret = fsm_read_vbat(fsm_dev, &batv);
	if (ret) {
		return ret;
	}
	// pr_addr(info, "batv:%d", batv);
	// get min batv of all devices
	if (vbat_state->cur_batv == 0 || batv < vbat_state->cur_batv) {
		vbat_state->cur_batv = batv;
	}

	return ret;
}

int fsm_write_stereo_ceof(fsm_dev_t *fsm_dev)
{
	stereo_coef_t *stereo_coef;
	int ret = 0;
	int i;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	stereo_coef = (stereo_coef_t *)fsm_get_data_list(fsm_dev, FSM_DSC_STEREO_COEF);
	if (!stereo_coef) {
		pr_addr(info, "note: not found data");
		return 0;
	}
	pr_addr(debug, "stereo coef len: %d", stereo_coef->len);
	for (i = 0; i < stereo_coef->len - 2; i++) {
		ret |= fsm_reg_write(fsm_dev, REG(FSM_STERC1) + i, stereo_coef->data[i]);
	}
	ret |= fsm_reg_write(fsm_dev, REG(FSM_STERCTRL), stereo_coef->data[i++]);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_STERGAIN), stereo_coef->data[i++]);

	FSM_FUNC_EXIT(ret);
	return ret;
}

int fsm_write_excer_ram(fsm_dev_t *fsm_dev)
{
	ram_data_t *excer_ram;
	uint8_t write_buf[4];
	int count;
	int ret;
	int i;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	excer_ram = fsm_get_data_list(fsm_dev, FSM_DSC_EXCER_RAM);
	if (!excer_ram) {
		pr_addr(err, "note: not found data");
		return -EINVAL;
	}
	count = excer_ram->len;
	pr_addr(debug, "excer ram len: %d", count);
	ret = fsm_reg_write(fsm_dev, fsm_dev->compat.DACEQA,
			fsm_dev->compat.addr_excer_ram);
	for (i = 0; i < count; i++) {
		convert_data_to_bytes(excer_ram->data[i], write_buf);
		ret |= fsm_burst_write(fsm_dev, fsm_dev->compat.DACEQWL,
				write_buf, sizeof(uint32_t));
	}

	return ret;
}

int fsm_write_reg_tbl(fsm_dev_t *fsm_dev, uint16_t scene)
{
	reg_scene_t *reg_scene;
	reg_comm_t *reg_comm;
	reg_unit_t *reg;
	regs_unit_t *regs;
	int ret = 0;
	int i;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (scene == FSM_SCENE_COMMON) {
		reg_comm = fsm_get_data_list(fsm_dev, FSM_DSC_REG_COMMON);
		if (!reg_comm) {
			pr_addr(info, "note: not found reg_common data");
			return 0;
		}
		reg = reg_comm->reg;
		pr_addr(debug, "reg comm len: %d", reg_comm->len);
		for (i = 0; i < reg_comm->len; i++) {
			ret |= fsm_reg_update_bits(fsm_dev, &reg[i]);
		}
	} else if (scene != FSM_SCENE_UNKNOW) {
		reg_scene = fsm_get_data_list(fsm_dev, FSM_DSC_REG_SCENES);
		if (!reg_scene) {
			pr_addr(info, "note: not found reg_scene data");
			return 0;
		}
		regs = reg_scene->regs;
		pr_addr(debug, "reg scene len: %d", reg_scene->len);
		for (i = 0; i < reg_scene->len; i++) {
			if ((regs[i].scene & scene) == 0) {
				continue;
			}
			ret |= fsm_reg_update_bits(fsm_dev, &regs[i].reg);
		}
	}

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int parse_otp_write_count(fsm_dev_t *fsm_dev, uint16_t valOTP)
{
	uint16_t count = 0;
	uint8_t devid;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	devid = HIGH8(fsm_dev->version);
	switch (devid) {
	case FS1603_DEV_ID:
		count = zero_bit_counter(valOTP & 0xFF);
		break;
	case FS1818_DEV_ID:
	case FS1896_DEV_ID:
		count = get_otp_counter(valOTP);
		break;
	default:
		pr_addr(err, "invalid DEVID:%02X", devid);
		break;
	}
	return count;
}

int fsm_write_preset_eq(fsm_dev_t *fsm_dev,
				int ram_id, uint16_t scene)
{
	uint8_t write_buf[sizeof(uint32_t)];
	preset_list_t *preset_list = NULL;
	dev_list_t *dev_list;
	int ret;
	int i;

	if (fsm_dev == NULL || fsm_dev->dev_list == NULL) {
		return -EINVAL;
	}
	if ((fsm_dev->ram_scene[ram_id] != 0xFFFF)
			&& ((fsm_dev->ram_scene[ram_id] & scene) != 0)) {
		pr_addr(info, "RAM%d scene:%04X", ram_id,
				fsm_dev->ram_scene[ram_id]);
		return 0;
	}
	dev_list = fsm_dev->dev_list;
	if ((dev_list->eq_scenes & scene) == 0) {
		pr_addr(warning, "eq_scenes:%04X unmatched scene:%04X",
				dev_list->eq_scenes, scene);
		return 0;
	}
	for (i = 0; i < dev_list->len; i++) {
		if (dev_list->index[i].type != FSM_DSC_PRESET_EQ) {
			continue;
		}
		preset_list = (preset_list_t *)((uint8_t *)dev_list->index
				+ dev_list->index[i].offset);
		if (preset_list && preset_list->scene & scene) {
			break;
		}
	}
	// pr_addr(debug, "preset offset: %04x", index[i].offset);
	if (!preset_list || i >= dev_list->len) {
		pr_addr(err, "not found preset: %04X", scene);
		return -EINVAL;
	}

	if (preset_list->len != fsm_dev->compat.preset_unit_len) {
		pr_addr(err, "invalid size: %d", preset_list->len);
		return -EINVAL;
	}
	ret = fsm_reg_write(fsm_dev, fsm_dev->compat.ACSEQA,
			((ram_id == FSM_EQ_RAM0) ? 0 : preset_list->len));
	for (i = 0; i < preset_list->len; i++) {
		convert_data_to_bytes(preset_list->data[i], write_buf);
		ret |= fsm_burst_write(fsm_dev, fsm_dev->compat.ACSEQWL,
				write_buf, sizeof(uint32_t));
	}
	if (!ret) {
		fsm_dev->ram_scene[ram_id] = preset_list->scene;
		pr_addr(info, "wroten ram_scene[%d]:%04X",
				ram_id, preset_list->scene);
	}

	FSM_FUNC_EXIT(ret);
	return ret;
}

int fsm_switch_preset(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	reg_temp_t sysctrl;
	reg_temp_t pllc4;
	uint16_t dspen;
	int ret;

	pr_addr(debug, "%s switching",
			(cfg->force_scene ? "force" : "auto"));
	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (!cfg->force_scene && fsm_dev->cur_scene == cfg->next_scene) {
		pr_addr(debug, "same scene, skip");
		return 0;
	}
	// need amplifier off
	ret = fsm_reg_read(fsm_dev, REG(FSM_SYSCTRL), &sysctrl.new_val);
	sysctrl.old_val = set_bf_val(&sysctrl.new_val, FSM_AMPE, 0);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl.new_val);
	// wait stable
	ret |= fsm_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
	if (ret) {
		pr_addr(err, "wait timeout!");
		return ret;
	}
	// enable pll osc
	ret = fsm_reg_read(fsm_dev, REG(FSM_PLLCTRL4), &pllc4.new_val);
	pllc4.old_val = set_bf_val(&pllc4.new_val, FSM_OSCEN, 1);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), pllc4.new_val);
	fsm_delay_ms(5); // 5ms

	// load reg table
	ret |= fsm_write_reg_tbl(fsm_dev, cfg->next_scene);

	ret |= fsm_get_bf(fsm_dev, FSM_DSPEN, &dspen);
	if (dspen == 0) { // bypass DSP
		pr_addr(info, "note: Bypass DSP mode");
		fsm_dev->state.bypass_dsp = true;
		ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), pllc4.old_val);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl.old_val);
		fsm_dev->cur_scene = cfg->next_scene;
		return 0;
	}
	fsm_dev->state.bypass_dsp = false;

	ret |= fsm_access_key(fsm_dev, 1);
	if (cfg->next_scene & fsm_dev->ram_scene[FSM_EQ_RAM0]) {
		ret |= fsm_reg_write(fsm_dev, REG(FSM_ACSCTRL), 0x9880); // eq ram0
	} else if (cfg->next_scene & fsm_dev->ram_scene[FSM_EQ_RAM1]) {
		ret |= fsm_reg_write(fsm_dev, REG(FSM_ACSCTRL), 0x9890); // eq ram1
	} else if (fsm_dev->dev_list->eq_scenes & cfg->next_scene) {
		// use ram1 to switch preset
		ret |= fsm_write_preset_eq(fsm_dev, FSM_EQ_RAM1, cfg->next_scene);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_ACSCTRL), 0x9890); // eq ram1
		if (ret) {
			pr_addr(err, "update scene[%04X] fail:%d",
					cfg->next_scene, ret);
		}
	}
	ret |= fsm_access_key(fsm_dev, 0);
	if (!ret) {
		fsm_dev->cur_scene = cfg->next_scene;
		pr_addr(info, "switched scene:%04X", cfg->next_scene);
	} else {
		fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
	}

	// recover pll and sysctrl
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), pllc4.old_val);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl.old_val);
	// ret |= fsm_reg_read(fsm_dev, REG(FSM_STATUS), NULL);//TODO

	FSM_FUNC_EXIT(ret);
	return ret;
}

int fsm_write_preset(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	reg_temp_t sysctrl;
	int ret;

	if (!cfg || !fsm_dev || fsm_dev->own_scene == FSM_SCENE_UNKNOW) {
		return -EINVAL;
	}

	if (cfg->force_scene) {
		fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
		fsm_dev->ram_scene[FSM_EQ_RAM0] = FSM_SCENE_UNKNOW;
		fsm_dev->ram_scene[FSM_EQ_RAM1] = FSM_SCENE_UNKNOW;
	}
	// need amplifier off
	ret = fsm_reg_read(fsm_dev, REG(FSM_SYSCTRL), &sysctrl.new_val);
	sysctrl.old_val = set_bf_val(&sysctrl.new_val, FSM_AMPE, 0);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl.new_val);

	ret |= fsm_set_spkset(fsm_dev);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_TEMPSEL), (fsm_dev->tcoef << 1));
	ret = fsm_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
	if (ret) {
		pr_addr(err, "wait timeout!");
		return ret;
	}

	ret |= fsm_access_key(fsm_dev, 1);
	ret |= fsm_write_stereo_ceof(fsm_dev);
	ret |= fsm_write_excer_ram(fsm_dev);
	// use music and voice scene config as default
	ret |= fsm_write_preset_eq(fsm_dev, FSM_EQ_RAM0, FSM_SCENE_MUSIC);
	ret |= fsm_write_preset_eq(fsm_dev, FSM_EQ_RAM1, FSM_SCENE_VOICE);
	ret |= fsm_write_reg_tbl(fsm_dev, FSM_SCENE_COMMON);
	ret |= fsm_access_key(fsm_dev, 0);

	// ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), 0);

	ret |= fsm_switch_preset(fsm_dev);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl.old_val);

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fsm_set_tsctrl(fsm_dev_t *fsm_dev,
				bool enable, bool auto_off)
{
	uint16_t tsctrl;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_reg_multiread(fsm_dev, REG(FSM_TSCTRL), &tsctrl);
	set_bf_val(&tsctrl, FSM_TSEN, enable);
	set_bf_val(&tsctrl, FSM_OFF_AUTOEN, auto_off);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_TSCTRL), tsctrl);

	FSM_FUNC_EXIT(ret);
	return ret;
}

uint32_t fsm_cal_spkr_zmimp(fsm_dev_t *fsm_dev, uint16_t data)
{
	uint32_t result;

	if (!fsm_dev) {
		return 0xFFFF;
	}
	if (fsm_dev->compat.RS2RL_RATIO == 0) {
		pr_addr(info, "invalid rs ratio");
		return 0;
	}
	if (data == 0) { // invalid data
		return 0xFFFF;
	}
	result = FSM_MAGNIF(fsm_dev->compat.RS2RL_RATIO * LOW8(fsm_dev->rstrim));
	result = (result / data);

	return result;
}

static uint16_t fsm_cal_threshold(fsm_dev_t *fsm_dev,
				int mt_type, uint16_t zmdata)
{
	uint16_t mt_tempr;
	uint32_t result;
	uint32_t spk_rt;
	int rapp;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	mt_tempr = fsm_get_spk_info(fsm_dev, mt_type);
	pr_addr(debug, "MT[%d]:%d", mt_type, mt_tempr);
	if (mt_tempr <= 0) {
		pr_addr(err, "get MT info failed");
		return 0;
	}
	result = (uint32_t)zmdata * FSM_MAGNIF_TEMPR_COEF;
	result = result / (FSM_MAGNIF_TEMPR_COEF + (uint32_t)fsm_dev->tcoef *
			(mt_tempr * fsm_dev->tmax / 100 - (fsm_dev->tsel >> 1)));
	rapp = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_RAPP);
	spk_rt = fsm_cal_spkr_zmimp(fsm_dev, result);
	if (rapp > 0) {
		pr_addr(info, "rapp:%d, zm:%X", rapp, result);
		spk_rt += rapp;
		result = fsm_cal_spkr_zmimp(fsm_dev, spk_rt);
	}
	pr_addr(info, "MT[%3d]:%X, rt:%d", mt_tempr, result, spk_rt);

	return (uint16_t)result;
}

static int fsm_check_re25_range(fsm_dev_t *fsm_dev, int re25)
{
	int temp_val;
	int re25_min;
	int re25_max;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	temp_val = FSM_MAGNIF(fsm_dev->spkr);
	if (fsm_dev->spkr <= 10) {
		re25_min = temp_val * (100 - FSM_SPKR_ALLOWANCE) / 100;
		re25_max = temp_val * (100 + FSM_SPKR_ALLOWANCE) / 100;
	} else {
		re25_min = temp_val * (100 - FSM_RCVR_ALLOWANCE) / 100;
		re25_max = temp_val * (100 + FSM_RCVR_ALLOWANCE) / 100;
	}
	pr_addr(info, "spkr:%d, min:%d, max:%d",
			fsm_dev->spkr, re25_min, re25_max);
	if (re25 < re25_min || re25 > re25_max) {
		pr_addr(err, "invalid re25:%d", re25);
		fsm_dev->state.calibrated = false;
		fsm_reg_write(fsm_dev, REG(FSM_ADCTIME), 0x0031);
		fsm_set_tsctrl(fsm_dev, true, true);
		return -EINVAL;
	}

	return 0;
}

static int fsm_set_threshold_v1(fsm_dev_t *fsm_dev, uint16_t zmdata)
{
	uint16_t value;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TRE, zmdata);
	ret = fsm_reg_write(fsm_dev, REG(FSM_SPKRE), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM6, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKM6), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM24, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKM24), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TERR, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKERR), value);

	return ret;
}

static int fsm_set_threshold_v2(fsm_dev_t *fsm_dev, uint16_t zmdata)
{
	uint16_t value;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TRE, zmdata);
	ret = fsm_reg_write(fsm_dev, REG(FSM_SPKRE), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM01, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKMT01), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM02, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKMT02), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM03, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKMT03), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM04, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKMT04), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM05, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKMT05), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TM06, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKMT06), value);
	value = fsm_cal_threshold(fsm_dev, FSM_INFO_SPK_TERR, zmdata);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKERR), value);

	return ret;
}

static int fsm_set_threshold(fsm_dev_t *fsm_dev,
				uint16_t data, int type)
{
	struct fsm_calib calib;
	uint16_t value;
	int temp_val;
	int rapp;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_reg_read(fsm_dev, REG(FSM_OTPPG1W2), &value);
	if (ret || LOW8(value) == 0) {
		pr_info("use default rs_trim value");
		value = FSM_RS_TRIM_DEFAULT;
	}
	fsm_dev->rstrim = value;
	temp_val = (data == 0) ? 0xFFFF : \
		(FSM_MAGNIF(fsm_dev->compat.RS2RL_RATIO * LOW8(value)) / data);

	if (type == FSM_DATA_TYPE_ZMDATA) {
		rapp = fsm_get_spk_info(fsm_dev, FSM_INFO_SPK_RAPP);
		calib.cal_zm = data;
		calib.re25 = ((data == 0xFFFF) ? 65535 : temp_val);
		if (rapp > 0) {
			pr_addr(info, "re25(has rapp:%d):%d", rapp, calib.re25);
			calib.re25 -= rapp;
			// update cal_zm by re25 without rapp
			calib.cal_zm = fsm_cal_spkr_zmimp(fsm_dev, calib.re25);
		}
	} else {
		calib.cal_zm = temp_val;
		calib.re25 = data;
	}
	pr_addr(info, "zm:%04X, re25:%d", calib.cal_zm, calib.re25);

	fsm_dev->re25 = calib.re25;
	ret |= fsm_check_re25_range(fsm_dev, calib.re25);
	if (ret) {
		pr_info("check re25 fail:%d", ret);
		fsm_dev->state.calibrated = false;
		return ret;
	}
	fsm_dev->state.calibrated = true;
	if ((IS_PRESET_V3(fsm_dev->dev_list->preset_ver))
			&& fsm_dev->is_1894s) {
		ret |= fsm_set_threshold_v2(fsm_dev, calib.cal_zm);
	} else {
		ret |= fsm_set_threshold_v1(fsm_dev, calib.cal_zm);
	}

	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCTIME), 0x0031);

	FSM_FUNC_EXIT(ret);
	return ret;
}

int fsm_parse_otp(fsm_dev_t *fsm_dev, uint16_t value,
				int *re25, int *count)
{
	uint8_t byte;
	int step_unit;
	int cal_count;
	int spkr_mag;
	int offset;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	// parse re25
	if (re25 != NULL) {
		byte = (uint8_t)((value >> 8) & 0xFF);
		spkr_mag = FSM_MAGNIF(fsm_dev->spkr);
		if (fsm_dev->spkr <= 10) {
			step_unit = spkr_mag * FSM_SPKR_ALLOWANCE / 12700;
		} else {
			step_unit = spkr_mag * FSM_RCVR_ALLOWANCE / 12700;
		}
		step_unit += 1; // fix integer precision
		if (byte == 0x7F) {
			byte = 0x00;
		}
		offset = (byte & 0x7F) * step_unit;
		if ((byte & 0x80) != 0) {
			*re25 = spkr_mag - offset;
		} else {
			*re25 = spkr_mag + offset;
		}
		pr_addr(info, "offset:%d, step:%d, re25:%d mohm",
				offset, step_unit, *re25);
	}

	// parse calibration count
	cal_count = parse_otp_write_count(fsm_dev, value);
	pr_addr(info, "cal_count:%d", cal_count);
	fsm_dev->state.otp_stored = ((cal_count > 0) ? 1 : 0);
	if (count != NULL) {
		*count = cal_count;
	}

	return 0;
}

int fsm_check_otp(fsm_dev_t *fsm_dev)
{
	uint16_t otppg2;
	uint16_t pllc4;
	int count;
	int re25;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_reg_read(fsm_dev, REG(FSM_PLLCTRL4), &pllc4);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), 0x000F);
	ret |= fsm_access_key(fsm_dev, 1);
	ret |= fsm_reg_read(fsm_dev, REG(FSM_OTPPG2), &otppg2);
	pr_addr(debug, "OTPPG2:%04X", otppg2);
	fsm_parse_otp(fsm_dev, otppg2, &re25, &count);
	fsm_dev->cal_count = count;

	if (count > 0 && count < fsm_dev->compat.otp_max_count) {
		fsm_set_threshold(fsm_dev, re25, FSM_DATA_TYPE_RE25);
	} else if (count == 0 && fsm_dev->re25_dft != 0) {
		fsm_set_threshold(fsm_dev, fsm_dev->re25_dft, FSM_DATA_TYPE_RE25);
		pr_addr(warning, "not calibrate yet");
	} else {
		pr_addr(err, "got something wrong");
	}
	ret |= fsm_access_key(fsm_dev, 0);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), pllc4);

	FSM_FUNC_EXIT(ret);
	return ret;
}

int fsm_config_vol(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsm_vbat_state *vbat_state;
	uint16_t cur_batv;
	uint16_t vol_vbat;
	uint16_t volume;

	vbat_state = &g_vbat_state;
	if (!fsm_dev || !cfg || !vbat_state) {
		return -EINVAL;
	}
	if (cfg->stream_muted) {
		return fsm_reg_write(fsm_dev, REG(FSM_AUDIOCTRL), 0x0000);
	}
	if (cfg->volume > FSM_VOLUME_MAX) {
		cfg->volume = FSM_VOLUME_MAX;
	}
	volume = ((fsm_dev->state.calibrated || fsm_dev->re25_dft != 0) ? cfg->volume : 0xDF); // -12db
	if (cfg->use_monitor) {
		cur_batv = vbat_state->cur_batv;
		if (cur_batv > FSM_VBAT_HIGH)
			cur_batv = FSM_VBAT_HIGH;
		else if (cur_batv < FSM_VBAT_LOW)
			cur_batv = FSM_VBAT_LOW;
		vol_vbat = 0xFF - (FSM_VBAT_HIGH - cur_batv) / FSM_VBAT_STEP;
		if (vbat_state->cur_batv != 0 && cfg->next_scene < FSM_SCENE_LOW_PWR) {
			// update volume
			volume = MIN(volume, vol_vbat);
		}
	}
	volume = ((volume << 8) & 0xFF00);
	pr_addr(info, "vol: %04X", volume);

	return fsm_reg_write(fsm_dev, REG(FSM_AUDIOCTRL), volume);
}

static uint8_t fsm_re25_to_byte(fsm_dev_t *fsm_dev, int re25)
{
	uint8_t valOTP;
	int step_unit;
	int spkr_mag;
	int offset;

	if (fsm_dev == NULL || re25 == 0) {
		return 0xff;
	}

	spkr_mag = FSM_MAGNIF(fsm_dev->spkr);
	if (fsm_dev->spkr <= 10) {
		step_unit = (spkr_mag * FSM_SPKR_ALLOWANCE) / 12700;
	} else {
		step_unit = (spkr_mag * FSM_RCVR_ALLOWANCE) / 12700;
	}

	step_unit += 1; // fix integer precision
	offset = re25 - spkr_mag;
	if (offset < 0) {
		offset *= -1;
		valOTP = (uint8_t)((offset / step_unit) & 0xFF) + 1;
		if (valOTP > 0x7F) {
			valOTP = 0x7F;
		}
		valOTP |= 0x80;
	} else {
		valOTP = (uint8_t)((offset / step_unit) & 0xFF) - 1;
		if (offset < step_unit) {
			valOTP = 0;
		}
		if (valOTP > 0x7F) {
			valOTP = 0x7F;
		}
	}
	pr_addr(info, "re25:%d mohm, step:%d, offset:%d, valOTP:%02X",
			re25, step_unit, offset, valOTP);

	return valOTP;
}

static int fsm_set_cal_mode(fsm_dev_t *fsm_dev)
{
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	fsm_access_key(fsm_dev, 1);
	ret = fsm_reg_write(fsm_dev, REG(FSM_ZMCONFIG), 0x0010);
	ret |= fsm_set_tsctrl(fsm_dev, true, false);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_AUDIOCTRL), 0x8F00);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKERR), 0x0000);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKRE), 0x0000);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKM6), 0x0000);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SPKM24), 0x0000);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCENV), 0xFFFF);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCTIME), 0x0031);
	fsm_access_key(fsm_dev, 0);

	return ret;
}

int fsm_check_status(fsm_dev_t *fsm_dev)
{
	uint16_t status;
	uint16_t bf_val;
	int ready;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_reg_multiread(fsm_dev, REG(FSM_STATUS), &status);
	pr_addr(info, "status:%04X", status);
	bf_val = get_bf_val(FSM_BOVDS, status);
	if (!bf_val)
		pr_addr(info, "overvoltage on boost");
	ready = bf_val;
	bf_val = get_bf_val(FSM_PLLS, status);
	if (!bf_val)
		pr_addr(info, "PLL is not in lock");
	ready &= bf_val;
	bf_val = get_bf_val(FSM_OTDS, status);
	if (!bf_val)
		pr_addr(info, "over temperature");
	ready &= bf_val;
	bf_val = get_bf_val(FSM_OVDS, status);
	if (!bf_val)
		pr_addr(info, "overvoltage on VBAT");
	ready &= bf_val;
	bf_val = get_bf_val(FSM_UVDS, status);
	if (!bf_val)
		pr_addr(info, "undervoltage on VBAT");
	ready &= bf_val;
	bf_val = get_bf_val(FSM_OCDS, status);
	if (bf_val)
		pr_addr(info, "overcurrent in amplifier");
	ready &= (!bf_val);
	bf_val = get_bf_val(FSM_CLKS, status);
	if (!bf_val)
		pr_addr(info, "bclk/ws is not stable");
	ready &= bf_val;
	if (!ret && ready) {
		return 0;
	}

	return -EINVAL;
}

/*
 * fsm stub api
 */
int fsm_stub_dev_init(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret;

	if (fsm_dev == NULL || cfg == NULL) {
		return -EINVAL;
	}

	if (cfg->force_init) {
		pr_addr(info, "force init");
	}
	if (!cfg->force_init && fsm_dev->state.dev_inited) {
		return 0;
	}
	if (fsm_dev->dev_ops.dev_init) {
		return fsm_dev->dev_ops.dev_init(fsm_dev);
	}
	ret = fsm_init_dev_list(fsm_dev);
	if (ret || !fsm_dev->dev_list) {
		pr_addr(err, "init dev_list fail:%d", ret);
		return ret;
	}
	fsm_dev->state.dev_inited = true;
	ret = fsm_init_info(fsm_dev);

	if (fsm_dev->dev_ops.reg_init) {
		fsm_dev->dev_ops.reg_init(fsm_dev);
	}
	ret |= fsm_write_preset(fsm_dev);
	ret |= fsm_check_otp(fsm_dev);

	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0001);
	fsm_wait_stable(fsm_dev, FSM_WAIT_AMP_ADC_OFF);
	if (ret) {
		pr_addr(err, "init failed:%d", ret);
		fsm_dev->state.dev_inited = false;
	}
	fsm_dev->errcode = ret;

	return ret;
}

int fsm_stub_start_up(fsm_dev_t *fsm_dev)
{
	uint16_t sysctrl;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.start_up) {
		return fsm_dev->dev_ops.start_up(fsm_dev);
	}
	if (fsm_dev->dev_ops.i2s_config) {
		ret = fsm_dev->dev_ops.i2s_config(fsm_dev);
	}
	if (fsm_dev->dev_ops.pll_config) {
		ret = fsm_dev->dev_ops.pll_config(fsm_dev, true);
	}
	// power on device
	ret = fsm_reg_read(fsm_dev, REG(FSM_SYSCTRL), &sysctrl);
	// set_bf_val(&sysctrl, FSM_AMPE, 1);
	set_bf_val(&sysctrl, FSM_PWDN, 0);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl);
	// ret |= fsm_config_vol(fsm_dev); // move to fsm_stub_set_mute
	fsm_dev->errcode = ret;

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fsm_stub_check_stable(fsm_dev_t *fsm_dev, int type)
{
	uint16_t value;
	int ready;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.check_stable) {
		return fsm_dev->dev_ops.check_stable(fsm_dev, type);
	}
	switch (type) {
	case FSM_WAIT_STATUS_ON:
		ret = fsm_check_status(fsm_dev);
		ready = !ret;
		break;
	case FSM_WAIT_AMP_ON:
	case FSM_WAIT_AMP_ADC_ON:
		ret = fsm_reg_multiread(fsm_dev, REG(FSM_STATUS), &value);
		ready = get_bf_val(FSM_CLKS, value);
		ready &= get_bf_val(FSM_PLLS, value);
		ret |= fsm_get_bf(fsm_dev, FSM_SSEND, &value);
		ready &= value;
		break;
	case FSM_WAIT_AMP_OFF:
		ret = fsm_get_bf(fsm_dev, FSM_DACRUN, &value);
		ready = (value == 0 ? 1 : 0);
		break;
	case FSM_WAIT_AMP_ADC_OFF:
	case FSM_WAIT_AMP_ADC_PLL_OFF:
		ret = fsm_reg_multiread(fsm_dev, REG(FSM_DIGSTAT), &value);
		ready = get_bf_val(FSM_ADCRUN, value);
		ready |= get_bf_val(FSM_DACRUN, value);
		ready = !ready;
		break;
	case FSM_WAIT_TSIGNAL_OFF:
		ret = fsm_get_bf(fsm_dev, FSM_OFFSTA, &value);
		ready = value;
		break;
	case FSM_WAIT_OTP_READY:
		ret = fsm_get_bf(fsm_dev, FSM_OTPBUSY, &value);
		ready = !value;
		break;
	case FSM_WAIT_BOOST_SSEND:
		ret = fsm_get_bf(fsm_dev, FSM_SSEND, &value);
		ready = value;
		break;
	default:
		ret = -EINVAL;
		ready = 0;
		break;
	}
	if (ready && type == FSM_WAIT_AMP_ADC_PLL_OFF) {
		if (fsm_dev->dev_ops.pll_config) {
			fsm_dev->dev_ops.pll_config(fsm_dev, false);
		}
	}
	if (ready && type == FSM_WAIT_AMP_ADC_ON) {
		ret |= fsm_reg_multiread(fsm_dev, REG(FSM_DIGSTAT), &value);
		ready &= get_bf_val(FSM_DACRUN, value);
		ready &= get_bf_val(FSM_ADCRUN, value);
	}
	if (!ret && !ready) {
		ret = -EINVAL;
	}

	return ret;
}

int fsm_stub_set_tsignal(fsm_dev_t *fsm_dev, bool enable)
{
	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.set_tsignal) {
		return fsm_dev->dev_ops.set_tsignal(fsm_dev, enable);
	}
	return fsm_set_tsctrl(fsm_dev, enable, true);
}

int fsm_stub_set_mute(fsm_dev_t *fsm_dev, bool mute)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.set_mute) {
		return fsm_dev->dev_ops.set_mute(fsm_dev, mute);
	}
	ret = fsm_config_vol(fsm_dev);
	ret |= fsm_set_bf(fsm_dev, FSM_AMPE, !mute);
	fsm_dev->errcode = ret;

	return ret;
}

int fsm_stub_shut_down(fsm_dev_t *fsm_dev)
{
	uint16_t sysctrl;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.shut_down) {
		return fsm_dev->dev_ops.shut_down(fsm_dev);
	}
	// fsm_reg_multiread(fsm_dev, REG(FSM_DIGSTAT), NULL); // TODO
	// ret = fsm_config_vol(fsm_dev);
	ret = fsm_reg_read(fsm_dev, REG(FSM_SYSCTRL), &sysctrl);
	// amplifier off and power down device
	// set_bf_val(&sysctrl, FSM_AMPE, 0);
	set_bf_val(&sysctrl, FSM_PWDN, 1);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl);

	FSM_FUNC_EXIT(ret);
	return ret;
}
static int fsm_stub_store_otp(fsm_dev_t *fsm_dev, uint8_t valOTP)
{
	if (fsm_dev && fsm_dev->dev_ops.store_otp) {
		return fsm_dev->dev_ops.store_otp(fsm_dev, valOTP);
	} else {
		pr_addr(err, "not support");
		return -EINVAL;
	}
}

int fsm_stub_pre_calib(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int force;
	int ret;

	if (fsm_dev == NULL || !cfg) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.pre_calib) {
		return fsm_dev->dev_ops.pre_calib(fsm_dev);
	}
	force = cfg->force_calib;
	pr_addr(info, "force:%d, calibrated:%d",
			force, fsm_dev->state.calibrated);
	if (!force && fsm_dev->state.calibrated) {
		pr_addr(info, "already calibrated");
		return 0;
	}
	fsm_dev->state.calibrated = false;
	fsm_dev->re25 = 0;
	if (fsm_dev->tdata) {
		fsm_dev->tdata->test_re25 = 0;
		memset(&fsm_dev->tdata->re25, 0, sizeof(struct re25_data));
	}
	ret = fsm_set_cal_mode(fsm_dev);
	ret |= fsm_check_status(fsm_dev);
	if (ret) {
		pr_addr(err, "error:%d, stop test", ret);
		fsm_dev->state.re25_runin = false;
		return ret;
	}
	fsm_dev->state.re25_runin = true;

	return ret;
}

int fsm_stub_post_calib(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	uint8_t valOTP;
	uint16_t cal_zm;
	int ret;

	if (fsm_dev == NULL || !cfg || !fsm_dev->tdata) {
		return -EINVAL;
	}
	if (!fsm_dev->state.re25_runin) {
		pr_addr(info, "invalid running state");
		return 0;
	}
	if (fsm_dev->dev_ops.post_calib) {
		return fsm_dev->dev_ops.post_calib(fsm_dev);
	}
	fsm_dev->state.re25_runin = false;
	cal_zm = fsm_dev->tdata->re25.zmdata;
	// calculate spk-t thrd and re25
	ret = fsm_access_key(fsm_dev, 1);
	ret |= fsm_set_threshold(fsm_dev, cal_zm, FSM_DATA_TYPE_ZMDATA);
	fsm_dev->tdata->test_re25 = fsm_dev->re25;
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ZMCONFIG), 0x0000);
	ret |= fsm_access_key(fsm_dev, 0);
	if (ret) {
		return ret;
	}
	valOTP = fsm_re25_to_byte(fsm_dev, fsm_dev->re25);

	if (valOTP != 0xFF && cfg->store_otp) {
		ret |= fsm_stub_store_otp(fsm_dev, valOTP);
		if (ret) {
			pr_addr(err, "store otp fail:%d", ret);
			fsm_dev->tdata->test_re25 = fsm_dev->re25 = 65535;
		}
	}
	// ret |= fsm_config_vol(fsm_dev); // move to fsm_stub_set_mute
	fsm_dev->errcode = ret;

	FSM_FUNC_EXIT(ret);
	return ret;
}

int fsm_stub_dev_deinit(fsm_dev_t *fsm_dev)
{
	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.deinit) {
		return fsm_dev->dev_ops.deinit(fsm_dev);
	}
	// deinit here

	return 0;
}

int fsm_dev_recover(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret;

	if (!cfg || !fsm_dev) {
		return -EINVAL;
	}
	if (fsm_skip_device(fsm_dev)) {
		return 0;
	}
	if (fsm_dev->dev_ops.dev_recover) {
		return fsm_dev->dev_ops.dev_recover(fsm_dev);
	}
	ret = fsm_check_status(fsm_dev);
	if (!ret) {
		return ret;
	}
	pr_addr(info, "recover device");
	cfg->force_init = true;
	cfg->force_scene = true;
	ret |= fsm_stub_dev_init(fsm_dev);
	ret |= fsm_stub_start_up(fsm_dev);
	fsm_wait_stable(fsm_dev, FSM_WAIT_AMP_ON);
	ret |= fsm_stub_set_mute(fsm_dev, false);
	fsm_wait_stable(fsm_dev, FSM_WAIT_AMP_ADC_ON);
	ret |= fsm_stub_set_tsignal(fsm_dev, true);
	cfg->force_init = false;
	cfg->force_scene = false;

	return ret;
}

int fsm_dev_count(void)
{
	return g_fsm_config.dev_count;
}

void fsm_set_i2s_clocks(uint32_t rate, uint32_t bclk)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg) {
		return;
	}
	fsm_mutex_lock();
	cfg->i2s_bclk = bclk;
	cfg->i2s_srate = rate;
	if (rate == 32000) {
		cfg->i2s_bclk += 32;
	}
	fsm_mutex_unlock();
}

void fsm_set_scene(int scene)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	bool state;

	if (!cfg || scene < 0 || scene >= FSM_SCENE_MAX) {
		pr_info("invaild scene:%d", scene);
		return;
	}
	fsm_mutex_lock();
	if (cfg->speaker_on) {
		// switch scene online
		state = cfg->skip_monitor;
		cfg->skip_monitor = true;
		// turn off devices firstly
		fsm_list_func_arg(fsm_dev, fsm_stub_set_tsignal, false);
		fsm_list_wait(FSM_WAIT_TSIGNAL_OFF);
		fsm_list_func_arg(fsm_dev, fsm_stub_set_mute, true);
		fsm_list_func(fsm_dev, fsm_stub_shut_down);
		fsm_list_wait(FSM_WAIT_AMP_ADC_PLL_OFF);
		// switch preset by scene
		cfg->next_scene = BIT(scene);
		fsm_list_func(fsm_dev, fsm_switch_preset);
		// turn on devices by scene
		fsm_list_func(fsm_dev, fsm_stub_start_up);
		fsm_list_wait(FSM_WAIT_AMP_ON);
		fsm_list_func(fsm_dev, fsm_get_vbat);
		fsm_list_func_arg(fsm_dev, fsm_stub_set_mute, false);
		fsm_list_wait(FSM_WAIT_AMP_ADC_ON);
		fsm_delay_ms(10);
		fsm_list_func_arg(fsm_dev, fsm_stub_set_tsignal, true);
		cfg->skip_monitor = state;
	} else {
		// swtich scene offline
		cfg->next_scene = BIT(scene);
		fsm_list_func(fsm_dev, fsm_switch_preset);
	}
	fsm_mutex_unlock();
}

void fsm_set_volume(int volume)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;

	if (!cfg) {
		return;
	}
	fsm_mutex_lock();
	if (volume < 0 || volume > FSM_VOLUME_MAX) {
		pr_info("invalid volume: %d, default 0dB", volume);
		volume = FSM_VOLUME_MAX;
	}
	cfg->volume = volume;
	if (!cfg->stream_muted) {
		fsm_list_func(fsm_dev, fsm_config_vol);
	}
	fsm_mutex_unlock();
}

void fsm_set_fw_name(char *name)
{
	fsm_config_t *cfg = fsm_get_config();

	if (cfg) {
		cfg->fw_name = name;
	}
}

int fsm_detect_device(fsm_dev_t *fsm_dev, uint8_t dev_id)
{
	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	fsm_dev->use_irq = false;
	switch (dev_id) {
	case FS1601S_DEV_ID:
		g_fsm_config.fw_name = "fs1601s.fsm";
		fs1601s_ops(fsm_dev);
		break;
	case FS1603_DEV_ID:
		g_fsm_config.fw_name = "fs1603.fsm";
		fs1603_ops(fsm_dev);
		break;
	case FS1818_DEV_ID:
	case FS1896_DEV_ID:
		g_fsm_config.fw_name = "fs1801.fsm";
		fs1801_ops(fsm_dev);
		break;
	case FS1860_DEV_ID:
		g_fsm_config.fw_name = "fs1860.fsm";
		fs1860_ops(fsm_dev);
		break;
	default:
		pr_addr(err, "invalid dev_id:%02X", dev_id);
		memset(&fsm_dev->dev_ops, 0, sizeof(fsm_dev->dev_ops));
		return -EINVAL;
	}

	return 0;
}

int fsm_probe(fsm_dev_t *fsm_dev, int addr)
{
	uint16_t id;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}

	fsm_mutex_lock();
	fsm_dev->addr = addr;
	ret = fsm_reg_read(fsm_dev, REG(FSM_ID), &id);
	if (ret) {
		fsm_dev->addr = 0;
		fsm_mutex_unlock();
		return ret;
	}
	ret = fsm_detect_device(fsm_dev, HIGH8(id));
	if (ret) {
		pr_addr(err, "Unknown DEVID: %04X", id);
		fsm_dev->addr = 0;
		fsm_mutex_unlock();
		return ret;
	}
	pr_addr(info, "Found DEVID: %04X", id);
	fsm_dev->ram_scene[FSM_EQ_RAM0] = FSM_SCENE_UNKNOW;
	fsm_dev->ram_scene[FSM_EQ_RAM1] = FSM_SCENE_UNKNOW;
	fsm_dev->own_scene = FSM_SCENE_UNKNOW;
	fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
	fsm_dev->version = id;
	fsm_dev->is_1894s = ((id == FS1894S_REVID) ? true : false);
	fsm_dev->acc_count = 0;

	fsm_list_init(fsm_dev);
	g_fsm_config.dev_count++;
	fsm_mutex_unlock();

	return 0;
}

void fsm_remove(fsm_dev_t *fsm_dev)
{
	if (!fsm_dev) {
		return;
	}
	fsm_mutex_lock();
#if defined(CONFIG_FSM_STEREO)
	list_del(&fsm_dev->list);
#endif
	if (fsm_dev->tdata) {
		fsm_free_mem(fsm_dev->tdata);
	}
	g_fsm_config.dev_count--;
	fsm_mutex_unlock();
}

/**
 * try init deivce without lock
 */
static int fsm_try_init(void)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	uint16_t next_scene;
	int ret;

	if (fsm_dev_count() <= 0) {
		pr_info("no found device");
		return -EINVAL;
	}
	cfg->force_scene = cfg->force_init;
	ret = fsm_firmware_init_sync(cfg->fw_name);
	if (!fsm_get_presets()) {
		pr_info("invalid firmware, ret:%d", ret);
		return -EINVAL;
	}
	next_scene = cfg->next_scene;
	cfg->next_scene = FSM_SCENE_MUSIC; // init all devices
	list_for_each_entry(fsm_dev, &fsm_dev_list, list) {
		fsm_stub_dev_init(fsm_dev);
	}
	cfg->next_scene = next_scene; // recovery scene
	cfg->force_scene = cfg->force_init = false;

	return 0;
}

void fsm_init(void)
{
	int ret;

	ret = fsm_hal_open();
	if (ret) {
		pr_info("hal open fail:%d", ret);
		return;
	}
	fsm_mutex_lock();
	ret = fsm_try_init();
	if (ret) { // no device or firmware
		pr_info("init failed: %d", ret);
	}
	pr_debug("done");
	fsm_mutex_unlock();
}

void fsm_speaker_onn(void)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsm_vbat_state *vbat_state;
	fsm_dev_t *fsm_dev = NULL;
	int ret;

	pr_info("scene: %04X", cfg->next_scene);
	fsm_mutex_lock();
	cfg->stream_muted = false;
	ret = fsm_try_init();
	if (ret) { // no device or firmware
		pr_info("init failed: %d", ret);
		fsm_mutex_unlock();
		return;
	}
	// fsm_list_func(fsm_dev, fsm_switch_preset);
	fsm_list_func(fsm_dev, fsm_stub_start_up);
	fsm_list_wait(FSM_WAIT_AMP_ON);
	vbat_state = &g_vbat_state;
	vbat_state->cur_batv = 0;
	fsm_list_func(fsm_dev, fsm_get_vbat);
	pr_info("vbat:%d", vbat_state->cur_batv);
	fsm_list_func_arg(fsm_dev, fsm_stub_set_mute, false);
	vbat_state->last_batv = vbat_state->cur_batv;
	fsm_list_wait(FSM_WAIT_AMP_ADC_ON);
	fsm_delay_ms(10);
	fsm_list_func_arg(fsm_dev, fsm_stub_set_tsignal, true);
	// cfg->skip_monitor = false; // not use fsm_set_monitor
	// fsm_list_func(fsm_dev, fsm_set_monitor);
	cfg->cur_angle = cfg->next_angle;
	cfg->speaker_on = true;
	pr_debug("done");
	fsm_mutex_unlock();
}

void fsm_speaker_off(void)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	int ret;

	pr_info("scene: %04X", cfg->next_scene);
	fsm_mutex_lock();
	// cfg->skip_monitor = true; // not use fsm_set_monitor
	cfg->stream_muted = true;
	// fsm_list_func(fsm_dev, fsm_set_monitor);
	ret = fsm_try_init();
	if (ret) {
		pr_info("try init failed: %d", ret);
		fsm_mutex_unlock();
		return;
	}
	fsm_list_func_arg(fsm_dev, fsm_stub_set_tsignal, false);
	fsm_list_wait(FSM_WAIT_TSIGNAL_OFF);
	fsm_list_func_arg(fsm_dev, fsm_stub_set_mute, true);
	fsm_list_func(fsm_dev, fsm_stub_shut_down);
	fsm_list_wait(FSM_WAIT_AMP_ADC_PLL_OFF);
	cfg->speaker_on = false;
	pr_debug("done");
	fsm_mutex_unlock();
}

void fsm_stereo_flip(int next_angle)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;

	if (next_angle >= 360) { // angle range: [0 ~ 359]
		next_angle %= 360;
	}
	if (cfg->cur_angle == next_angle) {
		return;
	}
	fsm_mutex_lock();
	pr_info("scene:%04X, angle:%d", cfg->next_scene, next_angle);
	cfg->next_angle = next_angle;
	if (cfg->stream_muted || !(cfg->next_scene & FSM_SCENE_MUSIC)) {
		fsm_mutex_unlock();
		return;
	}
	fsm_list_func_arg(fsm_dev, fsm_swap_channel, cfg->next_angle);
	cfg->cur_angle = cfg->next_angle;
	fsm_mutex_unlock();
}

void fsm_batv_monitor(void)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsm_vbat_state *vbat_state;
	fsm_dev_t *fsm_dev = NULL;

	vbat_state = &g_vbat_state;
	fsm_mutex_lock();
	if (cfg->skip_monitor  || cfg->dev_suspend || !vbat_state) {
		fsm_mutex_unlock();
		return;
	}
	vbat_state->cur_batv = 0;
	// get current batv(min) from smartpa (or from charge ic)
	fsm_list_func(fsm_dev, fsm_get_vbat);
	// update volume when:
	// 1) cur_vbat lower last_batv - step
	// 2) vbat rised FSM_VBATH_COUNT times (charging)
	// 3) the first time checking (last_batv = 0)
	pr_debug("batv: %d, %d, %d", vbat_state->last_batv,
			vbat_state->cur_batv, vbat_state->batvh_count);
	if ((vbat_state->cur_batv <= vbat_state->last_batv - FSM_VBAT_STEP)
			|| (vbat_state->batvh_count >= FSM_VBATH_COUNT)
			|| (vbat_state->last_batv == 0)) {
		// update volume
		fsm_list_func(fsm_dev, fsm_config_vol);
		// update last batv
		vbat_state->last_batv = vbat_state->cur_batv;
		// clear batv rised count
		vbat_state->batvh_count = 0;
	} else if (vbat_state->cur_batv >= vbat_state->last_batv + FSM_VBAT_STEP) {
		// batv rised
		vbat_state->batvh_count++;
	}
	fsm_mutex_unlock();
}

void fsm_re25_test(bool force)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	int retry;
	int ret;

	fsm_mutex_lock();
	pr_info("start testing");
	cfg->skip_monitor = true;
	cfg->test_type = TEST_RE25;
	cfg->force_calib = force;
	cfg->stop_test = false;
	fsm_mutex_unlock();
	// check play audio or not
	ret = fsm_list_wait(FSM_WAIT_STATUS_ON);
	if (ret) {
		pr_info("wait status timeout!");
		// return; // go ahead to clean data in pre calib
	}
	fsm_mutex_lock();
	fsm_list_func(fsm_dev, fsm_stub_pre_calib);
	fsm_delay_ms(2500); // 2500ms
	for (retry = 1; retry <= FSM_WAIT_STABLE_RETRY; retry++) {
		if (cfg->stop_test) {
			break;
		}
		fsm_delay_ms(100); // 100ms
		fsm_list_return(fsm_dev, fsm_cal_zmdata, ret);
		if (!ret) {
			break;
		}
	}
	pr_info("retry %d times, max:%d", retry, FSM_WAIT_STABLE_RETRY);
	fsm_list_func(fsm_dev, fsm_stub_post_calib);
	cfg->test_type = TEST_NONE;
	fsm_list_func_arg(fsm_dev, fsm_stub_set_mute, false);
	fsm_list_wait(FSM_WAIT_AMP_ADC_ON);
	fsm_delay_ms(10); // ms
	fsm_list_func_arg(fsm_dev, fsm_stub_set_tsignal, true);
	cfg->force_calib = false;
	cfg->skip_monitor = false;
	pr_debug("done");
	fsm_mutex_unlock();
}

void fsm_f0_test(void)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	int freq;
	int ret;

	fsm_mutex_lock();
	pr_info("start testing");
	cfg->skip_monitor = true;
	cfg->test_type = TEST_F0;
	cfg->stop_test = false;
	fsm_mutex_unlock();
	ret = fsm_list_wait(FSM_WAIT_STATUS_ON);
	if (ret) {
		pr_info("wait status timeout!");
		// return; // go ahead to clean data in pre f0 test
	}
	fsm_mutex_lock();
	fsm_list_entry(fsm_dev, fsm_dev->dev_ops.pre_f0_test);
	for (freq = FREQ_START; freq <= FREQ_END; freq += F0_FREQ_STEP) {
		if (cfg->stop_test) {
			break;
		}
		cfg->test_freq = freq;
		fsm_list_entry(fsm_dev, fsm_dev->dev_ops.f0_test);
		fsm_delay_ms((freq != FREQ_START ? F0_TEST_DELAY_MS : 800)); // ms
		fsm_list_return(fsm_dev, fsm_cal_zmdata, ret);
	}
	fsm_list_entry(fsm_dev, fsm_dev->dev_ops.post_f0_test);
	fsm_delay_ms(35); // 35ms
	cfg->test_type = TEST_NONE;
	// force init device
	cfg->force_init = true;
	cfg->force_scene = true;
	pr_debug("done");
	fsm_mutex_unlock();
	fsm_speaker_onn();
	cfg->force_init = false;
	cfg->force_scene = false;
}

int fsm_test_result(struct fsm_cal_result *result)
{
	struct preset_file *pfile;
	struct dev_list *dev_list;
	fsm_dev_t *fsm_dev;
	uint8_t *preset;
	int dev;

	pfile = fsm_get_presets();
	if (!result || !pfile) {
		pr_info("invalid parameters");
		return -EINVAL;
	}
	preset = (uint8_t *)pfile;
	result->ndev = pfile->hdr.ndev;
	result->freq_start = FREQ_START;
	result->freq_end = FREQ_END;
	result->freq_step = F0_FREQ_STEP;
	if (pfile->hdr.ndev > FSM_DEV_MAX) {
		pr_info("invalid ndev:%d", pfile->hdr.ndev);
		return -EINVAL;
	}
	for (dev = 0; dev < result->ndev; dev++) {
		if (FSM_DSC_DEV_INFO != pfile->index[dev].type) {
			continue;
		}
		dev_list = (struct dev_list *)&preset[pfile->index[dev].offset];
		result->data[dev].addr = dev_list->addr;
		fsm_dev = fsm_get_fsm_dev(dev_list->addr);
		if (fsm_dev && fsm_dev->tdata) {
			result->data[dev].pos = fsm_dev->pos_mask;
			result->data[dev].re25 = fsm_dev->tdata->test_re25;
			result->data[dev].count = fsm_dev->cal_count;
			result->data[dev].f0 = fsm_dev->f0;
			memcpy(result->data[dev].f0_zm, fsm_dev->tdata->f0.zmdata,
					sizeof(result->data[dev].f0_zm));
			result->data[dev].errcode = fsm_dev->errcode;
		} else {
			pr_addr(err, "invalid data");
		}
	}
	pr_info("result->ndev:%d", result->ndev);

	return 0;
}

void fsm_deinit(void)
{
	fsm_dev_t *fsm_dev = NULL;

	fsm_mutex_lock();
	if (fsm_dev_count() <= 0) {
		fsm_mutex_unlock();
		return;
	}
	fsm_list_func(fsm_dev, fsm_stub_dev_deinit);
	fsm_firmware_deinit();
	fsm_mutex_unlock();
	fsm_hal_close();
}
