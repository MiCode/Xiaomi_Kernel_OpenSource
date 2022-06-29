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

#include <linux/of_fdt.h>
#include <asm/setup.h>
#include <linux/atomic.h>
#include "ccci_util_log.h"
#include "ccci_util_lib_main.h"


/*==================================================== */
/* Image process section */
/*==================================================== */
#define IMG_POST_FIX_LEN	(32)
#define AP_PLATFORM_LEN		(16)
/*Note: must sync with sec lib, if ccci and sec has dependency change */
#define CURR_SEC_CCCI_SYNC_VER	(1)

struct md_chk_hdr_common {
	/* magic number is "CHECK_HEADER"*/
	unsigned char check_header[12];
	/* header structure version number */
	unsigned int  header_verno;
	/* 0x0:invalid;
	 * 0x1:debug version;
	 * 0x2:release version
	 */
	unsigned int  product_ver;
	/* 0x0:invalid;
	 * 0x1:2G modem;
	 * 0x2: 3G modem
	 */
	unsigned int  image_type;
	/* MT6573_S01 or MT6573_S02 */
	unsigned char platform[16];
	/* build time string */
	unsigned char build_time[64];
	/* project version, ex:11A_MD.W11.28 */
	unsigned char build_ver[64];

	/* bind to md sys id,
	 * MD SYS1: 1,
	 * MD SYS2: 2
	 */
	unsigned char bind_sys_id;
	/* no shrink: 0, shrink: 1*/
	unsigned char ext_attr;
	/* for reserved */
	unsigned char reserved[2];

	/* md ROM/RAM image size requested by md */
	unsigned int  mem_size;
	/* md image size, exclude head size*/
	unsigned int  md_img_size;

} __packed;

struct md_chk_hdr_v3 {
	/* magic number is "CHECK_HEADER"*/
	unsigned char check_header[12];
	/* header structure version number */
	unsigned int  header_verno;
	/* 0x0:invalid;
	 * 0x1:debug version;
	 * 0x2:release version
	 */
	unsigned int  product_ver;
	/* 0x0:invalid;
	 * 0x1:2G modem;
	 * 0x2: 3G modem
	 */
	unsigned int  image_type;
	/* MT6573_S01 or MT6573_S02 */
	unsigned char platform[16];
	/* build time string */
	unsigned char build_time[64];
	/* project version, ex:11A_MD.W11.28 */
	unsigned char build_ver[64];

	/* bind to md sys id,
	 * MD SYS1: 1,
	 * MD SYS2: 2,
	 * MD SYS5: 5
	 */
	unsigned char bind_sys_id;
	/* no shrink: 0, shrink: 1 */
	unsigned char ext_attr;
	/* for reserved */
	unsigned char reserved[2];

	/* md ROM/RAM image size requested by md */
	unsigned int  mem_size;
	/* md image size, exclude head size */
	unsigned int  md_img_size;
	/* RPC secure memory address */
	unsigned int  rpc_sec_mem_addr;

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;
	unsigned char reserved2[88];
	/* the size of this structure */
	unsigned int  size;
} __packed;

struct md_regin_info {
	unsigned int region_offset;
	unsigned int region_size;
};

struct free_padding_block {
	unsigned int start_offset;
	unsigned int length;
};

/* v4, v5 phased out */

struct md_chk_hdr_v6 {
	/* magic number is "CHECK_HEADER"*/
	unsigned char check_header[12];
	/* header structure version number */
	unsigned int  header_verno;
	/* 0x0:invalid;
	 * 0x1:debug version;
	 * 0x2:release version
	 */
	unsigned int  product_ver;
	/* 0x0:invalid;
	 * 0x1:2G modem;
	 * 0x2: 3G modem
	 */
	unsigned int  image_type;
	/* MT6573_S01 or MT6573_S02 */
	unsigned char platform[16];
	/* build time string */
	unsigned char build_time[64];
	/* project version, ex:11A_MD.W11.28 */
	unsigned char build_ver[64];

	/* bind to md sys id,
	 * MD SYS1: 1,
	 * MD SYS2: 2,
	 * MD SYS5: 5
	 */
	unsigned char bind_sys_id;
	/* no shrink: 0, shrink: 1 */
	unsigned char ext_attr;
	/* for reserved */
	unsigned char reserved[2];

	/* md ROM/RAM image size requested by md */
	unsigned int  mem_size;
	/* md image size, exclude head size */
	unsigned int  md_img_size;
	/* RPC secure memory address */
	unsigned int  rpc_sec_mem_addr;

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;

	/* total region number */
	unsigned int  region_num;
	/* max support 8 regions */
	struct md_regin_info region_info[8];
	/* max support 4 domain settings,
	 * each region has 4 control bits
	 */
	unsigned int  domain_attr[4];

	unsigned int  arm7_img_offset;
	unsigned int  arm7_img_size;

	struct free_padding_block padding_blk[8];

	unsigned int  ap_md_smem_size;
	unsigned int  md_to_md_smem_size;

	unsigned int ramdisk_offset;
	unsigned int ramdisk_size;

	unsigned char reserved_1[144];

	/* the size of this structure */
	unsigned int  size;
};

static const char * const md_img_type_str[] = {
	"invalid",
	"2g",
	"3g",
	"wg",
	"tg",
	"lwg",
	"ltg",
	"sglte",
	"ultg",
	"ulwg",
	"ulwtg",
	"ulwcg",
	"ulwctg",
	"unlwtg",
	"unlwctg",
};


static const char * const md_product_str[] = {
	"INVALID",
	"Debug",
	"Release"
};

static const char *get_md_product_str(unsigned int val)
{
	if (val >= ARRAY_SIZE(md_product_str))
		return md_product_str[0];
	return md_product_str[val];
}

static const char *get_md_cap_str(unsigned int val)
{
	if (val >= ARRAY_SIZE(md_img_type_str))
		return md_img_type_str[0];
	return md_img_type_str[val];
}

/* Fix me, phase out this two function later */
const char *ccci_get_md_product_str(unsigned int val)
{
	return get_md_product_str(val);
}

const char *ccci_get_md_cap_str(unsigned int val)
{
	return get_md_cap_str(val);
}


static unsigned int append_str(unsigned char buf[], unsigned int size, const char *new_sub)
{
	int ret;

	ret = scnprintf((char *)buf, size, "%s", new_sub);

	if (ret > 0)
		return (unsigned int)ret;
	return 0;
}

#define MD_EE_STR_BUFF_SIZE	(512)
int mtk_ccci_compatible_md_chk_hdr_parsing(void)
{
	int ret;
	struct md_chk_hdr_common *head = NULL;
	unsigned char *tmp_buf = NULL;
	unsigned char *hdr_buf = NULL;
	unsigned char *md_ee_str = NULL;
	unsigned int hdr_size;
	unsigned int hdr_ver = 0;
	unsigned int used = 0, new_used;
	unsigned int platform;

	hdr_buf = kmalloc(1024, GFP_KERNEL);
	if (!hdr_buf) {
		CCCI_UTIL_INF_MSG("%s(%d) alloc buff fail\n", __func__, __LINE__);
		goto _free_buf;
	}
	tmp_buf = kmalloc(128, GFP_KERNEL);
	if (!tmp_buf) {
		CCCI_UTIL_INF_MSG("%s(%d) alloc buff fail\n", __func__, __LINE__);
		goto _free_buf;
	}
	md_ee_str = kmalloc(MD_EE_STR_BUFF_SIZE, GFP_KERNEL);
	if (!md_ee_str) {
		CCCI_UTIL_INF_MSG("%s(%d) alloc buff fail\n", __func__, __LINE__);
		goto _free_buf;
	}

	ret = mtk_ccci_find_args_val("md1_chk", hdr_buf, 1024);
	if (ret <= 0) {
		CCCI_UTIL_INF_MSG("%s(%d) find md1_chk fail\n", __func__, __LINE__);
		goto _free_buf;
	}

	hdr_size = (unsigned int)ret;
	head = (struct md_chk_hdr_common *)hdr_buf;

	if (hdr_size == sizeof(struct md_chk_hdr_v3)) /* v3 */
		hdr_ver = 3;
	else if (hdr_size == sizeof(struct md_chk_hdr_v6)) /* v6 */
		hdr_ver = 6;
	else {
		CCCI_UTIL_INF_MSG("%s(%d)Un-supported check header size:%u\n",
				__func__, __LINE__, hdr_size);
		goto _free_buf;
	}

	if (hdr_ver == 3) {
		if ((head->header_verno != 3) && (head->header_verno != 4)) {
			CCCI_UTIL_INF_MSG("%s(%d) Check header version miss-match(%u:%u)\n",
				__func__, __LINE__, hdr_ver, head->header_verno);
			goto _free_buf;
		}
	} else if (hdr_ver == 6) {
		if (head->header_verno != hdr_ver) {
			CCCI_UTIL_INF_MSG("%s(%d) Check header version miss-match(%u:%u)\n",
				__func__, __LINE__, hdr_ver, head->header_verno);
			goto _free_buf;
		}
	}

	if (strncmp(head->check_header, "CHECK_HEADER", 12) != 0) {
		CCCI_UTIL_INF_MSG("%s(%d) Check header string not correct\n", __func__, __LINE__);
		goto _free_buf;
	}

	// MD:
	new_used = append_str(md_ee_str, MD_EE_STR_BUFF_SIZE - used, "MD:");
	used += new_used;

	// MD:ulwtg
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used,
				get_md_cap_str(head->image_type));
	used += new_used;

	// MD:ulwtg*MTxxxx_S00
	memcpy(tmp_buf, head->platform, 16);
	tmp_buf[16] = 0;
	mtk_ccci_add_new_args("md_platform", tmp_buf, 17, FROM_KERNEL);
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, "*");
	used += new_used;
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, tmp_buf);
	used += new_used;

	// MD:ulwtg*MTxxxx_S00*NR16.R1.xxxxx
	memcpy(tmp_buf, head->build_ver, 64);
	tmp_buf[64] = 0;
	mtk_ccci_add_new_args("md_build_version", tmp_buf, 65, FROM_KERNEL);
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, "*");
	used += new_used;
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, tmp_buf);
	used += new_used;

	// MD:ulwtg*MTxxxx_S00*NR16.R1.xxxxx*20xx/xx/xx 00:00*YYYY*ZZZ
	memcpy(tmp_buf, head->build_time, 64);
	tmp_buf[64] = 0;
	mtk_ccci_add_new_args("md_build_time", tmp_buf, 65, FROM_KERNEL);
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, "*");
	used += new_used;
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, tmp_buf);
	used += new_used;

	// MD:ulwtg*MTxxxx_S00*NR16.R1.xxxxx*20xx/xx/xx 00:00*YYYY*ZZZ*Release
	mtk_ccci_add_new_args("md_product_version", (unsigned char *)&head->product_ver, 4
				, FROM_KERNEL);
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, "*");
	used += new_used;
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used,
				get_md_product_str(head->product_ver));
	used += new_used;

	// \nAP:
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, "\nAP:");
	used += new_used;

	//\nAP:ulwtg
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used,
				get_md_cap_str(head->image_type));
	used += new_used;

	//\nAP:ulwtg*MTyyyy
	ret = mtk_ccci_find_args_val("ap_platform", (unsigned char *)&platform, sizeof(platform));
	if (ret)
		scnprintf(tmp_buf, MD_EE_STR_BUFF_SIZE, "*MT%u", platform);
	else
		scnprintf(tmp_buf, MD_EE_STR_BUFF_SIZE, "*MT0000");
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, tmp_buf);
	used += new_used;

	//\nAP:ulwtg*MTyyyy*AAAAAAAAA (MD)BBBBBBBB\n
	mtk_ccci_add_new_args("md_mem_size", (unsigned char *)&head->mem_size, sizeof(int)
				, FROM_KERNEL);
	scnprintf(tmp_buf, 128, "*%08x (MD)%08x\n", head->mem_size, head->mem_size);
	new_used = append_str(&md_ee_str[used], MD_EE_STR_BUFF_SIZE - used, tmp_buf);
	used += new_used;

	mtk_ccci_add_new_args("md_ee_img_inf", md_ee_str, used, FROM_KERNEL);

	if ((unsigned int)head->image_type >= ARRAY_SIZE(md_img_type_str))
		ret = scnprintf((char *)tmp_buf, 128, "err_img_type%d", head->image_type);
	else
		ret = scnprintf((char *)tmp_buf, 128, "1_%s_n", md_img_type_str[head->image_type]);
	mtk_ccci_add_new_args("md_img_cap_str", tmp_buf, ret, FROM_KERNEL);

_free_buf:
	kfree(md_ee_str);
	kfree(tmp_buf);
	kfree(hdr_buf);

	return 0;
}
