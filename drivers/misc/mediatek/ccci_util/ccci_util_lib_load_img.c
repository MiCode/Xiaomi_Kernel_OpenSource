// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
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
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_fdt.h>
#endif
#include <asm/setup.h>
#include <linux/atomic.h>
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
#include <sec_osal.h>
#include <sec_export.h>
#endif
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_util_log.h"
#include "ccci_util_lib_main.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif


/*==================================================== */
/* Image process section */
/*==================================================== */
#define IMG_POST_FIX_LEN	(32)
#define AP_PLATFORM_LEN		(16)
/*Note: must sync with sec lib, if ccci and sec has dependency change */
#define CURR_SEC_CCCI_SYNC_VER	(1)

static const char *get_md_img_cap_str(int md_img_type)
{
	return ccci_get_md_cap_str((unsigned int)md_img_type);
}

static struct md_check_header md_img_header;
/*static struct md_check_header_v3 md_img_header_v3;*/
static struct md_check_header_v4 md_img_header_v4;
static struct md_check_header_v6 md_img_header_v6;
/*static struct ccci_image_info		img_info[MAX_MD_NUM][IMG_NUM]; */
char md_img_info_str[256];

static char *s_ap_platform_info;

char *ccci_get_ap_platform(void)
{
	unsigned int ap_plat_id = 0;
	int ret = 0;

	if (!s_ap_platform_info) {
		ret = mtk_ccci_find_args_val("ap_platform", (unsigned char *)&ap_plat_id, 4);
		if (ret <= 0)
			CCCI_UTIL_INF_MSG("Get ap_platform tag fail:%d\n", ret);

		s_ap_platform_info = kzalloc(16, GFP_KERNEL);
		if (s_ap_platform_info)
			scnprintf(s_ap_platform_info, 16, "MT%u", ap_plat_id);
	}

	return s_ap_platform_info;
}

/*--- MD header check ------------ */
static int check_md_header_v3(void *parse_addr,
	struct ccci_image_info *image)
{
	int ret;
	bool md_size_check = false;
	int idx;
	unsigned int md_size = 0;
	unsigned char *start = NULL;
	unsigned char *ptr = NULL;
	/* struct md_check_header_v3 *head = &md_img_header_v3; */
	struct md_check_header_v4 *head = &md_img_header_v4;

	get_md_resv_mem_info(NULL, &md_size, NULL, NULL);
	/* memcpy(head, (void*)(parse_addr - sizeof(struct md_check_header_v3)),
	 * sizeof(struct md_check_header_v3));
	 */
	start = (unsigned char *)(head);
	ptr = (unsigned char *)(parse_addr - sizeof(struct md_check_header_v3));
	for (idx = 0; idx < sizeof(struct md_check_header_v3); idx++)
		*start++ = *ptr++;

	CCCI_UTIL_INF_MSG(
		"**********MD image check V3 %zu**************\n",
		sizeof(struct md_check_header_v3));
	ret = strncmp(head->check_header, MD_HEADER_MAGIC_NO, 12);
	if (ret) {
		CCCI_UTIL_ERR_MSG("md check header not exist!\n");
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
		goto EXIT_CHECK_V3;
	} else if (head->header_verno < 3) {
		CCCI_UTIL_ERR_MSG(
		"[Error]md check header version mis-match to AP:[%d]!\n",
		head->header_verno);
		goto EXIT_CHECK_V3;

	}

	if (head->header_verno >= 2) {
		/*md_size = md->mem_layout.md_region_size; */
		if (head->mem_size == md_size)
			md_size_check = true;
		else if (head->mem_size < md_size) {
			md_size_check = true;
			CCCI_UTIL_ERR_MSG(
				"[Warning]md size in md header isn't sync to DFO setting: (%08x, %08x)\n",
				head->mem_size, md_size);
		}
		image->img_info.mem_size = head->mem_size;
		image->ap_info.mem_size = md_size;
	} else
		md_size_check = true;

	image->ap_info.image_type = (char *)get_md_img_cap_str(head->image_type);
	image->ap_info.platform = ccci_get_ap_platform();
	image->img_info.image_type = (char *)get_md_img_cap_str(head->image_type);
	image->img_info.platform = head->platform;
	image->img_info.build_time = head->build_time;
	image->img_info.build_ver = head->build_ver;
	image->img_info.product_ver = (char *)ccci_get_md_product_str(head->product_ver);
	image->img_info.version = head->product_ver;
	image->img_info.header_verno = head->header_verno;

	if (md_size_check)
		CCCI_UTIL_INF_MSG("Modem header check OK!\n");
	else {
		CCCI_UTIL_ERR_MSG(
			"[Error]Modem header check fail!\n");

		if (!md_size_check)
			CCCI_UTIL_ERR_MSG(
			"[Reason]MD mem size mis-match to AP setting!\n");

		ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
	}

	CCCI_UTIL_INF_MSG(
			"(MD)[type]=%s, (AP)[type]=%s\n",
			image->img_info.image_type,
			image->ap_info.image_type);
	CCCI_UTIL_INF_MSG(
			"(MD)[plat]=%s, (AP)[plat]=%s\n",
			image->img_info.platform,
			image->ap_info.platform);
	if (head->header_verno >= 2) {
		CCCI_UTIL_INF_MSG(
			"(MD)[size]=%x, (AP)[size]=%x\n",
			image->img_info.mem_size,
			image->ap_info.mem_size);
		if (head->md_img_size) {
			if (image->size >= head->md_img_size)
				image->size = head->md_img_size;
			else {
				char title[50];
				char info[100];

				scnprintf(title, sizeof(title),
					"MD mem size smaller than image header setting");
				scnprintf(info, sizeof(info),
					"MD mem size(0x%x)<header size(0x%x),please check memory config in <chip>.dtsi",
					image->size, head->md_img_size);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(NULL, 0,
					(const int *)info,
					sizeof(info),
					(const char *)title,
					DB_OPT_DEFAULT);
#endif
				CCCI_UTIL_ERR_MSG(
					"[Reason]MD image size mis-match to AP!\n");
				ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
			}
			image->ap_info.md_img_size = image->size;
			image->img_info.md_img_size = head->md_img_size;
		}
		/* else {image->size -= 0x1A0;}
		 * workaround for md not check in check header
		 */
		CCCI_UTIL_INF_MSG(
			"(MD)[img_size]=%x, (AP)[img_size]=%x\n",
			head->md_img_size, image->size);
	}

	CCCI_UTIL_INF_MSG(
				"(MD)[build_ver]=%s, [build_time]=%s\n",
				image->img_info.build_ver,
				image->img_info.build_time);
	CCCI_UTIL_INF_MSG(
				"(MD)[product_ver]=%s\n",
				image->img_info.product_ver);
EXIT_CHECK_V3:
	CCCI_UTIL_INF_MSG("********MD image check V3*********\n");

	return ret;
}

static int md_check_header_parser(void *parse_addr,
	struct ccci_image_info *image)
{
	int ret;
	bool md_size_check = false;
	unsigned int md_size = 0;
	unsigned int header_size;
	int idx, header_up;
	unsigned char *start = NULL;
	unsigned char *ptr = NULL;

	struct md_check_header_struct *head = NULL;
	struct md_check_header_v4 *headv34 = NULL;
	struct md_check_header_v6 *headv6 = NULL;

	get_md_resv_mem_info(NULL, &md_size, NULL, NULL);

	header_size = *(((unsigned int *)parse_addr) - 1);
	CCCI_UTIL_INF_MSG(
		"MD image header size = %d\n", header_size);

	if (header_size == sizeof(struct md_check_header_v3)) { /* v3, v4 */
		headv34 = &md_img_header_v4;
		head = (struct md_check_header_struct *)headv34;
		header_up = 4;
	} else if (header_size == sizeof(struct md_check_header_v6)) {/* v6 */
		headv6 = &md_img_header_v6;
		headv34 = (struct md_check_header_v4 *)headv6;
		head = (struct md_check_header_struct *)headv6;
		header_up = 6;
	} else {
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
		goto CHECK_HEADER_PARSER;
	}
	start = (unsigned char *)(head);
	ptr = (unsigned char *)(parse_addr - header_size);
	for (idx = 0; idx < header_size; idx++)
		*start++ = *ptr++;

	CCCI_UTIL_INF_MSG(
			"******MD image check v%d %d*****\n",
			(int)head->header_verno, (int)header_size);
	ret = strncmp(head->check_header, MD_HEADER_MAGIC_NO, 12);
	if (ret) {
		CCCI_UTIL_ERR_MSG(
			"md check header not exist!\n");
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
		goto CHECK_HEADER_PARSER;
	} else if (head->header_verno > header_up) {
		CCCI_UTIL_ERR_MSG(
			"[Error]md check header version mis-match to AP:[%d]!\n",
			head->header_verno);
		ret = -CCCI_ERR_LOAD_IMG_CHECK_HEAD;
		goto CHECK_HEADER_PARSER;
	}

	if (head->header_verno >= 2) {
		/*md_size = md->mem_layout.md_region_size; */
		if (head->mem_size == md_size)
			md_size_check = true;
		else if (head->mem_size < md_size) {
			md_size_check = true;
			CCCI_UTIL_INF_MSG(
				"[Warning]md size in md header isn't sync to DFO setting: (%08x, %08x)\n",
				head->mem_size, md_size);
		}
		image->img_info.mem_size = head->mem_size;
		image->ap_info.mem_size = md_size;
	} else
		md_size_check = true;

	image->ap_info.image_type = (char *)get_md_img_cap_str(head->image_type);
	image->ap_info.platform = ccci_get_ap_platform();
	image->img_info.image_type = (char *)get_md_img_cap_str(head->image_type);
	image->img_info.platform = head->platform;
	image->img_info.build_time = head->build_time;
	image->img_info.build_ver = head->build_ver;
	image->img_info.product_ver = (char *)ccci_get_md_product_str(head->product_ver);
	image->img_info.version = head->product_ver;
	image->img_info.header_verno = head->header_verno;

	if (md_size_check)
		CCCI_UTIL_INF_MSG("Modem header check OK!\n");
	else {
		CCCI_UTIL_INF_MSG(
			"[Error]Modem header check fail!\n");

		if (!md_size_check)
			CCCI_UTIL_INF_MSG(
				"[Reason]MD mem size mis-match to AP setting!\n");

		ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
	}

	CCCI_UTIL_INF_MSG(
		"(MD)[type]=%s, (AP)[type]=%s\n",
		image->img_info.image_type,
		image->ap_info.image_type);
	CCCI_UTIL_INF_MSG(
		"(MD)[plat]=%s, (AP)[plat]=%s\n",
		image->img_info.platform,
		image->ap_info.platform);
	if (head->header_verno >= 2) {
		CCCI_UTIL_INF_MSG(
			"(MD)[size]=%x, (AP)[size]=%x\n",
			image->img_info.mem_size,
			image->ap_info.mem_size);
		if (head->md_img_size) {
			if (image->size >= head->md_img_size)
				image->size = head->md_img_size;
			else {
				char title[50];
				char info[100];

				scnprintf(title, sizeof(title),
					"MD mem size smaller than image header setting");
				scnprintf(info, sizeof(info),
					"MD mem cfg size(0x%x)<header size(0x%x),please check memory config in <chip>.dtsi",
					image->size, head->md_img_size);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(NULL, 0,
					(const int *)info, sizeof(info),
					(const char *)title,
					DB_OPT_DEFAULT);
#endif
				CCCI_UTIL_INF_MSG(
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
		CCCI_UTIL_INF_MSG(
			"(MD)[img_size]=%x, (AP)[img_size]=%x\n",
			head->md_img_size, image->size);
	}

	CCCI_UTIL_INF_MSG("(MD)[build_ver]=%s, [build_time]=%s\n",
			image->img_info.build_ver,
			image->img_info.build_time);
	CCCI_UTIL_INF_MSG(
			"(MD)[product_ver]=%s\n",
			image->img_info.product_ver);

CHECK_HEADER_PARSER:
	CCCI_UTIL_INF_MSG("*****MD image check end******\n");

	return ret;
}

static int check_md_header(void *parse_addr,
	struct ccci_image_info *image)
{
	unsigned int md_size = 0;
	unsigned int header_size;
	struct md_check_header *head = &md_img_header;

	get_md_resv_mem_info(NULL, &md_size, NULL, NULL);
	header_size = *(((unsigned int *)parse_addr) - 1);
	CCCI_UTIL_INF_MSG(
		"MD image header size = %d\n", header_size);
	/* v3, v4 */
	if (header_size == sizeof(struct md_check_header_v3))
		return check_md_header_v3(parse_addr, image);
	else if (header_size == sizeof(struct md_check_header_v6))
		return md_check_header_parser(parse_addr, image);

	CCCI_UTIL_ERR_MSG("[Reason]Not support header version:%u!\n", head->header_verno);
	return -CCCI_ERR_LOAD_IMG_MD_CHECK;
}

char *ccci_get_md_info_str(void)
{
	return md_img_info_str;
}
EXPORT_SYMBOL(ccci_get_md_info_str);

void get_md_postfix(const char k[], char buf[], char buf_ex[])
{
	/* name format: modem_X_YY_K_Ex.img */
	int Ex = 0;
	char YY_K[IMG_POSTFIX_LEN];
	unsigned int feature_val = 0;
	int img_type;

	img_type = get_md_img_type();

	if (img_type != 0) {
		if (buf) {
			scnprintf(buf, IMG_POSTFIX_LEN,
				"%s_n", get_md_img_cap_str(img_type));
			CCCI_UTIL_ERR_MSG(
				"MD image postfix=%s\n", buf);
		}

		if (buf_ex) {
			scnprintf(buf_ex, IMG_POSTFIX_LEN,
				"%s_n_E%d",	get_md_img_cap_str(img_type), Ex);
			CCCI_UTIL_ERR_MSG(
				"MD image postfix=%s\n", buf_ex);
		}
		return;
	}
	/* YY_ */
	YY_K[0] = '\0';

	feature_val = 3;//get_md_img_type(MD_SYS1); MT6580 using this

	/* K */
	if (k == NULL)
		scnprintf(YY_K, IMG_POSTFIX_LEN,
			"_%s_n", get_md_img_cap_str(feature_val));
	else
		scnprintf(YY_K, IMG_POSTFIX_LEN,
			"_%s_%s", get_md_img_cap_str(feature_val), k);

	/* [_Ex] Get chip version */
	Ex = 1;

	/* Gen post fix */
	if (buf) {
		scnprintf(buf, IMG_POSTFIX_LEN, "%s", YY_K);
		CCCI_UTIL_DBG_MSG(
			"MD image postfix=%s\n", buf);
	}

	if (buf_ex) {
		scnprintf(buf_ex, IMG_POSTFIX_LEN,
			"%s_E%d", YY_K, Ex);
		CCCI_UTIL_DBG_MSG(
			"MD image postfix=%s\n", buf_ex);
	}
}


int ccci_get_md_check_hdr_inf(void *img_inf, char post_fix[])
{
	int ret = 0;
	struct ccci_image_info *img_ptr = (struct ccci_image_info *)img_inf;
	char *img_str = NULL;
	char *buf;

	buf = kmalloc(1024, GFP_KERNEL);
	if (buf == NULL) {
		CCCI_UTIL_INF_MSG(
			"fail to allocate memor for chk_hdr\n");
		return -1;
	}

	img_str = md_img_info_str;

	ret = get_raw_check_hdr(buf, 1024);
	if (ret < 0) {
		CCCI_UTIL_INF_MSG(
			"fail to load header(%d)!\n", ret);
		kfree(buf);
		return -1;
	}

	img_ptr->size = get_md_img_raw_size();
	ret = check_md_header(buf+ret, img_ptr);
	if (ret < 0) {
		CCCI_UTIL_INF_MSG(
			"check header fail(%d)!\n", ret);
		kfree(buf);
		return -1;
	}

	kfree(buf);

	/* Construct image information string */
	scnprintf(img_str, sizeof(md_img_info_str),
		"MD:%s*%s*%s*%s*%s\nAP:%s*%s*%08x (MD)%08x\n",
		img_ptr->img_info.image_type, img_ptr->img_info.platform,
		img_ptr->img_info.build_ver, img_ptr->img_info.build_time,
		img_ptr->img_info.product_ver, img_ptr->ap_info.image_type,
		img_ptr->ap_info.platform, img_ptr->ap_info.mem_size,
		img_ptr->img_info.mem_size);

	CCCI_UTIL_INF_MSG(
		"check header str[%s]!\n", img_str);

	scnprintf(post_fix, IMG_POSTFIX_LEN,
		"%s_n", img_ptr->img_info.image_type);

	CCCI_UTIL_INF_MSG(
		"post fix[%s]!\n", post_fix);

	return 0;
}
EXPORT_SYMBOL(ccci_get_md_check_hdr_inf);

