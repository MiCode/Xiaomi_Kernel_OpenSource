/*
 * Copyright (C) 2018 MediaTek Inc.
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

/*
 * @file    mtk_clk_buf_hw.c
 * @brief   Driver for clock buffer control of each platform
 *
 */
#include <linux/string.h>
#include <linux/types.h>

#include <mtk_spm.h>
#include <mtk_srclken_rc.h>
#include <mtk_srclken_rc_common.h>
#include <mt-plat/mtk_boot.h>
#include <mach/mtk_pmic_wrap.h>
#include <mtk_clkbuf_ctl.h>

#ifdef CONFIG_OF
static void __iomem *srclken_base;
static void __iomem *pwrap_base;
static void __iomem *scp_base;
#if RC_GPIO_DBG_ENABLE
static void __iomem *gpio_base;
#endif

#define SRCLKEN_REG(ofs)			(srclken_base + ofs)
#define SRCLKEN_STA_REG(ofs)		(srclken_base + ofs + 0x900)
#define SCP_REG(ofs)				(scp_base + ofs)
#if RC_GPIO_DBG_ENABLE
#define GPIO_REG(ofs)				(gpio_base + ofs)
#endif
/* Srclken_rc Register */
#define SRCLKEN_RC_CFG				SRCLKEN_REG(0x000)
#define RC_CENTRAL_CFG1				SRCLKEN_REG(0x004)
#define RC_CENTRAL_CFG2				SRCLKEN_REG(0x008)
#define RC_CMD_ARB_CFG				SRCLKEN_REG(0x00C)
#define RC_PMIC_RCEN_ADDR			SRCLKEN_REG(0x010)
#define RC_PMIC_RCEN_SET_CLR_ADDR	SRCLKEN_REG(0x014)
#define RC_DCXO_FPM_CFG				SRCLKEN_REG(0x018)
#define RC_CENTRAL_CFG3				SRCLKEN_REG(0x01C)
#define RC_M00_SRCLKEN_CFG			SRCLKEN_REG(0x020)
#define RC_CENTRAL_CFG4				SRCLKEN_REG(0x058)
#define RC_FSM_STA_0				SRCLKEN_STA_REG(0x000)
#define RC_CMD_STA_0				SRCLKEN_STA_REG(0x004)
#define RC_CMD_STA_1				SRCLKEN_STA_REG(0x008)
#define RC_SPI_STA_0				SRCLKEN_STA_REG(0x00C)
#define RC_PI_PO_STA_0				SRCLKEN_STA_REG(0x010)
#define RC_M00_REQ_STA_0			SRCLKEN_STA_REG(0x014)
#define RC_MISC_0				SRCLKEN_REG(0x0B4)
#define RC_SPM_CTL				SRCLKEN_REG(0x0B8)
#define SUBSYS_INTF_CFG				SRCLKEN_REG(0x0BC)
#define DBG_TRACE_0_LSB				SRCLKEN_STA_REG(0x050)
#define DBG_TRACE_0_MSB				SRCLKEN_STA_REG(0x054)
#define TIMER_LATCH_0_LSB				SRCLKEN_STA_REG(0x098)
#define TIMER_LATCH_0_MSB				SRCLKEN_STA_REG(0x09C)
#endif	/* CONFIG_OF */

/* TODO: marked this after driver is ready */
#define SRCLKEN_RC_BRINGUP			0

/* Pwrap Register */
#define SRCLKEN_RCINF_STA_0			(0x1C4)
#define SRCLKEN_RCINF_STA_1			(0x1C8)

/* PMIC Register */
#define PMIC_REG_MASK				0xFFFF
#define PMIC_REG_SHIFT				0

/* SCP Register */
#define CPU_VREQ_CTRL				SCP_REG(0x054)

#if RC_GPIO_DBG_ENABLE
/* GPIO Register */
#define GPIO_DIR0				GPIO_REG(0x0)
#define GPIO_DIR0_SET				GPIO_REG(0x4)
#define GPIO_DIR0_CLR				GPIO_REG(0x8)

#define GPIO_DOUT0				GPIO_REG(0x100)
#define GPIO_DOUT0_SET				GPIO_REG(0x104)
#define GPIO_DOUT0_CLR				GPIO_REG(0x108)

#define GPIO_EINT6_BIT				6
#endif

#define TRACE_NUM				8

static bool srclken_debug;
static bool rc_dts_init_done;
static bool rc_stage_init_done;
static enum srclken_config rc_stage = SRCLKEN_BRINGUP;

static const char *subsys_n[MAX_SYS_NUM] = {
	[SYS_SUSPEND] = "SUSPEND",
	[SYS_RF] = "RF",
	[SYS_DPIDLE] = "DPIDLE",
	[SYS_MD] = "MD",
	[SYS_GPS] = "GPS",
	[SYS_BT] = "BT",
	[SYS_WIFI] = "WIFI",
	[SYS_MCU] = "MCU",
	[SYS_COANT] = "COANT",
	[SYS_NFC] = "NFC",
	[SYS_UFS] = "UFS",
	[SYS_SCP] = "SCP",
	[SYS_RSV] = "RSV",
};

#if RC_GPIO_DBG_ENABLE
static void __srclken_gpio_pull(bool enable)
{
	srclken_write(GPIO_DIR0_SET, 0x1 << GPIO_EINT6_BIT);

	if (enable)
		srclken_write(GPIO_DOUT0_SET, 0x1 << GPIO_EINT6_BIT);
	else
		srclken_write(GPIO_DOUT0_CLR, 0x1 << GPIO_EINT6_BIT);
}
#endif

static int __srclken_switch_subsys_ctrl(enum sys_id id,
		enum rc_ctrl_m mode, enum rc_ctrl_r req)
{
	u32 bit_mask = SW_SRCLKEN_RC_MSK << SW_SRCLKEN_RC_SHFT;

	if (id >= MAX_SYS_NUM || id < 0) {
		pr_notice("req_subsys is not available\n");
		return -1;
	}

	if ((mode != HW_MODE) && (mode != SW_MODE)) {
		pr_notice("req_mode is not allowed\n");
		return -1;
	}

	if ((req != OFF_REQ) && (req != NO_REQ) &&
			(req != FPM_REQ) && (req != BBLPM_REQ)) {
		pr_notice("req_type is not allowed\n");
		return -1;
	}

	if (req == NO_REQ)
		req = (srclken_read(RC_M00_SRCLKEN_CFG + 4 * id) &
				(FPM_REQ | BBLPM_REQ));

	srclken_write(RC_M00_SRCLKEN_CFG + 4 * id,
			(srclken_read(RC_M00_SRCLKEN_CFG + 4 * id)
			& ~(bit_mask)) | req | mode);

	if ((srclken_read(RC_M00_SRCLKEN_CFG + 4 * id) & bit_mask)
			== (mode | req))
		return 0;

	pr_info("read back value err.(0x%x)",
			srclken_read(RC_M00_SRCLKEN_CFG + 4 * id));
	return -1;

}

static ssize_t __subys_ctl_store(const char *buf, enum sys_id id)
{
	char mode[10];

	if ((sscanf(buf, "%9s", mode) != 1))
		return -EPERM;

	if (!strcmp(mode, "HW"))
		__srclken_switch_subsys_ctrl(id, HW_MODE, NO_REQ);
	else if (!strcmp(mode, "SW")) {
		#if RC_GPIO_DBG_ENABLE
		__srclken_gpio_pull(false);
		#endif
		__srclken_switch_subsys_ctrl(id, SW_MODE, NO_REQ);
	} else if (!strcmp(mode, "SW_OFF")) {
		#if RC_GPIO_DBG_ENABLE
		__srclken_gpio_pull(false);
		#endif
		__srclken_switch_subsys_ctrl(id, SW_MODE, OFF_REQ);
	} else if (!strcmp(mode, "SW_FPM")) {
		__srclken_switch_subsys_ctrl(id, SW_MODE, FPM_REQ);
		#if RC_GPIO_DBG_ENABLE
		__srclken_gpio_pull(true);
		#endif
	} else if (!strcmp(mode, "SW_BBLPM")) {
		__srclken_switch_subsys_ctrl(id, SW_MODE, BBLPM_REQ);
		#if RC_GPIO_DBG_ENABLE
		__srclken_gpio_pull(true);
		#endif
	} else {
		pr_info("bad argument!! please follow correct format\n");
		pr_info("echo $mode > proc/srclken_rc/$subsys\n");
		pr_info("mode = {HW, SW_OFF, SW_FPM, SW_BBLPM}\n");

		return -EPERM;
	}

	return 0;
}

static ssize_t __subsys_ctl_show(char *buf, enum sys_id id)
{
	u32 sys_sta;
	u32 filter;
	u32 cmd_ok;
	int len = 0, shift = 0;

	/* fix reg shift */
	if (id > 0)
		shift = 1;

	sys_sta = srclken_read(RC_M00_REQ_STA_0 + ((id + shift) * 4));
	filter = (sys_sta >> REQ_FILT_SHFT) & REQ_FILT_MSK;
	cmd_ok = (sys_sta >> CMD_OK_SHFT) & CMD_OK_MSK;

	len += snprintf(buf+len, PAGE_SIZE-len,
		"[%s] -\n", subsys_n[id]);
	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(req / ack) - FPM(%d/%d), BBLPM(%d/%d)\n",
		(sys_sta >> FPM_SHFT) & FPM_MSK,
		(sys_sta >> FPM_ACK_SHFT) & FPM_ACK_MSK,
		(sys_sta >> BBLPM_SHFT) & BBLPM_MSK,
		(sys_sta >> BBLPM_ACK_SHFT) & BBLPM_ACK_MSK);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(cur / target / chg) - DCXO(%x/%x/%d)\n",
		(sys_sta  >> CUR_DCXO_SHFT) & CUR_DCXO_MSK,
		(sys_sta  >> TAR_DCXO_SHFT) & TAR_DCXO_MSK,
		((sys_sta  >> DCXO_CHG_SHFT) & DCXO_CHG_MSK) ?
		(((sys_sta  >> DCXO_EQ_SHFT) & DCXO_EQ_MSK) ?
		0 : 1) : 0);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(done / ongo / filt / cur / cmd) - REQ(%d/%d/%s/%x/%s)\n",
		(sys_sta  >> ALLOW_REQ_SHFT) & ALLOW_REQ_MSK,
		(sys_sta  >> REQ_ONGO_SHFT) & REQ_ONGO_MSK,
		(filter & 0x1) ? "bys" : (filter & 0x2) ? "dcxo" :
		(filter & 0x4) ? "nd" : "none",
		(sys_sta  >> CUR_REQ_STA_SHFT) & CUR_REQ_STA_MSK,
		(cmd_ok & 0x3) == 0x3 ? "cmd_ok" : "cmd_not_ok");

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\tcur_pmic_bit(%d)\n", (sys_sta  >> CUR_RC_SHFT) & CUR_RC_MSK);
	return len;
}

bool srclken_get_debug_cfg(void)
{
#if SRCLKEN_RC_BRINGUP
	return false;
#endif

#if SRCLKEN_DBG
	return true;
#else
	return srclken_debug;
#endif
}

static u32 __srclken_dump_sta(char *buf, u8 id)
{
	u32 len = 0;

	switch (id) {
	case XO_SOC:
		len = __subsys_ctl_show(buf, SYS_SUSPEND);
		len += __subsys_ctl_show(buf + len, SYS_DPIDLE);
		break;
	case XO_WCN:
		len = __subsys_ctl_show(buf, SYS_GPS);
		len += __subsys_ctl_show(buf + len, SYS_BT);
		len += __subsys_ctl_show(buf + len, SYS_WIFI);
		len += __subsys_ctl_show(buf + len, SYS_MCU);
		len += __subsys_ctl_show(buf + len, SYS_RF);
		break;
	case XO_NFC:
		len = __subsys_ctl_show(buf, SYS_NFC);
		break;
	case XO_CEL:
		len = __subsys_ctl_show(buf, SYS_RF);
		len += __subsys_ctl_show(buf + len, SYS_GPS);
		break;
	case XO_EXT:
		len = __subsys_ctl_show(buf, SYS_GPS);
		len += __subsys_ctl_show(buf + len, SYS_BT);
		len += __subsys_ctl_show(buf + len, SYS_WIFI);
		len += __subsys_ctl_show(buf + len, SYS_MCU);
		len += __subsys_ctl_show(buf + len, SYS_RF);
		break;
	default:
		pr_notice("Not valid xo_buf id\n");
		break;
	}

	return len;
}

void srclken_dump_sta_log(void)
{
	char buf[1024];
	u32 len = 0;
	u8 sta = 0;
	u8 id = 0;

	clk_buf_get_aux_out();
	clk_buf_dump_clkbuf_log();
	pr_notice("%s:\n", __func__);
	for (id = 0; id < XO_NUMBER; id++) {
		sta = clk_buf_get_xo_en_sta(id);
		if (sta) {
			len = __srclken_dump_sta(buf, id);
			if (len)
				pr_notice("%s\n", buf);
		}
	}
}

static int __srclken_dump_cfg(char *buf)
{
	int len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
		"rc cfg : 0x%x\n", srclken_read(SRCLKEN_RC_CFG));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"centrol 1: 0x%x\n", srclken_read(RC_CENTRAL_CFG1));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"centrol 2: 0x%x\n", srclken_read(RC_CENTRAL_CFG2));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"centrol 3: 0x%x\n", srclken_read(RC_CENTRAL_CFG3));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"centrol 4: 0x%x\n", srclken_read(RC_CENTRAL_CFG4));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"cmd cfg: 0x%x\n", srclken_read(RC_CMD_ARB_CFG));

	len += snprintf(buf+len, PAGE_SIZE-len,
		"subsys cfg: = 0x%x\n", srclken_read(SUBSYS_INTF_CFG));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"pmrc_rc: 0x%x\n",
		srclken_read(RC_PMIC_RCEN_ADDR));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"pmrc setclr: 0x%x\n",
		srclken_read(RC_PMIC_RCEN_SET_CLR_ADDR));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"fpm cfg: 0x%x\n", srclken_read(RC_DCXO_FPM_CFG));

	return len;
}

void srclken_dump_cfg_log(void)
{
	char buf[256];

	__srclken_dump_cfg(buf);

	pr_notice("%s: %s\n", __func__, buf);
}

static int __srclken_dump_last_sta(char *buf, u8 idx)
{
	int len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
		"TRACE%d LSB : 0x%x, ", idx,
		srclken_read(DBG_TRACE_0_LSB + (idx * 8)));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"TRACE%d MSB : 0x%x\n", idx,
		srclken_read(DBG_TRACE_0_MSB + (idx * 8)));

	len += snprintf(buf+len, PAGE_SIZE-len,
		"TIME%d LSB : 0x%x, ", idx,
		srclken_read(TIMER_LATCH_0_LSB + (idx * 8)));
	len += snprintf(buf+len, PAGE_SIZE-len,
		"TIME%d MSB : 0x%x\n", idx,
		srclken_read(TIMER_LATCH_0_MSB + (idx * 8)));

	return len;
}

void srclken_dump_last_sta_log(void)
{
	char buf[1024];
	u8 i;

	pr_notice("%s:\n", __func__);

	for (i = 0; i < TRACE_NUM; i++) {
		__srclken_dump_last_sta(buf, i);

		pr_notice("%s", buf);
	}
}

#ifdef CONFIG_PM

static ssize_t cfg_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = __srclken_dump_cfg(buf);

	return len;
}

static ssize_t trace_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	u8 i;
	/* Todo: show all dump */
	for (i = 0; i < TRACE_NUM; i++)
		len = __srclken_dump_last_sta(buf, i);

	return len;
}

static ssize_t suspend_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_SUSPEND);
	if (ret)
		return ret;
	return count;
}

static ssize_t suspend_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_SUSPEND);
}

static ssize_t rf_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_RF);
	if (ret)
		return ret;
	return count;
}

static ssize_t rf_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_RF);
}

static ssize_t dpidle_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_DPIDLE);
	if (ret)
		return ret;
	return count;
}

static ssize_t dpidle_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_DPIDLE);
}

static ssize_t md_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_MD);
	if (ret)
		return ret;
	return count;
}

static ssize_t md_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_MD);
}

static ssize_t gps_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_GPS);
	if (ret)
		return ret;
	return count;
}

static ssize_t gps_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_GPS);
}

static ssize_t bt_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_BT);
	if (ret)
		return ret;
	return count;
}

static ssize_t bt_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_BT);
}

static ssize_t wifi_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_WIFI);
	if (ret)
		return ret;
	return count;
}

static ssize_t wifi_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_WIFI);
}

static ssize_t mcu_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_MCU);
	if (ret)
		return ret;
	return count;
}

static ssize_t mcu_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_MCU);
}

static ssize_t coant_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_COANT);
	if (ret)
		return ret;
	return count;
}

static ssize_t coant_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_COANT);
}

static ssize_t nfc_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_NFC);
	if (ret)
		return ret;
	return count;
}

static ssize_t nfc_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_NFC);
}

static ssize_t ufs_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_UFS);
	if (ret)
		return ret;
	return count;
}

static ssize_t ufs_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_UFS);
}

static ssize_t scp_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_SCP);
	if (ret)
		return ret;
	return count;
}

static ssize_t scp_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_SCP);
}

static ssize_t rsv_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = __subys_ctl_store(buf, SYS_RSV);
	if (ret)
		return ret;
	return count;
}

static ssize_t rsv_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return __subsys_ctl_show(buf, SYS_RSV);
}

static ssize_t all_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	int i;

	for (i = 0; i < MAX_SYS_NUM; i++)
		len += __subsys_ctl_show(buf, i);

	return len;
}

static ssize_t spi_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	u32 spi_sta;
	int len = 0;

	spi_sta = srclken_read(RC_SPI_STA_0);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"[SPI] -\n");

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(req / ack / addr / data) - (%d/%d/%x/%x)\n",
		(spi_sta  & SPI_CMD_REQ) ==  SPI_CMD_REQ,
		(spi_sta  & SPI_CMD_REQ_ACK) ==  SPI_CMD_REQ_ACK,
		(spi_sta >> SPI_CMD_ADDR_SHFT) & SPI_CMD_ADDR_MSK,
		(spi_sta >> SPI_CMD_DATA_SHFT) & SPI_CMD_DATA_MSK);

	return len;
}

static ssize_t cmd_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	u32 cmd_sta_0, cmd_sta_1;
	int len = 0;

	cmd_sta_0 = srclken_read(RC_CMD_STA_0);
	cmd_sta_1 = srclken_read(RC_CMD_STA_1);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"[CMD] -\n");

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(xo / dcxo / target) - PMIC (0x%x/0%x/0x%x)\n",
		(cmd_sta_0  >> PMIC_PMRC_EN_SHFT) & PMIC_PMRC_EN_MSK,
		(cmd_sta_0 >> PMIC_DCXO_SHFT) & PMIC_DCXO_MSK,
		(cmd_sta_0 >> TAR_CMD_ARB_SHFT) & TAR_CMD_ARB_MSK);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(xo / dcxo) - SUBSYS (0x%x/0x%x)\n",
		(cmd_sta_1  >> TAR_XO_REQ_SHFT) & TAR_XO_REQ_MSK,
		(cmd_sta_1 >> TAR_DCXO_REQ_SHFT) & TAR_DCXO_REQ_MSK);

	return len;
}

static ssize_t fsm_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	u32 fsm_sta;
	int len = 0;

	fsm_sta = srclken_read(RC_FSM_STA_0);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"[FSM] -\n");

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\tRC Request (%s)\n",
		((fsm_sta  & ANY_REQ_32K) != ANY_REQ_32K) ? "none" :
		((fsm_sta  & ANY_BYS_REQ) == ANY_BYS_REQ) ? "bys" :
		((fsm_sta  & ANY_ND_REQ) == ANY_ND_REQ) ? "nd" :
		((fsm_sta  & ANY_ND_REQ) == ANY_ND_REQ) ? "dcxo" : "err");

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(vcore/wrap_c/wrap_s/ulposc/spi) = (%x/%x/%x/%x/%x)\n",
		(fsm_sta >> VCORE_ULPOSC_STA_SHFT) & VCORE_ULPOSC_STA_MSK,
		(fsm_sta >> WRAP_CMD_STA_SHFT) & WRAP_CMD_STA_MSK,
		(fsm_sta >> WRAP_SLP_STA_SHFT) & WRAP_SLP_STA_MSK,
		(fsm_sta >> ULPOSC_STA_SHFT) & ULPOSC_STA_MSK,
		(fsm_sta >> SPI_STA_SHFT) & SPI_STA_MSK);

	return len;
}

static ssize_t popi_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	u32 popi_sta;
	int len = 0;

	popi_sta = srclken_read(RC_PI_PO_STA_0);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"[POPI] -\n");

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(o0 / vreq_msk / xo_soc) = spm input : (%d/%d/%d)\n",
		(popi_sta & SPM_O0) == SPM_O0,
		(popi_sta & SPM_VREQ_MASK_B) == SPM_VREQ_MASK_B,
		(popi_sta & AP_26M_RDY) == AP_26M_RDY);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(o0) = output spm : (%d)\n",
		(popi_sta & SPM_O0_ACK) == SPM_O0_ACK);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(vreq/pwrap/pwrap_slp) = scp input1: (%d/%d/%d)\n",
		(popi_sta & SCP_VREQ) == SCP_VREQ,
		(popi_sta & SCP_VREQ_WRAP) == SCP_VREQ_WRAP,
		(popi_sta & SCP_WRAP_SLP) == SCP_WRAP_SLP);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(ck/rst/en) = scp input2: (%d/%d/%d)\n",
		(popi_sta & SCP_ULPOSC_CK) == SCP_ULPOSC_CK,
		(popi_sta & SCP_ULPOSC_RST) == SCP_ULPOSC_RST,
		(popi_sta & SCP_ULPOSC_EN) == SCP_ULPOSC_EN);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(vreq/pwrap_slp) = output scp1: (%d/%d)\n",
		(popi_sta & SCP_VREQ_ACK) == SCP_VREQ_ACK,
		(popi_sta & SCP_WRAP_SLP_ACK) == SCP_WRAP_SLP_ACK);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(ck/rst/en) = output scp2: (%d/%d/%d)\n",
		(popi_sta & SCP_ULPOSC_CK_ACK) == SCP_ULPOSC_CK_ACK,
		(popi_sta & SCP_ULPOSC_RST_ACK) == SCP_ULPOSC_RST_ACK,
		(popi_sta & SCP_ULPOSC_EN_ACK) == SCP_ULPOSC_EN_ACK);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(o0/vreq/pwrap/pwrap_slp) = rc output: (%d/%d/%d/%d)\n",
		(popi_sta & RC_O0) == RC_O0,
		(popi_sta & RC_VREQ) == RC_VREQ,
		(popi_sta & RC_VREQ_WRAP) == RC_VREQ_WRAP,
		(popi_sta & RC_WRAP_SLP) == RC_WRAP_SLP);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(pwrap_slp) = input rc: (%d)\n",
		(popi_sta & RC_WRAP_SLP_ACK) == RC_WRAP_SLP_ACK);

	return len;
}

static ssize_t debug_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 onoff = 0;

	if (kstrtouint(buf, 10, &onoff))
		return -EPERM;

	if (onoff == 0)
		srclken_debug = false;
	else if (onoff == 1)
		srclken_debug = true;
	else
		goto ERROR_CMD;

	return count;
ERROR_CMD:
	pr_info("bad argument!! please follow correct format\n");
	return -EPERM;
}

static ssize_t debug_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf, PAGE_SIZE-len, "debug_cfg : %d\n", srclken_debug);

	return len;
}

static ssize_t scp_sw_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char mode[8];

	if (sscanf(buf, "%7s", mode) != 1)
		return -EPERM;

	if (!strcmp(mode, "HW")) {
		srclken_write(CPU_VREQ_CTRL,
			(srclken_read(CPU_VREQ_CTRL) & ~(0x7 << 27))
			| (1 << 27));
	} else if (!strcmp(mode, "SW_ON")) {
		srclken_write(CPU_VREQ_CTRL,
			(srclken_read(CPU_VREQ_CTRL) & ~(0x7 << 27))
			| (1 << 28));
	} else if (!strcmp(mode, "SW_OFF")) {
		srclken_write(CPU_VREQ_CTRL,
			srclken_read(CPU_VREQ_CTRL) & ~(0x7 << 27));
	} else
		goto ERROR_CMD;

	return count;
ERROR_CMD:
	pr_info("bad argument!! please follow correct format\n");
	return -EPERM;
}

static ssize_t scp_sw_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf, PAGE_SIZE-len, "cpu vreq ctrl : 0x%x(addr:0x%p)\n",
		srclken_read(CPU_VREQ_CTRL), CPU_VREQ_CTRL);

	return len;
}

DEFINE_ATTR_RO(cfg_ctl);
DEFINE_ATTR_RO(trace_ctl);
DEFINE_ATTR_RW(suspend_ctl);
DEFINE_ATTR_RW(rf_ctl);
DEFINE_ATTR_RW(dpidle_ctl);
DEFINE_ATTR_RW(md_ctl);
DEFINE_ATTR_RW(gps_ctl);
DEFINE_ATTR_RW(bt_ctl);
DEFINE_ATTR_RW(wifi_ctl);
DEFINE_ATTR_RW(mcu_ctl);
DEFINE_ATTR_RW(coant_ctl);
DEFINE_ATTR_RW(nfc_ctl);
DEFINE_ATTR_RW(ufs_ctl);
DEFINE_ATTR_RW(scp_ctl);
DEFINE_ATTR_RW(rsv_ctl);
DEFINE_ATTR_RO(all_ctl);
DEFINE_ATTR_RO(spi_ctl);
DEFINE_ATTR_RO(cmd_ctl);
DEFINE_ATTR_RO(fsm_ctl);
DEFINE_ATTR_RO(popi_ctl);
DEFINE_ATTR_RW(debug_ctl);
DEFINE_ATTR_RW(scp_sw_ctl);

static struct attribute *srclken_attrs[] = {
	/* for srclken rc control */
	__ATTR_OF(cfg_ctl),
	__ATTR_OF(trace_ctl),
	__ATTR_OF(suspend_ctl),
	__ATTR_OF(rf_ctl),
	__ATTR_OF(dpidle_ctl),
	__ATTR_OF(md_ctl),
	__ATTR_OF(gps_ctl),
	__ATTR_OF(bt_ctl),
	__ATTR_OF(wifi_ctl),
	__ATTR_OF(mcu_ctl),
	__ATTR_OF(coant_ctl),
	__ATTR_OF(nfc_ctl),
	__ATTR_OF(ufs_ctl),
	__ATTR_OF(scp_ctl),
	__ATTR_OF(rsv_ctl),
	__ATTR_OF(all_ctl),
	__ATTR_OF(spi_ctl),
	__ATTR_OF(cmd_ctl),
	__ATTR_OF(fsm_ctl),
	__ATTR_OF(popi_ctl),
	__ATTR_OF(debug_ctl),
	__ATTR_OF(scp_sw_ctl),

	/* must */
	NULL,
};

static struct attribute_group srclken_attr_group = {
	.name	= "srclken",
	.attrs	= srclken_attrs,
};

int srclken_fs_init(void)
{
	int r = 0;

	/* create /sys/power/srclken/xxx */
	r = sysfs_create_group(power_kobj, &srclken_attr_group);
	if (r)
		pr_notice("FAILED TO CREATE /sys/power/srclken (%d)\n", r);

	return r;
}
#else /* !CONFIG_PM */
int srclken_fs_init(void)
{
	return 0;
}
#endif /* CONFIG_PM */

#if defined(CONFIG_OF)
int srclken_dts_map(void)
{
	struct device_node *node;

	if (rc_dts_init_done)
		return 0;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,srclken");
	if (node) {
		srclken_base = of_iomap(node, 0);
		if (!srclken_base) {
			pr_notice("%s() can't find iomem for srclken\n",
				__func__);
			return -1;
		}
	} else {
		pr_notice("%s can't find compatible node for srclken\n",
			__func__);
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6885-pwrap");
	if (node) {
		pwrap_base = of_iomap(node, 0);
		if (!pwrap_base) {
			pr_notice("%s() can't find iomem for pwrap\n",
				__func__);
			return -1;
		}
	} else {
		pr_notice("%s can't find compatible node for pwrap\n",
			__func__);
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,scp");
	if (node) {
		scp_base = of_iomap(node, 2);
		if (!scp_base) {
			pr_notice("%s() can't find iomem for scp\n",
				__func__);
			return -1;
		}
	} else {
		pr_notice("%s can't find compatible node for scp\n",
			__func__);
		return -1;
	}
#if RC_GPIO_DBG_ENABLE
	node = of_find_compatible_node(NULL, NULL, "mediatek,gpio");
	if (node) {
		gpio_base = of_iomap(node, 0);
		if (!gpio_base) {
			pr_notice("%s() can't find iomem for gpio\n",
				__func__);
			return -1;
		}
	} else {
		pr_notice("%s can't find compatible node for gpio\n",
			__func__);
		return -1;
	}
#endif
	rc_dts_init_done = true;

	return 0;
}
#else /* !CONFIG_OF */
int srclken_dts_map(void)
{
	return 0;
}
#endif

void srclken_stage_init(void)
{
#if SRCLKEN_RC_BRINGUP
	pr_info("%s: skipped for bring up\n", __func__);
	return;
#else
#if 0
	u32 cfg;
	int i;
#endif
	if (rc_stage_init_done)
		return;

	if (!rc_dts_init_done)
		if (srclken_dts_map())
			goto RC_STAGE_ERR;

	if ((srclken_read(RC_CENTRAL_CFG1)
			& (1 << SRCLKEN_RC_EN_SHFT)) == 0) {
		srclken_dbg("%s: rc not support\n", __func__);
		rc_stage = SRCLKEN_NOT_SUPPORT;
		goto RC_STAGE_DONE;
	}

#if 0
	for (i = 0; i < MAX_SYS_NUM; i++) {
		cfg = srclken_read(RC_M00_SRCLKEN_CFG + i * 4);

		if (i == SYS_BT) {
			if ((cfg & (SW_MODE | BBLPM_REQ))
					!= (SW_MODE | BBLPM_REQ))
				break;
		} else {
			if ((cfg & (SW_MODE | OFF_REQ))
					!= (SW_MODE | OFF_REQ)) {
				srclken_dbg("%s: [BT-Only]M-%d check fail.\n",
						__func__, i);
				break;
			}
		}

		if (i == (MAX_SYS_NUM - 1)) {
			srclken_dbg("%s: rc bt only mode\n", __func__);
			rc_stage = SRCLKEN_BT_ONLY;
			goto RC_STAGE_DONE;
		}
	}

	for (i = 0; i < MAX_SYS_NUM; i++) {
		cfg = srclken_read(RC_M00_SRCLKEN_CFG + i * 4);

		if (i == SYS_COANT) {
			if ((cfg & (0x7 << SW_SRCLKEN_RC_SHFT)) != 0)
				break;
		} else {
			if ((cfg & (SW_MODE | OFF_REQ))
					!= (SW_MODE | OFF_REQ)) {
				srclken_dbg("%s: [COANT-Only]M-%d check fail.\n",
						__func__, i);
				break;
			}
		}

		if (i == (MAX_SYS_NUM - 1)) {
			srclken_dbg("%s: rc coant only mode\n", __func__);
			rc_stage = SRCLKEN_BT_ONLY;
			goto RC_STAGE_DONE;
		}
	}

	for (i = 0; i < MAX_SYS_NUM; i++) {
		cfg = srclken_read(RC_M00_SRCLKEN_CFG + i * 4);

		if (i == SYS_BT) {
			if ((cfg & (SW_MODE | BBLPM_REQ))
					!= (SW_MODE | BBLPM_REQ))
				break;
		} else if ((cfg & (1 << SW_SRCLKEN_RC_SHFT)) != 0) {
			srclken_dbg("%s: [Full-Set] M-%d check fail.\n",
					__func__, i);
			break;
		}

		if (i == (MAX_SYS_NUM - 1)) {
			srclken_dbg("%s: rc full set mode\n", __func__);
			rc_stage = SRCLKEN_FULL_SET;
			goto RC_STAGE_DONE;
		}
	}
#endif

RC_STAGE_DONE:
	rc_stage_init_done = true;
	rc_stage = SRCLKEN_FULL_SET;
	return;

RC_STAGE_ERR:
	pr_notice("%s: rc went wrong, need to check\n", __func__);
	rc_stage = SRCLKEN_ERR;

#endif
}

enum srclken_config srclken_get_stage(void)
{
	if (!rc_stage_init_done)
		srclken_stage_init();

	return rc_stage;
}

