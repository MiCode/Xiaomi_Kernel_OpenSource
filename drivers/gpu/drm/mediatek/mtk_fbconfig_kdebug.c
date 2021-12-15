/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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


#include "mtk_fbconfig_kdebug.h"
#include "mtk_mipi_tx.h"
#include "mtk_drm_crtc.h"
#include "mtk_panel_ext.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"





/* ************************************************************************* */
/* This part is for customization parameters of D-IC and DSI . */
/* ************************************************************************* */



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
/* mipi video mode timing setting */
#define MIPI_SET_VM      FBCONFIG_IOW(54, unsigned int)
/* mipi non-continuous clock */
#define MIPI_SET_CC	 FBCONFIG_IOW(55, unsigned int)
/* spread frequency */
#define MIPI_SET_SSC	 FBCONFIG_IOW(56, unsigned int)
/* For div1,div2,fbk_div case */
#define MIPI_SET_CLK_V2  FBCONFIG_IOW(57, unsigned int)


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
#define DP_COLOR_BITS_PER_PIXEL(color) UFMT_GET_bpp(color)
#else
#define DP_COLOR_BITS_PER_PIXEL(color) ((0x0003FF00 & color) >>  8)
#endif




struct dentry *ConfigPara_dbgfs;
struct CONFIG_RECORD_LIST head_list;
struct LCM_REG_READ reg_read;


struct drm_device *drm_dev;
struct drm_crtc *crtc;
struct mtk_drm_crtc *mtk_crtc;
struct mtk_ddp_comp *output_comp;



struct PM_TOOL_S {
	enum DSI_INDEX dsi_id;
	struct LCM_REG_READ reg_read;
	struct mtk_panel_params *pMtk_panel_params;
};
static struct PM_TOOL_S pm_params = {
	.dsi_id = PM_DSI0,
	.pMtk_panel_params = NULL,
};

static void *pm_get_handle(void)
{
	return (void *)&pm_params;
}





static int fbconfig_open(struct inode *inode, struct file *file)
{
	struct PM_TOOL_S *pm_params = NULL;

	file->private_data = inode->i_private;
	pm_params = (struct PM_TOOL_S *) pm_get_handle();
	if (pm_params == NULL)
		return -EFAULT;
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return -EFAULT;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!(crtc->enabled) || mtk_crtc->ddp_mode == DDP_NO_USE)
		return -EFAULT;
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!output_comp)
		return -EFAULT;
	pm_params->pMtk_panel_params = mtk_drm_get_lcm_ext_params(crtc);
	switch (output_comp->id) {
	case DDP_COMPONENT_DSI0:
	{
		pm_params->dsi_id = PM_DSI0;
		break;
	}
	case DDP_COMPONENT_DSI1:
	{
		pm_params->dsi_id = PM_DSI1;
		break;
	}
	default:
		break;
	}

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

static ssize_t fbconfig_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
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

	if (!(crtc->state->active))
		return -EFAULT;

#ifdef FBCONFIG_SHOULD_KICK_IDLEMGR
	mtk_drm_idlemgr_kick(__func__, crtc, 1);
#endif
	switch (cmd) {
	case GET_DSI_ID:
	{
		put_user(dsi_id, (unsigned long *)argp);
		return 0;
	}
	case SET_DSI_ID:
	{
		if (arg > PM_DSI1)
			return -EINVAL;
		pm->dsi_id = arg;
		pr_debug("fbconfig=>SET_DSI_ID:%d\n", dsi_id);
		return 0;
	}
	case LCM_TEST_DSI_CLK:
	{
		struct LCM_TYPE_FB lcm_fb;

		lcm_fb.clock = pm->pMtk_panel_params->pll_clk;
		lcm_fb.lcm_type = fbconfig_mtk_dsi_get_mode_type(output_comp);

		pr_debug("fbconfig=>LCM_TEST_DSI_CLK:%d\n", ret);
		return copy_to_user(argp, &lcm_fb,
			sizeof(lcm_fb)) ? -EFAULT : 0;

	}
	case LCM_GET_ID:
	{
		/* not support anymore */
		return 0;
	}
	case DRIVER_IC_CONFIG:
	{
		/* not support anymore */
		return 0;
	}
	case DRIVER_IC_CONFIG_DONE:
	{
		/* not support anymore */
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
		if (enable > 1)
			return -EFAULT;
		Panel_Master_mipi_set_cc_entry(crtc, enable);
		return 0;
	}
	case LCM_GET_DSI_CONTINU:
	{
		uint32_t ret = Panel_Master_mipi_get_cc_entry(crtc);

		/*need to improve ,0 now means nothing but one parameter. */
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
		Panel_Master_dsi_config_entry(crtc, "PM_CLK", clk);
		return 0;
	}
	case LCM_GET_DSI_CLK:
	{
		uint32_t clk = pm->pMtk_panel_params->pll_clk;

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
		/* not support ssc in drm */
		/* Panel_Master_dsi_config_entry(crtc, "PM_SSC", dsi_ssc); */
		return 0;
	}
	case LCM_GET_DSI_SSC:
	{
		/* but drm framework not support ssc anymore */
		uint32_t ssc = pm->pMtk_panel_params->ssc_range;

		if (pm->pMtk_panel_params->ssc_disable)
			ssc = 0;
		return put_user(ssc, (unsigned long *)argp);
	}
	case LCM_GET_DSI_LANE_NUM:
	{
		uint32_t lanes_num;

		lanes_num = fbconfig_mtk_dsi_get_lanes_num(output_comp);
		pr_debug("Panel Master=>LCM_GET_DSI_Lane_num=>dsi:%d\r\n",
			lanes_num);
		return put_user(lanes_num, (unsigned long *)argp);
	}
	case LCM_GET_DSI_TE:
	{
		/* not support anymore */
		return 0;
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
		ret = Panel_Master_lcm_get_dsi_timing_entry(crtc, timing.type);
		pr_debug("fbconfig=>LCM_GET_DSI_TIMING:%d\n", ret);
		timing.value = ret;
		return copy_to_user(argp,
			&timing, sizeof(timing)) ? -EFAULT : 0;
	}
	case MIPI_SET_TIMING:
	{
		struct MIPI_TIMING timing;
		int ret = 0;

		if (!(crtc->state->active))
			return -EFAULT;
		if (copy_from_user(&timing, (void __user *)argp,
			sizeof(timing))) {
			pr_debug("[MIPI_SET_TIMING]: copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EFAULT;
		}
		ret = Panel_Master_mipi_set_timing_entry(crtc, timing);
		return ret;
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
			pr_debug(
				"[LCM_GET_ESD]: wrong esd_para.para_num= %d! line:%d\n",
				esd_para.para_num, __LINE__);
			return -EFAULT;
		}

		buffer = kzalloc(esd_para.para_num + 6, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		ret = fbconfig_get_esd_check_test(crtc, esd_para.addr,
				buffer, esd_para.para_num);
		if (ret < 0) {
			kfree(buffer);
			return -EFAULT;
		}
		ret = copy_to_user(esd_para.esd_ret_buffer,
			buffer, esd_para.para_num);
		kfree(buffer);
		return ret;

	}
	case TE_SET_ENABLE:
	{

		return 0;
	}
	case DRIVER_IC_RESET:
	{
		Panel_Master_dsi_config_entry(crtc, "PM_DRIVER_IC_RESET", 0);
		return 0;
	}
	case FB_GET_MISC:
	{
		/* not support anymore */
		return 0;
	}
	default:
		return ret;
	}
}

static int fbconfig_release(struct inode *inode, struct file *file)
{
	return 0;
}


static const struct file_operations fbconfig_fops = {
	.read = fbconfig_read,
	.write = fbconfig_write,
	.open = fbconfig_open,
	.unlocked_ioctl = fbconfig_ioctl,
/*
 * #ifdef CONFIG_COMPAT
 * waiting to designed
 * .compat_ioctl = compat_fbconfig_ioctl,
 * #endif
 */
	.release = fbconfig_release,
};


void PanelMaster_probe(void)
{
	ConfigPara_dbgfs = debugfs_create_file("fbconfig", S_IFREG | 0444,
		NULL, (void *)0, &fbconfig_fops);
	INIT_LIST_HEAD(&head_list.list);
}

void PanelMaster_Init(struct drm_device *dev)
{
	drm_dev = dev;
}

void PanelMaster_Deinit(void)
{
	debugfs_remove(ConfigPara_dbgfs);
}

