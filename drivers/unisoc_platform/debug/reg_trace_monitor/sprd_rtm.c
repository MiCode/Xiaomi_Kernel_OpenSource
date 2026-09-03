// SPDX-License-Identifier: GPL-2.0
/*
 *copyright (C) 2024 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>

#include "sprd_rtm.h"

static bool sprd_rtm_inited;
#define SPRD_RTM_NAME	"sprd_rtm"

static u32 rtm_rd_reg(void __iomem *addr)
{
	return readl_relaxed(addr);
}

static void rtm_wr_reg(u32 value, void __iomem *addr)
{
	writel_relaxed(value, addr);
}

static void sprd_rtm_ctrl_en(struct sprd_rtm_drvdata *rtm_drv, bool enable)
{
	u32 val;

	val = rtm_rd_reg(rtm_drv->base + REG_TRACE_CTRL);
	if (enable)
		rtm_wr_reg(val | RTM_EN, rtm_drv->base + REG_TRACE_CTRL);
	else
		rtm_wr_reg(val & (~RTM_EN), rtm_drv->base + REG_TRACE_CTRL);
}

static void sprd_rtm_frm_ts_end_en(struct sprd_rtm_drvdata *rtm_drv, bool enable)
{
	u32 val;

	val = rtm_rd_reg(rtm_drv->base + REG_TRACE_CTRL);
	if (enable)
		rtm_wr_reg(val | FRM_TS_END_EN, rtm_drv->base + REG_TRACE_CTRL);
	else
		rtm_wr_reg(val & (~FRM_TS_END_EN), rtm_drv->base + REG_TRACE_CTRL);
}

static void sprd_rtm_uesr_ext_en(struct sprd_rtm_drvdata *rtm_drv, bool enable)
{
	u32 val;

	val = rtm_rd_reg(rtm_drv->base + REG_TRACE_CTRL);
	if (enable)
		rtm_wr_reg(val | BUS_USER_FLD_EN, rtm_drv->base + REG_TRACE_CTRL);
	else
		rtm_wr_reg(val & (~BUS_USER_FLD_EN), rtm_drv->base + REG_TRACE_CTRL);
}

static void sprd_rtm_addr_ext_en(struct sprd_rtm_drvdata *rtm_drv, bool enable)
{
	u32 val;

	val = rtm_rd_reg(rtm_drv->base + REG_TRACE_CTRL);
	if (enable)
		rtm_wr_reg(val | BUS_ADDR_FLD_EN, rtm_drv->base + REG_TRACE_CTRL);
	else
		rtm_wr_reg(val & (~BUS_ADDR_FLD_EN), rtm_drv->base + REG_TRACE_CTRL);
}

static void sprd_rtm_pkg_frm(struct sprd_rtm_drvdata *rtm_drv, u8 frame)
{
	u32 val;

	val = rtm_rd_reg(rtm_drv->base + FRAME_END_CTRL);
	val &= ~PKG_NUMS_PER_FRM_MASK;
	rtm_wr_reg(val | frame, rtm_drv->base + FRAME_END_CTRL);
}

static void sprd_rtm_ts_threshold(struct sprd_rtm_drvdata *rtm_drv, u32 threshold)
{
	u32 val;

	val = rtm_rd_reg(rtm_drv->base + FRAME_END_CTRL);
	val &= ~TS_THRESHOLD_PER_FRM_MASK;
	rtm_wr_reg(threshold << 16 | val, rtm_drv->base + FRAME_END_CTRL);
}

static void sprd_rtm_ts_fld_sel(struct sprd_rtm_drvdata *rtm_drv, u8 sel)
{
	rtm_wr_reg(sel, rtm_drv->base + TS_FLD_SEL);
}

static void sprd_rtm_atb_id_set(struct sprd_rtm_drvdata *rtm_drv, u8 atb_id)
{
	rtm_wr_reg(atb_id, rtm_drv->base + ATB_ID);
}

static void sprd_rtm_axi_wait_threshold_set(struct sprd_rtm_drvdata *rtm_drv, u16 threshold)
{
	rtm_wr_reg(threshold, rtm_drv->base + AXI_WAIT_THRESHOLD);
}

static void sprd_rtm_get_version(struct sprd_rtm_drvdata *rtm_drv, u16 *ver)
{
	*ver = rtm_rd_reg(rtm_drv->base + IP_VERSION);
}

static struct rtm_ops ops = {
	.trace_en = sprd_rtm_ctrl_en,
	.frm_ts_en = sprd_rtm_frm_ts_end_en,
	.user_en = sprd_rtm_uesr_ext_en,
	.addr_en = sprd_rtm_addr_ext_en,
	.pkg_frm = sprd_rtm_pkg_frm,
	.ts_threshold = sprd_rtm_ts_threshold,
	.ts_fld_sel = sprd_rtm_ts_fld_sel,
	.atb_id = sprd_rtm_atb_id_set,
	.axi_wait_threshold = sprd_rtm_axi_wait_threshold_set,
	.version = sprd_rtm_get_version,
};

static inline void cs_lock(void __iomem *addr)
{
	/* Wait for things to settle */
	mb();
	writel_relaxed(0x0, addr + LOCK_OFFSET);
}

static inline void cs_unlock(void __iomem *addr)
{
	writel_relaxed(LOCK_ACCESS, addr + LOCK_OFFSET);
	/* Make sure everyone has seen this */
	mb();
}

static void sprd_rtm_funnel_en(void __iomem *addr, u8 port, bool enable)
{
	u32 functl;

	cs_unlock(addr);

	functl = readl_relaxed(addr + RTM_FUNNEL_FUNCTL);
	functl &= ~RTM_FUNNEL_HOLDTIME_MASK;
	functl |= RTM_FUNNEL_HOLDTIME;

	if (enable) {
		functl |= (1 << port);
		writel_relaxed(functl, addr + RTM_FUNNEL_FUNCTL);
	} else {
		functl &= ~(1 << port);
		writel_relaxed(functl, addr + RTM_FUNNEL_FUNCTL);
	}

	cs_lock(addr);
}

static void sprd_rtm_replication_en(void __iomem *addr, bool enable)
{
	cs_unlock(addr);

	if (enable)
		writel_relaxed(0x0, addr + RTM_REPLICATOR_IDFILTER1);
	else
		writel_relaxed(0xff, addr + RTM_REPLICATOR_IDFILTER1);

	writel_relaxed(0xff, addr + RTM_REPLICATOR_IDFILTER0);
	cs_lock(addr);
}

static void sprd_rtm_tmc_etr_dba_set(void __iomem *addr, u64 val)
{
	u32 dba_l, dba_h;

	cs_unlock(addr);

	dba_l = val & 0xFFFFFFFF;
	dba_h = (val >> 32) & 0xFFFFFFFF;
	writel_relaxed(dba_l, addr + RTM_TMC_DBALO);
	writel_relaxed(dba_h, addr + RTM_TMC_DBAHI);
	cs_lock(addr);
}

static void sprd_rtm_tmc_config(void __iomem *addr, enum rtm_tmc_mode mode)
{
	cs_unlock(addr);

	switch (mode) {
	case TMC_SOFTWARE_FIFO:
		writel_relaxed(TMC_SOFTWARE_FIFO, addr + RTM_TMC_MODE);
		break;
	case TMC_HARDWARE_FIFO:
		writel_relaxed(TMC_HARDWARE_FIFO, addr + RTM_TMC_MODE);
		break;
	case TMC_CIRCULAR_BUFFER:
	default:
		writel_relaxed(TMC_CIRCULAR_BUFFER, addr + RTM_TMC_MODE);
		break;
	}

	cs_lock(addr);

}

static void sprd_rtm_tmc_en(void __iomem *addr, bool enable)
{
	cs_unlock(addr);
	if (enable)
		writel_relaxed(0x1, addr + RTM_TMC_CTL);
	else
		writel_relaxed(0x0, addr + RTM_TMC_CTL);

	cs_lock(addr);
}

static void sprd_rtm_trace_path_config(struct rtm_trace_cfg *trace_cfg, bool enable)
{
	u8 port_id = trace_cfg->funnel_port;
	u64 etr_db_addr = trace_cfg->hwaddr;
	u8 sink = trace_cfg->sink;

	switch (sink) {
	case RTM_TO_TMC_ETR:
		if (enable) {
			sprd_rtm_tmc_config(trace_cfg->etr_base, TMC_CIRCULAR_BUFFER);
			sprd_rtm_tmc_etr_dba_set(trace_cfg->etr_base, etr_db_addr);
			sprd_rtm_tmc_config(trace_cfg->etf_base, TMC_HARDWARE_FIFO);
		}

		sprd_rtm_replication_en(trace_cfg->replicate_base, enable);
		sprd_rtm_tmc_en(trace_cfg->etf_base, enable);
		sprd_rtm_funnel_en(trace_cfg->funnel_base, port_id, enable);
		sprd_rtm_tmc_en(trace_cfg->etr_base, enable);
		break;
	case RTM_TO_TMC_ETF:
	default:
		if (enable)
			sprd_rtm_tmc_config(trace_cfg->etf_base, TMC_CIRCULAR_BUFFER);

		sprd_rtm_funnel_en(trace_cfg->funnel_base, port_id, enable);
		sprd_rtm_tmc_en(trace_cfg->etf_base, enable);

		break;
	}
}

static void sprd_rtm_controler_config(struct sprd_rtm_drvdata *rtm_drv)
{
	struct rtm_glb_cfg cfg = rtm_drv->rtm_dev->glb_cfg;

	rtm_drv->rtm_dev->ops->frm_ts_en(rtm_drv, cfg.frm_ts_end_en);
	rtm_drv->rtm_dev->ops->user_en(rtm_drv, cfg.uesr_ext_en);
	rtm_drv->rtm_dev->ops->addr_en(rtm_drv, cfg.addr_ext_en);
	rtm_drv->rtm_dev->ops->pkg_frm(rtm_drv, cfg.pkg_per_frame);
	rtm_drv->rtm_dev->ops->ts_threshold(rtm_drv, cfg.ts_threshold);
	rtm_drv->rtm_dev->ops->ts_fld_sel(rtm_drv, cfg.ts_fld_sel);
	rtm_drv->rtm_dev->ops->atb_id(rtm_drv, cfg.atb_id);
	rtm_drv->rtm_dev->ops->axi_wait_threshold(rtm_drv, cfg.axi_wait_threshold);
}

static void sprd_rtm_port_config(void __iomem *base, struct rtm_port_cfg *cfg)
{
	u32 match_num = cfg->match_num;
	u32 id = cfg->port_id;

	switch (cfg->bus_type) {
	case APB_PORT_TYPE:
		rtm_wr_reg(cfg->direction, base + APB_PORT_CTLR(id));
		rtm_wr_reg(match_num, base + APB_PORT_FILTER(id));
		break;

	case AHB_PORT_TYPE:
		rtm_wr_reg(cfg->direction, base + AHB_PORT_CTLR(id));
		rtm_wr_reg(match_num, base + AHB_PORT_FILTER(id));
		break;

	case AXI_PORT_TYPE:
		rtm_wr_reg(cfg->direction | (cfg->filter_type << 0x2), base + AXI_PORT_CTLR(id));
		rtm_wr_reg(match_num, base + AXI_PORT_FILTER(id));
		break;
	default:
		pr_err("RTM No such bus type!");
		break;
	}
}

static void sprd_rtm_capture_enable(struct sprd_rtm_drvdata *rtm_drv)
{
	int i, err = 0;
	struct rtm_port_cfg *tmp_cfg = rtm_drv->rtm_dev->port_cfg;

	if (!sprd_rtm_inited) {
		clk_set_parent(rtm_drv->cs_clk, rtm_drv->cs_src_sel);
		err = clk_prepare_enable(rtm_drv->cs_clk);
		if (err)
			dev_err(rtm_drv->dev, " cs_src_sel open fail\n");

		regmap_update_bits(rtm_drv->atb_clk.aon_apb_base, rtm_drv->atb_clk.offset,
				rtm_drv->atb_clk.mask, rtm_drv->atb_clk.mask);
		regmap_update_bits(rtm_drv->atb_clk.aon_apb_base, rtm_drv->atb_clk.offset,
				rtm_drv->atb_clk.apb_clk_mask, rtm_drv->atb_clk.apb_clk_mask);
		sprd_rtm_controler_config(rtm_drv);
		for (i = 0; i < rtm_drv->rtm_dev->port_num; i++)
			sprd_rtm_port_config(rtm_drv->base, &tmp_cfg[i]);

		sprd_rtm_trace_path_config(&rtm_drv->rtm_dev->trace_cfg, true);
		rtm_drv->rtm_dev->ops->trace_en(rtm_drv, true);
		sprd_rtm_inited = true;
	}
}

static void sprd_rtm_capture_disable(struct sprd_rtm_drvdata *rtm_drv)
{
	if (sprd_rtm_inited) {
		rtm_drv->rtm_dev->ops->trace_en(rtm_drv, false);
		sprd_rtm_trace_path_config(&rtm_drv->rtm_dev->trace_cfg, false);
		regmap_update_bits(rtm_drv->atb_clk.aon_apb_base, rtm_drv->atb_clk.offset,
				rtm_drv->atb_clk.mask, ~rtm_drv->atb_clk.mask);
		regmap_update_bits(rtm_drv->atb_clk.aon_apb_base, rtm_drv->atb_clk.offset,
				rtm_drv->atb_clk.apb_clk_mask, ~rtm_drv->atb_clk.apb_clk_mask);
		clk_disable_unprepare(rtm_drv->cs_clk);
		sprd_rtm_inited = false;
	}
}

static void sprd_rtm_default_ctrl_init(struct rtm_glb_cfg *cfg)
{
	cfg->addr_ext_en = false;
	cfg->uesr_ext_en = true;
	cfg->frm_ts_end_en = true;
	cfg->pkg_per_frame = 0x10;
	cfg->ts_threshold = 0xFFFF;
	cfg->ts_fld_sel = 0x0;
	cfg->atb_id = 0x0;
	cfg->axi_wait_threshold = 0x100;
	cfg->version = 0x100;
}

static int sprd_rtm_default_port_init(struct sprd_rtm_drvdata *rtm_drv, u32 n)
{
	int list_num = RTM_CONFIG_ELEMENT_SIZE;
	struct rtm_port_cfg *pt_cfg = rtm_drv->rtm_dev->port_cfg;
	u32 *val_p = (u32 *)(&pt_cfg[n]);
	struct property *prop;
	const __be32 *val;

	prop = of_find_property(rtm_drv->dev->of_node,
					"sprd,rtm-port-config", NULL);
	if (!prop)
		dev_err(rtm_drv->dev, "get port config fail\n");

	val = (const __be32 *)prop->value + n * RTM_CONFIG_ELEMENT_SIZE;
	while (list_num--)
		*val_p++ = be32_to_cpup(val++);

	return 0;
}

static int sprd_rtm_init_arch(struct sprd_rtm_drvdata *rtm_drv)
{
	int ret;
	u32 size, temp;
	u64	etr_addr = 0x0;
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;
	struct device_node *np = rtm_drv->dev->of_node;

	ret = of_property_read_u32(np, "sprd,rtm-valid-num", &temp);
	if (ret) {
		dev_err(rtm_drv->dev, "rtm get valid-num property fail\n");
		return ret;
	}
	rtm_dev->port_num = (u8)temp;
	size = sizeof(*(rtm_dev->port_name)) * rtm_dev->port_num;
	rtm_dev->port_name = devm_kzalloc(rtm_drv->dev, size, GFP_KERNEL);
	if (!rtm_dev->port_name) {
		dev_err(rtm_drv->dev, "alloc rtm_dev->port_name fail\n");
		return -ENOMEM;
	}

	ret = of_property_read_string_array(np, "sprd,rtm-port-name",
						rtm_dev->port_name, rtm_dev->port_num);
	if (ret != rtm_dev->port_num) {
		dev_err(rtm_drv->dev, "get port-names from dt failed,ret = %d\n", ret);
		return ret;
	}

	size = sizeof(struct rtm_port_cfg) * rtm_dev->port_num;
	rtm_dev->port_cfg = devm_kzalloc(rtm_drv->dev, size, GFP_KERNEL);
	if (!rtm_dev->port_cfg) {
		dev_err(rtm_drv->dev, "alloc rtm_dev->port_cfg fail\n");
		return -ENOMEM;
	}

	sprd_rtm_default_ctrl_init(&rtm_dev->glb_cfg);
	for (size = 0; size < rtm_dev->port_num; size++) {
		ret = sprd_rtm_default_port_init(rtm_drv, size);
		if (ret)
			return ret;
	}

	ret = of_property_read_u32(np, "sprd,rtm_trace_funnel_port", &temp);
	if (ret) {
		dev_err(rtm_drv->dev, "rtm get rtm_trace_funnel_port property fail\n");
		return ret;
	}
	rtm_dev->trace_cfg.funnel_port = (u8)temp;

	ret = of_property_read_u32(np, "sprd,rtm-trace-etr_eb", &temp);
	if (ret) {
		dev_err(rtm_drv->dev, "rtm get rtm-trace-etr_eb property fail\n");
	} else {
		if (temp) {
			rtm_dev->trace_cfg.sink = RTM_TO_TMC_ETR;
			if (!of_property_read_u64(np, "sprd,trace-etr-addr", &etr_addr)) {
				rtm_dev->trace_cfg.hwaddr = etr_addr;
			} else {
				dev_err(rtm_drv->dev, "rtm get trace-etr-addr property fail\n");
				dev_err(rtm_drv->dev, "trace-etr-addr no set\n");
				return -1;
			}
		} else {
			rtm_dev->trace_cfg.sink = RTM_TO_TMC_ETF;
			rtm_dev->trace_cfg.hwaddr = 0x0;
		}
	}

	ret = of_property_read_u32(np, "sprd,rtm_trace_enable", &rtm_dev->enable);
	if (ret) {
		dev_err(rtm_drv->dev, "rtm get rtm_trace_enable property fail\n");
		return ret;
	}
	rtm_dev->ops = devm_kzalloc(rtm_drv->dev, sizeof(struct rtm_ops), GFP_KERNEL);
	if (!rtm_dev->ops)
		return -ENOMEM;

	rtm_dev->ops = &ops;
	regmap_update_bits(rtm_drv->atb_clk.aon_apb_base, rtm_drv->atb_clk.offset,
					rtm_drv->atb_clk.mask, ~rtm_drv->atb_clk.mask);
	regmap_update_bits(rtm_drv->atb_clk.aon_apb_base, rtm_drv->atb_clk.offset,
		rtm_drv->atb_clk.apb_clk_mask, ~rtm_drv->atb_clk.apb_clk_mask);

	return 0;
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	return sprintf(buf, "rtm enable status is %d\n", rtm_dev->enable);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
							const char *buf, size_t count)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	rtm_drv->rtm_dev->enable = val;
	if (val) {
		sprd_rtm_capture_enable(rtm_drv);
		rtm_drv->rtm_dev->enable = RTM_TRACE_START;
	} else {
		sprd_rtm_capture_disable(rtm_drv);
		rtm_drv->rtm_dev->enable = RTM_TRACE_STOP;
	}

	return count;
}
static DEVICE_ATTR_RW(enable);

static ssize_t frm_ts_end_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool symbol;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	symbol = rtm_dev->glb_cfg.frm_ts_end_en;

	return sprintf(buf, "rtm ts_end status is %d\n", symbol);
}

static ssize_t frm_ts_end_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	bool symbol;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtobool(buf, &symbol))
		return -EINVAL;

	rtm_dev->glb_cfg.frm_ts_end_en = symbol;

	return count;
}
static DEVICE_ATTR_RW(frm_ts_end);

static ssize_t user_ext_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool symbol;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	symbol = rtm_dev->glb_cfg.uesr_ext_en;

	return sprintf(buf, "rtm user_ext status is %d\n", symbol);
}

static ssize_t user_ext_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	bool symbol;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtobool(buf, &symbol))
		return -EINVAL;

	rtm_dev->glb_cfg.uesr_ext_en = symbol;

	return count;
}
static DEVICE_ATTR_RW(user_ext);

static ssize_t addr_ext_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool symbol;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	symbol = rtm_dev->glb_cfg.addr_ext_en;

	return sprintf(buf, "rtm addr_ext status is %d\n", symbol);
}

static ssize_t addr_ext_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	bool symbol;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtobool(buf, &symbol))
		return -EINVAL;

	rtm_dev->glb_cfg.addr_ext_en = symbol;

	return count;
}
static DEVICE_ATTR_RW(addr_ext);

static ssize_t pkg_frm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	val = rtm_dev->glb_cfg.pkg_per_frame;

	return sprintf(buf, "rtm pkg_frm is 0x%lx\n", val);
}

static ssize_t pkg_frm_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val < 3) {
		dev_err(rtm_drv->dev, "pkg_per_frame should not be less than 3\n");
		return count;
	}

	rtm_dev->glb_cfg.pkg_per_frame = val;

	return count;
}
static DEVICE_ATTR_RW(pkg_frm);

static ssize_t ts_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	val = rtm_dev->glb_cfg.ts_threshold;

	return sprintf(buf, "rtm ts_threshold is 0x%lx\n", val);
}

static ssize_t ts_threshold_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	rtm_dev->glb_cfg.ts_threshold = val;

	return count;
}
static DEVICE_ATTR_RW(ts_threshold);

static ssize_t ts_fld_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	val = rtm_dev->glb_cfg.ts_fld_sel;

	return sprintf(buf, "rtm ts_fld_sel is 0x%lx\n", val);
}

static ssize_t ts_fld_sel_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	rtm_dev->glb_cfg.ts_fld_sel = val;

	return count;
}
static DEVICE_ATTR_RW(ts_fld_sel);

static ssize_t atb_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);

	val = rtm_drv->rtm_dev->glb_cfg.atb_id;
	return sprintf(buf, "rtm atb_id is 0x%lx\n", val);
}

static ssize_t atb_id_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	rtm_dev->glb_cfg.atb_id = val;

	return count;
}
static DEVICE_ATTR_RW(atb_id);

static ssize_t axi_wait_ts_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	val = rtm_dev->glb_cfg.axi_wait_threshold;

	return sprintf(buf, "rtm axi_wait_ts is 0x%lx\n", val);
}

static ssize_t axi_wait_ts_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	rtm_dev->glb_cfg.axi_wait_threshold = val;

	return count;
}
static DEVICE_ATTR_RW(axi_wait_ts);

static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);

	return sprintf(buf, "rtm ip version is 0x%x\n", rtm_drv->rtm_dev->glb_cfg.version);
}
static DEVICE_ATTR_RO(version);

static ssize_t port_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i, cnt = 0;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct rtm_port_cfg *port = rtm_drv->rtm_dev->port_cfg;

	for (i = 0; i < rtm_drv->rtm_dev->port_num; i++) {
		cnt += sprintf(buf + cnt,
			"portid:%d   bustype(APB/AHB/AXI):%s   direction(WO/RO/RW/NO):%s   match num:0x%x\n",
			port[i].port_id, BUS_TYPE_PRINT(port[i].bus_type),
			BUS_DIRECTION_PRINT(port[i].direction), port[i].match_num);
	}

	return cnt;
}

static ssize_t port_info_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	u32 id, i, ret, tmp_type, match;
	char bustype_c[3], direction_c[2];
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct rtm_port_cfg *tmp_cfg = rtm_drv->rtm_dev->port_cfg;

	ret = sscanf(buf, "%d %s %s %x", &id, bustype_c, direction_c, &match);

	if (strcmp("APB", bustype_c) == 0)
		tmp_type = 0;
	else if (strcmp("AHB", bustype_c) == 0)
		tmp_type = 1;
	else if (strcmp("AXI", bustype_c) == 0)
		tmp_type = 2;
	else
		pr_err("No Such bus type\n");

	for (i = 0; i < rtm_drv->rtm_dev->port_num; i++) {
		if (tmp_cfg[i].port_id == id) {
			if (tmp_cfg[i].bus_type == tmp_type) {
				if (strcmp("WO", direction_c) == 0)
					tmp_cfg[i].direction = 0x1;
				else if (strcmp("RO", direction_c) == 0)
					tmp_cfg[i].direction = 0x2;
				else if (strcmp("RW", direction_c) == 0)
					tmp_cfg[i].direction = 0x3;
				else
					tmp_cfg[i].direction = 0x0;

				tmp_cfg[i].match_num = match;
			}
		}
	}
	return strlen(buf);
}
static DEVICE_ATTR_RW(port_info);

static ssize_t trace_sink_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	strcat(buf, "Trace sink selected: ETF / ETR\n");
	strcat(buf, "Now is: ");
	switch (rtm_dev->trace_cfg.sink) {
	case RTM_TO_TMC_ETF:
		strcat(buf, "ETF\n");
		break;
	case RTM_TO_TMC_ETR:
		strcat(buf, "ETR\n");
		break;
	default:
		strcat(buf, "NO such mode\n");
		break;
	}

	return strlen(buf);
}

static ssize_t trace_sink_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	char sink[3];
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (sscanf(buf, "%s", sink) <= 0)
		dev_err(dev->parent, "Invalid input chars\n");

	if (strcmp("ETF", sink) == 0)
		rtm_dev->trace_cfg.sink = RTM_TO_TMC_ETF;
	else if (strcmp("ETR", sink) == 0)
		rtm_dev->trace_cfg.sink = RTM_TO_TMC_ETR;
	else
		dev_err(dev->parent, "Invalid trace sink\n");

	return strlen(buf);
}
static DEVICE_ATTR_RW(trace_sink);

static ssize_t trace_addr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	strcat(buf, "Trace address only support ETR sink\n");
	strcat(buf, "Now Trace sink is: ");
	switch (rtm_dev->trace_cfg.sink) {
	case RTM_TO_TMC_ETF:
		strcat(buf, "ETF\n");
		break;
	case RTM_TO_TMC_ETR:
		strcat(buf, "ETR\n");
		return (strlen(buf) + sprintf(buf + (strlen(buf)),
				"ETR trace address is 0x%llx\n", rtm_dev->trace_cfg.hwaddr));
		break;
	default:
		strcat(buf, "NO such mode\n");
		break;
	}
	strcat(buf, "hwaddr is 0x0\n");

	return strlen(buf);
}

static ssize_t trace_addr_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long long val;
	struct sprd_rtm_drvdata *rtm_drv = dev_get_drvdata(dev->parent);
	struct sprd_rtm_device *rtm_dev = rtm_drv->rtm_dev;

	if (rtm_dev->trace_cfg.sink == RTM_TO_TMC_ETR) {
		if (kstrtou64(buf, 0, &val))
			return -EINVAL;
		rtm_dev->trace_cfg.hwaddr = val;
	} else {
		dev_err(dev->parent, "trace sink not support\n");
	}

	return count;
}
static DEVICE_ATTR_RW(trace_addr);

static struct attribute *rtm_ctl_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_frm_ts_end.attr,
	&dev_attr_user_ext.attr,
	&dev_attr_addr_ext.attr,
	&dev_attr_pkg_frm.attr,
	&dev_attr_ts_threshold.attr,
	&dev_attr_ts_fld_sel.attr,
	&dev_attr_atb_id.attr,
	&dev_attr_axi_wait_ts.attr,
	&dev_attr_version.attr,
	&dev_attr_port_info.attr,
	&dev_attr_trace_sink.attr,
	&dev_attr_trace_addr.attr,
	NULL,
};

static struct attribute_group sprd_rtm_group = {
	.name = SPRD_RTM_NAME,
	.attrs = rtm_ctl_attrs,
};

static int sprd_rtm_probe(struct platform_device *pdev)
{
	struct sprd_rtm_drvdata *drvdata;
	void __iomem *base, *funnel_base, *etf_base, *etr_base, *replication_base;
	struct regmap *regmap_base;
	int ret;
	u32 args_init[3];
	struct resource *res = NULL;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->rtm_dev = devm_kzalloc(&pdev->dev, sizeof(struct sprd_rtm_device), GFP_KERNEL);
	if (!drvdata->rtm_dev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Error: rtm get base addr failed\n");
		return PTR_ERR(base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	funnel_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(funnel_base)) {
		dev_err(&pdev->dev, "Error: rtm get funnel_base addr failed\n");
		return PTR_ERR(funnel_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	etf_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(etf_base)) {
		dev_err(&pdev->dev, "Error: rtm get etf_base addr failed\n");
		return PTR_ERR(etf_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	replication_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(replication_base)) {
		dev_err(&pdev->dev, "Error: rtm get replication_base addr failed\n");
		return PTR_ERR(replication_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	etr_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(etr_base)) {
		dev_err(&pdev->dev, "Error: rtm get etr_base addr failed\n");
		return PTR_ERR(etr_base);
	}

	drvdata->cs_clk = devm_clk_get(&pdev->dev, "clk_cs");
	if (IS_ERR(drvdata->cs_clk)) {
		dev_err(&pdev->dev, "Error: Can not get the coresight clock!\n");
		drvdata->cs_clk = NULL;
	}
	drvdata->cs_src_sel = devm_clk_get(&pdev->dev, "cs_src");
	if (IS_ERR(drvdata->cs_src_sel))
		dev_err(&pdev->dev, "Error: Can not get the coresight clock src sel, default 26M\n");

	regmap_base = syscon_regmap_lookup_by_phandle_args(pdev->dev.of_node,
						"sprd,rtm-syscon", 3, args_init);
	if (IS_ERR_OR_NULL(regmap_base)) {
		dev_err(&pdev->dev, "failed to get sprd,rtm-syscon\n");
		return IS_ERR(regmap_base);
	}

	drvdata->atb_clk.aon_apb_base = regmap_base;
	drvdata->atb_clk.offset = args_init[0];
	drvdata->atb_clk.mask = args_init[1];
	drvdata->atb_clk.apb_clk_mask = args_init[2];
	drvdata->base = base;
	drvdata->rtm_dev->trace_cfg.funnel_base = funnel_base;
	drvdata->rtm_dev->trace_cfg.etf_base = etf_base;
	drvdata->rtm_dev->trace_cfg.replicate_base = replication_base;
	drvdata->rtm_dev->trace_cfg.etr_base = etr_base;
	drvdata->dev = &pdev->dev;
	drvdata->misc.name = SPRD_RTM_NAME;
	drvdata->misc.parent = &pdev->dev;
	drvdata->misc.minor = MISC_DYNAMIC_MINOR;
	sprd_rtm_init_arch(drvdata);
	ret = misc_register(&drvdata->misc);
	if (ret) {
		dev_err(&pdev->dev, "Error: Unable to register misc dev\n");
		return ret;
	}

	ret = sysfs_create_group(&drvdata->misc.this_device->kobj, &sprd_rtm_group);
	if (ret) {
		dev_err(&pdev->dev, "Error: Unable to export rtm sysfs\n");
		misc_deregister(&drvdata->misc);
		return ret;
	}

	dev_set_drvdata(&pdev->dev, drvdata);
	if (drvdata->rtm_dev->enable) {
		drvdata->rtm_dev->enable = RTM_TRACE_START;
		drvdata->rtm_dev->glb_cfg.atb_id = 0x1;
		sprd_rtm_capture_enable(drvdata);
	}

	return 0;
}

static int sprd_rtm_remove(struct platform_device *pdev)
{
	struct sprd_rtm_drvdata *drvdata = dev_get_drvdata(&pdev->dev);

	sysfs_remove_group(&drvdata->misc.this_device->kobj,
					&sprd_rtm_group);
	misc_deregister(&drvdata->misc);

	return 0;
}

static const struct of_device_id sprd_rtm_ids[] = {
	{.compatible = "sprd,reg_trace_monitor"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_rtm_ids);

static struct platform_driver sprd_rtm_driver = {
	.driver = {
		.name = SPRD_RTM_NAME,
		.of_match_table = sprd_rtm_ids,
	},
	.probe = sprd_rtm_probe,
	.remove = sprd_rtm_remove,
};

static int __init sprd_rtm_init(void)
{
	return platform_driver_register(&sprd_rtm_driver);
}

static void __exit sprd_rtm_exit(void)
{
	platform_driver_unregister(&sprd_rtm_driver);
}
module_init(sprd_rtm_init);
module_exit(sprd_rtm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sitao Chen <sitao.chen@unisoc.com>");
MODULE_DESCRIPTION("Unisoc platform Reg Trace Monitor Device Driver");
