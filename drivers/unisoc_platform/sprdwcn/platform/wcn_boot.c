// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * Filename : wcn_boot.c
 * Abstract : This file is a implementation for wcn sdio hal function
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>

#include "../pcie/edma_engine.h"
#include "../sleep/sdio_int.h"
#include "../sleep/slp_mgr.h"
#include "mem_pd_mgr.h"
#include "wcn_op.h"
#include "wcn_parn_parser.h"
#if 0
#include "pcie_boot.h"
#include "rdc_debug.h"
#endif
#include "gnss/gnss_common.h"
#include "wcn_boot.h"
#include "wcn_dump.h"
#include "wcn_glb.h"
#include "wcn_log.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "wcn_gnss.h"
#include "wcn_txrx.h"
#include "mdbg_type.h"
#include "wcn_glb_reg.h"
#include "wcn_ca_trusty.h"
#include "wcn_gnss_dump.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"marlin."

static int clktype = -1;
module_param(clktype, int, 0444);

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"androidboot."

static char *slot_suffix;
module_param(slot_suffix, charp, 0444);

#ifndef SUFFIX
#define SUFFIX "androidboot.slot_suffix="
#define SUFFIXS "sprdboot.slot_suffix="
#endif

#ifndef WCN_CLKTYPE
#define WCN_CLKTYPE "marlin.clktype="
#endif

#ifndef WCN_CLKTYPE1
#define WCN_CLKTYPE1 "crystal="
#endif

#ifndef REG_PMU_APB_XTL_WAIT_CNT0
#define REG_PMU_APB_XTL_WAIT_CNT0 0xe42b00ac
#endif
static char BTWF_FIRMWARE_PATH[255];
static char GNSS_FIRMWARE_PATH[255];

static struct wifi_calibration wifi_data;
struct completion ge2_completion;
static int first_call_flag = 1;
static struct marlin_device *marlin_dev;
struct sprdwcn_gnss_ops *gnss_ops;
static struct completion find_tsx_completion;
static const struct firmware *tsx_firmware;
static bool is_tsx_found = true;
static bool is_boot_ufs;
unsigned char  flag_reset;
unsigned char  flag_download_done;
char functionmask[8];
static unsigned int reg_val;
static unsigned int clk_wait_val;
static unsigned int cp_clk_wait_val;
static unsigned int marlin2_clk_wait_reg;

#define USB_CARD_DETECT_WAIT_MS	30000
#define CARD_DETECT_WAIT_MS	3000

/* temp for rf pwm mode */
/* static struct regmap *pwm_regmap; */

#define IMG_HEAD_MAGIC		"WCNM"
#define IMG_HEAD_MAGIC_COMBINE	"WCNE"
#define IMG_MARLINAA_TAG	"MLAA"
#define IMG_MARLINAB_TAG	"MLAB"
#define IMG_MARLINAC_TAG	"MLAC"
#define MARLIN_MASK		0x27F
#define GNSS_MASK		0x080
#define AUTO_RUN_MASK		0X100

#define AFC_CALI_FLAG		0x54463031	/* cali flag */
#define AFC_CALI_READ_FINISH	0x12121212
#define WCN_AFC_CALI_PATH	"/productinfo/wcn/tsx_bt_data.txt"

/* #define E2S(x) { case x: return #x; } */

struct head {
	char magic[4];
	u32 version;
	u32 img_count;
} __packed;

struct imageinfo {
	char tag[4];
	u32 offset;
	u32 size;
} __packed;

unsigned int marlin_get_wcn_chipid(void)
{
	static unsigned int chip_id;
	int ret;

	if (unlikely(chip_id != 0))
		return chip_id;

	ret = sprdwcn_bus_reg_read(get_chipid_reg(), &chip_id, 4);
	if (ret < 0) {
		pr_err("marlin read chip ID fail\n");
		return 0;
	}
	pr_info("marlin: chipid=%x, %s\n", chip_id, __func__);

	return chip_id;
}

enum wcn_chip_id_type wcn_get_chip_type(void)
{
	static enum wcn_chip_id_type chip_type;

	if (likely(chip_type))
		return chip_type;

	switch (marlin_get_wcn_chipid()) {
	case MARLIN3_AA_CHIPID:
	case MARLIN3L_AA_CHIPID:
	case MARLIN3E_AA_CHIPID:
		chip_type = WCN_CHIP_ID_AA;
		break;
	case MARLIN3_AB_CHIPID:
	case MARLIN3L_AB_CHIPID:
	case MARLIN3E_AB_CHIPID:
		chip_type = WCN_CHIP_ID_AB;
		break;
	case MARLIN3_AC_CHIPID:
	case MARLIN3L_AC_CHIPID:
	case MARLIN3E_AC_CHIPID:
		chip_type = WCN_CHIP_ID_AC;
		break;
	case MARLIN3_AD_CHIPID:
	case MARLIN3L_AD_CHIPID:
	case MARLIN3E_AD_CHIPID:
		chip_type = WCN_CHIP_ID_AD;
		break;
	default:
		chip_type = WCN_CHIP_ID_INVALID;
		break;
	}

	return chip_type;
}
EXPORT_SYMBOL_GPL(wcn_get_chip_type);

#define WCN_CHIP_NAME_PRE_M3 "Marlin3_"
#define WCN_CHIP_NAME_PRE_M3L "Marlin3Lite_"
#define WCN_CHIP_NAME_PRE_M3E "Marlin3E_"
#define WCN_CHIP_NAME_PRE "ERRO_"

#define _WCN_STR(a) #a
#define WCN_STR(a) _WCN_STR(a)
#define WCN_CON_STR(a, b, c) (a b WCN_STR(c))

static const char *wcn_chip_name_m3[WCN_CHIP_ID_MAX] = {
	"UNKNOWN",
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3, "AA_", MARLIN3_AA_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3, "AB_", MARLIN3_AB_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3, "AC_", MARLIN3_AC_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3, "AD_", MARLIN3_AD_CHIPID),
};

static const char *wcn_chip_name_m3l[WCN_CHIP_ID_MAX] = {
	"UNKNOWN",
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3L, "AA_", MARLIN3L_AA_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3L, "AB_", MARLIN3L_AB_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3L, "AC_", MARLIN3L_AC_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3L, "AD_", MARLIN3L_AD_CHIPID),
};

static const char *wcn_chip_name_m3e[WCN_CHIP_ID_MAX] = {
	"UNKNOWN",
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3E, "AA_", MARLIN3E_AA_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3E, "AB_", MARLIN3E_AB_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3E, "AC_", MARLIN3E_AC_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE_M3E, "AD_", MARLIN3E_AD_CHIPID),
};

static const char *wcn_chip_name[WCN_CHIP_ID_MAX] = {
	"UNKNOWN",
	WCN_CON_STR(WCN_CHIP_NAME_PRE, "AA_", MARLIN_AA_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE, "AB_", MARLIN_AB_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE, "AC_", MARLIN_AC_CHIPID),
	WCN_CON_STR(WCN_CHIP_NAME_PRE, "AD_", MARLIN_AD_CHIPID),
};

const char *integ_wcn_get_chip_name(void);
const char *wcn_get_chip_name(void)
{
	enum wcn_chip_id_type chip_type;
	static const char *chip_name;
	const char **p_wcn_chip_name;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return integ_wcn_get_chip_name();

	if (likely(chip_name))
		return chip_name;

	if (g_match_config && g_match_config->unisoc_wcn_m3)
		p_wcn_chip_name = wcn_chip_name_m3;
	else if (g_match_config && g_match_config->unisoc_wcn_m3lite)
		p_wcn_chip_name = wcn_chip_name_m3l;
	else if (g_match_config && g_match_config->unisoc_wcn_m3e)
		p_wcn_chip_name = wcn_chip_name_m3e;
	else
		p_wcn_chip_name = wcn_chip_name;

	chip_type = wcn_get_chip_type();
	if (chip_type != WCN_CHIP_ID_INVALID)
		chip_name = p_wcn_chip_name[chip_type];

	return p_wcn_chip_name[chip_type];
}
EXPORT_SYMBOL_GPL(wcn_get_chip_name);

static char *wcn_get_chip_tag(void)
{
	enum wcn_chip_id_type chip_type;
	static char *wcn_chip_tag;
	static char * const magic_tag[] = {
		"NULL",
		IMG_MARLINAA_TAG,
		IMG_MARLINAB_TAG,
		IMG_MARLINAC_TAG,
		IMG_MARLINAC_TAG,
	};

	if (likely(wcn_chip_tag))
		return wcn_chip_tag;

	chip_type = wcn_get_chip_type();
	if (chip_type != WCN_CHIP_ID_INVALID)
		wcn_chip_tag = magic_tag[chip_type];

	return wcn_chip_tag;
}

/* get the subsys string */
const char *strno(enum wcn_sub_sys subsys)
{
	switch (subsys) {
	case MARLIN_BLUETOOTH:
		return "MARLIN_BLUETOOTH";
	case MARLIN_FM:
		return "MARLIN_FM";
	case MARLIN_WIFI:
		return "MARLIN_WIFI";
	case MARLIN_WIFI_FLUSH:
		return "MARLIN_WIFI_FLUSH";
	case MARLIN_SDIO_TX:
		return "MARLIN_SDIO_TX";
	case MARLIN_SDIO_RX:
		return "MARLIN_SDIO_RX";
	case MARLIN_MDBG:
		return "MARLIN_MDBG";
	case MARLIN_GNSS:
		return "MARLIN_GNSS";
	case WCN_AUTO:
		return "WCN_AUTO";
	case MARLIN_ALL:
		return "MARLIN_ALL";
	default: return "MARLIN_SUBSYS_UNKNOWN";
	}
/* #undef E2S */
}

static void request_firmware_find_tsx_func(struct work_struct *work)
{
	int ret = 0;

	is_tsx_found = false;
	ret = request_firmware(&tsx_firmware, "tsx_data", NULL);
	if (!ret)
		is_tsx_found = true;

	complete(&find_tsx_completion);
}

/* tsx/dac init */
int marlin_tsx_cali_data_read(struct tsx_data *p_tsx_data)
{
	u32 size = 0;
	char *pdata;
	unsigned long timeleft = 0;
	struct work_struct find_tsx_wq;

	init_completion(&find_tsx_completion);
	INIT_WORK(&find_tsx_wq, request_firmware_find_tsx_func);
	schedule_work(&find_tsx_wq);

	timeleft = wait_for_completion_timeout(&find_tsx_completion, msecs_to_jiffies(1000));
	if (!timeleft) {
		pr_err("find tsx_data timeout\n");
		return -1;
	}
	if (!is_tsx_found) {
		pr_err("file /mnt/vendor/productinfo/wcn/tsx_bt_data.txt not found\n");
		return -1;
	}

	pr_info("open tsx_data success, size: %ld, data: %s\n", tsx_firmware->size, tsx_firmware->data);
	/* copy tsx_firmware->data to buffer */
	size = sizeof(struct tsx_data);
	pdata = (char *)p_tsx_data;
	strncpy(pdata, tsx_firmware->data, size);

	release_firmware(tsx_firmware);
	pr_info("read tsx cali, pdata = %p, p_tsx_data->dac = %02x\n", pdata,
			 p_tsx_data->dac);
	return 0;
}

static u16 marlin_tsx_cali_data_get(void)
{
	//int ret = 1;

	pr_info("tsx cali init flag %d\n", marlin_dev->tsxcali.init_flag);

	if (marlin_dev->tsxcali.init_flag == AFC_CALI_READ_FINISH)
		return marlin_dev->tsxcali.tsxdata.dac;
	/*not find tsx data from a12,only ott use for bt cali*/
	//ret = marlin_tsx_cali_data_read(&marlin_dev->tsxcali.tsxdata);
	marlin_dev->tsxcali.init_flag = AFC_CALI_READ_FINISH;
 #if 0
	if (ret != 0) {
		marlin_dev->tsxcali.tsxdata.dac = 0xffff;
		pr_info("tsx cali read fail! default 0xffff\n");
		return marlin_dev->tsxcali.tsxdata.dac;
	}

	if (marlin_dev->tsxcali.tsxdata.flag != AFC_CALI_FLAG) {
		marlin_dev->tsxcali.tsxdata.dac = 0xffff;
		pr_info("tsx cali flag fail! default 0xffff\n");
		return marlin_dev->tsxcali.tsxdata.dac;
	}
	pr_info("dac flag %d value:0x%x\n",
			    marlin_dev->tsxcali.tsxdata.flag,
			    marlin_dev->tsxcali.tsxdata.dac);
#endif
	marlin_dev->tsxcali.tsxdata.dac = 0xffff;
	pr_info("tsx cali read fail! default 0xffff\n");
	return marlin_dev->tsxcali.tsxdata.dac;
}

static int marlin_judge_imagepack(char *buffer)
{
	struct head *imghead;

	if (buffer == NULL)
		return -1;

	imghead = (struct head *)buffer;

	return strncmp(IMG_HEAD_MAGIC, imghead->magic, 4);
}

static struct imageinfo *marlin_judge_images(char *buffer)
{

	struct imageinfo *imginfo = NULL;
	unsigned char *magic_str;

	magic_str = wcn_get_chip_tag();
	if (!magic_str) {
		pr_err("%s chip id erro\n", __func__);
		return NULL;
	}

	imginfo = kzalloc(sizeof(*imginfo), GFP_KERNEL);
	if (!imginfo)
		return NULL;

	memcpy(imginfo, (buffer + sizeof(struct head)),
	       sizeof(*imginfo));

	if (!strncmp(magic_str, imginfo->tag, 4)) {
		pr_info("%s: marlin imginfo1 type is %s\n",
			 __func__, magic_str);
		return imginfo;
	}
	memcpy(imginfo, buffer + sizeof(*imginfo) + sizeof(struct head),
	       sizeof(*imginfo));
	if (!strncmp(magic_str, imginfo->tag, 4)) {
		pr_info("%s: marlin imginfo2 type is %s\n",
			 __func__, magic_str);
		return imginfo;
	}

	pr_err("Marlin can't find marlin chip image!!!\n");
	kfree(imginfo);

	return  NULL;
}

static char *btwf_load_firmware_data(loff_t off, unsigned long int imag_size)
{
#ifdef FIRMWARE_PARTITION_DEBUG_EN
	int read_len, size, i, opn_num_max = 15;
	char *buffer = NULL;
	char *data = NULL;
	struct file *file = NULL;
	loff_t offset = 0, pos = 0;

	pr_debug("%s entry\n", __func__);

	file = filp_open(BTWF_FIRMWARE_PATH, O_RDONLY, 0);
	for (i = 1; i <= opn_num_max; i++) {
		if (IS_ERR(file)) {
			pr_info("try open file %s,count_num:%d,%s\n",
				BTWF_FIRMWARE_PATH, i, __func__);
			ssleep(1);
			file = filp_open(BTWF_FIRMWARE_PATH, O_RDONLY, 0);
		} else {
			break;
		}
	}
	if (IS_ERR(file)) {
		pr_err("%s open file %s error\n",
			BTWF_FIRMWARE_PATH, __func__);
		return NULL;
	}
	pr_debug("marlin %s open image file  successfully\n",
		__func__);
	size = imag_size;
	buffer = vmalloc(size);
	if (!buffer) {
		fput(file);
		pr_err("no memory for image\n");
		return NULL;
	}

	read_len = kernel_read(file, functionmask, 8, &pos);
	if ((functionmask[0] == 0x00) && (functionmask[1] == 0x00))
		offset = offset + 8;
	else
		functionmask[7] = 0;

	data = buffer;
	offset += off;
	do {
		read_len = kernel_read(file, buffer, size, &offset);
		if (read_len > 0) {
			size -= read_len;
			buffer += read_len;
		}
	} while ((read_len > 0) && (size > 0));
	fput(file);
	pr_info("%s finish read_Len:%d\n", __func__, read_len);
	if (read_len <= 0) {
		vfree(buffer);
		return NULL;
	}

	return data;
#else
	return NULL;
#endif
}

static int marlin_download_from_partition(void)
{
	int err, len, trans_size, ret;
	unsigned long int img_size;
	char *buffer = NULL;
	char *temp = NULL;
	struct imageinfo *imginfo = NULL;
	u32 sec_img_magic;
	struct sys_img_header *pimghdr = NULL;

	if (marlin_dev->maxsz_btwf > get_firmware_max_size())
		img_size = marlin_dev->maxsz_btwf;
	else
		img_size = get_firmware_max_size();

	pr_info("%s entry\n", __func__);
	buffer = btwf_load_firmware_data(0, img_size);
	if (!buffer) {
		pr_info("%s buff is NULL\n", __func__);
		return -1;
	}
	temp = buffer;

	img_size = get_firmware_max_size();
	pimghdr = (struct sys_img_header *)buffer;
	sec_img_magic = pimghdr->magic_num;
	if (sec_img_magic != SEC_IMAGE_MAGIC) {
		pr_info("%s image magic 0x%x.\n",
			__func__, sec_img_magic);
		goto judge_image;
	}

	if (pimghdr->img_real_size > get_firmware_max_size() ||
		pimghdr->img_real_size == 0 ||
		(pimghdr->img_signed_size <=
		(SEC_IMAGE_HDR_SIZE + pimghdr->img_real_size))) {
		pr_err("%s check signed img fail.\n", __func__);
		vfree(temp);
		return -1;
	}

	if (marlin_dev->maxsz_btwf >= pimghdr->img_signed_size) {
		wcn_write_data_to_phy_addr(
			marlin_dev->base_addr_btwf,
			buffer, pimghdr->img_signed_size);
		if (wcn_firmware_sec_verify(1, marlin_dev->base_addr_btwf,
			pimghdr->img_signed_size) < 0) {
			pr_err("%s sec verify fail.\n", __func__);
			vfree(temp);
			return -1;
		}
		memset(buffer + pimghdr->img_real_size, 0,
			pimghdr->img_signed_size - pimghdr->img_real_size);
		wcn_read_data_from_phy_addr(
			marlin_dev->base_addr_btwf + SEC_IMAGE_HDR_SIZE,
			buffer, pimghdr->img_real_size);
	} else if (marlin_dev->maxsz_btwf > 0) {
		// we only get part of the signed-image.
		pr_info("%s without enough reserved memory, check DTS pls.\n",
			__func__);
		vfree(temp);
		return -1;
	} else {
		vfree(temp);
		buffer = btwf_load_firmware_data(SEC_IMAGE_HDR_SIZE,
			img_size);
		if (!buffer) {
			pr_info("%s signed img buff is NULL\n", __func__);
			return -1;
		}
		temp = buffer;
	}

judge_image:
	ret = marlin_judge_imagepack(buffer);
	if (!ret) {
		pr_info("marlin %s imagepack is WCNM type,need parse it\n",
			__func__);
		marlin_get_wcn_chipid();

		imginfo = marlin_judge_images(buffer);
		vfree(temp);
		if (!imginfo) {
			pr_err("marlin:%s imginfo is NULL\n", __func__);
			return -1;
		}
		img_size = imginfo->size;
		if (img_size > get_firmware_max_size())
			pr_info("%s real size %ld is large than the max:%d\n",
				 __func__, img_size, get_firmware_max_size());
		buffer = btwf_load_firmware_data(imginfo->offset, img_size);
		if (!buffer) {
			pr_err("marlin:%s buffer is NULL\n", __func__);
			kfree(imginfo);
			return -1;
		}
		temp = buffer;
		kfree(imginfo);
	}

	len = 0;
	while (len < img_size) {
		trans_size = (img_size - len) > PACKET_SIZE ?
				PACKET_SIZE : (img_size - len);
		memcpy(marlin_dev->write_buffer, buffer + len, trans_size);
		err = sprdwcn_bus_direct_write(get_cp_start_addr() + len,
			marlin_dev->write_buffer, trans_size);
		if (err < 0) {
			pr_err(" %s: dt write SDIO error:%d\n", __func__, err);
			vfree(temp);
			return -1;
		}
		len += PACKET_SIZE;
	}
	vfree(temp);
	pr_info("%s finish and successful\n", __func__);

	return 0;
}

int wcn_gnss_ops_register(struct sprdwcn_gnss_ops *ops)
{
	if (gnss_ops) {
		WARN_ON(1);
		return -EBUSY;
	}

	gnss_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(wcn_gnss_ops_register);

void wcn_gnss_ops_unregister(void)
{
	gnss_ops = NULL;
}

static char *gnss_load_firmware_data(unsigned long int imag_size)
{
#ifdef FIRMWARE_PARTITION_DEBUG_EN
	int read_len, size, i, opn_num_max = 15;
	char *buffer = NULL;
	char *data = NULL;
	struct file *file = NULL;
	loff_t pos = 0;

	pr_info("%s entry\n", __func__);

	gnss_file_path_set(&GNSS_FIRMWARE_PATH[0]);
	pr_info("%s gnss file path=%s\n", __func__, GNSS_FIRMWARE_PATH);

	file = filp_open(GNSS_FIRMWARE_PATH, O_RDONLY, 0);
	for (i = 1; i <= opn_num_max; i++) {
		if (IS_ERR(file)) {
			pr_info("try open file %s,count_num:%d,errno=%ld,%s\n",
				 GNSS_FIRMWARE_PATH, i,
				 PTR_ERR(file), __func__);
			if (PTR_ERR(file) == -ENOENT)
				pr_err("No such file or directory\n");
			if (PTR_ERR(file) == -EACCES)
				pr_err("Permission denied\n");
			ssleep(1);
			file = filp_open(GNSS_FIRMWARE_PATH, O_RDONLY, 0);
		} else {
			break;
		}
	}

	if (IS_ERR(file)) {
		pr_err("%s marlin3 gnss open file %s error\n",
			GNSS_FIRMWARE_PATH, __func__);
		return NULL;
	}
	pr_info("%s open image file  successfully\n", __func__);
	size = imag_size;
	buffer = vmalloc(size);
	if (!buffer) {
		fput(file);
		pr_err("no memory for gnss img\n");
		return NULL;
	}

	data = buffer;
	do {
		read_len = kernel_read(file, buffer, size, &pos);
		if (read_len > 0) {
			size -= read_len;
			buffer += read_len;
		}
	} while ((read_len > 0) && (size > 0));
	fput(file);
	pr_info("%s finish read_Len:%d\n", __func__, read_len);
	if (read_len <= 0)
		return NULL;

	return data;
#else
	return NULL;
#endif
}

static int gnss_download_from_partition(void)
{
	int err, len, trans_size;
	unsigned long int imgpack_size, img_size;
	char *buffer = NULL;
	char *temp = NULL;
	char *temp_img = NULL;
	u32 sec_img_magic, img_copy_len;
	struct sys_img_header *pimghdr = NULL;

	img_size = imgpack_size = get_gnss_firmware_max_size();
	if (marlin_dev->maxsz_gnss > get_gnss_firmware_max_size())
		imgpack_size = marlin_dev->maxsz_gnss;

	pr_info("GNSS %s entry\n", __func__);
	temp = buffer = gnss_load_firmware_data(imgpack_size);
	if (!buffer) {
		pr_info("%s gnss buff is NULL\n", __func__);
		return -1;
	}

	pimghdr = (struct sys_img_header *)buffer;
	sec_img_magic = pimghdr->magic_num;
	if (sec_img_magic != SEC_IMAGE_MAGIC) {
		pr_info("%s image magic 0x%x.\n",
			__func__, sec_img_magic);
		goto write_gnss_img;
	}

	if (pimghdr->img_real_size == 0 ||
		(pimghdr->img_signed_size <=
		(SEC_IMAGE_HDR_SIZE + pimghdr->img_real_size))) {
		vfree(temp);
		pr_err("%s check signed img fail.\n", __func__);
		return -1;
	}

	if (marlin_dev->maxsz_gnss >= pimghdr->img_signed_size) {
		wcn_write_data_to_phy_addr(
			marlin_dev->base_addr_gnss,
			buffer, pimghdr->img_signed_size);
		if (wcn_firmware_sec_verify(2, marlin_dev->base_addr_gnss,
			pimghdr->img_signed_size) < 0) {
			vfree(temp);
			pr_err("%s sec verify fail.\n", __func__);
			return -1;
		}

		if (img_size > pimghdr->img_real_size) {
			memset(buffer + pimghdr->img_real_size, 0,
				img_size - pimghdr->img_real_size);
			img_copy_len = pimghdr->img_real_size;
		} else {
			img_copy_len = img_size;
		}
		wcn_read_data_from_phy_addr(
			marlin_dev->base_addr_gnss + SEC_IMAGE_HDR_SIZE,
			buffer, img_copy_len);
	} else if (marlin_dev->maxsz_gnss > 0) {
		// we only get part of the signed-image.
		pr_info("%s without enough reserved memory, check DTS pls.\n",
			__func__);
		vfree(temp);
		return -1;
	} else {
		if (img_size > pimghdr->img_real_size) {
			memset(buffer + pimghdr->img_real_size, 0,
				img_size - pimghdr->img_real_size);
			img_copy_len = pimghdr->img_real_size;
		} else {
			img_copy_len = img_size;
		}

		temp_img = gnss_load_firmware_data(
			SEC_IMAGE_HDR_SIZE + img_copy_len);
		if (!temp_img) {
			pr_info("%s read gnss buff is NULL\n", __func__);
			vfree(temp);
			return -1;
		}
		memcpy(buffer, (temp_img + SEC_IMAGE_HDR_SIZE), img_copy_len);
		vfree(temp_img);
	}

write_gnss_img:
	len = 0;
	while (len < img_size) {
		trans_size = (img_size - len) > PACKET_SIZE ?
				PACKET_SIZE : (img_size - len);
		memcpy(marlin_dev->write_buffer, buffer + len, trans_size);
		err = sprdwcn_bus_direct_write(get_gnss_cp_start_addr() + len,
			marlin_dev->write_buffer, trans_size);
		if (err < 0) {
			pr_err("gnss dt write %s error:%d\n", __func__, err);
			vfree(temp);
			return -1;
		}
		len += PACKET_SIZE;
	}
	vfree(temp);
	pr_info("%s gnss download firmware finish\n", __func__);

	return 0;
}

static int gnss_download_firmware(void)
{
	const struct firmware *firmware;
	char *buf;
	int err;
	int i, len, count, trans_size;
	char *tx_img_ptr = NULL;
	u32 sec_img_magic, tx_img_size;
	struct sys_img_header *pimghdr = NULL;

	if (marlin_dev->is_gnss_in_sysfs) {
		err = gnss_download_from_partition();
		return err;
	}

	pr_info("%s start from /system/etc/firmware/\n", __func__);
	buf = marlin_dev->write_buffer;

#ifdef FIRMWARE_PARTITION_DEBUG_EN
	err = request_firmware_direct(&firmware, "gnssmodem.bin", NULL);
#else
	err = request_firmware(&firmware, "gnssmodem.bin", NULL);
#endif
	if (err < 0) {
		pr_err("%s no find gnssmodem.bin err:%d(ignore)\n",
			__func__, err);
		marlin_dev->is_gnss_in_sysfs = true;
		err = gnss_download_from_partition();

		return err;
	}

	pimghdr = (struct sys_img_header *)(firmware->data);
	sec_img_magic = pimghdr->magic_num;
	if (sec_img_magic == SEC_IMAGE_MAGIC) {
		if (pimghdr->img_real_size == 0 ||
			(pimghdr->img_signed_size <=
			(SEC_IMAGE_HDR_SIZE + pimghdr->img_real_size)) ||
			pimghdr->img_signed_size > firmware->size) {
			release_firmware(firmware);
			pr_err("%s check signed img fail.\n", __func__);
			return -1;
		}

		tx_img_size = pimghdr->img_real_size;
		tx_img_ptr = vmalloc(tx_img_size);
		if (!tx_img_ptr) {
			release_firmware(firmware);
			pr_err("%s no memory for image\n", __func__);
			return -1;
		}
		memcpy(tx_img_ptr, (firmware->data + SEC_IMAGE_HDR_SIZE),
			tx_img_size);
	} else {
		tx_img_size = firmware->size;
		tx_img_ptr = (char *)firmware->data;
	}

	count = (tx_img_size + PACKET_SIZE - 1) / PACKET_SIZE;
	len = 0;
	for (i = 0; i < count; i++) {
		trans_size = (tx_img_size - len) > PACKET_SIZE ?
				PACKET_SIZE : (tx_img_size - len);
		memcpy(buf, tx_img_ptr + len, trans_size);
		err = sprdwcn_bus_direct_write(get_gnss_cp_start_addr() + len, buf,
				trans_size);
		if (err < 0) {
			pr_err("gnss dt write %s error:%d\n", __func__, err);
			release_firmware(firmware);

			return err;
		}
		len += trans_size;
	}
	release_firmware(firmware);
	pr_info("%s successfully through request_firmware!\n", __func__);

	if (sec_img_magic == SEC_IMAGE_MAGIC && tx_img_ptr)
		vfree(tx_img_ptr);

	return 0;
}

/* BT WIFI FM download */
static int btwifi_download_firmware(void)
{
	const struct firmware *firmware;
	char *buf;
	int err;
	int i, len, count, trans_size;
	char *tx_img_ptr = NULL;
	u32 sec_img_magic, tx_img_size;
	struct sys_img_header *pimghdr = NULL;

	if (marlin_dev->is_btwf_in_sysfs) {
		err = marlin_download_from_partition();
		return err;
	}

	pr_info("marlin %s from /system/etc/firmware/ start!\n", __func__);
	buf = marlin_dev->write_buffer;

#ifdef FIRMWARE_PARTITION_DEBUG_EN
	err = request_firmware_direct(&firmware, "wcnmodem.bin", NULL);
#else
	err = request_firmware(&firmware, "wcnmodem.bin", NULL);
#endif
	if (err < 0) {
		pr_err("no find wcnmodem.bin errno:(%d)(ignore!!)\n", err);
		marlin_dev->is_btwf_in_sysfs = true;
		err = marlin_download_from_partition();

		return err;
	}

	pimghdr = (struct sys_img_header *)(firmware->data);
	sec_img_magic = pimghdr->magic_num;
	if (sec_img_magic == SEC_IMAGE_MAGIC) {
		if (pimghdr->img_real_size == 0 ||
			(pimghdr->img_signed_size <=
			(SEC_IMAGE_HDR_SIZE + pimghdr->img_real_size)) ||
			pimghdr->img_signed_size > firmware->size) {
			release_firmware(firmware);
			pr_err("%s check signed img fail.\n", __func__);
			return -1;
		}

		tx_img_size = pimghdr->img_real_size;
		tx_img_ptr = vmalloc(tx_img_size);
		if (!tx_img_ptr) {
			release_firmware(firmware);
			pr_err("%s no memory for image\n", __func__);
			return -1;
		}
		memcpy(tx_img_ptr, (firmware->data + SEC_IMAGE_HDR_SIZE),
			tx_img_size);
	} else {
		tx_img_size = firmware->size;
		tx_img_ptr = (char *)firmware->data;
	}

	count = (tx_img_size + PACKET_SIZE - 1) / PACKET_SIZE;
	len = 0;

	for (i = 0; i < count; i++) {
		trans_size = (tx_img_size - len) > PACKET_SIZE ?
				PACKET_SIZE : (tx_img_size - len);
		memcpy(buf, tx_img_ptr + len, trans_size);
		pr_info("download count=%d,len =%d,trans_size=%d\n", count,
			 len, trans_size);
		err = sprdwcn_bus_direct_write(get_cp_start_addr() + len,
					       buf, trans_size);
		if (err < 0) {
			pr_err("marlin dt write %s error:%d\n", __func__, err);
			release_firmware(firmware);
			return err;
		}
		len += trans_size;
	}

	release_firmware(firmware);
	pr_info("marlin %s successfully!\n", __func__);

	if (sec_img_magic == SEC_IMAGE_MAGIC && tx_img_ptr)
		vfree(tx_img_ptr);

	return 0;
}

static int wcn_get_syscon_regmap(void)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");
		if (!regmap_np) {
			pr_err("unable to get syscon node\n");
			return -ENODEV;
		}
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		pr_err("unable to get syscon platform device\n");
		return -ENODEV;
	}

	marlin_dev->syscon_pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!marlin_dev->syscon_pmic)
		pr_err("unable to get pmic regmap device\n");

	of_node_put(regmap_np);
	put_device(&regmap_pdev->dev);

	return 0;
}

static void wcn_get_pmic_config(struct device_node *np)
{
	int ret;
	struct wcn_pmic_config *pmic;

	if (wcn_get_syscon_regmap())
		return;

	pmic = &marlin_dev->avdd12_parent_bound_chip;
	strcpy(pmic->name, "avdd12-parent-bound-chip");
	ret = of_property_read_u32_array(np, pmic->name,
					 (u32 *)pmic->config,
					 WCN_BOUND_CONFIG_NUM);
	pmic->enable = !ret;
	pr_info("vddgen1-bound-chip config enable:%d\n", pmic->enable);

	pmic = &marlin_dev->avdd12_bound_wbreq;
	strcpy(pmic->name, "avdd12-bound-wbreq");
	ret = of_property_read_u32_array(np, pmic->name,
					 (u32 *)pmic->config,
					 WCN_BOUND_CONFIG_NUM);
	pmic->enable = !ret;
	pr_info("avdd12-bound-wbreq config status:%d\n", pmic->enable);

	pmic = &marlin_dev->avdd33_bound_wbreq;
	strcpy(pmic->name, "avdd33-bound-wbreq");
	ret = of_property_read_u32_array(np, pmic->name,
					 (u32 *)pmic->config,
					 WCN_BOUND_CONFIG_NUM);
	pmic->enable = !ret;
	pr_info("avdd33-bound-wbreq config status:%d\n", pmic->enable);
}

static int wcn_pmic_do_bound(struct wcn_pmic_config *pmic, bool bound)
{
	int ret;
	u32 *chip;

	if (!marlin_dev->syscon_pmic || !pmic->enable)
		return -1;

	chip = pmic->config;

	if (bound) {
		pr_info("%s bound\n", pmic->name);
		ret = regmap_update_bits(marlin_dev->syscon_pmic,
					 chip[0], chip[1], chip[3]);
		if (ret)
			pr_err("%s bound:%d\n", pmic->name, ret);
	} else {
		pr_info("%s unbound\n", pmic->name);
		ret = regmap_update_bits(marlin_dev->syscon_pmic,
					 chip[0], chip[1], chip[2]);
		if (ret)
			pr_err("%s unbound:%d\n", pmic->name, ret);
	}
	usleep_range_state(1000, 2000, TASK_UNINTERRUPTIBLE);

	return 0;
}

static inline int wcn_avdd12_parent_bound_chip(bool enable)
{
	if (marlin_dev->need_to_check_ufs) {
		pr_info("is_boot_ufs=%d enable=%d", is_boot_ufs, enable);
		if (!is_boot_ufs) {
			pr_info("is emmc\n");
			return wcn_pmic_do_bound(&marlin_dev->avdd12_parent_bound_chip, enable);
		}
		return 0;
	} else {
		return wcn_pmic_do_bound(&marlin_dev->avdd12_parent_bound_chip, enable);
	}
}

static inline int wcn_avdd12_bound_xtl(bool enable)
{
	return wcn_pmic_do_bound(&marlin_dev->avdd12_bound_wbreq, enable);
}

static int get_boot_device(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	is_boot_ufs = 0;
	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret) {
		pr_err("Can not get bootargs \r\n");
		return ret;
	}

	if (marlin_dev->need_to_check_ufs) {
		if (strstr(cmd_line, "ufs")) {
			is_boot_ufs = 1;
			pr_info("boot from ufs\n");
			return 0;
		}
		pr_info("boot from emmc\n");
		return 0;
	}
	return 0;
}

/* wifipa bound XTLEN3, gnss not need wifipa bound */
static inline int wcn_wifipa_bound_xtl(bool enable)
{
	return wcn_pmic_do_bound(&marlin_dev->avdd33_bound_wbreq, enable);
}

static enum wcn_clock_type crystal_check(const char *cmd_line)
{
	char *parse_cmdline = NULL;

	parse_cmdline = strstr(cmd_line, WCN_CLKTYPE1);

	if (parse_cmdline) {
		pr_info("crystal %s\n", parse_cmdline);
		parse_cmdline += strlen(WCN_CLKTYPE1);
		switch (*parse_cmdline) {
		case '2':/*TSX*/
		case '7':/*M52_TSX*/
			return WCN_CLOCK_TYPE_TSX;
		case '3':/*TCXO*/
		case '4':/*M26_TCXO*/
		case '5':/*M52_TCXO*/
			return WCN_CLOCK_TYPE_TCXO;
		default:
			return WCN_CLOCK_TYPE_TSX;
		}

	}

	return WCN_CLOCK_TYPE_UNKNOWN;
}

static int marlin_parse_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cmdline_node;
	struct regmap *pmu_apb_gpr;
	int ret, rc;
	char *buf, *parse_cmdline;
	const char *cmd_line;
	struct wcn_clock_info *clk;
	struct resource res;

	if (!marlin_dev)
		return -1;

	wcn_get_pmic_config(np);

	ret = of_property_read_u32(np, "sprd,gnss-cp-start-addr",
				   &GNSS_CP_START_ADDR);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-cp-start-addr\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-firmware-max-size",
				   &GNSS_FIRMWARE_MAX_SIZE);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-firmware-max-size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-dump-packet-size",
				   &GNSS_DUMP_PACKET_SIZE);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-dump-packet-size\n");
		//return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,gnss-dump-reg-number",
				   &GNSS_DUMP_REG_NUMBER);
	if (ret) {
		WCN_ERR("failed to read sprd,gnss-dump-reg-number\n");
		return -EINVAL;
	}

	marlin_dev->wakeup_ap = of_get_named_gpio(np, "m2-wakeup-ap-gpios", 0);
	if (!gpio_is_valid(marlin_dev->wakeup_ap))
		pr_info("can not get wakeup gpio\n");

	marlin_dev->reset = of_get_named_gpio(np, "reset-gpios", 0);
	if (!gpio_is_valid(marlin_dev->reset))
		return -EINVAL;

	marlin_dev->chip_en = of_get_named_gpio(np, "enable-gpios", 0);
	if (!gpio_is_valid(marlin_dev->chip_en))
		return -EINVAL;

	marlin_dev->int_ap = of_get_named_gpio(np,
			"m2-to-ap-irq-gpios", 0);
	if (!gpio_is_valid(marlin_dev->int_ap)) {
		pr_err("Get int irq error!\n");
		return -EINVAL;
	}

	clk = &marlin_dev->clk_xtal_26m;
	clk->gpio = of_get_named_gpio(np, "xtal-26m-clk-type-gpio", 0);
	if (!gpio_is_valid(clk->gpio))
		pr_info("xtal-26m-clk gpio not config\n");

	marlin_dev->dvdd12 = devm_regulator_get(&pdev->dev, "dvdd12");
	if (IS_ERR(marlin_dev->dvdd12)) {
		pr_info("Get regulator of dvdd12 error!\n");
		pr_info("Maybe share the power with mem\n");
	}

	if (of_property_read_bool(np, "bound-avdd12")) {
		pr_info("forbid avdd12 power ctrl\n");
		marlin_dev->bound_avdd12 = true;
	} else {
		pr_info("do avdd12 power ctrl\n");
		marlin_dev->bound_avdd12 = false;
	}

	marlin_dev->avdd12 = devm_regulator_get(&pdev->dev, "avdd12");
	if (IS_ERR(marlin_dev->avdd12)) {
		pr_err("avdd12 err =%ld\n", PTR_ERR(marlin_dev->avdd12));
		if (PTR_ERR(marlin_dev->avdd12) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		pr_err("Get regulator of avdd12 error!\n");
	}

	marlin_dev->avdd33 = devm_regulator_get(&pdev->dev, "avdd33");
	if (IS_ERR(marlin_dev->avdd33)) {
		if (PTR_ERR(marlin_dev->avdd33) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		pr_err("Get regulator of avdd33 error!\n");
	}

	marlin_dev->dcxo18 = devm_regulator_get(&pdev->dev, "dcxo18");
	if (IS_ERR(marlin_dev->dcxo18)) {
		if (PTR_ERR(marlin_dev->dcxo18) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		pr_err("Get regulator of dcxo18 error!\n");
	}

	if (of_property_read_bool(np, "bound-dcxo18")) {
		pr_info("forbid dcxo18 power ctrl\n");
		marlin_dev->bound_dcxo18 = true;
	} else {
		pr_info("do dcxo18 power ctrl\n");
		marlin_dev->bound_dcxo18 = false;
	}

	marlin_dev->clk_32k = devm_clk_get(&pdev->dev, "clk_32k");
	if (IS_ERR(marlin_dev->clk_32k)) {
		pr_err("can't get wcn clock dts config: clk_32k\n");
		return -1;
	}

	marlin_dev->clk_parent = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(marlin_dev->clk_parent)) {
		pr_err("can't get wcn clock dts config: source\n");
		return -1;
	}
	clk_set_parent(marlin_dev->clk_32k, marlin_dev->clk_parent);
	clk_set_rate(marlin_dev->clk_32k, 32768);

	marlin_dev->clk_enable = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(marlin_dev->clk_enable)) {
		pr_err("can't get wcn clock dts config: enable\n");
		return -1;
	}

	ret = gpio_request(marlin_dev->reset, "reset");
	if (ret)
		pr_err("gpio reset request err: %d\n", marlin_dev->reset);

	ret = gpio_request(marlin_dev->chip_en, "chip_en");
	if (ret)
		pr_err("gpio_rst request err: %d\n", marlin_dev->chip_en);

	ret = gpio_request(marlin_dev->int_ap, "int_ap");
	if (ret)
		pr_err("gpio_rst request err: %d\n",
				marlin_dev->int_ap);

	if (gpio_is_valid(clk->gpio)) {
		ret = gpio_request(clk->gpio, "wcn_xtal_26m_type");
		if (ret)
			pr_err("xtal 26m gpio request err: %d\n", ret);
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_info("No BTWF mem.\n");
	} else {
		marlin_dev->base_addr_btwf = res.start;
		marlin_dev->maxsz_btwf = resource_size(&res);
		pr_info("cp base = 0x%llx, size = 0x%x\n",
			 (u64)marlin_dev->base_addr_btwf,
			 marlin_dev->maxsz_btwf);
	}

	ret = of_address_to_resource(np, 1, &res);
	if (ret) {
		pr_info("No GNSS mem.\n");
	} else {
		marlin_dev->base_addr_gnss = res.start;
		marlin_dev->maxsz_gnss = resource_size(&res);
		pr_info("cp base = 0x%llx, size = 0x%x\n",
			 (u64)marlin_dev->base_addr_gnss,
			 marlin_dev->maxsz_gnss);
	}

	/* check emmc or ufs */
	ret = of_property_read_string(np, "sprd,check-emmc-ufs",
		(const char **)&marlin_dev->emmc_ufs);
	if (!ret) {
		marlin_dev->need_to_check_ufs = true;
		pr_info("need to check emmc or ufs\n");
	} else {
		marlin_dev->need_to_check_ufs = false;
		pr_info("Don't need to check emmc or ufs\n");
	}

	pr_info("BTWF_FIRMWARE_PATH len=%ld\n",
		(long)strlen(BTWF_FIRMWARE_PATH));
	ret = of_property_read_string(np, "sprd,btwf-file-name",
				      (const char **)&marlin_dev->btwf_path);
	if (!ret) {
		pr_info("btwf firmware name:%s\n", marlin_dev->btwf_path);
		strcpy(BTWF_FIRMWARE_PATH, marlin_dev->btwf_path);
		pr_info("BTWG path is %s\n", BTWF_FIRMWARE_PATH);
	}

	pr_info("BTWF_FIRMWARE_PATH2 len=%ld\n",
		(long)strlen(BTWF_FIRMWARE_PATH));

	ret = of_property_read_string(np, "sprd,gnss-file-name",
				      (const char **)&marlin_dev->gnss_path);
	if (!ret) {
		pr_info("gnss firmware name:%s\n", marlin_dev->gnss_path);
		strcpy(GNSS_FIRMWARE_PATH, marlin_dev->gnss_path);
	}

	cmdline_node = of_find_node_by_path("/chosen");
	if (cmdline_node)
		rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	if (slot_suffix) {
		if (strcmp(slot_suffix, "_a") == 0) {
			strcat(BTWF_FIRMWARE_PATH, "_a");
			//strcat(GNSS_FIRMWARE_PATH, "_a");
		} else if (strcmp(slot_suffix, "_b") == 0) {
			strcat(BTWF_FIRMWARE_PATH, "_b");
			//strcat(GNSS_FIRMWARE_PATH, "_b");
		}
	} else {
		if (!rc) {
			parse_cmdline = strstr(cmd_line, SUFFIX);
			if (parse_cmdline) {
				pr_debug("fstab: %s\n", parse_cmdline);
				if (!strncmp(parse_cmdline + strlen(SUFFIX), "_a", 2)) {
					strcat(BTWF_FIRMWARE_PATH, "_a");
					//strcat(GNSS_FIRMWARE_PATH, "_a");
				} else if (!strncmp(parse_cmdline + strlen(SUFFIX), "_b", 2)) {
					strcat(BTWF_FIRMWARE_PATH, "_b");
					//strcat(GNSS_FIRMWARE_PATH, "_b");
				}
			} else {
				parse_cmdline = strstr(cmd_line, SUFFIXS);
				if (parse_cmdline) {
					pr_debug("fstab_sprdboot: %s\n", parse_cmdline);
					if (!strncmp(parse_cmdline + strlen(SUFFIXS), "_a", 2)) {
						strcat(BTWF_FIRMWARE_PATH, "_a");
						//strcat(GNSS_FIRMWARE_PATH, "_a");
					} else if (!strncmp(parse_cmdline + strlen(SUFFIXS), "_b", 2)) {
						strcat(BTWF_FIRMWARE_PATH, "_b");
						//strcat(GNSS_FIRMWARE_PATH, "_b");
					}
				}
			}
		}
	}
	pr_info("BTWG path:%s\n GNSS path:%s\n",
		BTWF_FIRMWARE_PATH, GNSS_FIRMWARE_PATH);

	/* xtal-26m-clk-type has priority over than xtal-26m-clk-type-gpio */
	ret = of_property_read_string(np, "xtal-26m-clk-type",
				      (const char **)&buf);
	if (!ret) {
		pr_info("force config xtal 26m clk %s\n", buf);
		if (!strncmp(buf, "TCXO", 4))
			clk->type = WCN_CLOCK_TYPE_TCXO;
		else if (!strncmp(buf, "TSX", 3))
			clk->type = WCN_CLOCK_TYPE_TSX;
		else
			pr_err("force config xtal 26m clk %s err!\n", buf);
	} else {
		if (clktype == 0) {
			clk->type = WCN_CLOCK_TYPE_TCXO;
		} else if (clktype == 1) {
			clk->type = WCN_CLOCK_TYPE_TSX;
		} else {
			if (!rc) {
				parse_cmdline = strstr(cmd_line, WCN_CLKTYPE);
				if (parse_cmdline) {
					pr_debug("marlin.clk %s\n", parse_cmdline);
					if (!strncmp(parse_cmdline + strlen(WCN_CLKTYPE), "0", 1))
						clk->type = WCN_CLOCK_TYPE_TCXO;
					else if (!strncmp
						 (parse_cmdline + strlen(WCN_CLKTYPE), "1", 1))
						clk->type = crystal_check(cmd_line);
				}
			}
		}
		if (!clk->type)
			clk->type = WCN_CLOCK_TYPE_UNKNOWN;
		pr_info("cmd config clk %d\n", clk->type);
	}

	if (of_property_read_bool(np, "keep-power-on")) {
		pr_info("wcn config keep power on\n");
		marlin_dev->keep_power_on = true;
	}

	if (of_property_read_bool(np, "wait-ge2")) {
		pr_info("wait-ge2 need wait gps ready\n");
		marlin_dev->wait_ge2 = true;
	}

	pmu_apb_gpr = syscon_regmap_lookup_by_phandle(np,
				"sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb_gpr)) {
		pr_err("%s:failed to find pmu_apb_gpr(26M)(ignore)\n",
				__func__);
		return -EINVAL;
	}
	ret = regmap_read(pmu_apb_gpr, REG_PMU_APB_XTL_WAIT_CNT0,
					&clk_wait_val);
	pr_info("marlin2 clk_wait value is 0x%x\n", clk_wait_val);

	ret = of_property_read_u32(np, "sprd,reg-m2-apb-xtl-wait-addr",
			&marlin2_clk_wait_reg);
	if (ret) {
		pr_err("Did not find reg-m2-apb-xtl-wait-addr\n");
		return -EINVAL;
	}
	pr_info("marlin2 clk reg is 0x%x\n", marlin2_clk_wait_reg);

	return 0;
}

static int marlin_gpio_free(struct platform_device *pdev)
{
	if (!marlin_dev)
		return -1;

	gpio_free(marlin_dev->reset);
	gpio_free(marlin_dev->chip_en);
	gpio_free(marlin_dev->int_ap);
	if (!gpio_is_valid(marlin_dev->clk_xtal_26m.gpio))
		gpio_free(marlin_dev->clk_xtal_26m.gpio);

	return 0;
}

static int marlin_clk_enable(bool enable)
{
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(marlin_dev->clk_32k);
		ret = clk_prepare_enable(marlin_dev->clk_enable);
		pr_info("marlin %s successfully!\n", __func__);
	} else {
		clk_disable_unprepare(marlin_dev->clk_enable);
		clk_disable_unprepare(marlin_dev->clk_32k);
	}

	return ret;
}

static int marlin_avdd18_dcxo_enable(bool enable)
{
	int ret = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (!marlin_dev->dcxo18)
		return 0;

	if (enable) {
		if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
			if (!marlin_dev->bound_dcxo18 &&
			    regulator_is_enabled(marlin_dev->dcxo18)) {
				pr_info("avdd18_dcxo 1v8 have enable\n");
				return 0;
			}
		}

		pr_info("avdd18_dcxo set 1v8\n");
		regulator_set_voltage(marlin_dev->dcxo18, 1800000, 1800000);
		if (!marlin_dev->bound_dcxo18) {
			pr_info("avdd18_dcxo power enable\n");
			ret = regulator_enable(marlin_dev->dcxo18);
			if (ret)
				pr_err("fail to enable avdd18_dcxo\n");
		}
	} else {
		if (!marlin_dev->bound_dcxo18 &&
		    regulator_is_enabled(marlin_dev->dcxo18)) {
			pr_info("avdd18_dcxo power disable\n");
			ret = regulator_disable(marlin_dev->dcxo18);
			if (ret)
				pr_err("fail to disable avdd18_dcxo\n");
		}
	}

	return ret;
}

static int marlin_digital_power_enable(bool enable)
{
	int ret = 0;

	pr_info("%s D1v2 %d\n", __func__, enable);
	if (marlin_dev->dvdd12 == NULL)
		return 0;

	if (enable) {
		regulator_set_voltage(marlin_dev->dvdd12, 200000, 1200000);
		ret = regulator_enable(marlin_dev->dvdd12);
	} else {
		if (regulator_is_enabled(marlin_dev->dvdd12))
			ret = regulator_disable(marlin_dev->dvdd12);
	}

	return ret;
}

static int marlin_analog_power_enable(bool enable)
{
	int ret = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (marlin_dev->avdd12 != NULL) {
		usleep_range_state(4000, 5000, TASK_UNINTERRUPTIBLE);
		if (enable) {
			if (g_match_config && g_match_config->unisoc_wcn_pcie) {
				pr_info("%s avdd12 set 1.35v\n", __func__);
				regulator_set_voltage(marlin_dev->avdd12,
						      1350000, 1350000);
			} else {
				pr_info("%s avdd12 set 1.2v\n", __func__);
				regulator_set_voltage(marlin_dev->avdd12,
						      1200000, 1200000);
			}
			if (!marlin_dev->bound_avdd12) {
				pr_info("%s avdd12 power enable\n", __func__);
				ret = regulator_enable(marlin_dev->avdd12);
				if (ret)
					pr_err("fail to enalbe avdd12\n");
			}
		} else {
			if (!marlin_dev->bound_avdd12 &&
			    regulator_is_enabled(marlin_dev->avdd12)) {
				pr_info("%s avdd12 power disable\n", __func__);
				ret = regulator_disable(marlin_dev->avdd12);
				if (ret)
					pr_err("fail to disable avdd12\n");
			}
		}
	}

	return ret;
}

/*
 * hold cpu means cpu register is clear
 * different from reset pin gpio
 * reset gpio is all register is clear
 */
void marlin_hold_cpu(void)
{
	int ret = 0;
	unsigned int temp_reg_val = 0;

	ret = sprdwcn_bus_reg_read(get_cp_reset_reg(), &temp_reg_val, 4);
	if (ret < 0) {
		pr_err("%s read reset reg error:%d\n", __func__, ret);
		return;
	}
	pr_info("%s reset reg val:0x%x\n", __func__, temp_reg_val);
	temp_reg_val |= 1;
	ret = sprdwcn_bus_reg_write(get_cp_reset_reg(), &temp_reg_val, 4);
	if (ret < 0) {
		pr_err("%s write reset reg error:%d\n", __func__, ret);
		return;
	}
}

void marlin_read_cali_data(void)
{
	int err;

	pr_info("marlin sync entry is_calibrated:%d\n",
		wifi_data.cali_data.cali_config.is_calibrated);

	if (!wifi_data.cali_data.cali_config.is_calibrated) {
		memset(&wifi_data.cali_data, 0x0,
			sizeof(struct wifi_cali_t));
		err = sprdwcn_bus_reg_read(CALI_OFSET_REG,
			&wifi_data.cali_data, sizeof(struct wifi_cali_t));
		if (err < 0) {
			pr_err("marlin read cali data fail:%d\n", err);
			return;
		}
	}

	if ((marlin2_clk_wait_reg > 0) && (clk_wait_val > 0)) {
		sprdwcn_bus_reg_read(marlin2_clk_wait_reg,
					&cp_clk_wait_val, 4);
		pr_info("marlin2 cp_clk_wait_val is 0x%x\n", cp_clk_wait_val);
		clk_wait_val = ((clk_wait_val & 0xFF00) >> 8);
		cp_clk_wait_val =
			((cp_clk_wait_val & 0xFFFFFC00) | clk_wait_val);
		pr_info("marlin2 cp_clk_wait_val is modifyed 0x%x\n",
					cp_clk_wait_val);
		err = sprdwcn_bus_reg_write(marlin2_clk_wait_reg,
					       &cp_clk_wait_val, 4);
		if (err < 0)
			pr_err("marlin2 write 26M error:%d\n", err);
	}

	/* write this flag to notify cp that ap read calibration data */
	reg_val = 0xbbbbbbbb;
	err = sprdwcn_bus_reg_write(CALI_REG, &reg_val, 4);
	if (err < 0) {
		pr_err("marlin write cali finish error:%d\n", err);
		return;
	}

	sprdwcn_bus_runtime_get();

	complete(&marlin_dev->download_done);
}

static int marlin_write_cali_data(void)
{
	int i;
	int ret = 0, init_state = 0, cali_data_offset = 0;

	pr_info("tsx_dac_data:%d\n", marlin_dev->tsxcali.tsxdata.dac);
	cali_data_offset = (unsigned long)(&(marlin_dev->sync_f.tsx_dac_data))
		- (unsigned long)(&(marlin_dev->sync_f));
	pr_info("cali_data_offset:0x%x\n", cali_data_offset);

	for (i = 0; i <= 65; i++) {
		ret = sprdwcn_bus_reg_read(get_sync_addr(), &init_state, 4);
		if (ret < 0) {
			pr_err("%s marlin3 read get_sync_addr error:%d\n",
				__func__, ret);
			return ret;
		}

		if (init_state != SYNC_CALI_WAITING)
			usleep_range_state(3000, 5000, TASK_UNINTERRUPTIBLE);
		/* wait cp in the state of waiting cali data */
		else {
			/* write cali data to cp */
			marlin_dev->sync_f.tsx_dac_data =
					marlin_dev->tsxcali.tsxdata.dac;
			ret = sprdwcn_bus_direct_write(get_sync_addr() +
					cali_data_offset,
					&(marlin_dev->sync_f.tsx_dac_data), 2);
			if (ret < 0) {
				pr_err("write cali data error:%d\n", ret);
				return ret;
			}

			/* tell cp2 can handle cali data */
			init_state = SYNC_CALI_WRITE_DONE;
			ret = sprdwcn_bus_reg_write(get_sync_addr(), &init_state, 4);
			if (ret < 0) {
				pr_err("write cali_done flag error:%d\n", ret);
				return ret;
			}

			pr_info("%s finish\n", __func__);
			return ret;
		}
	}

	pr_err("%s sync init_state:0x%x\n", __func__, init_state);

	return -1;
}

enum wcn_clock_type integ_wcn_get_xtal_26m_clk_type(void);
enum wcn_clock_type wcn_get_xtal_26m_clk_type(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return integ_wcn_get_xtal_26m_clk_type();
	else
		return marlin_dev->clk_xtal_26m.type;
}
EXPORT_SYMBOL_GPL(wcn_get_xtal_26m_clk_type);

enum wcn_clock_mode wcn_get_xtal_26m_clk_mode(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return integ_wcn_get_xtal_26m_clk_mode();
	else
		return marlin_dev->clk_xtal_26m.mode;
}
EXPORT_SYMBOL_GPL(wcn_get_xtal_26m_clk_mode);

static int spi_read_rf_reg(unsigned int addr, unsigned int *data)
{
	unsigned int reg_data = 0;
	int ret;

	reg_data = ((addr & 0x7fff) << 16) | SPI_BIT31;
	ret = sprdwcn_bus_reg_write(SPI_BASE_ADDR, &reg_data, 4);
	if (ret < 0) {
		pr_err("write SPI RF reg error:%d\n", ret);
		return ret;
	}

	usleep_range_state(4000, 6000, TASK_UNINTERRUPTIBLE);

	ret = sprdwcn_bus_reg_read(SPI_BASE_ADDR, &reg_data, 4);
	if (ret < 0) {
		pr_err("read SPI RF reg error:%d\n", ret);
		return ret;
	}
	*data = reg_data & 0xffff;

	return 0;
}

static void wcn_check_xtal_26m_clk(void)
{
	int ret = 0;
	unsigned int temp_val;
	struct wcn_clock_info *clk;

	clk = &marlin_dev->clk_xtal_26m;
	if (likely(clk->type != WCN_CLOCK_TYPE_UNKNOWN) &&
	    likely(clk->mode != WCN_CLOCK_MODE_UNKNOWN)) {
		pr_info("xtal 26m clk type:%s mode:%s\n",
			 (clk->type == WCN_CLOCK_TYPE_TSX) ? "TSX" : "TCXO",
			 (clk->mode == WCN_CLOCK_MODE_XO) ? "XO" : "BUFFER");
		return;
	}

	if (clk->type == WCN_CLOCK_TYPE_UNKNOWN) {
		if (gpio_is_valid(clk->gpio)) {
			gpio_direction_input(clk->gpio);
			ret = gpio_get_value(clk->gpio);
			clk->type = ret ? WCN_CLOCK_TYPE_TSX :
				    WCN_CLOCK_TYPE_TCXO;
			pr_info("xtal gpio clk type:%d %d\n",
				 clk->type, ret);
		} else {
			pr_err("xtal_26m clk type erro!\n");
		}
	}

	if (clk->mode == WCN_CLOCK_MODE_UNKNOWN) {
		ret = spi_read_rf_reg(AD_DCXO_BONDING_OPT, &temp_val);
		if (ret < 0) {
			pr_err("read AD_DCXO_BONDING_OPT error:%d\n", ret);
			return;
		}
		pr_info("read AD_DCXO_BONDING_OPT val:0x%x\n", temp_val);
		if (temp_val & get_wcn_bound_xo_mode()) {
			pr_info("xtal_26m clock XO mode\n");
			clk->mode = WCN_CLOCK_MODE_XO;
		} else {
			pr_info("xtal_26m clock Buffer mode\n");
			clk->mode = WCN_CLOCK_MODE_BUFFER;
		}
	}
}

static int check_cp_clock_mode(void)
{
	struct wcn_clock_info *clk;

	pr_info("%s\n", __func__);

	clk = &marlin_dev->clk_xtal_26m;
	if (clk->mode == WCN_CLOCK_MODE_BUFFER) {
		pr_info("xtal_26m clock use BUFFER mode\n");
		marlin_avdd18_dcxo_enable(false);
		return 0;
	} else if (clk->mode == WCN_CLOCK_MODE_XO) {
		pr_info("xtal_26m clock use XO mode\n");
		return 0;
	}

	return -1;
}

/* release CPU */
static int marlin_start_run(void)
{
	int ret;
	unsigned int ss_val = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	pr_info("%s\n", __func__);

	marlin_tsx_cali_data_get();
	if (g_match_config && g_match_config->unisoc_wcn_slp) {
		sdio_pub_int_btwf_en0();
		/* after chip power on, reset sleep status */
		slp_mgr_reset();
	}

	ret = sprdwcn_bus_reg_read(get_cp_reset_reg(), &ss_val, 4);
	if (ret < 0) {
		pr_err("%s read reset reg error:%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s read reset reg val:0x%x\n", __func__, ss_val);

	ss_val &= (~0) - 1;
	ret = sprdwcn_bus_reg_write(get_cp_reset_reg(), &ss_val, 4);
	if (ret < 0) {
		pr_err("%s write reset reg error:%d\n", __func__, ret);
		return ret;
	}
	/* update the time at once */
	marlin_bootup_time_update();

	ret = sprdwcn_bus_reg_read(get_cp_reset_reg(), &ss_val, 4);
	if (ret < 0) {
		pr_err("%s read reset reg error:%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s after reset reg val:0x%x\n", __func__, ss_val);

	return ret;
}

/* return 0 is ready, other values is error */
static int check_cp_ready(void)
{
	int i, ret;

	for (i = 0; i <= 200; i++) {
		ret = sprdwcn_bus_direct_read(get_sync_addr(),
			&(marlin_dev->sync_f), sizeof(struct wcn_sync_info_t));
		if (ret < 0) {
			pr_err("%s marlin3 read get_sync_addr error:%d\n",
			       __func__, ret);
			return ret;
		}
		usleep_range_state(3000, 5000, TASK_UNINTERRUPTIBLE);
		if (marlin_dev->sync_f.init_status == SYNC_ALL_FINISHED) {
			flag_download_done = 1;
			return 0;
		}
	}

	pr_err("%s sync val:0x%x, prj_type val:0x%x\n",
	       __func__, marlin_dev->sync_f.init_status,
	       marlin_dev->sync_f.prj_type);

	return -1;
}

static int gnss_start_run(void)
{
	int ret = 0;
	unsigned int temp = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	pr_info("gnss start run enter ");
	if (g_match_config && g_match_config->unisoc_wcn_slp)
		sdio_pub_int_gnss_en0();

	ret = sprdwcn_bus_reg_read(get_gnss_cp_reset_reg(), &temp, 4);
	if (ret < 0) {
		pr_err("%s marlin3_gnss read reset reg error:%d\n",
			__func__, ret);
		return ret;
	}
	pr_info("%s reset reg val:0x%x\n", __func__, temp);
	temp &= (~0) - 1;
	ret = sprdwcn_bus_reg_write(get_gnss_cp_reset_reg(), &temp, 4);
	if (ret < 0) {
		pr_err("%s marlin3_gnss write reset reg error:%d\n",
				__func__, ret);
		return ret;
	}

	return ret;
}

static int marlin_reset(int val)
{
	if (gpio_is_valid(marlin_dev->reset)) {
		gpio_direction_output(marlin_dev->reset, 0);
		mdelay(RESET_DELAY);
		gpio_direction_output(marlin_dev->reset, 1);
	}

	return 0;
}

static int chip_reset_release(int val)
{

	if (!gpio_is_valid(marlin_dev->reset)) {
		pr_err("reset gpio error\n");
		return -1;
	}

	if (val)
		gpio_direction_output(marlin_dev->reset, 1);
	else
		gpio_direction_output(marlin_dev->reset, 0);

	return 0;
}

void marlin_chip_en(bool enable, bool reset)
{
	if (gpio_is_valid(marlin_dev->chip_en)) {
		if (reset) {
			gpio_direction_output(marlin_dev->chip_en, 0);
			pr_info("marlin chip en reset\n");
			msleep(100);
			gpio_direction_output(marlin_dev->chip_en, 1);
		} else if (enable) {
			gpio_direction_output(marlin_dev->chip_en, 0);
			mdelay(1);
			gpio_direction_output(marlin_dev->chip_en, 1);
			mdelay(1);
			pr_info("marlin chip en pull up\n");
		} else {
			gpio_direction_output(marlin_dev->chip_en, 0);
			pr_info("marlin chip en pull down\n");
		}
	}
}
EXPORT_SYMBOL_GPL(marlin_chip_en);

static int set_cp_mem_status(enum wcn_sub_sys subsys, int val)
{
	int ret;
	unsigned int temp_val;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && (g_match_config->unisoc_wcn_m3lite ||
		g_match_config->unisoc_wcn_pcie)) {
		return 0;
	}

	ret = sprdwcn_bus_reg_read(REG_WIFI_MEM_CFG1, &temp_val, 4);
	if (ret < 0) {
		pr_err("%s read wifimem_cfg1 error:%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s read btram poweron(bit22)val:0x%x\n", __func__, temp_val);

	if ((subsys == MARLIN_BLUETOOTH) && (val == 1)) {
		temp_val = temp_val & (~FORCE_SHUTDOWN_BTRAM);
		pr_info("wr btram poweron(bit22) val:0x%x\n", temp_val);
		ret = sprdwcn_bus_reg_write(REG_WIFI_MEM_CFG1, &temp_val, 4);
		if (ret < 0) {
			pr_err("write wifimem_cfg1 reg error:%d\n", ret);
			return ret;
		}
		return 0;
	} else if (test_bit(MARLIN_BLUETOOTH, &marlin_dev->power_state) &&
		   (subsys != MARLIN_BLUETOOTH))
		return 0;

	temp_val = temp_val | FORCE_SHUTDOWN_BTRAM;
	pr_info(" shut down btram(bit22) val:0x%x\n", temp_val);
	ret = sprdwcn_bus_reg_write(REG_WIFI_MEM_CFG1, &temp_val, 4);
	if (ret < 0) {
		pr_err("write wifimem_cfg1 reg error:%d\n", ret);
		return ret;
	}

	return ret;
}

int enable_spur_remove(void)
{
	int ret;
	unsigned int temp_val;

	temp_val = FM_ENABLE_SPUR_REMOVE_FREQ2_VALUE;
	ret = sprdwcn_bus_reg_write(FM_REG_SPUR_FEQ1_ADDR, &temp_val, 4);
	if (ret < 0) {
		pr_err("write FM_REG_SPUR reg error:%d\n", ret);
		return ret;
	}

	return 0;
}

int disable_spur_remove(void)
{
	int ret;
	unsigned int temp_val;

	temp_val = FM_DISABLE_SPUR_REMOVE_VALUE;
	ret = sprdwcn_bus_reg_write(FM_REG_SPUR_FEQ1_ADDR, &temp_val, 4);
	if (ret < 0) {
		pr_err("write disable FM_REG_SPUR reg error:%d\n", ret);
		return ret;
	}

	return 0;
}

static void set_fm_supe_freq(enum wcn_sub_sys subsys,
			     int val, unsigned long sub_state)
{
	switch (subsys) {
	case MARLIN_FM:
		if (test_bit(MARLIN_GNSS, &sub_state) && (val == 1))
			enable_spur_remove();
		else
			disable_spur_remove();
		break;
	case MARLIN_GNSS:
		if (test_bit(MARLIN_FM, &sub_state) && (val == 1))
			enable_spur_remove();
		else
			disable_spur_remove();
		break;
	default:
		break;
	}
}

/*
 * MARLIN_GNSS no need loopcheck action
 * MARLIN_AUTO no need loopcheck action
 */
static void power_state_notify_or_not(enum wcn_sub_sys subsys, int poweron)
{
	if (poweron == 1) {
		set_cp_mem_status(subsys, poweron);
		set_fm_supe_freq(subsys, poweron, marlin_dev->power_state);
	}

	if ((test_bit(MARLIN_BLUETOOTH, &marlin_dev->power_state) +
		test_bit(MARLIN_FM, &marlin_dev->power_state) +
		test_bit(MARLIN_WIFI, &marlin_dev->power_state) +
		test_bit(MARLIN_MDBG, &marlin_dev->power_state)) == 1) {
		pr_info("only one module open, need to notify loopcheck\n");
		start_loopcheck();
		marlin_dev->loopcheck_status_change = 1;
		wakeup_loopcheck_int();
	}

	if (((marlin_dev->power_state) & MARLIN_MASK) == 0) {

		pr_info("marlin close, need to notify loopcheck\n");
		stop_loopcheck();
		marlin_dev->loopcheck_status_change = 1;
		wakeup_loopcheck_int();
	}
}

void marlin_scan_finish(void)
{
	pr_info("%s!\n", __func__);
	complete(&marlin_dev->carddetect_done);
}
#if 0
static int find_firmware_path(void)
{
	int ret;
	int pre_len;

	if (strlen(BTWF_FIRMWARE_PATH) != 0)
		return 0;

	ret = parse_firmware_path(BTWF_FIRMWARE_PATH);
	if (ret != 0) {
		pr_err("can not find wcn partition\n");
		return ret;
	}
	pr_info("BTWF path is %s\n", BTWF_FIRMWARE_PATH);
	pre_len = strlen(BTWF_FIRMWARE_PATH) - strlen("wcnmodem");
	memcpy(GNSS_FIRMWARE_PATH,
		BTWF_FIRMWARE_PATH,
		strlen(BTWF_FIRMWARE_PATH));
	memcpy(&GNSS_FIRMWARE_PATH[pre_len], "gnssmodem",
		strlen("gnssmodem"));
	GNSS_FIRMWARE_PATH[pre_len + strlen("gnssmodem")] = '\0';
	pr_info("GNSS path is %s\n", GNSS_FIRMWARE_PATH);

	return 0;
}
#endif

char *integ_gnss_firmware_path_get(void);
char *gnss_firmware_path_get(void)
{
	char *fpath = GNSS_FIRMWARE_PATH;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return integ_gnss_firmware_path_get();

	pr_info("%s %s\n", __func__, fpath);
	return fpath;
}
EXPORT_SYMBOL_GPL(gnss_firmware_path_get);

static void pre_gnss_download_firmware(struct work_struct *work)
{
	static int cali_flag;
	int ret = -2;

	/* ./fstab.xxx is prevent for user space progress */
	//find_firmware_path();

	if (gnss_download_firmware() != 0) {
		pr_err("gnss download firmware fail\n");
		return;
	}

	ret = gnss_write_data();
	if (ret != 0) {
		pr_err("%s gnss_write_data err=%d\n", __func__, ret);
		return;
	}

	if (gnss_start_run() != 0)
		pr_err("gnss start run fail\n");

	if (cali_flag == 0) {
		pr_info("gnss start to backup calidata\n");
		ret = gnss_backup_data();
		if (ret == 0)
			cali_flag = 1;
		else
			pr_err("%s gnss_backup_data err=%d\n", __func__, ret);
	} else {
		pr_info("gnss wait boot start\n");
		ret = gnss_boot_wait();
		if (ret != 0)
			pr_err("%s gnss wait boot err=%d\n", __func__, ret);
		else
			pr_info("%s gnss wait boot end\n", __func__);
	}
	complete(&marlin_dev->gnss_download_done);
}

static void pre_btwifi_download_sdio(struct work_struct *work)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	flag_download_done = 0;
	if (btwifi_download_firmware() == 0 &&
		marlin_start_run() == 0) {
		if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
			check_cp_clock_mode();
			marlin_write_cali_data();
			mem_pd_save_bin();
		}
#ifdef WCN_RDCDBG
		/*rdc_debug.c*/
		wcn_rdc_debug_init();
#endif
		check_cp_ready();
		complete(&marlin_dev->download_done);
	}
	/* Runtime PM is useless, mainly to enable sdio_func1 and rx irq */
	sprdwcn_bus_runtime_get();

	if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
		pr_info("%s is not pcie\n", __func__);
		wcn_firmware_init();
	}
}

static int bus_scan_card(void)
{
	unsigned int card_detect_wait_ms;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_usb)
		card_detect_wait_ms = USB_CARD_DETECT_WAIT_MS;
	else
		card_detect_wait_ms = CARD_DETECT_WAIT_MS;

	init_completion(&marlin_dev->carddetect_done);
	sprdwcn_bus_rescan(marlin_dev);
	if (wait_for_completion_timeout(&marlin_dev->carddetect_done,
		msecs_to_jiffies(card_detect_wait_ms)) == 0) {
		pr_err("wait bus rescan card time out\n");
		return -1;
	}

	return 0;
}

static void wifipa_enable(int enable)
{
	int ret = -1;

	if (marlin_dev->avdd33) {
		pr_info("wifipa 3v3 %d\n", enable);
		usleep_range_state(4000, 5000, TASK_UNINTERRUPTIBLE);
		if (enable) {
			if (regulator_is_enabled(marlin_dev->avdd33))
				return;

			regulator_set_voltage(marlin_dev->avdd33,
					      3300000, 3300000);
			ret = regulator_enable(marlin_dev->avdd33);
			if (ret)
				pr_err("fail to enable wifipa\n");
		} else {
			if (regulator_is_enabled(marlin_dev->avdd33)) {
				ret = regulator_disable(marlin_dev->avdd33);
				if (ret)
					pr_err("fail to disable wifipa\n");
			}
		}
	}
}

static void set_wifipa_status(enum wcn_sub_sys subsys, int val)
{
	if (val == 1) {
		if (((subsys == MARLIN_BLUETOOTH) || (subsys == MARLIN_WIFI)) &&
		    ((marlin_dev->power_state & 0x5) == 0)) {
			wifipa_enable(1);
			wcn_wifipa_bound_xtl(true);
		}

		if (((subsys != MARLIN_BLUETOOTH) && (subsys != MARLIN_WIFI)) &&
		    ((marlin_dev->power_state & 0x5) == 0)) {
			wcn_wifipa_bound_xtl(false);
			wifipa_enable(0);
		}
	} else {
		if (((subsys == MARLIN_BLUETOOTH) &&
		     ((marlin_dev->power_state & 0x4) == 0)) ||
		    ((subsys == MARLIN_WIFI) &&
		     ((marlin_dev->power_state & 0x1) == 0))) {
			wcn_wifipa_bound_xtl(false);
			wifipa_enable(0);
		}
	}
}

/*
 * RST_N (LOW)
 * VDDIO -> DVDD12/11 ->CHIP_EN ->DVDD_CORE(inner)
 * ->(>=550uS) RST_N (HIGH)
 * ->(>=100uS) ADVV12
 * ->(>=10uS)  AVDD33
 */
static int chip_power_on(enum wcn_sub_sys subsys)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	wcn_avdd12_parent_bound_chip(false);
	marlin_avdd18_dcxo_enable(true);
	marlin_clk_enable(true);
	marlin_digital_power_enable(true);
	marlin_chip_en(true, false);
	usleep_range_state(4000, 5000, TASK_UNINTERRUPTIBLE);
	chip_reset_release(1);
	marlin_analog_power_enable(true);
	wcn_avdd12_bound_xtl(true);
	usleep_range_state(50, 60, TASK_UNINTERRUPTIBLE);
	wifipa_enable(1);
	wcn_wifipa_bound_xtl(true);
	if (bus_scan_card() < 0)
		return -1;
	loopcheck_ready_set();
	if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
		mem_pd_poweroff_deinit();
		sdio_pub_int_poweron(true);
		wcn_check_xtal_26m_clk();
	}

	return 0;
}

static int chip_power_off(enum wcn_sub_sys subsys)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie)
		sprdwcn_bus_remove_card(marlin_dev);

	marlin_dev->power_state = 0;
	flag_download_done = 0;
	wcn_avdd12_bound_xtl(false);
	wcn_wifipa_bound_xtl(false);
	wcn_avdd12_parent_bound_chip(true);
	wifipa_enable(0);
	marlin_avdd18_dcxo_enable(false);
	marlin_clk_enable(false);
	marlin_chip_en(false, false);
	marlin_digital_power_enable(false);
	marlin_analog_power_enable(false);
	chip_reset_release(0);
	marlin_dev->wifi_need_download_ini_flag = 0;
	if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
		mem_pd_poweroff_deinit();
		sprdwcn_bus_remove_card(marlin_dev);
	}
	loopcheck_ready_clear();
	if (g_match_config && !g_match_config->unisoc_wcn_pcie)
		sdio_pub_int_poweron(false);

	return 0;
}

void wcn_chip_power_on(void)
{
	chip_power_on(0);
}
EXPORT_SYMBOL_GPL(wcn_chip_power_on);

void wcn_chip_power_off(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated) {
		integ_wcn_chip_power_off();
		return;
	}

	mutex_lock(&marlin_dev->power_lock);
	sprdwcn_bus_runtime_put();
	chip_power_off(0);
	mutex_unlock(&marlin_dev->power_lock);
}
EXPORT_SYMBOL_GPL(wcn_chip_power_off);

static int gnss_powerdomain_open(void)
{
	/* add by this. */
	int ret = 0, retry_cnt = 0;
	unsigned int temp = 0;

	pr_info("%s\n", __func__);
	ret = sprdwcn_bus_reg_read(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		pr_err("%s read PD_GNSS_SS_AON_CFG4 err:%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	temp = temp & (~(FORCE_DEEP_SLEEP));
	pr_info("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	ret = sprdwcn_bus_reg_write(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		pr_err("write PD_GNSS_SS_AON_CFG4 err:%d\n", ret);
		return ret;
	}

	/* wait gnss sys power on finish */
	do {
		usleep_range_state(3000, 6000, TASK_UNINTERRUPTIBLE);

		ret = sprdwcn_bus_reg_read(CHIP_SLP_REG, &temp, 4);
		if (ret < 0) {
			pr_err("%s read CHIP_SLP_REG err:%d\n", __func__, ret);
			return ret;
		}

		pr_info("%s CHIP_SLP:0x%x,bit12,13 need 1\n", __func__, temp);
		retry_cnt++;
	} while ((!(temp & GNSS_SS_PWRON_FINISH)) &&
		 (!(temp & GNSS_PWR_FINISH)) && (retry_cnt < 3));

	return 0;
}

/*
 * CGM_GNSS_FAKE_CFG : 0x0: for 26M clock; 0x2: for 266M clock
 * gnss should select 26M clock before powerdomain close
 *
 * PD_GNSS_SS_AON_CFG4: 0x4041308->0x4041300 bit3=0 power on
 */
static int gnss_powerdomain_close(void)
{
	/* add by this. */
	int ret;
	int i = 0;
	unsigned int temp = 0;

	pr_info("%s\n", __func__);

	ret = sprdwcn_bus_reg_read(CGM_GNSS_FAKE_CFG, &temp, 4);
	if (ret < 0) {
		pr_err("%s read CGM_GNSS_FAKE_CFG error:%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s R_CGM_GNSS_FAKE_CFG:0x%x\n", __func__, temp);
	temp = temp & (~(CGM_GNSS_FAKE_SEL));
	ret = sprdwcn_bus_reg_write(CGM_GNSS_FAKE_CFG, &temp, 4);
	if (ret < 0) {
		pr_err("write CGM_GNSS_FAKE_CFG err:%d\n", ret);
		return ret;
	}
retry:
	ret = sprdwcn_bus_reg_read(CGM_GNSS_FAKE_CFG, &temp, 4);
	if (ret < 0) {
		pr_err("%s read CGM_GNSS_FAKE_CFG error:%d\n", __func__, ret);
		return ret;
	}
	i++;
	if ((temp & 0x3) && (i < 3)) {
		pr_err("FAKE_CFG:0x%x, GNSS select clk err\n", temp);
		goto retry;
	}

	ret = sprdwcn_bus_reg_read(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		pr_err("read PD_GNSS_SS_AON_CFG4 err:%d\n", ret);
		return ret;
	}
	pr_info("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	temp = (temp | FORCE_DEEP_SLEEP | PD_AUTO_EN) &
		(~(CHIP_DEEP_SLP_EN));
	pr_info("%s PD_GNSS_SS_AON_CFG4:0x%x\n", __func__, temp);
	ret = sprdwcn_bus_reg_write(PD_GNSS_SS_AON_CFG4, &temp, 4);
	if (ret < 0) {
		pr_err("write PD_GNSS_SS_AON_CFG4 err:%d\n", ret);
		return ret;
	}

	return 0;
}

int open_power_ctl(void)
{
	marlin_dev->keep_power_on = false;
	clear_bit(WCN_AUTO, &marlin_dev->power_state);

	return 0;
}
EXPORT_SYMBOL_GPL(open_power_ctl);

static int marlin_set_power(enum wcn_sub_sys subsys, int val)
{
	unsigned long timeleft;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	int locked = 0;

	locked = mutex_trylock(&marlin_dev->power_lock);

	if (marlin_dev->wait_ge2) {
		if (first_call_flag == 1) {
			pr_info("(marlin2+ge2)waiting ge2 download finish\n");
			timeleft
				= wait_for_completion_timeout(
				&ge2_completion, 12*HZ);
			if (!timeleft)
				pr_err("wait ge2 timeout\n");
			first_call_flag = 2;
		}
	}

	pr_info("marlin power state:%lx, subsys: [%s] power %d\n",
		marlin_dev->power_state, strno(subsys), val);
	init_completion(&marlin_dev->download_done);
	init_completion(&marlin_dev->gnss_download_done);

	/*  power on */
	if (val) {
		/*
		 * 1. when the first open:
		 * `- first download gnss, and then download btwifi
		 */
		if (unlikely(!marlin_dev->first_power_on_ready)) {
			pr_info("the first power on start\n");

			if (chip_power_on(subsys) < 0) {
				if (unlikely(locked))
					mutex_unlock(&marlin_dev->power_lock);
				return -1;
			}

			set_bit(subsys, &marlin_dev->power_state);

			pr_info("GNSS start to auto download\n");
			schedule_work(&marlin_dev->gnss_dl_wq);
			timeleft
				= wait_for_completion_timeout(
				&marlin_dev->gnss_download_done, 10 * HZ);
			if (!timeleft) {
				pr_err("GNSS download timeout\n");
				goto out;
			}
			pr_info("gnss auto download finished and run ok\n");

			if (subsys & MARLIN_MASK)
				gnss_powerdomain_close();
			marlin_dev->first_power_on_ready = 1;

			pr_info("then marlin start to download\n");
			schedule_work(&marlin_dev->download_wq);
			timeleft = wait_for_completion_timeout(
				&marlin_dev->download_done,
				msecs_to_jiffies(POWERUP_WAIT_MS));
			if (!timeleft) {
				pr_err("marlin download timeout\n");
				goto out;
			}
			atomic_set(&marlin_dev->download_finish_flag, 1);
			pr_info("then marlin download finished and run ok\n");

			if (g_match_config && g_match_config->unisoc_wcn_pcie) {
				pr_info("then start wcn_firmware_init\n");
				wcn_firmware_init();
			}

			set_wifipa_status(subsys, val);
			if (unlikely(locked))
				mutex_unlock(&marlin_dev->power_lock);

			power_state_notify_or_not(subsys, val);
			if (subsys == WCN_AUTO) {
				marlin_set_power(WCN_AUTO, false);
				return 0;
			}
			/*
			 * If first power on is GNSS, must power off it
			 * after cali finish, and then re-power on it.
			 * This is gnss requirement.
			 */
			if (subsys == MARLIN_GNSS) {
				marlin_set_power(MARLIN_GNSS, false);
				marlin_set_power(MARLIN_GNSS, true);
				return 0;
			}

			return 0;
		}
		/* 2. the second time, WCN_AUTO coming */
		else if (subsys == WCN_AUTO) {
			if (marlin_dev->keep_power_on) {
				pr_info("have power on, no action\n");
				set_wifipa_status(subsys, val);
				set_bit(subsys, &marlin_dev->power_state);
			} else {
				pr_info("!1st,not to bkup gnss cal, no act\n");
			}
		}

		/*
		 * 3. when GNSS open,
		 *	  |- GNSS and MARLIN have power on and ready
		 */
		else if ((((marlin_dev->power_state) & AUTO_RUN_MASK) != 0)
			|| (((marlin_dev->power_state) & GNSS_MASK) != 0)) {
			pr_info("GNSS and marlin have ready\n");
			if (((marlin_dev->power_state) & MARLIN_MASK) == 0)
				loopcheck_ready_set();
			set_wifipa_status(subsys, val);
			set_bit(subsys, &marlin_dev->power_state);

			goto check_power_state_notify;
		}
		/* 4. when GNSS close, marlin open.
		 *	  ->  subsys=gps,GNSS download
		 */
		else if (((marlin_dev->power_state) & MARLIN_MASK) != 0) {
			if ((subsys == MARLIN_GNSS) || (subsys == WCN_AUTO)) {
				pr_info("BTWF ready, GPS start to download\n");
				set_wifipa_status(subsys, val);
				set_bit(subsys, &marlin_dev->power_state);
				gnss_powerdomain_open();

				schedule_work(&marlin_dev->gnss_dl_wq);
				timeleft = wait_for_completion_timeout(
					&marlin_dev->gnss_download_done, 10*HZ);
				if (!timeleft) {
					pr_err("GNSS download timeout\n");
					goto out;
				}

				pr_info("GNSS download finished and ok\n");

			} else {
				pr_info("marlin have open, GNSS is closed\n");
				set_wifipa_status(subsys, val);
				set_bit(subsys, &marlin_dev->power_state);

				goto check_power_state_notify;
			}
		}
		/* 5. when GNSS close, marlin close.no module to power on */
		else {
			pr_info("no module to power on, start to power on\n");
			if (chip_power_on(subsys) < 0) {
				if (unlikely(locked))
					mutex_unlock(&marlin_dev->power_lock);
				return -1;
			}
			set_bit(subsys, &marlin_dev->power_state);

			/* 5.1 first download marlin, and then download gnss */
			if ((subsys == WCN_AUTO || subsys == MARLIN_GNSS)) {
				pr_info("marlin start to download\n");
				schedule_work(&marlin_dev->download_wq);
				timeleft = wait_for_completion_timeout(
					&marlin_dev->download_done,
					msecs_to_jiffies(POWERUP_WAIT_MS));
				if (!timeleft) {
					pr_err("marlin download timeout\n");
					goto out;
				}
				atomic_set(&marlin_dev->download_finish_flag,
					   1);
				pr_info("marlin dl finished and run ok\n");

				pr_info("GNSS start to download\n");
				schedule_work(&marlin_dev->gnss_dl_wq);
				timeleft = wait_for_completion_timeout(
					&marlin_dev->gnss_download_done, 10*HZ);
				if (!timeleft) {
					pr_err("then GNSS download timeout\n");
					goto out;
				}
				pr_info("then gnss dl finished and ok\n");
			}
			/*
			 * 5.2 only download marlin, and then
			 * close gnss power domain
			 */
			else {
				pr_info("only marlin start to download\n");
				schedule_work(&marlin_dev->download_wq);
				if (wait_for_completion_timeout(
					&marlin_dev->download_done,
					msecs_to_jiffies(POWERUP_WAIT_MS))
					<= 0) {

					pr_err("marlin download timeout\n");
					goto out;
				}
				atomic_set(&marlin_dev->download_finish_flag,
					   1);
				pr_info("BTWF download finished and run ok\n");
				gnss_powerdomain_close();
			}
			set_wifipa_status(subsys, val);
		}
		/* power on together's Action */
		power_state_notify_or_not(subsys, val);

		pr_info("wcn chip power on and run finish: [%s]\n",
				  strno(subsys));
		pr_info("sync log status\n");
		wcn_firmware_init();
	/* power off */
	} else {
		if (marlin_dev->power_state == 0) {
			if (flag_reset)
				flag_reset = 0;
			goto check_power_state_notify;
		}

		if (marlin_dev->keep_power_on) {
			if (!flag_reset) {
				if (subsys != WCN_AUTO) {
					/* in order to not download again */
					set_bit(WCN_AUTO,
						&marlin_dev->power_state);
					clear_bit(subsys,
						&marlin_dev->power_state);
				}
				pr_debug("marlin reset flag_reset:%d\n",
					flag_reset);
				goto check_power_state_notify;
			}
		}

		set_wifipa_status(subsys, val);
		clear_bit(subsys, &marlin_dev->power_state);
		if ((marlin_dev->power_state != 0) && (!flag_reset)) {
			pr_info("can not power off, other module is on\n");
			if (subsys == MARLIN_GNSS)
				gnss_powerdomain_close();
			goto check_power_state_notify;
		}

		set_cp_mem_status(subsys, val);
		set_fm_supe_freq(subsys, val, marlin_dev->power_state);
		power_state_notify_or_not(subsys, val);

		pr_info("wcn chip start power off!\n");
		sprdwcn_bus_runtime_put();
		chip_power_off(subsys);
		pr_info("marlin power off!\n");
		atomic_set(&marlin_dev->download_finish_flag, 0);
		if (flag_reset) {
			flag_reset = FALSE;
			marlin_dev->power_state = 0;
		}
	} /* power off end */

	/* power on off together's Action */
	if (unlikely(locked))
		mutex_unlock(&marlin_dev->power_lock);

	return 0;

out:
	sprdwcn_bus_runtime_put();
	if (g_match_config && !g_match_config->unisoc_wcn_pcie)
		mem_pd_poweroff_deinit();

	wifipa_enable(0);
	marlin_avdd18_dcxo_enable(false);
	marlin_clk_enable(false);
	marlin_chip_en(false, false);
	marlin_digital_power_enable(false);
	marlin_analog_power_enable(false);
	chip_reset_release(0);
	marlin_dev->power_state = 0;
	atomic_set(&marlin_dev->download_finish_flag, 0);
	if (unlikely(locked))
		mutex_unlock(&marlin_dev->power_lock);

	return -1;

check_power_state_notify:
	power_state_notify_or_not(subsys, val);
	pr_debug("mutex_unlock\n");
	if (unlikely(locked))
		mutex_unlock(&marlin_dev->power_lock);

	return 0;

}

void marlin_power_off(enum wcn_sub_sys subsys)
{
	pr_info("%s all\n", __func__);

	marlin_dev->keep_power_on = false;
	set_bit(subsys, &marlin_dev->power_state);
	marlin_set_power(subsys, false);
}

int integ_marlin_get_power(void);
int marlin_get_power(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return integ_marlin_get_power();
	else
		return marlin_dev->power_state;
}
EXPORT_SYMBOL_GPL(marlin_get_power);

bool marlin_get_download_status(void)
{
	return atomic_read(&marlin_dev->download_finish_flag);
}
EXPORT_SYMBOL_GPL(marlin_get_download_status);

void marlin_set_download_status(int f)
{
	atomic_set(&marlin_dev->download_finish_flag, f);
}
EXPORT_SYMBOL_GPL(marlin_set_download_status);

int wcn_get_module_status_changed(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return integ_wcn_get_module_status_changed();
	else
		return marlin_dev->loopcheck_status_change;
}
EXPORT_SYMBOL_GPL(wcn_get_module_status_changed);

void wcn_set_module_status_changed(bool status)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		integ_wcn_set_module_status_changed(status);
	else
		marlin_dev->loopcheck_status_change = status;
}
EXPORT_SYMBOL_GPL(wcn_set_module_status_changed);

int marlin_get_module_status(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return integ_marlin_get_module_status();

	if (test_bit(MARLIN_BLUETOOTH, &marlin_dev->power_state) ||
	    test_bit(MARLIN_FM, &marlin_dev->power_state) ||
	    test_bit(MARLIN_WIFI, &marlin_dev->power_state) ||
	    test_bit(MARLIN_MDBG, &marlin_dev->power_state))
		/*
		 * Can't send mdbg cmd before download flag ok
		 * If download flag not ready,loopcheck get poweroff
		 */
		return atomic_read(&marlin_dev->download_finish_flag);
	else
		return 0;
}
EXPORT_SYMBOL_GPL(marlin_get_module_status);

int is_first_power_on(enum wcn_sub_sys subsys)
{
	if (marlin_dev->wifi_need_download_ini_flag == 1)
		return 1;	/*the first */
	else
		return 0;	/* not the first */
}
EXPORT_SYMBOL_GPL(is_first_power_on);

int cali_ini_need_download(enum wcn_sub_sys subsys)
{
	unsigned int pd_wifi_st = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
		pd_wifi_st = mem_pd_wifi_state();
		if ((marlin_dev->wifi_need_download_ini_flag == 1) || pd_wifi_st) {
			pr_info("%s return 1\n", __func__);
			return 1;	/* the first */
		}
	}

	return 0;	/* not the first */
}
EXPORT_SYMBOL_GPL(cali_ini_need_download);

int marlin_set_wakeup(enum wcn_sub_sys subsys)
{
	int ret = 0;	/* temp */

	return 0;
	if (!atomic_read(&marlin_dev->download_finish_flag))
		return -1;

	return ret;
}
EXPORT_SYMBOL_GPL(marlin_set_wakeup);

int marlin_set_sleep(enum wcn_sub_sys subsys, bool enable)
{
	return 0;	/* temp */

	if (!atomic_read(&marlin_dev->download_finish_flag))
		return -1;

	return 0;
}
EXPORT_SYMBOL_GPL(marlin_set_sleep);

int marlin_reset_reg(void)
{
	unsigned int card_detect_wait_ms;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_usb)
		card_detect_wait_ms = USB_CARD_DETECT_WAIT_MS;
	else
		card_detect_wait_ms = CARD_DETECT_WAIT_MS;

	init_completion(&marlin_dev->carddetect_done);
	marlin_reset(true);
	mdelay(1);
	sprdwcn_bus_rescan(marlin_dev);
	if (wait_for_completion_timeout(&marlin_dev->carddetect_done,
		msecs_to_jiffies(card_detect_wait_ms))) {
		return 0;
	}
	pr_err("marlin reset reg wait scan error!\n");

	return -1;
}
EXPORT_SYMBOL_GPL(marlin_reset_reg);

int start_marlin(enum wcn_sub_sys subsys)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	int ret = 0;

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return start_integ_marlin(subsys);

	pr_info("%s [%s]\n", __func__, strno(subsys));
	if (unlikely(mutex_is_locked(&marlin_dev->power_lock)))
		pr_info("%s wait for lock release\n", __func__);

	mutex_lock(&marlin_dev->power_lock);
	if (sprdwcn_bus_get_carddump_status() != 0) {
		pr_err("%s SDIO card dump\n", __func__);
		goto unlock;
	}

	if (get_loopcheck_status()) {
		pr_err("%s loopcheck status is fail\n", __func__);
		goto unlock;
	}

	if (subsys == MARLIN_WIFI) {
		/* not need write cali */
		if (marlin_dev->wifi_need_download_ini_flag == 0)
			/* need write cali */
			marlin_dev->wifi_need_download_ini_flag = 1;
		else
			/* not need write cali */
			marlin_dev->wifi_need_download_ini_flag = 2;
	}
	if (marlin_set_power(subsys, true) < 0)
		goto unlock;
	if (g_match_config && !g_match_config->unisoc_wcn_pcie)
		ret = mem_pd_mgr(subsys, true);

	mutex_unlock(&marlin_dev->power_lock);
	return ret;

unlock:
	mutex_unlock(&marlin_dev->power_lock);
	return -1;
}
EXPORT_SYMBOL_GPL(start_marlin);

int stop_marlin(enum wcn_sub_sys subsys)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	int ret = 0;

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		return stop_integ_marlin(subsys);

	pr_info("%s [%s]\n", __func__, strno(subsys));
	if (unlikely(mutex_is_locked(&marlin_dev->power_lock)))
		pr_info("%s wait for lock release\n", __func__);

	mutex_lock(&marlin_dev->power_lock);
	if (!marlin_get_power()) {
		mutex_unlock(&marlin_dev->power_lock);
		pr_info("%s no module opend\n", __func__);
		return 0;
	}

	if (sprdwcn_bus_get_carddump_status() != 0) {
		pr_err("%s SDIO card dump\n", __func__);
		goto unlock;
	}

	if (get_loopcheck_status()) {
		pr_err("%s loopcheck status is fail\n", __func__);
		goto unlock;
	}

	if (g_match_config && !g_match_config->unisoc_wcn_pcie)
		mem_pd_mgr(subsys, false);

	ret = marlin_set_power(subsys, false);

	mutex_unlock(&marlin_dev->power_lock);
	return ret;

unlock:
	mutex_unlock(&marlin_dev->power_lock);
	return -1;
}
EXPORT_SYMBOL_GPL(stop_marlin);

static void marlin_subsys_init(void)
{
	int ret;

	pr_info("%s start\n", __func__);
	ret = devm_of_platform_populate(marlin_dev->dev);
	if (ret)
		pr_err("init subsys WFBT error\n");
}

static void marlin_power_wq(struct work_struct *work)
{
	pr_info("%s start\n", __func__);

	/* WCN_AUTO is for auto backup gnss cali data */
	marlin_set_power(WCN_AUTO, true);
	marlin_subsys_init();
}

int marlin_probe(struct platform_device *pdev)
{
	int err;
	struct sprdwcn_bus_ops *bus_ops;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	marlin_dev = devm_kzalloc(&pdev->dev, sizeof(struct marlin_device),
				  GFP_KERNEL);
	if (!marlin_dev)
		return -ENOMEM;

	marlin_dev->write_buffer = devm_kzalloc(&pdev->dev,
						PACKET_SIZE, GFP_KERNEL);
	if (marlin_dev->write_buffer == NULL) {
		devm_kfree(&pdev->dev, marlin_dev);
		pr_err("%s write buffer low memory\n", __func__);
		return -ENOMEM;
	}

	marlin_dev->dev = &pdev->dev;
	marlin_dev->np = pdev->dev.of_node;
	pr_info("%s: device node name: %s\n", __func__, marlin_dev->np->name);

	mutex_init(&(marlin_dev->power_lock));
	marlin_dev->power_state = 0;
	flag_download_done = 0;
	err = marlin_parse_dt(pdev);
	if (err < 0) {
		pr_info("marlin parse_dt some para not config\n");
		if (err == -EPROBE_DEFER) {
			devm_kfree(&pdev->dev, marlin_dev);
			pr_err("%s: get some resources fail, defer probe it\n",
			       __func__);
			return err;
		}
	}
	if (gpio_is_valid(marlin_dev->reset))
		gpio_direction_output(marlin_dev->reset, 0);
	init_completion(&ge2_completion);
	init_completion(&marlin_dev->carddetect_done);

	if (g_match_config && g_match_config->unisoc_wcn_slp)
		slp_mgr_init();

	/* register ops */
	wcn_bus_init();
	get_boot_device();
	bus_ops = get_wcn_bus_ops();
	bus_ops->start_wcn = start_marlin;
	bus_ops->stop_wcn = stop_marlin;

	/* sdiom_init or pcie_init */
	err = sprdwcn_bus_preinit();
	if (err) {
		pr_err("sprdwcn_bus_preinit error: %d\n", err);
		goto error4;
	}

	sprdwcn_bus_register_rescan_cb((void *)marlin_scan_finish);
	if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
		err = sdio_pub_int_init(marlin_dev->int_ap);
		if (err) {
			pr_err("sdio_pub_int_init error: %d\n", err);
			sprdwcn_bus_deinit();
			wcn_bus_deinit();
			if (g_match_config && g_match_config->unisoc_wcn_slp)
				slp_mgr_deinit();
			devm_kfree(&pdev->dev, marlin_dev);
			return err;
		}

		mem_pd_init();
	}

	err = proc_fs_init();
	if (err) {
		pr_err("proc_fs_init error: %d\n", err);
		goto error3;
	}

	err = log_dev_init();
	if (err) {
		pr_err("log_dev_init error: %d\n", err);
		goto error2;
	}

	err = wcn_gnss_dump_init();
	if (err) {
		pr_err("wcn_gnss_dump_init error: %d\n", err);
		goto error1;
	}

	err = wcn_op_init();
	if (err) {
		pr_err("wcn_op_init: %d\n", err);
		goto error0;
	}

	/* init data for pre_gnss_download_firmware*/
	gnss_data_init();

	flag_reset = 0;
	loopcheck_init();
	reset_test_init();
	init_wcn_sysfs();
	INIT_WORK(&marlin_dev->download_wq, pre_btwifi_download_sdio);
	INIT_WORK(&marlin_dev->gnss_dl_wq, pre_gnss_download_firmware);

	INIT_DELAYED_WORK(&marlin_dev->power_wq, marlin_power_wq);
	schedule_delayed_work(&marlin_dev->power_wq,
				msecs_to_jiffies(500));

	pr_info("%s driver match successful v2!\n", __func__);

	return 0;
error0:
	wcn_gnss_dump_exit();
error1:
	log_dev_exit();
error2:
	proc_fs_exit();
error3:
	if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
		mem_pd_exit();
		sdio_pub_int_deinit();
	}
	sprdwcn_bus_deinit();
error4:
	wcn_bus_deinit();
	if (g_match_config && g_match_config->unisoc_wcn_slp)
		slp_mgr_deinit();
	devm_kfree(&pdev->dev, marlin_dev);
	return err;
}

int marlin_remove(struct platform_device *pdev)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	cancel_work_sync(&marlin_dev->download_wq);
	cancel_work_sync(&marlin_dev->gnss_dl_wq);
	cancel_delayed_work_sync(&marlin_dev->power_wq);
	loopcheck_deinit();
	wcn_op_exit();
	log_dev_exit();
	proc_fs_exit();
	if (g_match_config && !g_match_config->unisoc_wcn_pcie) {
		sdio_pub_int_deinit();
		mem_pd_exit();
	}
	exit_wcn_sysfs();
	sprdwcn_bus_deinit();
	if (marlin_dev->power_state != 0) {
		pr_info("marlin some subsys power is on, warning!\n");
		wcn_wifipa_bound_xtl(false);
		wifipa_enable(0);
		marlin_chip_en(false, false);
	}
	wcn_bus_deinit();
	if (g_match_config && g_match_config->unisoc_wcn_slp)
		slp_mgr_deinit();
	marlin_gpio_free(pdev);
	mutex_destroy(&marlin_dev->power_lock);
	devm_kfree(&pdev->dev, marlin_dev->write_buffer);
	devm_kfree(&pdev->dev, marlin_dev);

	pr_info("remove ok!\n");

	return 0;
}
static void __marlin_shutdown(void)
{
	u32 power_state = marlin_get_power();
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (!power_state)
		return;

	marlin_dev->power_state = 0;
	stop_loopcheck();
	wcn_avdd12_bound_xtl(false);
	wcn_wifipa_bound_xtl(false);
	wifipa_enable(0);

	if (g_match_config && !g_match_config->unisoc_wcn_pcie)
		sdio_pub_int_poweron(false);
}

void marlin_shutdown(struct platform_device *pdev)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	pr_info("%s start, power_state=%d\n", __func__, marlin_get_power());
	if (marlin_dev->power_state != 0) {
		pr_warn("marlin some subsys power is on, force close\n");
		sprdwcn_bus_set_carddump_status(true);
		__marlin_shutdown();
	}

	wcn_bus_deinit();
	if (g_match_config && g_match_config->unisoc_wcn_slp)
		slp_mgr_death(); /* AP shutdown, disable wakeup CP2 */

	pr_info("%s end\n", __func__);
}

#if 0
static int marlin_suspend(struct device *dev)
{

	pr_info("[%s]enter\n", __func__);

	return 0;
}

static int marlin_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops marlin_pm_ops = {
	.suspend = marlin_suspend,
	.resume	= marlin_resume,
};

static const struct of_device_id marlin_match_table[] = {
	{.compatible = "unisoc,marlin3",},
	{ },
};

static struct platform_driver marlin_driver = {
	.driver = {
		.name = "marlin",
		.pm = &marlin_pm_ops,
		.of_match_table = marlin_match_table,
	},
	.probe = marlin_probe,
	.remove = marlin_remove,
	.shutdown = marlin_shutdown,
};

static int __init marlin_init(void)
{
	pr_info("%s entry!\n", __func__);
	return platform_driver_register(&marlin_driver);
}

#ifdef WCN_PCIE
device_initcall(marlin_init);
#else
late_initcall(marlin_init);
#endif

static void __exit marlin_exit(void)
{
	platform_driver_unregister(&marlin_driver);
}
module_exit(marlin_exit);

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum  WCN Marlin Driver");
MODULE_AUTHOR("Yufeng Yang <yufeng.yang@spreadtrum.com>");
#endif
