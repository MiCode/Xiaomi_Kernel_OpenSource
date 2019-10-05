// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6315/registers.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spmi.h>
#include <linux/pmif.h>
#if 0
/*
 * spmi test API declaration
 */
static inline void spmi_dump_ap_register(void);

/*
 * spmi debug mechanism
 */
static inline void spmi_dump_ap_register(void)
{
	unsigned int i = 0;
#if (SPMI_KERNEL) || (SPMI_CTP)
	unsigned int *reg_addr;
#else
	unsigned int reg_addr;
#endif
	unsigned int reg_value = 0;

	SPMIERR("dump reg\n");
	for (i = 0; i <= SPMI_REG_RANGE; i++) {
#if (SPMI_KERNEL) || (SPMI_CTP)
		reg_addr = (unsigned int *) (SPMI_BASE + i * 4);
		reg_value = DRV_Reg32(((unsigned int *) (SPMI_BASE + i * 4)));
		SPMIERR("addr:0x%p = 0x%x\n", reg_addr, reg_value);
#else
		reg_addr = (SPMI_BASE + i * 4);
		reg_value = DRV_Reg32(reg_addr);
		SPMIERR("addr:0x%x = 0x%x\n", reg_addr, reg_value);
#endif
	}

}

int spmi_dump_dbg_register(struct spmi_controller *ctrl)
{
#ifdef SPMI_DBG_DEBUG
	SPMILOG("[0x%x]=0x%x ", SPMI_OP_ST_STA, DRV_Reg32(SPMI_OP_ST_STA));
	SPMILOG("[0x%x]=0x%x ", SPMI_REC0, DRV_Reg32(SPMI_REC0));
	SPMILOG("[0x%x]=0x%x ", SPMI_REC1, DRV_Reg32(SPMI_REC1));
	SPMILOG("[0x%x]=0x%x ", SPMI_REC2, DRV_Reg32(SPMI_REC2));
	SPMILOG("[0x%x]=0x%x ", SPMI_REC3, DRV_Reg32(SPMI_REC3));
	SPMILOG("[0x%x]=0x%x ", SPMI_REC4, DRV_Reg32(SPMI_REC4));
	SPMILOG("[0x%x]=0x%x\r\n", SPMI_MST_DBG, DRV_Reg32(SPMI_MST_DBG));
#endif
	return 0;
}

void spmi_dump_slv_dbg_reg(struct spmi_device *dev)
{
	u8 rdata1 = 0, rdata2 = 0, rdata3 = 0, rdata4 = 0;
	unsigned int offset, i;

	for (offset = 0x34; offset < 0x50; offset += 4) {
		spmi_ext_register_readl(dev,
			(MT6315_PLT0_ID_ANA_ID + offset), &rdata1);
		spmi_ext_register_readl(dev,
			(MT6315_PLT0_ID_ANA_ID + offset + 1), &rdata2);
		spmi_ext_register_readl(dev,
			(MT6315_PLT0_ID_ANA_ID + offset + 2), &rdata3);
		spmi_ext_register_readl(dev,
			(MT6315_PLT0_ID_ANA_ID + offset + 3), &rdata4);
		if ((offset + 3) == 0x37) {
			i = (rdata4 & 0xc) >> 2;
			if (i == 0)
				SPMILOG("DBG. Last cmd idx:0x3\r\n");
			else {
				SPMILOG("DBG. Last cmd idx:0x%x\r\n",
						((rdata4 & 0xc) >> 2) - 1);
			}

		}

		SPMILOG("[0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x ",
			offset, rdata1, (offset + 1), rdata2,
			(offset + 2), rdata3, (offset + 3), rdata4);


		SPMILOG("Type:0x%x, [0x%x]=0x%x\r\n",
			(rdata4 & 0x3), (rdata2 << 0x8) | rdata1, rdata3);

	}

}
#endif
int pmif_probe_rw_test(struct spmi_controller *ctrl)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	int ret = 0;
	u8 wdata = 0x0, rdata = 0;

	wdata = 0x22;
	ret = arb->write_cmd(ctrl, SPMI_CMD_EXT_WRITEL,
			0xa, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &wdata, 1);
	if (ret)
		return ret;
	dev_dbg(&ctrl->dev, "mtk-pmif %d\n", __LINE__);
	ret = arb->read_cmd(ctrl, SPMI_CMD_EXT_READL,
			0xa, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &rdata, 1);
	if (ret)
		return ret;
	dev_dbg(&ctrl->dev, "mtk-pmif [0xa]=0x%x\n", rdata);
	wdata = 0x33;
	ret = arb->write_cmd(ctrl, SPMI_CMD_EXT_WRITEL,
			0xb, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &wdata, 1);
	if (ret)
		return ret;
	dev_dbg(&ctrl->dev, "mtk-pmif %d\n", __LINE__);
	ret = arb->read_cmd(ctrl, SPMI_CMD_EXT_READL,
			0xb, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &rdata, 1);
	if (ret)
		return ret;
	dev_dbg(&ctrl->dev, "mtk-pmif [0xb]=0x%x\n", rdata);
	wdata = 0x44;
	ret = arb->write_cmd(ctrl, SPMI_CMD_EXT_WRITEL,
			0xc, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &wdata, 1);
	if (ret)
		return ret;
	dev_dbg(&ctrl->dev, "mtk-pmif %d\n", __LINE__);
	ret = arb->read_cmd(ctrl, SPMI_CMD_EXT_READL,
			0xc, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &rdata, 1);
	if (ret)
		return ret;
	dev_dbg(&ctrl->dev, "mtk-pmif [0xc]=0x%x\n", rdata);

	return 0;
}
