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

#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif

#include <linux/wait.h>
#include <linux/types.h>

#include "disp_drv_platform.h"
#include "mtkfb.h"
#include "debug.h"
#include "lcm_drv.h"
#include "ddp_path.h"
#include "fbconfig_kdebug.h"
#include "primary_display.h"
#include "ddp_ovl.h"
#include "ddp_dsi.h"
#include "ddp_irq.h"

/* #include "disp_drv.h" */
/* #include "lcd_drv.h" */

/* ********************************************************************* */
/* This part is for customization parameters of D-IC and DSI . */
/* ********************************************************************* */
bool fbconfig_start_LCM_config;
#define FBCONFIG_MDELAY(n)		(PM_lcm_utils_dsi0.mdelay((n)))
#define SET_RESET_PIN(v)		(PM_lcm_utils_dsi0.set_reset_pin((v)))
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		PM_lcm_utils_dsi0.dsi_set_cmdq(pdata, queue_size, force_update)
#define FBCONFIG_KEEP_NEW_SETTING	1
#define FBCONFIG_DEBUG			0

#define FBCONFIG_IOW(num, dtype)	_IOW('X', num, dtype)
#define FBCONFIG_IOR(num, dtype)	_IOR('X', num, dtype)
#define FBCONFIG_IOWR(num, dtype)	_IOWR('X', num, dtype)
#define FBCONFIG_IO(num)		_IO('X', num)

#define GET_DSI_ID		FBCONFIG_IOW(43, unsigned int)
#define SET_DSI_ID		FBCONFIG_IOW(44, unsigned int)
#define LCM_GET_ID		FBCONFIG_IOR(45, unsigned int)
#define LCM_GET_ESD		FBCONFIG_IOWR(46, unsigned int)
#define DRIVER_IC_CONFIG	FBCONFIG_IOR(47, unsigned int)
#define DRIVER_IC_CONFIG_DONE	FBCONFIG_IO(0)
#define DRIVER_IC_RESET		FBCONFIG_IOR(48, unsigned int)


#define MIPI_SET_CLK		FBCONFIG_IOW(51, unsigned int)
#define MIPI_SET_LANE		FBCONFIG_IOW(52, unsigned int)
#define MIPI_SET_TIMING		FBCONFIG_IOW(53, unsigned int)
/* mipi video mode timing setting */
#define MIPI_SET_VM		FBCONFIG_IOW(54, unsigned int)
/* mipi non-continuous clock */
#define MIPI_SET_CC		FBCONFIG_IOW(55, unsigned int)
/* spread frequency */
#define MIPI_SET_SSC		FBCONFIG_IOW(56, unsigned int)
/* For div1,div2,fbk_div case */
#define MIPI_SET_CLK_V2		FBCONFIG_IOW(57, unsigned int)


#define TE_SET_ENABLE		FBCONFIG_IOW(61, unsigned int)
#define FB_LAYER_DUMP		FBCONFIG_IOW(62, unsigned int)
#define FB_LAYER_GET_INFO	FBCONFIG_IOW(63, unsigned int)
#define FB_LAYER_GET_EN		FBCONFIG_IOW(64, unsigned int)
#define LCM_GET_ESD_RET		FBCONFIG_IOR(65, unsigned int)

#define LCM_GET_DSI_CONTINU	FBCONFIG_IOR(71, unsigned int)
#define LCM_GET_DSI_CLK		FBCONFIG_IOR(72, unsigned int)
#define LCM_GET_DSI_TIMING	FBCONFIG_IOR(73, unsigned int)
#define LCM_GET_DSI_LANE_NUM	FBCONFIG_IOR(74, unsigned int)
#define LCM_GET_DSI_TE		FBCONFIG_IOR(75, unsigned int)
#define LCM_GET_DSI_SSC		FBCONFIG_IOR(76, unsigned int)
#define LCM_GET_DSI_CLK_V2	FBCONFIG_IOR(77, unsigned int)
#define LCM_TEST_DSI_CLK	FBCONFIG_IOR(78, unsigned int)
#define FB_GET_MISC		FBCONFIG_IOR(80, unsigned int)

#ifdef UFMT_GET_bpp
#define DP_COLOR_BITS_PER_PIXEL(color)	UFMT_GET_bpp(color)
#else
#define DP_COLOR_BITS_PER_PIXEL(color)	((0x0003FF00 & color) >>  8)
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
struct dentry *ConfigPara_dbgfs;
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
struct proc_dir_entry *ConfigPara_procfs;
#endif

struct CONFIG_RECORD_LIST head_list;
struct LCM_REG_READ reg_read;

struct PM_TOOL_S {
	enum DSI_INDEX dsi_id;
	struct LCM_REG_READ reg_read;
	struct LCM_PARAMS *pLcm_params;
	struct LCM_DRIVER *pLcm_drv;
};
static struct PM_TOOL_S pm_params = {
	.dsi_id = PM_DSI0,
	.pLcm_params = NULL,
	.pLcm_drv = NULL,
};

static void *pm_get_handle(void)
{
	return (void *)&pm_params;
}

static enum DISP_MODULE_ENUM pm_get_dsi_handle(enum DSI_INDEX dsi_id)
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

int fbconfig_get_esd_check(enum DSI_INDEX dsi_id, uint32_t cmd,
			   uint8_t *buffer, uint32_t num)
{
	int array[4];
	int ret = 0;

	/* set max return packet size */
	/* array[0] = 0x00013700; */
	array[0] = 0x3700 + (num << 16);
	dsi_set_cmdq(array, 1, 1);
	atomic_set(&ESDCheck_byCPU, 1);
	if ((dsi_id == PM_DSI0) || (dsi_id == PM_DSI_DUAL))
		ret = DSI_dcs_read_lcm_reg_v2(pm_get_dsi_handle(PM_DSI0),
					      NULL, cmd, buffer, num);
	else if (dsi_id == PM_DSI1)
		ret = DSI_dcs_read_lcm_reg_v2(pm_get_dsi_handle(PM_DSI1),
					      NULL, cmd, buffer, num);
	atomic_set(&ESDCheck_byCPU, 0);
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
	struct CONFIG_RECORD_LIST *node;

	list_for_each_prev(p, &head_list.list) {
		node = list_entry(p, struct CONFIG_RECORD_LIST, list);
		switch (node->record.type) {
		case RECORD_CMD:
			dsi_set_cmdq(node->record.ins_array,
				     node->record.ins_num, 1);
			break;
		case RECORD_MS:
			FBCONFIG_MDELAY(node->record.ins_array[0]);
			break;
		case RECORD_PIN_SET:
			SET_RESET_PIN(node->record.ins_array[0]);
			break;
		default:
			pr_debug("sxk=>No such Type!!!!!\n");
			break;
		}
	}
}

static void free_list_memory(void)
{
	struct list_head *p, *n;
	struct CONFIG_RECORD_LIST *print;

	list_for_each_safe(p, n, &head_list.list) {
		print = list_entry(p, struct CONFIG_RECORD_LIST, list);
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
	struct PM_TOOL_S *pm_params;

	file->private_data = inode->i_private;
	pm_params = (struct PM_TOOL_S *)pm_get_handle();
	if (pm_params == NULL) {
		pr_debug("%s=>pm_params is empty!!\n", __func__);
		return -EFAULT;
	}
	PanelMaster_set_PM_enable(1);
	pm_params->pLcm_drv = DISP_GetLcmDrv();
	pm_params->pLcm_params = DISP_GetLcmPara();

	if (unlikely(pm_params->pLcm_params == NULL)) {
		pr_info("%s #%d pLcm_params is null\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	if (pm_params->pLcm_params->lcm_if == LCM_INTERFACE_DSI_DUAL)
		pm_params->dsi_id = PM_DSI_DUAL;
	else if (pm_params->pLcm_params->lcm_if == LCM_INTERFACE_DSI1)
		pm_params->dsi_id = PM_DSI1;
	return 0;
}


static char fbconfig_buffer[2048];

static ssize_t fbconfig_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(fbconfig_buffer) - 1;	/* 2047 */
	int n = 0;

	n += scnprintf(fbconfig_buffer + n, debug_bufmax - n, "sxkhome");
	fbconfig_buffer[n++] = 0;
	/* n = 5 ; */
	/* memcpy(fbconfig_buffer,"sxkhome",6); */
	return simple_read_from_buffer(ubuf, count, ppos, fbconfig_buffer, n);
}

static ssize_t fbconfig_write(struct file *file, const char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	return 0;
}

static long fbconfig_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct PM_TOOL_S *pm = (struct PM_TOOL_S *) pm_get_handle();
	uint32_t dsi_id = pm->dsi_id;
	struct LCM_DSI_PARAMS *pParams = get_dsi_params_handle(dsi_id);

#ifdef FBCONFIG_SHOULD_KICK_IDLEMGR
	primary_display_idlemgr_kick(__func__, 1);
#endif
	switch (cmd) {
	case GET_DSI_ID:
	{
		put_user(dsi_id, (unsigned long *)argp);
		return 0;
	}
	case SET_DSI_ID:
	{
		if (arg > PM_DSI_DUAL)
			return -EINVAL;
		pm->dsi_id = arg;
		pr_debug("fbconfig=>SET_DSI_ID:%d\n", dsi_id);

		return 0;
	}
	case LCM_TEST_DSI_CLK:
	{
		struct LCM_TYPE_FB lcm_fb;
		struct LCM_PARAMS *pLcm_params = pm->pLcm_params;

		lcm_fb.clock = pLcm_params->dsi.PLL_CLOCK;
		lcm_fb.lcm_type = pLcm_params->dsi.mode;

		pr_debug("fbconfig=>LCM_TEST_DSI_CLK:%d\n", ret);
		return copy_to_user(argp, &lcm_fb,
			sizeof(lcm_fb)) ? -EFAULT : 0;
	}
	case LCM_GET_ID:
	{
		/* LCM_DRIVER *pLcm_drv = pm->pLcm_drv; */
		unsigned int lcm_id = 0;
#if 0
		if (pLcm_drv != NULL)
			lcm_id = pLcm_drv->get_lcm_id();
		else
			pr_debug("fbconfig=>LCM_GET_ID:%x\n", lcm_id);
#endif
		return copy_to_user(argp, &lcm_id,
				    sizeof(lcm_id)) ? -EFAULT : 0;
	}
	case DRIVER_IC_CONFIG:
	{
		struct CONFIG_RECORD_LIST *record_tmp_list =
				kmalloc(sizeof(*record_tmp_list), GFP_KERNEL);
		if (record_tmp_list == NULL)
			return -ENOMEM;

		if (copy_from_user(&record_tmp_list->record, (void __user *)arg,
				   sizeof(struct CONFIG_RECORD))) {
			pr_debug("list_add: copy_from_user failed! line:%d\n",
				 __LINE__);
			kfree(record_tmp_list);
			record_tmp_list = NULL;
			return -EFAULT;
		}
		list_add(&record_tmp_list->list, &head_list.list);
		return 0;
	}
	case DRIVER_IC_CONFIG_DONE:
	{
		/* print_from_head_to_tail(); */
		Panel_Master_dsi_config_entry("PM_DDIC_CONFIG", NULL);
		/* free the memory ..... */
		free_list_memory();
		return 0;
	}
	case MIPI_SET_CC:
	{
		uint32_t enable = 0;

		if (get_user(enable, (uint32_t __user *) argp)) {
			pr_debug("[MIPI_SET_CC]: copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EFAULT;
		}

		PanelMaster_set_CC(dsi_id, enable);
		return 0;
	}
	case LCM_GET_DSI_CONTINU:
	{
		uint32_t ret = PanelMaster_get_CC(dsi_id);
		/* need to improve ,0 now means nothing but one parameter. */
		pr_debug("LCM_GET_DSI_CONTINU=>DSI: %d\n", ret);
		return put_user(ret, (unsigned long *)argp);
	}
	case MIPI_SET_CLK:
	{
		uint32_t clk = 0;

		if (get_user(clk, (uint32_t __user *) argp)) {
			pr_debug("[MIPI_SET_CLK]: copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EFAULT;
		}

		pr_debug("LCM_GET_DSI_CLK=>dsi:%d\n", clk);
		Panel_Master_dsi_config_entry("PM_CLK", &clk);
		return 0;
	}
	case LCM_GET_DSI_CLK:
	{
		uint32_t clk = pParams->PLL_CLOCK;

		pr_debug("LCM_GET_DSI_CLK=>dsi:%d\n", clk);
		return put_user(clk, (unsigned long *)argp);
	}
	case MIPI_SET_SSC:
	{
		struct DSI_RET dsi_ssc;

		if (copy_from_user(&dsi_ssc, (void __user *)argp,
			sizeof(dsi_ssc))) {
			pr_debug("[MIPI_SET_SSC]: copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EFAULT;
		}

		pr_debug("Pmaster:set mipi ssc line:%d\n", __LINE__);
		Panel_Master_dsi_config_entry("PM_SSC", &dsi_ssc);
		return 0;
	}
	case LCM_GET_DSI_SSC:
	{
		uint32_t ssc = pParams->ssc_range;

		if (pParams->ssc_disable)
			ssc = 0;
		return put_user(ssc, (unsigned long *)argp);
	}
	case LCM_GET_DSI_LANE_NUM:
	{
		uint32_t lane_num = pParams->LANE_NUM;

		pr_debug("Panel Master=>LCM_GET_DSI_Lane_num=>dsi:%d\r\n",
			 lane_num);
		return put_user(lane_num, (unsigned long *)argp);
	}
	case LCM_GET_DSI_TE:
	{
		int ret;

		ret = PanelMaster_get_TE_status(dsi_id);
		pr_debug("fbconfig=>LCM_GET_DSI_TE:%d\n", ret);
		return put_user(ret, (unsigned long *)argp);
	}
	case LCM_GET_DSI_TIMING:
	{
		uint32_t ret;
		struct MIPI_TIMING timing;

		if (copy_from_user(&timing, (void __user *)argp,
				   sizeof(timing))) {
			pr_debug("[MIPI_GET_TIMING]: copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EFAULT;
		}
		ret = PanelMaster_get_dsi_timing(dsi_id, timing.type);
		pr_debug("fbconfig=>LCM_GET_DSI_TIMING:%d\n", ret);
		timing.value = ret;
		return copy_to_user(argp, &timing,
				    sizeof(timing)) ? -EFAULT : 0;
	}
	case MIPI_SET_TIMING:
	{
		struct MIPI_TIMING timing;

		if (primary_display_is_sleepd())
			return -EFAULT;
		if (copy_from_user(&timing, (void __user *)argp,
				   sizeof(timing))) {
			pr_debug("[MIPI_SET_TIMING]: copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EFAULT;
		}

		PanelMaster_DSI_set_timing(dsi_id, timing);
		return 0;
	}
	case FB_LAYER_GET_EN:
	{
		pr_debug("[FB_LAYER_GET_EN] not support any more\n");
		return 0;
	}
	case FB_LAYER_GET_INFO:
	{
		pr_debug("[FB_LAYER_GET_INFO] not support any more\n");
		return  0;
	}

	case FB_LAYER_DUMP:
	{
		pr_debug("[FB_LAYER_DUMP] not support any more\n");
		return  0;
	}

	case LCM_GET_ESD:
	{
		struct ESD_PARA esd_para;
		uint8_t *buffer;

		if (copy_from_user(&esd_para, (void __user *)arg,
				   sizeof(esd_para))) {
			pr_debug("[LCM_GET_ESD]: copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EFAULT;
		}

		if (esd_para.para_num < 0 || esd_para.para_num > 0x30) {
			pr_debug("[LCM_GET_ESD]: wrong esd_para.para_num= %d! line:%d\n",
						 esd_para.para_num, __LINE__);
			return -EFAULT;
		}

		buffer = kzalloc(esd_para.para_num + 6, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;

		ret = fbconfig_get_esd_check_test(dsi_id, esd_para.addr, buffer,
						  esd_para.para_num);
		if (ret < 0) {
			kfree(buffer);
			return -EFAULT;
		}
		ret = copy_to_user(esd_para.esd_ret_buffer, buffer,
				   esd_para.para_num);
		kfree(buffer);
		return ret;
	}
	case TE_SET_ENABLE:
	{
		uint32_t te_enable = 0;

		if (get_user(te_enable, (unsigned long *)argp))
			return -EFAULT;

		return 0;
	}
	case DRIVER_IC_RESET:
	{
		Panel_Master_dsi_config_entry("DRIVER_IC_RESET", NULL);
		return 0;
	}
	case FB_GET_MISC:
	{
		struct misc_property misc = { 0 };

		if (pm->pLcm_params->lcm_if == LCM_INTERFACE_DSI_DUAL)
			misc.dual_port = 1;
		misc.overall_layer_num = TOTAL_OVL_LAYER_NUM;
		ret = copy_to_user(argp, &misc,  sizeof(misc));
		return 0;
	}
	default:
		return ret;
	}
}

static int fbconfig_release(struct inode *inode, struct file *file)
{
	PanelMaster_set_PM_enable(0);

	return 0;
}

/* compat-ioctl */
#ifdef CONFIG_COMPAT

#define COMPAT_GET_DSI_ID		FBCONFIG_IOW(43, compat_uint_t)
#define COMPAT_SET_DSI_ID		FBCONFIG_IOW(44, compat_uint_t)
#define COMPAT_LCM_GET_ID		FBCONFIG_IOR(45, compat_uint_t)
#define COMPAT_LCM_GET_ESD		FBCONFIG_IOWR(46, compat_uint_t)
#define COMPAT_DRIVER_IC_CONFIG		FBCONFIG_IOR(47, compat_uint_t)
#define COMPAT_DRIVER_IC_CONFIG_DONE	FBCONFIG_IO(0)
#define COMPAT_DRIVER_IC_RESET		FBCONFIG_IOR(48, compat_uint_t)

#define COMPAT_MIPI_SET_CLK		FBCONFIG_IOW(51, compat_uint_t)
#define COMPAT_MIPI_SET_LANE		FBCONFIG_IOW(52, compat_uint_t)
#define COMPAT_MIPI_SET_TIMING		FBCONFIG_IOW(53, compat_uint_t)
#define COMPAT_MIPI_SET_VM		FBCONFIG_IOW(54, compat_uint_t)
#define COMPAT_MIPI_SET_CC		FBCONFIG_IOW(55, compat_uint_t)
#define COMPAT_MIPI_SET_SSC		FBCONFIG_IOW(56, compat_uint_t)
#define COMPAT_MIPI_SET_CLK_V2		FBCONFIG_IOW(57, compat_uint_t)

#define COMPAT_TE_SET_ENABLE		FBCONFIG_IOW(61, compat_uint_t)
#define COMPAT_FB_LAYER_DUMP		FBCONFIG_IOW(62, compat_uint_t)
#define COMPAT_FB_LAYER_GET_INFO	FBCONFIG_IOW(63, compat_uint_t)
#define COMPAT_FB_LAYER_GET_EN		FBCONFIG_IOW(64, compat_uint_t)
#define COMPAT_LCM_GET_ESD_RET		FBCONFIG_IOR(65, compat_uint_t)

#define COMPAT_LCM_GET_DSI_CONTINU	FBCONFIG_IOR(71, compat_uint_t)
#define COMPAT_LCM_GET_DSI_CLK		FBCONFIG_IOR(72, compat_uint_t)
#define COMPAT_LCM_GET_DSI_TIMING	FBCONFIG_IOR(73, compat_uint_t)
#define COMPAT_LCM_GET_DSI_LANE_NUM	FBCONFIG_IOR(74, compat_uint_t)
#define COMPAT_LCM_GET_DSI_TE		FBCONFIG_IOR(75, compat_uint_t)
#define COMPAT_LCM_GET_DSI_SSC		FBCONFIG_IOR(76, compat_uint_t)
#define COMPAT_LCM_GET_DSI_CLK_V2	FBCONFIG_IOR(77, compat_uint_t)
#define COMPAT_LCM_TEST_DSI_CLK		FBCONFIG_IOR(78, compat_uint_t)
#define COMPAT_FB_GET_MISC		FBCONFIG_IOR(80, compat_uint_t)

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
	err |= put_user((unsigned char *)(unsigned long)d,
			&data->esd_ret_buffer);

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

static long compat_fbconfig_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, GET_DSI_ID,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, SET_DSI_ID,
						 (unsigned long)data);

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
		if (data == NULL)
			return -EFAULT;

		err = compat_get_lcm_type_fb(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_TEST_DSI_CLK,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_ID,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = compat_get_config_record(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, DRIVER_IC_CONFIG,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, DRIVER_IC_CONFIG_DONE,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_CC,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_CONTINU,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_CLK,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_CLK,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = compat_get_dsi_ret(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_SSC,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_SSC,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_LANE_NUM,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(i, data32);
		err |= put_user(i, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_TE,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = compat_get_mipi_timing(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_DSI_TIMING,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = compat_get_mipi_timing(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, MIPI_SET_TIMING,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = compat_get_pm_layer_en(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, FB_LAYER_GET_EN,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = compat_get_pm_layer_info(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, FB_LAYER_GET_INFO,
						 (unsigned long)data);
		err = compat_put_pm_layer_info(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_FB_LAYER_DUMP:
	{
		ret = file->f_op->unlocked_ioctl(file, FB_LAYER_DUMP,
						 (unsigned long)arg);
		return ret;
	}
	case COMPAT_LCM_GET_ESD:
	{
		struct compat_esd_para *data32;
		struct ESD_PARA __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_esd_para(data32, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, LCM_GET_ESD,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, TE_SET_ENABLE,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, DRIVER_IC_RESET,
						 (unsigned long)data);
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
		if (data == NULL)
			return -EFAULT;

		err = get_user(d, data32);
		err |= put_user(d, data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, FB_GET_MISC,
						 (unsigned long)data);
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
#if IS_ENABLED(CONFIG_DEBUG_FS)
#if defined(CONFIG_MTK_ENG_BUILD)
	ConfigPara_dbgfs = debugfs_create_file("fbconfig", S_IFREG | 0444,
					       NULL, (void *)0, &fbconfig_fops);
#endif
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	ConfigPara_procfs = proc_create("fbconfig", 0640,
				NULL,
				&fbconfig_fops);

	if (!ConfigPara_procfs) {
		pr_info("[%s %d] failed to register fbconfig in proc/display_ddp\n",
			__func__, __LINE__);
		return;
	}

	INIT_LIST_HEAD(&head_list.list);

#endif

}

void PanelMaster_Deinit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(ConfigPara_dbgfs);
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	if (ConfigPara_procfs) {
		proc_remove(ConfigPara_procfs);
		ConfigPara_procfs = NULL;
	}
#endif
}
