/*   Inc. (C) 2011. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ( SOFTWARE")
 * RECEIVED FROM  AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY.  EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES  PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE  SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN  SOFTWARE.  SHALL ALSO NOT BE RESPONSIBLE FOR ANY
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND 'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE  SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT 'S OPTION, TO REVISE OR REPLACE THE  SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 *  FOR SUCH  SOFTWARE AT ISSUE.
 *
 */

/*******************************************************************************
* Dependency
*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/hardware_info.h>

#include <linux/mm.h>
#include <linux/genhd.h>

#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/err.h>

#include <soc/qcom/smem.h>

#define HARDWARE_INFO_VERSION   "SDM660"
#define HARDWARE_INFO_WCN       "WCN3640B"

#define QCOM_SENSOR_INFO_NEED_SET

/******************************************************************************
 * EMMC Configuration
*******************************************************************************/
#define EMMC_VENDOR_CMP_SIZE  2
#define DDR_TYPE "LPDDR4X"
static EMMC_VENDOR_TABLE vendor_table[] =
{
	{ .id = "11", .name = "Toshiba", },
	{ .id = "13", .name = "Micron", },
	{ .id = "15", .name = "Samsung", },
	{ .id = "45", .name = "SanDisk", },
	{ .id = "90", .name = "Hynix", },
	{ .id = "00", .name = "NULL", },

};

/******************************************************************************
 * HW_ID Configuration
*******************************************************************************/
static BOARD_STAGE_TABLE pcba_stage_table[] =
{
	{ .stage_value = 0, .pcba_stage_name = "P0", },
	{ .stage_value = 1, .pcba_stage_name = "P1", },
	{ .stage_value = 2, .pcba_stage_name = "P2", },
	{ .stage_value = 3, .pcba_stage_name = "P2.1", },
	{ .stage_value = 4, .pcba_stage_name = "MP", },
	{ .stage_value = -1, .pcba_stage_name = "NULL", },

};

static unsigned int adc_val_tolerance = 50;
static BOARD_TYPE_TABLE pcba_type_table[] =
{
	{ .adc_value = 300,  .pcba_type_name = "D9_OLD_LTE", },
	{ .adc_value = 900,  .pcba_type_name = "D9_OLD_WIFI", },
	{ .adc_value = 0,    .pcba_type_name = "D9_LTE", },
	{ .adc_value = 1800, .pcba_type_name = "D9_WIFI", },
	{ .adc_value = 1000,  .pcba_type_name = "D9P_LTE", },
	{ .adc_value = 1500, .pcba_type_name = "D9P_WIFI", },
	{ .adc_value = -300, .pcba_type_name = "NULL", },

};

static SMEM_BOARD_INFO_DATA *board_info_data;

/******************************************************************************
 * Hardware Info Driver
*************************`*****************************************************/
struct global_otp_struct hw_info_main_otp;
struct global_otp_struct hw_info_sub_otp;
static HARDWARE_INFO hwinfo_data;
void get_hardware_info_data(enum hardware_id id, const void *data)
{
	if (NULL == data) {
		printk("%s the data of hwid %d is NULL\n", __func__, id);
	} else {
		switch (id) {
		case HWID_LCM:
			hwinfo_data.lcm = data;
			break;
		case HWID_CTP_DRIVER:
			hwinfo_data.ctp_driver = data;
			break;
		case HWID_CTP_MODULE:
			hwinfo_data.ctp_module = data;
			break;
		case HWID_CTP_FW_VER:
			strcpy(hwinfo_data.ctp_fw_version,data);
			break;
		case HWID_CTP_COLOR_INFO:
			hwinfo_data.ctp_color_info = data;
			break;
		case HWID_CTP_LOCKDOWN_INFO:
			hwinfo_data.ctp_lockdown_info = data;
			break;
		case HWID_CTP_FW_INFO:
			hwinfo_data.ctp_fw_info = data;
			break;
		case HWID_MAIN_CAM:
			hwinfo_data.main_camera = data;
			break;
		case HWID_SUB_CAM:
			hwinfo_data.sub_camera = data;
			break;
		case HWID_FLASH:
			hwinfo_data.flash = data;
			break;
		case HWID_ALSPS:
			hwinfo_data.alsps = data;
			break;
		case HWID_GSENSOR:
			hwinfo_data.gsensor = data;
			break;
		case HWID_GYROSCOPE:
			hwinfo_data.gyroscope = data;
			break;
		case HWID_MSENSOR:
			hwinfo_data.msensor = data;
			break;
		case HWID_SAR_SENSOR_1:
			hwinfo_data.sar_sensor_1 = data;
			break;
		case HWID_SAR_SENSOR_2:
			hwinfo_data.sar_sensor_2 = data;
			break;
		case HWID_BATERY_ID:
			hwinfo_data.bat_id = data;
			break;
		case HWID_NFC:
			hwinfo_data.nfc = data;
			break;
		case HWID_FINGERPRINT:
			hwinfo_data.fingerprint = data;
			break;
		default:
			printk("%s Invalid HWID\n", __func__);
			break;
		}
	}
}

static ssize_t show_lcm(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (NULL != hwinfo_data.lcm) {
		return sprintf(buf, "lcd name :%s\n", hwinfo_data.lcm);
	} else {
	   return sprintf(buf, "lcd name :Not Found\n");
	}
}

static ssize_t show_ctp(struct device *dev, struct device_attribute *attr, char *buf)
{
	if ((NULL != hwinfo_data.ctp_driver) || (NULL != hwinfo_data.ctp_module) || (NULL != hwinfo_data.ctp_fw_version)) {
		return sprintf(buf, "ctp name :%s\n", hwinfo_data.ctp_driver);
	} else {
		return sprintf(buf, "ctp name :Not Found\n");
	}
}

static ssize_t show_fingerprint(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (NULL != hwinfo_data.fingerprint) {
		return sprintf(buf, "fingerprint name :%s\n", hwinfo_data.fingerprint);
	} else {
		return sprintf(buf, "fingerprint name :Not Found\n");
	}
}

static ssize_t show_fw_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (NULL != hwinfo_data.ctp_fw_info) {
		return sprintf(buf, "%s\n", hwinfo_data.ctp_fw_info);
	} else {
		return sprintf(buf, "Invalid\n");
	}
}

static ssize_t show_color_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (NULL != hwinfo_data.ctp_color_info) {
		return sprintf(buf, "%s\n", hwinfo_data.ctp_color_info);
	} else {
		return sprintf(buf, "Invalid\n");
	}
}

static ssize_t show_lockdown_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (NULL != hwinfo_data.ctp_lockdown_info) {
		return sprintf(buf, "%s\n", hwinfo_data.ctp_lockdown_info);
	} else {
		return sprintf(buf, "Invalid\n");
	}
}


static ssize_t show_main_camera(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (NULL != hwinfo_data.main_camera) {
		return sprintf(buf , "main camera :%s\n", hwinfo_data.main_camera);
	} else {
		return sprintf(buf , "main camera :Not Found\n");
	}
}

static ssize_t show_sub_camera(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (NULL != hwinfo_data.sub_camera) {
		return sprintf(buf , "sub camera :%s\n", hwinfo_data.sub_camera);
	} else {
		return sprintf(buf , "sub camera :Not Found\n");
	}
}

static ssize_t show_main_otp(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (hw_info_main_otp.otp_valid) {
		return sprintf(buf, "main otp :Vendor 0x%0x, ModuleCode 0x%x, ModuleVer 0x%x, SW_ver 0x%x, Date %d-%d-%d, VcmVendor 0x%0x, VcmModule 0x%x \n",
				            hw_info_main_otp.vendor_id, hw_info_main_otp.module_code, hw_info_main_otp.module_ver, hw_info_main_otp.sw_ver, hw_info_main_otp.year,
				            hw_info_main_otp.month, hw_info_main_otp.day, hw_info_main_otp.vcm_vendorid, hw_info_main_otp.vcm_moduleid);
	} else {
		return sprintf(buf, "main otp :No Valid OTP\n");
	}
}

static ssize_t show_sub_otp(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (hw_info_sub_otp.otp_valid) {
		return sprintf(buf, "sub otp :Vendor 0x%0x, ModuleCode 0x%x, ModuleVer 0x%x, SW_ver 0x%x, Date %d-%d-%d, VcmVendor 0x%0x, VcmModule 0x%0x \n",
				            hw_info_sub_otp.vendor_id, hw_info_sub_otp.module_code, hw_info_sub_otp.module_ver, hw_info_sub_otp.sw_ver, hw_info_sub_otp.year,
				            hw_info_sub_otp.month, hw_info_sub_otp.day, hw_info_sub_otp.vcm_vendorid, hw_info_sub_otp.vcm_moduleid);
	} else {
		return sprintf(buf, "sub otp :No Valid OTP\n");
	}
}

static unsigned int get_emmc_size(void)
{
	unsigned int emmc_size = 0;
	struct file *pfile = NULL;
	mm_segment_t old_fs;
	loff_t pos;
	ssize_t ret = 0;

	unsigned long long Size_buf = 0;
	char buf_size[qcom_emmc_len];
	memset(buf_size, 0, sizeof(buf_size));

	pfile = filp_open(qcom_emmc, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		printk("HWINFO: open emmc size file failed!\n");
		goto ERR_0;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	ret = vfs_read(pfile, buf_size, qcom_emmc_len, &pos);
	if (ret <= 0) {
		printk("HWINFO: read emmc size file failed!\n");
		goto ERR_1;
	}

	Size_buf = simple_strtoull(buf_size, NULL, 0);
	Size_buf >>= 1;
	emmc_size = (((unsigned int)Size_buf) / 1024) / 1024;

	if (emmc_size > 64) {
		emmc_size = 128;
	} else if (emmc_size > 32) {
		emmc_size = 64;
	} else if (emmc_size > 16) {
		emmc_size = 32;
	} else if (emmc_size > 8) {
		emmc_size = 16;
	} else if (emmc_size > 6) {
		emmc_size = 8;
	} else if (emmc_size > 4) {
		emmc_size = 6;
	} else if (emmc_size > 3) {
		emmc_size = 4;
	} else {
		emmc_size = 0;
	}

ERR_1:

	filp_close(pfile, NULL);

	set_fs(old_fs);

	return emmc_size;

ERR_0:
	return emmc_size;
}

#define K(x) ((x) << (PAGE_SHIFT - 10))
static unsigned int get_ram_size(void)
{
	unsigned int ram_size, temp_size;
	struct sysinfo info;

	si_meminfo(&info);

	temp_size = K(info.totalram) / 1024;
	if (temp_size > 4096) {
		ram_size = 6;
	} else if (temp_size > 3072) {
		ram_size = 4;
	} else if (temp_size > 2048) {
		ram_size = 3;
	} else if (temp_size > 1024) {
		ram_size = 2;
	} else if (temp_size > 512) {
		ram_size = 1;
	} else {
		ram_size = 0;
	}

	return ram_size;
}

static ssize_t show_emmc_size(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%dGB\n", get_emmc_size());
}

static ssize_t show_ram_size(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%dGB\n", get_ram_size());
}

static ssize_t show_flash(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i = 0;
	unsigned int raw_cid[4] = {0};
	char emmc_cid[32] = {0};

	if (NULL == hwinfo_data.flash) {
		return sprintf(buf, "flash name :Can Not Detect EMMC\n");
	}

	memcpy(raw_cid, hwinfo_data.flash, sizeof(raw_cid));
	sprintf(emmc_cid, "%08x%08x%08x", raw_cid[0], raw_cid[1], raw_cid[2]);

	for (i = 0; i < ARRAY_SIZE(vendor_table); i++) {
		if (memcmp(emmc_cid, vendor_table[i].id, EMMC_VENDOR_CMP_SIZE) == 0) {
			return sprintf(buf, "flash name :%s %dGB+%dGB %s\n", vendor_table[i].name, get_ram_size(), get_emmc_size(), DDR_TYPE);
		}
	}

	return sprintf(buf, "flash name :Not Found\n");
}

static ssize_t show_wifi(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "wifi name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_bt(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "bt name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_gps(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "gps name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_fm(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "fm name :%s\n", HARDWARE_INFO_WCN);
}

static ssize_t show_alsps(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	if (NULL != hwinfo_data.alsps) {
		return sprintf(buf, "alsps name :%s\n", hwinfo_data.alsps);
	} else {
		return sprintf(buf, "alsps name :Not Found\n");
	}
#else
	return sprintf(buf, "alsps name :Not Support ALSPS\n");
#endif
}
static ssize_t store_alsps(struct device *dev, struct device_attribute *attr,\
		const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	int ret;
	static char name[100] = "Not found";
	ret = snprintf(name, count, "%s", buf);
	hwinfo_data.alsps = name;
	if (ret) {
		printk(KERN_ERR"%s success. %s\n", __func__, buf);
	} else {
		hwinfo_data.alsps = "Not found";
		printk(KERN_ERR"%s failed.\n", __func__);
	}
#else
	hwinfo_data.alsps = "Not found";
#endif
	return count;
}

static ssize_t show_gsensor(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	if (NULL != hwinfo_data.gsensor) {
		return sprintf(buf, "gsensor name :%s\n", hwinfo_data.gsensor);
	} else {
		return sprintf(buf, "gsensor name :Not Found\n");
	}
#else
	return sprintf(buf, "gsensor name :Not support GSensor\n");
#endif
}
static ssize_t store_gsensor(struct device *dev, struct device_attribute *attr,\
		const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	int ret;
	static char name[100] = "Not found";
	ret = snprintf(name, count, "%s", buf);
	hwinfo_data.gsensor = name;
	if (ret) {
		printk(KERN_ERR"%s success .%s\n", __func__, buf);
	} else {
		hwinfo_data.gsensor = "Not found";
		printk(KERN_ERR"%s failed.\n", __func__);
	}
#else
	hwinfo_data.gsensor = "Not found";
#endif
	return count;
}

static ssize_t show_msensor(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	if (NULL != hwinfo_data.msensor) {
		return sprintf(buf, "msensor name :%s\n", hwinfo_data.msensor);
	} else {
		return sprintf(buf, "msensor name :Not Found\n");
	}
#else
	return sprintf(buf, "msensor name :Not support MSensor\n");
#endif
}
static ssize_t store_msensor(struct device *dev, struct device_attribute *attr,\
		const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	int ret;
	static char name[100] = "Not found";
	ret = snprintf(name, 100, "%s", buf);
	hwinfo_data.msensor = name;
	if (ret) {
		printk(KERN_ERR"%s success. %s\n", __func__, buf);
	} else {
		hwinfo_data.msensor = "Not found";
		printk(KERN_ERR"%s failed.\n", __func__);
	}
#else
	hwinfo_data.msensor = "Not found";
#endif
	return count;
}

static ssize_t show_gyro(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	if (NULL != hwinfo_data.gyroscope) {
		return sprintf(buf, "gyro name :%s\n", hwinfo_data.gyroscope);
	} else {
		return sprintf(buf, "gyro name :Not Found\n");
	}
#else
	return sprintf(buf, "gyro name :Not support Gyro\n");
#endif
}
static ssize_t store_gyro(struct device *dev, struct device_attribute *attr,\
		const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	int ret;
	static char name[100] = "Not found";
	ret = snprintf(name, 100, "%s", buf);
	hwinfo_data.gyroscope = name;
	if (ret) {
		printk(KERN_ERR"%s success. %s\n", __func__, buf);
	} else {
		hwinfo_data.gyroscope = "Not found";
		printk(KERN_ERR"%s failed.\n", __func__);
	}
#else
	hwinfo_data.gyroscope = "Not found";
#endif
	return count;
}

static ssize_t show_sar_sensor_1(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	if (NULL != hwinfo_data.sar_sensor_1) {
		return sprintf(buf, "sar_sensor_1 name :%s\n", hwinfo_data.sar_sensor_1);
	} else {
		return sprintf(buf, "sar_sensor_1 name :Not Found\n");
	}
#else
	return sprintf(buf, "sar_sensor_1 name :Not support sar_sensor_1\n");
#endif
}
static ssize_t show_sar_sensor_2(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	if (NULL != hwinfo_data.sar_sensor_2) {
		return sprintf(buf, "sar_sensor_2 name :%s\n", hwinfo_data.sar_sensor_2);
	} else {
		return sprintf(buf, "sar_sensor_2 name :Not Found\n");
	}
#else
	return sprintf(buf, "sar_sensor_2 name :Not support sar_sensor_2\n");
#endif
}
static ssize_t store_sar_sensor_1(struct device *dev, struct device_attribute *attr,\
		const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	int ret;
	static char name[100] = "Not found";
	ret = snprintf(name, 100, "%s", buf);
	hwinfo_data.sar_sensor_1 = name;
	if (ret) {
		printk(KERN_ERR"%s success. %s\n", __func__, buf);
	} else {
		hwinfo_data.sar_sensor_1 = "Not found";
		printk(KERN_ERR"%s failed.\n", __func__);
	}
#else
	hwinfo_data.sar_sensor_1 = "Not found";
#endif
	return count;
}
static ssize_t store_sar_sensor_2(struct device *dev, struct device_attribute *attr,\
		const char *buf, size_t count)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	int ret;
	static char name[100] = "Not found";
	ret = snprintf(name, 100, "%s", buf);
	hwinfo_data.sar_sensor_2 = name;
	if (ret) {
		printk(KERN_ERR"%s success. %s\n", __func__, buf);
	} else {
		hwinfo_data.sar_sensor_2 = "Not found";
		printk(KERN_ERR"%s failed.\n", __func__);
	}
#else
	hwinfo_data.sar_sensor_2 = "Not found";
#endif
	return count;
}

static ssize_t show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "version:%s\n", HARDWARE_INFO_VERSION);
}

static ssize_t show_bat_id(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_HQ_BATTERY_ID
	if (NULL != hwinfo_data.bat_id) {
		return sprintf(buf, "bat_id name :%s\n", hwinfo_data.bat_id);
	} else {
		return sprintf(buf, "bat_id name :Not found\n");
	}
#else
	return sprintf(buf, "bat_id name :Not support Bat_id\n");
#endif
}

static ssize_t show_nfc(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef QCOM_SENSOR_INFO_NEED_SET
	if (NULL != hwinfo_data.nfc) {
		return sprintf(buf, "nfc name :%s\n", hwinfo_data.nfc);
	} else {
		return sprintf(buf, "nfc name :Not found\n");
	}
#else
	return sprintf(buf, "nfc name :Not support nfc\n");
#endif
}

static char *get_stage_name(void)
{
	unsigned int i;
	int stage_val = -1;

	if (NULL != board_info_data) {
	    stage_val = board_info_data->stage_value;
		printk("Reading HW_ID: stage_val: %d\n", stage_val);
	} else {
		printk("HW_ID board_info_data is NULL!\n");
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pcba_stage_table); i ++) {
		if (stage_val == pcba_stage_table[i].stage_value)
			return pcba_stage_table[i].pcba_stage_name;
	}

err:
	return pcba_stage_table[ARRAY_SIZE(pcba_stage_table) - 1].pcba_stage_name;
}
char *get_type_name(void)
{
	unsigned int i;
	int adc_val = -300;
	int adc_val_high, adc_val_low;

	if (NULL != board_info_data) {
		adc_val = board_info_data->adc_value;
		printk("Reading HW_ID: adc_val: %d\n", adc_val);
	} else {
		printk("HW_ID board_info_data is NULL!\n");
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pcba_type_table); i ++) {
		adc_val_high = pcba_type_table[i].adc_value + adc_val_tolerance;
		adc_val_low = adc_val_high - 2 * adc_val_tolerance;

		if ((adc_val >= adc_val_low) && (adc_val < adc_val_high)) {
			return pcba_type_table[i].pcba_type_name;
		}
	}

err:
	return pcba_type_table[ARRAY_SIZE(pcba_type_table) - 1].pcba_type_name;
}
static ssize_t show_hw_id(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "PCBA_%s_%s\n", get_stage_name(), get_type_name());
}

static DEVICE_ATTR(version, 0444, show_version, NULL);
static DEVICE_ATTR(lcm, 0444, show_lcm, NULL);
static DEVICE_ATTR(ctp, 0444, show_ctp, NULL);
static DEVICE_ATTR(ctp_fw, 0444, show_fw_info, NULL);
static DEVICE_ATTR(ctp_color, 0444, show_color_info, NULL);
static DEVICE_ATTR(ctp_lockdown, 0444, show_lockdown_info, NULL);
static DEVICE_ATTR(main_camera, 0444, show_main_camera, NULL);
static DEVICE_ATTR(sub_camera, 0444, show_sub_camera, NULL);
static DEVICE_ATTR(flash, 0444, show_flash, NULL);
static DEVICE_ATTR(emmc_size, 0444, show_emmc_size, NULL);
static DEVICE_ATTR(ram_size, 0444, show_ram_size, NULL);
static DEVICE_ATTR(gsensor, 0644, show_gsensor, store_gsensor);
static DEVICE_ATTR(msensor, 0644, show_msensor, store_msensor);
static DEVICE_ATTR(alsps, 0644, show_alsps, store_alsps);
static DEVICE_ATTR(gyro, 0644, show_gyro, store_gyro);
static DEVICE_ATTR(wifi, 0444, show_wifi, NULL);
static DEVICE_ATTR(bt, 0444, show_bt, NULL);
static DEVICE_ATTR(gps, 0444, show_gps, NULL);
static DEVICE_ATTR(fm, 0444, show_fm, NULL);
static DEVICE_ATTR(main_otp, 0444, show_main_otp, NULL);
static DEVICE_ATTR(sub_otp, 0444, show_sub_otp, NULL);
static DEVICE_ATTR(sar_sensor_1, 0644, show_sar_sensor_1, store_sar_sensor_1);
static DEVICE_ATTR(sar_sensor_2, 0644, show_sar_sensor_2, store_sar_sensor_2);
static DEVICE_ATTR(bat_id, 0444, show_bat_id,NULL);
static DEVICE_ATTR(nfc, 0444, show_nfc,NULL);
static DEVICE_ATTR(hw_id, 0444, show_hw_id, NULL);
static DEVICE_ATTR(fingerprint, 0444, show_fingerprint, NULL);


static struct attribute *hdinfo_attributes[] = {
	&dev_attr_version.attr,
	&dev_attr_lcm.attr,
	&dev_attr_ctp.attr,
	&dev_attr_ctp_fw.attr,
	&dev_attr_ctp_color.attr,
	&dev_attr_ctp_lockdown.attr,
	&dev_attr_main_camera.attr,
	&dev_attr_sub_camera.attr,
	&dev_attr_flash.attr,
	&dev_attr_emmc_size.attr,
	&dev_attr_ram_size.attr,
	&dev_attr_gsensor.attr,
	&dev_attr_msensor.attr,
	&dev_attr_alsps.attr,
	&dev_attr_gyro.attr,
	&dev_attr_wifi.attr,
	&dev_attr_bt.attr,
	&dev_attr_gps.attr,
	&dev_attr_fm.attr,
	&dev_attr_main_otp.attr,
	&dev_attr_sub_otp.attr,
	&dev_attr_sar_sensor_1.attr,
	&dev_attr_sar_sensor_2.attr,
	&dev_attr_bat_id.attr,
	&dev_attr_nfc.attr,
	&dev_attr_hw_id.attr,
	&dev_attr_fingerprint.attr,
	NULL
};

static struct attribute_group hdinfo_attribute_group = {
	.attrs = hdinfo_attributes
};

static int HardwareInfo_driver_probe(struct platform_device *dev)
{
	int err = -1;

	memset(&hwinfo_data, 0, sizeof(hwinfo_data));
	memset(&hw_info_main_otp, 0, sizeof(hw_info_main_otp));
	memset(&hw_info_main_otp, 0, sizeof(hw_info_main_otp));

	err = sysfs_create_group(&(dev->dev.kobj), &hdinfo_attribute_group);
	if (err < 0) {
		printk("** sysfs_create_group failed!\n");
		return err;
	}

	board_info_data = smem_find(SMEM_ID_VENDOR0, sizeof(SMEM_BOARD_INFO_DATA), 0, SMEM_ANY_HOST_FLAG);
	if (!board_info_data) {
		printk("HW_ID can not locate SMEM_ID_VENDOR0 addr\n");
		return err;
	}

	return err;
}

static int HardwareInfo_driver_remove(struct platform_device *dev)
{
	sysfs_remove_group(&(dev->dev.kobj), &hdinfo_attribute_group);

	return 0;
}

static struct platform_driver HardwareInfo_driver = {
	.probe = HardwareInfo_driver_probe,
	.remove = HardwareInfo_driver_remove,
	.driver = {
		.name = "HardwareInfo",
	},
};

static struct platform_device HardwareInfo_device = {
	.name = "HardwareInfo",
	.id = -1,
};

static int __init HardwareInfo_mod_init(void)
{
	int ret = -1;

	ret = platform_device_register(&HardwareInfo_device);
	if (ret) {
		printk("** platform_device_register failed!(%d)\n", ret);
		goto  err;
	}

	ret = platform_driver_register(&HardwareInfo_driver);
	if (ret) {
		printk("** platform_driver_register failed!(%d)\n", ret);
		goto  err2;
	}

	return ret;

err2:
	platform_device_unregister(&HardwareInfo_device);
err:
	return ret;
}


static void __exit HardwareInfo_mod_exit(void)
{
	platform_driver_unregister(&HardwareInfo_driver);
	platform_device_unregister(&HardwareInfo_device);
}


fs_initcall(HardwareInfo_mod_init);
module_exit(HardwareInfo_mod_exit);


MODULE_DESCRIPTION("Hareware Info driver");
MODULE_LICENSE("GPL");
