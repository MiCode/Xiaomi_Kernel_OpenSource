/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "../platform/gnss/gnss.h"
#include "wcn_glb.h"
#include "wcn_glb_reg.h"
#include "wcn_gnss.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "../include/wcn_dbg.h"
#include "wcn_ca_trusty.h"
#include "../sipc/wcn_sipc.h"
#include "wcn_debug_bus.h"
#define GNSS_CALI_DONE_FLAG (0x1314520)

static struct mutex marlin_lock;
static struct wifi_calibration wifi_data;

#ifdef FIRMWARE_PARTITION_DEBUG_EN
static char gnss_firmware_parent_path[FIRMWARE_FILEPATHNAME_LENGTH_MAX];
#endif

static char firmware_file_name[FIRMWARE_FILEPATHNAME_LENGTH_MAX];
static char firmware_file_path[FIRMWARE_FILEPATHNAME_LENGTH_MAX];
char gnss_firmware_path[FIRMWARE_FILEPATHNAME_LENGTH_MAX];
int is_wcn_shutdown;
int is_wcnpll_power_down;
int ge2_bin_type;
static void wcn_show_dev_status(const char *pre_str);

static int wcn_sys_merlion_soft_reset(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	bool force = false;

	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					0x0ba8, &reg_val);
	WCN_INFO("REG 0x64020ba8:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x2ba8, (1 << 20)); /* bit20 clear to 0 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
			0x0ba8, &reg_val);
	WCN_INFO("REG 0x64020ba8:val=0x%x(soft reset sel 1 clear)!\n", reg_val);


	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					0x0b98, &reg_val);
	WCN_INFO("REG 0x64020b98:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x1b98, (1 << 20)); /* bit20 set to 1 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
			0x0b98, &reg_val);
	WCN_INFO("REG 0x64020b98:val=0x%x(soft reset sel 0: WCN soft reset)!\n", reg_val);


	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x13a8, (1 << 24)); /* bit24 set to 1 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
			0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x(PD wcn cfg: WCN auto shutdown en)!\n", reg_val);

	if (wcn_sys_polling_powerdown(wcn_dev) == false) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
						0x0818, &reg_val);
		WCN_INFO("REG 0x64020818:val=0x%x!\n", reg_val);
		wcn_regmap_raw_write_bit(
				wcn_dev->rmap[REGMAP_PMU_APB],
				0x1818, (1 << 7)); /* bit7 set to 1 */
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				0x0818, &reg_val);
		WCN_INFO("REG 0x64020818:val=0x%x(WCN force deepsleep en)!\n", reg_val);
		force = true;
		if (wcn_sys_polling_powerdown(wcn_dev) == false) {
			WCN_ERR("WCN is not powerdown(soft reset)\n");
			wcn_debug_bus_show(wcn_dev, "WCN shutdown failed,First time...");
			wcn_debug_bus_show(wcn_dev, "WCN shutdown failed,Second time...");
			WARN_ON(true);
			return -EBUSY;
		}
	}

	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					0x0b98, &reg_val);
	WCN_INFO("REG 0x64020b98:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x2b98, (1 << 20)); /* 20 clear to 0 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
			0x0b98, &reg_val);
	WCN_INFO("REG 0x64020b98:val=0x%x(soft reset sel 0: WCN soft reset clear)!\n", reg_val);


	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					0x0ba8, &reg_val);
	WCN_INFO("REG 0x64020ba8:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x1ba8, (1 << 20)); /* bit20 set to 1 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
			0x0ba8, &reg_val);
	WCN_INFO("REG 0x64020ba8:val=0x%x(soft reset sel 1 set)!\n", reg_val);

	if (force) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				0x0818, &reg_val);
		WCN_INFO("REG 0x64020818:val=0x%x!\n", reg_val);
		wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x2818, (1 << 7)); /* bit7 clear to 0 */
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
			0x0818, &reg_val);
		WCN_INFO("REG 0x64020818:val=0x%x(WCN force deepsleep clear)!\n", reg_val);
	}

	return 0;
}

void wcn_boot_init(void)
{
	mutex_init(&marlin_lock);
}

void wcn_device_poweroff(void)
{
	u32 i;

	mutex_lock(&marlin_lock);

	for (i = 0; i < WCN_MARLIN_ALL; i++)
		stop_integrate_wcn_truely(i);

	if (marlin_reset_func)
		marlin_reset_func(marlin_callback_para);

	mutex_unlock(&marlin_lock);
	WCN_INFO("all subsys power off finish!\n");
}

static void wcn_assert_to_reset_mdbg(void)
{
	wcn_chip_power_off();
}

static int wcn_sys_chip_reset(struct notifier_block *this, unsigned long ev, void *ptr)
{
	WCN_INFO("%s: reset callback coming\n", __func__);
	wcn_assert_to_reset_mdbg();

	return NOTIFY_DONE;
}

static struct notifier_block wcn_reset_block = {
	.notifier_call = wcn_sys_chip_reset,
};

int wcn_reset_mdbg_notifier_init(void)
{
	atomic_notifier_chain_register(&wcn_reset_notifier_list,
				       &wcn_reset_block);

	return 0;
}

int wcn_reset_mdbg_notifier_deinit(void)
{
	atomic_notifier_chain_unregister(&wcn_reset_notifier_list,
				       &wcn_reset_block);

	return 0;
}

void integ_wcn_chip_power_off(void)
{
	int lock = 0;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		if (wcn_subsys_active_num() == 0) {
			WCN_INFO("%s not module open! test!!! Ignore\n", __func__);
			return;
		}
		lock = mutex_trylock(&marlin_lock);
		WCN_INFO("Poweron/off trylock:%d\n", lock);
		sprdwcn_bus_set_carddump_status(true);
		wcn_show_dev_status("Assert reset before:");
		wcn_sys_merlion_soft_reset(s_wcn_device.btwf_device);
		wcn_dfs_status_clear();
		wcn_set_module_state(false);
		wcn_set_loopcheck_state(false);
		/* WARNING: sblock 3-7 destroy */
		wcn_sipc_chn_set_status_all_false();
		wcn_rfi_status_clear();
		/* wcn_sys_power_clock_unsupport(true); */
		s_wcn_device.btwf_device->power_state = WCN_POWER_STATUS_OFF;
		s_wcn_device.btwf_device->wcn_open_status = 0;
		s_wcn_device.gnss_device->power_state = WCN_POWER_STATUS_OFF;
		s_wcn_device.gnss_device->wcn_open_status = 0;
		s_wcn_device.btwf_device->boot_cp_status = 0;
		s_wcn_device.gnss_device->boot_cp_status = 0;
		/* wcn_power_enable_merlion_domain(false); */
		wcn_sys_power_clock_unsupport(true);
		sprdwcn_bus_set_carddump_status(false);
		wcn_show_dev_status("Assert reset after:");
		if (lock)
			mutex_unlock(&marlin_lock);
	} else {
		sprdwcn_bus_set_carddump_status(false);
		wcn_device_poweroff();
	}

}

/*judge status of sbuf until timeout*/
static void wcn_sbuf_status(u8 dst, u8 channel)
{
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(1000);
	while (1) {
		if (!sbuf_status(dst, channel)) {
			break;
		} else if (time_after(jiffies, timeout)) {
			WCN_INFO("channel %d-%d is not ready!\n",
				 dst, channel);
			break;
		}
		msleep(20);
	}
}

/* only wifi need it */
static void marlin_write_cali_data(void)
{
	phys_addr_t phy_addr;
	u32 cali_flag;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		/* copy calibration file data to target ddr address */
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->wifi.calibration_data;
		wcn_write_data_to_phy_addr(phy_addr, &wifi_data, sizeof(wifi_data));

		/* notify CP to cali */
		cali_flag = WIFI_CALIBRATION_FLAG_VALUE;
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->wifi.calibration_flag;
		wcn_write_data_to_phy_addr(phy_addr, &cali_flag, sizeof(cali_flag));
	} else {
		/* get cali para from RF */
		get_connectivity_config_param(&wifi_data.config_data);
		get_connectivity_cali_param(&wifi_data.cali_data);

		/* copy calibration file data to target ddr address */
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->wifi.calibration_data;
		wcn_write_data_to_phy_addr(phy_addr, &wifi_data, sizeof(wifi_data));

		/* notify CP to cali */
		cali_flag = WIFI_CALIBRATION_FLAG_VALUE;
		phy_addr = s_wcn_device.btwf_device->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->wifi.calibration_flag;
		wcn_write_data_to_phy_addr(phy_addr, &cali_flag, sizeof(cali_flag));
	}

	WCN_INFO("finish\n");
}

/* only wifi need it: AP save the cali data to ini file */
static void marlin_save_cali_data(void)
{
	phys_addr_t phy_addr;

	if (s_wcn_device.btwf_device) {
		memset(&wifi_data.cali_data, 0x0,
		       sizeof(struct wifi_cali_t));
		/* copy calibration file data to target ddr address */
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			phy_addr = s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)
			   &qogirl6_s_wssm_phy_offset_p->wifi.calibration_data +
			   sizeof(struct wifi_config_t);
		} else {
			phy_addr = s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)
			   &s_wssm_phy_offset_p->wifi.calibration_data +
			   sizeof(struct wifi_config_t);
		}
		wcn_read_data_from_phy_addr(phy_addr, &wifi_data.cali_data,
					    sizeof(struct wifi_cali_t));
		dump_cali_file(&wifi_data.cali_data);
		WCN_INFO("finish\n");
	}
}

/* used for distinguish Pike2 or sharkle */
static void gnss_write_version_data(void)
{
	phys_addr_t phy_addr;
	u32 tmp_aon_id[2];

	tmp_aon_id[0] = g_platform_chip_id.aon_chip_id0;
	tmp_aon_id[1] = g_platform_chip_id.aon_chip_id1;
	phy_addr = wcn_get_gnss_base_addr() +
				   GNSS_REC_AON_CHIPID_OFFSET;
	wcn_write_data_to_phy_addr(phy_addr, &tmp_aon_id,
				   GNSS_REC_AON_CHIPID_SIZE);
}

#ifdef FIRMWARE_PARTITION_DEBUG_EN
static int wcn_get_firmware_path(char *firmwarename, char *firmware_path)
{
	if (!firmwarename || !firmware_path)
		return -EINVAL;

	memset(firmware_path, 0, FIRMWARE_FILEPATHNAME_LENGTH_MAX);
	if (strcmp(firmwarename, WCN_MARLIN_DEV_NAME) == 0) {
		if (parse_firmware_path(firmware_path))
			return -EINVAL;
	} else if (strcmp(firmwarename, WCN_GNSS_DEV_NAME) == 0) {
		int folder_path_length = 0;
		/*
		 * GNSS firmware path is the same as BTWF
		 * But the function parse_firmware_path return path
		 * includes filename of wcnmodem
		 */
		if (parse_firmware_path(firmware_path))
			return -EINVAL;
		folder_path_length = strlen(firmware_path)
				     - strlen(WCN_BTWF_FILENAME);
		*(firmware_path + folder_path_length) = 0;
		strncpy(gnss_firmware_parent_path, firmware_path,
			sizeof(gnss_firmware_parent_path));

	} else {
		return -EINVAL;
	}

	WCN_INFO("wcn_dev->firmware_path:%s\n",
		 firmware_path);

	return 0;
}

static int wcn_load_firmware_img(struct wcn_device *wcn_dev,
				 const char *path, unsigned int len)
{
	int read_len, size, i, ret;
	loff_t off = 0;
	unsigned long timeout;
	char *data = NULL;
	char *wcn_image_buffer;
	struct file *file;
	u32 sec_img_magic, wcn_or_gnss = 0;
	struct sys_img_header *pimghdr = NULL;

	/* try to open file */
	for (i = 1; i <= WCN_OPEN_MAX_CNT; i++) {
		file = filp_open(path, O_RDONLY, 0);
		if (IS_ERR(file)) {
			WCN_ERR("try open file %s,count_num:%d, file=%d\n",
				path, i, file);
			if (i == WCN_OPEN_MAX_CNT) {
				WCN_ERR("open file %s error\n", path);
				return -EINVAL;
			}
			ssleep(1);
		} else {
			break;
		}
	}

	WCN_INFO("open image file successfully\n");
	/* read file to buffer */
	size = len;
	wcn_image_buffer = vmalloc(size);
	if (!wcn_image_buffer) {
		fput(file);
		WCN_ERR("no memory\n");
		return -ENOMEM;
	}
	WCN_INFO("wcn_image_buffer=%p will read len:%u\n",
		 wcn_image_buffer, len);

	data = wcn_image_buffer;
	timeout = jiffies + msecs_to_jiffies(4000);
	do {
read_retry:
		read_len = kernel_read(file, wcn_image_buffer, size, &off);
		if (read_len > 0) {
			size -= read_len;
			wcn_image_buffer += read_len;
		} else if (read_len < 0) {
			WCN_INFO("image read erro:%d read:%lld\n",
				 read_len, off);
			msleep(200);
			if (time_before(jiffies, timeout)) {
				goto read_retry;
			} else {
				vfree(data);
				fput(file);
				WCN_INFO("load image fail:%d off:%lld len:%d\n",
					 read_len, off, len);
				return read_len;
			}
		}
	} while ((read_len > 0) && (size > 0));

	fput(file);
	WCN_INFO("After read, wcn_image_buffer=%p size:%d read:%lld\n",
		 wcn_image_buffer, size, off);
	if (size + off != len)
		WCN_INFO("download image may erro!!\n");

	wcn_image_buffer = data;

#if WCN_INTEGRATE_PLATFORM_DEBUG
	if (s_wcn_debug_case == WCN_START_MARLIN_DDR_FIRMWARE_DEBUG)
		memcpy(wcn_image_buffer, marlin_firmware_bin, len);
	else if (s_wcn_debug_case == WCN_START_GNSS_DDR_FIRMWARE_DEBUG)
		memcpy(wcn_image_buffer, gnss_firmware_bin, len);
#endif

	if (wcn_dev_is_marlin(wcn_dev)) {
		for (i = 0; i < 8; i++)
			wcn_write_zero_to_phy_addr(wcn_dev->base_addr +
						   i * 0x4000, 0x4000);
	}

	/* copy file data to target ddr address */
	wcn_write_data_to_phy_addr(wcn_dev->base_addr, data, len);

	pimghdr = (struct sys_img_header *)data;
	sec_img_magic = pimghdr->magic_num;
	if (sec_img_magic == SEC_IMAGE_MAGIC) {
		if (wcn_dev_is_marlin(wcn_dev))
			wcn_or_gnss = 1;
		else if (wcn_dev_is_gnss(wcn_dev))
			wcn_or_gnss = 2;
	} else {
		WCN_INFO("%s image magic 0x%x.\n",
			wcn_dev->name, sec_img_magic);
	}

	if (sec_img_magic == SEC_IMAGE_MAGIC &&
		wcn_or_gnss > 0) {
		if ((SEC_IMAGE_HDR_SIZE + pimghdr->img_real_size) >=
			pimghdr->img_signed_size ||
			pimghdr->img_real_size == 0) {
			WCN_ERR("%s check signed img fail.\n", __func__);
			vfree(wcn_image_buffer);
			return -EINVAL;
		}
		ret = wcn_firmware_sec_verify(wcn_or_gnss,
					wcn_dev->base_addr,
					pimghdr->img_signed_size);
		if (ret < 0) {
			WCN_ERR("%s sec verify fail.\n", wcn_dev->name);
			vfree(wcn_image_buffer);
			return ret;
		}

		wcn_write_zero_to_phy_addr(
			wcn_dev->base_addr + pimghdr->img_real_size,
			pimghdr->img_signed_size - pimghdr->img_real_size);
		if (wcn_write_data_to_phy_addr(
			wcn_dev->base_addr,
			(data + SEC_IMAGE_HDR_SIZE), pimghdr->img_real_size)) {
			WCN_ERR("copy sec bin to phy_addr error.\n");
			vfree(wcn_image_buffer);
			return -ENOMEM;
		}
	}

	vfree(wcn_image_buffer);

	WCN_INFO("%s finish\n", __func__);

	return 0;
}

static int wcn_load_firmware_data(struct wcn_device *wcn_dev)
{
	bool is_gnss;

	WCN_INFO("entry\n");

	if (!wcn_dev)
		return -EINVAL;
	if (strlen(wcn_dev->firmware_path) == 0) {
		/* get firmware path */
		if (wcn_get_firmware_path(wcn_dev->name,
					  wcn_dev->firmware_path) < 0) {
			WCN_ERR("wcn_get_firmware path Failed!\n");
			return -EINVAL;
		}
		WCN_INFO("firmware path=%s\n", wcn_dev->firmware_path);
	}
	is_gnss = wcn_dev_is_gnss(wcn_dev);
	if (is_gnss) {
		memset(wcn_dev->firmware_path, 0,
		       sizeof(wcn_dev->firmware_path));
		strncpy(wcn_dev->firmware_path, gnss_firmware_parent_path,
			sizeof(wcn_dev->firmware_path));
		strncat(wcn_dev->firmware_path, wcn_dev->firmware_path_ext,
			FIRMWARE_FILEPATHNAME_LENGTH_MAX - 1);
		WCN_INFO("gnss path=%s\n", wcn_dev->firmware_path);
	}

	return wcn_load_firmware_img(wcn_dev, wcn_dev->firmware_path,
				     wcn_dev->file_length);
}
#endif

/*
 * This function is used to use the firmware subsystem
 * to load the wcn image.And at the same time support
 * for reading from the partition image.The first way
 * to use the first.
 */
#define GAL_BIN_SIZE 0x57800
#define GNSS_COMBINE_FIRMWARE 0x100000
static int wcn_download_image(struct wcn_device *wcn_dev)
{
	const struct firmware *firmware;
#ifdef FIRMWARE_PARTITION_DEBUG_EN
	int load_fimrware_ret;
#endif
	bool is_marlin;
	int err;
	loff_t off = 0;
	u32 sec_img_magic, wcn_or_gnss = 0;
	struct sys_img_header *pimghdr = NULL;

	is_marlin = wcn_dev_is_marlin(wcn_dev);
	memset(firmware_file_name, 0, FIRMWARE_FILEPATHNAME_LENGTH_MAX);
	memset(firmware_file_path, 0, FIRMWARE_FILEPATHNAME_LENGTH_MAX);
	if (!is_marlin) {
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			strncpy(firmware_file_name, WCN_GNSSMODEM_FILENAME,
				sizeof(firmware_file_name));
		} else {
			if (s_wcn_device.gnss_type == WCN_GNSS_TYPE_GL)
				strncpy(firmware_file_name, WCN_GNSS_FILENAME,
					sizeof(firmware_file_name));
			else if (s_wcn_device.gnss_type == WCN_GNSS_TYPE_BD)
				strncpy(firmware_file_name, WCN_GNSS_BD_FILENAME,
					sizeof(firmware_file_name));
			else
				return -EINVAL;
		}
	}

	if (is_marlin)
		strncpy(firmware_file_name, WCN_BTWF_FILENAME,
			sizeof(firmware_file_name));
	strcat(firmware_file_name, ".bin");
	if (!is_marlin) {
		strcpy(firmware_file_path, gnss_firmware_path);
		strcat(firmware_file_path, firmware_file_name);
		WCN_INFO("gnss firmware path:%s\n", firmware_file_path);
	}

	WCN_INFO("loading image [%s] from firmware subsystem ...\n",
		 firmware_file_name);
	err = request_firmware(&firmware, firmware_file_name, NULL);
	if (err < 0) {
		WCN_ERR("no find image [%s] errno:(%d).\n",
			firmware_file_name, err);
#ifdef FIRMWARE_PARTITION_DEBUG_EN
		load_fimrware_ret = wcn_load_firmware_data(wcn_dev);
		if (load_fimrware_ret != 0) {
			WCN_ERR("wcn_load_firmware_data ERR!\n");
			return -EINVAL;
		}
#endif
	} else {
		WCN_INFO("image size = %d\n", (int)firmware->size);
				/*check is 2to1 bin*/
		if (wcn_check_2to1_bin(wcn_dev, firmware, &off) == 2) {
			if (wcn_write_data_to_phy_addr(wcn_dev->base_addr,
				(void *)(firmware->data + off), wcn_dev->file_length)) {
				WCN_ERR("L3 wcn_btwf_mem_ram_vmap_nocache fail\n");
				release_firmware(firmware);
				return -ENOMEM;
			}
		} else if (wcn_check_2to1_bin(wcn_dev, firmware, &off) == 1) {
			if (firmware->size != GNSS_COMBINE_FIRMWARE) {
				/* force assert */
				wcn_assert_interface(1, "gnss bin codesize error");
				release_firmware(firmware);
				return -1;
			}
			if (wcn_write_data_to_phy_addr(wcn_dev->base_addr,
				(void *)(firmware->data + off), GAL_BIN_SIZE)) {
				WCN_ERR("L3 wcn_gnss_mem_ram_vmap_nocache fail\n");
				release_firmware(firmware);
				return -ENOMEM;
			}
		} else {
			if (wcn_write_data_to_phy_addr(wcn_dev->base_addr,
				(void *)firmware->data, firmware->size)) {
				WCN_ERR("wcn_mem_ram_vmap_nocache fail\n");
				release_firmware(firmware);
				return -ENOMEM;
			}
		}

		pimghdr = (struct sys_img_header *)(firmware->data);
		sec_img_magic = pimghdr->magic_num;
		if (sec_img_magic == SEC_IMAGE_MAGIC) {
			if (wcn_dev_is_marlin(wcn_dev))
				wcn_or_gnss = 1;
			else if (wcn_dev_is_gnss(wcn_dev))
				wcn_or_gnss = 2;
		} else {
			WCN_INFO("%s image magic 0x%x.\n",
				wcn_dev->name, sec_img_magic);
		}

		if (sec_img_magic == SEC_IMAGE_MAGIC &&
			wcn_or_gnss > 0) {
			if (((SEC_IMAGE_HDR_SIZE + pimghdr->img_real_size) >=
				pimghdr->img_signed_size) ||
				pimghdr->img_real_size == 0 ||
				firmware->size < pimghdr->img_signed_size) {
				release_firmware(firmware);
				WCN_ERR("%s check signed size failed.\n",
					wcn_dev->name);
				return -EINVAL;
			}

			err = wcn_firmware_sec_verify(wcn_or_gnss,
				wcn_dev->base_addr, pimghdr->img_signed_size);
			if (err < 0) {
				release_firmware(firmware);
				WCN_ERR("%s sec verify fail.\n", wcn_dev->name);
				return err;
			}

			wcn_write_zero_to_phy_addr(
				wcn_dev->base_addr + pimghdr->img_real_size,
				(pimghdr->img_signed_size -
				pimghdr->img_real_size));

			if (wcn_write_data_to_phy_addr(
				wcn_dev->base_addr,
				(void *)(firmware->data + SEC_IMAGE_HDR_SIZE),
				pimghdr->img_real_size)) {
				WCN_ERR("ram_vmap_nocache fail.\n");
				release_firmware(firmware);
				return -ENOMEM;
			}
		}

		release_firmware(firmware);
		WCN_INFO("loading image [%s] successfully!\n",
			 firmware_file_name);
	}

	return 0;
}

#ifdef FIRMWARE_PARTITION_DEBUG_EN
static void fstab_ab(struct wcn_device *wcn_dev)
{
	if (wcn_dev->fstab == 'a')
		strncat(firmware_file_path, "_a",
			FIRMWARE_FILEPATHNAME_LENGTH_MAX - 1);
	else if (wcn_dev->fstab == 'b')
		strncat(firmware_file_path, "_b",
			FIRMWARE_FILEPATHNAME_LENGTH_MAX - 1);
}

int wcn_download_image_ufs(struct wcn_device *wcn_dev)
{
	int ret = 0;
	struct file *file;

	memset(firmware_file_path, 0, FIRMWARE_FILEPATHNAME_LENGTH_MAX);

	if (wcn_dev->file_path_ufs || wcn_dev->file_path_ext_ufs)
		WCN_INFO("%s load ufs\n", __func__);
	else {
		s_wcn_device.wcn_mm_flag = emmc;
		return 1;
	}

	if (wcn_dev_is_gnss(wcn_dev) &&
		s_wcn_device.gnss_type == WCN_GNSS_TYPE_BD) {
		if (!(wcn_dev->file_path_ext_ufs))
			return 1;
		strncpy(firmware_file_path, wcn_dev->file_path_ext_ufs,
			sizeof(firmware_file_path));
		gnss_file_path_set(firmware_file_path);
		fstab_ab(wcn_dev);
	} else {
		if (!(wcn_dev->file_path_ufs))
			return 1;
		strncpy(firmware_file_path, wcn_dev->file_path_ufs,
			sizeof(firmware_file_path));
		fstab_ab(wcn_dev);
	}

	WCN_INFO("load config ufs file:%s\n", firmware_file_path);
	file = filp_open(firmware_file_path, O_RDONLY, 0);
	if (IS_ERR(file)) {
		WCN_INFO("%s:open UFS failed check emmc, file=%d\n",
				 __func__, file);
		s_wcn_device.wcn_mm_flag = emmc;
		return 1;
	} else {
		WCN_INFO("%s:open UFS file succ", __func__);
		s_wcn_device.wcn_mm_flag = ufs;
	}
	ret = wcn_load_firmware_img(wcn_dev, firmware_file_path,
				    wcn_dev->file_length);
	if (ret == 1)
		WCN_INFO("%s:change dts file path", __func__);
	return ret;
}

int wcn_download_image_emmc(struct wcn_device *wcn_dev)
{
	int ret = 0;
	struct file *file;

	memset(firmware_file_path, 0, FIRMWARE_FILEPATHNAME_LENGTH_MAX);

	if (wcn_dev->file_path || wcn_dev->file_path_ext)
		WCN_INFO("%s load emmc\n", __func__);
	else {
		s_wcn_device.wcn_mm_flag = ufs;
		return 1;
	}

	if (wcn_dev_is_gnss(wcn_dev) &&
		s_wcn_device.gnss_type == WCN_GNSS_TYPE_BD) {
		if (!(wcn_dev->file_path_ext))
			return 1;
		strncpy(firmware_file_path, wcn_dev->file_path_ext,
				sizeof(firmware_file_path));
		fstab_ab(wcn_dev);
		gnss_file_path_set(firmware_file_path);
	} else {
		if (!(wcn_dev->file_path))
			return 1;
		strncpy(firmware_file_path, wcn_dev->file_path,
			sizeof(firmware_file_path));
		fstab_ab(wcn_dev);
	}

	WCN_INFO("load config emmc file:%s\n", firmware_file_path);
	file = filp_open(firmware_file_path, O_RDONLY, 0);
	if (IS_ERR(file)) {
		WCN_INFO("%s:open EMMC failed check ufs, file=%d\n",
				 __func__, file);
		s_wcn_device.wcn_mm_flag = ufs;
		return 1;
	} else {
		WCN_INFO("%s:open EMMC file succ", __func__);
		s_wcn_device.wcn_mm_flag = emmc;
	}
	ret = wcn_load_firmware_img(wcn_dev, firmware_file_path,
				    wcn_dev->file_length);
	if (ret == 1)
		WCN_INFO("%s:change dts file path", __func__);
	return ret;

}
#endif

static int wcn_download_image_new(struct wcn_device *wcn_dev)
{
#ifdef FIRMWARE_PARTITION_DEBUG_EN
	int ret = 0;
	int count = 0;

	while (wcn_dev->file_path_ufs || wcn_dev->file_path) {
		switch (s_wcn_device.wcn_mm_flag) {
		case 0:
			ret = wcn_download_image_ufs(wcn_dev);
			break;
		case 1:
			ret = wcn_download_image_emmc(wcn_dev);
			break;
		default:
			return -EINVAL;
		}
		ssleep(1);
		count++;
		if (count > 32)
			return -EINVAL;
		WCN_INFO("%s: ret=%d", __func__, ret);
		if (ret == 0)
			return ret;
	}
#endif

	/* old function */
	return wcn_download_image(wcn_dev);
}

int wcn_get_reset_reg_setting(void)
{
	const struct firmware *firmware = NULL;
	int err;
	char *fdata;

	err = request_firmware(&firmware, "wifi_board_config.ini", NULL);
	if (err < 0) {
		WCN_INFO("[-]%s request firmware fail\n", __func__);
		return -1;
	}

	fdata = kmalloc(firmware->size+1, GFP_KERNEL);
	*(fdata+firmware->size) = '\0';
	memcpy(fdata, firmware->data, firmware->size);
	if (strstr(fdata, "RST_REG = 1K8")) {
		WCN_INFO("[-]%s : RST_REG = 1K8\n", __func__);
		err = 1;
	} else if (strstr(fdata, "RST_REG = 4K7")) {
		WCN_INFO("[-]%s : RST_REG = 4K7\n", __func__);
		err = 2;
	} else {
		WCN_INFO("[-]%s has no RST Reg setting\n", __func__);
		err = -1;
	}
	kfree(fdata);
	release_firmware(firmware);
	WCN_INFO("%s is end\n", __func__);
	return err;
}

char *integ_gnss_firmware_path_get(void)
{
	char *fpath = firmware_file_path;

	pr_info("%s %s\n", __func__, fpath);

	return fpath;
}

static void wcn_clean_marlin_ddr_flag(struct wcn_device *wcn_dev)
{
	phys_addr_t phy_addr;
	u32 tmp_value;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		tmp_value = MARLIN_CP_INIT_START_MAGIC;
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->marlin.init_status;
		wcn_write_data_to_phy_addr(phy_addr, &tmp_value, sizeof(tmp_value));

		tmp_value = 0;
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->cp2_sleep_status;
		wcn_write_data_to_phy_addr(phy_addr, &tmp_value, sizeof(tmp_value));
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->sleep_flag_addr;
		wcn_write_data_to_phy_addr(phy_addr, &tmp_value, sizeof(tmp_value));
	} else {
		tmp_value = MARLIN_CP_INIT_START_MAGIC;
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->marlin.init_status;
		wcn_write_data_to_phy_addr(phy_addr, &tmp_value, sizeof(tmp_value));

		tmp_value = 0;
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->cp2_sleep_status;
		wcn_write_data_to_phy_addr(phy_addr, &tmp_value, sizeof(tmp_value));
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->sleep_flag_addr;
		wcn_write_data_to_phy_addr(phy_addr, &tmp_value, sizeof(tmp_value));
	}
}

static int wcn_wait_marlin_boot(struct wcn_device *wcn_dev)
{
	u32 wait_count = 0;
	u32 magic_value = 0;
	phys_addr_t phy_addr;
	u32 marlin_cp_init_ready_magic;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		marlin_cp_init_ready_magic = UMW2631_MARLIN_CP_INIT_READY_MAGIC;
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->marlin.init_status;
	} else {
		marlin_cp_init_ready_magic = MARLIN_CP_INIT_READY_MAGIC;
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->marlin.init_status;
	}

	for (wait_count = 0; wait_count < MARLIN_WAIT_CP_INIT_COUNT;
	     wait_count++) {
		wcn_read_data_from_phy_addr(phy_addr,
					    &magic_value, sizeof(u32));
		if (magic_value == marlin_cp_init_ready_magic) {
			WCN_INFO("BTWF: marlin cp init ready!!!\n");
			msleep(MARLIN_WAIT_CP_INIT_POLL_TIME_MS);
			break;
		}

		msleep(MARLIN_WAIT_CP_INIT_POLL_TIME_MS);
		WCN_INFO("BTWF: magic_value=0x%x, wait_count=%d\n",
			 magic_value, wait_count);
	}

	/* get CP ready flag failed */
	if (wait_count >= MARLIN_WAIT_CP_INIT_COUNT) {
		WCN_ERR("MARLIN boot cp timeout!\n");
		magic_value = MARLIN_CP_INIT_FAILED_MAGIC;
		wcn_write_data_to_phy_addr(phy_addr, &magic_value, sizeof(u32));
		return -1;
	}

	return 0;
}

static void wcn_marlin_boot_finish(struct wcn_device *wcn_dev)
{
	phys_addr_t phy_addr;
	u32 magic_value = 0;

	/* save cali data to INI file */
	if (!s_wcn_device.btwf_calibrated) {
		u32 cali_flag;

		marlin_save_cali_data();
		/* clear notify CP calibration flag */
		cali_flag = WIFI_CALIBRATION_FLAG_CLEAR_VALUE;
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			phy_addr = s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)
			   &qogirl6_s_wssm_phy_offset_p->wifi.calibration_flag;
		} else {
			phy_addr = s_wcn_device.btwf_device->base_addr +
			   (phys_addr_t)
			   &s_wssm_phy_offset_p->wifi.calibration_flag;
		}
		wcn_write_data_to_phy_addr(phy_addr, &cali_flag,
					   sizeof(cali_flag));
		s_wcn_device.btwf_calibrated = true;
	}

	/* set success flag */
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->marlin.init_status;
	} else {
		phy_addr = wcn_dev->base_addr +
		   (phys_addr_t)&s_wssm_phy_offset_p->marlin.init_status;
	}
	magic_value = MARLIN_CP_INIT_SUCCESS_MAGIC;
	wcn_write_data_to_phy_addr(phy_addr, &magic_value, sizeof(u32));
}

/*  GNSS assert workaround */
#define GNSS_TEST_OFFSET 0x150050
#define GNSS_TEST_MAGIC 0x12345678
static void gnss_clear_boot_flag(void)
{
	phys_addr_t phy_addr;
	u32 magic_value = 0;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		return;
	phy_addr = wcn_get_gnss_base_addr() + GNSS_TEST_OFFSET;
	wcn_read_data_from_phy_addr(phy_addr, &magic_value, sizeof(u32));
	WCN_INFO("magic value is 0x%x\n", magic_value);
	magic_value = 0;
	wcn_write_data_to_phy_addr(phy_addr, &magic_value, sizeof(u32));
}

/* used for distinguish Pike2 or sharkle or Qogril6 */
static void gnss_read_boot_flag(struct wcn_device *wcn_dev)
{
	phys_addr_t phy_addr;
	u32 magic_value = 0;
	u32 wait_count;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = wcn_dev->base_addr +
				wcn_get_apcp_sync_addr(wcn_dev) +
				s_wcngnss_sync_addr.init_status_phy_addr;
	} else {
		phy_addr = wcn_get_gnss_base_addr() + GNSS_TEST_OFFSET;
	}
	for (wait_count = 0; wait_count < MARLIN_WAIT_CP_INIT_COUNT;
	     wait_count++) {
		wcn_read_data_from_phy_addr(phy_addr,
					    &magic_value, sizeof(u32));
		if (magic_value == GNSS_TEST_MAGIC)
			break;

		msleep(MARLIN_WAIT_CP_INIT_POLL_TIME_MS);
		WCN_INFO("gnss boot: magic_value=%d, wait_count=%d\n",
			 magic_value, wait_count);
	}

	if (wait_count >= GNSS_WAIT_CP_INIT_COUNT) {
		gnss_set_boot_status(WCN_BOOT_CP2_ERR_BOOT);
	}

	WCN_INFO("gnss finish!\n");
}

static int wcn_wait_gnss_boot(struct wcn_device *wcn_dev)
{
	static int cali_flag;
	static int boot_flag;
	u32 wait_count = 0;
	u32 magic_value = 0;
	phys_addr_t phy_addr;

	if (cali_flag) {
		gnss_read_boot_flag(wcn_dev);
		return 0;
	}
	boot_flag = GNSS_CALI_DONE_FLAG;
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		phy_addr = wcn_dev->base_addr +
				wcn_get_apcp_sync_addr(wcn_dev) +
				s_wcngnss_sync_addr.init_status_phy_addr;
		WCN_DBG("gnss init sync flag addr %llu\n", phy_addr);
		boot_flag = GNSS_BOOT_DONE_FLAG;
	} else {
		phy_addr = wcn_dev->base_addr +
			GNSS_CALIBRATION_FLAG_CLEAR_ADDR;
		WCN_DBG("gnss init sync flag addr %llu\n", phy_addr);
	}

	for (wait_count = 0; wait_count < GNSS_WAIT_CP_INIT_COUNT;
		 wait_count++) {
		wcn_read_data_from_phy_addr(phy_addr,
					    &magic_value, sizeof(u32));
		WCN_INFO("gnss cali: magic_value=0x%x, wait_count=%d\n",
			magic_value, wait_count);
		if (magic_value == boot_flag) {
			WCN_DBG("gnss cali: magic_value=0x%x, wait_count=%d\n",
				 magic_value, wait_count);
			WCN_INFO("gnss finish\n");
			break;
		}
		msleep(GNSS_WAIT_CP_INIT_POLL_TIME_MS);
	}

	if (wait_count >= GNSS_WAIT_CP_INIT_COUNT) {
		gnss_set_boot_status(WCN_BOOT_CP2_ERR_BOOT);
		return -1;
	}

	cali_flag = 1;
	return 0;
}

static void wcn_marlin_pre_boot(struct wcn_device *wcn_dev)
{
	if (!s_wcn_device.btwf_calibrated)
		marlin_write_cali_data();
}

static void wcn_gnss_pre_boot(struct wcn_device *wcn_dev)
{
	gnss_write_version_data();
}

/* WCN SYS power && clock support
 * PMIC supports power and clock to WCN SYS.
 * is_btwf_sys: special for BTWF SYS, it needs VDDWIFIPA
 */
int wcn_power_clock_support(bool is_btwf_sys)
{
	int ret;

	WCN_INFO("[+]%s\n", __func__);

	ret = wcn_power_enable_dcxo1v8(true);
	if (ret) {
		WCN_ERR("wcn_power_enable_dcxo1v8:%d", ret);
		return -1;
	}
	usleep_range(10, 15); /* wait power stable */

	ret = wcn_power_enable_vddcon(true);
	if (ret) {
		WCN_ERR("wcn_power_enable_vddcon:%d", ret);
		return -1;
	}
	usleep_range(10, 15); /* wait power stable */

	ret = wcn_power_enable_merlion_domain(true);
	if (ret) {
		WCN_ERR("wcn_power_enable_merlion_domain:%d", ret);
		return -1;
	}
	usleep_range(10, 15); /* wait power stable */

	if (is_btwf_sys) {
		/* ASIC: enable vddcon and wifipa interval time > 1ms */
		usleep_range(VDDWIFIPA_VDDCON_MIN_INTERVAL_TIME,
			     VDDWIFIPA_VDDCON_MAX_INTERVAL_TIME);
		ret = wcn_marlin_power_enable_vddwifipa(true);
		if (ret) {
			WCN_ERR("wcn_marlin_power_enable_vddwifipa:%d", ret);
			return -1;
		}
	}

	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

/* WCN SYS power && clock Unsupport
 * PMIC un-supports power and clock to WCN SYS.
 * is_btwf_sys: special for BTWF SYS, it needs release VDDWIFIPA
 */
int wcn_sys_power_clock_unsupport(bool is_btwf_sys)
{
	int ret;

	WCN_INFO("[+]%s\n", __func__);

	if (is_btwf_sys) {
		ret = wcn_marlin_power_enable_vddwifipa(false);
		if (ret) {
			WCN_ERR("wcn_marlin_power_enable_vddwifipa:%d", ret);
			return -1;
		}

		/* ASIC: disable vddcon, wifipa interval time > 1ms */
		usleep_range(VDDWIFIPA_VDDCON_MIN_INTERVAL_TIME,
			     VDDWIFIPA_VDDCON_MAX_INTERVAL_TIME);
	}

	ret = wcn_power_enable_merlion_domain(false);
	if (ret) {
		WCN_ERR("wcn_power_enable_merlion_domain:%d", ret);
		return -1;
	}
	usleep_range(10, 15); /* wait power stable */

	ret = wcn_power_enable_vddcon(false);
	if (ret) {
		WCN_ERR("wcn_power_enable_vddcon:%d", ret);
		return -1;
	}
	usleep_range(10, 15); /* wait power stable */

	ret = wcn_power_enable_dcxo1v8(false);
	if (ret) {
		WCN_ERR("wcn_power_enable_dcxo1v8:%d", ret);
		return -1;
	}

	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

/* WCN SYS powerup:PMU support WCN SYS power switch on.
 * The WCN SYS will be at power on and wakeup status.
 */
int wcn_sys_power_up(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* PMU XTL, XTL-Buf, PLL stable delay count for WCN SYS
	 * one count time is 1/(32*1024) second.(PMU Clock is 32k)
	 * bit[15:8]:power sequence delay, default 0x90=>0x4
	 * bit[23:16]:power on delay,default 0x20=>0x2
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x23a8, (0xffff<<8)); /* clear to 0 */
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x13a8, (0x0204<<8)); /* set to 0204 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x(Delay cnt)!\n", reg_val);

	/*
	 * PMU: WCN SYS Auto Shutdown Clear
	 * The REG address is the same as XTL delay count,
	 * but we should config this bits with two steps as ASIC required.
	 * Bit[24] clear to 0
	 */
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x23a8, (0x1<<24));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x(auto shutdown clear)!\n",
		     reg_val);

	/*
	 * PMU: WCN SYS Force Shutdown Clear
	 * Power state machine start runs
	 * The REG address is the same as XTL delay count, auto shutdown,
	 * but we should config this bits with many steps as ASIC required.
	 * Bit[25] clear to 0
	 */
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x23a8, (0x1<<25));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x(force shutdown clear)!\n",
			 reg_val);
	/* usleep_range(500, 500); */ /* wait power on */
	/* check power on */
	if (wcn_sys_polling_poweron(wcn_dev) == false) {
		WCN_ERR("[-]%s wcn sys powerup fail\n", __func__);
		return -1;
	}

	/* PMU: WCN SYS Force Deepsleep Clear
	 * Clock state machine start runs
	 * Bit[7] clear to 0
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x0818, &reg_val);
	WCN_INFO("REG 0x64020818:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x2818, (0x1<<7));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x0818, &reg_val);
	WCN_INFO("REG 0x64020818:val=0x%x(Force deep clear)!\n",
		     reg_val);

	/* usleep_range(500, 500); */ /* wait exit deep sleep */

	/* check exit deep sleep */
	if (wcn_sys_polling_wakeup(wcn_dev) == false) {
		WCN_ERR("[-]%s wcn sys wakeup fail\n", __func__);
		return -1;
	}

	/* WCN SYS power on:
	 * The XTL, XTL-BUF, PLL1, PLL2 stable time is about 6.x ms
	 */
	msleep(WCN_SYS_POWER_ON_WAKEUP_TIME);

	/*
	 * Special Debug:maybe btwf,gnss sys wakeup too slow
	 * If set sfware_core_lv_en caused wcn sys enter deep,
	 * and then AP SYS can't access wcn REGs.
	 */
	/* double confirm WCN SYS power up success */
	btwf_sys_polling_wakeup(wcn_dev);
	btwf_sys_polling_poweron(wcn_dev);

	if (wcn_dev_is_gnss(wcn_dev) || wcn_subsys_active_is_gnss_only()) {
		gnss_sys_polling_wakeup(wcn_dev);
		gnss_sys_polling_poweron(wcn_dev);
	} else
		WCN_INFO("GNSS shutdown hold, polling is not required\n");

	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

/* WCN SYS powerdown:PMU support WCN SYS power switch on.
 * The WCN SYS will be at deepsleep and shutdown status.
 */
int wcn_sys_power_down(struct wcn_device *wcn_dev)
{
#if 1	//SPECIAL_DEBUG_EN	//Special debug
	u32 reg_val = 0;
#endif
	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

#if 1	//SPECIAL_DEBUG_EN	//Special debug
	/*
	 * PMU: WCN SYS Auto Shutdown Set
	 * The REG address is the same as XTL delay count,
	 * but we should config this bits with two steps as ASIC required.
	 * Bit[24] clear to 0
	 */
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x13a8, (0x1<<24));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x(auto shutdown set)!\n",
		     reg_val);
#endif
	wcn_ip_allow_sleep(wcn_dev, true);

#if 1	//SPECIAL_DEBUG_EN	//Special debug
	if (wcn_sys_polling_powerdown(wcn_dev) == false) {
		WCN_ERR("[-]%s wcn sys powerdown fail\n", __func__);
		return -1;
	}
#endif

	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

/* check wcn sys is whether at wakeup status:
 * If wcn sys isn't at wakeup status,AP SYS can't operate WCN REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 */
bool wcn_sys_is_wakeup_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s?\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

	/* bit[31:28]:6 wakeup, others(0:deep sleep...) isn't. */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					 0x0860, &reg_val);
		WCN_INFO("REG 0x64020860:val=0x%x!\n", reg_val);
		if ((reg_val & 0xF0000000) != (0x6<<28)) {
			WCN_INFO("wcn is not wakeup!\n");
			return false;
		}
	}

	WCN_INFO("wcn is wakeup!\n");
	return true;
}

/* check wcn sys is whether at deepsleep status:
 * If wcn sys is at deepsleep status,AP SYS can't operate WCN REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 */
bool wcn_sys_is_deepsleep_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s?\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

	/* bit[31:28]:6 wakeup, others(0:deep sleep...) isn't. */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					 0x0860, &reg_val);
		WCN_INFO("REG 0x64020860:val=0x%x!\n", reg_val);
		if ((reg_val & 0xF0000000) != (0x0<<28)) {
			WCN_INFO("wcn isn't deepsleep!\n");
			return false;
		}
	}

	WCN_INFO("wcn is deepsleep!\n");
	return true;
}

/* check wcn sys is whether at poweron status:
 * If wcn sys isn't at poweron status,AP SYS can't operate WCN REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 */
bool wcn_sys_is_poweron_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

	/* Bit[28:24] 0 power domain on,work mode */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					 0x0538, &reg_val);
		WCN_INFO("REG 0x64020538:val=0x%x!\n", reg_val);
		if (((reg_val)&(0x1f<<24)) != 0) {
			WCN_INFO("wcn isn't poweron!\n");
			return false;
		}
	}

	WCN_INFO("wcn is poweron!\n");
	return true;
}

/* check wcn sys is whether at shutdown status:
 * If wcn sys isn't at poweron status,AP SYS can't operate WCN REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 */
bool wcn_sys_is_shutdown_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

	/* Bit[28:24] 0 power domain on,work mode */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
					 0x0538, &reg_val);

		if (((reg_val)&(0x1f<<24)) != (0x7<<24)) {
			return false;
		}
	}

	WCN_INFO("wcn is shutdown!(i=%d, 0x64020538:0x%x)\n", i, reg_val);
	return true;
}

/* WCN_AON_IP_STOP:0xffffffff:allow WCN SYS enter deep sleep.
 * After BTWF and GNSS SYS enter deep sleep, WCN SYS
 * will check this REG, it means WCN IP is stopped working,
 * so WCN SYS can enter deep sleep.
 */
int wcn_ip_allow_sleep(struct wcn_device *wcn_dev, bool allow)
{
	u32 reg_val = 0;

	/* WCN_AON_IP_STOP:0xffffffff:allow WCN SYS enter deep sleep.
	 * After BTWF and GNSS SYS enter deep sleep, WCN SYS
	 * will check this REG, it means WCN IP is stopped working,
	 * so WCN SYS can enter deep sleep.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x00c4, &reg_val);
	WCN_INFO("REG 0x408800c4:val=0x%x, allow=%d!\n",
		reg_val, allow);
	if (allow)
		reg_val = 0xffffffff;
	else
		reg_val = 0;
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x00c4, reg_val);

	/* if allow deep, after ip stop set, the WCN SYS 26M clock is closed
	 * it will cause AP SYS bus-hang if access WCN REGs.
	 */
	if (allow == false) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
					 0x00c4, &reg_val);
		WCN_INFO("Set REG 0x408800c4:val=0x%x(wcn stop ip)!\n",
			      reg_val);
	}

	return 0;
}

/* wcn sys allow go to deep sleep status:
 * If doesn't call this function, the WCN SYS can't go to
 * deep sleep status.
 * WARNING: Operate WCN SYS REGs, we should confirm WCN SYS
 * is at wakeup and poweron status to call this function.
 */
int wcn_sys_allow_deep_sleep(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/*
	 * sfware_core_lv_en set:1 allow WCN SYS send deep sleep
	 * signal to PMU.
	 * After BTWF and GNSS SYS enter deep sleep
	 * WCN SYS will check this value, if it is 1, it can send deep sleep
	 * signal to PMU.
	 * Bit[30] default is 0, set to 1
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0090, &reg_val);
	WCN_INFO("REG 0x4080c090:val=0x%x!\n", reg_val);
	reg_val |= (0x1<<30);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0090, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0090, &reg_val);
	WCN_INFO("REG 0x4080c090:val=0x%x(sfware core lv en)!\n",
		      reg_val);

	wcn_ip_allow_sleep(wcn_dev, true);

	return 0;
}

/* wcn sys forbid go to deep sleep status:
 * If doesn't call this function, the WCN SYS can go to
 * deep sleep status and caused AP SYS can't access WCN REGs.
 */
int wcn_sys_forbid_deep_sleep(struct wcn_device *wcn_dev)
{
	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}
	/* clear force btwf top aon */
	btwf_clear_force_deepsleep_aontop(wcn_dev);
	/* force btwf sys exit deep, and then force wcn sys exit deep */
	btwf_sys_force_exit_deep_sleep(wcn_dev);
	btwf_sys_polling_wakeup(wcn_dev);
	btwf_sys_polling_poweron(wcn_dev);
	wcn_sys_polling_wakeup(wcn_dev);
	if (!(wcn_sys_polling_poweron(wcn_dev)))  {
		WCN_ERR("[-]%s wcn_sys_polling_poweron fail\n", __func__);
		return -1;
	}

	wcn_ip_allow_sleep(wcn_dev, false);

	/* restore btwf default status */
	btwf_sys_clear_force_exit_deep_sleep(wcn_dev);

	return 0;
}

bool wcn_sys_polling_poweron(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling WCN SYS wakeup */
	while (i < WCN_SYS_POWERON_WAKEUP_POLLING_COUNT) {
		if (wcn_sys_is_poweron_status(wcn_dev)) {
			WCN_INFO("wcn sys poweron i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s wcn sys wakeup fail\n", __func__);
	return false;
}

bool wcn_sys_polling_powerdown(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling WCN SYS shutdown */
	while (i < WCN_SYS_SHUTDOWN_POLLING_COUNT) {
		if (wcn_sys_is_shutdown_status(wcn_dev)) {
			WCN_INFO("wcn sys shutdown i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s wcn sys shutdown fail\n", __func__);
	return false;
}

bool wcn_sys_polling_wakeup(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling WCN SYS wakeup */
	while (i < WCN_SYS_POWERON_WAKEUP_POLLING_COUNT) {
		if (wcn_sys_is_wakeup_status(wcn_dev)) {
			WCN_INFO("wcn sys wakeup i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s wcn sys wakeup fail\n", __func__);
	return false;
}

bool wcn_sys_polling_deepsleep(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling WCN SYS deepsleep */
	while (i < WCN_SYS_DEEPSLEEP_POLLING_COUNT) {
		if (wcn_sys_is_deepsleep_status(wcn_dev)) {
			WCN_INFO("wcn sys deepsleep i=%d!\n", i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s wcn sys deepsleep fail\n", __func__);
	return false;
}

bool btwf_sys_polling_deepsleep(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling WCN SYS wakeup */
	while (i < BTWF_SYS_DEEPSLEEP_POLLING_COUNT) {
		if (btwf_sys_is_deepsleep_status(wcn_dev)) {
			WCN_INFO("btwf sys is deepsleep i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s btwf sys deepsleep fail\n", __func__);
	return false;
}

bool btwf_sys_polling_wakeup(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling WCN SYS wakeup */
	while (i < BTWF_SYS_WAKEUP_POLLING_COUNT) {
		if (btwf_sys_is_wakeup_status(wcn_dev)) {
			WCN_INFO("btwf sys is wakeup i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s btwf sys wakeup fail\n", __func__);
	return false;
}

bool btwf_sys_polling_poweron(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling BTWF SYS poweron */
	while (i < BTWF_SYS_POWERON_POLLING_COUNT) {
		if (btwf_sys_is_poweron_status(wcn_dev)) {
			WCN_INFO("btwf sys is poweron i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s btwf sys is powerdown\n", __func__);
	return false;
}

bool btwf_sys_polling_powerdown(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling BTWF SYS powerdown */
	while (i < BTWF_SYS_POWERDOWN_POLLING_COUNT) {
		if (btwf_sys_is_powerdown_status(wcn_dev)) {
			WCN_INFO("btwf sys is powerdown i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s btwf sys is not poweron\n", __func__);
	return false;
}

bool gnss_sys_polling_wakeup(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling WCN SYS wakeup */
	while (i < GNSS_SYS_WAKEUP_POLLING_COUNT) {
		if (gnss_sys_is_wakeup_status(wcn_dev)) {
			WCN_INFO("gnss sys is wakeup i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s gnss sys wakeup fail\n", __func__);
	return false;
}

bool gnss_sys_polling_deepsleep(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling GNSS SYS wakeup */
	while (i < GNSS_SYS_DEEPSLEEP_POLLING_COUNT) {
		if (gnss_sys_is_deepsleep_status(wcn_dev)) {
			WCN_INFO("gnss sys is deepsleep i=%d!\n",
					  i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s gnss sys deepsleep fail\n", __func__);
	return false;
}

bool gnss_sys_polling_poweron(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling BTWF SYS poweron */
	while (i < GNSS_SYS_POWERON_POLLING_COUNT) {
		if (gnss_sys_is_poweron_status(wcn_dev)) {
			WCN_INFO("gnss sys is poweron i=%d!\n", i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s gnss sys is powerdown\n", __func__);
	return false;
}

bool gnss_sys_polling_powerdown(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling BTWF SYS poweron */
	while (i < GNSS_SYS_POWERDOWN_POLLING_COUNT) {
		if (gnss_sys_is_powerdown_status(wcn_dev)) {
			WCN_INFO("gnss sys is powerdown i=%d!\n", i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s gnss sys isn't powerdown\n", __func__);
	return false;
}

bool gnss_sys_polling_wakeup_poweron(struct wcn_device *wcn_dev)
{
	int i = 0;

	/* Polling GNSS SYS wakeup */
	while (i < GNSS_SYS_POWERON_WAKEUP_POLLING_COUNT) {
		if (gnss_sys_is_poweron_status(wcn_dev) &&
			gnss_sys_is_wakeup_status(wcn_dev)) {
			WCN_INFO("gnss sys wakeup i=%d!\n", i);
			return true;
		}

		i++;
		usleep_range(64, 128);
	}

	WCN_ERR("[-]%s gnss sys wakeup poweron fail\n", __func__);
	return false;
}

/* Force BTWF SYS enter deep sleep and then set it auto shutdown.
 * after this operate, BTWF SYS will enter shutdown mode.
 */
int btwf_sys_force_deep_to_shutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* force deep sleep */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("REG 0x4080c098:val=0x%x!\n", reg_val);
	/* btwf_ss_force_deep_sleep Bit[3] default 0=>1 */
	reg_val |= (0x1<<3);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0098, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("Set REG 0x4080c098:val=0x%x(force deep)!\n",
		      reg_val);

	/* btwf_ss_arm_sys_pd_auto_en Bit[12] default 1=>1
	 * maybe the value is cleared.
	 */
	reg_val |= (0x1<<12);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0098, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("Set REG 0x4080c098:val=0x%x(auto shutdown)!\n",
		      reg_val);

	return 0;
}

/* Force GNSS SYS enter deep sleep and then set it auto shutdown.
 * after this operate, GNSS SYS will enter shutdown mode.
 */
int gnss_sys_force_deep_to_shutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* force deep sleep */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("REG 0x4080c0c8:val=0x%x!\n", reg_val);
	/* gnss_ss_force_deep_sleep Bit[3] default 0=>1 */
	reg_val |= (0x1<<3);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x00c8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("Set REG 0x4080c0c8:val=0x%x(force deep)!\n",
		      reg_val);

	/* gnss_ss_arm_sys_pd_auto_en Bit[12] default 1=>1
	 * maybe the value is cleared.
	 */
	reg_val |= (0x1<<12);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x00c8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("Set REG 0x4080c0c8:val=0x%x(auto shutdown)!\n",
		      reg_val);

	return 0;
}

/*
 * Force WCN SYS enter deep sleep and then run auto shutdown.
 * after this operate, WCN SYS will enter shutdown mode.
 */
int wcn_sys_force_deep_to_shutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/*
	 * PMU: WCN SYS Auto Shutdown Set
	 * The REG address is the same as XTL delay count,
	 * but we should config this bits with two steps as ASIC required.
	 * Bit[24] clear to 0
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x(auto shutdown)!\n",
			  reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x13a8, (0x1<<24));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x03a8, &reg_val);
	WCN_INFO("REG 0x640203a8:val=0x%x(auto shutdown set)!\n",
		     reg_val);

	/*
	 * PMU: WCN SYS Force Deepsleep Set
	 * Clock state machine start runs
	 * Bit[7] set to 1
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x0818, &reg_val);
	WCN_INFO("REG 0x64020818:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_PMU_APB],
			0x1818, (0x1<<7));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_PMU_APB],
				 0x0818, &reg_val);
	WCN_INFO("REG 0x64020818:val=0x%x(Force deep clear)!\n",
		     reg_val);

	/* check enter deep sleep */
	if (wcn_sys_polling_deepsleep(wcn_dev) == false) {
		WCN_ERR("[-]%s wcn sys deepsleep fail\n", __func__);
		return -1;
	}

	/* check shutdown */
	if (wcn_sys_polling_powerdown(wcn_dev) == false) {
		WCN_ERR("[-]%s wcn sys powerdown fail\n", __func__);
		return -1;
	}

	return 0;
}

int btwf_sys_wcnpll_power_down(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* wcn_aon_apb bpll1/2_pdn clear */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x0040, &reg_val);
	WCN_INFO("REG 0x4080c040:val=0x%x!\n", reg_val);
	/* wcn_aon_apb bpll1/2_pdn Bit[1:0] default 1=>0 */
	reg_val &= ~((0x1<<0)|(0x1<<1));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0040, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x0040, &reg_val);
	WCN_INFO("Set REG 0x4080c040:val=0x%x(shutdown bpll1/2_pdn)!\n",
		    reg_val);

	is_wcnpll_power_down = 1;
	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

int btwf_sys_wcnpll_power_on(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* wcn_aon_apb bpll1/2_pdn set */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x0040, &reg_val);
	WCN_INFO("REG 0x4080c040:val=0x%x!\n", reg_val);
	/* wcn_aon_apb bpll1/2_pdn Bit[1:0] default 0=>1 */
	reg_val |= ((0x1<<0)|(0x1<<1));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0040, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x0040, &reg_val);
	WCN_INFO("Set REG 0x4080c040:val=0x%x(poweron bpll1/2_pdn)!\n",
		    reg_val);

	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

/* Force BTWF SYS power on and let CPU run.
 * Clear BTWF SYS shutdown and force deep switch,
 * and then let SYS,CPU,Cache... run.
 */
int btwf_sys_poweron(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	u32 status = 0;
#ifdef FLAG_WCN_USER
	struct reg_wcn_aon_ahb_reserved2 *sio_pri = NULL;
#endif
	u32 *value;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/*Bug1772060 Scheme1:btwf wcnpll1/2 power on */
	if (is_wcnpll_power_down) {
		is_wcnpll_power_down = 0;
		if (btwf_sys_wcnpll_power_on(wcn_dev) != 0) {
			WCN_ERR("[-]%s:btwf wcnpll power on fail!\n", __func__);
			return -1;
		}
	}

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x0054, &reg_val);
		value = &reg_val;
		status = wcn_get_reset_reg_setting();
		if (status > 0) {
#ifdef FLAG_WCN_USER
			sio_pri = (struct reg_wcn_aon_ahb_reserved2 *)value;
			sio_pri->priority = 1;
			value = (u32 *)sio_pri;
			wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				0x0054, *value);
#endif
		}
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x0054, &reg_val);
		WCN_INFO("Set REG 0x40880054:val=0x%x(RST PAD Setting)!\n",
			reg_val);
	}
	/*
	 * Set SYS,CPU,Cache at reset status
	 * to avoid after BTWF SYS power on, the CPU runs auto
	 * without any control.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("REG 0x4088000c:val=0x%x!\n", reg_val);
	/* cpu Bit[0],sys Bit[2],cache Bit[4],busmoitor Bit[6] */
	reg_val |= ((0x1<<0)|(0x1<<2)|(0x1<<4)|(0x1<<6));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x000c, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("Set REG 0x4088000c:val=0x%x(cpu...reset)!\n",
		      reg_val);

	/* force deep sleep clear Bit[3] =>0 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("REG 0x4080c098:val=0x%x!\n", reg_val);
	/* btwf_ss_force_deep_sleep Bit[3] default 0=>0 */
	reg_val &= (~(0x1<<3));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0098, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("Set REG 0x4080c098:val=0x%x(clear deep)!\n",
		      reg_val);

	/*
	 * btwf_ss_arm_sys_pd_auto_en Bit[12] default 1=>0
	 * maybe the value is setted.
	 */
	/* be care: reg_val was read at previous step */
	reg_val &= (~(0x1<<12));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0098, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("Set REG 0x4080c098:val=0x%x(clr auto shutdown)!\n",
		      reg_val);

	/* [SharkL6][WCN]BTWF poweron add reg set bit[2]=0*/
	reg_val &= (~(0x1<<2));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0098, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("Set REG 0x4080c098:val=0x%x!(btwf_ss_arm_sys_power_down)\n",
			 reg_val);

	reg_val = 0xffffffff;
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_AON_APB],
				 0x2354, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x0354, &reg_val);
	WCN_INFO("Set REG 0x64000354:val=0x%x!\n", reg_val);


	/* check btwf sys power on whether succ */
	if (btwf_sys_polling_wakeup(wcn_dev) == false) {
		WCN_ERR("[-]%s btwf sys wakeup fail\n", __func__);
		return -1;
	}

	if (btwf_sys_polling_poweron(wcn_dev) == false) {
		WCN_ERR("[-]%s btwf sys poweron fail\n", __func__);
		return -1;
	}

	/* BTWF SYS boot select from DDR:Bit[25:24] => 3 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_BTWF_AHB],
				 0x0410, &reg_val);
	WCN_INFO("REG 0x40130410:val=0x%x!\n", reg_val);
	reg_val |= (0x3<<24);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_BTWF_AHB],
			0x0410, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_BTWF_AHB],
				 0x0410, &reg_val);
	WCN_INFO("Set REG 0x40130410:val=0x%x(DDR Boot)!\n", reg_val);

	/* BTWF SYS SYNC Value set:
	 * WARNING:WARNING:It will be used by BTWF SYS CPU.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x004c, &reg_val);

	WCN_INFO("REG 0x4088004c:val=0x%x!\n", reg_val);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x004c, QOGIRL6_WCN_SPECIAL_SHARME_MEM_ADDR);
	} else {
		wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x004c, WCN_SPECIAL_SHARME_MEM_ADDR);
	}

	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x004c, &reg_val);
	WCN_INFO("Set REG 0x4088004c:val=0x%x(Sync address)!\n",
		      reg_val);

	/* Set SYS,CPU,Cache at release status
	 * let BTWF CPU run
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("REG 0x4088000c:val=0x%x!\n", reg_val);
	/* cpu Bit[0],sys Bit[2],cache Bit[4],busmoitor Bit[6] */
	reg_val &= ~((0x1<<0)|(0x1<<2)|(0x1<<4)|(0x1<<6));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x000c, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("Set REG 0x4088000c:val=0x%x(cpu...reset)!\n",
		      reg_val);

	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

int btwf_gnss_force_unshutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Bit[22:21] 0x0 means poweron status */
	reg_val = (0x6<<21);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
						0x0360, &reg_val);
	WCN_INFO("REG 0x64000360:val=0x%x!(unshutdown)\n", reg_val);

	return 0;

}

int pll1_pll2_stable_time(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Bit[27:12] 0x1458 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					 0x3c8, &reg_val);
	WCN_INFO("REG 0x4080c3c8:val=0x%x!\n", reg_val);
	reg_val |= (0x1458<<12);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x3c8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x3c8, &reg_val);
	WCN_INFO("SET REG 0x4080c3c8:val=0x%x!(btwf pll2)\n", reg_val);

	/* Bit[27:12] 0x1458 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					 0x3c4, &reg_val);
	WCN_INFO("REG 0x4080c3c4:val=0x%x!\n", reg_val);
	reg_val |= (0x1458<<12);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x3c4, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x3c4, &reg_val);
	WCN_INFO("SET REG 0x4080c3c4:val=0x%x!(btwf pll1)\n", reg_val);

	/* Bit[27:12] 0x1458 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					 0x3cc, &reg_val);
	WCN_INFO("REG 0x4080c3cc:val=0x%x!\n", reg_val);
	reg_val |= (0x1458<<12);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x3cc, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x3cc, &reg_val);
	WCN_INFO("SET REG 0x4080c3cc:val=0x%x!(gnss pll)\n", reg_val);

	/* Bit[23:16] 0x37 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x19c, &reg_val);
	WCN_INFO("REG 0x4080c19c:val=0x%x!\n", reg_val);
	reg_val |= (0x37<<16);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				0x19c, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x19c, &reg_val);
	WCN_INFO("SET REG 0x4080c19c:val=0x%x!(btwf pll1)\n", reg_val);

	/* Bit[23:16] 0x37 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x168, &reg_val);
	WCN_INFO("REG 0x4080c168:val=0x%x!\n", reg_val);
	reg_val |= (0x7<<16);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x168, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x168, &reg_val);
	WCN_INFO("SET REG 0x4080c168:val=0x%x!(btwf pll2)\n", reg_val);

	return 0;

}

int btwf_clear_force_shutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Bit[22:21] 0x0 means poweron status */
	reg_val = 0x6<<21;
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			0x0360, &reg_val);
	WCN_INFO("REG 0x64000360:val=0x%x!\n", reg_val);

	return 0;

}

void gnss_shutdown_hold(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	/*GNSS deepsleep/shutdown hold */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x0350, &reg_val);
	WCN_INFO("REG 0x64000350:val=0x%x!\n",
			 reg_val);

	reg_val = (1 << 7) | (1 << 22) | (1 << 23);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_AON_APB],
				 0x1350, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x0350, &reg_val);
	WCN_INFO("Set REG 0x64000350:val=0x%x!(GNSS sleep/shutdown hold)\n", reg_val);
}

void gnss_shutdown_release(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	/*GNSS deepsleep/shutdown hold */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x0350, &reg_val);
	WCN_INFO("REG 0x64000350:val=0x%x!\n",
			 reg_val);

	reg_val = (1 << 7) | (1 << 22) | (1 << 23);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_AON_APB],
				 0x2350, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x0350, &reg_val);
	WCN_INFO("Set REG 0x64000350:val=0x%x!(GNSS sleep/shutdown release)\n", reg_val);
}

int wcn_poweron_device(struct wcn_device *wcn_dev)
{
	int ret;
	bool is_marlin;

	WCN_INFO("[+]%s\n", __func__);

	is_marlin = wcn_dev_is_marlin(wcn_dev);
	if (is_marlin)
		WCN_INFO("[+]%s: btwf!\n", __func__);

	if (is_marlin) { /*BTWF*/
		if (!wcn_subsys_active_is_gnss_only()) { /* gnss off */
			gnss_shutdown_hold(wcn_dev);
		} else { /* gnss on */
			gnss_shutdown_release(wcn_dev);
		}
	} else
		gnss_shutdown_release(wcn_dev);

	/* wcn sys has poweron, just power on self sys */
	if (wcn_subsys_active_num() != 0) {
		if (is_marlin && wcn_subsys_active_is_gnss_only()) {
			/* Enable vddwifipa(see wcn_power_clock_support)*/
			WCN_INFO("[-]%s: Enable VDDWIFIPA when GNSS is turned on\n", __func__);
			usleep_range(VDDWIFIPA_VDDCON_MIN_INTERVAL_TIME,
				VDDWIFIPA_VDDCON_MAX_INTERVAL_TIME);
			ret = wcn_marlin_power_enable_vddwifipa(true);
			if (ret) {
				WCN_ERR("wcn_marlin_power_enable_vddwifipa:%d", ret);
				return -1;
			}
		}
		if (is_marlin) {
			/* btwf power on */
			ret = btwf_sys_poweron(wcn_dev);
			if (ret) {
				WCN_ERR("[-]%s:btwf power on fail!\n",
					__func__);
				return -1;
			}
		} else {
			/* gnss power on */
			ret = gnss_sys_poweron(wcn_dev);
			if (ret) {
				WCN_ERR("[-]%s:gnss power on fail!\n",
					__func__);
				return -1;
			}
		}

		return 0;
	}

	wcn_rfi_status_clear();

	/* first sys(btwf or gnss) power on */
	ret = wcn_power_clock_support(is_marlin);
	if (ret) {
		WCN_ERR("[-]%s:wcn power support fail!\n", __func__);
		return -1;
	}

	ret = btwf_gnss_force_unshutdown(wcn_dev);
		if (ret) {
			WCN_ERR("[-]%s:wcn force shutdown fail!\n", __func__);
			return -1;
		}

	ret = wcn_sys_power_up(wcn_dev);
	if (ret) {
		WCN_ERR("[-]%s:wcn power up fail!\n", __func__);
		return -1;
	}

#if 0	//SPECIAL_DEBUG_EN	//temp debug
	msleep(1000);
#endif

	ret = pll1_pll2_stable_time(wcn_dev);
		if (ret) {
			WCN_ERR("[-]%s:wcn stable time fail!\n", __func__);
			return -1;
		}

	/* ap set allow wcn sys allow deep, btwf, gnss no need to set it */
	ret = wcn_sys_allow_deep_sleep(wcn_dev);
	if (ret) {
		WCN_ERR("[-]%s: ret=%d!\n", __func__, ret);
		return -1;
	}

	if (is_marlin) {
		/*
		 * When WCN power on, BTWF and GNSS is at power on status.
		 * If just BTWF SYS wants to work, we should shutdown GNSS SYS,
		 * or the WCN SYS can't enter deep.
		 */

		/*
		 * If GNSS is forced off: GNSS is accessing DDR at this time,
		 * which may cause DDR exception and cause boot failure.
		 * Solution: gnss_shutdown_release/gnss_shutdown_hold
		 */
		WCN_INFO("Ignore GNSS force deepsleep/shutdown\n");
		/*
		ret = gnss_sys_force_deep_to_shutdown(wcn_dev);
		if (ret) {
			WCN_ERR("[-]%s:gnss shutdown fail!\n", __func__);
			return -1;
		}
		*/

		ret = btwf_clear_force_shutdown(wcn_dev);
		if (ret) {
			WCN_ERR("[-]%s:clear force fail!\n", __func__);
			return -1;
		}

		ret = btwf_sys_poweron(wcn_dev);
		if (ret) {
			WCN_ERR("[-]%s:btwf power on fail!\n", __func__);
			return -1;
		}
	} else {
		/* When WCN power on, BTWF and GNSS is at power on status.
		 * If just GNSS SYS wants to work, we should shutdown BTWF SYS,
		 * or GNSS SYS went to deep sleep, but WCN SYS can't enter deep.
		 * But after GNSS SYS power up, BTWF SYS power up will be failed
		 * as WCN SYS at deep sleep status, or we should send Mailbox
		 * irq to GNSS SYS, which will cause the program too much hard.
		 * GNSS SYS deep sleep will lock the AP SYS, so WCN SYS no needs
		 * to enter deep sleep.(It means no need to shutdown BTWF SYS)
		 */

		/*Bug1772060 Scheme1:btwf wcnpll1/2 power down */
		ret = btwf_sys_wcnpll_power_down(wcn_dev);
		if (ret) {
			WCN_ERR("[-]%s:btwf wcnpll power down fail!\n", __func__);
			return -1;
		}

		/* gnss power on */
		ret = gnss_sys_poweron(wcn_dev);
		if (ret) {
			WCN_ERR("[-]%s:gnss power on fail!\n", __func__);
			return -1;
		}
	}

	WCN_INFO("[-]%s\n", __func__);
	return 0;
}

int btwf_sys_wait_cp2_wfi(struct wcn_device *wcn_dev)
{
	phys_addr_t cp2_sleep_status_phy_addr;
	u32 cp2_sleep_status = 0;
	ktime_t time_end;
	bool cp2_deepsleep = false;

	cp2_sleep_status_phy_addr = wcn_dev->base_addr +
		(phys_addr_t)&qogirl6_s_wssm_phy_offset_p->cp2_sleep_status;
	time_end = ktime_add_ms(ktime_get(), 500);

	do {
		wcn_read_data_from_phy_addr(cp2_sleep_status_phy_addr,
			&cp2_sleep_status, sizeof(cp2_sleep_status));
		if (cp2_sleep_status == BTWF_SW_DEEP_SLEEP_MAGIC) {
			cp2_deepsleep = true;
			break;
		}
	} while (!ktime_after(ktime_get(), time_end));

	WCN_INFO("%s BTWF CP2 deepsleep:%s\n", __func__, cp2_deepsleep ? "yes" : "no");

	return cp2_deepsleep;
}

/* wait BTWF SYS enter deep sleep and then set it auto shutdown.
 * after this operate, BTWF SYS will enter shutdown mode.
 */
int btwf_sys_shutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* btwf_ss_arm_sys_pd_auto_en Bit[12] default 1=>1
	 * maybe the value is cleared.
	 */
	/* Wait CP2 deepsleep Prevent register write conflicts */
	btwf_sys_wait_cp2_wfi(wcn_dev);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("REG 0x4080c098:val=0x%x!\n",
		      reg_val);
	reg_val |= (0x1<<12);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0098, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0098, &reg_val);
	WCN_INFO("Set REG 0x4080c098:val=0x%x(auto shutdown)!\n",
		      reg_val);

	if (btwf_sys_polling_deepsleep(wcn_dev) == false) {
		WCN_ERR("[-]%s btwf sys deep fail\n", __func__);
		return -EBUSY;
	}

	if (btwf_sys_polling_powerdown(wcn_dev) == false) { /* shutdown fail */
		WCN_ERR("[-]%s btwf sys shutdown fail\n", __func__);
		return -EBUSY;
	}
	wcn_sipc_chn_set_status_all_false();
	/*
	 * Set SYS,CPU,Cache at reset status
	 * to avoid after BTWF SYS power on, the CPU runs auto
	 * without any control.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("REG 0x4088000c:val=0x%x!\n", reg_val);
	/* cpu Bit[0],sys Bit[2],cache Bit[4],busmoitor Bit[6] */
	reg_val |= ((0x1<<0)|(0x1<<2)|(0x1<<4)|(0x1<<6));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x000c, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("Set REG 0x4088000c:val=0x%x(cpu...reset)!\n",
		      reg_val);

	/*
	 * Enable A-DIE Adjust voltage:
	 * BTWF SYS set A-DIE Adjust voltage bypass when initilized
	 * to ensure WCN SYS can enter deep sleep.
	 * When shutdown BTWF SYS, A-DIE adjust voltage should
	 * reserve to reduce A-DIE voltage.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0320, &reg_val);
	WCN_INFO("REG 0x4080c0320:val=0x%x!\n", reg_val);
	/* Bit[5]:1:enable adjust, 0: disable adjust */
	reg_val |= (0x1<<5);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0320, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0320, &reg_val);
	WCN_INFO("Set REG 0x4080c0320:val=0x%x(vol adj en)!\n",
			  reg_val);

	return 0;
}

/* wait GNSS SYS enter deep sleep and then set it auto shutdown.
 * after this operate, GNSS SYS will enter shutdown mode.
 */
int gnss_sys_shutdown(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/*
	 * gnss_ss_arm_sys_pd_auto_en Bit[12] default 1=>1
	 * maybe the value is cleared.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("Set REG 0x4080c0c8:val=0x%x(auto shutdown)!\n",
		      reg_val);
	reg_val |= (0x1<<12);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x00c8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("Set REG 0x4080c0c8:val=0x%x(auto shutdown)!\n",
		      reg_val);

	if (gnss_sys_polling_deepsleep(wcn_dev) == false) { /* isn't deep */
		WCN_ERR("[-]%s gnss sys deep fail\n", __func__);
		return -EBUSY;
	}

	if (gnss_sys_polling_powerdown(wcn_dev) == false) {
		WCN_ERR("[-]%s gnss sys shutdown fail\n", __func__);
		return -EBUSY;
	}

	/*
	 * Set SYS,CPU,Cache at reset status
	 * to avoid after BTWF SYS power on, the CPU runs auto
	 * without any control.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("REG 0x4088000c:val=0x%x!\n", reg_val);
	/* cpu Bit[1],cache Bit[5] */
	reg_val |= ((0x1<<1)|(0x1<<5));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x000c, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("Set REG 0x4088000c:val=0x%x(cpu...reset)!\n",
		      reg_val);

	/*
	 * Disable A-DIE Adjust voltage:
	 * Adust voltage time is too long, if GNSS shutdown,
	 * should disable A-DIE adjust voltage.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0320, &reg_val);
	WCN_INFO("REG 0x4080c0320:val=0x%x!\n", reg_val);
	/* Bit[5]:1:enable adjust, 0: disable adjust */
	reg_val &= ~(0x1<<5);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x0320, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x0320, &reg_val);
	WCN_INFO("Set REG 0x4080c0320:val=0x%x(vol adj dis)!\n",
			  reg_val);

	/* workround1 use after chip eco D-die sys dosen't sleep*/
	wcn_ip_allow_sleep(wcn_dev, true);
	return 0;
}

/* check btwf sys is whether at wakeup status:
 * If btwf sys isn't at wakeup status,AP SYS can't operate BTWF REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 */
bool btwf_sys_is_wakeup_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s?\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	/* BTWF: bit[10:7]:0 deep sleep, others(6:wakeup...) isn't. */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0364, &reg_val);
		WCN_INFO("REG 0x64000364:val=0x%x!\n", reg_val);
		if ((reg_val & (0xf<<7)) != (0x6<<7)) {
			WCN_INFO("btwf isn't wakeup!\n");
			return false;
		}
	}
#else
	/* bit[21:12]:6 wakeup, others(0:deep sleep...) isn't. */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x03b4, &reg_val);
	WCN_INFO("REG 0x4080c3b4:val=0x%x!\n", reg_val);
	/* just low 4 bits is for wakeup or deep sleep status */
	if ((reg_val & (0xf<<12)) == (0x6<<12)) {
		WCN_INFO("btwf is wakeup!\n");
		return true;
	}
#endif
	WCN_INFO("btwf is wakeup!\n");
	return true;
}

/* check btwf sys is whether at deepsleep status:
 * If btwf sys isn't at deepsleep status,AP SYS shutdown BTWF SYS
 * isn't safe, or caused BTWF SYS powerup, boot fail next time.
 * and so on.
 */
bool btwf_sys_is_deepsleep_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s?\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	/* BTWF: bit[10:7]:0 deep sleep, others(6:wakeup...) isn't. */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0364, &reg_val);
		WCN_INFO("REG 0x64000364:val=0x%x!\n", reg_val);
		if ((reg_val & (0xf<<7)) != (0x0<<7)) {
			WCN_INFO("btwf isn't deep sleep!\n");
			return false;
		}
	}
#else
	/* bit[21:12]:6 wakeup, others(0:deep sleep...) isn't. */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x03b4, &reg_val);
	WCN_INFO("REG 0x4080c3b4:val=0x%x!\n", reg_val);
	if ((reg_val & (0xf<<12)) == (0x0<<12)) {
		WCN_INFO("btwf is deepsleep!\n");
		return true;
	}
#endif
	WCN_INFO("btwf is deepsleep!\n");
	return true;
}

/* check gnss sys is whether at wakeup status:
 * If gnss sys isn't at wakeup status,AP SYS can't operate GNSS REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 */
bool gnss_sys_is_wakeup_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s?\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	/* GNSS: bit[6:3]:0 deep sleep, others(6:wakeup...) isn't. */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0364, &reg_val);
		WCN_INFO("REG 0x64000364:val=0x%x!\n", reg_val);
		if ((reg_val & (0xf<<3)) != (0x6<<3)) {
			WCN_INFO("gnss isn't wakeup!\n");
			return false;
		}
	}
#else
	/* bit[21:12]:6 wakeup, others(0:deep sleep...) isn't. */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x03c0, &reg_val);
	WCN_INFO("REG 0x4080c3c0:val=0x%x!\n", reg_val);
	if ((reg_val & (0xf<<9)) == (0x6<<9)) {
		WCN_INFO("gnss is wakeup!\n");
		return true;
	}
#endif

	WCN_INFO("gnss is wakeup!\n");
	return true;
}

/* check gnss sys is whether at deepsleep status:
 * If gnss sys isn't at deepsleep status,AP SYS close
 * gnss sys maybe cause gnss powerup,bootup fail
 * next time.
 */
bool gnss_sys_is_deepsleep_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s?\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	/* GNSS: bit[6:3]:0 deep sleep, others(6:wakeup...) isn't. */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0364, &reg_val);
		WCN_INFO("REG 0x64000364:val=0x%x!\n", reg_val);
		if ((reg_val & (0xf<<3)) != (0x0<<3)) {
			WCN_INFO("gnss isn't deepsleep!\n");
			return false;
		}
	}
#else
	/* bit[21:12]:6 wakeup, others(0:deep sleep...) isn't. */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x03c0, &reg_val);
	WCN_INFO("REG 0x4080c3c0:val=0x%x!\n", reg_val);
	if ((reg_val & (0xf<<9)) == (0x0<<9)) {
		WCN_INFO("gnss is deepsleep!\n");
		return true;
	}
#endif
	WCN_INFO("gnss is deepsleep!\n");
	return true;
}

/*
 * If WCN SYS is at deep sleep status, AP SYS can't operate WCN REGs
 * AP SYS can use special signal to force BTWF SYS exit deep sleep status,
 * and it cause WCN SYS exit deep sleep.
 * Notes:there isn't a similar signal for GNSS SYS, so GNSS SYS
 * power up/down will use this function to force wcn sys exit deep sleep.
 */
int btwf_sys_force_exit_deep_sleep(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Bit[16] clear to 0 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x034c, &reg_val);
	WCN_INFO("REG 0x6400034c:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_AON_APB],
			0x234c, (0x1<<16));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x034c, &reg_val);
	WCN_INFO("Set REG 0x6400034c:val=0x%x(exit deep)!\n",
		      reg_val);

	/*
	 * WCN SYS power on or exit from deep sleep:
	 * The XTL, XTL-BUF, PLL1, PLL2 stable time is about 7.2 ms
	 */
	msleep(WCN_SYS_POWER_ON_WAKEUP_TIME);
	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

int btwf_sys_clear_force_exit_deep_sleep(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Bit[16] set to 1 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x034c, &reg_val);
	WCN_INFO("REG 0x6400034c:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_AON_APB],
			0x134c, (0x1<<16));
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
				 0x034c, &reg_val);
	WCN_INFO("Set REG 0x6400034c:val=0x%x(exit deep)!\n",
			  reg_val);

	return 0;
}

/* check btwf sys is whether at poweron status:
 * If btwf sys isn't at poweron status,AP SYS can't operate BTWF REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 * If btwf sys is at poweron status but AP SYS close btwf sys,
 * it maybe cause btwf sys powerup/bootup fail next time
 */
bool btwf_sys_is_poweron_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		/* Bit[29:25] 0x0 means poweron status */
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0360, &reg_val);
		WCN_INFO("REG 0x64000360:val=0x%x!\n", reg_val);
		if ((reg_val & (0x1f<<25)) != (0x0<<25)) {
			WCN_INFO("btwf isn't poweron!\n");
			return false;
		}
	}
#else
	/* Bit[0] 1 power domain is at poweron status */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					 0x03b0, &reg_val);
		WCN_INFO("REG 0x4080c3b0:val=0x%x!\n", reg_val);
		if (((reg_val)&(0x1<<0)) != (0x1<<0)) {
			WCN_INFO("btwf isn't poweron!\n");
			return false;
		}
	}
#endif

	WCN_INFO("btwf is poweron!\n");
	return true;
}

/*
 * check btwf sys is whether at powerdown status:
 * If btwf sys isn't at powerdown status,AP SYS can't shutdown
 */
bool btwf_sys_is_powerdown_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		/* Bit[29:25] 0x7 means powerdown status */
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0360, &reg_val);
		WCN_INFO("REG 0x64000360:val=0x%x!\n", reg_val);
		if ((reg_val & (0x1f<<25)) != (0x7<<25)) {
			WCN_INFO("btwf isn't powerdown!\n");
			return false;
		}
	}
#else
	/* Bit[8] 1 power domain is at powerdown status */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					 0x03b0, &reg_val);
		WCN_INFO("REG 0x4080c3b0:val=0x%x!\n", reg_val);
		if (((reg_val)&(0x1<<8)) != (0x1<<8)) {
			WCN_INFO("btwf isn't powerdown!\n");
			return false;
		}
	}
#endif

	WCN_INFO("btwf is powerdown!\n");
	return true;
}

/*
 * Force GNSS SYS power on and let CPU run.
 * Clear GNSS SYS shutdown and force deep switch,
 * and then let CPU,Cache... run.
 */
int gnss_sys_poweron(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/*
	 * force btwf sys exit deep, and then force wcn sys exit deep
	 * to avoid wcn sys at deep sleep caused AP SYS access
	 * WCN REGs bus-hang.
	 */
	btwf_sys_force_exit_deep_sleep(wcn_dev);

	/* Polling WCN SYS wakeup */
	if (wcn_sys_polling_wakeup(wcn_dev) == false) {
		WCN_ERR("[-]%s wcn sys wakeup fail\n", __func__);
		return -1;
	}

	/* confirm WCN SYS is wakeup, it can access WCN REGs */
	if (btwf_sys_polling_wakeup(wcn_dev) == false) {
		WCN_ERR("[-]%s btwf sys wakeup fail\n", __func__);
		return -1;
	}

	/*
	 * Set CPU,Cache at reset status
	 * to avoid after GNSS SYS power on, the CPU runs auto
	 * without any control.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("REG 0x4088000c:val=0x%x!\n", reg_val);
	/* cpu Bit[1],cache Bit[5] */
	reg_val |= ((0x1<<1)|(0x1<<5));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x000c, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("Set REG 0x4088000c:val=0x%x(cpu...reset)!\n",
		      reg_val);

	/* force deep sleep clear */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("REG 0x4080c0c8:val=0x%x!\n", reg_val);
	/* gnss_ss_force_deep_sleep Bit[3] default 0=>0 */
	reg_val &= (~(0x1<<3));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x00c8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("Set REG 0x4080c0c8:val=0x%x(clear deep)!\n",
		      reg_val);

	/*
	 * gnss_ss_arm_sys_pd_auto_en Bit[12] default 1=>0
	 * maybe the value is setted.
	 */
	reg_val &= (~(0x1<<12));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_APB],
			0x00c8, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
				 0x00c8, &reg_val);
	WCN_INFO("Set REG 0x4080c0c8:val=0x%x(clr auto shutdown)!\n",
		      reg_val);

	/* check gnss sys power on whether succ */
	if (gnss_sys_polling_wakeup_poweron(wcn_dev) == false) {
		WCN_ERR("[-]%s gnss sys wakeup fail\n", __func__);
		return -1;
	}

	/*
	 * GNSS SYS boot select from DDR:Bit[25:24] => 3
	 * GNSS SYS addr offset with BTWF SYS:Bit[23:0]=>0x600000.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_GNSS_SYS_AHB],
				 0x0404, &reg_val);
	WCN_INFO("REG 0x40b18404:val=0x%x!\n", reg_val);
	reg_val |= (0x3<<24);
	reg_val &= ~0x00ffffff;
	reg_val |= WCN_GNSS_DDR_OFFSET;
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_GNSS_SYS_AHB],
			0x0404, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_GNSS_SYS_AHB],
				 0x0404, &reg_val);
	WCN_INFO("Set REG 0x40b18404:val=0x%x(DDR Boot)!\n", reg_val);

	/* GNSS SYS SYNC Value set:
	 * WARNING:WARNING:It will be used by GNSS SYS CPU.
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x0050, &reg_val);
	WCN_INFO("REG 0x40880050:val=0x%x!\n", reg_val);
	wcn_regmap_raw_write_bit(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x0050, WCN_GNSS_SPECIAL_SHARME_MEM_ADDR);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x0050, &reg_val);
	WCN_INFO("Set REG 0x40880050:val=0x%x(Sync address)!\n",
		      reg_val);

	/* Set SYS,CPU,Cache at release status
	 * let BTWF CPU run
	 */
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("REG 0x4088000c:val=0x%x!\n", reg_val);
	/* cpu Bit[1],cache Bit[5] */
	reg_val &= ~((0x1<<1)|(0x1<<5));
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_WCN_AON_AHB],
			0x000c, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_AHB],
				 0x000c, &reg_val);
	WCN_INFO("Set REG 0x4088000c:val=0x%x(cpu...reset)!\n",
		      reg_val);

	/* Restore the default status of BTWF SYS */
	btwf_sys_clear_force_exit_deep_sleep(wcn_dev);

	return 0;
}

/* check btwf sys is whether at poweron status:
 * If btwf sys isn't at poweron status,AP SYS can't operate BTWF REGs
 * or caused AP SYS kernel crash,doze screen,watchdog timeout
 * and so on.
 */
bool gnss_sys_is_poweron_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		/* Bit[9:5] 0x0 means powerdown status */
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0360, &reg_val);
		WCN_INFO("REG 0x64000360:val=0x%x!\n", reg_val);
		if ((reg_val & (0x1f<<5)) != (0x0<<5)) {
			WCN_INFO("gnss isn't poweron!\n");
			return false;
		}
	}
#else
	/* Bit[6] 0 power domain is at poweron status */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					 0x03b0, &reg_val);
		WCN_INFO("REG 0x4080c3b0:val=0x%x!\n", reg_val);
		if (((reg_val)&(0x1<<6)) != (0x1<<6)) {
			WCN_INFO("gnss isn't poweron!\n");
			return false;
		}
	}
#endif

	WCN_INFO("gnss is poweron!\n");
	return true;
}

/* check gnss sys is whether at powerdown status:
 * If gnss sys isn't at powerdown status,WCN SYS can't shutdown
 */
bool gnss_sys_is_powerdown_status(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;
	int i;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return false;
	}

#if 1
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		/* Bit[9:5] 0x7 means powerdown status */
		wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
					 0x0360, &reg_val);
		WCN_INFO("REG 0x64000360:val=0x%x!\n", reg_val);
		if ((reg_val & (0x1f<<5)) != (0x7<<5)) {
			WCN_INFO("gnss isn't powerdown!\n");
			return false;
		}
	}
#else
	/* Bit[14] 1 power domain is at poweron status */
	for (i = 0; i < WCN_REG_POLL_STABLE_COUNT; i++) {
		wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					 0x03b0, &reg_val);
		WCN_INFO("REG 0x4080c3b0:val=0x%x!\n", reg_val);
		if (((reg_val)&(0x1<<14)) != (0x1<<14)) {
			WCN_INFO("gnss isn't powerdown!\n");
			return false;
		}
	}
#endif
	WCN_INFO("gnss is powerdown!\n");
	return true;
}

/* load firmware and boot up sys. */
int wcn_proc_native_start(void *arg)
{
	bool is_marlin;
	int err;
	struct wcn_device *wcn_dev = (struct wcn_device *)arg;

	if (!wcn_dev) {
		WCN_ERR("dev is NULL\n");
		return -ENODEV;
	}

	WCN_INFO("%s enter\n", wcn_dev->name);
	is_marlin = wcn_dev_is_marlin(wcn_dev);

	/* when hot restart handset, the DDR value is error */
	if (is_marlin)
		wcn_clean_marlin_ddr_flag(wcn_dev);

	wcn_dev->boot_cp_status = WCN_BOOT_CP2_OK;
	err = wcn_download_image_new(wcn_dev);
	if (err < 0) {
		WCN_ERR("wcn download image err!\n");
		wcn_dev->boot_cp_status = WCN_BOOT_CP2_ERR_DONW_IMG;
		return -1;
	}

	if (is_marlin)
		/* wifi need calibrate */
		wcn_marlin_pre_boot(wcn_dev);
	else
		/* gnss need prepare some data before bootup */
		wcn_gnss_pre_boot(wcn_dev);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		wcn_dfs_poweron_status_clear(wcn_dev);

	/* boot up system */
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		wcn_poweron_device(wcn_dev);
	else
		wcn_cpu_bootup(wcn_dev);

	wcn_dev->power_state = WCN_POWER_STATUS_ON;
	WCN_DBG("device power_state:%d\n",
		wcn_dev->power_state);

	/* wifi need polling CP ready */
	if (is_marlin) {
		if (wcn_wait_marlin_boot(wcn_dev)) {
			wcn_dev->boot_cp_status = WCN_BOOT_CP2_ERR_BOOT;
			return -1;
		}
		wcn_sbuf_status(3, 4);
		wcn_marlin_boot_finish(wcn_dev);
	} else if (wcn_wait_gnss_boot(wcn_dev)) {
		return -1;
	}

	return 0;
}

int wcn_proc_native_stop(void *arg)
{
	struct wcn_device *wcn_dev = arg;
	bool is_marlin;
	u32 iloop_index;
	u32 reg_nr = 0;
	unsigned int val;
	unsigned int set_value;
	u32 reg_read;
	u32 type;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	WCN_DBG("%s enter\n", __func__);

	if (!wcn_dev)
		return -EINVAL;

	is_marlin = wcn_dev_is_marlin(wcn_dev);
	reg_nr = wcn_dev->reg_shutdown_nr < REG_SHUTDOWN_CNT_MAX ?
		wcn_dev->reg_shutdown_nr : REG_SHUTDOWN_CNT_MAX;
	for (iloop_index = 0; iloop_index < reg_nr; iloop_index++) {
		val = 0;
		type = wcn_dev->ctrl_shutdown_type[iloop_index];
		reg_read = wcn_dev->ctrl_shutdown_reg[iloop_index] -
			wcn_dev->ctrl_shutdown_rw_offset[iloop_index];
		wcn_regmap_read(wcn_dev->rmap[type],
				reg_read,
				&val);
		WCN_INFO("rmap[%d]:ctrl_shutdown_reg[%d] = 0x%x, val=0x%x\n",
			 type, iloop_index, reg_read, val);

		set_value =  wcn_dev->ctrl_shutdown_value
					 [iloop_index];
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			if (wcn_dev->ctrl_shutdown_rw_offset[iloop_index] == 0)
				set_value |= val;
		}
		wcn_regmap_raw_write_bit(wcn_dev->rmap[type],
					 wcn_dev->ctrl_shutdown_reg
					 [iloop_index],
					 set_value);
		udelay(wcn_dev->ctrl_shutdown_us_delay[iloop_index]);
		wcn_regmap_read(wcn_dev->rmap[type],
				reg_read,
				&val);
		WCN_INFO("ctrl_reg[%d] = 0x%x, val=0x%x\n",
			 iloop_index, reg_read, val);
	}

	if (g_match_config && g_match_config->unisoc_wcn_l3) {
		if (!is_marlin)
			/*delay for frequent gnss reset*/
			msleep(20);
	}

	return 0;
}

void wcn_power_wq(struct work_struct *pwork)
{
	bool is_marlin;
	struct wcn_device *wcn_dev;
	struct delayed_work *ppower_wq;
	int ret;

	ppower_wq = container_of(pwork, struct delayed_work, work);
	wcn_dev = container_of(ppower_wq, struct wcn_device, power_wq);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		ret = wcn_proc_native_start(wcn_dev);
		if (ret) {
			WCN_INFO("[-]%s wcn poweron module fail!\n", __func__);
			wcn_debug_bus_show(NULL, "WCN bootup fail,First time...");
			wcn_debug_bus_show(NULL, "WCN bootup fail,Second time...");
			return;
		}
	} else {
		WCN_INFO("start boot :%s\n", wcn_dev->name);
		is_marlin = wcn_dev_is_marlin(wcn_dev);
		if (!is_marlin)
			gnss_clear_boot_flag();

		wcn_power_enable_vddcon(true);
		if (is_marlin) {
			/* ASIC: enable vddcon and wifipa interval time > 1ms */
			usleep_range(VDDWIFIPA_VDDCON_MIN_INTERVAL_TIME,
				     VDDWIFIPA_VDDCON_MAX_INTERVAL_TIME);
			wcn_marlin_power_enable_vddwifipa(true);
		}

		wcn_power_enable_sys_domain(true);
		ret = wcn_proc_native_start(wcn_dev);
		if (ret) {	/* do no complete download done flag */
			WCN_INFO("[-]%s: ret=%d!\n", __func__, ret);
			return;
		}
		WCN_INFO("finish %s!\n", ret ? "ERR" : "OK");
	}

	complete(&wcn_dev->download_done);
}

static void wcn_clear_ddr_gnss_cali_bit(void)
{
	phys_addr_t phy_addr;
	u32 value;
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device;
	if (wcn_dev) {
		value = GNSS_CALIBRATION_FLAG_CLEAR_ADDR_CP;
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			phy_addr = wcn_dev->base_addr +
			   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->gnss_flag_addr;
		} else {
			phy_addr = wcn_dev->base_addr +
			   (phys_addr_t)&s_wssm_phy_offset_p->gnss_flag_addr;
		}
		wcn_write_data_to_phy_addr(phy_addr, &value, sizeof(u32));
		WCN_INFO("set gnss flag off:0x%x\n", value);
	}
	wcn_dev = s_wcn_device.gnss_device;
	value = GNSS_CALIBRATION_FLAG_CLEAR_VALUE;
	phy_addr = wcn_dev->base_addr + GNSS_CALIBRATION_FLAG_CLEAR_ADDR;
	wcn_write_data_to_phy_addr(phy_addr, &value, sizeof(u32));
	WCN_DBG("clear gnss ddr bit\n");
}

static void wcn_set_nognss(u32 val)
{
	phys_addr_t phy_addr;
	struct wcn_device *wcn_dev;

	wcn_dev = s_wcn_device.btwf_device;
	if (wcn_dev) {
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			phy_addr = wcn_dev->base_addr +
			   (phys_addr_t)&qogirl6_s_wssm_phy_offset_p->include_gnss;
		} else {
			phy_addr = wcn_dev->base_addr +
			   (phys_addr_t)&s_wssm_phy_offset_p->include_gnss;
		}
		wcn_write_data_to_phy_addr(phy_addr, &val, sizeof(u32));
		WCN_DBG("gnss:%u\n", val);
	}
}

static struct wcn_device *wcn_get_dev_by_type(u32 subsys_bit)
{
	if (subsys_bit & WCN_MARLIN_MASK)
		return s_wcn_device.btwf_device;
	else if ((subsys_bit & WCN_GNSS_MASK) ||
		 (subsys_bit & WCN_GNSS_BD_MASK) ||
		(subsys_bit & WCN_GNSS_GAL_MASK))
		return s_wcn_device.gnss_device;

	WCN_ERR("invalid subsys:0x%x\n", subsys_bit);

	return NULL;
}

/* pre_str should't NULl */
static void wcn_show_dev_status(const char *pre_str)
{
	u32 status;

	if (s_wcn_device.btwf_device) {
		status = s_wcn_device.btwf_device->wcn_open_status;
		WCN_INFO("%s malrin status[%d] BT:%d FM:%d WIFI:%d MDBG:%d\n",
			 pre_str, status,
			 status & (1 << WCN_MARLIN_BLUETOOTH),
			 status & (1 << WCN_MARLIN_FM),
			 status & (1 << WCN_MARLIN_WIFI),
			 status & (1 << WCN_MARLIN_MDBG));
	}
	if (s_wcn_device.gnss_device) {
		status = s_wcn_device.gnss_device->wcn_open_status;
		WCN_INFO("%s gnss status[%d] GPS:%d GNSS_BD:%d\n",
			 pre_str, status, status & (1 << WCN_GNSS),
			 status & (1 << WCN_GNSS_BD));
	}
}

int start_integrate_wcn_truely(u32 subsys)
{
	static u32 first_start;
	bool is_marlin;
	struct wcn_device *wcn_dev;
	u32 subsys_bit = 1 << subsys;
	unsigned long ret_wait_completion = 0;

	WCN_INFO("start subsys:%d\n", subsys);
	ge2_bin_type = subsys;
	wcn_dev = wcn_get_dev_by_type(subsys_bit);
	if (!wcn_dev) {
		WCN_ERR("wcn dev null!\n");
		return -EINVAL;
	}

	wcn_show_dev_status("before start");
	mutex_lock(&wcn_dev->power_lock);

	/* Check whether opened already */
	if (wcn_dev->wcn_open_status) {
		WCN_INFO("%s opened already = %d, subsys=%d!\n",
			 wcn_dev->name, wcn_dev->wcn_open_status, subsys);
		wcn_dev->wcn_open_status |= subsys_bit;
		wcn_show_dev_status("after start1");
		mutex_unlock(&wcn_dev->power_lock);
		return 0;
	}

	is_marlin = wcn_dev_is_marlin(wcn_dev);
	if (!is_marlin) {
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
			strncpy(&wcn_dev->firmware_path_ext[0], WCN_GNSSMODEM_FILENAME,
				FIRMWARE_FILEPATHNAME_LENGTH_MAX - 1);
			WCN_INFO("wcn gnss modem path=%s\n",
				&wcn_dev->firmware_path_ext[0]);

		} else {
			if (subsys_bit & WCN_GNSS_MASK) {
				strncpy(&wcn_dev->firmware_path_ext[0], WCN_GNSS_FILENAME,
					FIRMWARE_FILEPATHNAME_LENGTH_MAX - 1);
				s_wcn_device.gnss_type = WCN_GNSS_TYPE_GL;
				WCN_INFO("wcn gnss path=%s\n",
					&wcn_dev->firmware_path_ext[0]);
			} else {
				strncpy(&wcn_dev->firmware_path_ext[0], WCN_GNSS_BD_FILENAME,
					FIRMWARE_FILEPATHNAME_LENGTH_MAX - 1);
				s_wcn_device.gnss_type = WCN_GNSS_TYPE_BD;
				WCN_INFO("wcn bd path=%s\n",
					&wcn_dev->firmware_path_ext[0]);
			}
		}
	}

	/* Not opened, so first open */
	init_completion(&wcn_dev->download_done);
	schedule_delayed_work(&wcn_dev->power_wq, 0);

	/*request_firmware keep waiting for file system ready, the max waiting time is 80s*/
	/*after wcn ko move to stage2, the time can be modified back to 20s*/
	ret_wait_completion = wait_for_completion_timeout(&wcn_dev->download_done,
				msecs_to_jiffies(MARLIN_WAIT_CP_INIT_MAX_TIME));

	if (ret_wait_completion <= 0) {
		/* marlin download fail dump memory */
		if (is_marlin)
			goto err_boot_marlin;
		mutex_unlock(&wcn_dev->power_lock);
		return -1;
	} else if (wcn_dev->boot_cp_status) {
		if (wcn_dev->boot_cp_status == WCN_BOOT_CP2_ERR_DONW_IMG) {
			mutex_unlock(&wcn_dev->power_lock);
			return -1;
		}
		if (is_marlin) {
			goto err_boot_marlin;
		} else if (wcn_dev->boot_cp_status == WCN_BOOT_CP2_ERR_BOOT) {
			/* warnning! gnss fake status for poweroff */
			wcn_dev->wcn_open_status |= subsys_bit;
			mutex_unlock(&wcn_dev->power_lock);
			return -1;
		}
	}
	wcn_dev->wcn_open_status |= subsys_bit;

	if (is_marlin) {
		mdbg_atcmd_clean();
		wcn_set_module_state(true);
		wcn_set_loopcheck_state(true);
		marlin_bootup_time_update();
		if (unlikely(!first_start)) {
			wcn_firmware_init();
			first_start = 1;
		} else {
			schedule_work(&wcn_dev->firmware_init_wq);
		}
	}
	mutex_unlock(&wcn_dev->power_lock);

	wcn_show_dev_status("after start2");

	return 0;

err_boot_marlin:
	/* warnning! fake status for poweroff in usr mode */
	wcn_dev->wcn_open_status |= subsys_bit;
	mutex_unlock(&wcn_dev->power_lock);
	WCN_ERR("[-]%s:subsys=%d", __func__, subsys);
	return -ETIMEDOUT;
}

int start_integrate_wcn(u32 subsys)
{
	static u32 first_time;
	u32 btwf_subsys;
	u32 ret;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		first_time = 1;

	WCN_INFO("subsys:%d\n", subsys);
	mutex_lock(&marlin_lock);
	if (unlikely(sprdwcn_bus_get_carddump_status() != 0)) {
		WCN_ERR("in dump status subsys=%d!\n", subsys);
		mutex_unlock(&marlin_lock);
		return -1;
	}
	if (unlikely(get_loopcheck_status() >= 2)) {
		WCN_ERR("loopcheck status error subsys=%d!\n", subsys);
		mutex_unlock(&marlin_lock);
		return -1;
	}

	if (unlikely(!first_time)) {
		if (s_wcn_device.gnss_device) {
			/* clear ddr gps cali bit */
			wcn_set_nognss(WCN_INTERNAL_INCLUD_GNSS_VAL);
			wcn_clear_ddr_gnss_cali_bit();
			ret = start_integrate_wcn_truely(WCN_GNSS);
			if (ret) {
				stop_integrate_wcn_truely(WCN_GNSS);
				mutex_unlock(&marlin_lock);
				return ret;
			}
		} else {
			wcn_set_nognss(WCN_INTERNAL_NOINCLUD_GNSS_VAL);
			WCN_INFO("not include gnss\n");
		}

		/* after cali,gnss powerdown itself,AP sync state by stop op */
		if (s_wcn_device.gnss_device)
			stop_integrate_wcn_truely(WCN_GNSS);
		first_time = 1;

		if (s_wcn_device.btwf_device) {
			if (subsys == WCN_GNSS || subsys == WCN_GNSS_BD)
				btwf_subsys = WCN_MARLIN_MDBG;
			else
				btwf_subsys = subsys;
			ret = start_integrate_wcn_truely(btwf_subsys);
			if (ret) {
				if (ret == -ETIMEDOUT)
					mdbg_assert_interface(
						"MARLIN boot cp timeout 0\n");
				mutex_unlock(&marlin_lock);
				return ret;
			}
		}
		WCN_INFO("first time, start gnss and btwf\n");

		if (s_wcn_device.btwf_device &&
		    (subsys == WCN_GNSS || subsys == WCN_GNSS_BD)) {
			stop_integrate_wcn_truely(btwf_subsys);
		} else {
			mutex_unlock(&marlin_lock);
			return 0;
		}
	}
	ret = start_integrate_wcn_truely(subsys);
	if (ret == -ETIMEDOUT)
		mdbg_assert_interface("MARLIN boot cp timeout");
	mutex_unlock(&marlin_lock);
	return ret;
}

int start_integ_marlin(u32 subsys)
{
	return start_integrate_wcn(subsys);
}

/* force_sleep: 1 for send cmd, 0 for the old way */
static int wcn_wait_wcn_deep_sleep(struct wcn_device *wcn_dev, int force_sleep)
{
	u32 wait_sleep_count = 0;

	for (wait_sleep_count = 0;
	     wait_sleep_count < WCN_WAIT_SLEEP_MAX_COUNT;
	     wait_sleep_count++) {
		if (wcn_get_sleep_status(wcn_dev, force_sleep) == 0)
			break;
		msleep(20);
	}

	WCN_INFO("wait_sleep_count=%d!\n",
			 wait_sleep_count);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		wcn_subsys_shutdown_status(wcn_dev);

	return 0;
}

int stop_integrate_wcn_truely(u32 subsys)
{
	bool is_marlin;
	struct wcn_device *wcn_dev;
	u32 subsys_bit = 1 << subsys;
	int force_sleep = 0;

	WCN_INFO("stop subsys:%d\n", subsys);
	/* Check Parameter whether valid */
	wcn_dev = wcn_get_dev_by_type(subsys_bit);
	if (!wcn_dev) {
		WCN_ERR("wcn dev NULL: subsys=%d!\n", subsys);
		return -EINVAL;
	}

	wcn_show_dev_status("before stop");
	if (unlikely(!(subsys_bit & wcn_dev->wcn_open_status))) {
		/* It wants to stop not opened device */
		WCN_ERR("%s not opend, err: subsys = %d\n",
			wcn_dev->name, subsys);
		return -EINVAL;
	}

	is_marlin = wcn_dev_is_marlin(wcn_dev);

	mutex_lock(&wcn_dev->power_lock);
	wcn_dev->wcn_open_status &= ~subsys_bit;
	if (wcn_dev->wcn_open_status) {
		WCN_INFO("%s subsys(%d) close, and subsys(%d) opend\n",
			 wcn_dev->name, subsys, wcn_dev->wcn_open_status);
		wcn_show_dev_status("after stop1");
		mutex_unlock(&wcn_dev->power_lock);
		return 0;
	}

	WCN_INFO("%s,subsys=%d do stop\n", wcn_dev->name, subsys);
	if (is_marlin)
		wcn_set_loopcheck_state(false);
	/* btwf use the send shutdown cp2 cmd way */
	if (is_marlin && !sprdwcn_bus_get_carddump_status())
		force_sleep = wcn_send_force_sleep_cmd(wcn_dev);
	/* the last module will stop,AP should wait CP2 sleep */
	wcn_wait_wcn_deep_sleep(wcn_dev, force_sleep);

	/* only one module works: stop CPU */
	wcn_proc_native_stop(wcn_dev);
	wcn_power_enable_sys_domain(false);

	if (is_marlin) {
		/* stop common resources if can disable it */
		wcn_marlin_power_enable_vddwifipa(false);
		/* ASIC: disable vddcon, wifipa interval time > 1ms */
		usleep_range(VDDWIFIPA_VDDCON_MIN_INTERVAL_TIME,
			     VDDWIFIPA_VDDCON_MAX_INTERVAL_TIME);
	}

	wcn_power_enable_vddcon(false);
	wcn_sys_soft_reset();
	wcn_sys_soft_release();
	wcn_sys_deep_sleep_en();
	wcn_dev->power_state = WCN_POWER_STATUS_OFF;

	WCN_INFO("%s open_status = %d,power_state=%d,stop subsys=%d!\n",
		 wcn_dev->name, wcn_dev->wcn_open_status,
		 wcn_dev->power_state, subsys);

	if (is_marlin)
		wcn_set_module_state(false);
	mutex_unlock(&wcn_dev->power_lock);

	wcn_show_dev_status("after stop2");
	return 0;
}

int btwf_force_deepsleep_aontop(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Force btwf deep sleep. bit[5] set 1 */
	reg_val = 0x20;
	wcn_regmap_raw_write_bit(
		wcn_dev->rmap[REGMAP_AON_APB], 0x1350, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			0x0350, &reg_val);
	WCN_INFO("Write 0x64000350:val=0x%x!\n", reg_val);


	return 0;

}

int btwf_clear_force_deepsleep_aontop(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Force btwf deep sleep. bit[5] set 0 */
	reg_val = 0x20;
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_AON_APB],
			0x2350, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			0x0350, &reg_val);
	WCN_INFO("Write 0x64000350:val=0x%x!\n", reg_val);

	return 0;

}

int btwf_force_shutdown_aontop(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Force btwf shutdown. bit[21] set 1 */
	reg_val = 0x200000;
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_AON_APB],
			0x1350, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			0x0350, &reg_val);
	WCN_INFO("Write 0x64000350:val=0x%x!\n", reg_val);

	return 0;
}

int btwf_clear_force_shutdown_aontop(struct wcn_device *wcn_dev)
{
	u32 reg_val = 0;

	WCN_INFO("[+]%s\n", __func__);
	if (wcn_dev == NULL) {
		WCN_ERR("[-]%s NULL\n", __func__);
		return -1;
	}

	/* Force btwf shutdown. bit[21] set 0 */
	reg_val = 0x200000;
	wcn_regmap_raw_write_bit(
			wcn_dev->rmap[REGMAP_AON_APB],
			0x2350, reg_val);
	wcn_regmap_read(wcn_dev->rmap[REGMAP_AON_APB],
			0x0350, &reg_val);
	WCN_INFO("Write 0x64000350:val=0x%x!\n", reg_val);


	return 0;
}

int stop_integrate_wcn_module(u32 subsys)
{
	bool is_marlin;
	struct wcn_device *wcn_dev;
	u32 subsys_bit = 1 << subsys;
	int ret;

	WCN_INFO("stop subsys:%d\n", subsys);
	/* Check Parameter whether valid */
	wcn_dev = wcn_get_dev_by_type(subsys_bit);
	if (!wcn_dev) {
		WCN_ERR("wcn dev NULL: subsys=%d!\n", subsys);
		return -EINVAL;
	}

	wcn_show_dev_status("before stop");
	if (unlikely(!(subsys_bit & wcn_dev->wcn_open_status))) {
		/* It wants to stop not opened device */
		WCN_ERR("%s not opend, err: subsys = %d\n",
			wcn_dev->name, subsys);
		/* WARNING: Return 0 by GNSS */
		return wcn_dev_is_marlin(wcn_dev) ? -EINVAL : 0;
	}

	is_marlin = wcn_dev_is_marlin(wcn_dev);

	mutex_lock(&wcn_dev->power_lock);
	wcn_dev->wcn_open_status &= ~subsys_bit;
	if (wcn_dev->wcn_open_status) {
		WCN_INFO("%s subsys(%d) close, and subsys(%d) opend\n",
			 wcn_dev->name, subsys, wcn_dev->wcn_open_status);
		wcn_show_dev_status("after stop1");
		mutex_unlock(&wcn_dev->power_lock);
		return 0;
	}

	WCN_INFO("%s,subsys=%d do stop\n", wcn_dev->name, subsys);
	if (is_marlin)
		wcn_set_loopcheck_state(false);

	wcn_dfs_poweroff_state_clear(wcn_dev);

	/* confirm the shutdown sys is at deep status */
	if (is_marlin) {
		if (btwf_sys_polling_deepsleep(wcn_dev) == false) {
			if (wcn_subsys_active_num() == 0) {
				goto force_poweroff;
			}  else {
				WCN_ERR("%s BTWF deepsleep failed, GNSS on, Assert\n", __func__);
				mutex_unlock(&wcn_dev->power_lock);
				return -BTWF_SYS_ABNORMAL;
			}
		}
	} else {
		if (gnss_sys_polling_deepsleep(wcn_dev) == false) {
			if (wcn_subsys_active_num() == 0) {
				goto force_poweroff;
			}  else {
				WCN_ERR("%s GNSS deepsleep failed, BTWF on, Assert\n", __func__);
				mutex_unlock(&wcn_dev->power_lock);
				return -GNSS_SYS_ABNORMAL;
			}
		}
	}
	if (unlikely(sprdwcn_bus_get_carddump_status() != 0)) {
		WCN_ERR("in dump or reset status subsys=%d!\n", subsys);
		mutex_unlock(&wcn_dev->power_lock);
		return -1;
	}

	/*
	 * forbid deep sleep:
	 * confirm access wcn sys regs normally
	 * when shutdown btwf or gnss sys
	 */
	if (wcn_sys_forbid_deep_sleep(wcn_dev)) {
		WCN_ERR("[-]%s:wcn_sys_forbid_deep_sleep fail", __func__);
		mutex_unlock(&wcn_dev->power_lock);
		return -1;
	}

	if (is_marlin) {
		ret = btwf_sys_shutdown(wcn_dev);
		if (ret == -EBUSY) {
			WCN_ERR("[-]%s:device busy, force poweroff\n", __func__);
			if (wcn_subsys_active_num() == 0) {
				goto force_poweroff;
			}  else {
				WCN_ERR("%s BTWF deepsleep failed, GNSS on, Assert\n", __func__);
				mutex_unlock(&wcn_dev->power_lock);
				return -BTWF_SYS_ABNORMAL;
			}
		} else if (ret) {
			WCN_ERR("[-]%s:btwf_sys_shutdown fail", __func__);
			mutex_unlock(&wcn_dev->power_lock);
			return -1;
		}
	} else {
		ret = gnss_sys_shutdown(wcn_dev);
		if (-EBUSY == ret) {
			if (wcn_subsys_active_num() == 0) {
				goto force_poweroff;
			}  else {
				WCN_ERR("%s GNSS deepsleep failed, BTWF on, Assert\n", __func__);
				mutex_unlock(&wcn_dev->power_lock);
				return -GNSS_SYS_ABNORMAL;
			}
		} else if (ret) {
			WCN_ERR("[-]%s:gnss_sys_shutdown fail", __func__);
			mutex_unlock(&wcn_dev->power_lock);
			/* WARNING: Return 0 by GNSS */
			return 0;
		}
	}

	if (wcn_subsys_active_num() == 0) {
		/*
		 * GNSS only:power up and then down:
		 * wcn_sys_forbid_deep_sleep:it caused btwf sys can't
		 * enter shutdown mode,so force btwf enter deepsleep
		 * and then btwf can shutdown
		 */
		if (is_marlin == false) {
			wcn_ip_allow_sleep(wcn_dev, false);
			/* btwf force deep */
			btwf_force_deepsleep();

			/*
			 * MAX wait is about 7.8ms(222 cycle,32k):A-DIE adjust voltage.
			 * BTWF SYS bypass adjust voltage, GNSS SYS doesn't do it.
			 * Reserve here as ADJUST VOLTAGE feature maybe enable
			 * next time.
			 */
			/* msleep(WCN_SYS_DEEPSLEEP_ADJUST_VOLTAGE_TIME); */
		}

		/* wcn sys resource down */
		wcn_sys_power_down(wcn_dev);
		wcn_sys_power_clock_unsupport(is_marlin);

		wcn_rfi_status_clear();
	}

	wcn_dfs_poweroff_shutdown_clear(wcn_dev);

	wcn_dev->power_state = WCN_POWER_STATUS_OFF;

	WCN_INFO("%s open_status = %d,power_state=%d,stop subsys=%d!\n",
		 wcn_dev->name, wcn_dev->wcn_open_status,
		 wcn_dev->power_state, subsys);

	if (is_marlin) {
		btwf_clear_force_deepsleep_aontop(wcn_dev);
		btwf_clear_force_shutdown_aontop(wcn_dev);
		wcn_set_module_state(false);
	}
	mutex_unlock(&wcn_dev->power_lock);

	wcn_show_dev_status("after stop2");
	return 0;

force_poweroff:
	if (wcn_subsys_active_num() != 0) {
		WARN_ON(true);
		goto unlock_out;
	}

	if (is_marlin)
		wcn_sipc_chn_set_status_all_false();

	wcn_sys_merlion_soft_reset(wcn_dev);
	wcn_dfs_status_clear();
	wcn_sys_power_clock_unsupport(is_marlin);
	wcn_rfi_status_clear();
	wcn_dev->power_state = WCN_POWER_STATUS_OFF;
	if (is_marlin)
		wcn_set_module_state(false);

	WCN_INFO("%s open_status = %d,power_state=%d,stop subsys=%d!\n",
		 wcn_dev->name, wcn_dev->wcn_open_status,
		 wcn_dev->power_state, subsys);

unlock_out:
	mutex_unlock(&wcn_dev->power_lock);

	wcn_show_dev_status("after stop3");

	return 0;
}

bool wcn_is_power_busy(void)
{
	if (wcn_platform_chip_type() != WCN_PLATFORM_TYPE_QOGIRL6)
		return 0;

	return mutex_is_locked(&marlin_lock);
}
EXPORT_SYMBOL_GPL(wcn_is_power_busy);

int stop_integrate_wcn(u32 subsys)
{
	u32 ret;

	mutex_lock(&marlin_lock);
	if (unlikely(sprdwcn_bus_get_carddump_status() != 0)) {
		WCN_ERR("in dump or reset status subsys=%d!\n", subsys);
		mutex_unlock(&marlin_lock);
		return -1;
	}
	if (unlikely(get_loopcheck_status() >= 2)) {
		WCN_ERR("err:loopcheck status error subsys=%d!\n", subsys);
		mutex_unlock(&marlin_lock);
		return -1;
	}

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		ret = stop_integrate_wcn_module(subsys);
	else
		ret = stop_integrate_wcn_truely(subsys);

	mutex_unlock(&marlin_lock);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		if (ret == -BTWF_SYS_ABNORMAL) {
			wcn_assert_interface(WCN_SOURCE_BTWF, "BTWF sys deepsleep/shutdown failed");
		} else if (ret == -GNSS_SYS_ABNORMAL) {
			wcn_assert_interface(WCN_SOURCE_GNSS, "GNSS sys deepsleep/shutdown failed");
			/* WARNING: Return 0 by GNSS */
			return 0;
		}
	}

	return ret;
}

int stop_integ_marlin(u32 subsys)
{
	return stop_integrate_wcn(subsys);
}
