// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-srclken-rc-hw.c
 * @brief   Driver for subys request resource control of each platform
 *
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "mtk-srclken-rc.h"
#include "mtk-srclken-rc-common.h"
#include "mtk-srclken-rc-hw.h"
#include <mtk_clkbuf_ctl.h>

#define ADDR_NOT_VALID	0xffff
#define SCP_REG(ofs)	(hw->base[SCPDVFS_BASE] + ofs)
#define GPIO_REG(ofs)	(hw->base[GPIO_BASE] + ofs)

/* Pwrap Register */
#define SRCLKEN_RCINF_STA_0		(0x1C4)
#define SRCLKEN_RCINF_STA_1		(0x1C8)

/* PMIC Register */
#define PMIC_REG_MASK			0xFFFF
#define PMIC_REG_SHIFT			0

/* SCP Register */
#define CPU_VREQ_CTRL			SCP_REG(0x0)
#define VREQ_RC_SEL			(hw->val[SCP_RC_SEL_BIT])
#define VREQ_RC_VAL			(hw->val[SCP_RC_VAL_BIT])

#if RC_GPIO_DBG_ENABLE
/* GPIO Register */
#define GPIO_DIR			GPIO_REG(hw->val[GPIO_DIR_CFG])
#define GPIO_DIR_SET			GPIO_REG(hw->val[GPIO_DIR_CFG] + 0x4)
#define GPIO_DIR_CLR			GPIO_REG(hw->val[GPIO_DIR_CFG] + 0x8)

#define GPIO_DOUT			GPIO_REG(hw->val[GPIO_DOUT_CFG])
#define GPIO_DOUT_SET			GPIO_REG(hw->val[GPIO_DOUT_CFG] + 0x4)
#define GPIO_DOUT_CLR			GPIO_REG(hw->val[GPIO_DOUT_CFG] + 0x8)

#define GPIO_PULL_SHFT			(hw->val[GPIO_PULL_BIT])
#endif

#define TRACE_NUM			8
#define MAX_BUF_LEN			1024

static bool srclken_debug;
static bool is_rc_bringup;
static bool get_bringup_state_done;
static bool rc_dts_init_done;
static bool rc_cfg_init_done;
static int rc_cfg = NOT_SUPPORT_CFG;
static uint32_t subsys_num;

struct subsys_cfg {
	const char	*name;
	enum sys_id	id;
	enum rc_ctrl_m	mode;
	enum rc_ctrl_r	req;
	uint32_t		sta;
};

static struct subsys_cfg *sys_cfg;

static struct rc_dts_predef srclken_dts[DTS_NUM] = {
	[SCP_VREQ_CFG] = {"scp", "vreq", "cfg", 0},
	[SCP_RC_SEL_BIT] = {"scp", "rc-vreq", "bit", 0},
	[SCP_RC_VAL_BIT] = {"scp", "rc-vreq", "bit", 1},
	[GPIO_DIR_CFG] = {"gpio", "dir", "cfg", 0},
	[GPIO_DOUT_CFG] = {"gpio", "dout", "cfg", 0},
	[GPIO_PULL_BIT] = {"gpio", "pull", "bit", 0},
};

static struct srclken_hw *hw;
static const struct srclken_ops *srclken_ops;
static bool srclken_op_done;
void set_srclken_ops(const struct srclken_ops *ops)
{
	srclken_ops = ops;
	srclken_op_done = true;
}
EXPORT_SYMBOL(set_srclken_ops);

bool srclken_get_bringup_sta(void)
{
	if (is_rc_bringup)
		pr_info("%s: skipped for bring up\n", __func__);

	return is_rc_bringup;
}

static void __srclken_set_bringup_sta(bool enable)
{
	is_rc_bringup = enable;
}

void srclken_get_bringup_node(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	if (!get_bringup_state_done) {
		const char *str;
		int ret = 0;

		ret = of_property_read_string(node,
			"mediatek,bring-up", &str);
		if (ret || (!strcmp(str, "enable"))) {
			pr_info("[%s]: bring up enable\n",
				__func__);
			__srclken_set_bringup_sta(true);
		} else {
			__srclken_set_bringup_sta(false);
		}

		get_bringup_state_done = true;
	}
}

#if RC_GPIO_DBG_ENABLE
static void __srclken_gpio_pull(bool enable)
{
	srclken_write(GPIO_DIR_SET, 0x1 << GPIO_PULL_SHFT);

	if (enable)
		srclken_write(GPIO_DOUT_SET, 0x1 << GPIO_PULL_SHFT);
	else
		srclken_write(GPIO_DOUT_CLR, 0x1 << GPIO_PULL_SHFT);

	pr_info("gpio: dir(0x%x) out(0x%x)\n", srclken_read(GPIO_DIR),
		srclken_read(GPIO_DOUT));
}
#endif

static int __set_subsys_cfg(enum sys_id id,
		enum rc_ctrl_m mode, enum rc_ctrl_r req)
{
	int ret = 0;

	if (srclken_ops == NULL || srclken_ops->set_subsys_cfg == NULL)
		return -ENODEV;

	ret = srclken_ops->set_subsys_cfg(id, mode, req);
	if (ret != 0 && ret != SRCLKEN_INVLAID_REG) {
		pr_err("read back value err(%d), val = 0x%x\n", ret);
		return -EINVAL;
	}

	return ret;
}

static int __get_subsys_cfg(enum sys_id id, uint32_t *val)
{
	int ret = 0;

	if (srclken_ops == NULL || srclken_ops->get_subsys_cfg == NULL)
		return -ENODEV;

	ret = srclken_ops->get_subsys_cfg(id, val);
	if (ret != 0 && ret != SRCLKEN_INVLAID_REG) {
		pr_err("read subsys ctrl register(%d), val = 0x%x\n", ret, *val);
		return -EINVAL;
	}

	return ret;
}

static int __get_subsys_sta(enum sys_id id, uint32_t *val)
{
	int ret = 0;

	if (srclken_ops == NULL || srclken_ops->get_subsys_sta == NULL)
		return -ENODEV;

	ret = srclken_ops->get_subsys_sta(id, val);
	if (ret != 0 && ret != SRCLKEN_INVLAID_REG) {
		pr_err("read subsys sta register(%d), val = 0x%x\n", ret, *val);
		return -EINVAL;
	}

	return ret;
}

static int __get_cfg_reg(enum cfg_id id, uint32_t *val)
{
	int ret = 0;

	if (srclken_ops == NULL || srclken_ops->get_cfg_reg == NULL)
		return -ENODEV;

	ret = srclken_ops->get_cfg_reg(id, val);
	if (ret != 0 && ret != SRCLKEN_INVLAID_REG) {
		pr_err("read subsys ctrl register(%d), val = 0x%x\n", ret, *val);
		return -EINVAL;
	}

	return ret;
}

static int __get_sta_reg(enum sta_id id, uint32_t *val)
{
	int ret = 0;

	if (srclken_ops == NULL || srclken_ops->get_sta_reg == NULL)
		return -ENODEV;

	ret = srclken_ops->get_sta_reg(id, val);
	if (ret != 0 && ret != SRCLKEN_INVLAID_REG) {
		pr_err("read subsys ctrl register(%d), val = 0x%x\n", ret, *val);
		return -EINVAL;
	}

	return ret;
}

static int __get_trace_reg(uint32_t idx, uint32_t *val1, uint32_t *val2)
{
	int ret = 0;

	if (srclken_ops == NULL || srclken_ops->get_trace_sta == NULL)
		return -ENODEV;

	ret = srclken_ops->get_trace_sta(idx, val1, val2);
	if (ret != 0 && ret != SRCLKEN_INVLAID_REG) {
		pr_err("read subsys ctrl register(%d), val1 = 0x%x, val2 = 0x%x\n",
				ret, *val1, *val2);
		return -EINVAL;
	}

	return ret;
}

static int __get_timer_reg(uint32_t idx, uint32_t *val1, uint32_t *val2)
{
	int ret = 0;

	if (srclken_ops == NULL || srclken_ops->get_timer_latch == NULL)
		return -ENODEV;

	ret = srclken_ops->get_trace_sta(idx, val1, val2);
	if (ret != 0 && ret != SRCLKEN_INVLAID_REG) {
		pr_err("read subsys ctrl register(%d), val1 = 0x%x, val2 = 0x%x\n",
				ret, *val1, *val2);
		return -EINVAL;
	}

	return ret;
}

static ssize_t __subsys_ctl_store(const char *buf, enum sys_id id)
{
	char mode[10];

	if ((sscanf(buf, "%9s", mode) != 1))
		return -EPERM;

	if (!strcmp(mode, "HW"))
		__set_subsys_cfg(id, HW_MODE, NO_REQ);
	else if (!strcmp(mode, "SW")) {
#if RC_GPIO_DBG_ENABLE
		__srclken_gpio_pull(false);
#endif
		__set_subsys_cfg(id, SW_MODE, NO_REQ);
	} else if (!strcmp(mode, "SW_OFF")) {
#if RC_GPIO_DBG_ENABLE
		__srclken_gpio_pull(false);
#endif
		__set_subsys_cfg(id, SW_MODE, OFF_REQ);
	} else if (!strcmp(mode, "SW_FPM")) {
		__set_subsys_cfg(id, SW_MODE, FPM_REQ);
#if RC_GPIO_DBG_ENABLE
		__srclken_gpio_pull(true);
#endif
	} else if (!strcmp(mode, "SW_BBLPM")) {
		__set_subsys_cfg(id, SW_MODE, BBLPM_REQ);
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

static int __subsys_ctl_show(char *buf, enum sys_id id)
{
	uint32_t sys_sta;
	uint32_t filter;
	uint32_t cmd_ok;
	int len = 0;
	int ret = 0;

	ret = __get_subsys_sta(id, &sys_sta);
	if (ret)
		return ret;

	filter = (sys_sta >> REQ_FILT_SHFT) & REQ_FILT_MSK;
	cmd_ok = (sys_sta >> CMD_OK_SHFT) & CMD_OK_MSK;

	len += snprintf(buf+len, PAGE_SIZE-len,
		"[%s] -\n", sys_cfg[id].name);
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

bool srclken_hw_get_debug_cfg(void)
{
	if (srclken_get_bringup_sta())
		return false;

#if SRCLKEN_DBG
	return true;
#else
	return srclken_debug;
#endif
}

static int __srclken_dump_sta(char *buf, u8 id)
{
	int len = 0;
	int ret = 0;

	switch (id) {
	case XO_SOC:
		ret = __subsys_ctl_show(buf, SYS_SUSPEND);
		if (ret < 0)
			return ret;
		len += ret;
		ret = __subsys_ctl_show(buf + len, SYS_DPIDLE);
		if (ret < 0)
			return ret;
		break;
	case XO_WCN:
		len = __subsys_ctl_show(buf, SYS_GPS);
		if (ret < 0)
			return ret;
		len += ret;
		ret = __subsys_ctl_show(buf + len, SYS_BT);
		if (ret < 0)
			return ret;
		len += ret;
		ret = __subsys_ctl_show(buf + len, SYS_WIFI);
		if (ret < 0)
			return ret;
		len += ret;
		ret = __subsys_ctl_show(buf + len, SYS_MCU);
		if (ret < 0)
			return ret;
		len += ret;
		ret = __subsys_ctl_show(buf + len, SYS_COANT);
		if (ret < 0)
			return ret;
		len += ret;
		break;
	case XO_NFC:
		ret = __subsys_ctl_show(buf, SYS_NFC);
		if (ret < 0)
			return ret;
		len += ret;
		break;
	case XO_CEL:
		len = __subsys_ctl_show(buf, SYS_RF);
		if (ret < 0)
			return ret;
		len += ret;
		ret = __subsys_ctl_show(buf + len, SYS_MD);
		if (ret < 0)
			return ret;
		len += ret;
		break;
	case XO_EXT:
		len = __subsys_ctl_show(buf, SYS_UFS);
		if (ret < 0)
			return ret;
		len += ret;
		break;
	default:
		pr_notice("Not valid xo_buf id\n");
		return SRCLKEN_NOT_SUPPORT;
	}

	return len;
}

int srclken_hw_dump_sta_log(void)
{
	char buf[MAX_BUF_LEN];
	int ret = 0;
	u8 id = 0;

	for (id = 0; id < XO_NUMBER; id++) {
		ret = clk_buf_get_xo_en_sta(id);
		if (ret > 0) {
			ret = __srclken_dump_sta(buf, id);
			if (ret >= 0)
				pr_notice("%s\n", buf);
			else
				return ret;
		}
	}

	return 0;
}

static int __srclken_dump_cfg(char *buf)
{
	uint32_t val = 0;
	int len = 0;
	int ret = 0;
	int i;

	for (i = 0; i < CFG_NUM; i++) {
		ret = __get_cfg_reg(i, &val);
		if (ret != SRCLKEN_INVLAID_REG && ret)
			goto ERROR;
		else if (ret == SRCLKEN_INVLAID_REG)
			continue;
		else
			len += snprintf(buf+len, PAGE_SIZE-len,
					"%s : 0x%x\n", cfg_n[i], val);
	}

	return len;
ERROR:
	pr_err("%s: read back value err(%d), val = 0x%x\n", __func__, ret, val);
	return -EINVAL;
}

int srclken_hw_dump_cfg_log(void)
{
	char buf[MAX_BUF_LEN];
	int ret = 0;

	ret = __srclken_dump_cfg(buf);
	if (ret)
		return ret;

	pr_notice("%s: %s\n", __func__, buf);

	return ret;
}

static int __srclken_dump_last_sta(char *buf, u8 idx)
{
	uint32_t val1 = 0, val2 = 0;
	int len = 0;
	int ret = 0;

	ret = __get_trace_reg(idx, &val1, &val2);
	if (ret != SRCLKEN_INVLAID_REG && ret)
		return -EINVAL;
	else if (ret == SRCLKEN_INVLAID_REG)
		goto next;
	else {
		len += snprintf(buf+len, PAGE_SIZE-len,
			"TRACE%d LSB : 0x%x, ", idx,
			val1);

		len += snprintf(buf+len, PAGE_SIZE-len,
			"TRACE%d MSB : 0x%x\n", idx,
			val2);
	}

next:
	ret = __get_timer_reg(idx, &val1, &val2);
	if (ret != SRCLKEN_INVLAID_REG && ret)
		return ret;
	else if (ret == SRCLKEN_INVLAID_REG)
		goto done;
	else {
		len += snprintf(buf+len, PAGE_SIZE-len,
			"TIMER LATCH%d LSB : 0x%x, ", idx,
			val1);

		len += snprintf(buf+len, PAGE_SIZE-len,
			"TIMER LATCH%d MSB : 0x%x\n", idx,
			val2);
	}

done:
	return len;
}

int srclken_hw_dump_last_sta_log(void)
{
	char buf[MAX_BUF_LEN];
	u32 len = 0;
	int ret = 0;
	u8 i;

	for (i = 0; i < TRACE_NUM; i++) {
		ret = __srclken_dump_last_sta(buf + len, i);
		if (ret < 0)
			return ret;
		len += ret;
	}
	pr_notice("%s", buf);

	return ret;
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
	int ret = 0;
	u8 i;

	for (i = 0; i < TRACE_NUM; i++) {
		ret = __srclken_dump_last_sta(buf + len, i);
		if (ret < 0)
			return ret;
		len += ret;
	}

	ret = srclken_hw_dump_sta_log();
	if (ret) {
		pr_err("%s: fail to dump sta logs(%d)\n", __func__, ret);
		return ret;
	}

	return len;
}

static ssize_t suspend_ctl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;

	ret = __subsys_ctl_store(buf, SYS_SUSPEND);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_RF);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_DPIDLE);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_MD);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_GPS);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_BT);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_WIFI);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_MCU);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_COANT);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_NFC);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_UFS);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_SCP);
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
	int ret;

	ret = __subsys_ctl_store(buf, SYS_RSV);
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

	for (i = 0; i < subsys_num; i++)
		len += __subsys_ctl_show(buf + len, i);

	return len;
}

static ssize_t spi_ctl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	uint32_t spi_sta;
	int len = 0;
	int ret = 0;

	ret = __get_sta_reg(SPI_STA, &spi_sta);
	if (ret) {
		pr_err("%s: read back value err(%d), val = 0x%x\n", __func__, ret, spi_sta);
		return -EINVAL;
	}

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
	int ret  = 0;

	ret = __get_sta_reg(CMD_0_STA, &cmd_sta_0);
	if (ret) {
		pr_err("%s: read back value err(%d), val = 0x%x\n", __func__, ret, cmd_sta_0);
		return -EINVAL;
	}

	ret = __get_sta_reg(CMD_1_STA, &cmd_sta_1);
	if (ret) {
		pr_err("%s: read back value err(%d), val = 0x%x\n", __func__, ret, cmd_sta_1);
		return -EINVAL;
	}

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
	uint32_t fsm_sta;
	int len = 0;
	int ret = 0;

	ret = __get_sta_reg(FSM_STA, &fsm_sta);
	if (ret) {
		pr_err("%s: read back value err(%d), val = 0x%x\n", __func__, ret, fsm_sta);
		return -EINVAL;
	}

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
	uint32_t popi_sta;
	int len = 0;
	int ret = 0;

	ret = __get_sta_reg(PIPO_STA, &popi_sta);
	if (ret) {
		pr_err("%s: read back value err(%d), val = 0x%x\n", __func__, ret, popi_sta);
		return -EINVAL;
	}

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
	u32 onoff;

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
			srclken_read(CPU_VREQ_CTRL) | (1 << VREQ_RC_SEL));
	} else if (!strcmp(mode, "SW_ON")) {
		srclken_write(CPU_VREQ_CTRL,
			(srclken_read(CPU_VREQ_CTRL) & ~(0x1 << VREQ_RC_SEL))
			| (1 << VREQ_RC_VAL));
	} else if (!strcmp(mode, "SW_OFF")) {
		srclken_write(CPU_VREQ_CTRL,
			srclken_read(CPU_VREQ_CTRL) &
			~(0x1 << VREQ_RC_SEL | 0x1 << VREQ_RC_VAL));
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
	.name = "srclken",
	.attrs = srclken_attrs,
};

int srclken_fs_init(void)
{
	int r = 0;

	/* create /sys/kernel/srclken/xxx */
	r = sysfs_create_group(kernel_kobj, &srclken_attr_group);
	if (r)
		pr_err("FAILED TO CREATE /sys/kernel/srclken (%d)\n", r);

	return r;
}
#else /* !CONFIG_PM */
int srclken_fs_init(void)
{
	return 0;
}
#endif /* CONFIG_PM */

#if defined(CONFIG_OF)
int _srclken_dts_map_internal(struct device_node *node, int idx)
{
	char *buf;
	int ret = 0;

	buf = kzalloc(sizeof(char)*25, GFP_KERNEL);
	if (!buf)
		goto no_mem;

	snprintf(buf, 25, "%s-%s-%s", srclken_dts[idx].base_n,
			srclken_dts[idx].match, srclken_dts[idx].flag);

	ret = of_property_read_u32_index(node, buf,
			srclken_dts[idx].idx, &hw->val[idx]);
	if (ret)
		goto no_property;

	pr_info("%s-[%d]0x%x\n", buf, idx, hw->val[idx]);

	kfree(buf);

	return ret;

no_mem:
	pr_err("%s can't allocate memory %d\n",
			__func__, ret);
	return -ENOMEM;
no_property:
	pr_err("%s can't find property(%s) %d\n",
			__func__, buf, ret);
	kfree(buf);

	return 0;
}

int srclken_dts_map(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;
	int cnt = 0;
	int i, j;

	if (srclken_get_bringup_sta())
		return SRCLKEN_BRINGUP;

	if (rc_dts_init_done)
		return 0;

	if (of_property_match_string(node, "srclken-mode", "full-set") >= 0)
		rc_cfg = FULL_SET_CFG;
	else if (of_property_match_string(node, "srclken-mode", "bt-only") >= 0)
		rc_cfg = BT_ONLY_CFG;
	else if (of_property_match_string(node, "srclken-mode", "coant-only") >= 0)
		rc_cfg = COANT_ONLY_CFG;
	else
		rc_cfg = NOT_SUPPORT_CFG;

	cnt = of_property_count_strings(node, "subsys");
	if (cnt > 0) {
		subsys_num = cnt;
		sys_cfg = kcalloc(cnt, sizeof(*sys_cfg), GFP_KERNEL);
		if (!sys_cfg)
			goto hw_no_mem;

		for (i = 0; i < cnt; i++) {
			sys_cfg[i].name = kzalloc(sizeof(char) * 10, GFP_KERNEL);
			if (!sys_cfg[i].name)
				goto name_no_mem;
			ret = of_property_read_string_index(node,
					"subsys", i, &sys_cfg[i].name);
			if (ret)
				goto subsys_no_mem;
		}
	} else
		goto no_property;

	hw = kzalloc(sizeof(*hw), GFP_KERNEL);
	if (!hw)
		goto hw_no_mem;

	hw->val = kzalloc(sizeof(u32) * DTS_NUM, GFP_KERNEL);
	if (!hw->val)
		goto val_no_mem;

	for (i = 0; i < MAX_BASE_NUM; i++) {
		int start[] = {SCP_START, GPIO_START};
		int end[] = {SCP_END, GPIO_END};

		hw->base[i] = of_iomap(node, i);
		if (IS_ERR(hw->base[i])) {
			ret = PTR_ERR(hw->base[i]);
			goto no_base;
		}

		pr_info("base[%d]0x%pR\n", i, hw->base[i]);

		for (j = start[i]; j < end[i]; j++) {
			ret = _srclken_dts_map_internal(node, j);
			if (ret)
				goto no_base;
		}
	}

	rc_dts_init_done = true;

	return 0;

no_base:
	kfree(hw->val);
val_no_mem:
	kfree(hw);
hw_no_mem:
subsys_no_mem:
	kfree(sys_cfg->name);
name_no_mem:
	kfree(sys_cfg);
	pr_err("%s can't allocate memory %d\n",
			__func__, ret);
	return -ENOMEM;
no_property:
	pr_err("%s can't find property %d\n",
			__func__, ret);
	return ret;
}
#else /* !CONFIG_OF */
int srclken_dts_map(struct platform_device *pdev)
{
	return 0;
}
#endif

int srclken_cfg_init(void)
{
	uint32_t cfg = 0;
	int ret = 0;
	int i;

	if (srclken_get_bringup_sta())
		return SRCLKEN_BRINGUP;

	if (rc_cfg_init_done || rc_cfg == NOT_SUPPORT_CFG)
		goto RC_CFG_DONE;

	for (i = 0; i < DTS_NUM; i++)
		pr_info("[%d]0x%x\n", i, hw->val[i]);

	ret = __get_cfg_reg(CENTRAL_1_CFG, &cfg);
	if (ret < 0)
		goto RC_GET_CFG_FAIL;

	if ((cfg & (1 << SRCLKEN_RC_EN_SHFT)) == 0) {
		rc_cfg = NOT_SUPPORT_CFG;
		goto RC_CFG_DONE;
	}

	for (i = 0; i < subsys_num; i++) {
		ret = __get_subsys_cfg(i, &cfg);

		if (ret < 0)
			goto RC_GET_CFG_FAIL;

		/* set id for srclken subsys */
		if (!strcmp(sys_cfg[i].name, "SUSPEND"))
			sys_cfg[i].id = SYS_SUSPEND;
		else if (!strcmp(sys_cfg[i].name, "RF"))
			sys_cfg[i].id = SYS_RF;
		else if (!strcmp(sys_cfg[i].name, "DPIDLE"))
			sys_cfg[i].id = SYS_DPIDLE;
		else if (!strcmp(sys_cfg[i].name, "MD"))
			sys_cfg[i].id = SYS_MD;
		else if (!strcmp(sys_cfg[i].name, "GPS"))
			sys_cfg[i].id = SYS_GPS;
		else if (!strcmp(sys_cfg[i].name, "BT"))
			sys_cfg[i].id = SYS_BT;
		else if (!strcmp(sys_cfg[i].name, "WIFI"))
			sys_cfg[i].id = SYS_WIFI;
		else if (!strcmp(sys_cfg[i].name, "MCU"))
			sys_cfg[i].id = SYS_MCU;
		else if (!strcmp(sys_cfg[i].name, "COANT"))
			sys_cfg[i].id = SYS_COANT;
		else if (!strcmp(sys_cfg[i].name, "NFC"))
			sys_cfg[i].id = SYS_NFC;
		else if (!strcmp(sys_cfg[i].name, "UFS"))
			sys_cfg[i].id = SYS_UFS;
		else if (!strcmp(sys_cfg[i].name, "SCP"))
			sys_cfg[i].id = SYS_SCP;
		else if (!strcmp(sys_cfg[i].name, "RSV"))
			sys_cfg[i].id = SYS_RSV;
		else {
			rc_cfg = NOT_SUPPORT_CFG;
			goto RC_CFG_FAIL;
		}
		/* set mode for srclken subsys */
		if ((cfg & SW_MODE) == SW_MODE)
			sys_cfg[i].mode = SW_MODE;
		else
			sys_cfg[i].mode = HW_MODE;

		/* set req for srclken subsys */
		if ((cfg & BBLPM_REQ) == BBLPM_REQ)
			sys_cfg[i].req = BBLPM_REQ;
		else if ((cfg & FPM_REQ) == FPM_REQ)
			sys_cfg[i].req = FPM_REQ;
		else
			sys_cfg[i].req = NO_REQ;
	}

RC_CFG_DONE:
	pr_notice("%s: rc %s verify done(%d)\n", __func__,
			rc_cfg == BT_ONLY_CFG ? "bt only" :
			rc_cfg == COANT_ONLY_CFG ? "coant only" :
			rc_cfg == FULL_SET_CFG ? "full set" :
			rc_cfg == NOT_SUPPORT_CFG ? "not support" : "init",
			ret);
	rc_cfg_init_done = true;

	return ret;

RC_GET_CFG_FAIL:
RC_CFG_FAIL:
	pr_err("%s: %s went wrong, need to check\n", __func__,
			rc_cfg == BT_ONLY_CFG ? "bt only" :
			rc_cfg == COANT_ONLY_CFG ? "coant only" :
			rc_cfg == FULL_SET_CFG ? "full set" : "not support");
	pr_err("%s: %s cfg fail. (0x%x)\n", __func__, sys_cfg[i].name, cfg);

	return ret;
}

int srclken_hw_get_cfg(void)
{
	if (!rc_cfg_init_done)
		srclken_cfg_init();

	return rc_cfg;
}

int srclken_hw_is_ready(void)
{
	if (!srclken_op_done)
		return SRCLKEN_NOT_READY;

	return SRCLKEN_OK;
}

