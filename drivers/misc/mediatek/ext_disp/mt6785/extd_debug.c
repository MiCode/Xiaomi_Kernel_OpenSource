/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#if (defined(CONFIG_MTK_HDMI_SUPPORT)) ||	\
	(defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2))
#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
/* #include <mach/mt_typedefs.h> */
#include <linux/types.h>
/* #include <mach/mt_gpio.h> */
/* #include <mt-plat/mt_gpio.h> */
/* #include <cust_gpio_usage.h> */
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
#include "m4u.h"
#endif
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include "ddp_hal.h"
#include "ddp_reg.h"
#include "ddp_info.h"
#include "extd_hdmi.h"
#include "external_display.h"
#include "extd_multi_control.h"
#include "mtk_disp_mgr.h"
#include "DpDataType.h"
#include "disp_dts_gpio.h"

static const char STR_HELP[] =
	"\n"
	"USAGE\n"
	"        echo [ACTION]... > /d/extd\n"
	"\n"
	"ACTION\n"
	"        fakecablein:[enable|disable]\n"
	"	fake mhl cable in/out\n"
	"\n"
	"        echo [ACTION]... > /d/extd\n"
	"\n"
	"ACTION\n"
	"        force_res:0xffff\n"
	"	fix resolution or 3d enable(high 16 bit)\n"
	"\n";

/* TODO: this is a temp debug solution */
/* extern void hdmi_cable_fake_plug_in(void); */
/* extern int hdmi_drv_init(void); */

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
#define LCM_WIDTH	1080
#define LCM_HEIGHT	1920
#define BUFFER_COUNT	1

unsigned long buffer_va, buffer_mva;
unsigned long buffer_va_1, buffer_mva_1;

static int test_allocate_buffer(void)
{
	m4u_client_t *client = NULL;
	int ret = 0;
	int pixelSize = LCM_WIDTH * LCM_HEIGHT;
	int imageSize = pixelSize * 4;
	int bufferSize = imageSize * BUFFER_COUNT;

	pr_debug("%s\n", __func__);
	if (buffer_va) {
		pr_debug("buffer has allocated, return in %d\n", __LINE__);
		return 0;
	}

	buffer_va = (unsigned long)vmalloc(bufferSize);
	buffer_va_1 = (unsigned long)vmalloc(bufferSize);
	if (!buffer_va || !buffer_va_1) {
		pr_debug("vmalloc %d bytes fail!!!\n", bufferSize);
		return -1;
	}

	client = m4u_create_client();
	ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL1, buffer_va, 0,
			    bufferSize, M4U_PROT_READ | M4U_PROT_WRITE, 0,
			    (unsigned int *)(&buffer_mva));
	ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL1, buffer_va_1, 0,
			    bufferSize, M4U_PROT_READ | M4U_PROT_WRITE, 0,
			    (unsigned int *)(&buffer_mva_1));

	pr_debug("buffer_va:0x%lx, buffer_mva=0x%lx, size %d\n", buffer_va,
		 buffer_mva, bufferSize);

	return 0;
}

static void draw_block(unsigned long addr, unsigned int x, unsigned int y,
		       unsigned int w, unsigned int h, unsigned int color)
{
	int i = 0;
	int j = 0;
	unsigned long start_addr = addr + LCM_WIDTH * 4 * y + x * 4;

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++)
			*(unsigned int *)(start_addr + i * 4 +
					  j * LCM_WIDTH * 4) = color;
	}
}

static int internal_test(unsigned long va, unsigned int w, unsigned int h,
			 unsigned int odd)
{
	unsigned int i = 0;
	unsigned int color = 0;
	int _internal_test_block_h = 240;
	int _internal_test_block_w = 216;
	int block_cnts = 0;

	block_cnts = w * h / _internal_test_block_h / _internal_test_block_w;

	/* this is for debug */
	pr_debug("_mtkfb_internal_test, block counts:%d\n", block_cnts);
	for (i = 0; i < block_cnts; i++) {
		color = ((i + odd) & 0x1) * 0xff;
		/* color += ((i&0x2)>>1)*0xff00; */
		/* color += ((i&0x4)>>2)*0xff0000; */
		color += 0xff000000U;
		draw_block(va,
			   i % (w / _internal_test_block_w) *
			   _internal_test_block_w,
			   i / (w / _internal_test_block_w) *
			   _internal_test_block_h, _internal_test_block_w,
			   _internal_test_block_h, color);
	}

	return 0;
}

static void config_ovl(struct disp_frame_cfg_t *cfg, unsigned int offset)
{
	unsigned long pa;

	cfg->setter = SESSION_USER_HWC;
	cfg->session_id = 0x20003;
	cfg->input_layer_num = 1;
	cfg->tigger_mode = TRIGGER_NORMAL;
	cfg->mode = DISP_SESSION_DIRECT_LINK_MODE;

	pa = offset ? buffer_mva_1 : buffer_mva;

	memset(&(cfg->input_cfg[0]), 0, sizeof(struct disp_input_config));
	cfg->input_cfg[0].layer_id = 0;
	cfg->input_cfg[0].layer_enable = 1;
	cfg->input_cfg[0].src_fmt = DISP_FORMAT_BGRA8888;
	cfg->input_cfg[0].src_phy_addr = (void *)pa;
	cfg->input_cfg[0].src_offset_x = 0;
	cfg->input_cfg[0].src_offset_y = 0;
	cfg->input_cfg[0].src_width = LCM_WIDTH;
	cfg->input_cfg[0].src_height = LCM_HEIGHT;
	cfg->input_cfg[0].src_pitch = LCM_WIDTH;
	cfg->input_cfg[0].tgt_offset_x = 0;
	cfg->input_cfg[0].tgt_offset_y = 0;
	cfg->input_cfg[0].tgt_width = LCM_WIDTH;
	cfg->input_cfg[0].tgt_height = LCM_HEIGHT;
	cfg->input_cfg[0].alpha_enable = 1;
	cfg->input_cfg[0].alpha = 0xff;
	cfg->input_cfg[0].ext_sel_layer = -1;
}
#endif

static void process_dbg_opt(const char *opt)
{
	if (strncmp(opt, "on", 2) == 0) {
#if defined(CONFIG_MTK_HDMI_SUPPORT)
		hdmi_power_on();
	} else if (strncmp(opt, "off", 3) == 0) {
		hdmi_power_off();
	} else if (strncmp(opt, "suspend", 7) == 0) {
		hdmi_suspend();
	} else if (strncmp(opt, "resume", 6) == 0) {
		hdmi_resume();
	} else if (strncmp(opt, "fakecablein:", 12) == 0) {
		if (strncmp(opt + 12, "enable", 6) == 0)
			hdmi_cable_fake_plug_in();
		else if (strncmp(opt + 12, "disable", 7) == 0)
			hdmi_cable_fake_plug_out();
		else
			goto Error;
	} else if (strncmp(opt, "force_res:", 10) == 0) {
		char *p = NULL;
		unsigned int res = 0;
		int ret = 0;

		p = (char *)opt + 10;
		ret = kstrtouint(p, 0, &res);
		hdmi_force_resolution(res);
	} else if (strncmp(opt, "hdmireg", 7) == 0) {
		ext_disp_diagnose();
	} else if (strncmp(opt, "I2S1:", 5) == 0) {
#ifdef GPIO_MHL_I2S_OUT_WS_PIN
		if (strncmp(opt + 5, "on", 2) == 0) {
			pr_debug("[hdmi][Debug] Enable I2S1\n");
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_WS_PIN, GPIO_MODE_01);
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_CK_PIN, GPIO_MODE_01);
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN,
					 GPIO_MODE_01);
		} else if (strncmp(opt + 5, "off", 3) == 0) {
			pr_debug("[hdmi][Debug] Disable I2S1\n");
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_WS_PIN, GPIO_MODE_02);
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_CK_PIN, GPIO_MODE_01);
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN,
					 GPIO_MODE_02);
		}
#endif
	} else if (strncmp(opt, "forceon", 7) == 0) {
		hdmi_force_on(false);
	} else if (strncmp(opt, "extd_vsync:", 11) == 0) {
		if (strncmp(opt + 11, "on", 2) == 0)
			hdmi_wait_vsync_debug(1);
		else if (strncmp(opt + 11, "off", 3) == 0)
			hdmi_wait_vsync_debug(0);
	} else if (strncmp(opt, "dumpReg", 7) == 0) {
		hdmi_dump_vendor_chip_register();
	} else if (strncmp(opt, "extd_ut:", 8) == 0) {
		if (strncmp(opt + 8, "on", 2) == 0)
			enable_ut = 1;
		else if (strncmp(opt + 8, "off", 3) == 0)
			enable_ut = 0;
	} else if ((strncmp(opt, "dbgtype:", 8) == 0) ||
		   (strncmp(opt, "regw:", 5) == 0) ||
		   (strncmp(opt, "regr:", 5) == 0) ||
		   (strncmp(opt, "hdcp:", 5) == 0) ||
		   (strncmp(opt, "status", 6) == 0) ||
		   (strncmp(opt, "help", 4) == 0) ||
		   (strncmp(opt, "res:", 4) == 0) ||
		   (strncmp(opt, "edid", 4) == 0) ||
		   (strncmp(opt, "deepcolor:", 10) == 0) ||
		   (strncmp(opt, "irq:", 4) == 0)) {
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
		mt_hdmi_debug_write(opt);
#endif
#else /* !CONFIG_MTK_HDMI_SUPPORT */
		pr_debug("[Debug] Not enable 'CONFIG_MTK_HDMI_SUPPORT'\n");
#endif /* CONFIG_MTK_HDMI_SUPPORT */
	} else if (strncmp(opt, "duallcm:on", 10) == 0) {
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
		unsigned int frame_cnts = 0;
		unsigned int image_size = 0;
		unsigned int created_session[5];
		struct disp_session_info info;
		struct disp_session_config config;
		struct disp_session_vsync_config vsync_config;
		struct disp_frame_cfg_t *cfg = NULL;

		pr_debug("[Debug] dual lcm test debug duallcm:on start!!!\n");

		cfg = kzalloc(sizeof(struct disp_frame_cfg_t), GFP_KERNEL);

		created_session[0] = 0x20003;
		config.type = 2;
		config.device_id = 3;
		external_display_get_info((void *)&info, 0x20003);
		pr_debug("[Debug] get device info, width:%d, height:%d, mode:%d\n",
			 info.displayWidth, info.displayHeight,
			 info.displayMode);
		pr_debug("[Debug] create path in +\n");
		disp_create_session(&config);
		external_display_switch_mode(1, created_session, 0x20003);
		pr_debug("[Debug] create path out -\n");

		if (test_allocate_buffer() < 0)
			goto Error;

		do {
			ext_disp_wait_for_vsync((void *)&vsync_config, 0x20003);
			internal_test(buffer_va, 1080, 1920, 0);
			pr_debug("[Debug] config ovl !\n");
			config_ovl(cfg, 0);
			external_display_frame_cfg(cfg);

			msleep(500);

			ext_disp_wait_for_vsync((void *)&vsync_config, 0x20003);
			image_size = LCM_WIDTH * LCM_HEIGHT * 4;
			internal_test(buffer_va_1, 1080, 1920, 1);
			pr_debug("[Debug] config ovl !\n");
			config_ovl(cfg, 1);
			external_display_frame_cfg(cfg);
			msleep(500);
			frame_cnts++;

			if (frame_cnts == 15) {
				disp_dts_gpio_select_state
				    (DTS_GPIO_STATE_LCM1_RST_OUT1);
				msleep(20);
				disp_dts_gpio_select_state
				    (DTS_GPIO_STATE_LCM1_RST_OUT0);
				msleep(20);
				disp_dts_gpio_select_state
				    (DTS_GPIO_STATE_LCM1_RST_OUT1);
			}
		} while (frame_cnts < 20);

		kfree(cfg);
		pr_debug("[Debug] dual lcm test debug duallcm:on done!!!\n");
	} else if (strncmp(opt, "duallcm:suspend", 15) == 0) {
		pr_debug
		    ("[Debug] dual lcm test debug duallcm:suspend start!!!\n");
		external_display_suspend(0x20003);
		pr_debug
		    ("[Debug] dual lcm test debug duallcm:suspend done!!!\n");
	} else if (strncmp(opt, "lcm1_reset", 10) == 0) {
		pr_debug("[EXTD] LCM1 reset !\n");
		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM1_RST_OUT1);
		msleep(20);
		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM1_RST_OUT0);
		msleep(20);
		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM1_RST_OUT1);
	} else if (strncmp(opt, "duallcm:resume", 14) == 0) {
		struct disp_session_vsync_config vsync_config;
		struct disp_frame_cfg_t *cfg = NULL;
		unsigned int frame_cnts = 0;
		unsigned int image_size = 0;

		pr_debug("[Debug] dual lcm test debug duallcm:resume start!!!\n");
		external_display_resume(0x20003);

		cfg = kzalloc(sizeof(struct disp_frame_cfg_t), GFP_KERNEL);

		if (test_allocate_buffer() < 0)
			goto Error;

		msleep(500);

		do {
			ext_disp_wait_for_vsync((void *)&vsync_config, 0x20003);
			internal_test(buffer_va, 1080, 1920, 0);
			pr_debug("[Debug] config ovl !\n");
			config_ovl(cfg, 0);
			external_display_frame_cfg(cfg);

			msleep(500);

			ext_disp_wait_for_vsync((void *)&vsync_config, 0x20003);
			image_size = LCM_WIDTH * LCM_HEIGHT * 4;
			internal_test(buffer_va_1, 1080, 1920, 1);
			pr_debug("[Debug] config ovl !\n");
			config_ovl(cfg, 1);
			external_display_frame_cfg(cfg);
			msleep(500);
			frame_cnts++;

			if (frame_cnts == 15) {
				disp_dts_gpio_select_state(
						DTS_GPIO_STATE_LCM1_RST_OUT1);
				msleep(20);
				disp_dts_gpio_select_state(
						DTS_GPIO_STATE_LCM1_RST_OUT0);
				msleep(20);
				disp_dts_gpio_select_state(
						DTS_GPIO_STATE_LCM1_RST_OUT1);
			}
		} while (frame_cnts < 20);

		kfree(cfg);

		pr_debug("[Debug] dual lcm test debug duallcm:resume done!\n");
	} else if (strncmp(opt, "duallcm:off", 11) == 0) {
		struct disp_session_config config;

		config.session_id = 0x20003;

		pr_debug("[Debug] deinit path in +\n");
		/* LCM needn't destroy session */
		disp_destroy_session(&config);
		pr_debug("[Debug] dual lcm test debug duallcm:off!!!\n");
#else
		pr_debug("[Debug] no enable 'CONFIG_MTK_DUAL_DISPLAY_SUPPORT=2'\n");
#endif
	} else {
		goto Error;
	}

	return;

Error:
	pr_debug("[extd] parse command error!\n\n%s", STR_HELP);
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_debug("[extd] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* Debug FileSystem Routines */
struct dentry *extd_dbgfs;

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char debug_buffer[2048];

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	int n = 0;

	n += scnprintf(debug_buffer + n, debug_bufmax - n, STR_HELP);
	debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}

static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

void extd_dbg_init(void)
{
	extd_dbgfs = debugfs_create_file("extd", S_IFREG | 0444, NULL,
					 (void *)0, &debug_fops);
}

void extd_dbg_deinit(void)
{
	debugfs_remove(extd_dbgfs);
}

#endif
