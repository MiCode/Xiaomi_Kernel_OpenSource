// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/memblock.h>
//#include <asm/memblock.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#endif
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#include <asm/setup.h>
#include <linux/atomic.h>
#include "mt-plat/mtk_ccci_common.h"

//mtk099077: #include <memory-amms.h>
#include "ccci_util_log.h"
#include "ccci_util_lib_main.h"
/*************************************************************************
 **** Local debug option for this file only ******************************
 *************************************************************************
 */
/* #define LK_LOAD_MD_INFO_DEBUG_EN */

#define CCCI_MEM_ALIGN      (SZ_32M)
#define CCCI_SMEM_ALIGN_MD1 (0x200000)	/*2M */
#define CCCI_SMEM_ALIGN_MD2 (0x200000)	/*2M */

/*====================================================== */
/* LK LD MD Tag info support section                     */
/*====================================================== */
#define MAX_LK_INFO_SIZE	(0x10000)
#define CCCI_TAG_NAME_LEN	(16)
#define CCCI_TAG_NAME_LEN_V2	(64)
#define CCCI_LK_INFO_VER_V2	(2)


struct _ccci_lk_info {
	unsigned long long lk_info_base_addr;
	unsigned int       lk_info_size;
	unsigned int       lk_info_tag_num;
};

struct _ccci_lk_info_v2 {
	unsigned long long lk_info_base_addr;
	unsigned int       lk_info_size;
	int                lk_info_err_no;
	int                lk_info_version;
	int                lk_info_tag_num;
	unsigned int       lk_info_ld_flag;
	int                lk_info_ld_md_errno[MAX_MD_NUM_AT_LK];
};

struct _ccci_tag {
	char tag_name[CCCI_TAG_NAME_LEN];
	unsigned int data_offset;
	unsigned int data_size;
	unsigned int next_tag_offset;
};

struct _ccci_tag_v2 {
	char tag_name[CCCI_TAG_NAME_LEN_V2];
	unsigned int data_offset;
	unsigned int data_size;
	unsigned int next_tag_offset;
};


/*====================================================== */
/* Global variable support section                       */
/*====================================================== */
static unsigned int s_g_md_env_rdy_flag;
static unsigned int s_g_md_usage_case;
static unsigned int md_type_at_lk[MAX_MD_NUM_AT_LK];

static unsigned int s_g_lk_load_img_status;
static int s_g_lk_ld_md_errno;
static unsigned int s_g_tag_inf_size;

/* ------ tag info for each modem ---------------------- */
struct _modem_info {
	unsigned long long base_addr;
	unsigned int size;
	char md_id;
	char errno;
	char md_type;
	char ver;
	unsigned int reserved[2];
};

static int lk_load_img_err_no[MAX_MD_NUM_AT_LK];

static void __iomem *s_g_lk_inf_base;
static phys_addr_t s_g_tag_phy_addr;
static unsigned int s_g_tag_cnt;
/* Note, this for tag info solution version */
static unsigned int s_g_lk_info_tag_version;
/* Note, this for feature option solution version */
static int s_g_curr_ccci_fo_version;

static int find_ccci_tag_inf(char *name, char *buf, unsigned int size)
{
	unsigned int i;

	/* For strcmp/strncmp should not be used on device memory,
	 * so prepare a temp buffer.
	 */
	char tag_name[64];
	unsigned int data_offset;
	unsigned int data_size;
	unsigned int next_tag_offset;
	unsigned int tmp_buf;
	int cpy_size;
	char *curr = NULL;
	union u_tag {
		struct _ccci_tag v1;
		struct _ccci_tag_v2 v2;
	} tag;

	if (buf == NULL)
		return -1;

	if (s_g_lk_inf_base == NULL)
		return -2;

	curr = (char *)s_g_lk_inf_base;
	CCCI_UTIL_INF_MSG("------curr tags:%s----------\n", name);
	for (i = 0; i < s_g_tag_cnt; i++) {
		/* 1. Copy tag */
		memcpy_fromio(&tag, curr, sizeof(union u_tag));
		if (s_g_lk_info_tag_version >= CCCI_LK_INFO_VER_V2) {
			scnprintf(tag_name, 64, "%s", tag.v2.tag_name);
			data_offset = tag.v2.data_offset;
			data_size = tag.v2.data_size;
			next_tag_offset = tag.v2.next_tag_offset;
		} else {
			scnprintf(tag_name, 64, "%s", tag.v1.tag_name);
			data_offset = tag.v1.data_offset;
			data_size = tag.v1.data_size;
			next_tag_offset = tag.v1.next_tag_offset;
		}
		memcpy_fromio(&tmp_buf, (void *)(s_g_lk_inf_base + data_offset),
			sizeof(int));

		#ifdef LK_LOAD_MD_INFO_DEBUG_EN
		CCCI_UTIL_INF_MSG("tag->name:%s\n", tag_name);
		CCCI_UTIL_INF_MSG("tag->data_offset:%d\n", data_offset);
		CCCI_UTIL_INF_MSG("tag->data_size:%d\n", data_size);
		CCCI_UTIL_INF_MSG("tag->next_tag_offset:%d\n", next_tag_offset);
		CCCI_UTIL_INF_MSG("tag value:%d\n", tmp_buf);
		#endif

		/* 2. compare tag value. */
		if (strcmp(tag_name, name) != 0) {
			curr = (char *)(s_g_lk_inf_base + next_tag_offset);
			continue;
		}
		/* found it */
		cpy_size = size > data_size?data_size:size;
		memcpy_fromio(buf,
			(void *)(s_g_lk_inf_base + data_offset),
			cpy_size);

		return cpy_size;
	}
	return -1;
}

/*====================================================== */
/* Feature option setting support section                */
/*====================================================== */
/* Feature Option Setting */
struct fos_item {
	char *name;
	int value;
};

/* Default value from config file */

/* array for store default option setting,
 * option value may be updated at init if needed
 */
static struct fos_item ccci_fos_setting[] = {
	{"opt_md1_support", 1},
};

/* ccci option relate public function for option */
int ccci_get_opt_val(char *opt_name)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(ccci_fos_setting); i++) {
		if (strcmp(opt_name, ccci_fos_setting[i].name) == 0) {
			ret = ccci_fos_setting[i].value;
			CCCI_UTIL_INF_MSG("%s:%s->%d\n", __func__,
				opt_name, ret);
			return ret;
		}
	}

	/* not found */
	CCCI_UTIL_INF_MSG("%s:%s->-1\n", __func__, opt_name);
	return -1;
}
EXPORT_SYMBOL(ccci_get_opt_val);

/* ccci option relate private function for option */
static int ccci_update_opt_tbl(const char *opt_name, int val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ccci_fos_setting); i++) {
		if (strcmp(opt_name, ccci_fos_setting[i].name) == 0) {
			ccci_fos_setting[i].value = val;
			return 0;
		}
	}
	return -1;
}

static int ccci_get_opt_tbl_item_num(void)
{
	return ARRAY_SIZE(ccci_fos_setting);
}

static char *ccci_get_opt_name_by_idx(int idx)
{
	if ((idx >= 0) && (idx < ARRAY_SIZE(ccci_fos_setting)))
		return ccci_fos_setting[idx].name;
	return NULL;
}

static void ccci_dump_opt_tbl(void)
{
	char *ccci_name = NULL;
	int ccci_value;
	int i;

	for (i = 0; i < ARRAY_SIZE(ccci_fos_setting); i++) {
		ccci_name = ccci_fos_setting[i].name;
		ccci_value = ccci_fos_setting[i].value;
		CCCI_UTIL_INF_MSG("FO:%s -> %08x\n", ccci_name, ccci_value);
	}
}

static void parse_option_setting_from_lk(void)
{
	int i = 0;
	int val;
	char *name = NULL;
	int using_default = 1;
	int using_lk_setting;
	int opt_list_size = ccci_get_opt_tbl_item_num();

	if (find_ccci_tag_inf("opt_using_lk_val", (char *)&val, sizeof(int))
			!= sizeof(int))
		using_lk_setting = 0;
	else if (val > 0)
		using_lk_setting = 1;
	else
		using_lk_setting = 0;

	if (using_lk_setting) {
		for (i = 0; i < opt_list_size; i++) {
			name = ccci_get_opt_name_by_idx(i);
			if (name == NULL)
				continue;
			if (find_ccci_tag_inf(name, (char *)&val, sizeof(int))
					!= sizeof(int))
				CCCI_UTIL_ERR_MSG("%s using default\n", name);
			else {
				using_default = 0;
				ccci_update_opt_tbl(name, val);
			}
		}
	}

	if (using_default)
		CCCI_UTIL_INF_MSG("All option using default setting\n");
	else {
		CCCI_UTIL_INF_MSG("LK has new setting, Dump final\n");
		ccci_dump_opt_tbl();
	}

	/* Enter here mean's kernel dt not reserve memory */
	/* So, change to using kernel option to deside if modem is enabled */
	val = ccci_get_opt_val("opt_md1_support");
	if (val > 0)
		s_g_md_usage_case |= (1 << MD_SYS1);
}

/*====================================================== */
/* Tag info and device tree parsing section              */
/*====================================================== */
#define CCCI_FO_VER_02			(2) /* For ubin */
#define LK_LOAD_MD_EN			(1<<0)
#define LK_LOAD_MD_ERR_INVALID_MD_ID	(1<<1)
#define LK_LOAD_MD_ERR_NO_MD_LOAD	(1<<2)
#define LK_LOAD_MD_ERR_LK_INFO_FAIL	(1<<3)
#define LK_KERNEL_SETTING_MIS_SYNC	(1<<4)
#define LK_TAG_BUFF_SIZE_NOT_ENOUGH	(1<<5)



/*---- Memory info parsing section --------------------- */
/* MD ROM+RAM */
static unsigned int md_resv_mem_size[MAX_MD_NUM_AT_LK];
/* share memory */
static unsigned int md_resv_smem_size[MAX_MD_NUM_AT_LK];
static unsigned int md_resv_size_list[MAX_MD_NUM_AT_LK];
static unsigned int resv_smem_size;
static unsigned int md1md3_resv_smem_size;

static phys_addr_t md_resv_mem_list[MAX_MD_NUM_AT_LK];
static phys_addr_t md_resv_mem_addr[MAX_MD_NUM_AT_LK];
static phys_addr_t md_resv_smem_addr[MAX_MD_NUM_AT_LK];
static phys_addr_t resv_smem_addr;
static phys_addr_t md1md3_resv_smem_addr;

struct _smem_layout {
	unsigned long long base_addr;
	unsigned int ap_md1_smem_offset;
	unsigned int ap_md1_smem_size;
	unsigned int ap_md3_smem_offset;
	unsigned int ap_md3_smem_size;
	unsigned int md1_md3_smem_offset;
	unsigned int md1_md3_smem_size;
	unsigned int total_smem_size;
};

struct _ccb_layout {
	unsigned long long ccb_data_buffer_addr;
	unsigned int ccb_data_buffer_size;
};
static struct _ccb_layout ccb_info;
static unsigned int md1_phy_cap_size;
static int md1_smem_dfd_size = -1;
static int smem_amms_pos_size = -1;
static int smem_align_padding_size = -1;
static unsigned int md1_bank4_cache_offset;

struct _udc_info {
	unsigned int noncache_size;
	unsigned int cache_size;
};
static struct _udc_info udc_size;

/* non-cacheable share memory */
struct nc_smem_node {
	unsigned int ap_offset;
	unsigned int md_offset;
	unsigned int size;
	unsigned int id;
};
static struct nc_smem_node *s_nc_layout;
static unsigned int s_nc_smem_ext_num;

/* cacheable share memory */
struct _csmem_item {
	unsigned long long csmem_buffer_addr;
	unsigned int md_offset;
	unsigned int csmem_buffer_size;
	unsigned int item_cnt;
};
static struct _csmem_item csmem_info;
static struct _csmem_item *csmem_layout;

struct _sib_item {
	unsigned long long md1_sib_addr;
	unsigned int md1_sib_size;
};

static struct _sib_item sib_info;

static unsigned int md_mtee_support;

static void nc_smem_info_parsing(void)
{
	unsigned int size, num = 0, i;

	if (find_ccci_tag_inf("nc_smem_info_ext_num", (char *)&num,
		sizeof(unsigned int)) != sizeof(unsigned int)) {
		CCCI_UTIL_ERR_MSG("nc_smem_info_ext_num get fail\n");
		s_nc_smem_ext_num = 0;
		return;
	}

	s_nc_smem_ext_num = num;
	size = num * sizeof(struct nc_smem_node);
	s_nc_layout = kzalloc(size, GFP_KERNEL);
	if (s_nc_layout == NULL) {
		CCCI_UTIL_ERR_MSG("nc_layout:alloc nc_layout fail\n");
		return;
	}

	if (find_ccci_tag_inf("nc_smem_info_ext", (char *)s_nc_layout,
		size) != size) {
		CCCI_UTIL_ERR_MSG("Invalid nc_layout from tag\n");
		return;
	}

	for (i = 0; i < num; i++) {
		CCCI_UTIL_INF_MSG("nc_smem<%d>: ap:0x%08x md:0x%08x[0x%08x]\n",
			s_nc_layout[i].id, s_nc_layout[i].ap_offset,
			s_nc_layout[i].md_offset, s_nc_layout[i].size);
	}

	/* For compatible of legacy design */
	/* DFD part */
	if (get_nc_smem_region_info(SMEM_USER_RAW_DFD, NULL, NULL,
					(unsigned int *)&md1_smem_dfd_size))
		CCCI_UTIL_INF_MSG("change dfd to: 0x%x\n", md1_smem_dfd_size);
	/* AMMS POS part */
	if (get_nc_smem_region_info(SMEM_USER_RAW_AMMS_POS, NULL, NULL,
					(unsigned int *)&smem_amms_pos_size))
		CCCI_UTIL_INF_MSG("change POS to: 0x%x\n", smem_amms_pos_size);
}


int get_nc_smem_region_info(unsigned int id, unsigned int *ap_off,
				unsigned int *md_off, unsigned int *size)
{
	int i;

	if (s_nc_layout == NULL || s_nc_smem_ext_num == 0)
		return 0;

	for (i = 0; i < s_nc_smem_ext_num; i++) {
		if (s_nc_layout[i].id == id) {
			if (ap_off)
				*ap_off = s_nc_layout[i].ap_offset;
			if (md_off)
				*md_off = s_nc_layout[i].md_offset;
			if (size)
				*size = s_nc_layout[i].size;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(get_nc_smem_region_info);

static void cshare_memory_info_parsing(void)
{
	unsigned int size = 0;

	memset(&csmem_info, 0, sizeof(struct _csmem_item));
	if (find_ccci_tag_inf("md1_bank4_cache_info", (char *)&csmem_info,
		sizeof(struct _csmem_item)) != sizeof(struct _csmem_item)) {
		CCCI_UTIL_ERR_MSG("Invalid csmem_info dt para\n");
		if (ccb_info.ccb_data_buffer_addr != 0)
			goto OLD_LK_CSMEM;
		return;
	}
	size = csmem_info.item_cnt * sizeof(struct _csmem_item);
	csmem_layout = kzalloc(size, GFP_KERNEL);
	if (csmem_layout == NULL) {
		CCCI_UTIL_ERR_MSG("csmem_layout:alloc csmem_layout fail\n");
		return;
	}
	if (find_ccci_tag_inf("md1_bank4_cache_layout", (char *)csmem_layout,
		size) != size) {
		CCCI_UTIL_ERR_MSG("Invalid csmem_layout dt para\n");
		return;
	}
	return;

OLD_LK_CSMEM:
	/* old LK config compatibility: create ccb item. */
	csmem_info.csmem_buffer_addr = ccb_info.ccb_data_buffer_addr;
	csmem_info.csmem_buffer_size = ccb_info.ccb_data_buffer_size;
	csmem_info.item_cnt = 1;

	size = csmem_info.item_cnt * sizeof(struct _csmem_item);
	csmem_layout = kzalloc(size, GFP_KERNEL);
	if (csmem_layout == NULL) {
		CCCI_UTIL_ERR_MSG("csmem_layout:alloc csmem_layout fail\n");
		return;
	}
	csmem_layout[0].csmem_buffer_addr = csmem_info.csmem_buffer_addr;
	csmem_layout[0].csmem_buffer_size = csmem_info.csmem_buffer_size;
	csmem_layout[0].md_offset = 0;
	csmem_layout[0].item_cnt = SMEM_USER_CCB_START;
	CCCI_UTIL_INF_MSG("ccci_util get csmem: data:%llx data_size:%d\n",
		csmem_info.csmem_buffer_addr,
		csmem_info.csmem_buffer_size);
}

static void share_memory_info_parsing(void)
{
	struct _smem_layout smem_layout;
	/* Get share memory layout */
	if (find_ccci_tag_inf("smem_layout",
						  (char *)&smem_layout,
						  sizeof(struct _smem_layout))
			!= sizeof(struct _smem_layout)) {
		CCCI_UTIL_ERR_MSG("load smem layout fail\n");
		s_g_lk_load_img_status |= LK_LOAD_MD_ERR_LK_INFO_FAIL;
		/* Reset to zero if get share memory info fail */
		s_g_md_env_rdy_flag = 0;
		return;
	}

	/* Get ccb memory layout */
	memset(&ccb_info, 0, sizeof(struct _ccb_layout));
	if (find_ccci_tag_inf("ccb_info",
						  (char *)&ccb_info,
						  sizeof(struct _ccb_layout))
			!= sizeof(struct _ccb_layout)) {
		CCCI_UTIL_ERR_MSG("Invalid ccb info dt para\n");
	}

	CCCI_UTIL_INF_MSG("ccci_util get ccb: data:%llx data_size:%d\n",
			ccb_info.ccb_data_buffer_addr,
			ccb_info.ccb_data_buffer_size);

	/* Get udc cache&noncache size */
	memset(&udc_size, 0, sizeof(struct _udc_info));
	if (find_ccci_tag_inf("udc_layout", (char *)&udc_size,
		sizeof(struct _udc_info)) != sizeof(struct _udc_info))
		CCCI_UTIL_ERR_MSG("Invalid udc layout info dt para\n");

	CCCI_UTIL_INF_MSG(
		"ccci_util get udc: cache_size:0x%x noncache_size:0x%x\n",
		udc_size.cache_size, udc_size.noncache_size);

	/*Get sib info */
	if (find_ccci_tag_inf("md1_sib_info", (char *)&sib_info,
		sizeof(sib_info)) != sizeof(sib_info))
		CCCI_UTIL_ERR_MSG("get sib info fail\n");

	CCCI_UTIL_INF_MSG("ccci_util get sib addr: 0x%llx size: %d\n",
			sib_info.md1_sib_addr, sib_info.md1_sib_size);

	/* Get md1_phy_cap_size  */
	if (find_ccci_tag_inf("md1_phy_cap",  (char *)&md1_phy_cap_size,
		sizeof(md1_phy_cap_size)) != sizeof(md1_phy_cap_size))
		CCCI_UTIL_ERR_MSG("using 0 as phy capture size\n");

	CCCI_UTIL_INF_MSG("ccci_util get md1_phy_cap_size: 0x%x\n",
		md1_phy_cap_size);

	/* Get md1_smem_dfd_size  */
	if (find_ccci_tag_inf("smem_dfd_size", (char *)&md1_smem_dfd_size,
		sizeof(md1_smem_dfd_size)) != sizeof(md1_smem_dfd_size))
		CCCI_UTIL_ERR_MSG("get smem dfd size fail\n");

	CCCI_UTIL_INF_MSG("ccci_util get md1_smem_dfd_size: %d\n",
		md1_smem_dfd_size);

	/* Get smem_amms_pos_size  */
	if (find_ccci_tag_inf("smem_amms_pos_size",
		(char *)&smem_amms_pos_size,
		sizeof(smem_amms_pos_size)) != sizeof(smem_amms_pos_size))
		CCCI_UTIL_ERR_MSG("get smem amms pos size fail\n");

	CCCI_UTIL_INF_MSG("ccci_util get smem_amms_pos_size: %d\n",
		smem_amms_pos_size);

	/* Get smem_align_padding_size  */
	if (find_ccci_tag_inf("smem_align_padding_size",
		(char *)&smem_align_padding_size,
		sizeof(smem_align_padding_size)) !=
			sizeof(smem_align_padding_size))
		CCCI_UTIL_ERR_MSG("get smem align padding size fail\n");

	CCCI_UTIL_INF_MSG("ccci_util get smem_align_padding_size: %d\n",
		smem_align_padding_size);

	/* Get smem cachable offset  */
	md1_bank4_cache_offset = 0;
	if (find_ccci_tag_inf("md1_smem_cahce_offset",
			(char *)&md1_bank4_cache_offset,
			sizeof(md1_bank4_cache_offset))
			!= sizeof(md1_bank4_cache_offset))
		/* Using 128MB offset as default */
		md1_bank4_cache_offset = 0x8000000;
	CCCI_UTIL_INF_MSG("smem cachable offset 0x%X\n",
				md1_bank4_cache_offset);
	/* MD*_SMEM_SIZE */
	md_resv_smem_size[MD_SYS1] = smem_layout.ap_md1_smem_size;
	md_resv_smem_size[MD_SYS3] = smem_layout.ap_md3_smem_size;

	/* MD1MD3_SMEM_SIZE*/
	md1md3_resv_smem_size = smem_layout.md1_md3_smem_size;

	/* MD Share memory layout */
	/*   AP    <-->   MD1     */
	/*   MD1   <-->   MD3     */
	/*   AP    <-->   MD3     */
	md_resv_smem_addr[MD_SYS1] = (phys_addr_t)(smem_layout.base_addr +
		(unsigned long long)smem_layout.ap_md1_smem_offset);
	md1md3_resv_smem_addr = (phys_addr_t)(smem_layout.base_addr +
		(unsigned long long)smem_layout.md1_md3_smem_offset);
	md_resv_smem_addr[MD_SYS3] = (phys_addr_t)(smem_layout.base_addr +
		(unsigned long long)smem_layout.ap_md3_smem_offset);
	CCCI_UTIL_INF_MSG("AP  <--> MD1 SMEM(0x%08X):%016llx~%016llx\n",
			md_resv_smem_size[MD_SYS1],
			(unsigned long long)md_resv_smem_addr[MD_SYS1],
			(unsigned long long)(md_resv_smem_addr[MD_SYS1]
			+ md_resv_smem_size[MD_SYS1]-1));
	CCCI_UTIL_INF_MSG("MD1 <--> MD3 SMEM(0x%08X):%016llx~%016llx\n",
			md1md3_resv_smem_size,
			(unsigned long long)md1md3_resv_smem_addr,
			(unsigned long long)(md1md3_resv_smem_addr
			+ md1md3_resv_smem_size-1));
	CCCI_UTIL_INF_MSG("AP  <--> MD3 SMEM(0x%08X):%016llx~%016llx\n",
			md_resv_smem_size[MD_SYS3],
			(unsigned long long)md_resv_smem_addr[MD_SYS3],
			(unsigned long long)(md_resv_smem_addr[MD_SYS3]
			+ md_resv_smem_size[MD_SYS3]-1));
#ifdef CONFIG_MTK_DCS
	if (md_resv_smem_size[MD_SYS1])
		dcs_set_lbw_region(md_resv_smem_addr[MD_SYS1],
				(md_resv_smem_addr[MD_SYS1] +
				 md_resv_smem_size[MD_SYS1]));
	if (md1md3_resv_smem_size)
		dcs_set_lbw_region(md1md3_resv_smem_addr,
				(md1md3_resv_smem_addr +
				 md1md3_resv_smem_size));
	if (md_resv_smem_size[MD_SYS3])
		dcs_set_lbw_region(md_resv_smem_addr[MD_SYS3],
				(md_resv_smem_addr[MD_SYS3] +
				 md_resv_smem_size[MD_SYS3]));
#endif
	if (find_ccci_tag_inf("mtee_support", (char *)&md_mtee_support,
		sizeof(md_mtee_support)) != sizeof(md_mtee_support))
		CCCI_UTIL_ERR_MSG("using 0 as MTEE support\n");
	else
		CCCI_UTIL_INF_MSG("MTEE support: 0x%x\n", md_mtee_support);

	nc_smem_info_parsing();

	cshare_memory_info_parsing();
{
	int i;

	for (i = 0; i < csmem_info.item_cnt; i++) {
		CCCI_UTIL_INF_MSG(
			"csmem_region[%d][%d]: data_offset:%x data_size:%d\n",
			i, csmem_layout[i].item_cnt,
			csmem_layout[i].md_offset,
			csmem_layout[i].csmem_buffer_size);
	}
}
}

static void md_mem_info_parsing(void)
{
	struct _modem_info md_inf[4];
	struct _modem_info *curr = NULL;
	int md_num = 0;
	int md_id;

	if (find_ccci_tag_inf("hdr_count",
						  (char *)&md_num,
						  sizeof(int)) != sizeof(int)) {
		CCCI_UTIL_ERR_MSG("get hdr_count fail\n");
		s_g_lk_load_img_status |= LK_LOAD_MD_ERR_NO_MD_LOAD;
		return;
	}

	find_ccci_tag_inf("hdr_tbl_inf", (char *)md_inf, sizeof(md_inf));
	CCCI_UTIL_INF_MSG("md_num:%d\n", md_num);
	curr = md_inf;

	/* MD ROM and RW part */
	while (md_num--) {
		#ifdef LK_LOAD_MD_INFO_DEBUG_EN
		CCCI_UTIL_INF_MSG("===== Dump modem memory info (%d)=====\n",
			(int)sizeof(struct _modem_info));
		CCCI_UTIL_INF_MSG("base address : 0x%llX\n", curr->base_addr);
		CCCI_UTIL_INF_MSG("memory size  : 0x%08X\n", curr->size);
		CCCI_UTIL_INF_MSG("md id        : %d\n", (int)curr->md_id);
		CCCI_UTIL_INF_MSG("ver          : %d\n", (int)curr->ver);
		CCCI_UTIL_INF_MSG("type         : %d\n", (int)curr->md_type);
		CCCI_UTIL_INF_MSG("errno        : %d\n", (int)curr->errno);
		CCCI_UTIL_INF_MSG("=============================\n");
		#endif
		md_id = (int)curr->md_id;
		if ((md_id < MAX_MD_NUM_AT_LK)
				&& (md_resv_mem_size[md_id] == 0)) {
			md_resv_mem_size[md_id] = curr->size;
			md_resv_mem_addr[md_id] = (phys_addr_t)curr->base_addr;
			if (curr->errno & 0x80)
				/*signed extension */
				lk_load_img_err_no[md_id]
					= ((int)curr->errno) | 0xFFFFFF00;
			else
				lk_load_img_err_no[md_id] = (int)curr->errno;

			CCCI_UTIL_INF_MSG("md%d lk_load_img_err_no: %d\n",
				md_id+1, lk_load_img_err_no[md_id]);

			if (lk_load_img_err_no[md_id] == 0)
				s_g_md_env_rdy_flag |= 1<<md_id;
			md_type_at_lk[md_id] = (int)curr->md_type;
			CCCI_UTIL_INF_MSG(
				"md%d MemStart: 0x%016llx, MemSize:0x%08X\n",
				md_id+1,
				(unsigned long long)md_resv_mem_addr[md_id],
				md_resv_mem_size[md_id]);
#ifdef CONFIG_MTK_DCS
			if (md_resv_mem_size[md_id])
				dcs_set_lbw_region(md_resv_mem_addr[md_id],
						(md_resv_mem_addr[md_id] +
						md_resv_mem_size[md_id]));
#endif
		} else {
			CCCI_UTIL_ERR_MSG("Invalid dt para, id(%d)\n", md_id);
			s_g_lk_load_img_status |= LK_LOAD_MD_ERR_INVALID_MD_ID;
		}
		curr++;
	}
}

/*---- Modem check header section --------------------- */
static char *md1_check_hdr_info;
static char *md3_check_hdr_info;
static int md1_check_hdr_info_size;
static int md3_check_hdr_info_size;

static int md1_raw_img_size;
static int md3_raw_img_size;


void __weak *vmap_reserved_mem(phys_addr_t start,
		phys_addr_t size, pgprot_t prot)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return NULL;
}

void __iomem *ccci_map_phy_addr(phys_addr_t phy_addr, unsigned int size)
{
	void __iomem *map_addr = NULL;
	pgprot_t prot;

	phy_addr &= PAGE_MASK;
	if (!pfn_valid(__phys_to_pfn(phy_addr))) {
		map_addr = ioremap_wc(phy_addr, size);
		CCCI_UTIL_INF_MSG(
			"ioremap_wc: (%lx %px %d)\n",
			(unsigned long)phy_addr, map_addr, size);
	} else {
		prot = pgprot_writecombine(PAGE_KERNEL);
		map_addr = (void __iomem *)vmap_reserved_mem(
			phy_addr, size, prot);
		CCCI_UTIL_INF_MSG(
			"vmap_reserved_mem: (%lx %px %d)\n",
			(unsigned long)phy_addr, map_addr, size);
	}
	return map_addr;
}
EXPORT_SYMBOL(ccci_map_phy_addr);

static void md_chk_hdr_info_parse(void)
{
	int ret;

	if (s_g_md_usage_case & (1<<MD_SYS1)) {
		/* The allocated memory will be
		 * free after md structure initialized
		 */
		md1_check_hdr_info = kmalloc(1024, GFP_KERNEL);
		if (md1_check_hdr_info == NULL) {
			CCCI_UTIL_ERR_MSG(
			"alloc check header memory fail(MD1)\n");
			s_g_md_env_rdy_flag &= ~(1<<MD_SYS1);
			goto _check_md3;
		}
		ret = find_ccci_tag_inf("md1_chk", md1_check_hdr_info, 1024);
		if ((ret != sizeof(struct md_check_header_v5))
				&& (ret != sizeof(struct md_check_header_v6))
			&& (ret != sizeof(struct md_check_header))) {
			CCCI_UTIL_ERR_MSG("get md1 chk header info fail\n");
			s_g_lk_load_img_status |= LK_LOAD_MD_ERR_LK_INFO_FAIL;
			s_g_md_env_rdy_flag &= ~(1<<MD_SYS1);
			goto _check_md3;
		}
		md1_check_hdr_info_size = ret;

		/* Get MD1 raw image size */
		find_ccci_tag_inf("md1img",
				(char *)&md1_raw_img_size, sizeof(int));
	}
_check_md3:

	if (s_g_md_usage_case & (1<<MD_SYS3)) {
		/* The allocated memory will be
		 * free after md structure initialized
		 */
		md3_check_hdr_info =
			kmalloc(sizeof(struct md_check_header), GFP_KERNEL);
		if (md3_check_hdr_info == NULL) {
			CCCI_UTIL_ERR_MSG(
			"alloc check header memory fail(MD3)\n");
			s_g_md_env_rdy_flag &= ~(1<<MD_SYS3);
			return;
		}
		if (find_ccci_tag_inf("md3_chk", md3_check_hdr_info,
				sizeof(struct md_check_header))
			!= sizeof(struct md_check_header)) {
			CCCI_UTIL_ERR_MSG("get md3 chk header info fail\n");
			s_g_lk_load_img_status |= LK_LOAD_MD_ERR_LK_INFO_FAIL;
			s_g_md_env_rdy_flag &= ~(1<<MD_SYS3);
			return;
		}
		md3_check_hdr_info_size = sizeof(struct md_check_header);

		/* Get MD3 raw image size */
		find_ccci_tag_inf("md3img",
			(char *)&md3_raw_img_size, sizeof(int));
	}
}

static void lk_info_parsing_v1(unsigned int *raw_ptr)
{
	struct _ccci_lk_info lk_inf;

	memcpy((void *)&lk_inf, raw_ptr, sizeof(struct _ccci_lk_info));

	CCCI_UTIL_INF_MSG("lk info.lk_info_base_addr: 0x%llX\n",
		lk_inf.lk_info_base_addr);
	CCCI_UTIL_INF_MSG("lk info.lk_info_size:      0x%x\n",
		lk_inf.lk_info_size);
	CCCI_UTIL_INF_MSG("lk info.lk_info_tag_num:   0x%x\n",
		lk_inf.lk_info_tag_num);
	s_g_tag_inf_size = lk_inf.lk_info_size;

	if (lk_inf.lk_info_base_addr == 0LL) {
		CCCI_UTIL_ERR_MSG("no image load success\n");
		s_g_lk_load_img_status |= LK_LOAD_MD_ERR_NO_MD_LOAD;
		return;
	}

	if (lk_inf.lk_info_size > MAX_LK_INFO_SIZE) {
		CCCI_UTIL_ERR_MSG("tag info mem size too large\n");
		s_g_lk_load_img_status |= LK_TAG_BUFF_SIZE_NOT_ENOUGH;
		return;
	}

	s_g_lk_info_tag_version = 1;
	s_g_tag_cnt = (unsigned int)lk_inf.lk_info_tag_num;

	s_g_lk_inf_base = ccci_map_phy_addr(
		(phys_addr_t)lk_inf.lk_info_base_addr, MAX_LK_INFO_SIZE);

	if (s_g_lk_inf_base == NULL) {
		CCCI_UTIL_ERR_MSG("remap lk info buf fail\n");
		s_g_lk_load_img_status |= LK_LOAD_MD_ERR_NO_MD_LOAD;
	}
}

static int lk_info_parsing_v2(unsigned int *raw_ptr)
{
	struct _ccci_lk_info_v2 lk_inf;
	int i;

	memcpy((void *)&lk_inf, raw_ptr, sizeof(struct _ccci_lk_info_v2));

	CCCI_UTIL_INF_MSG("lk info.lk_info_base_addr: 0x%llX\n",
		lk_inf.lk_info_base_addr);
	CCCI_UTIL_INF_MSG("lk info.lk_info_size:      0x%x\n",
		lk_inf.lk_info_size);
	CCCI_UTIL_INF_MSG("lk info.lk_info_tag_num:   0x%x\n",
		lk_inf.lk_info_tag_num);

	s_g_lk_ld_md_errno = lk_inf.lk_info_err_no;
	s_g_tag_inf_size = lk_inf.lk_info_size;
	for (i = 0; i < MAX_MD_NUM_AT_LK; i++)
		lk_load_img_err_no[i] = lk_inf.lk_info_ld_md_errno[i];

	if ((lk_inf.lk_info_base_addr == 0LL) && (s_g_lk_ld_md_errno == 0)) {
		CCCI_UTIL_ERR_MSG("no image enabled\n");
		s_g_lk_inf_base = NULL;
		s_g_lk_load_img_status = 0;
		return 1;
	}

	if (lk_inf.lk_info_base_addr == 0LL) {
		CCCI_UTIL_ERR_MSG("no image load success\n");
		s_g_lk_load_img_status |= LK_LOAD_MD_ERR_NO_MD_LOAD;
		return -1;
	}

	if (lk_inf.lk_info_size > MAX_LK_INFO_SIZE) {
		CCCI_UTIL_ERR_MSG("tag info mem size too large\n");
		s_g_lk_load_img_status |= LK_TAG_BUFF_SIZE_NOT_ENOUGH;
		return -1;
	}

	s_g_tag_phy_addr = (phys_addr_t)lk_inf.lk_info_base_addr;
	s_g_lk_info_tag_version = (unsigned int)lk_inf.lk_info_version;
	s_g_tag_cnt = (unsigned int)lk_inf.lk_info_tag_num;

	s_g_lk_inf_base = ccci_map_phy_addr(
		(phys_addr_t)lk_inf.lk_info_base_addr, MAX_LK_INFO_SIZE);

	if (s_g_lk_inf_base == NULL) {
		CCCI_UTIL_ERR_MSG("remap lk info buf fail\n");
		s_g_lk_load_img_status |= LK_LOAD_MD_ERR_NO_MD_LOAD;
	}

	return 0;
}

static void verify_md_enable_setting(void)
{
	/* Show warning if has some error */
	/* MD1 part */
	if ((s_g_md_usage_case & (1<<MD_SYS1))
			&& (!(s_g_md_env_rdy_flag & (1<<MD_SYS1)))) {
		CCCI_UTIL_ERR_MSG(
		"md1 env prepare abnormal, disable this modem\n");
		s_g_md_usage_case &= ~(1<<MD_SYS1);
	} else if ((!(s_g_md_usage_case & (1<<MD_SYS1)))
		&& (s_g_md_env_rdy_flag & (1<<MD_SYS1))) {
		CCCI_UTIL_ERR_MSG("md1: kernel dis, but lk en\n");
		s_g_md_usage_case &= ~(1<<MD_SYS1);
		s_g_lk_load_img_status |= LK_KERNEL_SETTING_MIS_SYNC;
	} else if ((!(s_g_md_usage_case & (1<<MD_SYS1)))
		&& (!(s_g_md_env_rdy_flag & (1<<MD_SYS1)))) {
		CCCI_UTIL_INF_MSG("md1: both lk and kernel dis\n");
		s_g_md_usage_case &= ~(1<<MD_SYS1);
		/* For this case, clear error */
		lk_load_img_err_no[MD_SYS1] = 0;
	}
	/* MD3 part */
	if ((s_g_md_usage_case & (1<<MD_SYS3))
		&& (!(s_g_md_env_rdy_flag & (1<<MD_SYS3)))) {
		CCCI_UTIL_ERR_MSG(
		"md3 env prepare abnormal, disable this modem\n");
		s_g_md_usage_case &= ~(1<<MD_SYS3);
	} else if ((!(s_g_md_usage_case & (1<<MD_SYS3)))
		&& (s_g_md_env_rdy_flag & (1<<MD_SYS3))) {
		CCCI_UTIL_ERR_MSG("md3: kernel dis, but lk en\n");
		s_g_md_usage_case &= ~(1<<MD_SYS3);
		s_g_lk_load_img_status |= LK_KERNEL_SETTING_MIS_SYNC;
	} else if ((!(s_g_md_usage_case & (1<<MD_SYS3)))
		&& (!(s_g_md_env_rdy_flag & (1<<MD_SYS3)))) {
		CCCI_UTIL_INF_MSG("md3: both lk and kernel dis\n");
		s_g_md_usage_case &= ~(1<<MD_SYS3);
		/* For this case, clear error */
		lk_load_img_err_no[MD_SYS3] = 0;
	}
}

static struct _mpu_cfg *s_g_md_mpu_cfg_list;
static int s_g_mpu_info_num;
static void parse_mpu_setting(void)
{
	int size;

	/* Get MD MPU config info */
	find_ccci_tag_inf("md_mpu_num",
		(char *)&s_g_mpu_info_num, (int)sizeof(int));
	if (s_g_mpu_info_num) {
		size = ((int)sizeof(struct _mpu_cfg))*s_g_mpu_info_num;
		s_g_md_mpu_cfg_list = kmalloc(size, GFP_KERNEL);
		if (s_g_md_mpu_cfg_list)
			find_ccci_tag_inf("md_mpu_inf",
				(char *)s_g_md_mpu_cfg_list, size);
	}
}

int __weak free_reserved_memory(phys_addr_t start_phys, phys_addr_t end_phys)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

static void dump_retrieve_info(void)
{
	int retrieve_num, i;
	u64 array[2], md1_mem_addr;
	char buf[32];
	int ret = 0;
	int free_in_kernel = -1;

	md1_mem_addr =  md_resv_mem_addr[MD_SYS1];

	if (find_ccci_tag_inf("retrieve_num",
			(char *)&retrieve_num, (int)sizeof(int)) < 0) {
		CCCI_UTIL_ERR_MSG("get retrieve_num failed.\n");
		return;
	}

	CCCI_UTIL_INF_MSG("retrieve number is %d.\n", retrieve_num);

	for (i = 0; i < retrieve_num; i++) {
		scnprintf(buf, 32, "retrieve%d", i);
		if (find_ccci_tag_inf(buf,
				(char *)&array, sizeof(array))) {
			CCCI_UTIL_INF_MSG(
				"AP view(0x%llx ~ 0x%llx), MD view(0x%llx ~ 0x%llx)\n",
				array[0], array[0] + array[1],
				array[0] - md1_mem_addr,
				array[0] + array[1] - md1_mem_addr);

			if (find_ccci_tag_inf("free_in_kernel",
					(char *)&free_in_kernel, sizeof(int))
					&& free_in_kernel == 1) {
				ret = free_reserved_memory(array[0],
						array[0] + array[1]);
				CCCI_UTIL_INF_MSG(
				"free_reserved_memory result=%d\n",
				ret);
			} else {
				CCCI_UTIL_INF_MSG(
					"no free_in_kernel found, free_in_kernel=%d\n",
					free_in_kernel);
			}
		}
	}
}

static int __init collect_lk_boot_arguments(void)
{
	/* Device tree method */
	struct device_node *node = NULL;
	int ret;
	unsigned int *raw_ptr;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (!node) {
		CCCI_UTIL_INF_MSG("device node no mediatek,mddriver node\n");
		return -1;
	}

	raw_ptr = (unsigned int *)of_get_property(node, "ccci,modem_info_v2",
			NULL);
	if (raw_ptr != NULL) {
		if (lk_info_parsing_v2(raw_ptr) == 1) /* No md enabled in LK */
			return 0;
		goto _common_process;
	}

	CCCI_UTIL_INF_MSG("ccci,modem_info_v2 not found, try v1\n");
	raw_ptr = (unsigned int *)of_get_property(node, "ccci,modem_info",
			NULL);
	if (raw_ptr != NULL) {
		lk_info_parsing_v1(raw_ptr);
		goto _common_process;
	}

	CCCI_UTIL_INF_MSG("ccci,modem_info_v1 still not found, using v0!!!\n");
	return -1;

_common_process:
	parse_option_setting_from_lk();
	parse_mpu_setting();
	md_mem_info_parsing();
	dump_retrieve_info();
	md_chk_hdr_info_parse();
	share_memory_info_parsing();
	verify_md_enable_setting();

	s_g_lk_load_img_status |= LK_LOAD_MD_EN;
	s_g_curr_ccci_fo_version = CCCI_FO_VER_02;

	if (s_g_lk_inf_base && s_g_lk_info_tag_version < 3) {
		/* clear memory to zero that used by tag info. */
		memset_io(s_g_lk_inf_base, 0, s_g_tag_inf_size);
		iounmap(s_g_lk_inf_base);
	} else if (s_g_lk_info_tag_version >= 3) {
		if (!pfn_valid(__phys_to_pfn(s_g_tag_phy_addr))) {
			iounmap(s_g_lk_inf_base);
		} else {
			vunmap(s_g_lk_inf_base);
			ret = free_reserved_memory(s_g_tag_phy_addr,
				s_g_tag_phy_addr + MAX_LK_INFO_SIZE);
			CCCI_UTIL_INF_MSG(
				"unmap && free reserved tag result=%d\n", ret);
		}
	}

	return 0;
}

/* functions will be called by external */
int get_lk_load_md_info(char buf[], int size)
{
	int i = 0;
	int has_write = 0;
	int count = 0;

	if (s_g_lk_load_img_status & LK_LOAD_MD_EN)
		has_write = scnprintf(buf, size,
			"LK Load MD:[Enabled](0x%08x)\n",
			s_g_lk_load_img_status);
	else {
		has_write = scnprintf(buf, size,
			"LK Load MD:[Disabled](0x%08x)\n",
			s_g_lk_load_img_status);
		return has_write;
	}

	if ((s_g_lk_load_img_status & (~0x1)) == 0) {
		has_write += scnprintf(&buf[has_write], size - has_write,
						"LK load MD success!\n");
		return has_write;
	}

	has_write += scnprintf(&buf[has_write], size - has_write,
					"LK load MD has error:\n");
	has_write += scnprintf(&buf[has_write], size - has_write,
					"---- More details ----------------\n");
	if (s_g_lk_load_img_status & LK_LOAD_MD_ERR_INVALID_MD_ID)
		has_write += scnprintf(&buf[has_write], size - has_write,
					"Err: Got invalid md id(%d:%s)\n",
					s_g_lk_ld_md_errno,
					ld_md_errno_to_str(s_g_lk_ld_md_errno));
	else if (s_g_lk_load_img_status & LK_LOAD_MD_ERR_NO_MD_LOAD)
		has_write += scnprintf(&buf[has_write], size - has_write,
					"Err: No valid md image(%d:%s)\n",
					s_g_lk_ld_md_errno,
					ld_md_errno_to_str(s_g_lk_ld_md_errno));
	else if (s_g_lk_load_img_status & LK_LOAD_MD_ERR_LK_INFO_FAIL)
		has_write += scnprintf(&buf[has_write], size - has_write,
					"Err: Got lk info fail(%d:%s)\n",
					s_g_lk_ld_md_errno,
					ld_md_errno_to_str(s_g_lk_ld_md_errno));
	else if (s_g_lk_load_img_status & LK_KERNEL_SETTING_MIS_SYNC)
		has_write += scnprintf(&buf[has_write], size - has_write,
					"Err: lk kernel setting mis sync\n");

	has_write += scnprintf(&buf[has_write], size - has_write,
			"ERR> 1:[%d] 2:[%d] 3:[%d] 4:[%d]\n",
			lk_load_img_err_no[0], lk_load_img_err_no[1],
			lk_load_img_err_no[2], lk_load_img_err_no[3]);

	for (i = 0; i < MAX_MD_NUM_AT_LK; i++) {
		if (lk_load_img_err_no[i] == 0)
			continue;
		count = scnprintf(&buf[has_write], size - has_write,
			"hint for MD%d: %s\n",
			i+1, ld_md_errno_to_str(lk_load_img_err_no[i]));
		if (count <= 0 || count >= size - has_write) {
			CCCI_UTIL_INF_MSG("%s: scnprintf hint for MD fail\n",
				__func__);
			buf[has_write] = 0;
		} else {
			has_write += count;
		}
	}

	return has_write;
}

unsigned int get_mtee_is_enabled(void)
{
	return md_mtee_support;
}
EXPORT_SYMBOL(get_mtee_is_enabled);

int get_md_img_raw_size(int md_id)
{
	switch (md_id) {
	case MD_SYS1:
		return md1_raw_img_size;
	case MD_SYS3:
		return md3_raw_img_size;
	default:
		return 0;
	}
	return 0;
}

int get_raw_check_hdr(int md_id, char buf[], int size)
{
	char *chk_hdr_ptr = NULL;
	int cpy_size = 0;
	int ret = -1;

	if (buf == NULL)
		return -1;
	switch (md_id) {
	case MD_SYS1:
		chk_hdr_ptr = md1_check_hdr_info;
		cpy_size = md1_check_hdr_info_size;
		break;
	case MD_SYS3:
		chk_hdr_ptr = md3_check_hdr_info;
		cpy_size = md3_check_hdr_info_size;
		break;
	default:
		break;
	}
	if (chk_hdr_ptr == NULL)
		return ret;

	cpy_size = cpy_size > size?size:cpy_size;
	memcpy(buf, chk_hdr_ptr, cpy_size);

	return cpy_size;
}

int modem_run_env_ready(int md_id)
{
	return s_g_md_env_rdy_flag & (1<<md_id);
}

int get_md_resv_ccb_info(int md_id, phys_addr_t *ccb_data_base,
	unsigned int *ccb_data_size)
{
	*ccb_data_base = ccb_info.ccb_data_buffer_addr;
	*ccb_data_size = ccb_info.ccb_data_buffer_size;

	return 0;
}
EXPORT_SYMBOL(get_md_resv_ccb_info);

int get_md_resv_udc_info(int md_id, unsigned int *udc_noncache_size,
	unsigned int *udc_cache_size)
{
	*udc_noncache_size = udc_size.noncache_size;
	*udc_cache_size = udc_size.cache_size;

	return 0;
}
EXPORT_SYMBOL(get_md_resv_udc_info);

unsigned int get_md_resv_phy_cap_size(int md_id)
{
	if (md_id == MD_SYS1)
		return md1_phy_cap_size;

	return 0;
}
EXPORT_SYMBOL(get_md_resv_phy_cap_size);

unsigned int get_md_resv_sib_size(int md_id)
{
	if (md_id == MD_SYS1)
		return sib_info.md1_sib_size;

	return 0;
}
EXPORT_SYMBOL(get_md_resv_sib_size);

int get_md_smem_dfd_size(int md_id)
{
	if (md_id == MD_SYS1)
		return md1_smem_dfd_size;

	return 0;
}
EXPORT_SYMBOL(get_md_smem_dfd_size);

int get_smem_amms_pos_size(int md_id)
{
	if (md_id == MD_SYS1)
		return smem_amms_pos_size;

	return 0;
}
EXPORT_SYMBOL(get_smem_amms_pos_size);

int get_smem_align_padding_size(int md_id)
{
	if (md_id == MD_SYS1)
		return smem_align_padding_size;

	return 0;
}
EXPORT_SYMBOL(get_smem_align_padding_size);

unsigned int get_md_smem_cachable_offset(int md_id)
{
	if (md_id == MD_SYS1)
		return md1_bank4_cache_offset;

	return 0;
}
EXPORT_SYMBOL(get_md_smem_cachable_offset);

int get_md_resv_csmem_info(int md_id, phys_addr_t *buf_base,
	unsigned int *buf_size)
{
	*buf_base = csmem_info.csmem_buffer_addr;
	*buf_size = csmem_info.csmem_buffer_size;

	return 0;
}
EXPORT_SYMBOL(get_md_resv_csmem_info);

int get_md_cache_region_info(int region_id, unsigned int *buf_base,
	unsigned int *buf_size)
{
	int i;

	*buf_base = 0;
	*buf_size = 0;
	if (csmem_layout == NULL || csmem_info.item_cnt == 0)
		return 0;

	for (i = 0; i < csmem_info.item_cnt; i++) {
		if (csmem_layout[i].item_cnt == region_id) {
			*buf_base = csmem_layout[i].md_offset;
			*buf_size = csmem_layout[i].csmem_buffer_size;
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(get_md_cache_region_info);

int get_md_sib_mem_info(phys_addr_t *rw_base,
	unsigned int *rw_size)
{
	if (rw_base != NULL)
		*rw_base = sib_info.md1_sib_addr;

	if (rw_size != NULL)
		*rw_size = sib_info.md1_sib_size;

	return 0;
}
EXPORT_SYMBOL(get_md_sib_mem_info);

int get_md_resv_mem_info(int md_id, phys_addr_t *r_rw_base,
	unsigned int *r_rw_size, phys_addr_t *srw_base,
	unsigned int *srw_size)
{
	if (md_id >= MAX_MD_NUM_AT_LK)
		return -1;

	if (r_rw_base != NULL)
		*r_rw_base = md_resv_mem_addr[md_id];

	if (r_rw_size != NULL)
		*r_rw_size = md_resv_mem_size[md_id];

	if (srw_base != NULL)
		*srw_base = md_resv_smem_addr[md_id];

	if (srw_size != NULL)
		*srw_size = md_resv_smem_size[md_id];

	return 0;
}
EXPORT_SYMBOL_GPL(get_md_resv_mem_info);

int get_md1_md3_resv_smem_info(int md_id, phys_addr_t *rw_base,
	unsigned int *rw_size)
{
	if ((md_id != MD_SYS1) && (md_id != MD_SYS3))
		return -1;

	if (rw_base != NULL)
		*rw_base = md1md3_resv_smem_addr;

	if (rw_size != NULL)
		*rw_size = md1md3_resv_smem_size;

	return 0;
}
EXPORT_SYMBOL(get_md1_md3_resv_smem_info);

unsigned int get_md_smem_align(int md_id)
{
	return 0x4000;
}

unsigned int get_modem_is_enabled(int md_id)
{
	return !!(s_g_md_usage_case & (1 << md_id));
}
EXPORT_SYMBOL(get_modem_is_enabled);

struct _mpu_cfg *get_mpu_region_cfg_info(int region_id)
{
	int i;

	if (s_g_md_mpu_cfg_list == NULL)
		return NULL;

	for (i = 0; i < s_g_mpu_info_num; i++)
		if (s_g_md_mpu_cfg_list[i].region == region_id)
			return &s_g_md_mpu_cfg_list[i];

	return NULL;
}

/**************************************************************/
/* The following functions are back up for old platform       */
/**************************************************************/
static void cal_md_settings(int md_id)
{
	unsigned int md_en = 0;
	char tmp_buf[30];
	char *node_name = NULL;
	char *node_name2 = NULL;
	struct device_node *node = NULL;
	struct device_node *node2 = NULL;
	int val = 0;

	memset(tmp_buf, 0, sizeof(tmp_buf));
	val = snprintf(tmp_buf, sizeof(tmp_buf),
		"opt_md%d_support", (md_id + 1));
	if (val < 0) {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"%s-%d:scnprintf fail.val=%d\n", __func__, __LINE__, val);
		return;
	}
	/* MTK_ENABLE_MD* */
	val = ccci_get_opt_val(tmp_buf);
	if (val > 0)
		md_en = 1;

	if (!(md_en && (s_g_md_usage_case & (1 << md_id)))) {
		CCCI_UTIL_INF_MSG_WITH_ID(md_id,
			"md%d is disabled\n", (md_id + 1));
		return;
	}

	/* MD*_SMEM_SIZE */
	if (md_id == MD_SYS1) {
		/* For cldma case */
		node_name = "mediatek,mddriver";
		/* For ccif case */
		node_name2 = "mediatek,ap_ccif0";
	} else if (md_id == MD_SYS2) {
		node_name = "mediatek,ap_ccif1";
	} else if (md_id == MD_SYS3) {
		node_name = "mediatek,ap2c2k_ccif";
	} else {
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"md%d id is not supported,need to check\n",
			(md_id + 1));
		s_g_md_usage_case &= ~(1 << md_id);
		return;
	}

	if (md_id == MD_SYS1) {
		node = of_find_compatible_node(NULL, NULL, node_name);
		node2 = of_find_compatible_node(NULL, NULL, node_name2);
		if (node)
			of_property_read_u32(node,
				"mediatek,md_smem_size",
				&md_resv_smem_size[md_id]);
		else if (node2)
			of_property_read_u32(node2,
				"mediatek,md_smem_size",
				&md_resv_smem_size[md_id]);
		else {
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
				"md%d smem size is not set in device tree,need to check\n",
				(md_id + 1));
			s_g_md_usage_case &= ~(1 << md_id);
			return;
		}
	} else {
		node = of_find_compatible_node(NULL, NULL, node_name);
		if (node) {
			of_property_read_u32(node,
				"mediatek,md_smem_size",
				&md_resv_smem_size[md_id]);
		} else {
			CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
				"md%d smem size is not set in device tree,need to check\n",
				(md_id + 1));
			s_g_md_usage_case &= ~(1 << md_id);
			return;
		}
	}
	/* MD ROM start address should be 32M align
	 * as remap hardware limitation
	 */
	md_resv_mem_addr[md_id] = md_resv_mem_list[md_id];
	/*
	 * for legacy CCCI: make share memory start address to be 2MB align,
	 * as share memory size is 2MB - requested by MD MPU.
	 * for ECCCI: ROM+RAM size will be align to 1M,
	 * and share memory is 2K, 1M alignment is also 2K alignment.
	 */
	md_resv_mem_size[md_id] =
			round_up(
			md_resv_size_list[md_id] - md_resv_smem_size[md_id],
			get_md_smem_align(md_id));
	md_resv_smem_addr[md_id] = md_resv_mem_list[md_id]
			+ md_resv_mem_size[md_id];

	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"md%d modem_total_size=0x%x,md_size=0x%x, smem_size=0x%x\n",
		(md_id + 1),
		md_resv_size_list[md_id], md_resv_mem_size[md_id],
		md_resv_smem_size[md_id]);

	if ((s_g_md_usage_case & (1 << md_id))
			&& ((md_resv_mem_addr[md_id]
			& (CCCI_MEM_ALIGN - 1)) != 0))
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"md%d memory addr is not 32M align!!!\n", (md_id + 1));

	if ((s_g_md_usage_case & (1 << md_id))
			&& ((md_resv_smem_addr[md_id]
			& (CCCI_SMEM_ALIGN_MD1 - 1)) != 0))
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id,
			"md%d share memory addr %p is not 0x%x align!!\n",
			(md_id + 1),
			&md_resv_smem_addr[md_id],
			CCCI_SMEM_ALIGN_MD1);

	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"MemStart: %016llx, MemSize:0x%08X\n",
		(unsigned long long)md_resv_mem_addr[md_id],
		md_resv_mem_size[md_id]);
	CCCI_UTIL_INF_MSG_WITH_ID(md_id,
		"SMemStart: %016llx, SMemSize:0x%08X\n",
		(unsigned long long)md_resv_smem_addr[md_id],
		md_resv_smem_size[md_id]);
}

static void cal_md_settings_v2(struct device_node *node)
{
	unsigned int tmp = 0;
	char tmp_buf[30];
	int i = 0;

	CCCI_UTIL_INF_MSG("using kernel dt mem setting for md\n");
	memset(tmp_buf, 0, sizeof(tmp_buf));

	/* MD*_SMEM_SIZE */
	for (i = 0; i < MAX_MD_NUM_AT_LK; i++) {
		scnprintf(tmp_buf, 30, "mediatek,md%d-smem-size", i+1);
		if (!of_property_read_u32(node, tmp_buf, &tmp)) {
			CCCI_UTIL_INF_MSG("DT[%s]:%08X\n", tmp_buf, tmp);
			md_resv_smem_size[MD_SYS1+i] = tmp;
		} else
			CCCI_UTIL_INF_MSG("DT[%s]:%08X\n",
				tmp_buf, md_resv_smem_size[MD_SYS1+i]);
	}

	/* MD1MD3_SMEM_SIZE*/
	scnprintf(tmp_buf, 30, "mediatek,md1md3-smem-size");
	if (!of_property_read_u32(node, tmp_buf, &tmp)) {
		CCCI_UTIL_INF_MSG("DT[%s]:%08X\n", tmp_buf, tmp);
		md1md3_resv_smem_size = tmp;
	} else
		CCCI_UTIL_INF_MSG("DT[%s]:%08X\n",
			tmp_buf, md1md3_resv_smem_size);

	/* CFG version */
	scnprintf(tmp_buf, 30, "mediatek,version");
	tmp = 0;
	of_property_read_u32(node, tmp_buf, &tmp);
	CCCI_UTIL_INF_MSG("DT[%s]:%08X\n", tmp_buf, tmp);
	if (tmp != 1) {
		CCCI_UTIL_INF_MSG("Un-support version:%d\n", tmp);
		return;
	}

	/* MD ROM and RW part */
	for (i = 0; i < MAX_MD_NUM_AT_LK; i++) {
		if (s_g_md_usage_case & (1 << i)) {
			md_resv_mem_size[i] = md_resv_size_list[i];
			md_resv_mem_addr[i] = md_resv_mem_list[i];
			CCCI_UTIL_INF_MSG(
				"md%d MemStart: 0x%016llx, MemSize:0x%08X\n",
				i+1,
				(unsigned long long)md_resv_mem_addr[i],
				md_resv_mem_size[i]);
		}
	}

	/* MD Share memory part */
	/* AP  <--> MD1 */
	/* MD1 <--> MD3 */
	/* AP  <--> MD3 */
	md_resv_smem_addr[MD_SYS1] = resv_smem_addr;
	if (s_g_md_usage_case & (1 << MD_SYS3)) {
		md1md3_resv_smem_addr =
			resv_smem_addr + md_resv_smem_size[MD_SYS1];
		md_resv_smem_addr[MD_SYS3] =
			md1md3_resv_smem_addr + md1md3_resv_smem_size;
	} else {
		md1md3_resv_smem_addr = 0;
		md1md3_resv_smem_size = 0;
		md_resv_smem_addr[MD_SYS3] = 0;
		md_resv_smem_size[MD_SYS3] = 0;
	}
	CCCI_UTIL_INF_MSG(
			"AP  <--> MD1 SMEM(0x%08X):%016llx~%016llx\n",
			md_resv_smem_size[MD_SYS1],
			(unsigned long long)md_resv_smem_addr[MD_SYS1],
			(unsigned long long)(md_resv_smem_addr[MD_SYS1]
			+ md_resv_smem_size[MD_SYS1]-1));
	CCCI_UTIL_INF_MSG(
			"MD1 <--> MD3 SMEM(0x%08X):%016llx~%016llx\n",
			md1md3_resv_smem_size,
			(unsigned long long)md1md3_resv_smem_addr,
			(unsigned long long)(md1md3_resv_smem_addr
			+ md1md3_resv_smem_size-1));
	CCCI_UTIL_INF_MSG(
			"AP  <--> MD3 SMEM(0x%08X):%016llx~%016llx\n",
			md_resv_smem_size[MD_SYS3],
			(unsigned long long)md_resv_smem_addr[MD_SYS3],
			(unsigned long long)(md_resv_smem_addr[MD_SYS3]
			+ md_resv_smem_size[MD_SYS3]-1));
}

/********************************************************/
/* Global functions                                    */
/*******************************************************/
int get_md_img_type(int md_id)
{
	/* MD standalone, only one image case */
	if (s_g_lk_load_img_status & LK_LOAD_MD_EN) {
		if (md_id < MAX_MD_NUM_AT_LK)
			return md_type_at_lk[md_id];
	}

	return 0;
}
EXPORT_SYMBOL(get_md_img_type);

void ccci_md_mem_reserve(void)
{
	CCCI_UTIL_INF_MSG("%s phased out.\n", __func__);
}

#ifdef CONFIG_OF_RESERVED_MEM
#define CCCI_MD1_MEM_RESERVED_KEY "mediatek,reserve-memory-ccci_md1"
#define CCCI_MD2_MEM_RESERVED_KEY "mediatek,reserve-memory-ccci_md2"
#define CCCI_MD3_MEM_RESERVED_KEY "mediatek,reserve-memory-ccci_md3_ccif"
#define CCCI_MD1MD3_SMEM_RESERVED_KEY "mediatek,reserve-memory-ccci_share"
int ccci_reserve_mem_of_init(struct reserved_mem *rmem)
{
	phys_addr_t rptr = 0;
	unsigned int rsize = 0;
	int md_id = -1;

	rptr = rmem->base;
	rsize = (unsigned int)rmem->size;
	if (strstr(CCCI_MD1_MEM_RESERVED_KEY, rmem->name))
		md_id = MD_SYS1;
	else if (strstr(CCCI_MD2_MEM_RESERVED_KEY, rmem->name))
		md_id = MD_SYS2;
	else if (strstr(CCCI_MD3_MEM_RESERVED_KEY, rmem->name))
		md_id = MD_SYS3;
	else {
		if (strstr(CCCI_MD1MD3_SMEM_RESERVED_KEY, rmem->name)) {
			CCCI_UTIL_INF_MSG(
			"reserve_mem_of_init, rptr=0x%pa, rsize=0x%x\n",
			&rptr, rsize);
			resv_smem_addr = rptr;
			resv_smem_size = rsize;
		} else
			CCCI_UTIL_INF_MSG("memory reserve key %s not support\n",
				rmem->name);

		return 0;
	}
	CCCI_UTIL_INF_MSG("reserve_mem_of_init, rptr=0x%pa, rsize=0x%x\n",
			&rptr, rsize);
	md_resv_mem_list[md_id] = rptr;
	md_resv_size_list[md_id] = rsize;
	s_g_md_usage_case |= (1U << md_id);
	return 0;
}

RESERVEDMEM_OF_DECLARE(ccci_reserve_mem_md1_init,
	CCCI_MD1_MEM_RESERVED_KEY, ccci_reserve_mem_of_init);
RESERVEDMEM_OF_DECLARE(ccci_reserve_mem_md2_init,
	CCCI_MD2_MEM_RESERVED_KEY, ccci_reserve_mem_of_init);
RESERVEDMEM_OF_DECLARE(ccci_reserve_mem_md3_init,
	CCCI_MD3_MEM_RESERVED_KEY, ccci_reserve_mem_of_init);
RESERVEDMEM_OF_DECLARE(ccci_reserve_smem_md1md3_init,
	CCCI_MD1MD3_SMEM_RESERVED_KEY, ccci_reserve_mem_of_init);
#endif


/**************************************************************/
/* CCCI Feature option parsiong      entry                    */
/**************************************************************/
int __init ccci_util_fo_init(void)
{
	int idx;
	struct device_node *node = NULL;

	CCCI_UTIL_INF_MSG("%s 0.\n", __func__);

	CCCI_UTIL_INF_MSG("Dump default setting(@P/K)\n");
	ccci_dump_opt_tbl();

	if (collect_lk_boot_arguments() == 0) {
		CCCI_UTIL_INF_MSG("using v3.\n");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,ccci_util_cfg");
	if (node == NULL) {
		CCCI_UTIL_INF_MSG("using v1.\n");

		/* Calculate memory layout */
		for (idx = 0; idx < MAX_MD_NUM_AT_LK; idx++)
			cal_md_settings(idx);
	} else {
		CCCI_UTIL_INF_MSG("using v2.\n");
		cal_md_settings_v2(node);
	}
	CCCI_UTIL_INF_MSG("%s 2.\n", __func__);
	return 0;
}
