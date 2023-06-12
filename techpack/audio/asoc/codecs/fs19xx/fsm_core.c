/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-17 File created.
 */

#include "fsm_public.h"

#define CRC16_TABLE_SIZE      256
#define CRC16_POLY_NOMIAL     0xA001

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
	} while(0)

#define fsm_list_func(fsm_dev, func) \
	do { \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev)) { \
				func(fsm_dev); \
			} \
		} \
	} while(0)

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
	} while(0)

#define fsm_list_return(fsm_dev, func, ret) \
	do { \
		ret = 0; \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev)) { \
				ret |= func(fsm_dev); \
			} \
		} \
	} while(0)

#define fsm_list_func_arg(fsm_dev, func, argv) \
	do { \
		list_for_each_entry(fsm_dev, &fsm_dev_list, list) { \
			if (!fsm_skip_device(fsm_dev)) { \
				func(fsm_dev, argv); \
			} \
		} \
	} while(0)


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
	.freq_start = FREQ_START,
	.freq_end   = FREQ_END,
	.freq_step  = F0_FREQ_STEP,
	.freq_count = FREQ_COUNT,
	.amb_tempr  = FSM_DFT_AMB_TEMPR,
	.cur_angle  = 0,
	.next_angle = 0,

	// flags
	.vddd_on = 0,
	.codec_inited = 0,
	.force_fw = 0,
	.force_init = 0,
	.force_scene = 0,
	.force_calib = 0,
	.store_otp = 0,
	.nondsp_mode = 1,
#ifdef FSM_DEBUG
	.i2c_debug = 1,
#endif
	.force_mute = 0,
	.stop_test = 0,
	.use_monitor = 0,
	.skip_monitor = 0,
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

int fsm_skip_device(fsm_dev_t *fsm_dev)
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

fsm_config_t *fsm_get_config(void)
{
	return &g_fsm_config;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(fsm_get_config);
#endif

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

void fsm_free_mem(void **buf)
{
	if (*buf == NULL) {
		return;
	}

#if defined(__KERNEL__)
	kfree(*buf);
#elif defined(FSM_HAL_SUPPORT)
	free(*buf);
#endif
	*buf = NULL;
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
		pr_err("get bf:%04X failed", bf);
		return ret;
	}

	msk = ((1 << (reg.len + 1)) - 1) << reg.pos;
	reg.value = oldval & (~msk);
	reg.value |= val << reg.pos;

	if(oldval == reg.value) {
		return 0;
	}
	ret = fsm_reg_write(fsm_dev, reg.addr, reg.value);
	if (ret) {
		pr_err("set bf:%04X failed", bf);
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
		pr_err("get bf:%04X failed", bf);
		return ret;
	}
	msk = ((1 << (reg.len + 1)) - 1) << reg.pos;
	reg.value &= msk;
	if(pval) {
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

struct fsm_dev *fsm_get_fsm_dev_by_id(int id)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;

	if (!cfg || cfg->dev_count <= 0) {
		return NULL;
	}

	list_for_each_entry(fsm_dev, &fsm_dev_list, list) {
		if (fsm_dev && id == fsm_dev->id) {
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
	if (g_fsm_config.i2c_debug) {
		pr_addr(info, "%02X<-%04X", reg, val);
	}
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
	if (g_fsm_config.i2c_debug) {
		pr_addr(info, " %02X->%04X", reg, value);
	}
#endif
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

int fsm_burst_write(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *data, int len)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
#if defined(FSM_DEBUG_I2C)
	if (len >= 4) {
		if (g_fsm_config.i2c_debug) {
			pr_addr(info, "%02X<-%02X %02X %02X %02X", reg,
				data[0], data[1], data[2], data[3]);
		}
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

int fsm_reg_case(fsm_dev_t *fsm_dev, reg_unit_t *reg)
{
	reg_wait_t *reg_wait;
	uint16_t temp;
	uint16_t mask;
	int count;
	int ret = 0;

	if (!fsm_dev || !reg) {
		return -EINVAL;
	}
	switch (reg->len) {
	case 1: // delay ms
		fsm_delay_ms(reg->value);
		break;
	case 2: // wait bits, 8bits max
		reg_wait = (reg_wait_t *)&reg->value;
		if (reg_wait->len >= 8) {
			pr_addr(err, "invalid len:%d", reg_wait->len);
			return -EINVAL;
		}
		mask = ((1 << (reg_wait->len + 1)) - 1);
		for (count = 0; count < 35; count++) {
			fsm_delay_ms(1);
			ret = fsm_reg_multiread(fsm_dev, reg->addr, &temp);
			temp = (temp >> reg_wait->pos) & (~mask);
			if (temp == reg_wait->val)
				break;
		}
		break;
	default:
		break;
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
	if (reg->pos == 0xF && reg->len > 0) {
		return fsm_reg_case(fsm_dev, reg);
	}
	addr = reg->addr;
	if (reg->pos == 0 && reg->len == 15) {
		return fsm_reg_write(fsm_dev, addr, reg->value);
	}
	mask = ((1 << (reg->len + 1)) - 1) << reg->pos;
	val = (reg->value << reg->pos);
	ret = fsm_reg_multiread(fsm_dev, addr, &temp);
	temp = ((~mask & temp) | (val & mask));
	ret |= fsm_reg_write(fsm_dev, addr, temp);

	return ret;
}

int fsm_reg_update(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val)
{
	uint16_t temp;
	int ret;

	ret = fsm_reg_multiread(fsm_dev, reg, &temp);
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

	if (!fsm_dev) {
		return -EINVAL;
	}
	if (srate > 48000 && HIGH8(fsm_dev->version) != FS1860_DEV_ID) {
		pr_addr(err, "invalid srate:%d", srate);
		return -EINVAL;
	}
	if (srate == 32000 && fsm_dev->is1603s) {
		return 6; // I2SSR=6
	}
	size = sizeof(g_srate_tbl)/ sizeof(struct fsm_srate);
	for (idx = 0; idx < size; idx++) {
		if (srate == g_srate_tbl[idx].srate)
			return g_srate_tbl[idx].bf_val;
	}
	return -EINVAL;
}

int fsm_access_key(fsm_dev_t *fsm_dev, int access)
{
	uint8_t otpacc;
	int ret = 0;

	if (!fsm_dev) {
		return -EINVAL;
	}
	if (fsm_dev->is1820) {
		otpacc = 0x1F;
	} else {
		otpacc = 0x0B;
	}
	ret = fsm_reg_write(fsm_dev, otpacc, (access ? FSM_OTP_ACC_KEY2 : 0));

	return ret;
}

int fsm_reg_dump(fsm_dev_t *fsm_dev)
{
	char buf[LOG_BUF_SIZE];
	uint16_t value;
	int reg_addr;
	int reg_end;
	int idx = 0;
	int ret = 0;

	if (!fsm_dev) {
		return -EINVAL;
	}
	reg_end = (fsm_dev->is1958 ? 0xEF : (fsm_dev->is1820 ? 0xEA : 0xE8));
	for (reg_addr = 0; reg_addr <= reg_end; reg_addr++) {
		if (fsm_dev->is1958) {
			if (reg_addr == 0x60) reg_addr = 0x80;
			else if (reg_addr == 0x81) reg_addr = 0xA1;
			else if (reg_addr == 0xD0)
				ret |= fsm_access_key(fsm_dev, 1);
		}
		if (fsm_dev->is1820) {
			if (reg_addr == 0x60) reg_addr = 0x80;
		}
		else if (reg_addr == 0x10) reg_addr = 0x40;
		else if (reg_addr == 0xD0) {
			ret |= fsm_access_key(fsm_dev, 1);
		}
		ret |= fsm_reg_read(fsm_dev, reg_addr, &value);
		snprintf(buf+idx*8, 9, "%02X:%04X ", reg_addr, value);
		idx++;
		if (idx == 8 || reg_addr == reg_end) {
			buf[idx*8-1] = '\0';
			pr_addr(info, "%s", buf);
			idx = 0;
		}
	}
	ret |= fsm_access_key(fsm_dev, 0);

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
	if (fsm_dev->is1820) {
		if (LOW8(fsm_dev->version) != HIGH8(dev_type)) {
			pr_addr(err, "type:%02X not match version:%02X",
					HIGH8(dev_type), HIGH8(fsm_dev->version));
			return -EINVAL;
		}
		return 0;
	}
	if (HIGH8(fsm_dev->version) == 0x06 || HIGH8(fsm_dev->version) == 0x0B) {
		// fs1801 series
		if (HIGH8(dev_type) != 0x0A && HIGH8(dev_type) != 0x06
				&& HIGH8(dev_type) != 0x0B) {
			pr_addr(err, "invalid type:%04X", dev_type);
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

void *fsm_get_presets(void)
{
	return g_fsm_config.preset;
}

void fsm_set_presets(void *preset)
{
	fsm_config_t *cfg = fsm_get_config();

	if (cfg) {
		fsm_free_mem((void **)&cfg->preset);
		cfg->preset = preset;
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
		pr_err("bad parameter, size:%d", size);
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
		pr_err("alloc memery failed");
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
		pr_err("invalid size: hdr:%d, fw:%d", hdr->size, size);
		fsm_free_mem((void **)&pfile);
		return -EINVAL;
	}
	checksum = fsm_calc_checksum((uint16_t *)(&(pfile->hdr.ndev)), crc_size);
	if (checksum != hdr->crc16) {
		pr_err("checksum(%04X) not match(%04X)", checksum, hdr->crc16);
		fsm_free_mem((void **)&pfile);
		return -EINVAL;
	}
	else {
		pr_info("checksum success!");
		fsm_set_presets(pfile);
	}
	fsm_try_init();

	return 0;
}

int fsm_swap_channel(fsm_dev_t *fsm_dev, int next_angle)
{
	uint16_t left_chn;
	uint8_t i2sctrl;
	uint16_t chs12;
	int ret = 0;

	if (!fsm_dev) {
		return -EINVAL;
	}
	switch (next_angle) {
	case 90:
		left_chn = (FSM_POS_LBTM | FSM_POS_RBTM);
		break;
	case 180:
		left_chn = (FSM_POS_RTOP | FSM_POS_RBTM);
		break;
	case 270:
		left_chn = (FSM_POS_LTOP | FSM_POS_RTOP);
		break;
	case 0:
	default:
		left_chn = (FSM_POS_LTOP | FSM_POS_LBTM);
		break;
	}
	left_chn = (left_chn & fsm_dev->pos_mask);
	chs12 = ((left_chn != 0) ? 1 : 2);
	if ((g_fsm_config.dev_count == 1)
			|| (fsm_dev->pos_mask == FSM_POS_MONO)) {
		chs12 = 3;
	}
	if (fsm_dev->is1958) {
		i2sctrl = 0x17;
		ret = fsm_set_bf(fsm_dev, 0x1317, chs12); // 0x17[4..3]
	} else {
		i2sctrl = 0x04;
		ret = fsm_set_bf(fsm_dev, 0x1304, chs12); // 0x04[4..3]
	}
	pr_addr(debug, "pos:%02X, CHS12:%d", fsm_dev->pos_mask, chs12);

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

int fsm_get_rstrim(fsm_dev_t *fsm_dev)
{
	uint16_t reg_rstrim;
	uint16_t value;
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	if (fsm_dev->is1958) {
		fsm_dev->rstrim = 0;
		return 0;
	} else {
		reg_rstrim = 0xE6;
	}
	if (fsm_dev->rstrim != 0) {
		return 0;
	}
	fsm_access_key(fsm_dev, 1);
	ret = fsm_reg_read(fsm_dev, reg_rstrim, &value);
	fsm_access_key(fsm_dev, 0);
	if (ret || value == 0) {
		pr_warning("use default rs_trim value");
		value = FSM_RS_TRIM_DEFAULT;
	}
	fsm_dev->rstrim = value;

	return 0;
}

int fsm_check_spkcoef(fsm_dev_t *fsm_dev)
{
	uint8_t spkcoef;
	uint16_t value;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	spkcoef = (fsm_dev->is1958 ? 0x14 : 0x08);
	ret = fsm_reg_read(fsm_dev, spkcoef, &value);
	if (value == 0) {
		// not inited yet
		return -EINVAL;
	}

	return ret;
}

int fsm_config_vol(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	uint16_t volctrl;
	uint16_t volume;

	if (!fsm_dev || !cfg) {
		return -EINVAL;
	}

	if (cfg->volume > FSM_VOLUME_MAX) {
		cfg->volume = FSM_VOLUME_MAX;
	}
	// volume = (fsm_dev->state.calibrated ? cfg->volume : 0xDF); // -12dB
	if (fsm_dev->is1958) {
		volctrl = 0x16;
		volume = ((cfg->volume << 1) + 1) << 7;
	} else {
		volctrl = 0x06;
		volume = ((cfg->volume << 8) & 0xFF00);
	}
	pr_addr(debug, "vol: %04X", volume);

	return fsm_reg_write(fsm_dev, volctrl, volume);
}

int fsm_ops_dummy(fsm_dev_t *fsm_dev)
{
	return 0;
}

/*
 * fsm stub api
 */

int fsm_stub_switch_preset(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret = 0;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	pr_addr(debug, "%s switching",
			(cfg->force_scene ? "force" : "auto"));
	if (!cfg->force_scene && fsm_dev->cur_scene == cfg->next_scene) {
		pr_addr(debug, "same scene, skip");
		return 0;
	}
	if (cfg->speaker_on) {
		ret |= fsm_stub_shut_down(fsm_dev);
		fsm_delay_ms(30);
	}
	if ((fsm_dev->own_scene & cfg->next_scene) == 0) {
		return ret;
	}
	if (fsm_dev->dev_ops.switch_preset) {
		ret |= fsm_dev->dev_ops.switch_preset(fsm_dev);
	}
	ret |= fsm_write_reg_tbl(fsm_dev, cfg->next_scene);
	fsm_dev->cur_scene = cfg->next_scene;
	if (cfg->speaker_on) {
		ret |= fsm_stub_start_up(fsm_dev);
	}

	FSM_FUNC_EXIT(ret);
	return ret;
}

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
	ret = fsm_check_spkcoef(fsm_dev);
	if (ret) {
		pr_addr(info, "try init device");
		fsm_dev->state.dev_inited = false;
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
		ret |= fsm_dev->dev_ops.reg_init(fsm_dev);
	}
	ret |= fsm_get_rstrim(fsm_dev);
	ret |= fsm_stub_shut_down(fsm_dev);
	fsm_dev->errcode = ret;

	return ret;
}

int fsm_stub_start_up(fsm_dev_t *fsm_dev)
{
	int ret = 0;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.start_up) {
		return fsm_dev->dev_ops.start_up(fsm_dev);
	}
	pr_addr(err, "none ops");

	return ret;
}

int fsm_stub_shut_down(fsm_dev_t *fsm_dev)
{
	int ret = 0;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.shut_down) {
		return fsm_dev->dev_ops.shut_down(fsm_dev);
	}
	pr_addr(err, "none ops");

	return ret;
}

int fsm_stub_pre_calib(fsm_dev_t *fsm_dev)
{
	int ret = 0;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->dev_ops.pre_calib) {
		return fsm_dev->dev_ops.pre_calib(fsm_dev);
	}
	pr_addr(err, "none ops");

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
	pr_addr(err, "none ops");

	return 0;
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

	if (!cfg || scene < 0 || scene > FSM_SCENE_MAX) {
		pr_err("invaild scene:%d", scene);
		return;
	}
	fsm_mutex_lock();
	if (scene == 16) {
		pr_info("Note: calib scene id: %d", scene);
		cfg->force_calib = 1;
		scene = 0; // music scene for calibration
	} else {
		cfg->force_calib = 0; // exit calibration
	}
	cfg->next_scene = BIT(scene);
	list_for_each_entry(fsm_dev, &fsm_dev_list, list) {
		if (!fsm_dev->state.dev_inited)
			continue;
		if (fsm_dev->own_scene & cfg->next_scene)
			continue;
		if (fsm_dev->cur_scene == cfg->next_scene)
			continue;
		fsm_dev->cur_scene = cfg->next_scene;
		if (cfg->speaker_on)
			fsm_stub_shut_down(fsm_dev);
	}
	fsm_list_func(fsm_dev, fsm_stub_switch_preset);
	fsm_mutex_unlock();
	if (cfg->speaker_on) {
		fsm_set_calib_mode();
	}
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
		pr_warning("invalid volume: %d, default 0dB", volume);
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
	memset(&fsm_dev->dev_ops, 0, sizeof(fsm_dev->dev_ops));
	switch (dev_id) {
	case FS1603_DEV_ID:
		fs1603_ops(fsm_dev);
		break;
	case FS1818_DEV_ID:
	case FS1896_DEV_ID:
		fsm_dev->is1801 = true;
		//fs1801_ops(fsm_dev);
		break;
	case FS1958_DEV_ID:
		fsm_dev->is1958 = true;
		fs1958_ops(fsm_dev);
		break;
	default:
		pr_addr(err, "invalid dev_id:%02X", dev_id);
		return -EINVAL;
	}

	return 0;
}

int fsm_probe(fsm_dev_t *fsm_dev, int addr)
{
	uint16_t id = 0;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}

	fsm_mutex_lock();
	fsm_dev->addr = addr;
	do {
		ret = fsm_reg_read(fsm_dev, 0x03, &id);
		if (ret) {
			fsm_dev->addr = 0;
			fsm_mutex_unlock();
			return ret;
		}
		fsm_dev->is1603s = IS_FS1603S(id);
		ret = fsm_detect_device(fsm_dev, HIGH8(id));
		if (ret) {
			pr_addr(err, "Unknown DEVID: %04X", id);
			fsm_dev->addr = 0;
			fsm_mutex_unlock();
			return ret;
		}
	} while (0);
#ifndef BUILD_FSTOOL
	pr_addr(info, "Found DEVID: %04X", id);
#endif
	fsm_dev->ram_scene[FSM_EQ_RAM0] = FSM_SCENE_UNKNOW;
	fsm_dev->ram_scene[FSM_EQ_RAM1] = FSM_SCENE_UNKNOW;
	fsm_dev->own_scene = FSM_SCENE_UNKNOW;
	fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
	fsm_dev->version = id;
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
	list_del(&fsm_dev->list);
	if (fsm_dev->tdata) {
		fsm_free_mem((void **)&fsm_dev->tdata);
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
	struct preset_file *preset;
	fsm_dev_t *fsm_dev;
	uint16_t next_scene;
	int ret;

	if (cfg->dev_count <= 0) {
		pr_err("no found device");
		return -EINVAL;
	}
	cfg->force_scene = cfg->force_init;
	ret = fsm_firmware_init_sync(cfg->fw_name);
	preset = fsm_get_presets();
	if (preset == NULL) {
		pr_err("invalid firmware, ret:%d", ret);
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
		pr_err("hal open fail:%d", ret);
		return;
	}
	fsm_mutex_lock();
	pr_info("version: %s", FSM_CODE_VERSION);
	pr_info("branch : %s", FSM_GIT_BRANCH);
	pr_info("date   : %s", FSM_CODE_DATE);
	ret = fsm_try_init();
	if (ret) { // no device or firmware
		pr_err("init failed: %d", ret);
	}
	pr_debug("done");
	fsm_mutex_unlock();
}

void fsm_speaker_onn(void)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	int ret;

	pr_info("scene: %04X", cfg->next_scene);
	fsm_mutex_lock();
	cfg->stream_muted = false;
	ret = fsm_try_init();
	if (ret) { // no device or firmware
		pr_err("init failed: %d", ret);
		fsm_mutex_unlock();
		return;
	}
	fsm_list_func(fsm_dev, fsm_stub_switch_preset);
	fsm_list_func(fsm_dev, fsm_stub_start_up);
	cfg->cur_angle = cfg->next_angle;
	cfg->speaker_on = true;
	pr_debug("done");
	fsm_mutex_unlock();
	fsm_set_calib_mode();
}

void fsm_speaker_off(void)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	int ret;

	pr_info("scene: %04X", cfg->next_scene);
	fsm_mutex_lock();
	cfg->stream_muted = true;
	ret = fsm_try_init();
	if (ret) {
		pr_err("try init failed: %d", ret);
		fsm_mutex_unlock();
		return;
	}
	fsm_list_func(fsm_dev, fsm_stub_shut_down);
	cfg->speaker_on = false;
	cfg->force_calib = false;
	pr_debug("done");
	fsm_mutex_unlock();
}

void fsm_set_calib_mode(void)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev = NULL;
	int retry = 0;

	if (!cfg->force_calib) {
		return;
	}
	while (!cfg->speaker_on && retry < 50) {
		fsm_delay_ms(10);
		retry++;
	}
	fsm_mutex_lock();
	fsm_list_func(fsm_dev, fsm_stub_pre_calib);
	cfg->next_scene = FSM_SCENE_MUSIC;
	fsm_mutex_unlock();
}

void fsm_stereo_rotation(int next_angle)
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

void fsm_dump(void)
{
	fsm_dev_t *fsm_dev = NULL;
	fsm_mutex_lock();
	fsm_list_func(fsm_dev, fsm_reg_dump);
	fsm_mutex_unlock();
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
