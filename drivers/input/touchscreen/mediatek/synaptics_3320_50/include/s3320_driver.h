/*
 * Copyright (C) 2013 LG Electironics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "tpd.h"
#include <include/ext_wd_drv.h>
#include <mach/gpio_const.h>
/****************************************************************************
* Debugging Macros
****************************************************************************/
#define TPD_TAG                  "[S3320] "
#define TPD_FUN(f)               pr_err("[S3320] %s\n", __func__)
#define TPD_ERR(fmt, args...)    pr_err("[S3320] %s %d : "fmt, __func__, __LINE__, ##args)
#define TPD_LOG(fmt, args...)    pr_err("[S3320] %s %d : "fmt, __func__, __LINE__, ##args)

#define _ERROR(e)      ((0x01 << e) | (0x01 << (sizeof(s32) * 8 - 1)))
#define ERROR          _ERROR(1)	/*for common use */
/*system relevant*/
#define ERROR_IIC      _ERROR(2)	/*IIC communication error. */
#define ERROR_MEM      _ERROR(3)	/*memory error. */

/*system irrelevant*/
#define ERROR_HN_VER   _ERROR(10)	/*HotKnot version error. */
#define ERROR_CHECK    _ERROR(11)	/*Compare src and dst error. */
#define ERROR_RETRY    _ERROR(12)	/*Too many retries. */
#define ERROR_PATH     _ERROR(13)	/*Mount path error */
#define ERROR_FW       _ERROR(14)
#define ERROR_FILE     _ERROR(15)
#define ERROR_VALUE    _ERROR(16)	/*Illegal value of variables */

struct synaptics_ts_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query_6;
			struct {
				unsigned char ctrl_00_is_present:1;
				unsigned char ctrl_01_is_present:1;
				unsigned char ctrl_02_is_present:1;
				unsigned char ctrl_03_is_present:1;
				unsigned char ctrl_04_is_present:1;
				unsigned char ctrl_05_is_present:1;
				unsigned char ctrl_06_is_present:1;
				unsigned char ctrl_07_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl_08_is_present:1;
				unsigned char ctrl_09_is_present:1;
				unsigned char ctrl_10_is_present:1;
				unsigned char ctrl_11_is_present:1;
				unsigned char ctrl_12_is_present:1;
				unsigned char ctrl_13_is_present:1;
				unsigned char ctrl_14_is_present:1;
				unsigned char ctrl_15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl_16_is_present:1;
				unsigned char ctrl_17_is_present:1;
				unsigned char ctrl_18_is_present:1;
				unsigned char ctrl_19_is_present:1;
				unsigned char ctrl_20_is_present:1;
				unsigned char ctrl_21_is_present:1;
				unsigned char ctrl_22_is_present:1;
				unsigned char ctrl_23_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl_24_is_present:1;
				unsigned char ctrl_25_is_present:1;
				unsigned char ctrl_26_is_present:1;
				unsigned char ctrl_27_is_present:1;
				unsigned char ctrl_28_is_present:1;
				unsigned char ctrl_29_is_present:1;
				unsigned char ctrl_30_is_present:1;
				unsigned char ctrl_31_is_present:1;
			} __packed;
		};
		unsigned char data[5];
	};
};
struct synaptics_ts_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query_9;
			struct {
				unsigned char data_00_is_present:1;
				unsigned char data_01_is_present:1;
				unsigned char data_02_is_present:1;
				unsigned char data_03_is_present:1;
				unsigned char data_04_is_present:1;
				unsigned char data_05_is_present:1;
				unsigned char data_06_is_present:1;
				unsigned char data_07_is_present:1;
			} __packed;
			struct {
				unsigned char data_08_is_present:1;
				unsigned char data_09_is_present:1;
				unsigned char data_10_is_present:1;
				unsigned char data_11_is_present:1;
				unsigned char data_12_is_present:1;
				unsigned char data_13_is_present:1;
				unsigned char data_14_is_present:1;
				unsigned char data_15_is_present:1;
			} __packed;
		};
		unsigned char data[3];
	};
};

/****************************************************************************
* Extern Function Prototypes
****************************************************************************/
#ifdef CONFIG_MTK_LEGACY
#else
#define SYNAP_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define SYNAP_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)

#define SYNAP_RST_PORT    0
#define SYNAP_INT_PORT    1

#define GPIO_CTP_EINT_PIN       (GPIO85|0x80000000)
#define GPIO_CTP_RST_PIN        (GPIO68|0x80000000)

#define P_GPIO_CTP_EINT_PIN       85
#define P_GPIO_CTP_RST_PIN        68
#endif				/* CONFIG_MTK_LEGACY */

extern int FirmwareUpgrade(struct i2c_client *client, const char *fw_path, unsigned long fw_size,
			   unsigned char *fw_start);
extern unsigned int DISP_GetScreenHeight(void);
extern unsigned int DISP_GetScreenWidth(void);
extern int mtk_wdt_enable(enum wk_wdt_en en);
/* extern g_qem_check; */
extern void arch_reset(char mode, const char *cmd);

extern struct tpd_device *tpd;
extern struct i2c_client *ds4_i2c_client;
extern int f54_window_crack;
extern int f54_window_crack_check_mode;

/* s3320_fw_update.c */
extern int CompleteReflash(struct i2c_client *client);
/* int ConfigBlockReflash(struct i2c_client *client); */
/* int CompleteReflash_Lockdown(struct i2c_client *client); */
extern void SynaInitialize(struct i2c_client *client);
/* void SynaReadConfigInfo(struct i2c_client *client); */
extern void SynaReadFirmwareInfo(struct i2c_client *client);
extern void SynaEnableFlashing(struct i2c_client *client);
extern void SynaProgramFirmware(struct i2c_client *client);
/* int SynaProgramConfiguration(struct i2c_client *client); */
extern void SynaFinalizeReflash(struct i2c_client *client);
/* int SynaWaitForATTN(int time); */
extern unsigned int SynaWaitForATTN(int timeout, struct i2c_client *client);
extern void eraseAllBlock(struct i2c_client *client);
extern void EraseConfigBlock(struct i2c_client *client);
extern bool CheckTouchControllerType(struct i2c_client *client);

extern int synaptics_ts_read(struct i2c_client *client, u8 reg, int num, u8 *buf);
extern int synaptics_ts_write(struct i2c_client *client, u8 reg, u8 *buf, int len);
extern int synaptics_ts_read_f54(struct i2c_client *client, u8 reg, int num, u8 *buf);
