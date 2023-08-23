#include "goodix_ts_core.h"
#include <linux/rtc.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <asm/uaccess.h>


#define TEST_CODE_VERSION			"tp_test_v1.0.0"
/* test switch */
#define TOTAL_FRAME_NUM 			16 /* get rawdata frames */
#define NOISEDATA_TEST_TIMES		10
#define SHORT_TEST
#define NOISE_DATA_TEST
/* #define SELFNOISE_DATA_TEST */
/* #define SELF_RAWDATA_TEST */
#define SAVE_IN_CSV

#define GOODIX_RESULT_SAVE_PATH		"/data/misc/tp_selftest_data/Test_Data.csv"
#define GOODIX_TEST_FILE_NAME		"k9a"
#define MAX_DATA_BUFFER				15000
#define MAX_DRV_NUM					21
#define MAX_SEN_NUM					42
#define MAX_SHORT_NUM				15
#define MAX_LINE_LEN				(1024 * 3 * 5)

#define STATISTICS_DATA_LEN			32
#define MAX_STR_LEN					32
#define MAX_TEST_ITEMS				10 /* 0P-1P-2P-3P-5P total test items */
#define GTP_CAP_TEST				1
#define GTP_DELTA_TEST				2
#define GTP_NOISE_TEST				3
#define GTP_SHORT_TEST				5
#define GTP_SELFCAP_TEST			6
#define GTP_SELFNOISE_TEST			7

#define GTP_TEST_PASS				1
#define GTP_PANEL_REASON			2
#define SYS_SOFTWARE_REASON			3

#define CHN_VDD						0xFF
#define CHN_GND						0x7F
#define DRV_CHANNEL_FLAG			0x80

#define CSV_TP_SPECIAL_RAW_MIN		"specail_raw_min"
#define CSV_TP_SPECIAL_RAW_MAX		"specail_raw_max"
#define CSV_TP_SPECIAL_RAW_DELTA	"special_raw_delta"
#define CSV_TP_SHORT_THRESHOLD		"shortciurt_threshold"
#define CSV_TP_SPECIAL_SELFRAW_MAX	"special_selfraw_max"
#define CSV_TP_SPECIAL_SELFRAW_MIN	"special_selfraw_min"
#define CSV_TP_NOISE_LIMIT			"noise_data_limit"
#define CSV_TP_SELFNOISE_LIMIT		"noise_selfdata_limit"
#define CSV_TP_TEST_CONFIG			"test_config"

#define MAX_TEST_TIME_MS			5000
#define DEFAULT_TEST_TIME_MS		1000
#define MIN_TEST_TIME_MS			800

#define ABS(val)			((val < 0)? -(val) : val)
#define MAX(a, b)			((a > b)? a : b)
#define DRV_TO_DRV_R(selfcode, V2)	(((selfcode) * 63 / (V2) - 1))
#define SEN_TO_SEN_R(selfcode, V2)	(((selfcode) * 63 / (V2) - 1))
#define SEN_TO_DRV_R(selfcode, V2)	(((selfcode) * 63 / (V2) - 1))

#define DRV_TO_AVDD_R(diffcode, avdd) \
	((125 * 1024 * (10 * (avdd) - 125) * 40) / ((diffcode) * 10000) - 40)
#define SEN_TO_AVDD_R(diffcode, avdd) \
	((125 * 1024 * (10 * (avdd) - 125) * 40) / ((diffcode) * 10000) - 40)
#define DRV_TO_GND_R(diffcode)	((64148 / (diffcode)) - 40)
#define SEN_TO_GND_R(diffcode)	((64148 / (diffcode)) - 40)
/*
#define FORMAT_PATH(path, mdir, name, suffix) do{\
	struct timex txc;\
	struct rtc_time tm;\
	do_gettimeofday(&(txc.time));\
	rtc_time_to_tm(txc.time.tv_sec, &tm);\
	sprintf((char*)path, "%s%s_%04d%02d%02d%02d%02d%02d%s", mdir,name,\
		(tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday, tm.tm_hour,\
		tm.tm_min, tm.tm_sec, suffix);\
}while(0)
*/

static u32 gt9897s_short_parms[] = {10, 150, 150, 150, 100, 100, 30};
static u8 gt9897s_drv_map[] = {
		42,43,44,45,46,47,48,49,
		50,51,52,53,54,55,56,57,
		58,59,60,61,62
};
static u8 gt9897s_sen_map[] = {
		0,1,2,3,4,5,6,7,8,9,10,
		11,12,13,14,15,16,17,18,
		19,20,21,22,23,24,25,26,
		27,28,29,30,31,32,33,34,
		35,36,37,38,39,40,41
};

typedef struct __attribute__((packed)) {
	u8 result;
	u8 drv_drv_num;
	u8 sen_sen_num;
	u8 drv_sen_num;
	u8 drv_gnd_avdd_num;
	u8 sen_gnd_avdd_num;
	u16 checksum;
} test_result_t;

struct ts_test_params {
	u32 rawdata_addr;
	u32 noisedata_addr;
	u32 self_rawdata_addr;
	u32 self_noisedata_addr;

	u32 max_drv_num;
	u32 max_sen_num;
	u32 drv_num;
	u32 sen_num;
	u8 *drv_map;
	u8 *sen_map;

	s32 cfg_buf[GOODIX_CFG_MAX_SIZE];
	s32 max_limits[MAX_DRV_NUM * MAX_SEN_NUM];
	s32 min_limits[MAX_DRV_NUM * MAX_SEN_NUM];
	s32 deviation_limits[MAX_DRV_NUM * MAX_SEN_NUM];
	s32 self_max_limits[MAX_DRV_NUM + MAX_SEN_NUM];
	s32 self_min_limits[MAX_DRV_NUM + MAX_SEN_NUM];
	s32 noise_threshold;
	s32 self_noise_threshold;

	u32 short_threshold;
	u32 r_drv_drv_threshold;
	u32 r_drv_sen_threshold;
	u32 r_sen_sen_threshold;
	u32 r_drv_gnd_threshold;
	u32 r_sen_gnd_threshold;
	u32 avdd_value;
};

struct ts_test_rawdata {
	s16 data[MAX_DRV_NUM * MAX_SEN_NUM];
	u32 size;
};

struct ts_test_self_rawdata {
	s16 data[MAX_DRV_NUM + MAX_SEN_NUM];
	u32 size;
};

struct ts_short_res {
	u8 short_num;
	s16 short_msg[4 * MAX_SHORT_NUM];
};

struct ts_open_res {
	u8 beyond_max_limit_cnt[MAX_DRV_NUM * MAX_SEN_NUM];
	u8 beyond_min_limit_cnt[MAX_DRV_NUM * MAX_SEN_NUM];
	u8 beyond_accord_limit_cnt[MAX_DRV_NUM * MAX_SEN_NUM];
};

struct goodix_ts_test {
	struct goodix_ts_core *ts;
	struct ts_test_params test_params;
	struct ts_test_rawdata rawdata[TOTAL_FRAME_NUM];
	struct ts_test_rawdata accord_arr[TOTAL_FRAME_NUM];
	struct ts_test_rawdata noisedata[NOISEDATA_TEST_TIMES];
	struct goodix_ic_config test_config;
	struct ts_test_self_rawdata self_rawdata;
	struct ts_test_self_rawdata self_noisedata;
	struct ts_short_res short_res;
	struct ts_open_res open_res;

	/*[0][0][0][0][0]..  0 without test; 1 pass, 2 panel failed; 3 software failed */
	char test_result[MAX_TEST_ITEMS];
	char test_info[TS_RAWDATA_RESULT_MAX];
};

static int ts_test_reset(struct goodix_ts_test *ts_test,
	u32 delay_ms)
{
	return ts_test->ts->hw_ops->reset(ts_test->ts, delay_ms);
}

static int ts_test_read(struct goodix_ts_test *ts_test,
	u32 addr, u8 *data, u32 len)
{
	return ts_test->ts->hw_ops->read(ts_test->ts, addr, data, len);
}

static int ts_test_write(struct goodix_ts_test *ts_test,
	u32 addr, u8 *data, u32 len)
{
	return ts_test->ts->hw_ops->write(ts_test->ts, addr, data, len);
}

static int ts_test_send_cmd(struct goodix_ts_test *ts_test,
	struct goodix_ts_cmd *cmd)
{
	return ts_test->ts->hw_ops->send_cmd(ts_test->ts, cmd);
}

static int ts_test_irq_enable(struct goodix_ts_test *ts_test,
	bool flag)
{
	return ts_test->ts->hw_ops->irq_enable(ts_test->ts, flag);
}

static int ts_test_send_config(struct goodix_ts_test *ts_test,
	int type)
{
	struct goodix_ic_config *cfg;

	if (type >= GOODIX_MAX_CONFIG_GROUP) {
		ts_err("unsupproted config type %d", type);
		return -EINVAL;
	}
	cfg = ts_test->ts->ic_configs[type];
	if (!cfg || cfg->len <= 0) {
		ts_err("no valid normal config found");
		return -EINVAL;
	}

	return ts_test->ts->hw_ops->send_config(ts_test->ts, cfg->data, cfg->len);
}

#ifdef SHORT_TEST
static int ts_test_read_version(struct goodix_ts_test *ts_test,
	struct goodix_fw_version *version)
{
	return ts_test->ts->hw_ops->read_version(ts_test->ts, version);
}
#endif /* SHROT_TEST */

static void goto_next_line(char **ptr)
{
	do {
		*ptr = *ptr + 1;
	} while (**ptr != '\n' && **ptr != '\0');
	if (**ptr == '\0') {
		return;
	}
	*ptr = *ptr + 1;
}

static void copy_this_line(char *dest, char *src)
{
	char *copy_from;
	char *copy_to;

	copy_from = src;
	copy_to = dest;
	do {
		*copy_to = *copy_from;
		copy_from++;
		copy_to++;
	} while((*copy_from != '\n') && (*copy_from != '\r') && (*copy_from != '\0'));
	*copy_to = '\0';
}


static int getrid_space(s8* data, s32 len)
{
	u8* buf = NULL;
	s32 i;
	u32 count = 0;

	buf = (char*)kzalloc(len + 5, GFP_KERNEL);
	if (buf == NULL){
		ts_err("get space kzalloc error");
		return -ESRCH;
	}

	for (i = 0; i < len; i++)
	{
		if (data[i] == ' ' || data[i] == '\r' || data[i] == '\n')
		{
			continue;
		}
		buf[count++] = data[i];
	}

	buf[count++] = '\0';

	memcpy(data, buf, count);
	kfree(buf);

	return count;
}

static int parse_valid_data(char *buf_start, loff_t buf_size,
	char *ptr, s32 *data, s32 rows)
{
	int i = 0;
	int j = 0;
	char *token = NULL;
	char *tok_ptr = NULL;
	char *row_data = NULL;
	long temp_val;

	if (!ptr) {
		ts_err("ptr is NULL");
		return -EINVAL;
	}
	if (!data) {
		ts_err("data is NULL");
		return -EINVAL;
	}

	row_data = (char *)kzalloc(MAX_LINE_LEN, GFP_KERNEL);
	if (!row_data) {
		ts_err("alloc bytes %d failed.", MAX_LINE_LEN);
		return -ENOMEM;
	}

	for (i = 0; i < rows; i++) {
		memset(row_data, 0, MAX_LINE_LEN);
		copy_this_line(row_data, ptr);
		getrid_space(row_data, strlen(row_data));
		tok_ptr = row_data;
		while ((token = strsep(&tok_ptr,","))) {
			if (strlen(token) == 0)
				continue;
			if (kstrtol(token, 0, &temp_val)) {
				kfree(row_data);
				return -EINVAL;
			}
			data[j++] = (s32)temp_val;
		}
		if (i == rows - 1)
			break;
		goto_next_line(&ptr); /* next row */
		if(!ptr || (0 == strlen(ptr)) || (ptr >= (buf_start + buf_size))) {
			ts_info("invalid ptr, return");
			kfree(row_data);
			row_data = NULL;
			return -EPERM;
		}
	}
	kfree(row_data);
	return j;
}

static int parse_csvfile(char *buf, size_t size, char *target_name,
		s32 *data, s32 rows, s32 col)
{
	int ret = 0;
	char *ptr = NULL;
	int read_ret;

	read_ret = size;
	if (read_ret > 0) {
		ptr = buf;
		ptr = strstr(ptr, target_name);
		if (!ptr) {
			ts_err("load %s failed 1!", target_name);
			return -EINTR;
		}

		goto_next_line(&ptr);
		if (!ptr || (0 == strlen(ptr))) {
			ts_err("load %s failed 2!", target_name);
			return -EIO;
		}

		if (data) {
			ret = parse_valid_data(buf, size, ptr, data, rows);
		} else {
			ts_err("load %s failed 3!", target_name);
			return -EINTR;
		}
	} else {
		ts_err("ret=%d, read_ret=%d", ret, read_ret);
		ret = -ENXIO;
	}

	return ret;
}


static void goodix_init_params(struct goodix_ts_test *ts_test)
{
	struct goodix_ts_core *ts = ts_test->ts;
	struct ts_test_params *test_params = &ts_test->test_params;

	test_params->rawdata_addr = ts->ic_info.misc.mutual_rawdata_addr;
	test_params->noisedata_addr = ts->ic_info.misc.mutual_diffdata_addr;
	test_params->self_rawdata_addr = ts->ic_info.misc.self_rawdata_addr;
	test_params->self_noisedata_addr = ts->ic_info.misc.self_diffdata_addr;

	test_params->max_drv_num = MAX_DRV_NUM;
	test_params->max_sen_num = MAX_SEN_NUM;
	test_params->drv_num = ts->ic_info.parm.drv_num;
	test_params->sen_num = ts->ic_info.parm.sen_num;
	test_params->drv_map = gt9897s_drv_map;
	test_params->sen_map = gt9897s_sen_map;

	test_params->short_threshold = gt9897s_short_parms[0];
	test_params->r_drv_drv_threshold = gt9897s_short_parms[1];
	test_params->r_drv_sen_threshold = gt9897s_short_parms[2];
	test_params->r_sen_sen_threshold = gt9897s_short_parms[3];
	test_params->r_drv_gnd_threshold = gt9897s_short_parms[4];
	test_params->r_sen_gnd_threshold = gt9897s_short_parms[5];
	test_params->avdd_value = gt9897s_short_parms[6];
}

static int goodix_init_testlimits(struct goodix_ts_test *ts_test)
{
	int ret;
	int i;
#ifdef SHORT_TEST
	u32 data_buf[10] = {0};
#endif /* SHORT_TEST */
	char *temp_buf = NULL;
	struct ts_test_params *test_params = &ts_test->test_params;
	struct goodix_ts_core *ts_core = ts_test->ts;
	const struct firmware *firmware = NULL;
	struct device *dev = &ts_core->pdev->dev;
	char limit_file[100] = {0};
	u32 tx = test_params->drv_num;
	u32 rx = test_params->sen_num;

	sprintf(limit_file, "%s_test_limits_%d.csv", GOODIX_TEST_FILE_NAME,
			ts_core->fw_version.sensor_id);
	ts_info("limit_file_name:%s.", limit_file);

	ret = request_firmware(&firmware, limit_file, dev);
	if (ret < 0) {
		ts_err("limits file [%s] not available", limit_file);
		return -EINVAL;
	}
	if (firmware->size <= 0) {
		ts_err("request_firmware, limits param length error,len:%zu",
			firmware->size);
		ret = -EINVAL;
		goto exit_free;
	}
	temp_buf = kzalloc(firmware->size + 1, GFP_KERNEL);
	if (!temp_buf) {
		ts_err("kzalloc bytes failed.");
		ret = -ENOMEM;
		goto exit_free;
	}
	memcpy(temp_buf, firmware->data, firmware->size);

	/* obtain config data */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_TEST_CONFIG,
		test_params->cfg_buf, 1, GOODIX_CFG_MAX_SIZE);
	if (ret < 0) {
		ts_info("Can't find %s", CSV_TP_TEST_CONFIG);
	} else {
		ts_info("parse_csvfile %s OK, cfg_len:%d", CSV_TP_TEST_CONFIG, ret);
		for (i = 0; i < ret; i++)
			ts_test->test_config.data[i] = (u8)test_params->cfg_buf[i];
		ts_test->test_config.len = ret;
	}

	/* obtain mutual_raw min */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_SPECIAL_RAW_MIN,
		test_params->min_limits, rx, tx);
	if (ret < 0) {
		ts_err("Failed get min_limits");
		goto exit_free;
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_SPECIAL_RAW_MIN);
	}
	/* obtain mutual_raw max */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_SPECIAL_RAW_MAX,
		test_params->max_limits, rx, tx);
	if (ret < 0) {
		ts_err("Failed get max_limits");
		goto exit_free;
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_SPECIAL_RAW_MAX);
	}
	/* obtain delta limit */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_SPECIAL_RAW_DELTA,
		test_params->deviation_limits, rx, tx);
	if (ret < 0) {
		ts_err("Failed get delta limit");
		goto exit_free;
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_SPECIAL_RAW_DELTA);
	}

#ifdef SELF_RAWDATA_TEST
	/* obtain self_raw min */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_SPECIAL_SELFRAW_MIN,
		test_params->self_min_limits, 1, tx + rx);
	if (ret < 0) {
		ts_err("Failed get self_min_limits");
		goto exit_free;
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_SPECIAL_SELFRAW_MIN);
	}
	/* obtain self_raw max */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_SPECIAL_SELFRAW_MAX,
		test_params->self_max_limits, 1, tx + rx);
	if (ret < 0) {
		ts_err("Failed get self_max_limits");
		goto exit_free;
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_SPECIAL_SELFRAW_MAX);
	}
#endif /* SELF_RAWDATA_TEST */

#ifdef NOISE_DATA_TEST
	/* obtain noise_threshold */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_NOISE_LIMIT,
		&test_params->noise_threshold, 1, 1);
	if (ret < 0) {
		ts_err("Failed get noise_threshold");
		goto exit_free;
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_NOISE_LIMIT);
	}
#endif /* NOISE_DATA_TEST */

#ifdef SELFNOISE_DATA_TEST
	/* obtain self_noise_threshold */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_SELFNOISE_LIMIT,
		&test_params->self_noise_threshold, 1, 1);
	if (ret < 0) {
		ts_err("Failed get self_noise_threshold");
		goto exit_free;
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_SELFNOISE_LIMIT);
	}
#endif /* SELFNOISE_DATA_TEST */

#ifdef SHORT_TEST
	/* obtain short_params */
	ret = parse_csvfile(temp_buf, firmware->size, CSV_TP_SHORT_THRESHOLD,
		(s32 *)data_buf, 1, 7);
	if (ret < 0) {
		ts_info("shortciurt_threshold isn't exist, use default value");
	} else {
		ts_info("parse_csvfile %s OK", CSV_TP_SHORT_THRESHOLD);
		test_params->short_threshold = data_buf[0];
		test_params->r_drv_drv_threshold = data_buf[1];
		test_params->r_drv_sen_threshold = data_buf[2];
		test_params->r_sen_sen_threshold = data_buf[3];
		test_params->r_drv_gnd_threshold = data_buf[4];
		test_params->r_sen_gnd_threshold = data_buf[5];
		test_params->avdd_value = data_buf[6];
	}
#endif /* SHORT_TEST */

exit_free:
	kfree(temp_buf);
	if (firmware)
		release_firmware(firmware);
	return ret;
}

static int goodix_tptest_prepare(struct goodix_ts_test *ts_test)
{
	int ret;
	struct goodix_ic_config *cfg = &ts_test->test_config;

	ts_info("TP test prepare IN");

	goodix_init_params(ts_test);
	/* parse test limits from csv */
	ret = goodix_init_testlimits(ts_test);
	if (ret < 0) {
		ts_err("Failed to init testlimits from csv.");
		return ret;
	}

	/* send test config */
	if (cfg->len > 0) {
		ret = ts_test->ts->hw_ops->send_config(ts_test->ts, cfg->data, cfg->len);
		if (ret < 0) {
			ts_err("Send test config failed, exit");
			return ret;
		}
	}

	/* disable irq */
	ts_test_irq_enable(ts_test, false);
	/* close esd */
	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	/* wait for irq work to end */
	msleep(30);

	return 0;
}

static void goodix_tptest_finish(struct goodix_ts_test *ts_test)
{
	int ret = 0;

	ts_info("TP test finish IN");
	/* reset chip */
	ts_test_reset(ts_test, 100);
	/* send normal config */
	ret = ts_test_send_config(ts_test, CONFIG_TYPE_NORMAL);
	if (ret < 0)
		ts_err("Send normal config failed");

	/* open esd */
	goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	/* enable irq */
	ts_test_irq_enable(ts_test, true);
}

#ifdef SHORT_TEST

#define INSPECT_FW_SWITCH_CMD		0x85
#define TEST_FW_PID					"OST"
static int goodix_short_test_prepare(struct goodix_ts_test *ts_test)
{
	struct goodix_ts_cmd tmp_cmd;
	struct goodix_fw_version fw_ver;
	int ret;
	int retry = 3;

	ts_info("short test prepare IN");
	ts_test->test_result[GTP_SHORT_TEST] = SYS_SOFTWARE_REASON;
	tmp_cmd.len = 4;
	tmp_cmd.cmd = INSPECT_FW_SWITCH_CMD;

	while (retry--) {
		ret = ts_test_send_cmd(ts_test, &tmp_cmd);
		if (ret < 0) {
			ts_err("send test mode failed");
			return ret;
		}
		usleep_range(10000, 11000);
		ret = ts_test_read_version(ts_test, &fw_ver);
		if (ret < 0) {
			ts_err("read test version failed");
			return ret;
		}
		ret = memcmp(&(fw_ver.patch_pid[3]), TEST_FW_PID, strlen(TEST_FW_PID));
		if (ret == 0)
			break;
		else
			ts_err("patch ID dismatch %s != %s", fw_ver.patch_pid, TEST_FW_PID);
	}

	return ret;
}

static u32 map_die2pin(struct ts_test_params *test_params, u32 chn_num)
{
	int i = 0;
	u32 res = 255;

	if (chn_num & DRV_CHANNEL_FLAG)
		chn_num = (chn_num & ~DRV_CHANNEL_FLAG) + test_params->max_sen_num;

	for (i = 0; i < test_params->max_sen_num; i++) {
		if (test_params->sen_map[i] == chn_num) {
			res = i;
			break;
		}
	}
	/* res != 255 mean found the corresponding channel num */
	if (res != 255)
		return res;
	/* if cannot find in SenMap try find in DrvMap */
	for (i = 0; i < test_params->max_drv_num; i++) {
		if (test_params->drv_map[i] == chn_num) {
			res = i;
			break;
		}
	}
	if (i >= test_params->max_drv_num)
		ts_err("Faild found corrresponding channel num:%d", chn_num);
	else
		res |= DRV_CHANNEL_FLAG;

	return res;
}

static void goodix_save_short_res(struct ts_test_params *params,
	u16 chn1, u16 chn2, int r)
{
	int i;
	u8 repeat_cnt = 0;
	u8 repeat = 0;
	struct goodix_ts_test *ts_test = container_of(params,
		struct goodix_ts_test, test_params);
	struct ts_short_res *short_res = &ts_test->short_res;

	if (chn1 == chn2 || short_res->short_num >= MAX_SHORT_NUM)
		return;

	for (i = 0; i < short_res->short_num; i++) {
		repeat_cnt = 0;
		if (short_res->short_msg[4 * i] == chn1)
			repeat_cnt++;
		if (short_res->short_msg[4 * i] == chn2)
			repeat_cnt++;
		if (short_res->short_msg[4 * i + 1] == chn1)
			repeat_cnt++;
		if (short_res->short_msg[4 * i + 1] == chn2)
			repeat_cnt++;
		if (repeat_cnt >= 2){
			repeat = 1;
			break;
		}
	}
	if (repeat == 0) {
		short_res->short_msg[4 * short_res->short_num + 0] = chn1;
		short_res->short_msg[4 * short_res->short_num + 1] = chn2;
		short_res->short_msg[4 * short_res->short_num + 2] = (r >> 8) & 0xFF;
		short_res->short_msg[4 * short_res->short_num + 3] = r & 0xFF;
		if (short_res->short_num < MAX_SHORT_NUM)
			short_res->short_num++;
	}
}

#define DRV_DRV_SELFCODE_REG	0x1045E
static int gdix_check_tx_tx_shortcircut(struct goodix_ts_test *ts_test,
		u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf = NULL;
	u32 data_reg;
	struct ts_test_params *test_params = &ts_test->test_params;
	u16 self_capdata, short_die_num = 0;

	size = 4 + MAX_DRV_NUM * 2 + 2;	/* 4 + MAX_DRV_NUM * 2 + 2; */
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* drv&drv shortcircut check */
	data_reg = DRV_DRV_SELFCODE_REG;
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(ts_test, data_reg, data_buf, size);
		if (ret < 0) {
			ts_err("Failed read Drv-to-Drv short rawdata");
			err = -EINVAL;
			break;
		}

		if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
			ts_err("Drv-to-Drv adc data checksum error");
			err = -EINVAL;
			break;
		}

		r_threshold = test_params->r_drv_drv_threshold;
		short_die_num = le16_to_cpup((__le16 *)&data_buf[0]);
		short_die_num -= MAX_SEN_NUM;
		if (short_die_num >= MAX_DRV_NUM) {
			ts_info("invalid short pad num:%d",
				short_die_num + MAX_SEN_NUM);
			continue;
		}

		/* TODO: j start position need recheck */
		self_capdata = le16_to_cpup((__le16 *)&data_buf[2]);
		if (self_capdata == 0xffff || self_capdata == 0) {
			ts_info("invalid self_capdata:0x%x", self_capdata);
			continue;
		}

		for (j = short_die_num + 1; j < MAX_DRV_NUM; j++) {
			adc_signal = le16_to_cpup((__le16 *)&data_buf[4 + j * 2]);

			if (adc_signal < test_params->short_threshold)
				continue;

			short_r = DRV_TO_DRV_R(self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num =
					map_die2pin(test_params, short_die_num + MAX_SEN_NUM);
				slave_pin_num =
					map_die2pin(test_params, j + MAX_SEN_NUM);
				if (master_pin_num == 0xFF || slave_pin_num == 0xFF) {
					ts_info("WARNNING invalid pin");
					continue;
				}
				goodix_save_short_res(test_params, master_pin_num,
					slave_pin_num, short_r);
				ts_err("short circut:R=%dK,R_Threshold=%dK",
							short_r, r_threshold);
				ts_err("%s%d--%s%d shortcircut",
					(master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(master_pin_num & ~DRV_CHANNEL_FLAG),
					(slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(slave_pin_num & ~DRV_CHANNEL_FLAG));
				err = -EINVAL;
			}
		}
		data_reg += size;
	}

	kfree(data_buf);
	return err;
}

#define SEN_SEN_SELFCODE_REG	0x1084E
static int gdix_check_rx_rx_shortcircut(struct goodix_ts_test *ts_test,
		u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf = NULL;
	u32 data_reg;
	struct ts_test_params *test_params = &ts_test->test_params;
	u16 self_capdata, short_die_num = 0;

	size = 4 + MAX_SEN_NUM * 2 + 2;	/* 4 + MAX_SEN_NUM * 2 + 2; */
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* drv&drv shortcircut check */
	data_reg = SEN_SEN_SELFCODE_REG;
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(ts_test, data_reg, data_buf, size);
		if (ret) {
			ts_err("Failed read Sen-to-Sen short rawdata");
			err = -EINVAL;
			break;
		}

		if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
			ts_err("Sen-to-Sen adc data checksum error");
			err = -EINVAL;
			break;
		}

		r_threshold = test_params->r_sen_sen_threshold;
		short_die_num = le16_to_cpup((__le16 *)&data_buf[0]);
		if (short_die_num >= MAX_SEN_NUM) {
			ts_info("invalid short pad num:%d",	short_die_num);
			continue;
		}

		/* TODO: j start position need recheck */
		self_capdata = le16_to_cpup((__le16 *)&data_buf[2]);
		if (self_capdata == 0xffff || self_capdata == 0) {
			ts_info("invalid self_capdata:0x%x", self_capdata);
			continue;
		}

		for (j = short_die_num + 1; j < MAX_SEN_NUM; j++) {
			adc_signal = le16_to_cpup((__le16 *)&data_buf[4 + j * 2]);
			if (adc_signal < test_params->short_threshold)
				continue;

			short_r = SEN_TO_SEN_R(self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num = map_die2pin(test_params, short_die_num);
				slave_pin_num = map_die2pin(test_params, j);
				if (master_pin_num == 0xFF || slave_pin_num == 0xFF) {
					ts_info("WARNNING invalid pin");
					continue;
				}
				goodix_save_short_res(test_params, master_pin_num,
					slave_pin_num, short_r);
				ts_err("short circut:R=%dK,R_Threshold=%dK",
							short_r, r_threshold);
				ts_err("%s%d--%s%d shortcircut",
					(master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(master_pin_num & ~DRV_CHANNEL_FLAG),
					(slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(slave_pin_num & ~DRV_CHANNEL_FLAG));
				err = -EINVAL;
			}
		}
		data_reg += size;
	}

	kfree(data_buf);
	return err;
}

#define DRV_SEN_SELFCODE_REG 0x11712
static int gdix_check_tx_rx_shortcircut(struct goodix_ts_test *ts_test,
		u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf = NULL;
	u32 data_reg;
	struct ts_test_params *test_params = &ts_test->test_params;
	u16 self_capdata, short_die_num = 0;

	size = 4 + MAX_DRV_NUM * 2 + 2;	/* 4 + MAX_SEN_NUM * 2 + 2; */
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* drv&sen shortcircut check */
	data_reg = DRV_SEN_SELFCODE_REG;
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(ts_test, data_reg, data_buf, size);
		if (ret) {
			ts_err("Failed read Drv-to-Sen short rawdata");
			err = -EINVAL;
			break;
		}

		if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
			ts_err("Drv-to-Sen adc data checksum error");
			err = -EINVAL;
			break;
		}

		r_threshold = test_params->r_drv_sen_threshold;
		short_die_num = le16_to_cpup((__le16 *)&data_buf[0]);
		if (short_die_num >= MAX_SEN_NUM) {
			ts_info("invalid short pad num:%d",	short_die_num);
			continue;
		}

		/* TODO: j start position need recheck */
		self_capdata = le16_to_cpup((__le16 *)&data_buf[2]);
		if (self_capdata == 0xffff || self_capdata == 0) {
			ts_info("invalid self_capdata:0x%x", self_capdata);
			continue;
		}

		for (j = 0; j < MAX_DRV_NUM; j++) {
			adc_signal = le16_to_cpup((__le16 *)&data_buf[4 + j * 2]);

			if (adc_signal < test_params->short_threshold)
				continue;

			short_r = SEN_TO_DRV_R(self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num = map_die2pin(test_params, short_die_num);
				slave_pin_num = map_die2pin(test_params, j + MAX_SEN_NUM);
				if (master_pin_num == 0xFF || slave_pin_num == 0xFF) {
					ts_info("WARNNING invalid pin");
					continue;
				}
				goodix_save_short_res(test_params, master_pin_num,
					slave_pin_num, short_r);
				ts_err("short circut:R=%dK,R_Threshold=%dK",
							short_r, r_threshold);
				ts_err("%s%d--%s%d shortcircut",
					(master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(master_pin_num & ~DRV_CHANNEL_FLAG),
					(slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(slave_pin_num & ~DRV_CHANNEL_FLAG));
				err = -EINVAL;
			}
		}
		data_reg += size;
	}

	kfree(data_buf);
	return err;
}

#define SHORT_TYPE_FLAG		((uint16_t)1 << 15)
static int gdix_check_resistance_to_gnd(struct ts_test_params *test_params,
		u16 adc_signal, u32 pos)
{
	long r = 0;
	u16 r_th = 0, avdd_value = 0;
	u16 chn_id_tmp = 0;
	u8 pin_num = 0;

	avdd_value = test_params->avdd_value;
	if (adc_signal == 0 || adc_signal == 0x8000)
		adc_signal |= 1;

	if ((adc_signal & 0x8000) == 0) { /* short to GND */
		if (pos < MAX_DRV_NUM)
			r = DRV_TO_GND_R(adc_signal);
		else
			r = SEN_TO_GND_R(adc_signal);
	} else { /* short to VDD */
		/* adc_signal = adc_signal & ~0x8000; */
		if (pos < MAX_DRV_NUM)
			r = DRV_TO_AVDD_R((adc_signal & ~0x8000), avdd_value);
		else
			r = SEN_TO_AVDD_R((adc_signal & ~0x8000), avdd_value);
	}

	if (pos < MAX_DRV_NUM)
		r_th = test_params->r_drv_gnd_threshold;
	else
		r_th = test_params->r_sen_gnd_threshold;

	chn_id_tmp = pos;
	if (chn_id_tmp < MAX_DRV_NUM)
		chn_id_tmp += MAX_SEN_NUM;
	else
		chn_id_tmp -= MAX_DRV_NUM;

	if (r < r_th) {
		pin_num = map_die2pin(test_params, chn_id_tmp);
		goodix_save_short_res(test_params, pin_num,
			(adc_signal & 0x8000)? CHN_VDD : CHN_GND, r);
		ts_err("%s%d shortcircut to %s,R=%ldK,R_Threshold=%dK",
				(pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
				(pin_num & ~DRV_CHANNEL_FLAG),
				(adc_signal & 0x8000) ? "VDD" : "GND",
				r, r_th);

		return -EINVAL;
	}

	return 0;
}

#define DIFF_CODE_DATA_REG		0x11F72
static int gdix_check_gndvdd_shortcircut(struct goodix_ts_test *ts_test)
{
	int ret = 0, err = 0;
	int size = 0, i = 0;
	u16 adc_signal = 0;
	u32 data_reg;
	u8 *data_buf = NULL;

	size = (MAX_DRV_NUM + MAX_SEN_NUM) * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* read diff code, diff code will be used to calculate
		* resistance between channel and GND */
	data_reg = DIFF_CODE_DATA_REG;
	ret = ts_test_read(ts_test, data_reg, data_buf, size); /* DIFF_CODE_REG   0xA97A */
	if (ret < 0) {
		ts_err("Failed read to-gnd rawdata");
		err = -EINVAL;
		goto err_out;
	}

	if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
		ts_err("diff code checksum error");
		err = -EINVAL;
		goto err_out;
	}

	for (i = 0; i < MAX_DRV_NUM + MAX_SEN_NUM; i++) {
		adc_signal = le16_to_cpup((__le16 *)&data_buf[i * 2]);
		ret = gdix_check_resistance_to_gnd(&ts_test->test_params,
					adc_signal, i);
		if (ret != 0) {
			ts_err("Resistance to-gnd/vdd short");
			err = ret;
		}
	}

err_out:
	kfree(data_buf);
	return err;
}

#define TEST_RESULT_REG			0x10410
#define TEST_CHANNEL_MAP_REG	0x10410
static int goodix_shortcircut_analysis(struct goodix_ts_test *ts_test)
{
	int ret;
	int err = 0;
	test_result_t test_result;

	ret = ts_test_read(ts_test, TEST_RESULT_REG,
		(u8 *)&test_result, sizeof(test_result));
	if (ret < 0) {
		ts_err("Read TEST_RESULT_REG failed");
		return ret;
	}

	if (checksum_cmp((u8 *)&test_result, sizeof(test_result),
			CHECKSUM_MODE_U8_LE)) {
		ts_err("shrot result checksum err");
		return -EINVAL;
	}

	if (!(test_result.result & 0x0F)) {
		ts_info(">>>>> No shortcircut");
		return 0;
	}
	ts_info("short flag 0x%02x, drv&drv:%d, sen&sen:%d, drv&sen:%d, drv/GNDVDD:%d, sen/GNDVDD:%d",
		test_result.result, test_result.drv_drv_num, test_result.sen_sen_num,
		test_result.drv_sen_num, test_result.drv_gnd_avdd_num, test_result.sen_gnd_avdd_num);

	if (test_result.drv_drv_num)
		err |= gdix_check_tx_tx_shortcircut(ts_test, test_result.drv_drv_num);
	if (test_result.sen_sen_num)
		err |= gdix_check_rx_rx_shortcircut(ts_test, test_result.sen_sen_num);
	if (test_result.drv_sen_num)
		err |= gdix_check_tx_rx_shortcircut(ts_test, test_result.drv_sen_num);
	if (test_result.drv_gnd_avdd_num || test_result.sen_gnd_avdd_num)
		err |= gdix_check_gndvdd_shortcircut(ts_test);
	ts_info(">>>>> short check return 0x%x", err);
	return err;
}

#define SHORT_FW_CMD_REG				0x10400
static int send_test_cmd(struct goodix_ts_test *ts_test,
	struct goodix_ts_cmd *cmd)
{
	int ret;
	u32 reg = SHORT_FW_CMD_REG;
	cmd->state = 0;
	cmd->ack = 0;
	goodix_append_checksum(&(cmd->buf[2]), cmd->len - 2,
			CHECKSUM_MODE_U8_LE);
	ret = ts_test_write(ts_test, reg, cmd->buf, cmd->len + 2);
	if (ret < 0)
		return ret;
	usleep_range(10000, 11000);
	return ret;
}

#define SHORT_TEST_TIME_REG				0x11FF2
#define INSPECT_PARAM_CMD				0xAA
#define INSPECT_CMD_ACK_DONE			0xEE
#define DFT_SHORT_THRESHOLD				16
#define DFT_DIFFCODE_SHORT_THRESHOLD	16
#define DFT_ADC_DUMP_NUM				1396
#define INSPECT_CMD_STATUS_FINISH		0x88
static void goodix_shortcircut_test(struct goodix_ts_test *ts_test)
{
	int ret = 0;
	int retry;
	u16 test_time;
	struct goodix_ts_cmd test_parm_cmd;
	struct goodix_ts_cmd tmp_cmd;

	ts_info("---------------------- short_test begin ----------------------");
	ret = goodix_short_test_prepare(ts_test);
	if (ret < 0) {
		ts_err("Failed enter short test mode");
		return;
	}

	ret = ts_test_read(ts_test, SHORT_TEST_TIME_REG, (u8 *)&test_time, 2);
	if (ret < 0) {
		ts_err("Failed to get test_time, default %dms", DEFAULT_TEST_TIME_MS);
		test_time = DEFAULT_TEST_TIME_MS;
	} else {
		test_time /= 10;
		if (test_time > MAX_TEST_TIME_MS) {
			ts_info("test time too long %d > %d",
				test_time, MAX_TEST_TIME_MS);
			test_time = MAX_TEST_TIME_MS;
		}
		ts_info("get test time %dms", test_time);
	}

	test_parm_cmd.len = 0x0A;
	test_parm_cmd.cmd = INSPECT_PARAM_CMD;
	test_parm_cmd.data[0] = DFT_SHORT_THRESHOLD & 0xFF;
	test_parm_cmd.data[1] = (DFT_SHORT_THRESHOLD >> 8) & 0xFF;
	test_parm_cmd.data[2] = DFT_DIFFCODE_SHORT_THRESHOLD & 0xFF;
	test_parm_cmd.data[3] = (DFT_DIFFCODE_SHORT_THRESHOLD >> 8) & 0xFF;
	test_parm_cmd.data[4] = DFT_ADC_DUMP_NUM & 0xFF;
	test_parm_cmd.data[5] = (DFT_ADC_DUMP_NUM >> 8) & 0xFF;
	ret = send_test_cmd(ts_test, &test_parm_cmd);
	if (ret < 0) {
		ts_err("send INSPECT_PARAM_CMD failed");
		return;
	}

	retry = 10;
	do {
		ret = ts_test_read(ts_test, SHORT_FW_CMD_REG, tmp_cmd.buf, sizeof(tmp_cmd.buf));
		if (!ret && tmp_cmd.ack == INSPECT_CMD_ACK_DONE)
			break;
		ts_info("ack not ready 0x%x", tmp_cmd.ack);
		usleep_range(2000, 2100);
	} while (--retry);

	if (tmp_cmd.ack != INSPECT_CMD_ACK_DONE) {
		ts_err("failed get ack ready flag ack 0x%x != 0x%x",
			tmp_cmd.ack, INSPECT_CMD_ACK_DONE);
		return;
	}

	msleep(test_time);
	retry = 50;
	do{
		ret = ts_test_read(ts_test, SHORT_FW_CMD_REG, tmp_cmd.buf,
				sizeof(tmp_cmd.buf));
		if(!ret && tmp_cmd.state == INSPECT_CMD_STATUS_FINISH)
			break;
		ts_info("short test ack 0x%x status 0x%x", tmp_cmd.ack, tmp_cmd.state);
		msleep(50);
	} while(--retry);

	if (tmp_cmd.state == INSPECT_CMD_STATUS_FINISH) {
		ts_info("short_test finished, start analysis");
		ret = goodix_shortcircut_analysis(ts_test);
		if(ret < 0)
			ts_test->test_result[GTP_SHORT_TEST] = GTP_PANEL_REASON;
		else
			ts_test->test_result[GTP_SHORT_TEST] = GTP_TEST_PASS;
	} else {
		ts_err("ack state is invalid, state:0x%02x", tmp_cmd.state);
	}
}
#endif /* SHORT_TEST */

#define GOODIX_CMD_RAWDATA	2
#define GOODIX_TOUCH_EVENT	0x80
static int goodix_cap_test_prepare(struct goodix_ts_test *ts_test)
{
	int ret;
	struct goodix_ts_cmd temp_cmd;

	ts_info("cap test prepare IN");
	ts_test->test_result[GTP_CAP_TEST] = SYS_SOFTWARE_REASON;
	ts_test->test_result[GTP_DELTA_TEST] = SYS_SOFTWARE_REASON;
#ifdef SELF_RAWDATA_TEST
	ts_test->test_result[GTP_SELFCAP_TEST] = SYS_SOFTWARE_REASON;
#endif /* SELF_RAWDATA_TEST */
#ifdef NOISE_DATA_TEST
	ts_test->test_result[GTP_NOISE_TEST] = SYS_SOFTWARE_REASON;
#endif /* NOISE_DATA_TEST */
#ifdef SELFNOISE_DATA_TEST
	ts_test->test_result[GTP_SELFNOISE_TEST] = SYS_SOFTWARE_REASON;
#endif /* SELFNOISE_DATA_TEST */

	/* switch rawdata mode */
	temp_cmd.cmd = GOODIX_CMD_RAWDATA;
	temp_cmd.len = 4;
	ret = ts_test_send_cmd(ts_test, &temp_cmd);
	if (ret < 0)
		ts_err("Enter rawdata mode failed");

	return ret;
}

#define GOODIX_CMD_COORD	0
static int goodix_cap_test_finish(struct goodix_ts_test *ts_test)
{
	int ret;
	u8 val = 0;
	struct goodix_ts_cmd temp_cmd;
	u32 flag_addr = ts_test->ts->ic_info.misc.touch_data_addr;

	ts_info("cap_test finished");
	/* switch coor mode */
	temp_cmd.cmd = GOODIX_CMD_COORD;
	temp_cmd.len = 4;
	ret = ts_test_send_cmd(ts_test, &temp_cmd);
	if (ret < 0) {
		ts_err("Enter coor mode failed");
		return ret;
	}
	/* clean touch event flag */
	ts_test_write(ts_test, flag_addr, &val, 1);
	return ret;
}

static int goodix_cache_rawdata(struct goodix_ts_test *ts_test)
{
	int ret;
	int i;
	int retry = 50;
	u8 val;
	u32 sen_num = ts_test->test_params.sen_num;
	u32 drv_num = ts_test->test_params.drv_num;
	u32 data_size = sen_num * drv_num;
	u32 data_addr = ts_test->test_params.rawdata_addr;
	u32 flag_addr = ts_test->ts->ic_info.misc.touch_data_addr;

	for (i = 0; i < TOTAL_FRAME_NUM; i++) {
		val = 0;
		ret = ts_test_write(ts_test, flag_addr, &val, 1);
		if (ret < 0) {
			ts_err("clean touch event failed, exit");
			return -EAGAIN;
		}
		while (retry--) {
			usleep_range(5000, 5100);
			ret = ts_test_read(ts_test, flag_addr, &val, 1);
			if (!ret && (val & 0x80))
				break;
		}
		if (retry < 0) {
			ts_err("rawdata is not ready val:0x%02x i:%d, exit", val, i);
			return -EAGAIN;
		}

		ret = ts_test_read(ts_test, data_addr,
			(u8 *)ts_test->rawdata[i].data, data_size * sizeof(s16));
		if (ret < 0)
			return ret;
		ts_test->rawdata[i].size = data_size;
		goodix_rotate_abcd2cbad(drv_num, sen_num, ts_test->rawdata[i].data);
	}

	return ret;
}

static void goodix_cache_deltadata(struct goodix_ts_test *ts_test)
{
	u32 data_size;
	int tx = ts_test->test_params.drv_num;
	int i;
	int j;
	int max_val;
	int raw;
	int temp;

	for (i = 0; i < TOTAL_FRAME_NUM; i++) {
		data_size = ts_test->rawdata[i].size;
		if (data_size == 0)
			continue;
		for (j = 0; j < data_size; j++) {
			raw = ts_test->rawdata[i].data[j];
			max_val = 0;
			/* calcu delta with above node */
			if (j - tx >= 0) {
				temp = ts_test->rawdata[i].data[j - tx];
				temp = ABS(temp - raw);
				max_val = MAX(max_val, temp);
			}
			/* calcu delta with bellow node */
			if (j + tx < data_size) {
				temp = ts_test->rawdata[i].data[j + tx];
				temp = ABS(temp - raw);
				max_val = MAX(max_val, temp);
			}
			/* calcu delta with left node */
			if (j % tx) {
				temp = ts_test->rawdata[i].data[j - 1];
				temp = ABS(temp - raw);
				max_val = MAX(max_val, temp);
			}
			/* calcu delta with right node */
			if ((j + 1) % tx) {
				temp = ts_test->rawdata[i].data[j + 1];
				temp = ABS(temp - raw);
				max_val = MAX(max_val, temp);
			}
			ts_test->accord_arr[i].data[j] = max_val * 1000 / raw;
		}
		ts_test->accord_arr[i].size = data_size;
	}
}

#ifdef SELF_RAWDATA_TEST
static int goodix_cache_self_rawdata(struct goodix_ts_test *ts_test)
{
	int ret;
	u32 sen_num = ts_test->test_params.sen_num;
	u32 drv_num = ts_test->test_params.drv_num;
	u32 data_size = sen_num + drv_num;
	u32 data_addr = ts_test->test_params.self_rawdata_addr;

	ret = ts_test_read(ts_test, data_addr,
		(u8 *)ts_test->self_rawdata.data, data_size * sizeof(s16));
	if (ret < 0)
		return ret;
	ts_test->self_rawdata.size = data_size;

	return ret;
}
#endif /* SELF_RAWDATA_TEST */

#ifdef NOISE_DATA_TEST
static int goodix_cache_noisedata(struct goodix_ts_test *ts_test)
{
	int ret;
	int i;
	int cnt;
	int retry = 50;
	u8 val;
	u32 sen_num = ts_test->test_params.sen_num;
	u32 drv_num = ts_test->test_params.drv_num;
	u32 data_size = sen_num * drv_num;
	u32 data_addr = ts_test->test_params.noisedata_addr;
	u32 flag_addr = ts_test->ts->ic_info.misc.touch_data_addr;

	for (cnt = 0; cnt < NOISEDATA_TEST_TIMES; cnt++) {
		val = 0;
		ret = ts_test_write(ts_test, flag_addr, &val, 1);
		if (ret < 0) {
			ts_err("clean touch event failed, exit");
			return -EAGAIN;
		}
		while (retry--) {
			usleep_range(5000, 5100);
			ret = ts_test_read(ts_test, flag_addr, &val, 1);
			if (!ret && (val & 0x80))
				break;
		}
		if (retry < 0) {
			ts_err("noisedata is not ready val:0x%02x i:%d, exit", val, cnt);
			return -EAGAIN;
		}

		ret = ts_test_read(ts_test, data_addr,
			(u8 *)ts_test->noisedata[cnt].data, data_size * sizeof(s16));
		if (ret < 0)
			return ret;
		ts_test->noisedata[cnt].size = data_size;
		goodix_rotate_abcd2cbad(drv_num, sen_num, ts_test->noisedata[cnt].data);
		for (i = 0; i < data_size; i++) {
			ts_test->noisedata[cnt].data[i] = ABS(ts_test->noisedata[cnt].data[i]);
		}
	}

	return ret;
}
#endif /* NOISE_DATA_TEST */

#ifdef SELFNOISE_DATA_TEST
static int goodix_cache_self_noisedata(struct goodix_ts_test *ts_test)
{
	int ret;
	int i;
	u32 sen_num = ts_test->test_params.sen_num;
	u32 drv_num = ts_test->test_params.drv_num;
	u32 data_size = sen_num + drv_num;
	u32 data_addr = ts_test->test_params.self_noisedata_addr;

	ret = ts_test_read(ts_test, data_addr,
		(u8 *)ts_test->self_noisedata.data, data_size * sizeof(s16));
	if (ret < 0)
		return ret;
	ts_test->self_noisedata.size = data_size;
	for (i = 0; i < data_size; i++) {
		ts_test->self_noisedata.data[i] = ABS(ts_test->self_noisedata.data[i]);
	}

	return ret;
}
#endif /* SELFNOISE_DATA_TEST */

static int goodix_analysis_rawdata(struct goodix_ts_test *ts_test)
{
	int i;
	int j;
	bool fail_flag = false;
	int err_cnt = 0;
	int times = TOTAL_FRAME_NUM;
	s16 val;
	u32 data_size = ts_test->rawdata[0].size;

	for (i = 0; i < times; i++) {
		for (j = 0; j < data_size; j++) {
			val = ts_test->rawdata[i].data[j];
			if (val < ts_test->test_params.min_limits[j]) {
				fail_flag = true;
				ts_test->open_res.beyond_min_limit_cnt[j]++;
			}
			if (val > ts_test->test_params.max_limits[j]) {
				fail_flag = true;
				ts_test->open_res.beyond_max_limit_cnt[j]++;
			}
		}
		if (fail_flag)
			err_cnt++;
		fail_flag = false;
	}

	err_cnt *= 100;
	if (err_cnt > times * 100 * 9 / 10)
		return -EINVAL;

	return 0;
}

static int goodix_analysis_deltadata(struct goodix_ts_test *ts_test)
{
	int i;
	int j;
	int ret = 0;
	s16 val;
	u32 data_size = ts_test->accord_arr[0].size;

	for (i = 0; i < TOTAL_FRAME_NUM; i++) {
		for (j = 0; j < data_size; j++) {
			val = ts_test->accord_arr[i].data[j];
			if (val > ts_test->test_params.deviation_limits[j]) {
				ts_test->open_res.beyond_accord_limit_cnt[j]++;
				ret = -EINVAL;
			}
		}
	}

	return ret;
}

#ifdef SELF_RAWDATA_TEST
static int goodix_analysis_self_rawdata(struct goodix_ts_test *ts_test)
{
	int i;
	s16 val;
	u32 data_size = ts_test->self_rawdata.size;

	for (i = 0; i < data_size; i++) {
		val = ts_test->self_rawdata.data[i];
		if (val < ts_test->test_params.self_min_limits[i] ||
			val > ts_test->test_params.self_max_limits[i]) {
			ts_err("self_rawdata isn't in range, val:%d threshold:[%d,%d]",
				val, ts_test->test_params.self_min_limits[i],
				ts_test->test_params.self_max_limits[i]);
			return -EINVAL;
		}
	}

	return 0;
}
#endif /* SELF_RAWDATA_TEST */

#ifdef NOISE_DATA_TEST
static int goodix_analysis_noisedata(struct goodix_ts_test *ts_test)
{
	int cnt;
	int i;
	bool fail_flag = false;
	int err_cnt = 0;
	int times = NOISEDATA_TEST_TIMES;
	s16 val;
	u32 data_size = ts_test->noisedata[0].size;

	for (cnt = 0; cnt < times; cnt++) {
		for (i = 0; i < data_size; i++) {
			val = ts_test->noisedata[cnt].data[i];
			if (val > ts_test->test_params.noise_threshold)
				fail_flag = true;
		}
		if (fail_flag)
			err_cnt++;
		fail_flag = false;
	}

	err_cnt *= 100;
	if (err_cnt > times * 100 * 2 / 10)
		return -EINVAL;

	return 0;
}
#endif /* NOISE_DATA_TEST */

#ifdef SELFNOISE_DATA_TEST
static int goodix_analysis_self_noisedata(struct goodix_ts_test *ts_test)
{
	int i;
	s16 val;
	u32 data_size = ts_test->self_noisedata.size;

	for (i = 0; i < data_size; i++) {
		val = ts_test->self_noisedata.data[i];
		if (val > ts_test->test_params.self_noise_threshold) {
			ts_err("self noisedata isn't in range, val:%d threshold:[0,%d]",
				val, ts_test->test_params.self_noise_threshold);
			return -EINVAL;
		}
	}

	return 0;
}
#endif /* SELFNOISE_DATA_TEST */

static void goodix_capacitance_test(struct goodix_ts_test *ts_test)
{
	int ret;

	ts_info("---------------------- cap_test begin ----------------------");
	ret = goodix_cap_test_prepare(ts_test);
	if (ret < 0) {
		ts_err("cap_test prepare failed, exit");
		goto exit;
	}
	ts_info("cap rawdata prepare OK");

	/* obtain rawdata */
	ret = goodix_cache_rawdata(ts_test);
	if (ret < 0) {
		if (ret == -EAGAIN) {
			ts_err("Capacitance exit");
			goto exit;
		} else {
			ts_err("Failed to read capdata");
		}
	} else {
		ts_info("get rawdata finish, start analysis");
		ret = goodix_analysis_rawdata(ts_test);
		if (ret < 0)
			ts_test->test_result[GTP_CAP_TEST] = GTP_PANEL_REASON;
		else
			ts_test->test_result[GTP_CAP_TEST] = GTP_TEST_PASS;
	}

	/* obtain delta_data */
	goodix_cache_deltadata(ts_test);
	ts_info("get deltadata finish, start analysis");
	ret = goodix_analysis_deltadata(ts_test);
	if (ret < 0)
		ts_test->test_result[GTP_DELTA_TEST] = GTP_PANEL_REASON;
	else
		ts_test->test_result[GTP_DELTA_TEST] = GTP_TEST_PASS;

#ifdef NOISE_DATA_TEST
	/* obtain noisedata */
	ret = goodix_cache_noisedata(ts_test);
	if (ret < 0) {
		ts_err("Failed to read noisedata");
	} else {
		ts_info("get noisedata finish, start analysis");
		ret = goodix_analysis_noisedata(ts_test);
		if (ret < 0)
			ts_test->test_result[GTP_NOISE_TEST] = GTP_PANEL_REASON;
		else
			ts_test->test_result[GTP_NOISE_TEST] = GTP_TEST_PASS;
	}
#endif /* NOISE_DATA_TEST */

#ifdef SELF_RAWDATA_TEST
	/* obtain self_rawdata */
	ret = goodix_cache_self_rawdata(ts_test);
	if (ret < 0) {
		ts_err("Failed to read self_capdata");
	} else {
		ts_info("get self_rawdata finish, start analysis");
		ret = goodix_analysis_self_rawdata(ts_test);
		if (ret < 0)
			ts_test->test_result[GTP_SELFCAP_TEST] = GTP_PANEL_REASON;
		else
			ts_test->test_result[GTP_SELFCAP_TEST] = GTP_TEST_PASS;
	}
#endif /* SELF_RAWDATA_TEST */

#ifdef SELFNOISE_DATA_TEST
	/* obtain self_noisedata */
	ret = goodix_cache_self_noisedata(ts_test);
	if (ret < 0) {
		ts_err("Failed to read self_noisedata");
	} else {
		ts_info("get self_noisedata finish, start analysis");
		ret = goodix_analysis_self_noisedata(ts_test);
		if (ret < 0)
			ts_test->test_result[GTP_SELFNOISE_TEST] = GTP_PANEL_REASON;
		else
			ts_test->test_result[GTP_SELFNOISE_TEST] = GTP_TEST_PASS;
	}
#endif /* SELFNOISE_DATA_TEST */

exit:
	goodix_cap_test_finish(ts_test);
}

char *goodix_strncat(char *dest, char *src, size_t dest_size)
{
	size_t dest_len = 0;

	dest_len = strnlen(dest, dest_size);
	return strncat(&dest[dest_len], src, dest_size - dest_len - 1);
}

char *goodix_strncatint(char *dest, int src, char *format, size_t dest_size)
{
	char src_str[MAX_STR_LEN] = {0};

	snprintf(src_str, MAX_STR_LEN, format, src);
	return goodix_strncat(dest, src_str, dest_size);
}

static void goodix_data_cal(s16 *data, size_t data_size, s16 *stat_result)
{
	int i = 0;
	s16 avg = 0;
	s16 min = 0;
	s16 max = 0;
	long long sum = 0;

	min = data[0];
	max = data[0];
	for (i = 0; i < data_size; i++) {
		sum += data[i];
		if (max < data[i])
			max = data[i];
		if (min > data[i])
			min = data[i];
	}
	avg = div_s64(sum, data_size);
	stat_result[0] = avg;
	stat_result[1] = max;
	stat_result[2] = min;
}

static void goodix_data_statistics(s16 *data, size_t data_size,
		char *result, size_t res_size)
{
	s16 stat_value[3];

	if (!data || !result) {
		ts_err("parameters error please check *data and *result value");
		return;
	}

	if (data_size <= 0 || res_size <= 0) {
		ts_err("input parameter is illegva:data_size=%ld, res_size=%ld",
			data_size, res_size);
		return;
	}
	goodix_data_cal(data, data_size, stat_value);

	memset(result, 0, res_size);
	snprintf(result, res_size, "[%d,%d,%d]",
			stat_value[0], stat_value[1], stat_value[2]);
	return;
}

#ifdef SAVE_IN_CSV
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static ssize_t fs_write(const void* buf, size_t size, struct file* fp)
{
	loff_t pos;
	ssize_t len;

	pos = fp->f_pos;
	len = kernel_write(fp, buf, size, &pos);
	fp->f_pos = pos;

	return len;
}
#else
static ssize_t fs_write(const void* buf, size_t size, struct file* fp)
{
	mm_segment_t old_fs;
	loff_t pos;
	ssize_t len;

	pos = fp->f_pos;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	len = vfs_write(fp, buf, size, &pos);
	set_fs(old_fs);
	fp->f_pos = pos;

	return len;
}
#endif


static int goodix_save_test_config(struct goodix_ts_test *ts_test,
		struct file *fp)
{
	int ret = 0;
	int i;
	int bytes = 0;
	char *data;
	struct goodix_ic_config *cfg = &ts_test->test_config;

	if (cfg->len <= 0) {
		ts_info("Can't find vaild test config");
		return 0;
	}

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "<OrderConfig>\n");
	for (i = 0; i < cfg->len; i++) {
		bytes += sprintf(&data[bytes], "0x%02x,", cfg->data[i]);
	}
	bytes += sprintf(&data[bytes], "\n");
	bytes += sprintf(&data[bytes], "</OrderConfig>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("test config write failed");
		goto save_end;
	}

save_end:
	kfree(data);
	return ret;
}

static int goodix_save_header(struct goodix_ts_test *ts_test,
		struct file *fp)
{
	int ret;
	int i;
	int bytes = 0;
	bool result = false;
	char *data = NULL;
	struct goodix_ts_core *ts = ts_test->ts;

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	bytes += sprintf(&data[bytes], "<TESTLOG>\n");
	bytes += sprintf(&data[bytes], "<Header>\n");
	/* sava test result */
	for (i = 0; i < MAX_TEST_ITEMS; i++) {
		if ((ts_test->test_result[i] > 0) &&
			(ts_test->test_result[i] != GTP_TEST_PASS)) {
				result = true;
				break;
		}
	}
	if (result)
		bytes += sprintf(&data[bytes], "<Result>NG</Result>\n");
	else
		bytes += sprintf(&data[bytes], "<Result>OK</Result>\n");
	bytes += sprintf(&data[bytes], "<DeviceType>GT%s</DeviceType>\n",
			ts->fw_version.patch_pid);
	bytes += sprintf(&data[bytes], "<SensorId>%d</SensorId>\n",
			ts_test->ts->fw_version.sensor_id);
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("header write failed");
		goto save_end;
	}
	bytes = 0;
	/* save test config */
	ret = goodix_save_test_config(ts_test, fp);
	if (ret < 0)  {
		ts_err("save test config failed");
		goto save_end;
	}

	bytes += sprintf(&data[bytes], "</Header>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("header write failed");
		goto save_end;
	}
	bytes = 0;

	/* item list */
	bytes += sprintf(&data[bytes], "<ItemList>\n");
	if (ts_test->test_result[GTP_CAP_TEST]) {
		if (GTP_TEST_PASS == ts_test->test_result[GTP_CAP_TEST])
			bytes += sprintf(&data[bytes],
					"<Item name=\"Rawdata MAX/MIN Test\" result=\"OK\"/>\n");
		else
			bytes += sprintf(&data[bytes],
					"<Item name=\"Rawdata MAX/MIN Test\" result=\"NG\"/>\n");
	}

	if (ts_test->test_result[GTP_DELTA_TEST]) {
		if (GTP_TEST_PASS == ts_test->test_result[GTP_DELTA_TEST])
			bytes += sprintf(&data[bytes],
					"<Item name=\"Rawdata Adjcent Deviation Test\" result=\"OK\"/>\n");
		else
			bytes += sprintf(&data[bytes],
					"<Item name=\"Rawdata Adjcent Deviation Test\" result=\"NG\"/>\n");
	}

	if (ts_test->test_result[GTP_NOISE_TEST]) {
		if (GTP_TEST_PASS == ts_test->test_result[GTP_NOISE_TEST])
			bytes += sprintf(&data[bytes],
					"<Item name=\"Diffdata Jitter Test\" result=\"OK\"/>\n");
		else
			bytes += sprintf(&data[bytes],
					"<Item name=\"Diffdata Jitter Test\" result=\"NG\"/>\n");
	}

	if (ts_test->test_result[GTP_SELFNOISE_TEST]) {
		if (GTP_TEST_PASS == ts_test->test_result[GTP_SELFNOISE_TEST])
			bytes += sprintf(&data[bytes],
					"<Item name=\"Self Diffdata Jitter Limit Test\" result=\"OK\"/>\n");
		else
			bytes += sprintf(&data[bytes],
					"<Item name=\"Self Diffdata Jitter Limit Test\" result=\"NG\"/>\n");
	}

	if (ts_test->test_result[GTP_SELFCAP_TEST]) {
		if (GTP_TEST_PASS == ts_test->test_result[GTP_SELFCAP_TEST])
			bytes += sprintf(&data[bytes], 
					"<Item name=\"Self Rawdata Upper Limit Test\" result=\"OK\"/>\n");
		else
			bytes += sprintf(&data[bytes],
					"<Item name=\"Self Rawdata Upper Limit Test\" result=\"NG\"/>\n");
	}

	if (ts_test->test_result[GTP_SHORT_TEST]) {
		if (GTP_TEST_PASS == ts_test->test_result[GTP_SHORT_TEST])
			bytes += sprintf(&data[bytes],
					"<Item name=\"Short Test\" result=\"OK\"/>\n");
		else
			bytes += sprintf(&data[bytes],
					"<Item name=\"Short Test\" result=\"NG\"/>\n");
	}

	bytes += sprintf(&data[bytes], "</ItemList>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("item list write failed");
		goto save_end;
	}

save_end:
	kfree(data);
	return ret;
}

static int goodix_save_limits(struct goodix_ts_test *ts_test,
		struct file *fp)
{
	int ret;
	int i;
	int bytes = 0;
	char *data = NULL;
	int tx = ts_test->test_params.drv_num;
	int rx = ts_test->test_params.sen_num;
	int chn1;
	int chn2;
	int r;

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed for ");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "<TestItems>\n");

	/* save short result */
	if (ts_test->test_result[GTP_SHORT_TEST]) {
		bytes += sprintf(&data[bytes], "<Item name=\"Short Test\">\n");
		bytes += sprintf(&data[bytes], "<ShortNum>%d</ShortNum>\n",
				ts_test->short_res.short_num);
		for (i = 0; i < ts_test->short_res.short_num; i++) {
			chn1 = ts_test->short_res.short_msg[4 * i];
			chn2 = ts_test->short_res.short_msg[4 * i + 1];
			r = (ts_test->short_res.short_msg[4 * i + 2] << 8) +
				ts_test->short_res.short_msg[4 * i + 3];
			if (chn1 == CHN_VDD)
				bytes += sprintf(&data[bytes], "<ShortMess Chn1=\"VDD\" ");
			else if (chn1 == CHN_GND)
				bytes += sprintf(&data[bytes], "<ShortMess Chn1=\"GND\" ");
			else if (chn1 & DRV_CHANNEL_FLAG)
				bytes += sprintf(&data[bytes], "<ShortMess Chn1=\"Tx%d\" ",
						chn1 & 0x7f);
			else
				bytes += sprintf(&data[bytes], "<ShortMess Chn1=\"Rx%d\" ",
						chn1 & 0x7f);
			if (chn2 == CHN_VDD)
				bytes += sprintf(&data[bytes],
						"Chn2=\"VDD\" ShortResistor= \"%dKom\"/>\n", r);
			else if (chn2 == CHN_GND)
				bytes += sprintf(&data[bytes],
						"Chn2=\"GND\" ShortResistor= \"%dKom\"/>\n", r);
			else if (chn2 & DRV_CHANNEL_FLAG)
				bytes += sprintf(&data[bytes],
						"Chn2=\"Tx%d\" ShortResistor= \"%dKom\"/>\n",
						chn2 & 0x7f, r);
			else
				bytes += sprintf(&data[bytes],
						"Chn2=\"Rx%d\" ShortResistor= \"%dKom\"/>\n",
						chn2 & 0x7f, r);
		}
		bytes += sprintf(&data[bytes], "</Item>\n");
		ret = fs_write(data, bytes, fp);
		if (ret < 0) {
			ts_err("short res write fail.");
			goto save_end;
		}
		bytes = 0;
	}

	/* rawdata max limit */
	bytes += sprintf(&data[bytes], "<Item name=\"Rawdata Test Sets\">\n");
	bytes += sprintf(&data[bytes], "<TotalFrameCnt>%d</TotalFrameCnt>\n",
			TOTAL_FRAME_NUM);
	bytes += sprintf(&data[bytes], "<MaxRawLimit>\n");
	for (i = 0; i < tx * rx; i++) {
		bytes += sprintf(&data[bytes], "%d,",
			ts_test->test_params.max_limits[i]);
		if ((i + 1) % tx == 0)
			bytes += sprintf(&data[bytes], "\n");
	}
	bytes += sprintf(&data[bytes], "</MaxRawLimit>\n");
	/* BeyondRawdataUpperLimit */
	bytes += sprintf(&data[bytes], "<BeyondRawdataUpperLimitCnt>\n");
	for (i = 0; i < tx * rx; i++) {
		bytes += sprintf(&data[bytes], "%d,",
				ts_test->open_res.beyond_max_limit_cnt[i]);
		if ((i + 1) % tx == 0)
			bytes += sprintf(&data[bytes], "\n");
	}
	bytes += sprintf(&data[bytes], "</BeyondRawdataUpperLimitCnt>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("rawdata limit write failed");
		goto save_end;
	}
	bytes = 0;

	/* rawdata min limit */
	bytes += sprintf(&data[bytes], "<MinRawLimit>\n");
	for (i = 0; i < tx * rx; i++) {
		bytes += sprintf(&data[bytes], "%d,",
			ts_test->test_params.min_limits[i]);
		if ((i + 1) % tx == 0)
			bytes += sprintf(&data[bytes], "\n");
	}
	bytes += sprintf(&data[bytes], "</MinRawLimit>\n");
	/* BeyondRawdataLower limit */
	bytes += sprintf(&data[bytes], "<BeyondRawdataLowerLimitCnt>\n");
	for (i = 0; i < tx * rx; i++) {
		bytes += sprintf(&data[bytes], "%d,",
				ts_test->open_res.beyond_min_limit_cnt[i]);
		if ((i + 1) % tx == 0)
			bytes += sprintf(&data[bytes], "\n");
	}
	bytes += sprintf(&data[bytes], "</BeyondRawdataLowerLimitCnt>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("rawdata limit write failed");
		goto save_end;
	}
	bytes = 0;

	/* Max Accord limit */
	bytes += sprintf(&data[bytes], "<MaxAccordLimit>\n");
	for (i = 0; i < tx * rx; i++) {
		bytes += sprintf(&data[bytes], "%d,",
			ts_test->test_params.deviation_limits[i]);
		if ((i + 1) % tx == 0)
			bytes += sprintf(&data[bytes], "\n");
	}
	bytes += sprintf(&data[bytes], "</MaxAccordLimit>\n");
	/* BeyondAccordLimitCnt */
	bytes += sprintf(&data[bytes], "<BeyondAccordLimitCnt>\n");
	for (i = 0; i < tx * rx; i++) {
		bytes += sprintf(&data[bytes], "%d,",
			ts_test->open_res.beyond_accord_limit_cnt[i]);
		if ((i + 1) % tx == 0)
			bytes += sprintf(&data[bytes], "\n");
	}
	bytes += sprintf(&data[bytes], "</BeyondAccordLimitCnt>\n");
	bytes += sprintf(&data[bytes], "</Item>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("rawdata limit write failed");
		goto save_end;
	}
	bytes = 0;

	/* save noise limit */
	if (ts_test->test_result[GTP_NOISE_TEST]) {
		bytes += sprintf(&data[bytes], "<Item name=\"Diffdata Test Sets\">\n");
		bytes += sprintf(&data[bytes], "<TotalFrameCnt>%d</TotalFrameCnt>\n",
				NOISEDATA_TEST_TIMES);
		bytes += sprintf(&data[bytes], "<MaxJitterLimit>%d</MaxJitterLimit>\n",
				ts_test->test_params.noise_threshold);
		bytes += sprintf(&data[bytes], "</Item>\n");
		ret = fs_write(data, bytes, fp);
		if (ret < 0) {
			ts_err("noise limit write failed");
			goto save_end;
		}
		bytes = 0;
	}

	/* save self rawdata limit */
	if (ts_test->test_result[GTP_SELFCAP_TEST]) {
		bytes += sprintf(&data[bytes], "<Item name=\"Self Rawdata Test Sets\">\n");
		bytes += sprintf(&data[bytes], "<TotalFrameCnt>1</TotalFrameCnt>\n");
		bytes += sprintf(&data[bytes], "<MaxRawLimit>\n");
		for (i = 0; i < tx + rx; i++) {
			bytes += sprintf(&data[bytes], "%d,",
					ts_test->test_params.self_max_limits[i]);
			if ((i + 1) % tx == 0)
				bytes += sprintf(&data[bytes], "\n");
		}
		if ((tx + rx) % tx != 0)
			bytes += sprintf(&data[bytes], "\n");
		bytes += sprintf(&data[bytes], "</MaxRawLimit>\n");
		bytes += sprintf(&data[bytes], "<MinRawLimit>\n");
		for (i = 0; i < tx + rx; i++) {
			bytes += sprintf(&data[bytes], "%d,",
					ts_test->test_params.self_min_limits[i]);
			if ((i + 1) % tx == 0)
				bytes += sprintf(&data[bytes], "\n");
		}
		if ((tx + rx) % tx != 0)
			bytes += sprintf(&data[bytes], "\n");
		bytes += sprintf(&data[bytes], "</MinRawLimit>\n");
		bytes += sprintf(&data[bytes], "</Item>\n");
		ret = fs_write(data, bytes, fp);
		if (ret < 0) {
			ts_err("self rawdata limit write failed");
			goto save_end;
		}
		bytes = 0;
	}

	/* save selfnoise limit */
	if (ts_test->test_result[GTP_SELFNOISE_TEST]) {
		bytes += sprintf(&data[bytes], "<Item name=\"Self Diffdata Test Sets\">\n");
		bytes += sprintf(&data[bytes], "<TotalFrameCnt>1</TotalFrameCnt>\n");
		bytes += sprintf(&data[bytes], "<MaxJitterLimit>%d</MaxJitterLimit>\n",
				ts_test->test_params.self_noise_threshold);
		bytes += sprintf(&data[bytes], "</Item>\n");
		ret = fs_write(data, bytes, fp);
		if (ret < 0) {
			ts_err("raw limit write failed");
			goto save_end;
		}
		bytes = 0;
	}

	bytes += sprintf(&data[bytes], "</TestItems>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0)
		ts_err("limit write fail.");

save_end:
	kfree(data);
	return ret;
}

static int goodix_save_rawdata(struct goodix_ts_test *ts_test,
		struct file *fp)
{
	int i;
	int j;
	int ret;
	int bytes = 0;
	s16 stat_result[3];
	char *data = NULL;
	int tx = ts_test->test_params.drv_num;
	int rx = ts_test->test_params.sen_num;
	int len = tx * rx;

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed for ");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "<RawDataRecord>\n");
	for (i = 0; i < TOTAL_FRAME_NUM; i++) {
		goodix_data_cal(ts_test->rawdata[i].data, len, stat_result);
		bytes += sprintf(&data[bytes],
			"<DataContent No.=\"%d\" DataCount=\"%d\" Maximum=\"%d\" Minimum=\"%d\" Average=\"%d\">\n",
			i, len, stat_result[1], stat_result[2], stat_result[0]);
		for (j = 0; j < len; j++) {
			bytes += sprintf(&data[bytes], "%d,", ts_test->rawdata[i].data[j]);
			if ((j + 1) % tx == 0)
				bytes += sprintf(&data[bytes], "\n");
		}
		bytes += sprintf(&data[bytes], "</DataContent>\n");
		goodix_data_cal(ts_test->accord_arr[i].data, len, stat_result);
		bytes += sprintf(&data[bytes],
			"<RawAccord No.=\"%d\" DataCount=\"%d\" Maximum=\"%d\" Minimum=\"%d\" Average=\"%d\">\n",
			i, len, stat_result[1], stat_result[2], stat_result[0]);
		for (j = 0; j < len; j++) {
			bytes += sprintf(&data[bytes], "%d,", ts_test->accord_arr[i].data[j]);
			if ((j + 1) % tx == 0)
				bytes += sprintf(&data[bytes], "\n");
		}
		bytes += sprintf(&data[bytes], "</RawAccord>\n");
		ret = fs_write(data, bytes, fp);
		if (ret < 0) {
			ts_err("rawdata write fail.");
			goto save_end;
		}
		bytes = 0;
	}

	bytes += sprintf(&data[bytes], "</RawDataRecord>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0)
		ts_err("rawdata write fail.");

save_end:
	kfree(data);
	return ret;
}

static int goodix_save_noise_data(struct goodix_ts_test *ts_test, struct file *fp)
{
	int i;
	int j;
	int ret = 0;
	int bytes = 0;
	s16 stat_result[3];
	char *data = NULL;
	int tx = ts_test->test_params.drv_num;
	int rx = ts_test->test_params.sen_num;
	int len = tx * rx;

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed for ");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "<DiffDataRecord>\n");
	for (i = 0; i < NOISEDATA_TEST_TIMES; i++) {
		goodix_data_cal(ts_test->noisedata[i].data, len, stat_result);
		bytes += sprintf(&data[bytes],
			"<DataContent No.=\"%d\" DataCount=\"%d\" Maximum=\"%d\" Minimum=\"%d\" Average=\"%d\">\n",
			i, len, stat_result[1], stat_result[2], stat_result[0]);
		for (j = 0; j < len; j++) {
			bytes += sprintf(&data[bytes], "%d,", ts_test->noisedata[i].data[j]);
			if ((j + 1) % tx == 0)
				bytes += sprintf(&data[bytes], "\n");
		}
		bytes += sprintf(&data[bytes], "</DataContent>\n");
		ret = fs_write(data, bytes, fp);
		if (ret < 0) {
			ts_err("noisedata write fail.");
			goto save_end;
		}
		bytes = 0;
	}

	bytes += sprintf(&data[bytes], "</DiffDataRecord>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0)
		ts_err("noisedata write fail.");

save_end:
	kfree(data);
	return ret;
}

static int goodix_save_self_data(struct goodix_ts_test *ts_test,
		struct file *fp, s16 *src_data, u8 *title, int len)
{
	int i;
	int ret = 0;
	s32 bytes = 0;
	char *data;
	s16 stat_result[3];
	int tx = ts_test->test_params.drv_num;

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed for ");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "<%s>\n",title);
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("rawdata write fail.");
		goto save_end;
	}
	bytes = 0;

	goodix_data_cal(src_data, len, stat_result);
	bytes += sprintf(&data[bytes],
		"<DataContent No.=\"0\" DataCount=\"%d\" Maximum=\"%d\" Minimum=\"%d\" Average=\"%d\">\n",
		len, stat_result[1], stat_result[2], stat_result[0]);
	for (i = 0; i < len; i++) {
		bytes += sprintf(&data[bytes], "%d,", src_data[i]);
		if ((i + 1) % tx == 0)
			bytes += sprintf(&data[bytes], "\n");
	}
	if (len % tx != 0)
		bytes += sprintf(&data[bytes], "\n");
	bytes += sprintf(&data[bytes], "</DataContent>\n");
	bytes += sprintf(&data[bytes], "</%s>\n",title);
	ret = fs_write(data, bytes, fp);
	if (ret < 0)
		ts_err("rawdata write fail.");

save_end:
	kfree(data);
	return ret;
}

static int goodix_save_data(struct goodix_ts_test *ts_test,
		struct file *fp)
{
	int ret;
	int bytes = 0;
	char *data = NULL;

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed for ");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "<DataRecord>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0) {
		ts_err("rawdata record lable failed");
		goto save_end;
	}
	bytes = 0;

	ret = goodix_save_rawdata(ts_test, fp);
	if (ret < 0)
		goto save_end;

	if (ts_test->test_result[GTP_NOISE_TEST]) {
		ret = goodix_save_noise_data(ts_test, fp);
		if (ret < 0)
			goto save_end;
	}

	if (ts_test->test_result[GTP_SELFCAP_TEST]) {
		ret = goodix_save_self_data(ts_test, fp, ts_test->self_rawdata.data,
				"selfDataRecord", ts_test->self_rawdata.size);
		if (ret < 0)
			goto save_end;
	}

	if (ts_test->test_result[GTP_SELFNOISE_TEST]) {
		ret = goodix_save_self_data(ts_test, fp, ts_test->self_noisedata.data,
				"selfDiffDataRecord", ts_test->self_noisedata.size);
		if (ret < 0)
			goto save_end;
	}

	bytes += sprintf(&data[bytes], "</DataRecord>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0)
		ts_err("rawdata data record lable fail.");

save_end:
	kfree(data);
	return ret;
}

/* save end tag in csv file */
static int goodix_save_tail(struct goodix_ts_test *ts_test,
		struct file *fp)
{
	int ret = 0;
	int bytes = 0;
	char *data = NULL;

	data = kzalloc(MAX_DATA_BUFFER, GFP_KERNEL);
	if (!data) {
		ts_err("alloc memory failed for ");
		return -ENOMEM;
	}

	bytes += sprintf(&data[bytes], "</TESTLOG>\n");
	ret = fs_write(data, bytes, fp);
	if (ret < 0)
		ts_err("tail write failed");

	kfree(data);
	return ret;
}

static void goodix_save_result_data(struct goodix_ts_test *ts_test)
{
	int ret = 0;
	char save_path[100];
	struct file *fp = NULL;

	/* format result file */
	/* FORMAT_PATH(save_path, GOODIX_RESULT_SAVE_PATH, "Test_Data", ".csv"); */
	sprintf(save_path, GOODIX_RESULT_SAVE_PATH);
	ts_info("save result IN, file_name:%s", save_path);

	fp = filp_open(save_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (IS_ERR(fp)) {
		ts_err("create file:%s failed, fp:%ld", save_path, PTR_ERR(fp));
		return;
	}

	/* save header */
	ret = goodix_save_header(ts_test, fp);
	if (ret < 0)
		goto save_end;

	/* save limits */
	ret = goodix_save_limits(ts_test, fp);
	if (ret < 0)
		goto save_end;

	/* save data */
	ret = goodix_save_data(ts_test, fp);
	if (ret < 0)
		goto save_end;

	/* save tail */
	ret = goodix_save_tail(ts_test, fp);
	if (ret < 0)
		goto save_end;

	ts_info("the test result save in %s", save_path);
save_end:
	filp_close(fp, NULL);
}
#endif /* SAVE_IN_CSV */

static void goodix_put_test_result(struct goodix_ts_test *ts_test,
		struct ts_rawdata_info *info)
{
	int i;
	bool have_bus_error = false;
	bool have_panel_error = false;
	char statistics_data[STATISTICS_DATA_LEN] = {0};
	struct goodix_ts_core *ts = ts_test->ts;

	ts_info("put test result IN");

	info->buff[0] = ts_test->test_params.sen_num;
	info->buff[1] = ts_test->test_params.drv_num;
	info->used_size = 2;
	/* save rawdata to info->buff, only one frame */
	if (ts_test->rawdata[0].size) {
		for (i = 0; i < ts_test->rawdata[0].size; i++)
			info->buff[info->used_size + i] = ts_test->rawdata[0].data[i];
		info->used_size += ts_test->rawdata[0].size;
	}

	/* save noisedata to info->buff */
	if (ts_test->noisedata[0].size) {
		for (i = 0; i < ts_test->noisedata[0].size; i++)
			info->buff[info->used_size + i] = ts_test->noisedata[0].data[i];
		info->used_size += ts_test->noisedata[0].size;
	}

	/* save self_noisedata to info->buff */
	if (ts_test->self_noisedata.size) {
		for (i = 0; i < ts_test->self_noisedata.size; i++)
			info->buff[info->used_size + i] = ts_test->self_noisedata.data[i];
		info->used_size += ts_test->self_noisedata.size;
	}

	/* save self_rawdata to info->buff */
	if (ts_test->self_rawdata.size) {
		for (i = 0; i < ts_test->self_rawdata.size; i++)
			info->buff[info->used_size + i] = ts_test->self_rawdata.data[i];
		info->used_size += ts_test->self_rawdata.size;
	}

	/* check if there have bus error */
	for (i = 0; i < MAX_TEST_ITEMS; i++) {
		if (ts_test->test_result[i] == SYS_SOFTWARE_REASON)
			have_bus_error = true;
		else if (ts_test->test_result[i] == GTP_PANEL_REASON)
			have_panel_error = true;
	}
	ts_info("Have bus error:%d", have_bus_error);
	if (have_bus_error || have_panel_error)
		goodix_strncat(ts_test->test_info, "[FAIL]-", TS_RAWDATA_RESULT_MAX);
	else
		goodix_strncat(ts_test->test_info, "[PASS]-", TS_RAWDATA_RESULT_MAX);

	if (have_bus_error)
		goodix_strncat(ts_test->test_info, "0F-", TS_RAWDATA_RESULT_MAX);
	else
		goodix_strncat(ts_test->test_info, "0P-", TS_RAWDATA_RESULT_MAX);

	for (i = 0; i < MAX_TEST_ITEMS; i++) {
		/* if have tested, show result */
		if (ts_test->test_result[i]) {
			if (GTP_TEST_PASS == ts_test->test_result[i])
				goodix_strncatint(ts_test->test_info, i, "%dP-",
					TS_RAWDATA_RESULT_MAX);
			else
				goodix_strncatint(ts_test->test_info, i, "%dF-",
					TS_RAWDATA_RESULT_MAX);
		}
	}

	/* calculate rawdata min avg max value*/
	if (ts_test->rawdata[0].size) {
		goodix_data_statistics(
				ts_test->rawdata[0].data,
				ts_test->rawdata[0].size,
				statistics_data,
				STATISTICS_DATA_LEN);
		goodix_strncat(ts_test->test_info, statistics_data,
			TS_RAWDATA_RESULT_MAX);
	} else {
		ts_err("NO valiable rawdata");
		goodix_strncat(ts_test->test_info, "[0,0,0]",
			TS_RAWDATA_RESULT_MAX);
	}

#ifdef NOISE_DATA_TEST
	/* calculate noisedata min avg max value*/
	if (ts_test->noisedata[0].size) {
		goodix_data_statistics(
				ts_test->noisedata[0].data,
				ts_test->noisedata[0].size,
				statistics_data,
				STATISTICS_DATA_LEN);
		goodix_strncat(ts_test->test_info, statistics_data,
			TS_RAWDATA_RESULT_MAX);
	} else {
		ts_err("NO valiable noisedata");
		goodix_strncat(ts_test->test_info, "[0,0,0]",
			TS_RAWDATA_RESULT_MAX);
	}
#endif /* NOISE_DATA_TEST */

#ifdef SELF_RAWDATA_TEST
	/* calculate self_rawdata min avg max value*/
	if (ts_test->self_rawdata.size) {
		goodix_data_statistics(
				ts_test->self_rawdata.data,
				ts_test->self_rawdata.size,
				statistics_data,
				STATISTICS_DATA_LEN);
		goodix_strncat(ts_test->test_info, statistics_data,
			TS_RAWDATA_RESULT_MAX);
	} else {
		ts_err("NO valiable self_rawdata");
		goodix_strncat(ts_test->test_info, "[0,0,0]",
			TS_RAWDATA_RESULT_MAX);
	}
#endif /* SELF_RAWDATA_TEST */

#ifdef SELFNOISE_DATA_TEST
	/* calculate self_noisedata min avg max value*/
	if (ts_test->self_noisedata.size) {
		goodix_data_statistics(
				ts_test->self_noisedata.data,
				ts_test->self_noisedata.size,
				statistics_data,
				STATISTICS_DATA_LEN);
		goodix_strncat(ts_test->test_info, statistics_data,
			TS_RAWDATA_RESULT_MAX);
	} else {
		ts_err("NO valiable self_noisedata");
		goodix_strncat(ts_test->test_info, "[0,0,0]",
			TS_RAWDATA_RESULT_MAX);
	}
#endif /* SELFNOISE_DATA_TEST */

	goodix_strncat(ts_test->test_info, "-GT",
		TS_RAWDATA_RESULT_MAX);
	goodix_strncat(ts_test->test_info, ts->fw_version.patch_pid,
		TS_RAWDATA_RESULT_MAX);
	goodix_strncat(ts_test->test_info, "\n",
		TS_RAWDATA_RESULT_MAX);
	strncpy(info->result, ts_test->test_info, TS_RAWDATA_RESULT_MAX - 1);

#ifdef SAVE_IN_CSV
	/* save result to file */
	goodix_save_result_data(ts_test);
#endif
}

int goodix_get_rawdata(struct device *dev, struct ts_rawdata_info *info)
{
	int ret;
	struct goodix_ts_test *ts_test = NULL;

	ts_info("tp test code version:%s", TEST_CODE_VERSION);

	if (!dev || !info) {
		ts_err("ts_dev or info is NULL");
		return -ENODEV;
	}

	ts_test = kzalloc(sizeof(*ts_test), GFP_KERNEL);
	if (!ts_test) {
		ts_err("Failed to alloc mem");
		return -ENOMEM;
	}

	ts_test->ts = dev_get_drvdata(dev);
	ret = goodix_tptest_prepare(ts_test);
	if (ret < 0) {
		ts_err("Failed to prepare TP test, exit");
		strncpy(info->result, "[FAIL]-0F-software reason",
			TS_RAWDATA_RESULT_MAX - 1);
		goto exit_finish;
	}
	ts_info("TP test prepare OK");

	goodix_capacitance_test(ts_test); /* 1F 3F 6F 7F test */
#ifdef SHORT_TEST
	goodix_shortcircut_test(ts_test); /* 5F test */
#endif /* SHORT_TEST */
	goodix_put_test_result(ts_test, info);
	goodix_tptest_finish(ts_test);

exit_finish:
	kfree(ts_test);
	return ret;
}
