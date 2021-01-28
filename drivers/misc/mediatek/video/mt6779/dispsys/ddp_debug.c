// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "DEBUG"

#include <linux/string.h>
#include <linux/uaccess.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif

#ifdef CONFIG_MTK_AEE_FEATURE
#include "mt-plat/aee.h"
#endif
#include "disp_assert_layer.h"
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/types.h>

#if defined(CONFIG_MTK_M4U)
#include "m4u.h"
#endif

#include "disp_drv_ddp.h"

#include "ddp_debug.h"
#include "ddp_reg.h"
#include "ddp_drv.h"
#include "ddp_wdma.h"
#include "ddp_wdma_ex.h"
#include "ddp_hal.h"
#include "ddp_path.h"
#include "ddp_color.h"
#include "ddp_aal.h"
#include "ddp_pwm.h"
#include <ddp_od.h>
#include "ddp_dither.h"
#include "ddp_info.h"
#include "ddp_dsi.h"
#include "ddp_rdma.h"
#include "ddp_rdma_ex.h"
#include "ddp_manager.h"
#include "ddp_log.h"
#include "ddp_met.h"
#include "display_recorder.h"
#include "disp_session.h"
#include "disp_lowpower.h"
#include "disp_drv_log.h"
#include "disp_cust.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *debugfs;// file: /d/dispsys
static struct dentry *debugDir;//dir: /d/disp/

static struct dentry *debugfs_dump; //file: /d/disp/dump
static int debug_init;
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
//file: /proc/dispsys
static struct proc_dir_entry *dispsys_procfs;
//dir: /proc/disp/
static struct proc_dir_entry *disp_dir_procfs;
//file: /proc/disp/dump
static struct proc_dir_entry *disp_dump_procfs;
//file: /proc/disp/lowpowermode
static struct proc_dir_entry *disp_lpmode_procfs;
static int debug_procfs_init;
#endif

static const long DEFAULT_LOG_FPS_WND_SIZE = 30;

unsigned char pq_debug_flag;
unsigned char aal_debug_flag;

static unsigned int dbg_log_level;
static unsigned int irq_log_level;
static unsigned int dump_to_buffer;
static enum ONESHOT_DUMP_STAGE oneshot_dump = ONESHOT_DUMP_INIT;

static int dbg_force_roi;
static int dbg_partial_x;
static int dbg_partial_y;
static int dbg_partial_w;
static int dbg_partial_h;
static int dbg_partial_statis;

unsigned int gUltraEnable = 1;
static char STR_HELP[] =
	"USAGE:\n"
	"       echo [ACTION]>/d/dispsys\n"
	"ACTION:\n"
	"       regr:addr\n              :regr:0xf400c000\n"
	"       regw:addr,value          :regw:0xf400c000,0x1\n"
	"       dbg_log:0|1|2            :0 off, 1 dbg, 2 all\n"
	"       irq_log:0|1              :0 off, !0 on\n"
	"       met_on:[0|1],[0|1],[0|1] :fist[0|1]on|off,other [0|1]direct|decouple\n"
	"       backlight:level\n"
	"       dump_aal:arg\n"
	"       mmp\n"
	"       dump_reg:moduleID\n dump_path:mutexID\n  dpfd_ut1:channel\n";


/* ------------------------------------------------------------------------- */
/* Command Processor */
/* ------------------------------------------------------------------------- */
static int low_power_cust_mode = LP_CUST_DISABLE;
static unsigned int vfp_backup;

int get_lp_cust_mode(void)
{
	return low_power_cust_mode;
}

void backup_vfp_for_lp_cust(unsigned int vfp)
{
	vfp_backup = vfp;
}

unsigned int get_backup_vfp(void)
{
	return vfp_backup;
}

static char dbg_buf[2048];

static void process_dbg_opt(const char *opt)
{
	char *buf = dbg_buf + strlen(dbg_buf);
	int ret;
	int buf_size_left = ARRAY_SIZE(dbg_buf) - strlen(dbg_buf) - 10;

#ifdef CONFIG_MTK_ENG_BUILD
	if (strncmp(opt, "get_reg", 7) == 0) {
		unsigned long pa;
		unsigned int *va;

		ret = sscanf(opt, "get_reg:0x%lx\n", &pa);
		if (ret != 1) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		va = ioremap(pa, 4);
		DDPMSG("get_reg: 0x%lx = 0x%08X\n", pa, DISP_REG_GET(va));
		snprintf(buf, buf_size_left, "get_reg: 0x%lx = 0x%08X\n",
			 pa, DISP_REG_GET(va));
		iounmap(va);
		return;
	}

	if (strncmp(opt, "set_reg", 7) == 0) {
		unsigned long pa;
		unsigned int *va, val;

		ret = sscanf(opt, "set_reg:0x%lx,0x%x\n", &pa, &val);
		if (ret != 2) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		va = ioremap(pa, 4);
		DISP_CPU_REG_SET(va, val);
		DDPMSG("set_reg: 0x%lx = 0x%08X(0x%x)\n",
		       pa, DISP_REG_GET(va), val);
		snprintf(buf, buf_size_left, "set_reg: 0x%lx = 0x%08X(0x%x)\n",
			 pa, DISP_REG_GET(va), val);
		iounmap(va);
		return;
	}
#endif

	if (strncmp(opt, "regr:", 5) == 0) {
		char *p = (char *)opt + 5;
		unsigned long addr;

		ret = kstrtoul(p, 16, &addr);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		if (is_reg_addr_valid(1, addr)) {
			unsigned int regVal = DISP_REG_GET(addr);

			DDPMSG("regr: 0x%lx = 0x%08X\n", addr, regVal);
			sprintf(buf, "regr: 0x%lx = 0x%08X\n", addr, regVal);
		} else {
			sprintf(buf, "regr, invalid address 0x%lx\n", addr);
			goto error;
		}
	} else if (strncmp(opt, "lfr_update", 10) == 0) {
		dsi_lfr_update(DISP_MODULE_DSI0, NULL);
	} else if (strncmp(opt, "regw:", 5) == 0) {
		unsigned long addr;
		unsigned int val;

		ret = sscanf(opt, "regw:0x%lx,0x%x\n", &addr, &val);
		if (ret != 2) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		if (is_reg_addr_valid(1, addr)) {
			unsigned int regVal;

			DISP_CPU_REG_SET(addr, val);
			regVal = DISP_REG_GET(addr);
			DDPMSG("regw: 0x%lx, 0x%08X = 0x%08X\n",
			       addr, val, regVal);
			sprintf(buf, "regw: 0x%lx, 0x%08X = 0x%08X\n",
				addr, val, regVal);
		} else {
			sprintf(buf, "regw, invalid address 0x%lx\n", addr);
			goto error;
		}
	} else if (strncmp(opt, "dbg_log:", 8) == 0) {
		char *p = (char *)opt + 8;

		ret = kstrtouint(p, 0, &dbg_log_level);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		sprintf(buf, "dbg_log: %d\n", dbg_log_level);
	} else if (strncmp(opt, "vfp:", 4) == 0) {
		char *p = (char *)opt + 4;
		unsigned int vfp = 0;

		ret = kstrtouint(p, 0, &vfp);
		if (ret) {
			pr_info("error to parse cmd %s\n", opt);
			return;
		}

		if (vfp > 0 && vfp <= 2000)
			backup_vfp_for_lp_cust(vfp);
	} else if (strncmp(opt, "irq_log:", 8) == 0) {
		char *p = (char *)opt + 8;
		unsigned int enable;

		ret = kstrtouint(p, 0, &enable);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		if (enable)
			irq_log_level = 1;
		else
			irq_log_level = 0;

		sprintf(buf, "irq_log: %d\n", irq_log_level);
	} else if (strncmp(opt, "met_on:", 7) == 0) {
		int met_on, rdma0_mode, rdma1_mode;

		ret = sscanf(opt, "met_on:%d,%d,%d\n",
			     &met_on, &rdma0_mode, &rdma1_mode);
		if (ret != 3) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		ddp_init_met_tag(met_on, rdma0_mode, rdma1_mode);
		DDPMSG("%s, met_on=%d,rdma0_mode %d, rdma1 %d\n", __func__,
		       met_on, rdma0_mode, rdma1_mode);
		sprintf(buf, "met_on:%d,rdma0_mode:%d,rdma1_mode:%d\n",
			met_on, rdma0_mode, rdma1_mode);
	} else if (strncmp(opt, "backlight:", 10) == 0) {
		char *p = (char *)opt + 10;
		unsigned int level;

		ret = kstrtouint(p, 0, &level);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		if (!level)
			goto error;

		disp_bls_set_backlight(level);
		sprintf(buf, "backlight: %d\n", level);
	} else if (strncmp(opt, "partial:", 8) == 0) {
		ret = sscanf(opt, "partial:%d,%d,%d,%d,%d\n", &dbg_force_roi,
			     &dbg_partial_x, &dbg_partial_y, &dbg_partial_w,
			     &dbg_partial_h);
		if (ret != 5) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		DDPMSG("%s, partial force=%d (%d,%d,%d,%d)\n", __func__,
		       dbg_force_roi, dbg_partial_x, dbg_partial_y,
		       dbg_partial_w, dbg_partial_h);
	} else if (strncmp(opt, "partial_s:", 10) == 0) {
		ret = sscanf(opt, "partial_s:%d\n", &dbg_partial_statis);
		if (ret != 5) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}
		DDPMSG("%s, partial_s:%d\n", __func__, dbg_partial_statis);
	} else if (strncmp(opt, "pwm0:", 5) == 0 ||
		   strncmp(opt, "pwm1:", 5) == 0) {
		char *p = (char *)opt + 5;
		unsigned int level;
		enum disp_pwm_id_t pwm_id = DISP_PWM0;

		ret = kstrtouint(p, 0, &level);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		if (!level)
			goto error;

		if (opt[3] == '1')
			pwm_id = DISP_PWM1;

		disp_pwm_set_backlight(pwm_id, level);
		sprintf(buf, "PWM 0x%x : %d\n", pwm_id, level);
	} else if (strncmp(opt, "rdma_color:", 11) == 0) {
		if (strncmp(opt + 11, "on", 2) == 0) {
			unsigned int r, g, b; /* red, green, blue */
			struct rdma_color_matrix matrix = { 0 };
			struct rdma_color_pre pre = { 0 };
			struct rdma_color_post post = { 255, 0, 0 };

			ret = sscanf(opt, "rdma_color:on,%d,%d,%d\n",
				     &r, &g, &b);
			if (ret != 3) {
				snprintf(buf, 50, "error to parse cmd %s\n",
					 opt);
				pr_info("error to parse cmd %s\n", opt);
				return;
			}

			post.ADD0 = r;
			post.ADD1 = g;
			post.ADD2 = b;
			rdma_set_color_matrix(DISP_MODULE_RDMA0, &matrix, &pre,
					      &post);
			rdma_enable_color_transform(DISP_MODULE_RDMA0);
		} else if (strncmp(opt + 11, "off", 3) == 0) {
			rdma_disable_color_transform(DISP_MODULE_RDMA0);
		}
	} else if (strncmp(opt, "aal_dbg:", 8) == 0) {
		char *p = (char *)opt + 8;

		ret = kstrtouint(p, 0, &aal_dbg_en);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		sprintf(buf, "aal_dbg_en = 0x%x\n", aal_dbg_en);
	} else if (strncmp(opt, "color_dbg:", 10) == 0) {
		char *p = (char *)opt + 10;
		unsigned int debug_level;

		ret = kstrtouint(p, 0, &debug_level);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		disp_color_dbg_log_level(debug_level);

		sprintf(buf, "color_dbg_en = 0x%x\n", debug_level);
	} else if (strncmp(opt, "corr_dbg:", 9) == 0) {
		char *p = (char *)opt + 9;

		ret = kstrtouint(p, 0, &corr_dbg_en);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		sprintf(buf, "corr_dbg_en = 0x%x\n", corr_dbg_en);
	} else if (strncmp(opt, "aal_test:", 9) == 0) {
		aal_test(opt + 9, buf);
	} else if (strncmp(opt, "pwm_test:", 9) == 0) {
		disp_pwm_test(opt + 9, buf);
	} else if (strncmp(opt, "dither_test:", 12) == 0) {
		/* dither_test(opt + 12, buf); */
	} else if (strncmp(opt, "ccorr_test:", 11) == 0) {
		/* ccorr_test(opt + 11, buf); */
	} else if (strncmp(opt, "od_test:", 8) == 0) {
		/* od_test(opt + 8, buf); */
	} else if (strncmp(opt, "dump_reg:", 9) == 0) {
		char *p = (char *)opt + 9;
		unsigned int module;

		ret = kstrtouint(p, 0, &module);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		DDPMSG("%s, module=%d\n", __func__, module);
		if (module < DISP_MODULE_NUM) {
			ddp_dump_reg(module);
			sprintf(buf, "dump_reg: %d\n", module);
		} else {
			DDPMSG("process_dbg_opt2, module=%d\n", module);
			goto error;
		}
	} else if (strncmp(opt, "dump_path:", 10) == 0) {
		char *p = (char *)opt + 10;
		unsigned int mutex_idx;

		ret = kstrtouint(p, 0, &mutex_idx);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		DDPMSG("%s, path mutex=%d\n", __func__, mutex_idx);
		dpmgr_debug_path_status(mutex_idx);
		sprintf(buf, "dump_path: %d\n", mutex_idx);
	} else if (strncmp(opt, "get_module_addr", 15) == 0) {
		unsigned int i = 0;
		char *buf_temp = buf;

		for (i = 0; i < DISP_MODULE_NUM; i++) {
			if (!is_ddp_module_has_reg_info(i))
				continue;
			DDPDUMP("i=%d,module=%s,va=0x%lx,pa=0x%lx,irq(%d)\n",
				i, ddp_get_module_name(i), ddp_get_module_va(i),
				ddp_get_module_pa(i), ddp_get_module_irq(i));
			sprintf(buf_temp,
				"i=%d,module=%s,va=0x%lx,pa=0x%lx,irq(%d)\n",
				i, ddp_get_module_name(i), ddp_get_module_va(i),
				ddp_get_module_pa(i), ddp_get_module_irq(i));
			buf_temp += strlen(buf_temp);
		}
	} else if (strncmp(opt, "debug:", 6) == 0) {
		char *p = (char *)opt + 6;
		unsigned int enable;

		ret = kstrtouint(p, 0, &enable);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		if (enable == 1) {
			DDPMSG("[DDP] debug=1, trigger AEE\n");
		} else if (enable == 2) {
			ddp_mem_test();
		} else if (enable == 3) {
			ddp_lcd_test();
		} else if (enable == 4) {
			/* DDPAEE("test 4"); */
		} else if (enable == 12) {
			if (gUltraEnable == 0)
				gUltraEnable = 1;
			else
				gUltraEnable = 0;
			sprintf(buf, "gUltraEnable: %d\n", gUltraEnable);
		}
	} else if (strncmp(opt, "mmp", 3) == 0) {
		init_ddp_mmp_events();
	} else if (strncmp(opt, "low_power_mode:", 15) == 0) {
		char *p = (char *)opt + 15;
		unsigned int mode;

		ret = kstrtouint(p, 0, &mode);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		low_power_cust_mode = mode;
	} else if (strncmp(opt, "set_dsi_cmd:", 12) == 0) {
		int cmd;
		int para_cnt, i;
		char para[15] = {0};
		char fmt[256] = "set_dsi_cmd:0x%x";
		struct cmdqRecStruct *cmdq;

		for (i = 0; i < ARRAY_SIZE(para); i++)
			strncat(fmt, ",0x%hhx", sizeof(fmt) - strlen(fmt) - 1);

		strncat(fmt, "\n", sizeof(fmt) - strlen(fmt) - 1);

		ret = sscanf(opt, fmt, &cmd, &para[0], &para[1], &para[2],
			     &para[3], &para[4], &para[5], &para[6], &para[7],
			     &para[8], &para[9], &para[10], &para[11],
			     &para[12], &para[13], &para[14]);

		if (ret < 1 || ret > ARRAY_SIZE(para) + 1) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		para_cnt = ret - 1;

		cmdqRecCreate(CMDQ_SCENARIO_DISP_ESD_CHECK, &cmdq);
		cmdqRecReset(cmdq);
		DSI_set_cmdq_V2(DISP_MODULE_DSI0, cmdq, cmd, para_cnt, para, 1);

		cmdqRecFlush(cmdq);
		cmdqRecDestroy(cmdq);

		DISPMSG("set_dsi_cmd cmd=0x%x\n", cmd);
		for (i = 0; i < para_cnt; i++)
			DISPMSG("para[%d] = 0x%x\n", i, para[i]);
	} else if (strncmp(opt, "dsi_read:", 9) == 0) {
		int cmd;
		int size, i, tmp = 0;
		char para[15] = {0};

		ret = sscanf(opt, "dsi_read:0x%x,%d\n",	&cmd, &size);

		if (ret != 2 || size > ARRAY_SIZE(para)) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		DSI_dcs_read_lcm_reg_v2(DISP_MODULE_DSI0, NULL, cmd,
					para, size);

		tmp += snprintf(buf, buf_size_left, "dsi_read cmd=0x%x:", cmd);

		for (i = 0; i < size; i++)
			tmp += snprintf(buf + tmp, buf_size_left - tmp,
					"para[%d]=0x%x,", i, para[i]);
		DISPMSG("%s\n", buf);
	} else if (strncmp(opt, "set_customer_cmd:", 17) == 0) {
		int cmd;
		int hs;
		int para_cnt;
		int i;
		struct LCM_setting_table_V3 test;
		char para[15] = {0};
		char fmt[256] = "set_customer_cmd:0x%x,%d";

		for (i = 0; i < ARRAY_SIZE(para); i++)
			strncat(fmt, ",0x%hhx", sizeof(fmt) - strlen(fmt) - 1);

		strncat(fmt, "\n", sizeof(fmt) - strlen(fmt) - 1);

		ret = sscanf(opt, fmt, &cmd, &hs, &para[0], &para[1],
				&para[2], &para[3], &para[4], &para[5],
				&para[6], &para[7], &para[8], &para[9],
				&para[10], &para[11], &para[12], &para[13],
				&para[14]);


		if (ret < 1 || ret > ARRAY_SIZE(para) + 1) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		para_cnt = ret - 2;
		test.id = REGFLAG_ESCAPE_ID;
		test.cmd = cmd;
		test.count = para_cnt;
		for (i = 0 ; i < 15 ; i++)
			test.para_list[i] = para[i];

		set_lcm(&test, 1, hs);
		DISPMSG("set_lcm: 0x%x\n", cmd);
		for (i = 0; i < para_cnt; i++)
			DISPMSG("para[%d] = 0x%x\n", i, para[i]);

	} else if (strncmp(opt, "read_customer_cmd:", 18) == 0) {
		int cmd;
		int size;
		int i;
		int sendhs;
		char para[15] = {0};

		ret = sscanf(opt, "read_customer_cmd:0x%x,%d,%d\n",
				&cmd, &size, &sendhs);

		if (ret != 3 || size > ARRAY_SIZE(para)) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		read_lcm(cmd, para, size, sendhs);
		DISPMSG("read_lcm: 0x%x,%d\n", cmd, sendhs);
		for (i = 0; i < size; i++)
			DISPMSG("para[%d] = 0x%x\n", i, para[i]);
	} else {
		dbg_buf[0] = '\0';
		goto error;
	}

	return;

error:
	DDP_PR_ERR("parse command error!\n%s\n\n%s", opt, STR_HELP);
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DDPDBG("cmd: %s\n", cmd);
	memset(dbg_buf, 0, sizeof(dbg_buf));
	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* ------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* ------------------------------------------------------------------------- */

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char cmd_buf[512];

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	if (strlen(dbg_buf))
		return simple_read_from_buffer(ubuf, count, ppos, dbg_buf,
					       strlen(dbg_buf));
	else
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP,
					       strlen(STR_HELP));
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buf, ubuf, count))
		return -EFAULT;

	cmd_buf[count] = 0;

	process_dbg_cmd(cmd_buf);

	return ret;
}

static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

static ssize_t lp_cust_read(struct file *file, char __user *ubuf, size_t count,
			    loff_t *ppos)
{
	char *mode0 = "low power mode(1)\n";
	char *mode1 = "just make mode(2)\n";
	char *mode2 = "performance mode(3)\n";
	char *mode4 = "unknown mode(n)\n";

	switch (low_power_cust_mode) {
	case LOW_POWER_MODE:
		return simple_read_from_buffer(ubuf, count, ppos, mode0,
					       strlen(mode0));
	case JUST_MAKE_MODE:
		return simple_read_from_buffer(ubuf, count, ppos, mode1,
					       strlen(mode1));
	case PERFORMANC_MODE:
		return simple_read_from_buffer(ubuf, count, ppos, mode2,
					       strlen(mode2));
	default:
		return simple_read_from_buffer(ubuf, count, ppos, mode4,
					       strlen(mode4));
	}
}

static const struct file_operations low_power_cust_fops = {
	.read = lp_cust_read,
};

static ssize_t debug_dump_read(struct file *file, char __user *buf,
			       size_t size, loff_t *ppos)
{
	char *str = "idlemgr disable mtcmos now, all the regs may 0x00000000\n";

	dprec_logger_dump_reset();
	dump_to_buffer = 1;
	/* dump all */
	dpmgr_debug_path_status(-1);
	dump_to_buffer = 0;
	if (is_mipi_enterulps())
		return simple_read_from_buffer(buf, size, ppos, str,
					       strlen(str));
	return simple_read_from_buffer(buf, size, ppos,
				       dprec_logger_get_dump_addr(),
				       dprec_logger_get_dump_len());
}

static const struct file_operations debug_fops_dump = {
	.read = debug_dump_read,
};

void ddp_debug_init(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *d;

	if (debug_init)
		return;

	debug_init = 1;
	debugfs = debugfs_create_file("dispsys", S_IFREG | 0444, NULL,
				      (void *)0, &debug_fops);

	debugDir = debugfs_create_dir("disp", NULL);
	if (!debugDir)
		return;

	debugfs_dump = debugfs_create_file("dump", S_IFREG | 0444, debugDir,
					   NULL, &debug_fops_dump);
	d = debugfs_create_file("lowpowermode", S_IFREG | 0444, debugDir,
				NULL, &low_power_cust_fops);
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	if (debug_procfs_init)
		return;
	debug_procfs_init = 1;

	dispsys_procfs = proc_create("dispsys",
				S_IFREG | 0444,
				NULL,
				&debug_fops);
	if (!dispsys_procfs) {
		pr_info("[%s %d]failed to create dispsys in /proc/\n",
			__func__, __LINE__);
		goto out;
	}

	disp_dir_procfs = proc_mkdir("disp", NULL);
	if (!disp_dir_procfs) {
		pr_info("[%s %d]failed to create dir disp in /proc/\n",
			__func__, __LINE__);
		goto out;
	}

	disp_dump_procfs = proc_create("dump",
				S_IFREG | 0444,
				disp_dir_procfs,
				&debug_fops_dump);
	if (!disp_dump_procfs) {
		pr_info("[%s %d]failed to create dump in /proc/disp/\n",
			__func__, __LINE__);
		goto out;
	}

	disp_lpmode_procfs = proc_create("lowpowermode",
				S_IFREG | 0444,
				disp_dir_procfs,
				&low_power_cust_fops);
	if (!disp_lpmode_procfs) {
		pr_info("[%s %d]failed to create lowpowermode in /proc/disp/\n",
			__func__, __LINE__);
		goto out;
	}

out:
	return;

#endif
}

unsigned int ddp_debug_analysis_to_buffer(void)
{
	return dump_to_buffer;
}

unsigned int ddp_debug_dbg_log_level(void)
{
	return dbg_log_level;
}

unsigned int ddp_debug_irq_log_level(void)
{
	return irq_log_level;
}

int ddp_debug_force_roi(void)
{
	return dbg_force_roi;
}

int ddp_debug_partial_statistic(void)
{
	return dbg_partial_statis;
}

int ddp_debug_force_roi_x(void)
{
	return dbg_partial_x;
}

int ddp_debug_force_roi_y(void)
{
	return dbg_partial_y;
}

int ddp_debug_force_roi_w(void)
{
	return dbg_partial_w;
}

int ddp_debug_force_roi_h(void)
{
	return dbg_partial_h;
}

void ddp_debug_exit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(debugfs);
	debugfs_remove(debugDir);//remove dir
	debug_init = 0;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	if (dispsys_procfs) {
		proc_remove(dispsys_procfs);
		dispsys_procfs = NULL;
	}
	if (disp_dir_procfs) {
		proc_remove(disp_dir_procfs);
		disp_dir_procfs = NULL;
	}
	debug_procfs_init = 0;
#endif
}

int ddp_mem_test(void)
{
	return -1;
}

int ddp_lcd_test(void)
{
	return -1;
}

enum ONESHOT_DUMP_STAGE get_oneshot_dump(void)
{
	return oneshot_dump;
}

void set_oneshot_dump(enum ONESHOT_DUMP_STAGE value)
{
	oneshot_dump = value;
}

