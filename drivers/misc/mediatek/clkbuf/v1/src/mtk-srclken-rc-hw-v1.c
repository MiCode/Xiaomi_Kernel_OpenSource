// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>

#include "mtk-srclken-rc-hw.h"
#include "mtk-srclken-rc-hw-v1.h"


#define TRACE_NUM		8
#define TRACE_P_NUM		4
#define RC_BC_SUPPORT		"mediatek,srclken-rc-broadcast"
#define CFGID_NOT_FOUND     "UNSUPPORT_CFGID"

static bool srclken_rc_bc_support;

static const char *rc_cfg_name[RC_CFG_MAX] = {
	[RC_CFG_REG] = "srclken_rc_cfg",
	[CENTRAL_CFG1] = "central_cfg1",
	[CENTRAL_CFG2] = "central_cfg2",
	[CENTRAL_CFG3] = "central_cfg3",
	[CENTRAL_CFG4] = "central_cfg4",
	[RC_PMRC_ADDR] = "rc_pmrc_addr",
	[SUBSYS_INF] = "subsys_inf",
	[CENTRAL_CFG5] = "central_cfg5",
	[CENTRAL_CFG6] = "central_cfg6",
	[MT_CMD_M_CFG0] = "multi_cmd_m_cfg0",
	[MT_CMD_M_CFG1] = "multi_cmd_m_cfg1",
	[MT_CMD_P_CFG0] = "multi_cmd_p_cfg0",
	[MT_CMD_P_CFG1] = "multi_cmd_p_cfg1",
	[MT_CMD_CFG0] = "multi_cmd_cfg0",
};

static const char *rc_sta_name[RC_STA_MAX] = {
	[CMD_STA] = "cmd_sta",
	[SPI_STA] = "spi_sta",
	[FSM_STA] = "fsm_sta",
	[POPI_STA] = "popi_sta",
	[SPMI_P_STA] = "spmi_p_sta",
};

struct srclken_rc_cfg rc_cfg = {
	SET_REG(rc_cfg_reg, SRCLKEN_RC_CFG, 0xFFFFFFFF, 0)
	SET_REG(central_cfg1, REG_CENTRAL_CFG1, 0xFFFFFFFF, 0)
	SET_REG(central_cfg2, REG_CENTRAL_CFG2, 0xFFFFFFFF, 0)
	SET_REG(central_cfg3, REG_CENTRAL_CFG3, 0xFFFFFFFF, 0)
	SET_REG(central_cfg4, REG_CENTRAL_CFG4, 0xFFFFFFFF, 0)
	SET_REG(rc_pmrc_en_addr, PMIC_RCEN_ADDR_REG, 0xFFFFFFFF, 0)
	SET_REG(subsys_inf_cfg, SUBSYS_INF_CFG, 0xFFFFFFFF, 0)
	SET_REG(m00_cfg, M00_SRCLKEN_CFG, 0xFFFFFFFF, 0)
	SET_REG(central_cfg5, REG_CENTRAL_CFG5, 0xFFFFFFFF, 0)
	SET_REG(central_cfg6, REG_CENTRAL_CFG6, 0xFFFFFFFF, 0)
	SET_REG(mt_m_cfg0, RCEN_MT_M_CFG_0, 0xFFFFFFFF, 0)
	SET_REG(mt_m_cfg1, RCEN_MT_M_CFG_1, 0xFFFFFFFF, 0)
	SET_REG(mt_p_cfg0, RCEN_MT_P_CFG_0, 0xFFFFFFFF, 0)
	SET_REG(mt_p_cfg1, RCEN_MT_P_CFG_1, 0xFFFFFFFF, 0)
	SET_REG(mt_cfg0, RCEN_MT_CFG_0, 0xFFFFFFFF, 0)
};

struct srclken_rc_sta rc_sta = {
	SET_REG(cmd_sta_0, CMD_STA_0, 0xFFFFFFFF, 0)
	SET_REG(cmd_sta_1, CMD_STA_1, 0xFFFFFFFF, 0)
	SET_REG(spi_sta, SPI_STA_0, 0xFFFFFFFF, 0)
	SET_REG(fsm_sta, FSM_STA_0, 0xFFFFFFFF, 0)
	SET_REG(popi_sta, PIPO_STA_0, 0xFFFFFFFF, 0)
	SET_REG(m00_sta, M00_REQ_STA, 0xFFFFFFFF, 0)
	SET_REG(trace_lsb, TRACE_0_LSB, 0xFFFFFFFF, 0)
	SET_REG(trace_msb, TRACE_0_MSB, 0xFFFFFFFF, 0)
	SET_REG(timer_lsb, TIMER_0_LSB, 0xFFFFFFFF, 0)
	SET_REG(timer_msb, TIMER_0_MSB, 0xFFFFFFFF, 0)
	SET_REG(spmi_p_sta, SPMI_P_STA_0, 0xFFFFFFFF, 0)
	SET_REG(trace_p_msb, TRACE_P_0_LSB, 0xFFFFFFFF, 0)
	SET_REG(trace_p_lsb, TRACE_P_0_MSB, 0xFFFFFFFF, 0)
	SET_REG(timer_p_msb, TIMER_P_0_LSB, 0xFFFFFFFF, 0)
	SET_REG(timer_p_lsb, TIMER_P_0_MSB, 0xFFFFFFFF, 0)
};

int srclken_rc_get_cfg_count(void)
{
	if (!srclken_rc_bc_support)
		return RC_CFG_NON_BC_MAX;

	return RC_CFG_MAX;
}

const char *srclken_rc_get_cfg_name(u32 idx)
{
	if ((!srclken_rc_bc_support && idx >= RC_CFG_NON_BC_MAX)
			|| (srclken_rc_bc_support && idx >= RC_CFG_MAX))
		return CFGID_NOT_FOUND;

	return rc_cfg_name[idx];
}

int srclken_rc_get_cfg_val(const char *name, u32 *val)
{
	if (!strcmp(rc_cfg_name[RC_CFG_REG], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._rc_cfg_reg, val);
	else if (!strcmp(rc_cfg_name[CENTRAL_CFG1], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._central_cfg1, val);
	else if (!strcmp(rc_cfg_name[CENTRAL_CFG2], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._central_cfg2, val);
	else if (!strcmp(rc_cfg_name[CENTRAL_CFG3], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._central_cfg3, val);
	else if (!strcmp(rc_cfg_name[CENTRAL_CFG4], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._central_cfg4, val);
	else if (!strcmp(rc_cfg_name[RC_PMRC_ADDR], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._rc_pmrc_en_addr, val);
	else if (!strcmp(rc_cfg_name[SUBSYS_INF], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._subsys_inf_cfg, val);

	if (!srclken_rc_bc_support)
		return -EPERM;
	else if (!strcmp(rc_cfg_name[CENTRAL_CFG5], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._central_cfg5, val);
	else if (!strcmp(rc_cfg_name[CENTRAL_CFG6], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._central_cfg6, val);
	else if (!strcmp(rc_cfg_name[MT_CMD_M_CFG0], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._mt_m_cfg0, val);
	else if (!strcmp(rc_cfg_name[MT_CMD_M_CFG1], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._mt_m_cfg1, val);
	else if (!strcmp(rc_cfg_name[MT_CMD_P_CFG0], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._mt_p_cfg0, val);
	else if (!strcmp(rc_cfg_name[MT_CMD_P_CFG1], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._mt_p_cfg1, val);
	else if (!strcmp(rc_cfg_name[MT_CMD_CFG0], name))
		return clk_buf_read(&rc_cfg.hw, &rc_cfg._mt_cfg0, val);

	return -EPERM;
}

u8 rc_get_trace_num(void)
{
	return TRACE_NUM;
}

int srclken_rc_dump_time(u8 idx, char *buf, u32 buf_size)
{
	u32 time_msb = 0;
	u32 time_lsb = 0;
	u32 time_p_msb = 0;
	u32 time_p_lsb = 0;
	int len = 0;

	if (idx >= TRACE_NUM)
		return len;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._timer_msb,
			&time_msb, idx * 8))
		return len;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._timer_lsb,
			&time_lsb, idx * 8))
		return len;

	len += snprintf(buf + len, buf_size - len,
		"TIME%u LSB: 0x%x ", idx, time_lsb);

	len += snprintf(buf + len, buf_size - len,
		"TIME%u MSB: 0x%x\n", idx, time_msb);

	if (!srclken_rc_bc_support || idx >= TRACE_P_NUM)
		return len;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._timer_p_msb,
			&time_p_msb, idx * 8))
		return len;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._timer_p_lsb,
			&time_p_lsb, idx * 8))
		return len;

	len -= 1;

	len += snprintf(buf + len, buf_size - len,
		", TIME_P_%u LSB: 0x%x ", idx, time_p_lsb);

	len += snprintf(buf + len, buf_size - len,
		", TIME_P_%u_MSB: 0x%x\n", idx, time_p_msb);

	return len;
}

int srclken_rc_dump_trace(u8 idx, char *buf, u32 buf_size)
{
	u32 trace_msb = 0;
	u32 trace_lsb = 0;
	u32 trace_p_msb = 0;
	u32 trace_p_lsb = 0;
	u32 mofs = idx * 8;
	u32 lofs = idx * 8;
	int len = 0;

	if (idx >= TRACE_NUM)
		return len;

	if (!srclken_rc_bc_support) {
		mofs = (idx > 1) ? (mofs + 8) : (idx > 0) ? (mofs + 4) : mofs;
		lofs = (idx > 2) ? (lofs + 8) : (idx > 0) ? (lofs + 4) : lofs;
	}

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._trace_msb,
			&trace_msb, mofs))
		return len;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._trace_lsb,
			&trace_lsb, lofs))
		return len;

	len += snprintf(buf + len, buf_size - len,
		"TRACE%u LSB: 0x%x ", idx, trace_lsb);

	len += snprintf(buf + len, buf_size - len,
		"TRACE%u MSB: 0x%x\n", idx, trace_msb);

	if (!srclken_rc_bc_support || idx >= TRACE_P_NUM)
		return len;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._trace_p_msb,
			&trace_p_msb, mofs))
		return len;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._trace_p_lsb,
			&trace_p_lsb, lofs))
		return len;

	len -= 1;

	len += snprintf(buf + len, buf_size - len,
		", TRACE_P_%u LSB: 0x%x ", idx, trace_p_msb);

	len += snprintf(buf + len, buf_size - len,
		", TRACE_P_%u MSB: 0x%x\n", idx, trace_p_lsb);

	return len;
}

static int srclken_rc_dump_cmd_sta(char *buf)
{
	u32 cmd_sta_0 = 0;
	u32 cmd_sta_1 = 0;
	int len = 0;

	if (clk_buf_read(&rc_sta.hw, &rc_sta._cmd_sta_0, &cmd_sta_0))
		return len;

	if (clk_buf_read(&rc_sta.hw, &rc_sta._cmd_sta_1, &cmd_sta_1))
		return len;

	len += snprintf(buf + len, PAGE_SIZE - len, "[CMD] -\n");

	len += snprintf(buf + len, PAGE_SIZE - len,
		"\t(xo/dcxo/target) - PMIC (0x%x/0x%x/0x%x)\n",
		EXTRACT_REG_VAL(cmd_sta_0,
			CUR_PMIC_RC_EN_MASK,
			CUR_PMIC_RC_EN_SHIFT),
		EXTRACT_REG_VAL(cmd_sta_0,
			CUR_PMIC_DCXO_MODE_MASK,
			CUR_PMIC_DCXO_MODE_SHIFT),
		EXTRACT_REG_VAL(cmd_sta_0,
			CMD_ARB_TARGET_MASK,
			CMD_ARB_TARGET_SHIFT));

	len += snprintf(buf + len, PAGE_SIZE - len,
		"\t(xo/dcxo) - subsys (0x%x/0x%x)\n",
		EXTRACT_REG_VAL(cmd_sta_1,
			CUR_LATCHED_REQ_MASK,
			CUR_LATCHED_REQ_SHIFT),
		EXTRACT_REG_VAL(cmd_sta_1,
			CUR_LATCHED_DCXO_MASK,
			CUR_LATCHED_DCXO_SHIFT));

	return len;
}

static int srclken_rc_dump_spi_sta(char *buf)
{
	u32 spi_sta = 0;
	int len = 0;

	if (clk_buf_read(&rc_sta.hw, &rc_sta._spi_sta, &spi_sta))
		return len;

	len += snprintf(buf + len, PAGE_SIZE - len, "[SPI] -\n");

	len += snprintf(buf + len, PAGE_SIZE - len,
		"\t(req/ack/addr/data) - (%d/%d/%x/%x)\n",
		EXTRACT_REG_VAL(spi_sta,
			SPI_CMD_REQ_MASK,
			SPI_CMD_REQ_SHIFT),
		EXTRACT_REG_VAL(spi_sta,
			SPI_CMD_ACK_MASK,
			SPI_CMD_ACK_SHIFT),
		EXTRACT_REG_VAL(spi_sta,
			SPI_CMD_ADDR_MASK,
			SPI_CMD_ADDR_SHIFT),
		EXTRACT_REG_VAL(spi_sta,
			SPI_CMD_DATA_MASK,
			SPI_CMD_DATA_SHIFT));

	return len;
}

static int srclken_rc_dump_spmi_p_sta(char *buf)
{
	u32 spmi_p_sta = 0;
	int len = 0;

	if (!srclken_rc_bc_support)
		return len;

	if (clk_buf_read(&rc_sta.hw, &rc_sta._spmi_p_sta, &spmi_p_sta))
		return len;

	len += snprintf(buf + len, PAGE_SIZE - len, "[SPMI] -\n");

	len += snprintf(buf + len, PAGE_SIZE - len,
		"\t(req/ack/addr/data) - (%d/%d/%x/%x)\n",
		EXTRACT_REG_VAL(spmi_p_sta,
			SPMI_P_CMD_REQ_MASK,
			SPMI_P_CMD_REQ_SHIFT),
		EXTRACT_REG_VAL(spmi_p_sta,
			SPMI_P_CMD_ACK_MASK,
			SPMI_P_CMD_ACK_SHIFT),
		EXTRACT_REG_VAL(spmi_p_sta,
			SPMI_P_CMD_ADDR_MASK,
			SPMI_P_CMD_ADDR_SHIFT),
		EXTRACT_REG_VAL(spmi_p_sta,
			SPMI_P_CMD_DATA_MASK,
			SPMI_P_CMD_DATA_SHIFT));

	return len;
}

static int srclken_rc_dump_fsm_sta(char *buf)
{
	u32 fsm_sta = 0;
	int len = 0;

	if (clk_buf_read(&rc_sta.hw, &rc_sta._fsm_sta, &fsm_sta))
		return len;

	len += snprintf(buf + len, PAGE_SIZE - len, "[FSM] -\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "\tRC Request (%s)\n",
		((fsm_sta & ANY_32K_REQ) != ANY_32K_REQ) ? "none" :
		((fsm_sta & ANY_BYPASS_REQ) == ANY_BYPASS_REQ) ? "bys" :
		((fsm_sta & ANY_NON_DCXO_REQ) == ANY_NON_DCXO_REQ) ? "nd" :
		((fsm_sta & ANY_DCXO_REQ) == ANY_DCXO_REQ) ? "dcxo" : "err");

	len += snprintf(buf+len, PAGE_SIZE-len,
		"\t(vcore/wrap_c/wrap_s/ulposc/spi) = (%x/%x/%x/%x/%x)\n",
		EXTRACT_REG_VAL(fsm_sta,
			CUR_VCORE_ULPOSC_STA_MASK,
			CUR_VCORE_ULPOSC_STA_SHIFT),
		EXTRACT_REG_VAL(fsm_sta,
			CUR_PWRAP_CMD_STA_MASK,
			CUR_PWRAP_CMD_STA_SHIFT),
		EXTRACT_REG_VAL(fsm_sta,
			CUR_PWRAP_SLP_STA_MASK,
			CUR_PWRAP_SLP_STA_SHIFT),
		EXTRACT_REG_VAL(fsm_sta,
			CUR_ULPOSC_STA_MASK,
			CUR_ULPOSC_STA_SHIFT),
		EXTRACT_REG_VAL(fsm_sta,
			CUR_SPI_STA_MASK,
			CUR_SPI_STA_SHIFT));

	return len;
}

static int srclken_rc_dump_popi_sta(char *buf)
{
	uint32_t popi_sta;
	int len = 0;

	if (clk_buf_read(&rc_sta.hw, &rc_sta._popi_sta, &popi_sta))
		return len;

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

int srclken_rc_dump_sta(const char *name, char *buf)
{
	int len = 0;

	if (!strcmp(rc_sta_name[CMD_STA], name))
		return srclken_rc_dump_cmd_sta(buf);
	else if (!strcmp(rc_sta_name[SPI_STA], name))
		return srclken_rc_dump_spi_sta(buf);
	else if (!strcmp(rc_sta_name[FSM_STA], name))
		return srclken_rc_dump_fsm_sta(buf);
	else if (!strcmp(rc_sta_name[POPI_STA], name))
		return srclken_rc_dump_popi_sta(buf);

	if (!srclken_rc_bc_support)
		goto RC_DUMP_STA_OUT;
	else if (!strcmp(rc_sta_name[SPMI_P_STA], name))
		return srclken_rc_dump_spmi_p_sta(buf);

RC_DUMP_STA_OUT:
	len += snprintf(buf + len, PAGE_SIZE - len,
		"unknown sta reg name: %s\n", name);

	return len;
}

int srclken_rc_get_subsys_req_mode(u8 idx, u32 *mode)
{
	int ret = 0;

	if (idx >= srclken_rc_get_subsys_count()) {
		pr_notice("invalid subsys idx: %u\n", idx);
		return -EINVAL;
	}

	ret = clk_buf_read_with_ofs(&rc_cfg.hw, &rc_cfg._m00_cfg,
			mode, idx * 4);
	if (ret)
		return ret;

	(*mode) = ((*mode) & (SW_SRCLKEN_RC_EN_MASK << SW_SRCLKEN_RC_EN_SHIFT))
		>> SW_SRCLKEN_RC_EN_SHIFT;

	return ret;
}

int srclken_rc_get_subsys_sw_req(u8 idx, u32 *req)
{
	int ret = 0;

	if (idx >= srclken_rc_get_subsys_count()) {
		pr_notice("invalid subsys idx: %u\n", idx);
		return -EINVAL;
	}

	ret = clk_buf_read_with_ofs(&rc_cfg.hw, &rc_cfg._m00_cfg, req, idx * 4);
	if (ret)
		return ret;

	(*req) = ((*req) & (SW_RC_REQ_MASK << SW_RC_REQ_SHIFT))
		>> SW_RC_REQ_SHIFT;

	return ret;
}

int srclken_rc_dump_subsys_sta(u8 idx, char *buf)
{
	u32 sys_sta = 0;
	u32 offset = 0;
	u32 filter;
	u32 cmd_ok;
	int len = 0;

	if (idx >= srclken_rc_get_subsys_count())
		return len;

	offset = (idx > 0) ? (idx + 1) : idx;

	if (clk_buf_read_with_ofs(&rc_sta.hw, &rc_sta._m00_sta,
			&sys_sta, offset * 4))
		return len;

	filter = (sys_sta >> REQ_FILTER_SHIFT) & REQ_FILTER_MASK;
	cmd_ok = (sys_sta >> CMD_OK_SHIFT) & CMD_OK_MASK;
	len += snprintf(buf + len, PAGE_SIZE - len,
		"\t(req/ack) - FPM(%d/%d), BBLPM(%d/%d)\n",
		(sys_sta >> RC_FPM_REQ_SHIFT) & RC_FPM_REQ_MASK,
		(sys_sta >> RC_FPM_ACK_SHIFT) & RC_FPM_ACK_MASK,
		(sys_sta >> RC_BBLPM_REQ_SHIFT) & RC_BBLPM_REQ_MASK,
		(sys_sta >> RC_BBLPM_ACK_SHIFT) & RC_BBLPM_ACK_MASK);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"\t(cur/target/chg) - DCXO(%x/%x/%d)\n",
		(sys_sta >> M00_CUR_PMIC_DCXO_MODE_SHIFT)
			& M00_CUR_PMIC_DCXO_MODE_MASK,
		(sys_sta >> DCXO_MODE_TARGET_SHIFT) & DCXO_MODE_TARGET_MASK,
		((sys_sta >> DCXO_MODE_CHANGED_SHIFT) & DCXO_MODE_TARGET_MASK) ?
		(((sys_sta >> DCXO_MODE_EQUAL_SHIFT) & DCXO_MODE_EQUAL_SHIFT) ?
		0 : 1) : 0);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"\t(done/ongo/filt/cur/cmd) - REQ(%d/%d/%s/%x/%s)\n",
		(sys_sta >> ALLOW_REQ_IN_SHIFT) & ALLOW_REQ_IN_MASK,
		(sys_sta >> RC_CMD_ON_GO_SHIFT) & RC_CMD_ON_GO_MASK,
		(filter & 0x1) ? "bys" : (filter & 0x2) ? "dcxo" :
		(filter & 0x4) ? "nd" : "none",
		(sys_sta >> CUR_REQ_STATE_SHIFT) & CUR_REQ_STATE_MASK,
		(cmd_ok & 0x3) == 0x3 ? "cmd_ok" : "cmd_not_ok");

	len += snprintf(buf + len, PAGE_SIZE - len,
		"\tcur_pmic_bit(%d)\n",
		(sys_sta >> CUR_PMIC_RCEN_BIT_SHIFT) & CUR_PMIC_RCEN_BIT_MASK);
	return len;
}


int __srclken_rc_subsys_ctrl(struct srclken_rc_subsys *subsys,
		enum CLKBUF_CTL_CMD cmd, enum SRCLKEN_RC_REQ rc_req)
{
	u32 val = 0;
	u32 cfg6_val = 0;
	int ret = 0;

	ret = clk_buf_read_with_ofs(&rc_cfg.hw, &rc_cfg._m00_cfg,
			&val, subsys->idx * 4);
	if (ret) {
		pr_notice("read srclken_rc subsys cfg failed\n");
		return ret;
	}

	ret = clk_buf_read_with_ofs(&rc_cfg.hw, &rc_cfg._central_cfg6,
			&cfg6_val, 0);
	if (ret) {
		pr_notice("read srclken_rc cfg6 failed\n");
		return ret;
	}
	/* Reset previous CFG6 setting (Force subsys vote LPM) */
	cfg6_val &= (~(1 << subsys->idx));

	if (cmd <= CLKBUF_CMD_SW) {
		val &= (~(SW_SRCLKEN_RC_EN_MASK << SW_SRCLKEN_RC_EN_SHIFT));
		val |= (SW_SRCLKEN_RC_EN_MASK << SW_SRCLKEN_RC_EN_SHIFT);
		if (rc_req == RC_FPM_REQ) {
			val &= (~(SW_RC_REQ_MASK << SW_RC_REQ_SHIFT));
			val |= (SW_FPM_EN_MASK << SW_FPM_EN_SHIFT);
		} else if (rc_req == RC_LPM_VOTE_REQ) {
			val &= (~(SW_RC_REQ_MASK << SW_RC_REQ_SHIFT));
			val |= (SW_FPM_EN_MASK << SW_FPM_EN_SHIFT);
			cfg6_val |= (1 << subsys->idx);
		} else if (rc_req == RC_LPM_REQ) {
			val &= (~(SW_RC_REQ_MASK << SW_RC_REQ_SHIFT));
		}
	} else if (cmd == CLKBUF_CMD_HW) {
		val &= (~(SW_RC_REQ_MASK << SW_RC_REQ_SHIFT));
		val &= (~(SW_SRCLKEN_RC_EN_MASK << SW_SRCLKEN_RC_EN_SHIFT));
	} else if (cmd == CLKBUF_CMD_INIT) {
		val &= (~(SW_RC_REQ_MASK << SW_RC_REQ_SHIFT));
		val |= (subsys->init_req << SW_RC_REQ_SHIFT);
		val &= (~(SW_SRCLKEN_RC_EN_MASK << SW_SRCLKEN_RC_EN_SHIFT));
		val |= (subsys->init_mode << SW_SRCLKEN_RC_EN_SHIFT);
	}

	ret = clk_buf_write_with_ofs(&rc_cfg.hw, &rc_cfg._central_cfg6,
			cfg6_val, 0);
	if (ret)
		pr_notice("write srclken_rc cfg6 failed\n");

	ret = clk_buf_write_with_ofs(&rc_cfg.hw, &rc_cfg._m00_cfg,
			val, subsys->idx * 4);
	if (ret)
		pr_notice("write srclken_rc subsys cfg failed\n");

	return ret;
}

static int rc_xo_ctl(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	struct srclken_rc_subsys *subsys = NULL;
	int ret = 0;

	if (ctl_cmd->hw_id != CLKBUF_RC_SUBSYS
			&& ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	subsys = container_of(xo_buf_ctl, struct srclken_rc_subsys, xo_buf_ctl);

	ret = __srclken_rc_subsys_ctrl(subsys, ctl_cmd->cmd, ctl_cmd->rc_req);

	return ret;
}

static int rc_xo_ctl_show(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
		struct xo_buf_ctl_t *xo_buf_ctl)
{
	struct srclken_rc_subsys *subsys = NULL;
	char *buf = NULL;
	int len = 0;
	int ret = 0;

	if (ctl_cmd->hw_id != CLKBUF_RC_SUBSYS
			&& ctl_cmd->hw_id != CLKBUF_HW_ALL)
		return 0;

	subsys = container_of(xo_buf_ctl, struct srclken_rc_subsys, xo_buf_ctl);

	buf = ctl_cmd->buf;
	if (!buf) {
		pr_notice("Null output buffer\n");
		return -EPERM;
	}
	/* TODO: check again */
	len = strlen(buf);

	len += snprintf(buf + len, PAGE_SIZE - len, "[%s] -\n", subsys->name);

	ret = srclken_rc_dump_subsys_sta(subsys->idx, buf + len);

	pr_notice("%s\n", buf);

	return 0;
}

void __srclken_rc_xo_buf_callback_init(struct xo_buf_ctl_t *xo_buf_ctl)
{
	xo_buf_ctl->clk_buf_on_ctrl = rc_xo_ctl;
	xo_buf_ctl->clk_buf_off_ctrl = rc_xo_ctl;
	xo_buf_ctl->clk_buf_sw_ctrl = rc_xo_ctl;
	xo_buf_ctl->clk_buf_hw_ctrl = rc_xo_ctl;
	xo_buf_ctl->clk_buf_init_ctrl = rc_xo_ctl;
	xo_buf_ctl->clk_buf_show_ctrl = rc_xo_ctl_show;
}

static int srclken_rc_broadcast_hw_shift(void)
{
	if (!srclken_rc_bc_support) {
		rc_cfg._central_cfg5.ofs = 0;
		rc_cfg._central_cfg5.mask = 0;
		rc_cfg._central_cfg6.ofs = 0;
		rc_cfg._central_cfg6.mask = 0;
		rc_cfg._mt_m_cfg0.ofs = 0;
		rc_cfg._mt_m_cfg0.mask = 0;
		rc_cfg._mt_m_cfg1.ofs = 0;
		rc_cfg._mt_m_cfg1.mask = 0;
		rc_cfg._mt_p_cfg0.ofs = 0;
		rc_cfg._mt_p_cfg0.mask = 0;
		rc_cfg._mt_p_cfg1.ofs = 0;
		rc_cfg._mt_p_cfg1.mask = 0;
		rc_cfg._mt_cfg0.ofs = 0;
		rc_cfg._mt_cfg0.mask = 0;
		rc_sta._spmi_p_sta.ofs = 0;
		rc_sta._spmi_p_sta.mask = 0;
		rc_sta._timer_p_msb.ofs = 0;
		rc_sta._timer_p_msb.mask = 0;
		rc_sta._timer_p_lsb.ofs = 0;
		rc_sta._timer_p_lsb.mask = 0;
		rc_sta._trace_p_msb.ofs = 0;
		rc_sta._trace_p_msb.mask = 0;
		rc_sta._trace_p_lsb.ofs = 0;
		rc_sta._trace_p_lsb.mask = 0;

		return 0;
	}

	rc_cfg._central_cfg4.ofs = BC_REG_CENTRAL_CFG4;
	rc_sta._trace_lsb.ofs = BC_TRACE_0_LSB;
	rc_sta._trace_msb.ofs = BC_TRACE_0_MSB;
	rc_sta._timer_lsb.ofs = BC_TIMER_0_LSB;
	rc_sta._timer_msb.ofs = BC_TIMER_0_MSB;

	return 0;
}

static int srclken_rc_dts_base_init(struct platform_device *pdev)
{
	struct resource *cfg_res;
	struct resource *sta_res;

	cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sta_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	rc_cfg.hw.base.addr = devm_ioremap(&pdev->dev,
			cfg_res->start, resource_size(cfg_res));
	if (IS_ERR(rc_cfg.hw.base.addr)) {
		pr_notice("get rc_cfg base failed: %l\n",
			PTR_ERR(rc_cfg.hw.base.addr));
		goto RC_BASE_INIT_FAILED;
	}
	rc_cfg.hw.enable = true;

	rc_sta.hw.base.addr = devm_ioremap(&pdev->dev,
			sta_res->start, resource_size(sta_res));
	if (IS_ERR(rc_sta.hw.base.addr)) {
		pr_notice("get rc_sta base failed\n");
		goto RC_BASE_INIT_FAILED;
	}
	rc_sta.hw.enable = true;

	return 0;

RC_BASE_INIT_FAILED:
	return -EGET_BASE_FAILED;
}

int srclken_rc_hw_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	ret = srclken_rc_dts_base_init(pdev);
	if (ret)
		return ret;

	srclken_rc_bc_support = of_property_read_bool(node, RC_BC_SUPPORT);
	ret = srclken_rc_broadcast_hw_shift();

	return ret;
}
