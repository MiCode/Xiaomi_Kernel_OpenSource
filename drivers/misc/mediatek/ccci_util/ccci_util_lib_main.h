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
#include <mt-plat/mt_ccci_common.h>

struct md_check_header_v3 {
	unsigned char check_header[12];  /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;	  /* header structure version number */
	unsigned int  product_ver;	   /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type;		/* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16];	  /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];	/* build time string */
	unsigned char build_ver[64];	 /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id;	   /* bind to md sys id, MD SYS1: 1, MD SYS2: 2, MD SYS5: 5 */
	unsigned char ext_attr;		  /* no shrink: 0, shrink: 1 */
	unsigned char reserved[2];	   /* for reserved */

	unsigned int  mem_size;		  /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size;	   /* md image size, exclude head size */
	unsigned int  rpc_sec_mem_addr;  /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;
	unsigned char reserved2[88];

	unsigned int  size;			  /* the size of this structure */
} __packed;

struct md_check_header_v4 {
	unsigned char check_header[12]; /* magic number is "CHECK_HEADER"*/
	unsigned int header_verno; /* header structure version number */
	unsigned int product_ver; /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int image_type; /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16]; /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64]; /* build time string */
	unsigned char build_ver[64]; /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id; /* bind to md sys id, MD SYS1: 1, MD SYS2: 2, MD SYS5: 5 */
	unsigned char ext_attr; /* no shrink: 0, shrink: 1 */
	unsigned char reserved[2]; /* for reserved */

	unsigned int mem_size; /* md ROM/RAM image size requested by md */
	unsigned int md_img_size; /* md image size, exclude head size */
	unsigned int rpc_sec_mem_addr; /* RPC secure memory address */

	unsigned int dsp_img_offset;
	unsigned int dsp_img_size;

	unsigned int region_num; /* total region number */
	struct _md_regin_info region_info[8]; /* max support 8 regions */
	unsigned int domain_attr[4]; /* max support 4 domain settings, each region has 4 control bits*/

	unsigned char reserved2[4];

	unsigned int size; /* the size of this structure */
} __packed;

struct md_check_header_v5 {
	unsigned char check_header[12]; /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno; /* header structure version number */
	unsigned int  product_ver; /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type; /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16]; /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64]; /* build time string */
	unsigned char build_ver[64]; /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id; /* bind to md sys id, MD SYS1: 1, MD SYS2: 2, MD SYS5: 5 */
	unsigned char ext_attr; /* no shrink: 0, shrink: 1 */
	unsigned char reserved[2]; /* for reserved */

	unsigned int  mem_size; /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size; /* md image size, exclude head size */
	unsigned int  rpc_sec_mem_addr; /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;

	unsigned int  region_num; /* total region number */
	struct _md_regin_info region_info[8]; /* max support 8 regions */
	unsigned int  domain_attr[4]; /* max support 4 domain settings, each region has 4 control bits*/

	unsigned int  arm7_img_offset;
	unsigned int  arm7_img_size;

	unsigned char reserved_1[56];


	unsigned int  size; /* the size of this structure */
} __packed;

struct md_check_header_struct {
	unsigned char check_header[12];  /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;	  /* header structure version number */
	unsigned int  product_ver;	   /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type;		/* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16];	  /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];	/* build time string */
	unsigned char build_ver[64];	 /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id;	   /* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	unsigned char ext_attr;		  /* no shrink: 0, shrink: 1*/
	unsigned char reserved[2];	   /* for reserved */

	unsigned int  mem_size;		  /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size;	   /* md image size, exclude head size*/
#if 0 /* no use now, we still use struct */
	union {
		struct {
			unsigned int  reserved_info;	 /* for reserved */
			unsigned int  size;			  /* the size of this structure */
		} v12;
		struct {
			unsigned int  rpc_sec_mem_addr;  /* RPC secure memory address */

			unsigned int  dsp_img_offset;
			unsigned int  dsp_img_size;
			unsigned char reserved2[88];

			unsigned int  size;			  /* the size of this structure */
		} v3;
		struct {
			unsigned int rpc_sec_mem_addr; /* RPC secure memory address */

			unsigned int dsp_img_offset;
			unsigned int dsp_img_size;

			unsigned int region_num; /* total region number */
			struct _md_regin_info region_info[8]; /* max support 8 regions */
			unsigned int domain_attr[4]; /* max support 4 domain settings, each region has 4 control bits*/

			unsigned char reserved2[4];

			unsigned int size; /* the size of this structure */
		} v4;
		struct {
			unsigned int  rpc_sec_mem_addr; /* RPC secure memory address */

			unsigned int  dsp_img_offset;
			unsigned int  dsp_img_size;

			unsigned int  region_num; /* total region number */
			struct _md_regin_info region_info[8]; /* max support 8 regions */
			unsigned int  domain_attr[4]; /* max support 4 domain settings, each region has 4 control bits*/

			unsigned int  arm7_img_offset;
			unsigned int  arm7_img_size;

			unsigned char reserved_1[56];

			unsigned int  size; /* the size of this structure */
		} v5;
	} diff;
#endif
} __packed;

typedef struct _free_padding_block {
	unsigned int start_offset;
	unsigned int length;
} free_padding_block_t;

struct md_check_header_v6 {
	unsigned char check_header[12]; /* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno; /* header structure version number */
	unsigned int  product_ver; /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  image_type; /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned char platform[16]; /* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64]; /* build time string */
	unsigned char build_ver[64]; /* project version, ex:11A_MD.W11.28 */

	unsigned char bind_sys_id; /* bind to md sys id, MD SYS1: 1, MD SYS2: 2, MD SYS5: 5 */
	unsigned char ext_attr; /* no shrink: 0, shrink: 1 */
	unsigned char reserved[2]; /* for reserved */

	unsigned int  mem_size; /* md ROM/RAM image size requested by md */
	unsigned int  md_img_size; /* md image size, exclude head size */
	unsigned int  rpc_sec_mem_addr; /* RPC secure memory address */

	unsigned int  dsp_img_offset;
	unsigned int  dsp_img_size;

	unsigned int  region_num; /* total region number */
	struct _md_regin_info region_info[8];    /* max support 8 regions */
	unsigned int  domain_attr[4];    /* max support 4 domain settings, each region has 4 control bits*/

	unsigned int  arm7_img_offset;
	unsigned int  arm7_img_size;

	free_padding_block_t padding_blk[8];

	unsigned int  ap_md_smem_size;
	unsigned int  md_to_md_smem_size;

	unsigned int ramdisk_offset;
	unsigned int ramdisk_size;

	unsigned char reserved_1[144];

	unsigned int  size; /* the size of this structure */
};

/* Meta parsing section */
#define MD1_EN (1<<0)
#define MD2_EN (1<<1)
#define MD3_EN (1<<2)
#define MD5_EN (1<<4)

#define MD_2G_FLAG    (1<<0)
#define MD_FDD_FLAG   (1<<1)
#define MD_TDD_FLAG   (1<<2)
#define MD_LTE_FLAG   (1<<3)
#define MD_SGLTE_FLAG (1<<4)

#define MD_WG_FLAG  (MD_FDD_FLAG|MD_2G_FLAG)
#define MD_TG_FLAG  (MD_TDD_FLAG|MD_2G_FLAG)
#define MD_LWG_FLAG (MD_LTE_FLAG|MD_FDD_FLAG|MD_2G_FLAG)
#define MD_LTG_FLAG (MD_LTE_FLAG|MD_TDD_FLAG|MD_2G_FLAG)

extern char *ccci_get_ap_platform(void);
extern int ccci_common_sysfs_init(void);
extern void ccci_log_init(void);
extern int ccci_util_fo_init(void);
extern void ccci_timer_for_md_init(void);
extern const char *ld_md_errno_to_str(int errno);
