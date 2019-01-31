/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>
#include <linux/slab.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <asm/setup.h>
#include <linux/atomic.h>
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
#include <sec_osal.h>
#include <sec_export.h>
#endif
#include <mt-plat/mtk_boot_common.h>
#include <mt-plat/mtk_ccci_common.h>
#include "ccci_util_log.h"
#include "ccci_util_lib_main.h"
#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#define ENABLE_MEM_SIZE_CHECK
#define MAX_MD_NUM (6)		/* Max 4 internal + Max 2 exteranl */

/*==================================================== */
/* Image process section */
/*==================================================== */
#define IMG_POST_FIX_LEN	(32)
#define AP_PLATFORM_LEN		(16)
/*Note: must sync with sec lib, if ccci and sec has dependency change */
#define CURR_SEC_CCCI_SYNC_VER	(1)
static char *type_str[] = {[md_type_invalid] = "invalid",
	[modem_2g] = "2g",
	[modem_3g] = "3g",
	[modem_wg] = "wg",
	[modem_tg] = "tg",
	[modem_lwg] = "lwg",
	[modem_ltg] = "ltg",
	[modem_sglte] = "sglte",
	[modem_ultg] = "ultg",
	[modem_ulwg] = "ulwg",
	[modem_ulwtg] = "ulwtg",
	[modem_ulwcg] = "ulwcg",
	[modem_ulwctg] = "ulwctg",
	[modem_ulttg] = "ulttg",
	[modem_ulfwg] = "ulfwg",
	[modem_ulfwcg] = "ulfwcg",
	[modem_ulctg] = "ulctg",
	[modem_ultctg] = "ultctg",
	[modem_ultwg] = "ultwg",
	[modem_ultwcg] = "ultwcg",
	[modem_ulftg] = "ulftg",
	[modem_ulfctg] = "ulfctg"
};

static int curr_ubin_id;
static char *product_str[] = {[INVALID_VARSION] = INVALID_STR,
	[DEBUG_VERSION] = DEBUG_STR,
	[RELEASE_VERSION] = RELEASE_STR
};

static struct md_check_header md_img_header[MAX_MD_NUM];
/*static struct md_check_header_v3 md_img_header_v3[MAX_MD_NUM];*/
static struct md_check_header_v4 md_img_header_v4[MAX_MD_NUM];
static struct md_check_header_v5 md_img_header_v5[MAX_MD_NUM];
static struct md_check_header_v6 md_img_header_v6[MAX_MD_NUM];
/*static struct ccci_image_info		img_info[MAX_MD_NUM][IMG_NUM]; */
char md_img_info_str[MAX_MD_NUM][256];

/*--- MD header check ------------ */
static int check_dsp_header(int md_id, void *parse_addr,
	struct ccci_image_info *image)
{
	return 0;
}

static int check_md_header_v3(int md_id, void *parse_addr,
	struct ccci_image_info *image)
{
	int ret;
	bool md_type_check = false;
	bool md_plat_check = false;
	bool md_sys_match = false;
	bool md_size_check = false;
	int idx;
	unsigned int md_size;
	unsigned char *start, *ptr;
	int region_id, domain_id; /* add for v4 v5 */
	/* struct md_check_header_v3 *head = &md_img_header_v3[md_id]; */
	struct md_check_header_v4 *head = &md_img_header_v4[md_id];

	get_md_resv_mem_info(md_id, NULL, &md_size, NULL, NULL);
	/* memcpy(head, (void*)(parse_addr - sizeof(struct md_check_header_v3)),
	 * sizeof(struct md_check_header_v3));
	 */
	start = (unsigned char *)(head);
	ptr = (unsigned char *)(parse_addr - sizeof(struct md_check_header_v3));
	for (idx = 0; idx < sizeof(struct md_check_header_v3); idx++)
		*start++ = *ptr++;

	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"**********MD image check V3 %d**************\n",
		(int)sizeof(struct md_check_header_v3));
	ret = strncmp(head->check_header, MD_HEADER_MAGIC_NO, 12);
	if (ret) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"md check header not exist!\n");
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
	} else {
		if (head->header_verno < 3) {
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"[Error]md check header version mis-match to AP:[%d]!\n",
			head->header_verno);
		} else {
#ifdef ENABLE_2G_3G_CHECK
			if ((head->image_type != 0)
				&& (head->image_type
					== md->config.load_type))
				md_type_check = true;
#else
			md_type_check = true;
#endif

#ifdef ENABLE_CHIP_VER_CHECK
			if (!strncmp(head->platform,
					ccci_get_ap_platform(),
					AP_PLATFORM_LEN))
				md_plat_check = true;
#else
			md_plat_check = true;
#endif

			if (head->bind_sys_id == (md_id + 1))
				md_sys_match = true;
#ifdef ENABLE_MEM_SIZE_CHECK
			if (head->header_verno >= 2) {
				/*md_size = md->mem_layout.md_region_size; */
				if (head->mem_size == md_size) {
					md_size_check = true;
				} else if (head->mem_size < md_size) {
					md_size_check = true;
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Warning]md size in md header isn't sync to DFO setting: (%08x, %08x)\n",
						head->mem_size, md_size);
				}
				image->img_info.mem_size = head->mem_size;
				image->ap_info.mem_size = md_size;
			} else {
				md_size_check = true;
			}
#else
			md_size_check = true;
#endif

			if ((md_id == MD_SYS1)
				&& (head->image_type >= modem_ultg)
				&& (head->image_type <= MAX_IMG_NUM))
				curr_ubin_id = head->image_type;
			image->ap_info.image_type = type_str[head->image_type];
			image->ap_info.platform = ccci_get_ap_platform();
			image->img_info.image_type = type_str[head->image_type];
			image->img_info.platform = head->platform;
			image->img_info.build_time = head->build_time;
			image->img_info.build_ver = head->build_ver;
			image->img_info.product_ver =
				product_str[head->product_ver];
			image->img_info.version = head->product_ver;
			image->img_info.header_verno = head->header_verno;

			if (md_type_check && md_plat_check
				&& md_sys_match && md_size_check) {
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"Modem header check OK!\n");
			} else {
				CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
					"[Error]Modem header check fail!\n");
				if (!md_type_check)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Reason]MD type(2G/3G) mis-match to AP!\n");

				if (!md_plat_check)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Reason]MD platform mis-match to AP!\n");

				if (!md_sys_match)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Reason]MD image is not for MD SYS%d!\n",
						md_id + 1);

				if (!md_size_check)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
					"[Reason]MD mem size mis-match to AP setting!\n");

				ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
			}

			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"(MD)[type]=%s, (AP)[type]=%s\n",
					image->img_info.image_type,
					image->ap_info.image_type);
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"(MD)[plat]=%s, (AP)[plat]=%s\n",
					image->img_info.platform,
					image->ap_info.platform);
			if (head->header_verno >= 2) {
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"(MD)[size]=%x, (AP)[size]=%x\n",
					image->img_info.mem_size,
					image->ap_info.mem_size);
				if (head->md_img_size) {
					if (image->size >= head->md_img_size)
						image->size = head->md_img_size;
					else {
						char title[50];
						char info[100];

						snprintf(title, sizeof(title),
							"MD%d mem size smaller than image header setting",
							md_id + 1);
						snprintf(info, sizeof(info),
							"MD%d mem size(0x%x)<header size(0x%x),please check memory config in <chip>.dtsi",
							md_id + 1, image->size,
							head->md_img_size);
#if defined(CONFIG_MTK_AEE_FEATURE)
						aed_md_exception_api(NULL, 0,
							(const int *)info,
							sizeof(info),
							(const char *)title,
							DB_OPT_DEFAULT);
#endif
						CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
							"[Reason]MD image size mis-match to AP!\n");
						ret =
						-CCCI_ERR_LOAD_IMG_MD_CHECK;
					}
					image->ap_info.md_img_size
						= image->size;
					image->img_info.md_img_size
						= head->md_img_size;
				}
				/* else {image->size -= 0x1A0;}
				 * workaround for md not check in check header
				 */
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"(MD)[img_size]=%x, (AP)[img_size]=%x\n",
					head->md_img_size, image->size);
			}
			if (head->header_verno >= 3) {
				image->dsp_offset = head->dsp_img_offset;
				image->dsp_size = head->dsp_img_size;
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"DSP image offset=%x size=%x\n",
					image->dsp_offset,
					image->dsp_size);
				if (image->dsp_offset == 0xCDCDCDAA) {
					CCCI_UTIL_INF_MSG_WITH_ID(md_id,
						"DSP on EMI disabled\n");
				} else if (((image->dsp_offset&0xFFFF) != 0)
						&& (head->header_verno == 3)) {
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"DSP image offset not 64KB align\n");
					ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
				} else if (image->dsp_offset + image->dsp_size
							> md_size) {
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"DSP image size too large %x\n",
						md_size);
					ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
				}
			} else {
				image->dsp_offset = 0;
			}

			/* RMPU only avilable after check header v4 */
			if (head->header_verno >= 4) {
				for (region_id = 0;
					 region_id <= MPU_REGION_INFO_ID_LAST;
					 region_id++) {
					image->rmpu_info.region_info[region_id].
						region_offset =
						head->region_info[region_id].
						region_offset;
					image->rmpu_info.region_info[region_id].
						region_size =
						head->region_info[region_id].
						region_size;
					CCCI_UTIL_INF_MSG_WITH_ID(md_id,
						"load_image: check_header_v4, region(%d): size = %x , offset = %x\n",
						region_id,
						head->region_info[region_id]
						.region_size,
						head->region_info[region_id]
						.region_offset);
				}

				for (domain_id = 0;
				domain_id < MPU_DOMAIN_INFO_ID_TOTAL_NUM;
					 domain_id++) {
					image->rmpu_info.domain_attr[domain_id]
						= head->domain_attr[domain_id];
					CCCI_UTIL_INF_MSG_WITH_ID(md_id,
						"load_image: check_header_v4, domain(%d): attr = %x\n",
						domain_id,
						head->domain_attr[domain_id]);
				}
			}
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
						"(MD)[build_ver]=%s, [build_time]=%s\n",
						image->img_info.build_ver,
						image->img_info.build_time);
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
						"(MD)[product_ver]=%s\n",
						image->img_info.product_ver);
		}
	}
	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"********MD image check V3*********\n");

	return ret;
}

static int md_check_header_parser(int md_id, void *parse_addr,
	struct ccci_image_info *image)
{
	int ret;
	bool md_type_check = false;
	bool md_plat_check = false;
	bool md_sys_match = false;
	bool md_size_check = false;
	unsigned int md_size;
	unsigned int header_size;
	int idx, header_up;
	unsigned char *start, *ptr;
	int region_id, domain_id; /* add for v4 v5 */

	struct md_check_header_struct *head = NULL;
	struct md_check_header *headv12 = NULL;
	struct md_check_header_v4 *headv34 = NULL;
	struct md_check_header_v5 *headv5 = NULL;
	struct md_check_header_v6 *headv6 = NULL;

	get_md_resv_mem_info(md_id, NULL, &md_size, NULL, NULL);

	header_size = *(((unsigned int *)parse_addr) - 1);
	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"MD image header size = %d\n", header_size);

	if (header_size == sizeof(struct md_check_header_v3)) { /* v3, v4 */
		headv34 = &md_img_header_v4[md_id];
		headv12 = (struct md_check_header *)headv34;
		head = (struct md_check_header_struct *)headv34;
		header_up = 4;
	} else if (header_size == sizeof(struct md_check_header_v5)) {/* v5 */
		headv5 = &md_img_header_v5[md_id];
		headv34 = (struct md_check_header_v4 *)headv5;
		headv12 = (struct md_check_header *)headv5;
		head = (struct md_check_header_struct *)headv5;
		header_up = 5;
	} else if (header_size == sizeof(struct md_check_header_v6)) {/* v6 */
		headv6 = &md_img_header_v6[md_id];
		headv34 = (struct md_check_header_v4 *)headv6;
		headv12 = (struct md_check_header *)headv6;
		headv5 = (struct md_check_header_v5 *)headv6;
		head = (struct md_check_header_struct *)headv6;
		header_up = 6;
	} else { /* Default Load v1/2 */
		/* if (header_size == sizeof(struct md_check_header))
		 * {*//* v1, v2
		 */
		headv12 = &md_img_header[md_id];
		head = (struct md_check_header_struct *)headv12;
		header_up = 2;
	}

	start = (unsigned char *)(head);
	ptr = (unsigned char *)(parse_addr - header_size);
	for (idx = 0; idx < header_size; idx++)
		*start++ = *ptr++;

	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"******MD image check v%d %d*****\n",
			(int)head->header_verno, (int)header_size);
	ret = strncmp(head->check_header, MD_HEADER_MAGIC_NO, 12);
	if (ret) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"md check header not exist!\n");
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
	} else if (head->header_verno > header_up) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"[Error]md check header version mis-match to AP:[%d]!\n",
			head->header_verno);
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
	} else {
#ifdef ENABLE_2G_3G_CHECK
		if ((head->image_type != 0)
				&& (head->image_type == md->config.load_type))
			md_type_check = true;
#else
		md_type_check = true;
#endif

#ifdef ENABLE_CHIP_VER_CHECK
		if (!strncmp(head->platform,
				ccci_get_ap_platform(), AP_PLATFORM_LEN))
			md_plat_check = true;
#else
		md_plat_check = true;
#endif

		if (head->bind_sys_id == (md_id + 1))
			md_sys_match = true;
#ifdef ENABLE_MEM_SIZE_CHECK
		if (head->header_verno >= 2) {
			/*md_size = md->mem_layout.md_region_size; */
			if (head->mem_size == md_size) {
				md_size_check = true;
			} else if (head->mem_size < md_size) {
				md_size_check = true;
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"[Warning]md size in md header isn't sync to DFO setting: (%08x, %08x)\n",
					head->mem_size, md_size);
			}
			image->img_info.mem_size = head->mem_size;
			image->ap_info.mem_size = md_size;
		} else {
			md_size_check = true;
		}
#else
		md_size_check = true;
#endif
		if ((md_id == MD_SYS1)
			&& (head->image_type >= modem_ultg)
			&& (head->image_type <= MAX_IMG_NUM))
			curr_ubin_id = head->image_type;
		image->ap_info.image_type = type_str[head->image_type];
		image->ap_info.platform = ccci_get_ap_platform();
		image->img_info.image_type = type_str[head->image_type];
		image->img_info.platform = head->platform;
		image->img_info.build_time = head->build_time;
		image->img_info.build_ver = head->build_ver;
		image->img_info.product_ver = product_str[head->product_ver];
		image->img_info.version = head->product_ver;
		image->img_info.header_verno = head->header_verno;

		if (md_type_check
			&& md_plat_check
			&& md_sys_match
			&& md_size_check) {
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"Modem header check OK!\n");
		} else {
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"[Error]Modem header check fail!\n");
			if (!md_type_check)
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"[Reason]MD type(2G/3G) mis-match to AP!\n");

			if (!md_plat_check)
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"[Reason]MD platform mis-match to AP!\n");

			if (!md_sys_match)
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"[Reason]MD image is not for MD SYS%d!\n",
					md_id + 1);

			if (!md_size_check)
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"[Reason]MD mem size mis-match to AP setting!\n");

			ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
		}

		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"(MD)[type]=%s, (AP)[type]=%s\n",
			image->img_info.image_type,
			image->ap_info.image_type);
		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"(MD)[plat]=%s, (AP)[plat]=%s\n",
			image->img_info.platform,
			image->ap_info.platform);
		if (head->header_verno >= 2) {
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[size]=%x, (AP)[size]=%x\n",
				image->img_info.mem_size,
				image->ap_info.mem_size);
			if (head->md_img_size) {
				if (image->size >= head->md_img_size)
					image->size = head->md_img_size;
				else {
					char title[50];
					char info[100];

					snprintf(title, sizeof(title),
						"MD%d mem size smaller than image header setting",
						md_id + 1);
					snprintf(info, sizeof(info),
						"MD%d mem cfg size(0x%x)<header size(0x%x),please check memory config in <chip>.dtsi",
						md_id + 1, image->size,
						head->md_img_size);
#if defined(CONFIG_MTK_AEE_FEATURE)
					aed_md_exception_api(NULL, 0,
						(const int *)info, sizeof(info),
						(const char *)title,
						DB_OPT_DEFAULT);
#endif
					CCCI_UTIL_INF_MSG_WITH_ID(md_id,
						"[Reason]MD image size mis-match to AP!\n");
					ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
				}
				image->ap_info.md_img_size
					= image->size;
				image->img_info.md_img_size
					= head->md_img_size;
			}
			/* else {image->size -= 0x1A0;}
			 * workaround for md not check in check header
			 */
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[img_size]=%x, (AP)[img_size]=%x\n",
				head->md_img_size, image->size);
		}

		/* dsp offset && size */
		if (head->header_verno >= 3) {
			image->dsp_offset = headv34->dsp_img_offset;
			image->dsp_size = headv34->dsp_img_size;
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"DSP image offset=%x size=%x\n",
				image->dsp_offset,
				image->dsp_size);
			if (image->dsp_offset == 0xCDCDCDAA) {
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"DSP on EMI disabled\n");
			} else if (((image->dsp_offset&0xFFFF) != 0)
					&& (head->header_verno == 3)) {
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"DSP image offset not 64KB align\n");
				ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
			} else if (image->dsp_offset
						+ image->dsp_size > md_size) {
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"DSP image size too large %x\n",
					md_size);
				ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
			}
		} else {
			image->dsp_offset = 0;
		}

		/* RMPU only avilable after check header v4 */
		if (head->header_verno >= 4) {
			for (region_id = 0;
				 region_id <= MPU_REGION_INFO_ID_LAST;
				 region_id++) {
				image->rmpu_info.region_info[region_id].
				region_offset =
				headv34->region_info[region_id].
				region_offset;
				image->rmpu_info.region_info[region_id].
				region_size =
				headv34->region_info[region_id].
				region_size;
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"load_image: check_header_v4, region(%d): size = %x , offset = %x\n",
					region_id,
					headv34->region_info[region_id].
					region_size,
					headv34->region_info[region_id].
					region_offset);
			}
			for (domain_id = 0;
				 domain_id < MPU_DOMAIN_INFO_ID_TOTAL_NUM;
				 domain_id++) {
				image->rmpu_info.domain_attr[domain_id]
					= headv34->domain_attr[domain_id];
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"load_image: check_header_v4, domain(%d): attr = %x\n",
					domain_id,
					headv34->domain_attr[domain_id]);
			}
		}

		/* ARM7 only avilable after check header v5 */
		if (head->header_verno >= 5) {
			image->arm7_offset = headv5->arm7_img_offset;
			image->arm7_size = headv5->arm7_img_size;
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"load_image: check_header_v5, arm7_offset = 0x%08X, arm_size = 0x%08X\n",
				image->arm7_offset,
				image->arm7_size);
		}

		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[build_ver]=%s, [build_time]=%s\n",
				image->img_info.build_ver,
				image->img_info.build_time);
		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[product_ver]=%s\n",
				image->img_info.product_ver);

	}
	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"*****MD image check end******\n");

	return ret;
}

static int check_md_header(int md_id, void *parse_addr,
	struct ccci_image_info *image)
{
	int ret;
	bool md_type_check = false;
	bool md_plat_check = false;
	bool md_sys_match = false;
	bool md_size_check = false;
	unsigned int md_size;
	unsigned int header_size;
	int idx;
	unsigned char *start, *ptr;
	struct md_check_header *head = &md_img_header[md_id];

	get_md_resv_mem_info(md_id, NULL, &md_size, NULL, NULL);
	header_size = *(((unsigned int *)parse_addr) - 1);
	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"MD image header size = %d\n", header_size);
	/* v3, v4 */
	if (header_size == sizeof(struct md_check_header_v3))
		return check_md_header_v3(md_id, parse_addr, image);
	else if (header_size == sizeof(struct md_check_header_v5))
		return md_check_header_parser(md_id, parse_addr, image);
	else if (header_size == sizeof(struct md_check_header_v6))
		return md_check_header_parser(md_id, parse_addr, image);

	/* memcpy(head,
	 * (void*)(parse_addr - sizeof(struct md_check_header)),
	 * sizeof(struct md_check_header));
	 */
	start = (unsigned char *)(head);
	ptr = (unsigned char *)(parse_addr
			- sizeof(struct md_check_header));
	for (idx = 0; idx < sizeof(struct md_check_header); idx++)
		*start++ = *ptr++;

	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"******MD image check %d******\n",
		(int)sizeof(struct md_check_header));
	ret = strncmp(head->check_header, MD_HEADER_MAGIC_NO, 12);
	if (ret) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"md check header not exist! %d\n", header_size);
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
	} else {
		if (head->header_verno > 2) {
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
				"[Error]md check header version mis-match to AP:[%d]!\n",
				head->header_verno);
		} else {
#ifdef ENABLE_2G_3G_CHECK
			if ((head->image_type != 0)
					&& (head->image_type
						== md->config.load_type))
				md_type_check = true;
#else
			md_type_check = true;
#endif

#ifdef ENABLE_CHIP_VER_CHECK
			if (!strncmp(head->platform,
					ccci_get_ap_platform(),
					AP_PLATFORM_LEN))
				md_plat_check = true;
#else
			md_plat_check = true;
#endif

			if (head->bind_sys_id == (md_id + 1))
				md_sys_match = true;
#ifdef ENABLE_MEM_SIZE_CHECK
			if (head->header_verno == 2) {
				/*md_size = md->mem_layout.md_region_size; */
				if (head->mem_size == md_size) {
					md_size_check = true;
				} else if (head->mem_size < md_size) {
					md_size_check = true;
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Warning]md size in md header isn't sync to DFO setting: (%08x, %08x)\n",
						head->mem_size, md_size);
				}
				image->img_info.mem_size
					= head->mem_size;
				image->ap_info.mem_size
					= md_size;
			} else {
				md_size_check = true;
			}
#else
			md_size_check = true;
#endif

			image->ap_info.image_type = type_str[head->image_type];
			image->ap_info.platform = ccci_get_ap_platform();
			image->img_info.image_type = type_str[head->image_type];
			image->img_info.platform = head->platform;
			image->img_info.build_time = head->build_time;
			image->img_info.build_ver = head->build_ver;
			image->img_info.product_ver =
				product_str[head->product_ver];
			image->img_info.version = head->product_ver;

			if (md_type_check && md_plat_check
					&& md_sys_match && md_size_check) {
				CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
					"Modem header check OK!\n");
			} else {
				CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
					"[Error]Modem header check fail!\n");
				if (!md_type_check)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Reason]MD type(2G/3G) mis-match to AP!\n");

				if (!md_plat_check)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Reason]MD platform mis-match to AP!\n");

				if (!md_sys_match)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Reason]MD image is not for MD SYS%d!\n",
						md_id + 1);

				if (!md_size_check)
					CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
						"[Reason]MD mem size mis-match to AP setting!\n");

				ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
			}

			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[type]=%s, (AP)[type]=%s\n",
				image->img_info.image_type,
				image->ap_info.image_type);
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[plat]=%s, (AP)[plat]=%s\n",
				image->img_info.platform,
				image->ap_info.platform);
			if (head->header_verno >= 2) {
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"(MD)[size]=%x, (AP)[size]=%x\n",
					image->img_info.mem_size,
					image->ap_info.mem_size);
				if (head->md_img_size) {
					if (image->size >= head->md_img_size)
						image->size = head->md_img_size;
					else {
						CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
							"[Reason]MD image size mis-match to AP!\n");
						ret =
						-CCCI_ERR_LOAD_IMG_MD_CHECK;
					}
					image->ap_info.md_img_size
						= image->size;
					image->img_info.md_img_size
						= head->md_img_size;
				}
				CCCI_UTIL_INF_MSG_WITH_ID(md_id,
					"(MD)[img_size]=%x, (AP)[img_size]=%x\n",
					head->md_img_size, image->size);
			}
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[build_ver]=%s, [build_time]=%s\n",
				image->img_info.build_ver,
				image->img_info.build_time);
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"(MD)[product_ver]=%s\n",
				image->img_info.product_ver);
		}
	}
	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"******MD image check*******\n");

	return ret;
}

char *ccci_get_md_info_str(int md_id)
{
	return md_img_info_str[md_id];
}

void get_md_postfix(int md_id, char k[], char buf[], char buf_ex[])
{
	/* name format: modem_X_YY_K_Ex.img */
	int X, Ex = 0;
	char YY_K[IMG_POSTFIX_LEN];
	unsigned int feature_val = 0;

	if ((md_id < 0) || (md_id > MAX_MD_NUM)) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"wrong MD ID to get postfix\n");
		return;
	}

	/* X */
	X = md_id + 1;

	if ((curr_ubin_id != 0) && (md_id == MD_SYS1)) {
		if (buf) {
			snprintf(buf, IMG_POSTFIX_LEN,
				"%d_%s_n", X, type_str[curr_ubin_id]);
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
				"MD%d image postfix=%s\n",
				md_id + 1, buf);
		}

		if (buf_ex) {
			snprintf(buf_ex, IMG_POSTFIX_LEN,
				"%d_%s_n_E%d", X,
				type_str[curr_ubin_id], Ex);
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
				"MD%d image postfix=%s\n",
				md_id + 1, buf_ex);
		}
		return;
	}
	/* YY_ */
	YY_K[0] = '\0';
	switch (md_id) {
	case MD_SYS1:
		feature_val = get_md_img_type(MD_SYS1);
		break;
	case MD_SYS2:
		feature_val = get_md_img_type(MD_SYS2);
		break;
	case MD_SYS3:
		feature_val = get_md_img_type(MD_SYS3);
		break;
	case MD_SYS5:
		feature_val = get_md_img_type(MD_SYS5);
		break;
	default:
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"request MD ID %d not supported\n", md_id);
		break;
	}

	if ((feature_val == 0) || (feature_val >= MAX_IMG_NUM)) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"request MD type %d not supported\n",
			feature_val);
		feature_val = md_type_invalid;
	}

	/* K */
	if (k == NULL)
		snprintf(YY_K, IMG_POSTFIX_LEN,
			"_%s_n", type_str[feature_val]);
	else
		snprintf(YY_K, IMG_POSTFIX_LEN,
			"_%s_%s", type_str[feature_val], k);

	/* [_Ex] Get chip version */
#if 0
	if (get_chip_version() == CHIP_SW_VER_01)
		Ex = 1;
	else if (get_chip_version() == CHIP_SW_VER_02)
		Ex = 2;
#else
	Ex = 1;
#endif

	/* Gen post fix */
	if (buf) {
		snprintf(buf, IMG_POSTFIX_LEN, "%d%s", X, YY_K);
		CCCI_UTIL_DBG_MSG_WITH_ID(md_id,
			"MD%d image postfix=%s\n", md_id + 1, buf);
	}

	if (buf_ex) {
		snprintf(buf_ex, IMG_POSTFIX_LEN,
			"%d%s_E%d", X, YY_K, Ex);
		CCCI_UTIL_DBG_MSG_WITH_ID(md_id,
			"MD%d image postfix=%s\n", md_id + 1, buf_ex);
	}
}
static int check_if_bypass_header(void *buf, int *img_size);
int ccci_load_firmware(int md_id, void *img_inf,
	char img_err_str[], char post_fix[], struct device *dev)
{
#define MAX_REMAP_SIZE (1024 * 1024)
	int i = 0;
	int ret = 0;
	int check_ret = 0;
	int read_size = 0;
	unsigned long load_addr = 0;
	void *start = NULL;
	unsigned long end_addr;
	const struct firmware *fw_entry = NULL;
	const int dsp_header_size = 1024;
	int size_per_read = MAX_REMAP_SIZE;
	char img_name[IMG_NAME_LEN];
	struct ccci_image_info *img
		= (struct ccci_image_info *)img_inf;
	int img_size = 0;
	int hdr_size = 0;
	void *img_data_ptr = NULL;
	char *img_str = md_img_info_str[md_id];
	int scan_max;
	int md_type_val;


	if (dev == NULL) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"dev == NULL\n");
		ret = -CCCI_ERR_LOAD_IMG_FILE_OPEN;
		goto out;
	}
	/*  Gen file name */
	get_md_postfix(md_id, NULL, post_fix, NULL);

	/*  Gen MD image name */
	if (img->type == IMG_MD) {
		snprintf(img_name, IMG_NAME_LEN,
			"modem_%s.img", post_fix);
	/*  Gen DSP image name */
	} else if (img->type == IMG_DSP) {
		snprintf(img_name, IMG_NAME_LEN,
			"dsp_%s.bin", post_fix);
	/* Gen ARMV7 image name */
	}  else if (img->type == IMG_ARMV7) {
		snprintf(img_name, IMG_NAME_LEN,
			"armv7_%s.bin", post_fix);
	} else {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"[Error]Invalid img type%d\n",
			img->type);
		return -CCCI_ERR_INVALID_PARAM;
	}

	md_type_val = get_modem_support_cap(md_id);
	if ((md_type_val > 0)
			&& (md_type_val < modem_ultg)) {
		i = md_type_val;
		scan_max = md_type_val;
	} else {
		/*
		 * NOTES:
		 * if md support type is ubin,
		 * then need try to find suitable md image
		 */
		i = modem_ultg;
		scan_max = MAX_IMG_NUM;
	}
TRY_LOAD_IMG:
	ret = request_firmware(&fw_entry, img_name, dev);
	if (ret != 0) {
		/*CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
		 *	"Try to load firmware %s failed:ret=%d!\n",
		 * img_name, ret);
		 */
		if (i <= scan_max) {
			CCCI_UTIL_INF_MSG_WITH_ID(md_id, "Curr i:%d\n", i);
			if (img->type == IMG_MD)
				snprintf(img_name, IMG_NAME_LEN,
					"modem_%d_%s_n.img",
					md_id+1, type_str[i]);
			else if (img->type == IMG_DSP)
				snprintf(img_name, IMG_NAME_LEN,
					"dsp_%d_%s_n.bin",
					md_id+1, type_str[i]);
			else if (img->type == IMG_ARMV7)
				snprintf(img_name, IMG_NAME_LEN,
					"armv7_%d_%s_n.bin",
					md_id+1, type_str[i]);
			else {
				CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
					"[Error]Invalid img type%d\n",
					img->type);
				return -CCCI_ERR_INVALID_PARAM;
			}
			i++;
			goto TRY_LOAD_IMG;
		} else {
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			     "Try to load all md image failed:ret=%d!\n", ret);
#if defined(CONFIG_MTK_AEE_FEATURE)
			aed_md_exception_api(NULL, 0, NULL, 0,
				"Try to load all md image failed!",
				DB_OPT_DEFAULT);
#endif
			ret = -CCCI_ERR_INVALID_PARAM;
			goto out;
		}
	}
	strncpy(img->file_name, img_name, sizeof(img->file_name));
	img->offset = 0;
	img->tail_length = 0;
	/*Check whether need skip header*/
	hdr_size = check_if_bypass_header((void *)fw_entry->data, &img_size);
	if (hdr_size != 0) {
		img->size = img_size;
		img_data_ptr = (void *)fw_entry->data + hdr_size;
	} else {
		img->size = fw_entry->size;
		img_data_ptr = (void *)fw_entry->data;
	}

	/*load modem img context to kernel addr*/
	load_addr = img->address;
	CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
		"Not cipher image: %s Firmware:size=%zu, img_size=%d\n",
		img_name, fw_entry->size, img->size);
	while (1) {
		if (read_size + size_per_read > img->size)
			size_per_read = img->size - read_size;
		else
			size_per_read = MAX_REMAP_SIZE;
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"Firmware:read_size=%d, size_per_read=%d\n",
			read_size, size_per_read);
		/* size_per_read 8 bytes aligned remap memory */
		start = ioremap_nocache((load_addr + read_size),
					roundup(size_per_read, 8));
		if (start == 0) {
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
				"image ioremap fail %d\n",
				(unsigned int)(load_addr + read_size));
			ret = -CCCI_ERR_LOAD_IMG_NOMEM;
			goto out;
		}
		memcpy(start,
			(void *)(img_data_ptr + read_size),
			size_per_read);
		iounmap(start);
		start = NULL;
		read_size += size_per_read;
		if (read_size == img->size - img->tail_length)
			break;
	}

	CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
		"Firmware check header:load_addr=%lx, size=%d\n",
		load_addr, img->size);
	if (img->type == IMG_MD) {
		start = ioremap_nocache(
				round_down(load_addr
					+ img->size - 0x4000, 0x4000),
				round_up(img->size, 0x4000)
				- round_down(img->size - 0x4000, 0x4000));
		end_addr =
		    ((unsigned long)start
			+ img->size
			- round_down(img->size - 0x4000, 0x4000));
		check_ret = check_md_header(md_id, (void *)end_addr, img);
		if (check_ret < 0) {
			ret = check_ret;
			goto out;
		}
	} else if (img->type == IMG_DSP) {
		start = ioremap_nocache(load_addr, dsp_header_size);
		check_ret = check_dsp_header(md_id, start, img);
		if (check_ret < 0) {
			ret = check_ret;
			goto out;
		}
	}
	ret = read_size;
	CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
		"Request firmware: %s (size=0x%x) to 0x%lx\n",
		img->file_name, img->size - img->tail_length,
		load_addr);

	/* Construct image information string */
	if (img->type == IMG_MD) {
		snprintf(img_str, sizeof(md_img_info_str[md_id]),
			"MD:%s*%s*%s*%s*%s\nAP:%s*%s*%08x (MD)%08x\n",
			img->img_info.image_type, img->img_info.platform,
			img->img_info.build_ver, img->img_info.build_time,
			img->img_info.product_ver, img->ap_info.image_type,
			img->ap_info.platform, img->ap_info.mem_size,
			img->img_info.mem_size);
	}

 out:
	if (fw_entry != NULL) {
		release_firmware(fw_entry);
		fw_entry = NULL;
	}
	if (start != NULL) {
		iounmap(start);
		start = NULL;
	}

	/* Prepare error string if needed */
	if (img_err_str != NULL) {
		if (ret == -CCCI_ERR_LOAD_IMG_SIGN_FAIL) {
			snprintf(img_err_str, IMG_ERR_STR_LEN,
				"%s Signature check fail\n", img->file_name);
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"signature check fail!\n");
		} else if (ret == -CCCI_ERR_LOAD_IMG_CIPHER_FAIL) {
			snprintf(img_err_str, IMG_ERR_STR_LEN,
				"%s Cipher chekc fail\n", img->file_name);
			CCCI_UTIL_INF_MSG_WITH_ID(md_id,
				"cipher check fail!\n");
		} else if (ret == -CCCI_ERR_LOAD_IMG_FILE_OPEN) {
			snprintf(img_err_str, IMG_ERR_STR_LEN,
				"Modem image not exist\n");
		} else if (ret == -CCCI_ERR_LOAD_IMG_MD_CHECK) {
			snprintf(img_err_str, IMG_ERR_STR_LEN,
				"Modem mismatch to AP\n");
		}
	}

	return ret;
}

int ccci_init_security(void)
{
	int ret = 0;
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	CCCI_UTIL_INF_MSG("security is on!\n");
#else
	CCCI_UTIL_INF_MSG("security is off!\n");
#endif
	return ret;
}

#define IMG_MAGIC		0x58881688
#define EXT_MAGIC		0x58891689

#define IMG_NAME_SIZE		32
#define IMG_HDR_SIZE		512
union prt_img_hdr {
	struct {
		/* always IMG_MAGIC */
		unsigned int magic;
		/* image size, image header and padding are not included */
		unsigned int dsize;
		char name[IMG_NAME_SIZE];
		/* image load address in RAM */
		unsigned int maddr;
		/* maddr is counted from the beginning or end of RAM */
		unsigned int mode;
		/* extension */
		/* always EXT_MAGIC */
		unsigned int ext_magic;
		/* header size is 512 bytes currently,
		 * but may extend in the future
		 */
		unsigned int hdr_size;
		/* see HDR_VERSION */
		unsigned int hdr_version;
		/* please refer to #define beginning with SEC_IMG_TYPE_ */
		unsigned int img_type;
		/* end of image list?
		 * 0: this image is followed by another image
		 * 1: end
		 */
		unsigned int img_list_end;
		/* image size alignment setting in bytes,
		 * 16 by default for AES encryption
		 */
		unsigned int align_size;
		/* high word of image size for 64 bit address support */
		unsigned int dsize_extend;
		/* high word of image load address in RAM
		 * for 64 bit address support
		 */
		unsigned int maddr_extend;
	} info;
	unsigned char data[IMG_HDR_SIZE];
};

int ccci_get_md_check_hdr_inf(int md_id, void *img_inf, char post_fix[])
{
	int ret = 0;
	struct ccci_image_info *img_ptr = (struct ccci_image_info *)img_inf;
	char *img_str;
	char *buf;
	unsigned int md_type = 0;

	buf = kmalloc(1024, GFP_KERNEL);
	if (buf == NULL) {
		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"fail to allocate memor for chk_hdr\n");
		return -1;
	}

	img_str = md_img_info_str[md_id];

	ret = get_raw_check_hdr(md_id, buf, 1024);
	if (ret < 0) {
		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"fail to load header(%d)!\n", ret);
		kfree(buf);
		return -1;
	}

	img_ptr->size = get_md_img_raw_size(md_id);
	ret = check_md_header(md_id, buf+ret, img_ptr);
	if (ret < 0) {
		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"check header fail(%d)!\n", ret);
		kfree(buf);
		return -1;
	}

	/* Get modem capability */
	md_type = get_md_type_from_lk(md_id);

	kfree(buf);

	/* Construct image information string */
	snprintf(img_str, sizeof(md_img_info_str[md_id]),
		"MD:%s*%s*%s*%s*%s\nAP:%s*%s*%08x (MD)%08x\n",
		img_ptr->img_info.image_type, img_ptr->img_info.platform,
		img_ptr->img_info.build_ver, img_ptr->img_info.build_time,
		img_ptr->img_info.product_ver, img_ptr->ap_info.image_type,
		img_ptr->ap_info.platform, img_ptr->ap_info.mem_size,
		img_ptr->img_info.mem_size);

	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"check header str[%s]!\n", img_str);

	if (md_id == MD_SYS1) {
		curr_ubin_id = md_type;
		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"type @ header(%d)!\n", curr_ubin_id);
	}

	snprintf(post_fix, IMG_POSTFIX_LEN,
		"%d_%s_n", md_id+1, img_ptr->img_info.image_type);

	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"post fix[%s]!\n", post_fix);

	return 0;
}

int check_if_bypass_header(void *buf, int *img_size)
{
	union prt_img_hdr *hdr_ptr;

	if (buf == NULL) {
		CCCI_UTIL_ERR_MSG("buffer is NULL, no need bypass\n");
		return 0;
	}
	hdr_ptr = (union prt_img_hdr *)buf;
	if ((hdr_ptr->info.magic == IMG_MAGIC)
			&& (hdr_ptr->info.ext_magic == EXT_MAGIC)) {
		CCCI_UTIL_ERR_MSG("This image has header, bypass %d bytes\n",
			(int)hdr_ptr->info.hdr_size);
		if (img_size)
			*img_size = hdr_ptr->info.dsize;
		return (int)hdr_ptr->info.hdr_size;
	}

	CCCI_UTIL_ERR_MSG("This image does not find header, no need bypass\n");
	return 0;
}

