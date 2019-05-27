/* drivers/input/touchscreen/gt1x_test.c
 *

 * 2010 - 2014 Shenzhen Huiding Technology Co.,Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.0
 * Release Date: 2014/11/14
 *
 */

#include <linux/string.h>
#include "test_function.h"
#include "gt1x_generic.h"
#include "dsp_isp.h"

#define REG_CONFIG_BASE       0x8050
#define REG_DRV_SEN_CNT       0x8086
#define REG_MATRIX_DRV_SEN    0x806E
#define REG_MODULE_SWITCH_DK  0x8057
#define REG_MODULE_SWITCH_SK  0x80A1

#define GT1X_ANDROID_TOUCH           "android_touch"
#define GT1X_ITO_TEST                "tp_selftest"
#define CONFIG_GTP_ESD_PROTECT       1

static struct proc_dir_entry *gt1x_ito_test_proc;



static unsigned char *upload_short_data;
static unsigned char *warn_short_data;
static unsigned short *self_data;
static bool drv_sen_exchange;
static char *self_test_state;

static const unsigned char gt1143_sen_map[40] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 21, 24, 26, 28, 30,
	0, 2, 4, 6, 8, 12, 13, 14, 15, 19, 21, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

struct system_variable sys;
static mm_segment_t old_fs;
static loff_t file_pos;
extern void gt1x_esd_switch(s32 on);

FILE *fopen(const char *path, const char *mode)
{

	FILE *filp = NULL;
	if (!strcmp(mode, "a+")) {
		/*if(file_pos == 0) {
			filp = filp_open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
		}else*/ {
			filp = filp_open(path, O_RDWR | O_CREAT, 0666);
		}
		if (!IS_ERR(filp)) {
			filp->f_op->llseek(filp, 0, SEEK_END);

		}

		if (filp == NULL){
			GTP_ERROR("open file as a+ mode filp == NULL 1\n");
		}

	} else if (!strcmp(mode, "r")) {

		filp = filp_open(path, O_RDONLY, 0666);

	}

/* 	if (IS_ERR(filp)) {

		GTP_ERROR("fopen file not exist");

		return NULL;

	} */

	old_fs = get_fs();

	set_fs(KERNEL_DS);
	if (filp == NULL){
		GTP_ERROR("open file as a+ mode filp == NULL 2\n");
	}
	return filp;
}

int fclose(FILE *filp)
{
	filp_close(filp, NULL);

	filp = NULL;

	set_fs(old_fs);

	return 0;
}

 size_t fread(void *buffer, size_t size, size_t count, FILE *filp)
{
	return filp->f_op->read(filp, (char *)buffer, count, &filp->f_pos);
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *filp)
{
	ssize_t  writeCount = -1;
	GTP_DEBUG("[%s] enter",__func__);
	writeCount = vfs_write(filp, (char __user *)buffer, size, &filp->f_pos);

	GTP_DEBUG("[%s] exit",__func__);
	return writeCount;
}

char *strtok(char *s, const char *delim)
{

	const char *spanp;

	int c, sc;

	char *tok;

	static char *last;
	if (s == NULL && (s = last) == NULL) {

		return (NULL);

	}

	/* Skip (span) leading delimiters (s += strspn(s, delim), sort of). */
cont:
	c = *s++;

	for (spanp = delim; (sc = *spanp++) != 0;) {

		if (c == sc)

			goto cont;

	}

	if (c == 0) {		/* no non-delimiter characters */

		last = NULL;

		return (NULL);

	}

	tok = s - 1;

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {

		c = *s++;

		spanp = delim;

		do {

			if ((sc = *spanp++) == c) {

				if (c == 0)

					s = NULL;

				else

					s[-1] = 0;

				last = s;

				return (tok);

			}

		} while (sc != 0);

	}

	/* NOTREACHED */
}

int atoi(const char *str)
{

	int res = 0;

	char sign = '+';

	const char *pStr = str;
	/* delete space */
	while (*pStr == ' ')

		pStr++;

	/* judge +/- */
	if (*pStr == '+' || *pStr == '-')

		sign = *pStr++;

	/* calc abs */
	while (*pStr >= '0' && *pStr <= '9') {

		res = res * 10 + *pStr - '0';

		pStr++;

	}

	return sign == '-' ? -res : res;
}

int is_digit(char ch)
{
	if (ch >= '0' && ch <= '9')

		return 1;

	else

		return 0;
}

int is_space(char ch)
{
	if (ch == ' ')

		return 1;

	else

		return 0;
}

/* return value = float value * 1000; */
long atof(char const *s)
{

	long power, value;

	int i, sign, j = 0;
	for (i = 0; is_space(s[i]); i++) ;

	sign = (s[i] == '-') ? -1 : 1;

	if (s[i] == '-' || s[i] == '+')

		i++;

	for (value = 0; is_digit(s[i]); i++)

		value = value * 10 + (s[i] - '0');

	if (s[i] == '.')

		i++;

	for (power = 1; (is_digit(s[i]) && j < 3); i++, j++) {

		value = value * 10 + (s[i] - '0');

		power *= 10;

	}

	return sign * value * FLOAT_AMPLIFIER / power;
}

int i2c_read_data(unsigned short addr, unsigned char *data, int length)
{

	int ret;
	ret = gt1x_i2c_read(addr, data, length);

	if (ret >= 0) {

		return length;

	}

	return ret;
}

int i2c_write_data(unsigned short addr, unsigned char *data, int length)
{

	int ret;
	ret = gt1x_i2c_write(addr, data, length);

	if (ret >= 0) {

		return length;

	}

	return ret;
}

int enable_irq_esd(void)
{
	gt1x_irq_enable();

#if CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_ON);

#endif

	return 1;
}

int disable_irq_esd(void)
{
	gt1x_irq_disable();

#if CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_OFF);
	msleep(2000);
#endif

	return 1;
}

int reset_guitar(void)
{

	int ret;
	ret = gt1x_reset_guitar();

	if (ret >= 0) {

		return 1;

	}

	return ret;
}

int enter_update_mode(void)
{
	return 1;
}

int read_sensorid(void)
{
	return gt1x_version.sensor_id;
}

int parse_config(unsigned char *config)
{
	if (config == NULL) {

		return PARAMETERS_ILLEGL;

	}

	GTP_ERROR("product id %s", gt1x_version.product_id);

	if (!strncmp((const char *)gt1x_version.product_id, "1143", 4)) {

		sys.driver_num = config[REG_MATRIX_DRV_SEN - REG_CONFIG_BASE];

		sys.sensor_num = config[REG_MATRIX_DRV_SEN + 1 - REG_CONFIG_BASE];

		sys.sc_driver_num = sys.driver_num - (config[REG_MODULE_SWITCH_DK - REG_CONFIG_BASE] & 0x01);

		sys.sc_sensor_num = sys.sensor_num;

		sys.key_number = (config[46] & 0x1F) + (config[47] & 0x1F) + (config[48] & 0xFF) - sys.sc_driver_num * sys.sc_sensor_num;

                			if (config[0x80dc - 0x8050] < 20)
			{
				drv_sen_exchange = true;
			}
			else
			{
				drv_sen_exchange = false;
			}
	}

	else if (!strncmp((const char *)gt1x_version.product_id, "1151", 4)) {
		sys.driver_num = (config[REG_DRV_SEN_CNT - REG_CONFIG_BASE] & 0x1F) + (config[REG_DRV_SEN_CNT + 1 - REG_CONFIG_BASE] & 0x1F);

		sys.sensor_num = (config[REG_DRV_SEN_CNT + 2 - REG_CONFIG_BASE] & 0xFF);

		if (config[REG_MODULE_SWITCH_DK - REG_CONFIG_BASE] & 0x01) {

			sys.sc_sensor_num = sys.sensor_num - (config[REG_MODULE_SWITCH_SK - REG_CONFIG_BASE] & 0x01);

			if (config[0x805A - REG_CONFIG_BASE] & 0x01) {

				sys.sc_driver_num = sys.driver_num;

			} else {

				sys.sc_driver_num = sys.driver_num - (config[REG_MODULE_SWITCH_DK - REG_CONFIG_BASE] & 0x01);

			}

		}

		else {
			sys.sc_driver_num = sys.driver_num;

			sys.sc_sensor_num = sys.sensor_num;

		}

		sys.key_number = (sys.driver_num * sys.sensor_num - sys.sc_driver_num * sys.sc_sensor_num);

	} else {

		sys.driver_num = (config[REG_DRV_SEN_CNT - REG_CONFIG_BASE] & 0x1F) + (config[REG_DRV_SEN_CNT + 1 - REG_CONFIG_BASE] & 0x1F);

		sys.sensor_num = (config[REG_DRV_SEN_CNT + 2 - REG_CONFIG_BASE] & 0xFF);

		sys.sc_driver_num = sys.driver_num - (config[REG_MODULE_SWITCH_DK - REG_CONFIG_BASE] & 0x01);

		sys.sc_sensor_num = sys.sensor_num;

		sys.key_number = (sys.driver_num * sys.sensor_num - sys.sc_driver_num * sys.sc_sensor_num);

	}

	GTP_ERROR("drv %d, sen %d", sys.driver_num, sys.sensor_num);

	return 1;
}

int read_config(unsigned char *config, int len)
{

	int ret = -1;
	ret = gt1x_i2c_read(GTP_REG_CONFIG_DATA, config, len);

	if (ret >= 0) {

		return len;

	}

	return ret;
}

int send_config(unsigned char *config, int len)
{

	int ret;
	ret = gt1x_send_cfg(config, len);

	if (ret >= 0) {

		return len;

	}

	return ret;
}

int check_version(unsigned char *ini_version)
{

	int ret;

	u8 tmp[20];
	ret = strncmp((const char *)gt1x_version.product_id, (const char *)ini_version, 4);

	if (ret != 0) {

		GTP_ERROR("PID not match.");

		return -EPERM;

	}

	sprintf((char *)tmp, "%d.%d.%d_%d.%d", (gt1x_version.mask_id >> 16) & 0x0FF, (gt1x_version.mask_id >> 8) & 0x0FF, (gt1x_version.mask_id & 0xFF),
		(gt1x_version.patch_id >> 16) & 0x0FF, (gt1x_version.patch_id & 0xFF));

	ret = strncmp((char *)tmp, (const char *)(&ini_version[5]), strlen((const char *)ini_version) - 5);

	GTP_ERROR("ic version %s and ini %s compare result %d", tmp, ini_version, ret);

	if (ret > 0) {

		return 2;

	}

	else if (ret < 0) {

		return 1;

	}

	return 0;
}

int read_raw_data(unsigned short *data, int len)
{

	int ret, i, index;

	u16 *rawdata;

	u8 *buf;

	u8 flag = 0x00;
	GTP_ERROR("[%s] enter.\n",__func__);

	if (data == NULL || len < sys.sensor_num * sys.driver_num) {

		GTP_ERROR("the parameters is null.");

		return PARAMETERS_ILLEGL;

	}

	gt1x_rawdiff_mode = true;

	ret = gt1x_send_cmd(1, 0);

	if (ret < 0) {

		GTP_ERROR("enter rawdiff mode fail.");

		return ret;

	}

	i = 0;

	while (i++ < 50) {

		gt1x_i2c_read(GTP_READ_COOR_ADDR, &flag, 1);

		if ((flag & 0x80) == 0x80) {

			break;

		}

		msleep(20);

	}

	if (i > 50) {

		GTP_ERROR("wait 0x80 timeout.");

		return -EPERM;

	}

	buf = (unsigned char *)malloc(sys.sensor_num * sys.driver_num * 4);
	if (buf == NULL) {
		return MEMORY_ERR;
	}
	rawdata = (unsigned short *)(&buf[sys.sensor_num * sys.driver_num * 2]);

	GTP_ERROR("0x%X read %d*%d data", sys.reg_rawdata_base, sys.sensor_num, sys.driver_num);

	ret = gt1x_i2c_read(sys.reg_rawdata_base, buf, sys.sensor_num * sys.driver_num * 2);

	if (ret < 0) {

		GTP_ERROR("read raw data fail.");
		free(buf);
		return ret;

	}

	flag = 0x00;

	gt1x_i2c_write(GTP_READ_COOR_ADDR, &flag, 1);

	for (i = 0, index = 0; i < sys.sensor_num * sys.driver_num * 2; i += 2) {

		rawdata[index++] = (buf[i] << 8) + (buf[i + 1] & 0x0ff);

	}

	memcpy(data, rawdata, sys.sensor_num * sys.driver_num * 2);
       if (drv_sen_exchange)
       {
    	   memcpy(&data[0],&rawdata[sys.sc_sensor_num*sys.sc_driver_num/2],sys.sc_sensor_num*sys.sc_driver_num);
    	   memcpy(&data[sys.sc_sensor_num*sys.sc_driver_num/2],&rawdata[0],sys.sc_sensor_num*sys.sc_driver_num);
       }
	free(buf);
	GTP_ERROR("[%s]exit.\n",__func__);

	return sys.sensor_num * sys.driver_num;
}

int be_normal(void)
{

	int ret;

	unsigned char end_cmd = 0;

	gt1x_rawdiff_mode = false;
	ret = gt1x_send_cmd(0, 0);

	ret |= gt1x_i2c_write(GTP_READ_COOR_ADDR, &end_cmd, 1);

	if (ret >= 0) {
		return 1;
	}

	GTP_ERROR("be noraml failed.");

	return ret;
}

int disable_hopping(unsigned char *config, int len, int flag)
{
	char i = 0;
	unsigned char *tmp_config;
	tmp_config = (unsigned char *)malloc(len);
	memset(tmp_config, 0, len);

	if (flag) {
		config[63] &= 0x7F;
	}

	do {
		send_config(config, len);
		msleep(250);
		read_config(tmp_config, len);
		if (!memcmp(&config[1], &tmp_config[1], len - 4)) {
			break;
		}
	} while (++i < 1);

	free(tmp_config);
	return i;
}

int enter_update_mode_noreset(void)
{

	int ret = -1;

	unsigned char buf[1];

	buf[0] = 0x00;
	i2c_write_data(_rRW_MISCTL__BOOT_CTL_, buf, 1);

	/* CPU_SWRST */
	buf[0] = 0x01;

	i2c_write_data(_bWO_MISCTL__CPU_SWRST_PULSE, buf, 1);

	/* disable watchdog */
	buf[0] = 0x00;

	i2c_write_data(_bRW_MISCTL__TMR0_EN, buf, 1);

	buf[0] = 0x02;

	i2c_write_data(_rRW_MISCTL__BOOTCTL_B0_, buf, 1);

	/* set scramble */
	buf[0] = 0x00;

	ret = i2c_write_data(_rRW_MISCTL__BOOT_OPT_B0_, buf, 1);

	if (ret <= 0) {

		GTP_ERROR("set scramble fail.");

		return -EPERM;

	}
	/* clr bu */
	buf[0] = 0x00;

	ret = i2c_write_data(_bRW_MISCTL__CACHE_EN, buf, 1);

	if (ret <= 0) {

		GTP_ERROR("clr bu fail.");
		return -EPERM;
	}

	buf[0] = 0x04;

	i2c_write_data(_bRW_MISCTL__SRAM_BANK, buf, 1);

	/* patch0_en */
	buf[0] = 0x01;

	i2c_write_data(0x404D, buf, 1);

	return 1;
}

int dsp_fw_startup(unsigned char dsp_flag)
{

	unsigned char buf[8];

	/* patch0_en */
	buf[0] = 0x00;
	i2c_write_data(0x404D, buf, 1);

	memset(buf, 0x55, sizeof(buf));

	i2c_write_data(0x8006, buf, 8);

	buf[0] = 0x08;

	i2c_write_data(_rRW_MISCTL__SWRST_B0_, buf, 1);

	buf[0] = 0xA0;

	i2c_write_data(0X4274, buf, 1);

	return 1;
}

void test_to_show(unsigned char *test_result, unsigned short upload_cnt)
{

	unsigned char m, i, j;

	unsigned char _GT1143_DRV_TOTAL_NUM = 20, _GT1143_SEN_TOTAL_NUM = 20;
	if (!strncmp((const char *)gt1x_version.product_id, (const char *)"1143", 4)) {

		for (i = 0; i < (upload_cnt * 4); i++) {

			if ((test_result[i] & _CHANNEL_TX_FLAG) == _CHANNEL_TX_FLAG) {

				m = test_result[i] & 0x7f;

				for (j = 0; j < _GT1143_DRV_TOTAL_NUM; j++) {

					if (gt1143_sen_map[_GT1143_SEN_TOTAL_NUM + j] == m) {

						test_result[i] = _GT1143_SEN_TOTAL_NUM + j;

						break;

					}

				}

			}

			else {

				m = test_result[i];

				for (j = 0; j < _GT1143_DRV_TOTAL_NUM; j++) {

					if (gt1143_sen_map[j] == m) {

						test_result[i] = j;

						break;

					}

				}

			}

			if ((i % 2) == 1)

				i += 2;

		}

	}
}

void write_test_params_bak(u8 *config)
{

	u8 data[sys.max_driver_num + sys.max_sensor_num], i, m;

	u16 drv_offest, sen_offest;

	u8 chksum = 0;
	/* gt900_short_threshold */
	data[0] = (gt900_short_threshold >> 8) & 0xff;

	data[1] = gt900_short_threshold & 0xff;

	i2c_write_data(0x8808, data, 2);

	/* ADC Read Delay */
	data[0] = (150 >> 8) & 0xff;

	data[1] = 150 & 0xff;

	i2c_write_data(0x880C, data, 2);

	/* DiffCode Short Threshold */
	data[0] = (20 >> 8) & 0xff;

	data[1] = 20 & 0xff;

	i2c_write_data(0x880A, data, 2);

	memset(data, 0xFF, sys.max_driver_num + sys.max_sensor_num);

	if (!strncmp((const char *)gt1x_version.product_id, "1143", 4)) {

		drv_offest = 0x80f0 - 0x8050;

		sen_offest = 0x80dc - 0x8050;

		for (i = 0, m = 0; i < 20; i++) {

			if (config[sen_offest + i] < 20)

				data[m++] = gt1143_sen_map[config[sen_offest + i]];

		}

		for (i = 0, m = 0; i < 20; i++) {

			if ((config[drv_offest + i] >= 20) && (config[drv_offest + i] < 40))
				data[sys.max_sensor_num + m++] = gt1143_sen_map[config[drv_offest + i]];

		}

	}

	else {

		drv_offest = 0x80fc - 0x8050;

		sen_offest = 0x80dc - 0x8050;

		for (i = 0, m = 0; i < sys.max_sensor_num; i++) {

			if (config[sen_offest + i] == 0xFF)

				break;

			data[m++] = config[sen_offest + i];

		}

		for (i = 0, m = 0; i < sys.max_driver_num; i++) {

			if (config[drv_offest + i] == 0xFF)

				break;

			data[sys.max_sensor_num + m++] = config[drv_offest + i];

		}

	}

	for (i = 0; i < sys.max_driver_num + sys.max_sensor_num; i++) {

		chksum += data[i];

	}

	chksum = 0 - chksum;

	i2c_write_data(0x880E, &data[sys.max_sensor_num], sys.max_driver_num);

	i2c_write_data(0x882E, data, sys.max_sensor_num);

	i2c_write_data(0x884E, &chksum, 1);

	GTP_ERROR("write test parameters sen%d*drv%d", sys.max_sensor_num, sys.max_driver_num);
}

void write_test_params(unsigned char *config)
{
	unsigned char data[sys.max_driver_num + sys.max_sensor_num], i, j;
	unsigned char tmp[sys.max_driver_num + sys.max_sensor_num], k;
	unsigned short drv_offest, sen_offest;
	unsigned char chksum = 0;

	data[0] = (gt900_short_threshold >> 8) & 0xff;
	data[1] = gt900_short_threshold & 0xff;
	i2c_write_data(0x8808, data, 2);


	data[0] = (150 >> 8) & 0xff;
	data[1] = 150 & 0xff;
	i2c_write_data(0x880C, data, 2);


	data[0] = (20 >> 8) & 0xff;
	data[1] = 20 & 0xff;
	i2c_write_data(0x880A, data, 2);

	memset(data, 0xFF, sys.max_driver_num + sys.max_sensor_num);
	switch (sys.chip_type) {
	case _GT1143:
		{
			drv_offest = 0x80f0 - 0x8050;
			sen_offest = 0x80dc - 0x8050;
			for (i = 0, k = 0, j = 0; i < 40; i++) {
				if (config[sen_offest + i] < 20) {
					if ((i % 10) < sys.sensor_num) {
						data[j++] = gt1143_sen_map[config[sen_offest + i]];
					} else {
						tmp[k++] = gt1143_sen_map[config[sen_offest + i]];
					}
				}
			}
			memcpy(&data[j], tmp, k);

			for (i = 0, j = 0, k = 0; i < 40; i++) {
				if ((config[sen_offest + i] >= 20) && (config[sen_offest + i] < 40)) {
					if ((i % 10) < sys.sensor_num) {
						data[sys.max_driver_num + j++] = gt1143_sen_map[config[sen_offest + i]];
					} else {
						tmp[k++] = gt1143_sen_map[config[sen_offest + i]];
					}
				}
			}
			memcpy(&data[sys.max_driver_num + j], tmp, k);

			break;
		}
	default:
		{
			drv_offest = 0x80fc - 0x8050;
			sen_offest = 0x80dc - 0x8050;

			for (i = 0, j = 0; i < sys.max_sensor_num; i++) {
				if (config[sen_offest + i] == 0xFF)
					break;
				data[j++] = config[sen_offest + i];
			}
			for (i = 0, j = 0; i < sys.max_driver_num; i++) {
				if (config[drv_offest + i] == 0xFF)
					break;
				data[sys.max_sensor_num + j++] = config[drv_offest + i];
			}
			break;
		}
	}

	for (i = 0; i < sys.max_driver_num + sys.max_sensor_num; i++) {
		chksum += data[i];
	}
	chksum = 0 - chksum;
	i2c_write_data(0x880E, &data[sys.max_sensor_num], sys.max_driver_num);

	i2c_write_data(0x882E, data, sys.max_sensor_num);

	i2c_write_data(0x884E, &chksum, 1);

	data[0] = 0x01;
	i2c_write_data(0x813E, data, 1);
}

static long _cal_resistance(u16 self_data, u8 master, u8 slave, u16 shor_code)
{
	return (long)self_data *81 * FLOAT_AMPLIFIER / shor_code - (81 * FLOAT_AMPLIFIER);
}

static int _save_short_data(u8 *short_data, strShortRecord r_data, u16 short_code)
{

	u8 n, cnt;

	cnt = short_data[0];
	for (n = 0; n < cnt; n++) {

		if (short_data[n * 4 + 1] == r_data.master && short_data[n * 4 + 2] == r_data.slave) {

			break;

		}

		if (short_data[n * 4 + 1] == r_data.slave && short_data[n * 4 + 2] == r_data.master) {

			break;

		}

	}

	if (n >= cnt) {

		short_data[1 + cnt * 4 + 0] = r_data.master;

		short_data[1 + cnt * 4 + 1] = r_data.slave;

		short_data[1 + cnt * 4 + 2] = (short_code >> 8) & 0xff;

		short_data[1 + cnt * 4 + 3] = short_code & 0xff;

		if (cnt < _GT9_UPLOAD_SHORT_TOTAL) {

			cnt++;

			short_data[0] = cnt;

		}

		return _GT_SHORT;

	}

	return _CHANNEL_PASS;
}

static int _calculate_short_resistance_with_gnd(u16 short_code, u8 pos)
{

	long r;

	u16 r_th;

	u8 m, totals;
	totals = upload_short_data[0];

	if (short_code == 0) {

		short_code = 1;

	}

	if ((short_code & (0x8000)) == 0)
		r = (52662850) * 10 / (short_code & (~0x8000)) - 40 * FLOAT_AMPLIFIER * 10;
	else

		r = 40 * 9 * 1024 * (sys_avdd - 9) * FLOAT_AMPLIFIER / (short_code & (~0x8000) * 7) - 40 * FLOAT_AMPLIFIER * 10;

	r *= 2;

	r = r / FLOAT_AMPLIFIER;

	if (r > 65535) {
		r = 65535;
	}
	short_code = (r >= 0 ? r : 0xffff);

	m = pos >> 1;

	if (pos < sys.max_driver_num * 2) {


		m = m | _CHANNEL_TX_FLAG;

		r_th = gt900_drv_gnd_resistor_threshold;

	}

	else {

		m -= sys.max_driver_num;

		r_th = gt900_sen_gnd_resistor_threshold;

	}

	if (short_code < r_th * 10) {

		upload_short_data[totals * 4 + 1] = m;

		upload_short_data[totals * 4 + 2] = sys.max_driver_num + 1;
		upload_short_data[totals * 4 + 3] = (short_code >> 8) & 0xff;

		upload_short_data[totals * 4 + 4] = short_code & 0xff;


		{

			if (totals < _GT9_UPLOAD_SHORT_TOTAL) {

				upload_short_data[0] = totals + 1;

				return _GT_SHORT;

			}

		}

	}

	return _CHANNEL_PASS;
}

static int _calculate_short_resistance(strShortRecord r_data, unsigned short short_r_th, unsigned char flag)
{

	unsigned short j;

	unsigned short short_code, warn_threshold;

	long r;

	u16 totals = upload_short_data[0];

	int test_error_code = 0;
	if (totals >= _GT9_UPLOAD_SHORT_TOTAL)

		return _CHANNEL_PASS;


	j = r_data.position;

	if (j > (sys.max_driver_num + sys.max_sensor_num)) {
		upload_short_data[0] = _GT9_SHORT_TEST_ERR;
		return _CHANNEL_PASS;

	}

	if (self_data[j] == 0xffff) {

		return _CHANNEL_PASS;

	}

	if (self_data[j] == 0) {

		return _CHANNEL_PASS;

	}

	r = _cal_resistance(self_data[j], r_data.master, r_data.slave, r_data.short_code);

	r = r * 10 / FLOAT_AMPLIFIER;
	if (r > 65535) {
		r = 65535;
	}
	short_code = (r >= 0 ? r : 0);


	if (gt900_resistor_warn_threshold < short_r_th) {

		warn_threshold = short_r_th;

	}

	else {

		warn_threshold = gt900_resistor_warn_threshold;

	}

	GTP_ERROR("calc:short_code=%d,r_th=%d,warn=%d", short_code, short_r_th, warn_threshold);

	if (short_code < (short_r_th * 10)) {


		test_error_code |= _save_short_data(upload_short_data, r_data, short_code);

	}

	else if (short_code < (warn_threshold * 10)) {






		test_error_code |= _save_short_data(warn_short_data, r_data, short_code);

	}

	return test_error_code;
}

int short_test_analysis(unsigned char *result, int t_size)
{

	unsigned char *large_buf;

	unsigned char *tmp;

	unsigned char short_status[3];



	unsigned char short_flag, i, j;

	unsigned short short_code, result_addr;

	int offest, length, test_error_code;
	strShortRecord r_data;
	test_error_code = _CHANNEL_PASS;
	i2c_read_data(0x8801, &short_flag, 1);
	if ((short_flag & 0x0F) == 0) {

		GTP_ERROR("No short.");

		return _CHANNEL_PASS;

	}

	large_buf = (unsigned char *)malloc(sizeof(unsigned char) * 3 * 1024);

	if (large_buf == NULL) {

		return MEMORY_ERR;

	}

	memset(large_buf, 0, 3 * 1024);

	offest = 0;

	upload_short_data = (unsigned char *)(&large_buf[offest]);

	offest += _GT9_UPLOAD_SHORT_TOTAL * 4 + 1;

	warn_short_data = (unsigned char *)(&large_buf[offest]);

	offest += _GT9_UPLOAD_SHORT_TOTAL * 4 + 1;

	self_data = (unsigned short *)(&large_buf[offest]);

	offest += (sys.max_driver_num + sys.max_sensor_num) * sizeof(unsigned short);

	tmp = (unsigned char *)(&large_buf[offest]);

	if ((short_flag & 0x08) == 0x08) {

		length = (sys.max_driver_num + sys.max_sensor_num) * 2;

		i2c_read_data(0xA322, tmp, length);

		for (i = 0; i < length; i += 2) {

			short_code = (tmp[i] << 8) + tmp[i + 1];

			test_error_code |= _calculate_short_resistance_with_gnd(short_code, i);

		}

	}

	length = (sys.max_driver_num + sys.max_sensor_num) * 2;

	i2c_read_data(0xA2A0, tmp, length);

	DEBUG_ARRAY(tmp, length);

	for (i = 0; i < sys.max_driver_num + sys.max_sensor_num; i++) {

		self_data[i] = ((tmp[i * 2] << 8) + tmp[i * 2 + 1]) & 0x7fff;


	}

	i2c_read_data(0x8802, short_status, 3);

	GTP_ERROR("Tx&Tx:%d,Rx&Rx:%d,Tx&Rx:%d", short_status[0], short_status[1], short_status[2]);


	result_addr = 0x8800 + 0x60;

	for (i = 0; i < short_status[0]; i++) {

		length = sys.short_head + sys.max_driver_num * 2 + 2;

		i2c_read_data(result_addr, tmp, length);

		for (j = i + 1; j < sys.max_driver_num; j++) {

			short_code = (tmp[sys.short_head + j * 2] << 8) + tmp[sys.short_head + j * 2 + 1];

			if (short_code > gt900_short_threshold) {

				r_data.master = ((tmp[0] << 8) + tmp[1]) | _CHANNEL_TX_FLAG;

				r_data.position = r_data.master & 0x7f;

				r_data.slave = j | _CHANNEL_TX_FLAG;
				r_data.short_code = short_code;

				test_error_code |= _calculate_short_resistance(r_data, gt900_drv_drv_resistor_threshold, 0);

			}

		}

		result_addr += length;

	}


	result_addr = 0x9120;

	for (i = 0; i < short_status[1]; i++)
	{

		length = sys.short_head + sys.max_sensor_num * 2 + 2;

		i2c_read_data(result_addr, tmp, length);

		DEBUG_ARRAY(tmp, length);

		for (j = 0; j < sys.max_sensor_num; j++) {

			if (j == i || (j < i && (j & 0x01) == 0))

				continue;

			short_code = (tmp[sys.short_head + j * 2] << 8) + tmp[sys.short_head + j * 2 + 1];

			if (short_code > gt900_short_threshold) {

				r_data.master = (tmp[0] << 8) + tmp[1];

				r_data.position = (r_data.master & 0x7f) + sys.max_driver_num;

				r_data.slave = j;

				r_data.short_code = short_code;

				test_error_code |= _calculate_short_resistance(r_data, gt900_sen_sen_resistor_threshold, 0);

			}

		}

		result_addr += length;

	}


	result_addr = 0x99e0;

	for (i = 0; i < short_status[2]; i++)
	{

		length = sys.short_head + sys.max_sensor_num * 2 + 2;

		i2c_read_data(result_addr, tmp, length);

		for (j = 0; j < sys.max_sensor_num; j++) {

			short_code = (tmp[sys.short_head + j * 2] << 8) + tmp[sys.short_head + j * 2 + 1];

			if (short_code > gt900_short_threshold) {

				r_data.master = (tmp[0] << 8) + tmp[1];

				r_data.position = (r_data.master & 0x7f) + sys.max_driver_num;

				r_data.slave = j | _CHANNEL_TX_FLAG;

				r_data.short_code = short_code;

				test_error_code |= _calculate_short_resistance(r_data, gt900_drv_sen_resistor_threshold, 0);

			}

		}

		result_addr += length;

	}

	if ((result != NULL) && (upload_short_data[0] != _GT9_SHORT_TEST_ERR)) {

		if (upload_short_data[0] > t_size) {

			upload_short_data[0] = t_size;

		}

		memcpy(result, upload_short_data, upload_short_data[0] * 4 + 1);

		if (warn_short_data[0] != 0) {

			if (warn_short_data[0] + upload_short_data[0] > t_size) {

				warn_short_data[0] = t_size - upload_short_data[0];

			}

			memcpy(&result[upload_short_data[0] * 4 + 1], &warn_short_data[1], warn_short_data[0] * 4);

			result[0] += warn_short_data[0];

		}

	}

	free(large_buf);

	return test_error_code;
}

int short_test_end(void)
{
	unsigned char data[10];
	memset(data, 0x00, sizeof(data));
	return i2c_write_data(0x8000, data, 10);
}

s32 load_code_and_check(unsigned char *codes, int size)
{
	u8 i, count, packages;

	u8 *ram;

	u16 start_addr, tmp_addr;

	s32 len, code_len, ret = -1;
	ram = (u8 *) malloc(PACKAGE_SIZE);
	if (ram == NULL) {
		return MEMORY_ERR;
	}

	start_addr = 0xC000;

	len = PACKAGE_SIZE;

	tmp_addr = start_addr;

	count = 0;

	code_len = size;
	packages = code_len / PACKAGE_SIZE + 1;

	for (i = 0; i < packages; i++) {

		if (len > code_len) {

			len = code_len;

		}

		i2c_write_data(tmp_addr, (u8 *) & codes[tmp_addr - start_addr], len);

		i2c_read_data(tmp_addr, ram, len);

		ret = memcmp(&codes[tmp_addr - start_addr], ram, len);

		if (ret) {

			if (count++ > 5) {

				WARNING("equal error.\n");

				break;

			}

			continue;

		}

		tmp_addr += len;

		code_len -= len;

		if (code_len <= 0) {

			break;

		}

	}

	free(ram);

	if (count < 5) {

		GTP_ERROR("Burn DSP code successfully!\n");

		return 1;

	}

	return -EPERM;

}

int load_dsp_code_check(void)
{
	return load_code_and_check((u8 *) dsp_short_9p, sizeof(dsp_short_9p));
}

/*********************file operate function***********************************/
int auto_find_ini(char *findDir, char *format, char *inipath)
{

	 struct file *pfile = NULL;
	char tmp[128];
	memset(tmp, 0, sizeof(tmp));

	if (findDir == NULL){
		WARNING("dir is null.");
		return PARAMETERS_ILLEGL;
	}

	sprintf((char *)tmp, "/system/etc/test_sensor_%d.ini", gt1x_version.sensor_id);

    pfile = filp_open(tmp, O_RDONLY, 0);
#if 0
    if (IS_ERR(pfile))
    {
        FTS_TEST_ERROR("error occured while opening file ");
        return -EIO;
    }
#endif


	/*if(null==tmp){
		WARNING("Not found %s.ini in %s",inipath);
		return -3;
		}*/


	memcpy(inipath, tmp, sizeof(tmp));
	return 1;
}



unsigned int my_getline(char *buf, int size, FILE *fp)
{

	size_t bytes;
	unsigned int i = 0;
	GTP_ERROR("[%s] enter.\n",__func__);
	if (fp == NULL){
		return -EPERM;
	}
	if (fp->f_op == NULL){
		return -EPERM;
	}
	while (i < size) {

		bytes = vfs_read(fp, &buf[i], 1, &fp->f_pos);
		if (bytes < 0) {
			GTP_ERROR("getline read file failed\n.");
			return INI_FILE_READ_ERR;

		}

		if (buf[i] == ' ') {

			continue;

		}

		if (buf[i] == '\r' || buf[i] == '\n' || buf[i] == '\x0d' || buf[i] == '\x0a') {
			break;

		}
		i+= bytes;

	}

	if (i >= size) {

		GTP_ERROR("getline over length.");
		return INI_FILE_READ_ERR;

	}

	GTP_ERROR("[%s]exit.buf :%s\n",__func__,buf);
	return i;
}

int getrid_space(char *data, int len)
{

	unsigned char *buf = NULL;

	int i;

	unsigned int count = 0;
	buf = (unsigned char *)malloc(len + 5);

	if (buf == NULL) {

		return MEMORY_ERR;

	}

	for (i = 0; i < len; i++) {


		{


		}

		if (data[i] == ' ' || data[i] == '\r' || data[i] == '\n') {

			continue;

		}

		buf[count++] = data[i];

	}

	buf[count++] = '\0';

	memcpy(data, buf, count);

	free(buf);

	return count;
}

unsigned char atohex(char *buf)
{

	unsigned char value = 0;

	unsigned char byte1, byte2;
	if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {

		byte1 = buf[2];

		byte2 = buf[3];

	}

	else {

		byte1 = buf[0];

		byte2 = buf[1];

	}

	if (HEX(byte1) && HEX(byte2)) {

		if (byte1 >= '0' && byte1 <= '9') {

			value = (byte1 - '0') << 4;

		}

		else if (byte1 >= 'A' && byte1 <= 'F') {

			value = (byte1 - 'A' + 10) << 4;

		}

		else if (byte1 >= 'a' && byte1 <= 'f') {

			value = (byte1 - 'a' + 10) << 4;

		}

		if (byte2 >= '0' && byte2 <= '9') {

			value |= (byte2 - '0');

		}

		else if (byte2 >= 'A' && byte2 <= 'F') {

			value |= (byte2 - 'A' + 10);

		}

		else if (byte2 >= 'a' && byte2 <= 'f') {

			value |= (byte2 - 'a' + 10);

		}

		return value;

	}

	else {

		return 0;

	}
}

int ini_read(char *file, const char *key, char *value)
{

	FILE *fp = NULL;

	char *buf;

	char *tmp = NULL;
	int ret = -1;
	struct inode *inode = NULL;
	off_t fsize = 0;

	GTP_ERROR("[%s] enter.\n",__func__);
	if (file == NULL) {

		return PARAMETERS_ILLEGL;

	}

	fp = fopen((const char *)file, "r");

	if (fp == NULL) {

		GTP_ERROR("open %s fail!", file);

		return INI_FILE_OPEN_ERR;

	}
	inode = fp->f_inode;
	fsize = inode->i_size;

	buf = (char *)malloc(fsize);

	if (buf == NULL) {

		fclose(fp);

		return MEMORY_ERR;

	}

	memset(buf, 0, fsize);
	while (1) {
		ret = my_getline(buf, fsize, fp);

		if (ret < 0) {

			fclose(fp);

			free(buf);

			return INI_FILE_READ_ERR;

		}

		buf[ret] = '\0';

		getrid_space(buf, ret);

		strtok((char *)buf, "=");
		GTP_ERROR("buf=%s",buf);
		GTP_ERROR("key=%s",key);
		if (0 == strcmp((const char *)buf, (const char *)key)) {
			tmp = (char *)strtok((char *)NULL, "=");

			if (tmp == NULL) {

				break;

			}

			memcpy((void *)value, (const void *)tmp, (size_t) strlen((const char *)tmp));

			value[(size_t) strlen((const char *)tmp)] = '\0';

			fclose(fp);

			free(buf);

			return 0;

		}

	}

	fclose(fp);

	free(buf);
	GTP_ERROR("[%s] exit.\n",__func__);

	return INI_FILE_ILLEGAL;
}

int ini_read_int(char *file, const char *key)
{

	int value = 0;

	int ret = 0;

	char *tmp = NULL;
	tmp = (char *)malloc(32);

	if (tmp == NULL) {

		return MEMORY_ERR;

	}

	memset(tmp, 0, 32);

	ret = ini_read(file, key, tmp);

	if (ret) {

		free(tmp);

		return ret;

	}

	value = atoi((const char *)tmp);

	free(tmp);

	return value;
}

long ini_read_float(char *file, const char *key)
{

	long value = 0;

	int ret = 0;

	char *tmp = NULL;
	tmp = (char *)malloc(32);

	if (tmp == NULL) {

		return MEMORY_ERR;

	}

	memset(tmp, 0, 32);

	ret = ini_read(file, key, tmp);

	if (ret) {

		free(tmp);

		return ret;

	}

	GTP_ERROR("ini read float ascii:%s", tmp);

	value = atof((const char *)tmp);

	GTP_ERROR("ini read float value:%ld", value);

	free(tmp);

	return value;
}
#if 0
int ini_read_text(char *file, const char *key, unsigned char *text)
{

	int ret = -1;

	int i;

	char *tmp;
	GTP_ERROR("[%s] enter.\n",__func__);
	tmp = (char *)malloc(2 * (sys.max_sensor_num * sys.max_driver_num / 8) * 5 + 30);

	if (tmp == NULL) {

		return MEMORY_ERR;

	}

	ret = ini_read(file, key, tmp);

	if (ret < 0) {

		free(tmp);

		return ret;

	}
	GTP_ERROR("length:%s.\n",tmp);
	for (i = 0; i < (strlen((const char *)tmp) + 1) / 5; i++) {

		text[i] = (unsigned char)atohex((char *)&tmp[i * 5]);


	}
	GTP_ERROR("RAW KEY value:%s\n", tmp);


	free(tmp);

	return i;
}
#endif
int ini_read_cfg(char *file, const char *key, unsigned char *text)
{

	int ret = -1;

	int i;

	char *tmp;
	GTP_ERROR("[%s] enter.\n",__func__);
	tmp = (char *)malloc(sys.config_length * 5 + 30);

	if (tmp == NULL) {

		return MEMORY_ERR;

	}

	ret = ini_read(file, key, tmp);

	if (ret < 0) {

		free(tmp);
		GTP_ERROR("ini_read return err:%d\n",ret);
		return ret;

	}
	GTP_ERROR("length:%s.\n",tmp);
	for (i = 0; i < (strlen((const char *)tmp) + 1) / 5; i++) {

		text[i] = (unsigned char)atohex((char *)&tmp[i * 5]);

		GTP_ERROR("text[%d]:0x%x\n", i,text[i]);
	}



	free(tmp);

	return i;
}
int ini_read_hex(char *file, const char *key)
{

	int value = 0;

	int ret = 0;

	int i = 0;

	int len;

	char *tmp = NULL;
	tmp = (char *)malloc(32);

	if (tmp == NULL) {

		return MEMORY_ERR;

	}

	memset(tmp, 0, 32);


	ret = ini_read(file, key, tmp);

	if (ret) {

		free(tmp);

		return ret;

	}

	len = strlen((const char *)tmp);


	for (i = 1; i < (len / 2); i++) {

		value |= atohex(&tmp[2 * i]) << (8 * ((len / 2) - i - 1));

	}

	if ((unsigned int)len & 0x01) {

		char buf[4] = { '0', 'x', '0' };

		buf[3] = tmp[len - 1];

		value = (value << 4) | atohex(buf);

	}

	free(tmp);

	return value;
}

int init_chip_type(void)
{
	u8 *largebuf;
	int ret = -1;
	GTP_ERROR("[%s] enter\n",__func__);
	largebuf = (unsigned char *)malloc(2 * 1024);
	if (largebuf == NULL) {
		return MEMORY_ERR;
	}



	sys.reg_rawdata_base = 0xB5F8;
	sys.key_offest = 83;

	sys.config_length = 367;
	sys.max_sensor_num = 32;
	sys.max_driver_num = 32;
	sys.short_head = 4;

	file_pos = 0;
	if (!strncmp((const char *)gt1x_version.product_id, "1143", 4)) {
		sys.chip_type = _GT1143;
	} else if (!strncmp((const char *)gt1x_version.product_id, "1151", 4)) {
		sys.chip_type = _GT1151;
	} else if  (!strncmp((const char *)gt1x_version.product_id, "1152", 4)) {
		sys.chip_type = _GT1152;
		sys.chip_name = "GT1152";
	} else {
		sys.chip_type = _GT9P;
	}
	ret = read_config(largebuf, sys.config_length);
	if (ret < 0){
		GTP_ERROR("i2c communication error. ret =%d\n", ret);
		free(largebuf);
		return ret;
	}
	parse_config(largebuf);


	free(largebuf);

	GTP_ERROR("sen*drv:%d*%d", sys.sensor_num, sys.driver_num);
	return 1;
}

/*****************************************************************************/
/*
static int gtp_sysfs_openshort_show(struct seq_file *file, void* data)
{
	unsigned char *result = NULL;
	char *ito_buf = NULL;
	ssize_t i, len = 0;
	s32 ito_result = 0;
	int r;

	ito_buf = (unsigned char *)malloc(20);
	if(ito_buf == NULL){
		GTP_ERROR("ito_buf malloc fail!!!");
		return -1;
	}

	r = init_chip_type();
	if(r < 0){
		ito_result |=_I2C_ERR;
	}else{
		result = (unsigned char *)malloc(600 + sys.sensor_num * sys.driver_num * 2);
		if(result == NULL){
		GTP_ERROR("result malloc fail!!!");
		free(ito_buf);
		return -1;
		}

		ito_result = open_short_test(result);
		GTP_ERROR("ito_result = %x",ito_result);
	}

	if (ito_result == _TEST_PASS) {
		len += sprintf((char *)&ito_buf[len], "0P-1P-2P-3P-4P\r\n\n\n");
	}else {
		//len += sprintf((char *)&buff[len], "NG\r\n");
		GTP_ERROR("test error code 0x%x\n", ito_result);
		if(ito_result & _I2C_ERR){
			len +=sprintf((char*)&ito_buf[len],"0F");
		}else if(!(ito_result & _I2C_ERR)){
			len +=sprintf((char*)&ito_buf[len],"0P");
		}

		if (ito_result & _BEYOND_MAX_LIMIT) {
			len += sprintf((char *)&ito_buf[len], "-1F");

		}else if (!(ito_result & _BEYOND_MAX_LIMIT)) {
			len += sprintf((char *)&ito_buf[len], "-1P");

		}

		if (ito_result & _BEYOND_MIN_LIMIT) {
			len += sprintf((char *)&ito_buf[len], "-2F");
		}else if (!(ito_result & _BEYOND_MIN_LIMIT)) {
			len += sprintf((char *)&ito_buf[len], "-2P");
		}

		if (ito_result & _BEYOND_ACCORD_LIMIT) {
			len += sprintf((char *)&ito_buf[len], "-3F");
		}else if (!(ito_result & _BEYOND_ACCORD_LIMIT)) {
			len += sprintf((char *)&ito_buf[len], "-3P");
		}

		if (ito_result & _GT_SHORT) {
			len += sprintf((char *)&ito_buf[len], "-4F\t\n\n");
			//len += sprintf((char *)&buff[len], "short circuit\r\n");
			for (i = 0; i < result[0]; i++) {
				if (result[1 + i * 4] & 0x80) {
					len += sprintf((char *)&ito_buf[len], "drv%d & ", result[1 + i * 4] & 0x7F);
				}else if (result[1 + i * 4] == (sys.max_driver_num + 1)) {
					len += sprintf((char *)&ito_buf[len], "GND\\VDD & ");
				}else {
					len += sprintf((char *)&ito_buf[len], "sen%d & ", result[1 + i * 4] & 0x7F);
				}

				if (result[2 + i * 4] & 0x80) {
					len += sprintf((char *)&ito_buf[len], "drv%d", result[2 + i * 4] & 0x7F);
				}else if (result[2 + i * 4] == (sys.max_driver_num + 1)) {
					len += sprintf((char *)&ito_buf[len], "GND\\VDD");
				}else {
					len += sprintf((char *)&ito_buf[len], "sen%d", result[2 + i * 4] & 0x7F);
				}

				len += sprintf((char *)&ito_buf[len], "(%dk)\r\n", ((result[3 + i * 4] << 8) + result[4 + i * 4]) / 10);
			}
		}else if(!(ito_result & _GT_SHORT)){
			len += sprintf((char *)&ito_buf[len], "-4P\t\n\n");
		}
	}

	GTP_ERROR("This is gtp_sysfs_openshort_show result len = %ld\n\n",len);
	seq_printf(file,"%s",ito_buf);
	free(ito_buf);
	free(result);
	return 0;
}*/

static int gtp_sysfs_openshort_show(struct seq_file *file, void *data)
{
	if (NULL==self_test_state || 0==strcmp(self_test_state,"0")){
		seq_printf(file,"%s\n","0");
		goto EXIT;
	}

	strcmp(self_test_state,"1")? seq_printf(file,"%s\n","2"):seq_printf(file,"%s\n","1");

EXIT:
	self_test_state = "0";
	return 0;
}


static ssize_t gtp_sysfs_openshort_store(struct file *file, const char __user *buf,
                       size_t count, loff_t *pos)
{
	unsigned char *result = NULL;
	char *ito_buf = NULL;
	ssize_t ret = 0;
	s32 ito_result = 0;

	ito_buf = (unsigned char *)malloc(count);
	if (ito_buf == NULL){
		GTP_ERROR("ito_buf malloc fail!!!");
		ret = -EFAULT;
        goto fail;
	}

    if (copy_from_user(ito_buf, buf, count))
    {
        GTP_ERROR("copy from user error!!");
		ret = -EFAULT;
        goto fail;
    }

	if ( !(strstr(ito_buf,"i2c") || strstr(ito_buf,"open") || strstr(ito_buf,"short")))
	{
		self_test_state = "0";
		ret = -EFAULT;
        goto fail;
	}

	ret = init_chip_type();
	if (ret < 0){
		ito_result |=_I2C_ERR;
	}else{
		result = (unsigned char *)malloc(600 + sys.sensor_num * sys.driver_num * 2);
		if (result == NULL){
		GTP_ERROR("result malloc fail!!!");
		free(ito_buf);
		ret = -EFAULT;
        goto error;
		}

		ito_result = open_short_test(result);
		GTP_ERROR("ito_result = %x",ito_result);
	}

	if (ito_result == _CHANNEL_PASS) {
		self_test_state = "2";
	}else {
		self_test_state = "1";
	}
	ret = count;

error:
    free(result);
fail:
    free(ito_buf);
	return ret;
}

static int gtp_sysfs_openshort_open (struct inode *inode, struct file *file)
{
    return single_open(file, gtp_sysfs_openshort_show, NULL);
}

static const struct file_operations ito_test_ops = {
	.owner = THIS_MODULE,
	.open = gtp_sysfs_openshort_open,
	.read = seq_read,
	.write = gtp_sysfs_openshort_store,
};
/*******************************************************
Description:
    Goodix debug sysfs init function.
Parameter:
    none.

return:
    Executive outcomes. 0---succeed.
*******************************************************/
s32 gtp_test_sysfs_init(void)
{
	GTP_INFO("%s:ENTER FUNC ---- %d\n",__func__,__LINE__);

	gt1x_ito_test_proc = proc_create(GT1X_ITO_TEST, 0777,
					      NULL, &ito_test_ops);

	if (!gt1x_ito_test_proc){
		GTP_INFO("create proc entry %s failed",GT1X_ITO_TEST);
		return -EPERM;
	}
	else{
		GTP_INFO("create proc entry %s success",GT1X_ITO_TEST);
		return 0;
	}
}
