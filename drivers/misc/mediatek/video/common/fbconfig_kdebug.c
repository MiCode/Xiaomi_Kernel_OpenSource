/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/types.h>

#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6570) || defined(CONFIG_ARCH_MT6580)
#include "disp_debug.h"
#include "mtkfb_debug.h"
#else
#include "debug.h"
#endif

#include "disp_drv_platform.h"
#include "m4u_priv.h"
#include "mtkfb.h"
#include "lcm_drv.h"
#include "ddp_path.h"
#include "fbconfig_kdebug.h"
#include "primary_display.h"
#include "ddp_ovl.h"
#include "ddp_dsi.h"
#include "ddp_irq.h"

/* **************************************************************************** */
/* This part is for customization parameters of D-IC and DSI . */
/* **************************************************************************** */
bool fbconfig_start_LCM_config;
#define FBCONFIG_MDELAY(n)	(PM_lcm_utils_dsi0.mdelay((n)))
#define SET_RESET_PIN(v)	(PM_lcm_utils_dsi0.set_reset_pin((v)))
#define dsi_set_cmdq(pdata, queue_size, force_update) PM_lcm_utils_dsi0.dsi_set_cmdq(pdata, queue_size, force_update)
#define FBCONFIG_KEEP_NEW_SETTING 1
#define FBCONFIG_DEBUG 0

#define FBCONFIG_IOW(num, dtype)     _IOW('X', num, dtype)
#define FBCONFIG_IOR(num, dtype)     _IOR('X', num, dtype)
#define FBCONFIG_IOWR(num, dtype)    _IOWR('X', num, dtype)
#define FBCONFIG_IO(num)             _IO('X', num)

#define GET_DSI_ID	   FBCONFIG_IOW(43, unsigned int)
#define SET_DSI_ID	   FBCONFIG_IOW(44, unsigned int)
#define LCM_GET_ID     FBCONFIG_IOR(45, unsigned int)
#define LCM_GET_ESD    FBCONFIG_IOWR(46, unsigned int)
#define DRIVER_IC_CONFIG    FBCONFIG_IOR(47, unsigned int)
#define DRIVER_IC_CONFIG_DONE  FBCONFIG_IO(0)
#define DRIVER_IC_RESET    FBCONFIG_IOR(48, unsigned int)


#define MIPI_SET_CLK     FBCONFIG_IOW(51, unsigned int)
#define MIPI_SET_LANE    FBCONFIG_IOW(52, unsigned int)
#define MIPI_SET_TIMING  FBCONFIG_IOW(53, unsigned int)
#define MIPI_SET_VM      FBCONFIG_IOW(54, unsigned int)	/* mipi video mode timing setting */
#define MIPI_SET_CC	 FBCONFIG_IOW(55, unsigned int)	/* mipi non-continuous clock */
#define MIPI_SET_SSC	 FBCONFIG_IOW(56, unsigned int)	/* spread frequency */
#define MIPI_SET_CLK_V2  FBCONFIG_IOW(57, unsigned int)	/* For div1,div2,fbk_div case */


#define TE_SET_ENABLE  FBCONFIG_IOW(61, unsigned int)
#define FB_LAYER_DUMP  FBCONFIG_IOW(62, unsigned int)
#define FB_LAYER_GET_INFO FBCONFIG_IOW(63, unsigned int)
#define FB_LAYER_GET_EN FBCONFIG_IOW(64, unsigned int)
#define LCM_GET_ESD_RET    FBCONFIG_IOR(65, unsigned int)

#define LCM_GET_DSI_CONTINU    FBCONFIG_IOR(71, unsigned int)
#define LCM_GET_DSI_CLK   FBCONFIG_IOR(72, unsigned int)
#define LCM_GET_DSI_TIMING   FBCONFIG_IOR(73, unsigned int)
#define LCM_GET_DSI_LANE_NUM    FBCONFIG_IOR(74, unsigned int)
#define LCM_GET_DSI_TE    FBCONFIG_IOR(75, unsigned int)
#define LCM_GET_DSI_SSC    FBCONFIG_IOR(76, unsigned int)
#define LCM_GET_DSI_CLK_V2    FBCONFIG_IOR(77, unsigned int)
#define LCM_TEST_DSI_CLK    FBCONFIG_IOR(78, unsigned int)
#define FB_GET_MISC FBCONFIG_IOR(80, unsigned int)

#ifdef UFMT_GET_bpp
#define DP_COLOR_BITS_PER_PIXEL(color)    UFMT_GET_bpp(color)
#else
#define DP_COLOR_BITS_PER_PIXEL(color)    ((0x0003FF00 & color) >>  8)
#endif
static int global_layer_id = -1;

struct dentry *ConfigPara_dbgfs = NULL;
CONFIG_RECORD_LIST head_list;
LCM_REG_READ reg_read;

/* int esd_check_addr; */
/* int esd_check_para_num; */
/* int esd_check_type; */
/* char * esd_check_buffer =NULL; */
/* extern void fbconfig_disp_set_mipi_timing(MIPI_TIMING timing); */
/* extern unsigned int fbconfig_get_layer_info(FBCONFIG_LAYER_INFO *layers); */
/* extern unsigned int fbconfig_get_layer_vaddr(int layer_id,int * layer_size,int * enable); */
/* unsigned int fbconfig_get_layer_height(int layer_id,int * layer_size,int * enable,int* height ,int * fmt); */
typedef struct PM_TOOL_ST {
	DSI_INDEX dsi_id;
	LCM_REG_READ reg_read;
	LCM_PARAMS *pLcm_params;
	LCM_DRIVER *pLcm_drv;
} PM_TOOL_T;
static PM_TOOL_T pm_params = {
	.dsi_id = PM_DSI0,
	.pLcm_params = NULL,
	.pLcm_drv = NULL,
};
struct mutex fb_config_lock;

static void *pm_get_handle(void)
{
	return (void *)&pm_params;
}

static DISP_MODULE_ENUM pm_get_dsi_handle(DSI_INDEX dsi_id)
{
	if (dsi_id == PM_DSI0)
		return DISP_MODULE_DSI0;
	else if (dsi_id == PM_DSI1)
		return DISP_MODULE_DSI1;
	else if (dsi_id == PM_DSI_DUAL)
		return DISP_MODULE_DSIDUAL;
	else
		return DISP_MODULE_UNKNOWN;
}

int fbconfig_get_esd_check(DSI_INDEX dsi_id, uint32_t cmd, uint8_t *buffer, uint32_t num)
{
	int array[4];
	int ret = 0;
	/* set max return packet size */
	/* array[0] = 0x00013700; */
	array[0] = 0x3700 + (num << 16);
	dsi_set_cmdq(array, 1, 1);
	atomic_set(&ESDCheck_byCPU , 1);
	if ((dsi_id == PM_DSI0) || (dsi_id == PM_DSI_DUAL))
		ret = DSI_dcs_read_lcm_reg_v2(pm_get_dsi_handle(PM_DSI0), NULL, cmd, buffer, num);
	else if (dsi_id == PM_DSI1)
		ret = DSI_dcs_read_lcm_reg_v2(pm_get_dsi_handle(PM_DSI1), NULL, cmd, buffer, num);
	atomic_set(&ESDCheck_byCPU , 0);
	if (ret == 0)
		return -1;

	return 0;
}

/* RECORD_CMD = 0, */
/* RECORD_MS = 1, */
/* RECORD_PIN_SET        = 2, */

void Panel_Master_DDIC_config(void)
{

	struct list_head *p;
	CONFIG_RECORD_LIST *node;

	list_for_each_prev(p, &head_list.list) {
		node = list_entry(p, CONFIG_RECORD_LIST, list);
		switch (node->record.type) {
		case RECORD_CMD:
			dsi_set_cmdq(node->record.ins_array, node->record.ins_num, 1);
			break;
		case RECORD_MS:
			FBCONFIG_MDELAY(node->record.ins_array[0]);
			break;
		case RECORD_PIN_SET:
			SET_RESET_PIN(node->record.ins_array[0]);
			break;
		default:
			pr_debug("sxk=>No such Type!!!!!\n");
		}

	}

}

/*static void print_from_head_to_tail(void)
{
	int i;
	struct list_head *p;
	CONFIG_RECORD_LIST *print;
	pr_debug("DDIC=====>:print_from_head_to_tail  START\n");

	list_for_each_prev(p, &head_list.list) {
		print = list_entry(p, CONFIG_RECORD_LIST, list);
		pr_debug("type:%d num %d value:\r\n", print->record.type, print->record.ins_num);
		for (i = 0; i < print->record.ins_num; i++)
			pr_debug("0x%x\t", print->record.ins_array[i]);
		pr_debug("\r\n");
	}
	pr_debug("DDIC=====>:print_from_head_to_tail  END\n");

}*/

static void free_list_memory(void)
{
	struct list_head *p, *n;
	CONFIG_RECORD_LIST *print;

	list_for_each_safe(p, n, &head_list.list) {
		print = list_entry(p, CONFIG_RECORD_LIST, list);
		list_del(&print->list);
		kfree(print);
	}
	/* test here : head->next == head ?? */
	if (list_empty(&head_list.list))
		pr_debug("*****list is empty!!\n");
	else
		pr_debug("*****list is NOT empty!!\n");

}

static int fbconfig_open(struct inode *inode, struct file *file)
{
	PM_TOOL_T *pm_params;
	file->private_data = inode->i_private;
	mutex_init(&fb_config_lock);
	pm_params = (PM_TOOL_T *) pm_get_handle();
	PanelMaster_set_PM_enable(1);
	pm_params->pLcm_drv = DISP_GetLcmDrv();
	pm_params->pLcm_params = DISP_GetLcmPara();

	if (pm_params->pLcm_params->lcm_if == LCM_INTERFACE_DSI_DUAL)
		pm_params->dsi_id = PM_DSI_DUAL;
	else if (pm_params->pLcm_params->lcm_if == LCM_INTERFACE_DSI1)
		pm_params->dsi_id = PM_DSI1;
	return 0;
}


static char fbconfig_buffer[2048];

static ssize_t fbconfig_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(fbconfig_buffer) - 1;	/* 2047 */
	int n = 0;

	n += scnprintf(fbconfig_buffer + n, debug_bufmax - n, "sxkhome");
	fbconfig_buffer[n++] = 0;
	/* n = 5 ; */
	/* memcpy(fbconfig_buffer,"sxkhome",6); */
	return simple_read_from_buffer(ubuf, count, ppos, fbconfig_buffer, n);
}

static ssize_t fbconfig_write(struct file *file,
			      const char __user *ubuf, size_t count, loff_t *ppos)
{
	return 0;
}


static long fbconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int ret_val = 0; /* for other function call and put_user / get_user */
	unsigned long copy_ret_val = 0; /* for copy_from_user / copy_to_user */
	void __user *argp = (void __user *)arg;
	PM_TOOL_T *pm = (PM_TOOL_T *)pm_get_handle();
	uint32_t dsi_id = pm->dsi_id;
	LCM_DSI_PARAMS *pParams = get_dsi_params_handle(dsi_id);

#ifdef FBCONFIG_SHOULD_KICK_IDLEMGR
	primary_display_idlemgr_kick(__func__, 1);
#endif
	switch (cmd) {
	case GET_DSI_ID:
	{
		ret_val = put_user(dsi_id, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>GET_DSI_ID put_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case SET_DSI_ID:
	{
		unsigned int set_dsi_id = 0;

		ret_val = get_user(set_dsi_id, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>SET_DSI_ID get_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		if (set_dsi_id > PM_DSI_DUAL)
			return -EINVAL;
		pm->dsi_id = set_dsi_id;
		pr_debug("fbconfig=>SET_DSI_ID:%d\n", pm->dsi_id);
	}
	break;
	case LCM_TEST_DSI_CLK:
	{
		LCM_TYPE_FB lcm_fb;
		LCM_PARAMS *pLcm_params = pm->pLcm_params;

		lcm_fb.clock = pLcm_params->dsi.PLL_CLOCK;
		lcm_fb.lcm_type = (unsigned int)pLcm_params->dsi.mode;

		pr_debug("fbconfig=>LCM_TEST_DSI_CLK:%d mode %d\n", lcm_fb.clock, lcm_fb.lcm_type);
		copy_ret_val = copy_to_user(argp, &lcm_fb, sizeof(lcm_fb));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>LCM_TEST_DSI_CLK copy_to_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case LCM_GET_ID:
	{
		unsigned int lcm_id = 0; /* the lcm driver does not impl "get_lcm_id" */
		/* so we cannot use pLcm_drv->get_lcm_id(), instead return 0 */
#if 0
		LCM_DRIVER *pLcm_drv = pm->pLcm_drv;
		if (pLcm_drv != NULL)
			lcm_id = pLcm_drv->get_lcm_id();
		else
			pr_debug("fbconfig=>LCM_GET_ID not implemented in lcm driver\n");
#endif
		copy_ret_val = copy_to_user(argp, &lcm_id, sizeof(lcm_id));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_ID copy_to_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case DRIVER_IC_CONFIG:
	{
		CONFIG_RECORD_LIST *record_tmp_list = kmalloc(sizeof(*record_tmp_list), GFP_KERNEL);
		if (record_tmp_list == NULL) {
			pr_debug("fbconfig=>DRIVER_IC_CONFIG kmalloc failed @line %d\n", __LINE__);
			return -ENOMEM;
		}
		copy_ret_val = copy_from_user(&record_tmp_list->record, argp, sizeof(CONFIG_RECORD));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>DRIVER_IC_CONFIG list_add: copy_from_user failed @line %d\n", __LINE__);
			kfree(record_tmp_list);
			record_tmp_list = NULL;
			return -EFAULT;
		}
		mutex_lock(&fb_config_lock);
		list_add(&record_tmp_list->list, &head_list.list);
		mutex_unlock(&fb_config_lock);
	}
	break;
	case DRIVER_IC_CONFIG_DONE:
	{
		/* while all DRIVER_IC_CONFIG is added, use this to set complete */
		mutex_lock(&fb_config_lock);
		Panel_Master_dsi_config_entry("PM_DDIC_CONFIG", NULL);
		/* free the memory ..... */
		free_list_memory();
		mutex_unlock(&fb_config_lock);
	}
	break;
	case MIPI_SET_CC:
	{
		unsigned int enable = 0;

		ret_val = get_user(enable, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>MIPI_SET_CC get_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		PanelMaster_set_CC(dsi_id, enable); /* TODO: no error code */
		pr_debug("fbconfig=>MIPI_SET_CC DSI:%d value %d\n", dsi_id, enable);
	}
	break;
	case LCM_GET_DSI_CONTINU:
	{
		unsigned int enable = 0;

		enable = PanelMaster_get_CC(dsi_id);
		/* need to improve ,0 now means nothing but one parameter.... */
		pr_debug("fbconfig=>LCM_GET_DSI_CONTINU DSI:%d value %d\n", dsi_id, enable);
		ret_val = put_user(enable, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_DSI_CONTINU put_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case MIPI_SET_CLK:
	{
		unsigned int clk = 0;

		ret_val = get_user(clk, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>MIPI_SET_CLK get_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		Panel_Master_dsi_config_entry("PM_CLK", &clk); /* TODO: check error code */
		pr_debug("fbconfig=>MIPI_SET_CLK DSI:%d value %d\n", dsi_id, clk);
	}
	break;
	case LCM_GET_DSI_CLK:
	{
		unsigned int clk = 0;

		clk = pParams->PLL_CLOCK;
		pr_debug("fbconfig=>LCM_GET_DSI_CLK DSI:%d value %d\n", dsi_id, clk);
		ret_val = put_user(clk, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_DSI_CLK put_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case MIPI_SET_SSC:
	{
		unsigned int dsi_ssc = 0;

		ret_val = get_user(dsi_ssc, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>MIPI_SET_SSC get_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		Panel_Master_dsi_config_entry("PM_SSC", &dsi_ssc);
		pr_debug("fbconfig=>MIPI_SET_SSC DSI:%d value %d\n", dsi_id, dsi_ssc);
	}
	break;
	case LCM_GET_DSI_SSC:
	{
		unsigned int ssc = 0;

		ssc = pParams->ssc_range;
		if (pParams->ssc_disable != 0)
			ssc = 0;
		pr_debug("fbconfig=>LCM_GET_DSI_SSC DSI:%d value %d\n", dsi_id, ssc);
		ret_val = put_user(ssc, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_DSI_SSC put_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case LCM_GET_DSI_LANE_NUM:
	{
		unsigned int lane_num = 0;

		lane_num = pParams->LANE_NUM;
		pr_debug("fbconfig=>LCM_GET_DSI_LANE_NUM DSI:%d value %d\n", dsi_id, lane_num);
		ret_val = put_user(lane_num, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_DSI_LANE_NUM put_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case LCM_GET_DSI_TE:
	{
		unsigned int dsi_te = 0;

		dsi_te = PanelMaster_get_TE_status(dsi_id);
		pr_debug("fbconfig=>LCM_GET_DSI_TE DSI:%d value %d\n", dsi_id, dsi_te);
		ret_val = put_user(dsi_te, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_DSI_TE put_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case LCM_GET_DSI_TIMING:
	{
		unsigned int r = 0;
		MIPI_TIMING timing;

		copy_ret_val = copy_from_user(&timing, argp, sizeof(timing));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_DSI_TIMING copy_from_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		r = PanelMaster_get_dsi_timing(dsi_id, timing.type);
		pr_debug("fbconfig=>LCM_GET_DSI_TIMING DSI:%d result %d\n", dsi_id, r);
		timing.value = r;
		copy_ret_val = copy_to_user(argp, &timing, sizeof(timing));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_DSI_TIMING copy_to_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
	}
	break;
	case MIPI_SET_TIMING:
	{
		MIPI_TIMING timing;

		if (primary_display_is_sleepd())
			return -EPERM;
		copy_ret_val = copy_from_user(&timing, argp, sizeof(timing));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>MIPI_SET_TIMING copy_from_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		PanelMaster_DSI_set_timing(dsi_id, timing); /* TODO: no error code */
	}
	break;
	case FB_LAYER_GET_EN:
	{
		PM_LAYER_EN layers;
		OVL_BASIC_STRUCT ovl_all[TOTAL_OVL_LAYER_NUM];
		int i = 0;

		memset(ovl_all, 0, sizeof(ovl_all));
#ifdef PRIMARY_THREE_OVL_CASCADE
		ovl_get_info(DISP_MODULE_OVL0_2L, ovl_all);
		ovl_get_info(DISP_MODULE_OVL0, &ovl_all[2]);
		ovl_get_info(DISP_MODULE_OVL1_2L, &ovl_all[6]);
		for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++)
			layers.layer_en[i] = (ovl_all[i].layer_en ? 1 : 0);
#else
		ovl_get_info(DISP_MODULE_OVL0, ovl_all);
		layers.layer_en[i + 0] = (ovl_all[0].layer_en ? 1 : 0);
		layers.layer_en[i + 1] = (ovl_all[1].layer_en ? 1 : 0);
		layers.layer_en[i + 2] = (ovl_all[2].layer_en ? 1 : 0);
		layers.layer_en[i + 3] = (ovl_all[3].layer_en ? 1 : 0);
#ifdef OVL_CASCADE_SUPPORT
		layers.layer_en[i + 4] = (ovl_all[4].layer_en ? 1 : 0);
		layers.layer_en[i + 5] = (ovl_all[5].layer_en ? 1 : 0);
		layers.layer_en[i + 6] = (ovl_all[6].layer_en ? 1 : 0);
		layers.layer_en[i + 7] = (ovl_all[7].layer_en ? 1 : 0);
#endif
#endif
		pr_debug("[FB_LAYER_GET_EN] L0:%d L1:%d L2:%d L3:%d\n",
			ovl_all[0].layer_en, ovl_all[1].layer_en, ovl_all[2].layer_en, ovl_all[3].layer_en);
		return copy_to_user(argp, &layers, sizeof(layers)) ? -EFAULT : 0;
	}
	break;
	case FB_LAYER_GET_INFO:
	{
		PM_LAYER_INFO layer_info;
		OVL_BASIC_STRUCT ovl_all[TOTAL_OVL_LAYER_NUM];

		memset(ovl_all, 0, sizeof(ovl_all));
		if (copy_from_user(&layer_info, argp, sizeof(layer_info))) {
			pr_debug("[FB_LAYER_GET_INFO]: copy_from_user failed! line:%d\n", __LINE__);
			return -EFAULT;
		}
		global_layer_id = layer_info.index;
#ifdef PRIMARY_THREE_OVL_CASCADE
		ovl_get_info(DISP_MODULE_OVL0_2L, ovl_all);
		ovl_get_info(DISP_MODULE_OVL0, &ovl_all[2]);
		ovl_get_info(DISP_MODULE_OVL1_2L, &ovl_all[6]);
#else
		ovl_get_info(DISP_MODULE_OVL0, ovl_all);
#endif
		layer_info.height = ovl_all[layer_info.index].src_h;
		layer_info.width = ovl_all[layer_info.index].src_w;
		layer_info.fmt = DP_COLOR_BITS_PER_PIXEL(ovl_all[layer_info.index].fmt);
		layer_info.layer_size = ovl_all[layer_info.index].src_pitch * ovl_all[layer_info.index].src_h;
		pr_debug("===>: layer_size:0x%x height:%d\n", layer_info.layer_size, layer_info.height);
		pr_debug("===>: width:%d src_pitch:%d\n", layer_info.width, ovl_all[layer_info.index].src_pitch);
		pr_debug("===>: layer_id:%d fmt:%d\n", global_layer_id, layer_info.fmt);
		pr_debug("===>: layer_en:%d\n", (ovl_all[layer_info.index].layer_en));
		if ((layer_info.height == 0) || (layer_info.width == 0) || (ovl_all[layer_info.index].layer_en == 0)) {
			pr_debug("===> Error, height/width is 0 or layer_en == 0!!\n");
			return -2;
		} else
			return copy_to_user(argp, &layer_info, sizeof(layer_info)) ? -EFAULT : 0;
	}
	break;
	case FB_LAYER_DUMP:
	{
		int layer_size;
		int ret = 0;
		unsigned long kva = 0;
		unsigned int mva;
		unsigned int mapped_size = 0;
		unsigned int real_mva = 0;
		unsigned int real_size = 0;
		OVL_BASIC_STRUCT ovl_all[TOTAL_OVL_LAYER_NUM];

		memset(ovl_all, 0, sizeof(ovl_all));
#ifdef PRIMARY_THREE_OVL_CASCADE
		ovl_get_info(DISP_MODULE_OVL0_2L, ovl_all);
		ovl_get_info(DISP_MODULE_OVL0, &ovl_all[2]);
		ovl_get_info(DISP_MODULE_OVL1_2L, &ovl_all[6]);
#else
		ovl_get_info(DISP_MODULE_OVL0, ovl_all);
#endif
		layer_size = ovl_all[global_layer_id].src_pitch * ovl_all[global_layer_id].src_h;
		mva = ovl_all[global_layer_id].addr;
		pr_debug("layer_size=%d, src_pitch=%d, h=%d, mva=0x%x,\n",
			 layer_size, ovl_all[global_layer_id].src_pitch, ovl_all[global_layer_id].src_h, mva);

		if ((layer_size != 0) && (ovl_all[global_layer_id].layer_en != 0)) {
			ret = m4u_query_mva_info(mva, layer_size, &real_mva, &real_size);
			if (ret < 0) {
				pr_debug("m4u_query_mva_info error: ret=%d mva=0x%x layer_size=%d\n",
					ret, mva, layer_size);
				return ret;
			}
			ret = m4u_mva_map_kernel(real_mva, real_size, &kva, &mapped_size);
			if (ret < 0) {
				pr_debug("m4u_mva_map_kernel error: ret=%d real_mva=0x%x real_size=%d\n",
					ret, real_mva, real_size);
				return ret;
			}
			if (layer_size > mapped_size) {
				pr_debug("==>layer size(0x%x)>mapped size(0x%x)!!!\n", layer_size, mapped_size);
				return -EFAULT;
			}
			pr_debug("==> addr from user space is 0x%p\n", argp);
			pr_debug("==> kva=0x%lx real_mva=%x mva=%x mmaped_size=%d layer_size=%d\n",
				kva, real_mva, mva, mapped_size, layer_size);
			ret = copy_to_user(argp,
				(void *)kva + (mva - real_mva), layer_size - (mva - real_mva)) ? -EFAULT : 0;
			m4u_mva_unmap_kernel(real_mva, real_size, kva);
			return ret;
		} else
			return -2;
	}
	break;
	case LCM_GET_ESD:
	{
		ESD_PARA esd_para;
		uint8_t *buffer = NULL;

		copy_ret_val = copy_from_user(&esd_para, argp, sizeof(esd_para));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_ESD copy_from_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		buffer = kzalloc(esd_para.para_num + 6, GFP_KERNEL);
		if (buffer == NULL)
			return -ENOMEM;

		ret_val = fbconfig_get_esd_check_test(dsi_id, esd_para.addr, buffer,
						esd_para.para_num);
		if (ret_val < 0) {
			kfree(buffer);
			buffer = NULL;
			return -EAGAIN;
		}
		/* TODO: esd_para.esd_ret_buffer not transferred */
		copy_ret_val = copy_to_user(esd_para.esd_ret_buffer, buffer, esd_para.para_num);
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>LCM_GET_ESD copy_to_user failed @line %d\n", __LINE__);
			kfree(buffer);
			buffer = NULL;
			return -EFAULT;
		}
		kfree(buffer);
		buffer = NULL;
	}
	break;
	case TE_SET_ENABLE:
	{
		unsigned int te_enable = 0;

		ret_val = get_user(te_enable, (unsigned int __user *)argp);
		if (ret_val != 0) {
			pr_debug("fbconfig=>TE_SET_ENABLE get_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		/* TODO: acutally set TE? */
		pr_debug("fbconfig=>TE_SET_ENABLE\n");
	}
	break;
	case DRIVER_IC_RESET:
	{
		Panel_Master_dsi_config_entry("DRIVER_IC_RESET", NULL);
		ret = 0;
		pr_debug("fbconfig=>DRIVER_IC_RESET done\n");
	}
	break;
	case FB_GET_MISC:
	{
		struct misc_property misc = {
			.dual_port = 0,
			.overall_layer_num = 0,
			.reserved = 0,
		};

		if (pm->pLcm_params->lcm_if == LCM_INTERFACE_DSI_DUAL)
			misc.dual_port = 1;
		misc.overall_layer_num = TOTAL_OVL_LAYER_NUM;
		copy_ret_val = copy_to_user(argp, &misc, sizeof(misc));
		if (copy_ret_val != 0) {
			pr_debug("fbconfig=>FB_GET_MISC copy_to_user failed @line %d\n", __LINE__);
			return -EFAULT;
		}
		pr_debug("fbconfig=>FB_GET_MISC ret\n");
	}
	break;
	default:
		pr_debug("fbconfig=>INVALID IOCTL CMD:%d\n", cmd);
		break;
	}

	return ret;
}

static int fbconfig_release(struct inode *inode, struct file *file)
{
	PanelMaster_set_PM_enable(0);

	return 0;
}

/* compat-ioctl */
#ifdef CONFIG_COMPAT

#define COMPAT_GET_DSI_ID	   FBCONFIG_IOW(43, compat_uint_t)
#define COMPAT_SET_DSI_ID	   FBCONFIG_IOW(44, compat_uint_t)
#define COMPAT_LCM_GET_ID     FBCONFIG_IOR(45, compat_uint_t)
#define COMPAT_LCM_GET_ESD    FBCONFIG_IOWR(46, compat_uint_t)
#define COMPAT_DRIVER_IC_CONFIG    FBCONFIG_IOR(47, compat_uint_t)
#define COMPAT_DRIVER_IC_CONFIG_DONE  FBCONFIG_IO(0)
#define COMPAT_DRIVER_IC_RESET    FBCONFIG_IOR(48, compat_uint_t)

#define COMPAT_MIPI_SET_CLK     FBCONFIG_IOW(51, compat_uint_t)
#define COMPAT_MIPI_SET_LANE    FBCONFIG_IOW(52, compat_uint_t)
#define COMPAT_MIPI_SET_TIMING  FBCONFIG_IOW(53, compat_uint_t)
#define COMPAT_MIPI_SET_VM      FBCONFIG_IOW(54, compat_uint_t)
#define COMPAT_MIPI_SET_CC	 FBCONFIG_IOW(55, compat_uint_t)
#define COMPAT_MIPI_SET_SSC	 FBCONFIG_IOW(56, compat_uint_t)
#define COMPAT_MIPI_SET_CLK_V2  FBCONFIG_IOW(57, compat_uint_t)

#define COMPAT_TE_SET_ENABLE  FBCONFIG_IOW(61, compat_uint_t)
#define COMPAT_FB_LAYER_DUMP  FBCONFIG_IOW(62, compat_uint_t)
#define COMPAT_FB_LAYER_GET_INFO FBCONFIG_IOW(63, compat_uint_t)
#define COMPAT_FB_LAYER_GET_EN FBCONFIG_IOW(64, compat_uint_t)
#define COMPAT_LCM_GET_ESD_RET    FBCONFIG_IOR(65, compat_uint_t)

#define COMPAT_LCM_GET_DSI_CONTINU    FBCONFIG_IOR(71, compat_uint_t)
#define COMPAT_LCM_GET_DSI_CLK   FBCONFIG_IOR(72, compat_uint_t)
#define COMPAT_LCM_GET_DSI_TIMING   FBCONFIG_IOR(73, compat_uint_t)
#define COMPAT_LCM_GET_DSI_LANE_NUM    FBCONFIG_IOR(74, compat_uint_t)
#define COMPAT_LCM_GET_DSI_TE    FBCONFIG_IOR(75, compat_uint_t)
#define COMPAT_LCM_GET_DSI_SSC    FBCONFIG_IOR(76, compat_uint_t)
#define COMPAT_LCM_GET_DSI_CLK_V2    FBCONFIG_IOR(77, compat_uint_t)
#define COMPAT_LCM_TEST_DSI_CLK    FBCONFIG_IOR(78, compat_uint_t)
#define COMPAT_FB_GET_MISC    FBCONFIG_IOR(80, compat_uint_t)

static int compat_get_lcm_type_fb(struct compat_lcm_type_fb __user *data32,
				  struct LCM_TYPE_FB __user *data)
{
	compat_int_t i;
	int err;

	err = get_user(i, &data32->clock);
	err |= put_user(i, &data->clock);
	err |= get_user(i, &data32->lcm_type);
	err |= put_user(i, &data->lcm_type);

	return err;
}

static int compat_put_lcm_type_fb(struct compat_lcm_type_fb __user *data32,
				  struct LCM_TYPE_FB __user *data)
{
	compat_int_t i;
	int err;

	err = get_user(i, &data->clock);
	err |= put_user(i, &data32->clock);
	err |= get_user(i, &data->lcm_type);
	err |= put_user(i, &data32->lcm_type);

	return err;
}

static int compat_get_config_record(struct compat_config_record *data32,
				    struct CONFIG_RECORD *data)
{
	compat_int_t i;
	int err;
	int n = 0;

	err = get_user(i, &data32->ins_num);
	err |= put_user(i, &data->ins_num);
	err |= get_user(i, &data32->type);
	err |= put_user(i, &data->type);

	for (n = 0; n < MAX_INSTRUCTION; ++n) {
		err |= get_user(i, &data32->ins_array[n]);
		err |= put_user(i, &data->ins_array[n]);
	}
	return err;
}

static int compat_put_config_record(struct compat_config_record *data32,
				    struct CONFIG_RECORD *data)
{
	compat_int_t i;
	int err;
	int n = 0;

	err = get_user(i, &data->ins_num);
	err |= put_user(i, &data32->ins_num);
	err |= get_user(i, &data->type);
	err |= put_user(i, &data32->type);

	for (n = 0; n < MAX_INSTRUCTION; ++n) {
		err |= get_user(i, &data->ins_array[n]);
		err |= put_user(i, &data32->ins_array[n]);
	}
	return err;
}

static int compat_get_dsi_ret(struct compat_dsi_ret *data32,
			      struct DSI_RET *data)
{
	compat_int_t i;
	int err = 0;
	int n = 0;

	for (n = 0; n < NUM_OF_DSI; ++n) {
		err |= get_user(i, &data32->dsi[n]);
		err |= put_user(i, &data->dsi[n]);
	}
	return err;
}

static int compat_put_dsi_ret(struct compat_dsi_ret *data32,
			      struct DSI_RET *data)
{
	compat_int_t i;
	int err = 0;
	int n = 0;

	for (n = 0; n < NUM_OF_DSI; ++n) {
		err |= get_user(i, &data->dsi[n]);
		err |= put_user(i, &data32->dsi[n]);
	}
	return err;
}

static int compat_get_mipi_timing(struct compat_mipi_timing *data32,
				    struct MIPI_TIMING *data)
{
	compat_int_t i;
	compat_uint_t d;
	int err;

	err = get_user(i, &data32->type);
	err |= put_user(i, &data->type);
	err |= get_user(d, &data32->value);
	err |= put_user(d, &data->value);
	return err;
}

static int compat_put_mipi_timing(struct compat_mipi_timing *data32,
				    struct MIPI_TIMING *data)
{
	compat_int_t i;
	compat_uint_t d;
	int err;

	err = get_user(i, &data->type);
	err |= put_user(i, &data32->type);
	err |= get_user(d, &data->value);
	err |= put_user(d, &data32->value);
	return err;
}

static int compat_get_pm_layer_en(struct compat_pm_layer_en *data32,
				  struct PM_LAYER_EN *data)
{
	compat_int_t i;
	int err = 0;
	int n = 0;

	for (n = 0; n < TOTAL_OVL_LAYER_NUM; ++n) {
		err |= get_user(i, &data32->layer_en[n]);
		err |= put_user(i, &data->layer_en[n]);
	}
	return err;
}

static int compat_put_pm_layer_en(struct compat_pm_layer_en *data32,
				  struct PM_LAYER_EN *data)
{
	compat_int_t i;
	int err = 0;
	int n = 0;

	for (n = 0; n < TOTAL_OVL_LAYER_NUM; ++n) {
		err |= get_user(i, &data->layer_en[n]);
		err |= put_user(i, &data32->layer_en[n]);
	}
	return err;
}

static int compat_get_pm_layer_info(struct compat_pm_layer_info *data32,
				    struct PM_LAYER_INFO *data)
{
	compat_int_t i;
	compat_uint_t d;
	int err;

	err = get_user(i, &data32->index);
	err |= put_user(i, &data->index);
	err |= get_user(i, &data32->height);
	err |= put_user(i, &data->height);
	err |= get_user(i, &data32->width);
	err |= put_user(i, &data->width);
	err |= get_user(i, &data32->fmt);
	err |= put_user(i, &data->fmt);
	err |= get_user(d, &data32->layer_size);
	err |= put_user(d, &data->layer_size);

	return err;
}

static int compat_put_pm_layer_info(struct compat_pm_layer_info *data32,
				    struct PM_LAYER_INFO *data)
{
	compat_int_t i;
	compat_uint_t d;
	int err;

	err = get_user(i, &data->index);
	err |= put_user(i, &data32->index);
	err |= get_user(i, &data->height);
	err |= put_user(i, &data32->height);
	err |= get_user(i, &data->width);
	err |= put_user(i, &data32->width);
	err |= get_user(i, &data->fmt);
	err |= put_user(i, &data32->fmt);
	err |= get_user(d, &data->layer_size);
	err |= put_user(d, &data32->layer_size);

	return err;
}

static int compat_get_esd_para(struct compat_esd_para *data32,
			       struct ESD_PARA *data)
{
	compat_int_t i;
	compat_uint_t d;
	int err;

	err = get_user(i, &data32->addr);
	err |= put_user(i, &data->addr);
	err |= get_user(i, &data32->type);
	err |= put_user(i, &data->type);
	err |= get_user(i, &data32->para_num);
	err |= put_user(i, &data->para_num);
	err |= get_user(d, &data32->esd_ret_buffer);
	err |= put_user((unsigned char *)(unsigned long)d, &data->esd_ret_buffer);

	return err;
}

static int compat_put_esd_para(struct compat_esd_para *data32,
			       struct ESD_PARA *data)
{
	compat_int_t i;
	compat_uint_t d;
	int err;

	err = get_user(i, &data->addr);
	err |= put_user(i, &data32->addr);
	err |= get_user(i, &data->type);
	err |= put_user(i, &data32->type);
	err |= get_user(i, &data->para_num);
	err |= put_user(i, &data32->para_num);
	err |= get_user(d, (compat_uint_t *)&data->esd_ret_buffer);
	err |= put_user(d, &data32->esd_ret_buffer);

	return err;
}

static long compat_fbconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_GET_DSI_ID:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, GET_DSI_ID, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_SET_DSI_ID:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, SET_DSI_ID, (unsigned long)data);

		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_LCM_TEST_DSI_CLK:
	{
		struct compat_lcm_type_fb __user *data32;
		struct LCM_TYPE_FB __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_lcm_type_fb(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_TEST_DSI_CLK, (unsigned long)data);
		err = compat_put_lcm_type_fb(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_LCM_GET_ID:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_ID, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return err ? err : 0;
	}
	case COMPAT_DRIVER_IC_CONFIG:
	{
		struct compat_config_record __user *data32;
		struct CONFIG_RECORD __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_config_record(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, DRIVER_IC_CONFIG, (unsigned long)data);
		err = compat_put_config_record(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_DRIVER_IC_CONFIG_DONE:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, DRIVER_IC_CONFIG_DONE, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_MIPI_SET_CC:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_CC, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}

	case COMPAT_LCM_GET_DSI_CONTINU:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_CONTINU, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_MIPI_SET_CLK:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_CLK, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_LCM_GET_DSI_CLK:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_CLK, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_MIPI_SET_SSC:
	{
		struct compat_dsi_ret *data32;
		struct DSI_RET __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_dsi_ret(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_SSC, (unsigned long)data);
		err = compat_put_dsi_ret(data32, data);
		return ret ? ret : err;
	}

	case COMPAT_LCM_GET_DSI_SSC:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_SSC, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_LCM_GET_DSI_LANE_NUM:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_LANE_NUM, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_LCM_GET_DSI_TE:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_int_t i;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(i, data32);
		err |= put_user(i, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_TE, (unsigned long)data);
		err |= get_user(i, data);
		err |= put_user(i, data32);
		return ret ? ret : err;
	}
	case COMPAT_LCM_GET_DSI_TIMING:
	{
		struct compat_mipi_timing *data32;
		struct MIPI_TIMING __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_mipi_timing(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_TIMING, (unsigned long)data);
		err = compat_put_mipi_timing(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_MIPI_SET_TIMING:
	{
		struct compat_mipi_timing *data32;
		struct MIPI_TIMING __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_mipi_timing(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_TIMING, (unsigned long)data);
		err = compat_put_mipi_timing(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_FB_LAYER_GET_EN:
	{
		struct compat_pm_layer_en *data32;
		struct PM_LAYER_EN __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_pm_layer_en(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, FB_LAYER_GET_EN, (unsigned long)data);
		err = compat_put_pm_layer_en(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_FB_LAYER_GET_INFO:
	{
		struct compat_pm_layer_info *data32;
		struct PM_LAYER_INFO __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_pm_layer_info(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, FB_LAYER_GET_INFO, (unsigned long)data);
		err = compat_put_pm_layer_info(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_FB_LAYER_DUMP:
	{
		ret = file->f_op->unlocked_ioctl(file, FB_LAYER_DUMP, (unsigned long)arg);
		return ret;
	}
	case COMPAT_LCM_GET_ESD:
	{
		struct compat_esd_para *data32;
		struct ESD_PARA __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = compat_get_esd_para(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_ESD, (unsigned long)data);
		err = compat_put_esd_para(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_TE_SET_ENABLE:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, TE_SET_ENABLE, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_DRIVER_IC_RESET:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, DRIVER_IC_RESET, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	case COMPAT_FB_GET_MISC:
	{
		compat_uint_t __user *data32;
		unsigned int __user *data;
		int err;
		compat_uint_t d;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, FB_GET_MISC, (unsigned long)data);
		err |= get_user(d, data);
		err |= put_user(d, data32);
		return ret ? ret : err;
	}
	default:
		return ret;
	}
}

#endif
/* end CONFIG_COMPAT */

static const struct file_operations fbconfig_fops = {
	.read = fbconfig_read,
	.write = fbconfig_write,
	.open = fbconfig_open,
	.unlocked_ioctl = fbconfig_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_fbconfig_ioctl,
#endif
	.release = fbconfig_release,
};

void PanelMaster_Init(void)
{
	ConfigPara_dbgfs = debugfs_create_file("fbconfig",
					       S_IFREG | S_IRUGO, NULL, (void *)0, &fbconfig_fops);

	INIT_LIST_HEAD(&head_list.list);
}

void PanelMaster_Deinit(void)
{
	debugfs_remove(ConfigPara_dbgfs);
}
