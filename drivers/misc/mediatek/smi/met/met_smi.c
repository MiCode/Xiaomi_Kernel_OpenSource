/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <met_drv.h>
#include <sync_write.h>

#include "met_smi.h"
#include "met_smi_config.h"
#include "../smi_public.h"

#ifdef SMI_MET_IPI
#include <mtk_qos_ipi.h>
#endif

#define	smi_readl		readl
#define	smi_reg_sync_writel	mt_reg_sync_writel
static void __iomem *(SMICommBaseAddr[SMI_MET_TOTAL_MASTER_NUM]);
static void __iomem **SMILarbBaseAddr = SMICommBaseAddr + MET_SMI_COMM_NUM;
static char debug_msg[SMI_MET_DEBUGBUF_SIZE];
static unsigned int smi_parallel_mode;

#define SMI_MAX_STR_BUFFER 512
#define SMI_MET_MSG(fmt, args...) \
	do { \
		char buf[SMI_MAX_STR_BUFFER]; \
		snprintf(buf, SMI_MAX_STR_BUFFER, fmt, ##args); \
		met_smi_debug(buf); \
	} while (0)

#define SMI_MET_KERNEL_MSG(fmt, args...) \
	do { \
		pr_notice(fmt, ##args); \
		SMI_MET_MSG(fmt, ##args); \
	} while (0)

static void met_smi_debug(char *debug_log)
{
	SMI_MET_PRINTK("%s", debug_log);
}

/* get larb config */
int MET_SMI_GetEna(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_ENA(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetClr(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_CLR(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetPortNo(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_PORT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetCon(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_CON(SMILarbBaseAddr[larbno]));
}

/* get larb counter */
int MET_SMI_GetActiveCnt(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_ACT_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetRequestCnt(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_REQ_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetBeatCnt(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_BEA_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetByteCnt(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_BYT_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetCPCnt(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_CP_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetDPCnt(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_DP_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetOSTDCnt(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_OSTD_CNT(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetCP_MAX(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_CP_MAX(SMILarbBaseAddr[larbno]));
}

int MET_SMI_GetOSTD_MAX(int larbno)
{
	return ioread32(SMI_MET_LARB_MON_OSTD_MAX(SMILarbBaseAddr[larbno]));
}

/* get common config */
int MET_SMI_Comm_GetEna(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_ENA(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetClr(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_CLR(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetType(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_TYPE(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetCon(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_CON(SMICommBaseAddr[commonno]));
}

/* get common counter */
int MET_SMI_Comm_GetActiveCnt(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_ACT_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetRequestCnt(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_REQ_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetBeatCnt(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_BEA_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetByteCnt(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_BYT_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetCPCnt(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_CP_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetDPCnt(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_DP_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetOSTDCnt(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_OSTD_CNT(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetCP_MAX(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_CP_MAX(SMICommBaseAddr[commonno]));
}

int MET_SMI_Comm_GetOSTD_MAX(int commonno)
{
	return ioread32(SMI_MET_COMM_MON_OSTD_MAX(SMICommBaseAddr[commonno]));
}


static inline int check_master_valid(int master)
{
	if ((master < 0) || (master >= SMI_MET_TOTAL_MASTER_NUM))
		return MET_SMI_FAIL;
	return MET_SMI_SUCCESS;
}

/*
 * Note:
 * The argument - master has already been checked by check_master_valid(),
 * so we don't need to check it again.
 */
static inline int check_port_valid(int master, int port)
{
	if (port < smi_map[master].count)
		return MET_SMI_SUCCESS;
	else
		return MET_SMI_FAIL;
}

static int check_legacy_port_valid(int master, int port)
{
	if (smi_parallel_mode == 0)
		return check_port_valid(master, port);
	else
		return MET_SMI_FAIL;
}

static int check_parallel_port_valid(int master, const int *port, int size)
{
	int i;
	int ret = MET_SMI_SUCCESS;

	if (smi_parallel_mode == 1) {
		for (i = 0; i < size; i++) {
			if ((port[i] != -1) &&
				(check_port_valid(master, port[i]) != ret)) {
				ret = MET_SMI_FAIL;
				break;
			}
		}
	} else {
		ret = MET_SMI_FAIL;
	}

	return ret;
}

/*
 * Note:
 * The arguments - master & port have already been checked by
 * check_master_valid() & check_port_valid(), so we don't need
 * to check them again.
 */
static inline int check_rwtype_valid(int master, int port, int rwtype)
{
	if (smi_map[master].desc[port].rwtype == SMI_RW_ALL) {
		if ((rwtype == SMI_RW_ALL)
		    || (rwtype == SMI_READ_ONLY)
		    || (rwtype == SMI_WRITE_ONLY))
			return MET_SMI_SUCCESS;
		else
			return MET_SMI_FAIL;
	} else if (smi_map[master].desc[port].rwtype == SMI_RW_RESPECTIVE) {
		if ((rwtype == SMI_READ_ONLY) || (rwtype == SMI_WRITE_ONLY))
			return MET_SMI_SUCCESS;
		else
			return MET_SMI_FAIL;
	} else {
		return MET_SMI_FAIL;
	}
}

/*
 * Note:
 * The arguments - master & port have already been checked by
 * check_master_valid() & check_port_valid(), so we don't need
 * to check them again.
 */
static inline int check_desttype_valid(int master, int port, int desttype)
{
	if ((smi_map[master].desc[port].desttype == SMI_DEST_NONE)
	    || (smi_map[master].desc[port].desttype == desttype))
		return MET_SMI_SUCCESS;
	else if (smi_map[master].desc[port].desttype == SMI_DEST_ALL) {
		if ((desttype == SMI_DEST_ALL)
		    || (desttype == SMI_DEST_EMI)
		    || (desttype == SMI_DEST_INTERNAL))
			return MET_SMI_SUCCESS;
		else
			return MET_SMI_FAIL;
	} else {
		return MET_SMI_FAIL;
	}
	return MET_SMI_SUCCESS;
}

static inline int check_reqtype_valid(int master, int port, int reqtype)
{
	if ((reqtype & ~0x3) != 0)
		return MET_SMI_FAIL;
	else
		return MET_SMI_SUCCESS;
}

/*
 * Note:
 * The arguments - master & port have already been checked by
 * check_master_valid() & check_port_valid(), so we don't need
 * to check them again.
 */
static inline int check_bustype_valid(int master, int port, int bustype)
{
	if ((smi_map[master].desc[port].bustype == SMI_BUS_NONE)
	    || (smi_map[master].desc[port].bustype == bustype))
		return MET_SMI_SUCCESS;
	else
		return MET_SMI_FAIL;
	return MET_SMI_SUCCESS;
}

int SMI_MET_SMI_base_Init(void)
{
	int i;
	int err = 0;

	/* map address for SMI_COMMs & SMI_LARBs */
	for (i = 0; i < MET_SMI_COMM_NUM; i++) {
		SMICommBaseAddr[i] = smi_base_addr_get(MET_SMI_LARB_NUM);
		if (SMICommBaseAddr[i] == 0) {
			err = MET_SMI_FAIL;
			break;
		}
		pr_debug("MET SMI:  get_common_base_addr to %p\n",
			SMICommBaseAddr[i]);
	}
	for (i = 0; i < MET_SMI_LARB_NUM; i++) {
		SMILarbBaseAddr[i] = smi_base_addr_get(i);
		if (SMILarbBaseAddr[i] == 0) {
			err = MET_SMI_FAIL;
			break;
		}
		pr_debug("MET SMI:  get_larb_base_addr<%d> to %p\n",
			i, SMILarbBaseAddr[i]);
	}
	return err;
}

static const char smi_met_header_legacy_mode[] =
	"TS0,TS1,metadata,active,request,beat,byte,CP,DP,OSTD,CPMAX,OSTDMAX\n";
static const char smi_met_header_parallel_mode[] = "TS0,TS1,metadata,byte\n";
static int larb_already_configed[SMI_MET_TOTAL_MASTER_NUM];
static int parall_port_num[SMI_MET_TOTAL_MASTER_NUM];
struct met_smi_conf met_smi_conf_array[SMI_MET_TOTAL_MASTER_NUM];

int SMI_MET_get_header(unsigned int parallel_mode, char *buf,
	unsigned int *larb_number)
{
	int ret = 0;

	*larb_number = MET_SMI_LARB_NUM;
	if (parallel_mode == 0)
		ret = snprintf(buf, PAGE_SIZE, smi_met_header_legacy_mode);
	else
		ret = snprintf(buf, PAGE_SIZE, smi_met_header_parallel_mode);

	smi_parallel_mode = parallel_mode;
	return ret;
}
EXPORT_SYMBOL(SMI_MET_get_header);

int SMI_MET_get_header_ext(unsigned int parallel_mode, char *buf,
	unsigned int *larb_number, unsigned int *comm_number,
	unsigned int *larb_monitor_number, unsigned int *comm_monitor_number)
{
	*comm_number = MET_SMI_COMM_NUM;
	*larb_monitor_number = 1;
	*comm_monitor_number = 1;
	return SMI_MET_get_header(parallel_mode, buf, larb_number);
}
EXPORT_SYMBOL(SMI_MET_get_header_ext);

int SMI_MET_type_check(unsigned int parallel_mode, struct met_smi_conf *config)
{
	int ret = MET_SMI_SUCCESS;

	if (!config) {
		met_smi_debug("met_smi_conf object is NULL\n");
		return MET_SMI_FAIL;
	}

	SMI_MET_KERNEL_MSG(
		"master=%d, reqtype=%d, desttype=%d, port=%d:%d:%d:%d:, rwtype=%d:%d:%d:%d\n",
		config->master, config->reqtype, config->desttype,
		config->port[0], config->port[1], config->port[2],
		config->port[3], config->rwtype[0], config->rwtype[1],
		config->rwtype[2], config->rwtype[3]);

	if (check_master_valid(config->master)) {
		met_smi_debug("master is not available");
		return MET_SMI_FAIL;
	}

	if (parallel_mode != 0 && parallel_mode != 1) {
		met_smi_debug("parallel_mode is not available");
		return MET_SMI_FAIL;
	} else if (parallel_mode == 1) {
		ret = check_parallel_port_valid(config->master, config->port,
			SMI_MET_MAX_PORT_NUM);
	} else {
		ret = check_legacy_port_valid(config->master, config->port[0])
			|| (check_rwtype_valid(config->master, config->port[0],
				config->rwtype[0]))
			|| (check_desttype_valid(config->master,
				config->port[0], config->desttype))
			|| (check_reqtype_valid(config->master, config->port[0],
				config->reqtype));
	}

	return ret;
}
EXPORT_SYMBOL(SMI_MET_type_check);

static int already_configured;

#ifndef SMI_MET_IPI
static void SMI_MET_clear_monitor(void)
{
	int larbno;
	unsigned int u4RegVal;

	for (larbno = 0; larbno < MET_SMI_LARB_NUM; larbno++) {
		u4RegVal = smi_readl(
			SMI_MET_LARB_MON_CLR(SMILarbBaseAddr[larbno]));
		smi_reg_sync_writel(u4RegVal | 0x1,
			SMI_MET_LARB_MON_CLR(SMILarbBaseAddr[larbno]));
		smi_reg_sync_writel(u4RegVal & ~0x1,
			SMI_MET_LARB_MON_CLR(SMILarbBaseAddr[larbno]));
	}
	u4RegVal = smi_readl(SMI_MET_COMM_MON_CLR(SMICommBaseAddr[0]));
	smi_reg_sync_writel(u4RegVal | 0x1,
		SMI_MET_COMM_MON_CLR(SMICommBaseAddr[0])); /* enable clear */
	smi_reg_sync_writel(u4RegVal & ~0x1,
		SMI_MET_COMM_MON_CLR(SMICommBaseAddr[0])); /* disable clear */
}

static void SMI_MET_monitor(const bool on_off)
{
	int larbno;
	unsigned int u4RegVal;

	for (larbno = 0; larbno < MET_SMI_LARB_NUM; larbno++) {
		u4RegVal = smi_readl(
			SMI_MET_LARB_MON_ENA(SMILarbBaseAddr[larbno]));
		if (on_off)
			u4RegVal |= 0x1; /* start monitor counting */
		else
			u4RegVal &= ~0x1; /* stop monitor counting */
		smi_reg_sync_writel(u4RegVal,
			SMI_MET_LARB_MON_ENA(SMILarbBaseAddr[larbno]));
	}
	u4RegVal = smi_readl(SMI_MET_COMM_MON_ENA(SMICommBaseAddr[0]));
	if (on_off)
		u4RegVal |= 0x1; /* start monitor counting */
	else
		u4RegVal &= ~0x1; /* stop monitor counting */
	smi_reg_sync_writel(u4RegVal, SMI_MET_COMM_MON_ENA(SMICommBaseAddr[0]));
}

static char *SMI_MET_ms_formatH_EOL(char *__restrict__ buf, unsigned char cnt,
	unsigned int *__restrict__ value)
{
	char	*s = buf;
	int	len;

	if (cnt == 0) {
		buf[0] = '\0';
		return buf;
	}

	switch (cnt % 4) {
	case 1:
		len = sprintf(s, "%x", value[0]);
		s += len;
		value += 1;
		cnt -= 1;
		break;
	case 2:
		len = sprintf(s, "%x,%x", value[0], value[1]);
		s += len;
		value += 2;
		cnt -= 2;
		break;
	case 3:
		len = sprintf(s, "%x,%x,%x", value[0], value[1], value[2]);
		s += len;
		value += 3;
		cnt -= 3;
		break;
	case 0:
		len = sprintf(s, "%x,%x,%x,%x",
			value[0], value[1], value[2], value[3]);
		s += len;
		value += 4;
		cnt -= 4;
		break;
	}

	while (cnt) {
		len = sprintf(s, ",%x,%x,%x,%x",
			value[0], value[1], value[2], value[3]);
		s += len;
		value += 4;
		cnt -= 4;
	}

	s[0] = '\n';
	s[1] = '\0';

	return (s + 1);
}

static noinline void ms_smi(unsigned char cnt, unsigned int *value)
{
	char	*SOB, *EOB;

	SMI_MET_PRINTK_GETBUF(&SOB, &EOB);
	EOB = SMI_MET_ms_formatH_EOL(EOB, cnt, value);
	SMI_MET_PRINTK_PUTBUF(SOB, EOB);
}

#else
#define SMI_MET_MASK(val, mask, bit)	(((val) & (mask)) << (bit))
static inline u32
smi_met_enc(const u32 para, const struct met_smi_conf conf, const u32 i)
{
	return SMI_MET_MASK(conf.master, 0xf, 12) |
		SMI_MET_MASK(para, 0x1, 24) |
		SMI_MET_MASK(conf.reqtype, 0x3, 0) |
		SMI_MET_MASK(conf.rwtype[i], 0x3, 4) |
		SMI_MET_MASK(conf.port[i], 0x3f, 6);
}

static void
smi_met_ipi(const bool ena, const u32 para, const struct met_smi_conf conf)
{
	struct qos_ipi_data qos_ipi_d;
	s32 i;

	qos_ipi_d.cmd = QOS_IPI_SMI_MET_MON;
	qos_ipi_d.u.smi_met_mon.ena = ena;
	memset(qos_ipi_d.u.smi_met_mon.enc, ~0,
		sizeof(qos_ipi_d.u.smi_met_mon.enc));

	for (i = 0; i < parall_port_num[conf.master] && ena; i++)
		qos_ipi_d.u.smi_met_mon.enc[i] = smi_met_enc(para, conf, i);
	qos_ipi_to_sspm_command(&qos_ipi_d, 6);
}
#endif

int SMI_MET_config(struct met_smi_conf *met_smi_config, unsigned int conf_num,
		   unsigned int parallel_mode)
{
	int index, larbno;
	unsigned int master;
	struct met_smi_conf *smi_cfg;
	unsigned int j;

	if (already_configured == 0) {
		if (SMI_MET_SMI_base_Init()) {
			pr_notice("Fail to init SMI\n");
			return MET_SMI_FAIL;
		}
		already_configured = 1;
	}

	for (index = 0; index < conf_num; index++) {
		/* which master is needed to be monitored */
		smi_cfg = &met_smi_config[index];
		master = smi_cfg->master;
		if (master > MET_SMI_LARB_NUM) {
			met_smi_debug("SMI_MET_config FAIL: master overflow\n");
			return MET_SMI_FAIL;
		}
		larb_already_configed[master] = 1;

		/* @parallel mode, how many ports are needed to be monitored */
		if (parallel_mode == 1) {
			for (j = 0; j < SMI_MET_MAX_PORT_NUM; j++) {
				if (smi_cfg->port[j] != -1)
					parall_port_num[master] = j + 1;
			}
		} else
			parall_port_num[master] = 1;

		met_smi_conf_array[master].master = smi_cfg->master;
		met_smi_conf_array[master].port[0] = smi_cfg->port[0];
		met_smi_conf_array[master].port[1] = smi_cfg->port[1];
		met_smi_conf_array[master].port[2] = smi_cfg->port[2];
		met_smi_conf_array[master].port[3] = smi_cfg->port[3];
		met_smi_conf_array[master].rwtype[0] = smi_cfg->rwtype[0];
		met_smi_conf_array[master].rwtype[1] = smi_cfg->rwtype[1];
		met_smi_conf_array[master].rwtype[2] = smi_cfg->rwtype[2];
		met_smi_conf_array[master].rwtype[3] = smi_cfg->rwtype[3];
		met_smi_conf_array[master].desttype = smi_cfg->desttype;
		met_smi_conf_array[master].reqtype = smi_cfg->reqtype;
#ifdef SMI_MET_IPI
		smi_met_ipi(true, parallel_mode, met_smi_conf_array[master]);
#endif
	}


	for (larbno = 0; larbno < MET_SMI_LARB_NUM; larbno++) {
		smi_bus_prepare_enable(larbno, "MET_SMI", true);
		snprintf(debug_msg, SMI_MET_DEBUGBUF_SIZE,
			"[MET_SMI] larb %d poweron\n", larbno);
		met_smi_debug(debug_msg);
	}
#ifndef SMI_MET_IPI
	for (index = 0; index < conf_num; index++) {
		static void __iomem *addr;
		unsigned int u4RegVal;

		smi_cfg = &met_smi_config[index];
		if (smi_cfg->master != MET_SMI_LARB_NUM) { /* larb config */
			addr = SMILarbBaseAddr[smi_cfg->master];
			/* stop counting */
			u4RegVal = smi_readl(SMI_MET_LARB_MON_ENA(addr));
			smi_reg_sync_writel(u4RegVal & ~0x1,
				SMI_MET_LARB_MON_ENA(addr));

			/* config mode & reqtype & rwtype & desttype */
			u4RegVal = (parallel_mode << 10)
				| (smi_cfg->reqtype << 4)
				| (smi_cfg->rwtype[0] << 2)
				| (smi_cfg->desttype << 0);
			smi_reg_sync_writel(u4RegVal,
				SMI_MET_LARB_MON_CON(addr));

			/* config port */
			j = 0;
			do {
				u4RegVal = smi_readl(
					SMI_MET_LARB_MON_PORT(addr));
				u4RegVal &= ~(0x3f << (j * 8));
				u4RegVal |= (smi_cfg->port[j] << (j * 8));
				smi_reg_sync_writel(u4RegVal,
					SMI_MET_LARB_MON_PORT(addr));
				j += 1;
			} while (parallel_mode &&
				j < parall_port_num[smi_cfg->master]);
		} else { /* common config */
			addr = SMICommBaseAddr[0];
			/* stop counting */
			u4RegVal = smi_readl(SMI_MET_COMM_MON_ENA(addr));
			smi_reg_sync_writel(u4RegVal & ~0x1,
				SMI_MET_COMM_MON_ENA(addr));

			/* config mode */
			u4RegVal = smi_readl(SMI_MET_COMM_MON_ENA(addr));
			smi_reg_sync_writel(u4RegVal | (parallel_mode << 1),
				SMI_MET_COMM_MON_ENA(addr));

			/* config reqtype */
			u4RegVal = smi_readl(SMI_MET_COMM_MON_TYPE(addr));
			u4RegVal &= ~(0x3 << 4);
			u4RegVal |= (smi_cfg->reqtype << 4);
			smi_reg_sync_writel(u4RegVal,
				SMI_MET_COMM_MON_TYPE(addr));

			j = 0;
			do {
				/* config rwtype */
				u4RegVal = smi_readl(
					SMI_MET_COMM_MON_TYPE(addr));
				u4RegVal &= ~(0x1 << j);
				if (smi_cfg->rwtype[j] == 0x2)
					u4RegVal |= (0x1 << j);
				smi_reg_sync_writel(u4RegVal,
					SMI_MET_COMM_MON_TYPE(addr));
				/* config port */
				u4RegVal = smi_readl(
					SMI_MET_COMM_MON_CON(addr));
				u4RegVal &= ~(0xf << ((j + 1) * 4));
				u4RegVal |= (smi_cfg->port[j]) << ((j + 1) * 4);
				smi_reg_sync_writel(u4RegVal,
					SMI_MET_COMM_MON_CON(addr));
				j += 1;
			} while (parallel_mode &&
				j < parall_port_num[smi_cfg->master]);
		}
	}
#endif
	return MET_SMI_SUCCESS;
}
EXPORT_SYMBOL(SMI_MET_config);

void SMI_MET_start(void)
{
#ifndef SMI_MET_IPI
	SMI_MET_clear_monitor();
	SMI_MET_monitor(true); /* start monitor counting */
#endif
}
EXPORT_SYMBOL(SMI_MET_start);

static void smi_init_value(void)
{
	int i;

	smi_parallel_mode = 0;
	for (i = 0; i < SMI_MET_TOTAL_MASTER_NUM; i++) {
		parall_port_num[i] = 0;
		larb_already_configed[i] = 0;
		met_smi_conf_array[i].master = 0;
		met_smi_conf_array[i].port[0] = -1;
		met_smi_conf_array[i].port[1] = -1;
		met_smi_conf_array[i].port[2] = -1;
		met_smi_conf_array[i].port[3] = -1;
		met_smi_conf_array[i].rwtype[0] = SMI_RW_ALL;
		met_smi_conf_array[i].rwtype[1] = SMI_RW_ALL;
		met_smi_conf_array[i].rwtype[2] = SMI_RW_ALL;
		met_smi_conf_array[i].rwtype[3] = SMI_RW_ALL;
		met_smi_conf_array[i].desttype = SMI_DEST_EMI;
		met_smi_conf_array[i].reqtype = SMI_REQ_ALL;
	}
}

void SMI_MET_stop(void)
{
	int larbno;

	/* reset configurations */
	smi_init_value(); /* clear config array */
#ifdef SMI_MET_IPI
	smi_met_ipi(false, 0, met_smi_conf_array[0]);
#else
	SMI_MET_clear_monitor(); /* clear monitor */
#endif
	for (larbno = 0; larbno < MET_SMI_LARB_NUM; larbno++) {
		smi_bus_disable_unprepare(larbno, "MET_SMI", true);
		snprintf(debug_msg, SMI_MET_DEBUGBUF_SIZE,
			"[MET_SMI] larb %d poweroff\n", larbno);
		met_smi_debug(debug_msg);
	}
}
EXPORT_SYMBOL(SMI_MET_stop);

#define NARRAY		(SMI_MET_TOTAL_MASTER_NUM * 10 + 2)
#define metadata	(0x5555aaaa)
void SMI_MET_polling(void)
{
#ifndef SMI_MET_IPI
	int larbno;
	unsigned int smi_value[NARRAY];
	int j, cnt = 0;

	smi_value[cnt++] = 0;
	smi_value[cnt++] = 0; /* deprecated index and set 0 */

	SMI_MET_monitor(false); /* stop monitor counting */
	/* get counters */
	for (larbno = 0; larbno <= MET_SMI_LARB_NUM; larbno++) {
		bool pm = smi_parallel_mode, lt = (larbno < MET_SMI_LARB_NUM);

		if (larb_already_configed[larbno] != 1)
			continue;
		j = 0;
		do {
			smi_value[cnt++] = (smi_parallel_mode << SMI_MET_BIT_PM)
				| (met_smi_conf_array[larbno].master
					<< SMI_MET_BIT_MASTER)
				| (met_smi_conf_array[larbno].port[j]
					<< SMI_MET_BIT_PORT)
				| ((pm & lt) ? 0x0 : met_smi_conf_array[larbno]
					.rwtype[j]) << SMI_MET_BIT_RW
				| (!lt ? 0x1 : (!pm ? met_smi_conf_array[larbno]
					.desttype : 0x0)) << SMI_MET_BIT_DST
				| ((pm & lt) ? 0x0 : met_smi_conf_array[larbno]
					.reqtype) << SMI_MET_BIT_REQ;
			if (smi_parallel_mode) { /* parallel mode */
				switch (j) {
				case 0:
					smi_value[cnt++] = (lt ?
						MET_SMI_GetActiveCnt(larbno) :
						MET_SMI_Comm_GetByteCnt(0));
					break;
				case 1:
					smi_value[cnt++] = (lt ?
						MET_SMI_GetRequestCnt(larbno) :
						MET_SMI_Comm_GetActiveCnt(0));
					break;
				case 2:
					smi_value[cnt++] = (lt ?
						MET_SMI_GetBeatCnt(larbno) :
						MET_SMI_Comm_GetRequestCnt(0));
					break;
				case 3:
					smi_value[cnt++] = (lt ?
						MET_SMI_GetByteCnt(larbno) :
						MET_SMI_Comm_GetBeatCnt(0));
					break;
				default:
					smi_value[cnt++] = 0x0;
					break;
				}
			} else { /* legacy mode */
				smi_value[cnt++] = (lt ?
					MET_SMI_GetActiveCnt(larbno) :
					MET_SMI_Comm_GetActiveCnt(0));
				smi_value[cnt++] = (lt ?
					MET_SMI_GetRequestCnt(larbno) :
					MET_SMI_Comm_GetRequestCnt(0));
				smi_value[cnt++] = (lt ?
					MET_SMI_GetBeatCnt(larbno) :
					MET_SMI_Comm_GetBeatCnt(0));
				smi_value[cnt++] = (lt ?
					MET_SMI_GetByteCnt(larbno) :
					MET_SMI_Comm_GetByteCnt(0));
				smi_value[cnt++] =  (lt ?
					MET_SMI_GetCPCnt(larbno) :
					MET_SMI_Comm_GetCPCnt(0));
				smi_value[cnt++] = (lt ?
					MET_SMI_GetDPCnt(larbno) :
					MET_SMI_Comm_GetDPCnt(0));
				smi_value[cnt++] = (lt ?
					MET_SMI_GetOSTDCnt(larbno) :
					MET_SMI_Comm_GetOSTDCnt(0));
				smi_value[cnt++] = (lt ?
					MET_SMI_GetCP_MAX(larbno) :
					MET_SMI_Comm_GetCP_MAX(0));
				smi_value[cnt++] = (lt ?
					MET_SMI_GetOSTD_MAX(larbno) :
					MET_SMI_Comm_GetOSTD_MAX(0));
			}
			j += 1;
		} while (smi_parallel_mode && j < parall_port_num[larbno]);
	}
	SMI_MET_clear_monitor();
	SMI_MET_monitor(true); /* restart monitor counting */
	ms_smi(cnt, smi_value); /* output log to ftrace buffer */
#endif
}
EXPORT_SYMBOL(SMI_MET_polling);
