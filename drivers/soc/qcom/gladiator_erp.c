/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <soc/qcom/scm.h>
#include <linux/of.h>
#include <linux/clk.h>

#define MODULE_NAME "gladiator_error_reporting"

#define INVALID_NUM	0xDEADBEEF

struct reg_off {
	unsigned int gladiator_id_coreid;
	unsigned int gladiator_id_revisionid;
	unsigned int gladiator_faulten;
	unsigned int gladiator_errvld;
	unsigned int gladiator_errclr;
	unsigned int gladiator_errlog0;
	unsigned int gladiator_errlog1;
	unsigned int gladiator_errlog2;
	unsigned int gladiator_errlog3;
	unsigned int gladiator_errlog4;
	unsigned int gladiator_errlog5;
	unsigned int gladiator_errlog6;
	unsigned int gladiator_errlog7;
	unsigned int gladiator_errlog8;
	unsigned int observer_0_id_coreid;
	unsigned int observer_0_id_revisionid;
	unsigned int observer_0_faulten;
	unsigned int observer_0_errvld;
	unsigned int observer_0_errclr;
	unsigned int observer_0_errlog0;
	unsigned int observer_0_errlog1;
	unsigned int observer_0_errlog2;
	unsigned int observer_0_errlog3;
	unsigned int observer_0_errlog4;
	unsigned int observer_0_errlog5;
	unsigned int observer_0_errlog6;
	unsigned int observer_0_errlog7;
	unsigned int observer_0_errlog8;
	unsigned int observer_0_stallen;
};

struct reg_masks_shift {
	unsigned int gld_trans_opcode_mask;
	unsigned int gld_trans_opcode_shift;
	unsigned int gld_error_type_mask;
	unsigned int gld_error_type_shift;
	unsigned int gld_len1_mask;
	unsigned int gld_len1_shift;
	unsigned int gld_trans_sourceid_mask;
	unsigned int gld_trans_sourceid_shift;
	unsigned int gld_trans_targetid_mask;
	unsigned int gld_trans_targetid_shift;
	unsigned int gld_errlog_error;
	unsigned int gld_errlog5_error_type_mask;
	unsigned int gld_errlog5_error_type_shift;
	unsigned int gld_ace_port_parity_mask;
	unsigned int gld_ace_port_parity_shift;
	unsigned int gld_ace_port_disconnect_mask;
	unsigned int gld_ace_port_disconnect_shift;
	unsigned int gld_ace_port_directory_mask;
	unsigned int gld_ace_port_directory_shift;
	unsigned int gld_index_parity_mask;
	unsigned int gld_index_parity_shift;
	unsigned int obs_trans_opcode_mask;
	unsigned int obs_trans_opcode_shift;
	unsigned int obs_error_type_mask;
	unsigned int obs_error_type_shift;
	unsigned int obs_len1_mask;
	unsigned int obs_len1_shift;
};

struct msm_gladiator_data {
	void __iomem *gladiator_virt_base;
	int erp_irq;
	struct notifier_block pm_notifier_block;
	struct clk *qdss_clk;
	struct reg_off *reg_offs;
	struct reg_masks_shift *reg_masks_shifts;
	bool glad_v2;
	bool glad_v3;
};

static int enable_panic_on_error;
module_param(enable_panic_on_error, int, 0000);

enum gld_trans_opcode {
	GLD_RD,
	GLD_RDX,
	GLD_RDL,
	GLD_RESERVED,
	GLD_WR,
	GLD_WRC,
	GLD_PRE,
};

enum obs_trans_opcode {
	OBS_RD,
	OBS_RDW,
	OBS_RDL,
	OBS_RDX,
	OBS_WR,
	OBS_WRW,
	OBS_WRC,
	OBS_RESERVED,
	OBS_PRE,
	OBS_URG,
};

enum obs_err_code {
	OBS_SLV,
	OBS_DEC,
	OBS_UNS,
	OBS_DISC,
	OBS_SEC,
	OBS_HIDE,
	OBS_TMO,
	OBS_RSV,
};

enum err_log {
	ID_COREID,
	ID_REVISIONID,
	FAULTEN,
	ERRVLD,
	ERRCLR,
	ERR_LOG0,
	ERR_LOG1,
	ERR_LOG2,
	ERR_LOG3,
	ERR_LOG4,
	ERR_LOG5,
	ERR_LOG6,
	ERR_LOG7,
	ERR_LOG8,
	STALLEN,
	MAX_NUM,
};

enum type_logger_error {
	DATA_TRANSFER_ERROR,
	DVM_ERROR,
	TX_ERROR,
	TXR_ERROR,
	DISCONNECT_ERROR,
	DIRECTORY_ERROR,
	PARITY_ERROR,
};

static void clear_gladiator_error(void __iomem *gladiator_virt_base,
				struct reg_off *offs)
{
	writel_relaxed(1, gladiator_virt_base + offs->gladiator_errclr);
	writel_relaxed(1, gladiator_virt_base + offs->observer_0_errclr);
}

static inline void print_gld_transaction(unsigned int opc)
{
	switch (opc) {
	case GLD_RD:
		pr_alert("Transaction type: READ\n");
		break;
	case GLD_RDX:
		pr_alert("Transaction type: EXCLUSIVE READ\n");
		break;
	case GLD_RDL:
		pr_alert("Transaction type: LINKED READ\n");
		break;
	case GLD_WR:
		pr_alert("Transaction type: WRITE\n");
		break;
	case GLD_WRC:
		pr_alert("Transaction type: CONDITIONAL WRITE\n");
		break;
	case GLD_PRE:
		pr_alert("Transaction: Preamble packet of linked sequence\n");
		break;
	default:
		pr_alert("Transaction type: Unknown; value:%u\n", opc);
	}
}

static inline void print_gld_errtype(unsigned int errtype)
{
	if (errtype == 0)
		pr_alert("Error type: Snoop data transfer\n");
	else if (errtype == 1)
		pr_alert("Error type: DVM error\n");
	else if (errtype == 3)
		pr_alert("Error type: Disconnect, directory, or parity error\n");
	else
		pr_alert("Error type: Unknown; value:%u\n", errtype);
}

static void decode_gld_errlog0(u32 err_reg,
			struct reg_masks_shift *mask_shifts)
{
	unsigned int opc, errtype, len1;

	opc = (err_reg & mask_shifts->gld_trans_opcode_mask) >>
					mask_shifts->gld_trans_opcode_shift;
	errtype = (err_reg & mask_shifts->gld_error_type_mask) >>
					mask_shifts->gld_error_type_shift;
	len1 = (err_reg & mask_shifts->gld_len1_mask) >>
					mask_shifts->gld_len1_shift;

	print_gld_transaction(opc);
	print_gld_errtype(errtype);
	pr_alert("number of payload bytes: %d\n", len1 + 1);
}

static void decode_gld_errlog1(u32 err_reg,
			struct reg_masks_shift *mask_shifts)
{
	if ((err_reg & mask_shifts->gld_errlog_error) ==
					mask_shifts->gld_errlog_error)
		pr_alert("Transaction issued on IO target generic interface\n");
	else
		pr_alert("Transaction source ID: %d\n",
				(err_reg & mask_shifts->gld_trans_sourceid_mask)
				>> mask_shifts->gld_trans_sourceid_shift);
}

static void decode_gld_errlog2(u32 err_reg,
			struct reg_masks_shift *mask_shifts)
{
	if ((err_reg & mask_shifts->gld_errlog_error) ==
					mask_shifts->gld_errlog_error)
		pr_alert("Error response coming from: external DVM network\n");
	else
		pr_alert("Error response coming from: Target ID: %d\n",
				(err_reg & mask_shifts->gld_trans_targetid_mask)
				>> mask_shifts->gld_trans_targetid_shift);
}

static void decode_ace_port_index(u32 type, u32 error,
			struct reg_masks_shift *mask_shifts)
{
	unsigned int port;

	switch (type) {
	case DISCONNECT_ERROR:
		port = (error & mask_shifts->gld_ace_port_disconnect_mask)
			>> mask_shifts->gld_ace_port_disconnect_shift;
		pr_alert("ACE port index: %d\n", port);
		break;
	case DIRECTORY_ERROR:
		port = (error & mask_shifts->gld_ace_port_directory_mask)
			>> mask_shifts->gld_ace_port_directory_shift;
		pr_alert("ACE port index: %d\n", port);
		break;
	case PARITY_ERROR:
		port = (error & mask_shifts->gld_ace_port_parity_mask)
			>> mask_shifts->gld_ace_port_parity_shift;
		pr_alert("ACE port index: %d\n", port);
	}
}

static void decode_index_parity(u32 error, struct reg_masks_shift *mask_shifts)
{
	pr_alert("Index: %d\n",
			(error & mask_shifts->gld_index_parity_mask)
			>> mask_shifts->gld_index_parity_shift);
}

static void decode_gld_logged_error(u32 err_reg5,
				struct reg_masks_shift *mask_shifts)
{
	unsigned int log_err_type, i, value;

	log_err_type = (err_reg5 & mask_shifts->gld_errlog5_error_type_mask)
		>> mask_shifts->gld_errlog5_error_type_shift;
	for (i = 0 ; i <= 6 ; i++) {
		value = log_err_type & 0x1;
		switch (i) {
		case DATA_TRANSFER_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: Data transfer error\n");
			break;
		case DVM_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: DVM error\n");
			break;
		case TX_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: Tx error\n");
			break;
		case TXR_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: TxR error\n");
			break;
		case DISCONNECT_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: Disconnect error\n");
			decode_ace_port_index(
					DISCONNECT_ERROR,
					err_reg5,
					mask_shifts);
			break;
		case DIRECTORY_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: Directory error\n");
			decode_ace_port_index(
					DIRECTORY_ERROR,
					err_reg5,
					mask_shifts);
			break;
		case PARITY_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: Parity error\n");
			decode_ace_port_index(PARITY_ERROR, err_reg5,
					mask_shifts);
			decode_index_parity(err_reg5, mask_shifts);
			break;
		}
		log_err_type = log_err_type >> 1;
	}
}

static void decode_gld_errlog(u32 err_reg, unsigned int err_log,
				struct msm_gladiator_data *msm_gld_data)
{
	switch (err_log) {
	case ERR_LOG0:
		decode_gld_errlog0(err_reg, msm_gld_data->reg_masks_shifts);
		break;
	case ERR_LOG1:
		decode_gld_errlog1(err_reg, msm_gld_data->reg_masks_shifts);
		break;
	case ERR_LOG2:
		decode_gld_errlog2(err_reg, msm_gld_data->reg_masks_shifts);
		break;
	case ERR_LOG3:
		pr_alert("Lower 32-bits of error address: %08x\n", err_reg);
		break;
	case ERR_LOG4:
		pr_alert("Upper 32-bits of error address: %08x\n", err_reg);
		break;
	case ERR_LOG5:
		pr_alert("Lower 32-bits of user: %08x\n", err_reg);
		break;
	case ERR_LOG6:
		pr_alert("Mid 32-bits(63-32) of user: %08x\n", err_reg);
		break;
	case ERR_LOG7:
		break;
	case ERR_LOG8:
		pr_alert("Upper 32-bits(95-64) of user: %08x\n", err_reg);
		break;
	default:
		pr_alert("Invalid error register; reg num:%u\n", err_log);
	}
}

static inline void print_obs_transaction(unsigned int opc)
{
	switch (opc) {
	case OBS_RD:
		pr_alert("Transaction type: READ\n");
		break;
	case OBS_RDW:
		pr_alert("Transaction type: WRAPPED READ\n");
		break;
	case OBS_RDL:
		pr_alert("Transaction type: LINKED READ\n");
		break;
	case OBS_RDX:
		pr_alert("Transaction type: EXCLUSIVE READ\n");
		break;
	case OBS_WR:
		pr_alert("Transaction type: WRITE\n");
		break;
	case OBS_WRW:
		pr_alert("Transaction type: WRAPPED WRITE\n");
		break;
	case OBS_WRC:
		pr_alert("Transaction type: CONDITIONAL WRITE\n");
		break;
	case OBS_PRE:
		pr_alert("Transaction: Preamble packet of linked sequence\n");
		break;
	case OBS_URG:
		pr_alert("Transaction type: Urgency Packet\n");
		break;
	default:
		pr_alert("Transaction type: Unknown; value:%u\n", opc);
	}
}

static inline void print_obs_errcode(unsigned int errcode)
{
	switch (errcode) {
	case OBS_SLV:
		pr_alert("Error code: Target error detected by slave\n");
		pr_alert("Source: Target\n");
		break;
	case OBS_DEC:
		pr_alert("Error code: Address decode error\n");
		pr_alert("Source: Initiator NIU\n");
		break;
	case OBS_UNS:
		pr_alert("Error code: Unsupported request\n");
		pr_alert("Source: Target NIU\n");
		break;
	case OBS_DISC:
		pr_alert("Error code: Disconnected target or domain\n");
		pr_alert("Source: Power Disconnect\n");
		break;
	case OBS_SEC:
		pr_alert("Error code: Security violation\n");
		pr_alert("Source: Initiator NIU or Firewall\n");
		break;
	case OBS_HIDE:
		pr_alert("Error :Hidden security violation, reported as OK\n");
		pr_alert("Source: Firewall\n");
		break;
	case OBS_TMO:
		pr_alert("Error code: Time-out\n");
		pr_alert("Source: Target NIU\n");
		break;
	default:
		pr_alert("Error code: Unknown; code:%u\n", errcode);
	}
}

static void decode_obs_errlog0(u32 err_reg,
			struct reg_masks_shift *mask_shifts)
{
	unsigned int opc, errcode;

	opc = (err_reg & mask_shifts->obs_trans_opcode_mask) >>
				mask_shifts->obs_trans_opcode_shift;
	errcode = (err_reg & mask_shifts->obs_error_type_mask) >>
				mask_shifts->obs_error_type_shift;

	print_obs_transaction(opc);
	print_obs_errcode(errcode);
}

static void decode_obs_errlog0_len(u32 err_reg,
				struct reg_masks_shift *mask_shifts)
{
	unsigned int len1;

	len1 = (err_reg & mask_shifts->obs_len1_mask) >>
				mask_shifts->obs_len1_shift;
	pr_alert("number of payload bytes: %d\n", len1 + 1);
}

static void decode_obs_errlog(u32 err_reg, unsigned int err_log,
		struct msm_gladiator_data *msm_gld_data)
{
	switch (err_log) {
	case ERR_LOG0:
		decode_obs_errlog0(err_reg, msm_gld_data->reg_masks_shifts);
		decode_obs_errlog0_len(err_reg, msm_gld_data->reg_masks_shifts);
		break;
	case ERR_LOG1:
		pr_alert("RouteId of the error: %08x\n", err_reg);
		break;
	case ERR_LOG2:
		/* reserved error log register */
		break;
	case ERR_LOG3:
		pr_alert("Lower 32-bits of error address: %08x\n", err_reg);
		break;
	case ERR_LOG4:
		pr_alert("Upper 12-bits of error address: %08x\n", err_reg);
		break;
	case ERR_LOG5:
		pr_alert("Lower 13-bits of user: %08x\n", err_reg);
		break;
	case ERR_LOG6:
		/* reserved error log register */
		break;
	case ERR_LOG7:
		pr_alert("Security filed of the logged error: %08x\n", err_reg);
		break;
	case ERR_LOG8:
		/* reserved error log register */
		break;
	case STALLEN:
		pr_alert("stall mode of the error logger: %08x\n",
				err_reg & 0x1);
		break;
	default:
		pr_alert("Invalid error register; reg num:%u\n", err_log);
	}
}

static void decode_obs_errlog_v3(u32 err_reg, unsigned int err_log,
		struct msm_gladiator_data *msm_gld_data)
{
	switch (err_log) {
	case ERR_LOG0:
		decode_obs_errlog0(err_reg, msm_gld_data->reg_masks_shifts);
		break;
	case ERR_LOG1:
		decode_obs_errlog0_len(err_reg, msm_gld_data->reg_masks_shifts);
		break;
	case ERR_LOG2:
		pr_alert("Path of the error: %08x\n", err_reg);
		break;
	case ERR_LOG3:
		pr_alert("ExtID of the error: %08x\n", err_reg);
		break;
	case ERR_LOG4:
		pr_alert("ERRLOG2_LSB: %08x\n", err_reg);
		break;
	case ERR_LOG5:
		pr_alert("ERRLOG2_MSB: %08x\n", err_reg);
		break;
	case ERR_LOG6:
		pr_alert("ERRLOG3_LSB: %08x\n", err_reg);
		break;
	case ERR_LOG7:
		pr_alert("ERRLOG3_MSB: %08x\n", err_reg);
		break;
	case FAULTEN:
		pr_alert("stall mode of the error logger: %08x\n",
				err_reg & 0x3);
		break;
	default:
		pr_alert("Invalid error register; reg num:%u\n", err_log);
	}
}

static u32 get_gld_offset(unsigned int err_log, struct reg_off *offs)
{
	u32 offset = 0;

	switch (err_log) {
	case FAULTEN:
		offset = offs->gladiator_faulten;
		break;
	case ERRVLD:
		offset = offs->gladiator_errvld;
		break;
	case ERRCLR:
		offset = offs->gladiator_errclr;
		break;
	case ERR_LOG0:
		offset = offs->gladiator_errlog0;
		break;
	case ERR_LOG1:
		offset = offs->gladiator_errlog1;
		break;
	case ERR_LOG2:
		offset = offs->gladiator_errlog2;
		break;
	case ERR_LOG3:
		offset = offs->gladiator_errlog3;
		break;
	case ERR_LOG4:
		offset = offs->gladiator_errlog4;
		break;
	case ERR_LOG5:
		offset = offs->gladiator_errlog5;
		break;
	case ERR_LOG6:
		offset = offs->gladiator_errlog6;
		break;
	case ERR_LOG7:
		offset = offs->gladiator_errlog7;
		break;
	case ERR_LOG8:
		offset = offs->gladiator_errlog8;
		break;
	default:
		pr_alert("Invalid gladiator error register; reg num:%u\n",
				err_log);
	}
	return offset;
}

static u32 get_obs_offset(unsigned int err_log, struct reg_off *offs)
{
	u32 offset = 0;

	switch (err_log) {
	case FAULTEN:
		offset = offs->observer_0_faulten;
		break;
	case ERRVLD:
		offset = offs->observer_0_errvld;
		break;
	case ERRCLR:
		offset = offs->observer_0_errclr;
		break;
	case ERR_LOG0:
		offset = offs->observer_0_errlog0;
		break;
	case ERR_LOG1:
		offset = offs->observer_0_errlog1;
		break;
	case ERR_LOG2:
		offset = offs->observer_0_errlog2;
		break;
	case ERR_LOG3:
		offset = offs->observer_0_errlog3;
		break;
	case ERR_LOG4:
		offset = offs->observer_0_errlog4;
		break;
	case ERR_LOG5:
		offset = offs->observer_0_errlog5;
		break;
	case ERR_LOG6:
		offset = offs->observer_0_errlog6;
		break;
	case ERR_LOG7:
		offset = offs->observer_0_errlog7;
		break;
	case ERR_LOG8:
		offset = offs->observer_0_errlog8;
		break;
	case STALLEN:
		offset = offs->observer_0_stallen;
		break;
	default:
		pr_alert("Invalid observer error register; reg num:%u\n",
				err_log);
	}
	return offset;
}

static void decode_gld_errlog5(struct msm_gladiator_data *msm_gld_data)
{
	unsigned int errtype;
	u32 err_reg0, err_reg5;
	struct reg_masks_shift *mask_shifts = msm_gld_data->reg_masks_shifts;

	err_reg0 = readl_relaxed(msm_gld_data->gladiator_virt_base +
			get_gld_offset(ERR_LOG0, msm_gld_data->reg_offs));
	err_reg5 = readl_relaxed(msm_gld_data->gladiator_virt_base +
			get_gld_offset(ERR_LOG5, msm_gld_data->reg_offs));

	errtype = (err_reg0 & mask_shifts->gld_error_type_mask) >>
			mask_shifts->gld_error_type_shift;
	if (errtype == 3)
		decode_gld_logged_error(err_reg5, mask_shifts);
	else if (errtype == 0 || errtype == 1)
		pr_alert("Lower 32-bits of user: %08x\n", err_reg5);
	else
		pr_alert("Error type: Unknown; value:%u\n", errtype);
}

static void dump_gld_err_regs(struct msm_gladiator_data *msm_gld_data,
			unsigned int err_buf[MAX_NUM])
{
	unsigned int err_log;
	unsigned int start = FAULTEN;
	unsigned int end = ERR_LOG8;

	if (msm_gld_data->glad_v2 || msm_gld_data->glad_v3) {
		start = FAULTEN;
		end = ERR_LOG8;
	}

	pr_alert("Main log register data:\n");
	for (err_log = start; err_log <= end; err_log++) {
		err_buf[err_log] = readl_relaxed(
				msm_gld_data->gladiator_virt_base +
				get_gld_offset(err_log,
					msm_gld_data->reg_offs));
		pr_alert("%08x ", err_buf[err_log]);
	}
}

static void dump_obsrv_err_regs(struct msm_gladiator_data *msm_gld_data,
			unsigned int err_buf[MAX_NUM])
{
	unsigned int err_log;
	unsigned int start = ID_COREID;
	unsigned int end = STALLEN;

	if (msm_gld_data->glad_v2) {
		start = ID_COREID;
		end = STALLEN;
	} else if (msm_gld_data->glad_v3) {
		start = FAULTEN;
		end = ERR_LOG7;
	}

	pr_alert("Observer log register data:\n");
	for (err_log = start; err_log <= end; err_log++) {
		err_buf[err_log] = readl_relaxed(
				msm_gld_data->gladiator_virt_base +
				get_obs_offset(
					err_log,
					msm_gld_data->reg_offs)
				);
		pr_alert("%08x ", err_buf[err_log]);
	}
}

static void parse_gld_err_regs(struct msm_gladiator_data *msm_gld_data,
			unsigned int err_buf[MAX_NUM])
{
	unsigned int err_log;

	pr_alert("Main error log register data:\n");
	for (err_log = ERR_LOG0; err_log <= ERR_LOG8; err_log++) {
		/* skip log register 7 as its reserved */
		if (err_log == ERR_LOG7)
			continue;
		if (err_log == ERR_LOG5) {
			decode_gld_errlog5(msm_gld_data);
			continue;
		}
		decode_gld_errlog(err_buf[err_log], err_log,
				msm_gld_data);
	}
}

static void parse_obsrv_err_regs(struct msm_gladiator_data *msm_gld_data,
			unsigned int err_buf[MAX_NUM])
{
	unsigned int err_log;

	pr_alert("Observor error log register data:\n");
	if (msm_gld_data->glad_v2) {
		for (err_log = ERR_LOG0; err_log <= STALLEN; err_log++)	{
			/* skip log register 2, 6 and 8 as they are reserved */
			if ((err_log == ERR_LOG2) || (err_log == ERR_LOG6)
					|| (err_log == ERR_LOG8))
				continue;
			decode_obs_errlog(err_buf[err_log], err_log,
					msm_gld_data);
		}
	} else if (msm_gld_data->glad_v3) {
		decode_obs_errlog_v3(err_buf[STALLEN], STALLEN,
					msm_gld_data);
		for (err_log = ERR_LOG0; err_log <= ERR_LOG7; err_log++) {
			decode_obs_errlog_v3(err_buf[err_log], err_log,
					msm_gld_data);
		}
	}

}

static irqreturn_t msm_gladiator_isr(int irq, void *dev_id)
{
	unsigned int gld_err_buf[MAX_NUM], obs_err_buf[MAX_NUM];

	struct msm_gladiator_data *msm_gld_data = dev_id;

	/* Check validity */
	bool gld_err_valid = readl_relaxed(msm_gld_data->gladiator_virt_base +
			msm_gld_data->reg_offs->gladiator_errvld);

	bool obsrv_err_valid = readl_relaxed(
			msm_gld_data->gladiator_virt_base +
			msm_gld_data->reg_offs->observer_0_errvld);

	if (!gld_err_valid && !obsrv_err_valid) {
		pr_err("%s Invalid Gladiator error reported, clear it\n",
				__func__);
		/* Clear IRQ */
		clear_gladiator_error(msm_gld_data->gladiator_virt_base,
					msm_gld_data->reg_offs);
		return IRQ_HANDLED;
	}
	pr_alert("Gladiator Error Detected:\n");
	if (gld_err_valid)
		dump_gld_err_regs(msm_gld_data, gld_err_buf);

	if (obsrv_err_valid)
		dump_obsrv_err_regs(msm_gld_data, obs_err_buf);

	if (gld_err_valid)
		parse_gld_err_regs(msm_gld_data, gld_err_buf);

	if (obsrv_err_valid)
		parse_obsrv_err_regs(msm_gld_data, obs_err_buf);

	/* Clear IRQ */
	clear_gladiator_error(msm_gld_data->gladiator_virt_base,
				msm_gld_data->reg_offs);
	if (enable_panic_on_error)
		panic("Gladiator Cache Interconnect Error Detected!\n");
	else
		WARN(1, "Gladiator Cache Interconnect Error Detected\n");

	return IRQ_HANDLED;
}

static const struct of_device_id gladiator_erp_match_table[] = {
	{ .compatible = "qcom,msm-gladiator-v2" },
	{ .compatible = "qcom,msm-gladiator-v3" },
	{},
};

static int parse_dt_node(struct platform_device *pdev,
		struct msm_gladiator_data *msm_gld_data)
{
	int ret = 0;
	struct resource *res;

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "gladiator_base");
	if (!res)
		return -ENODEV;
	if (!devm_request_mem_region(&pdev->dev, res->start,
				resource_size(res),
				"msm-gladiator-erp")) {

		dev_err(&pdev->dev, "%s cannot reserve gladiator erp region\n",
				__func__);
		return -ENXIO;
	}
	msm_gld_data->gladiator_virt_base  = devm_ioremap(&pdev->dev,
			res->start, resource_size(res));
	if (!msm_gld_data->gladiator_virt_base) {
		dev_err(&pdev->dev, "%s cannot map gladiator register space\n",
				__func__);
		return -ENXIO;
	}
	msm_gld_data->erp_irq = platform_get_irq(pdev, 0);
	if (!msm_gld_data->erp_irq)
		return -ENODEV;

	/* clear existing errors before enabling the interrupt */
	clear_gladiator_error(msm_gld_data->gladiator_virt_base,
			msm_gld_data->reg_offs);
	ret = devm_request_irq(&pdev->dev, msm_gld_data->erp_irq,
			msm_gladiator_isr, IRQF_TRIGGER_HIGH,
			"gladiator-error", msm_gld_data);
	if (ret)
		dev_err(&pdev->dev, "Failed to register irq handler\n");

	return ret;
}

static inline void gladiator_irq_init(void __iomem *gladiator_virt_base,
				struct reg_off *offs)
{
	writel_relaxed(1, gladiator_virt_base + offs->gladiator_faulten);
	writel_relaxed(1, gladiator_virt_base + offs->observer_0_faulten);
}

#define CCI_LEVEL 2
static int gladiator_erp_pm_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	unsigned int level = (unsigned long) data;
	struct msm_gladiator_data *msm_gld_data = container_of(nb,
			struct msm_gladiator_data, pm_notifier_block);

	if (level != CCI_LEVEL)
		return NOTIFY_DONE;

	switch (val) {
	case CPU_CLUSTER_PM_EXIT:
		gladiator_irq_init(msm_gld_data->gladiator_virt_base,
				msm_gld_data->reg_offs);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static void init_offsets_and_masks_v2(struct msm_gladiator_data *msm_gld_data)
{
	msm_gld_data->reg_offs->gladiator_id_coreid		= 0x0;
	msm_gld_data->reg_offs->gladiator_id_revisionid		= 0x4;
	msm_gld_data->reg_offs->gladiator_faulten		= 0x1010;
	msm_gld_data->reg_offs->gladiator_errvld		= 0x1014;
	msm_gld_data->reg_offs->gladiator_errclr		= 0x1018;
	msm_gld_data->reg_offs->gladiator_errlog0		= 0x101C;
	msm_gld_data->reg_offs->gladiator_errlog1		= 0x1020;
	msm_gld_data->reg_offs->gladiator_errlog2		= 0x1024;
	msm_gld_data->reg_offs->gladiator_errlog3		= 0x1028;
	msm_gld_data->reg_offs->gladiator_errlog4		= 0x102C;
	msm_gld_data->reg_offs->gladiator_errlog5		= 0x1030;
	msm_gld_data->reg_offs->gladiator_errlog6		= 0x1034;
	msm_gld_data->reg_offs->gladiator_errlog7		= 0x1038;
	msm_gld_data->reg_offs->gladiator_errlog8		= 0x103C;
	msm_gld_data->reg_offs->observer_0_id_coreid		= 0x8000;
	msm_gld_data->reg_offs->observer_0_id_revisionid	= 0x8004;
	msm_gld_data->reg_offs->observer_0_faulten		= 0x8008;
	msm_gld_data->reg_offs->observer_0_errvld		= 0x800C;
	msm_gld_data->reg_offs->observer_0_errclr		= 0x8010;
	msm_gld_data->reg_offs->observer_0_errlog0		= 0x8014;
	msm_gld_data->reg_offs->observer_0_errlog1		= 0x8018;
	msm_gld_data->reg_offs->observer_0_errlog2		= 0x801C;
	msm_gld_data->reg_offs->observer_0_errlog3		= 0x8020;
	msm_gld_data->reg_offs->observer_0_errlog4		= 0x8024;
	msm_gld_data->reg_offs->observer_0_errlog5		= 0x8028;
	msm_gld_data->reg_offs->observer_0_errlog6		= 0x802C;
	msm_gld_data->reg_offs->observer_0_errlog7		= 0x8030;
	msm_gld_data->reg_offs->observer_0_errlog8		= 0x8034;
	msm_gld_data->reg_offs->observer_0_stallen		= 0x8038;

	msm_gld_data->reg_masks_shifts->gld_trans_opcode_mask = 0xE;
	msm_gld_data->reg_masks_shifts->gld_trans_opcode_shift = 1;
	msm_gld_data->reg_masks_shifts->gld_error_type_mask = 0x700;
	msm_gld_data->reg_masks_shifts->gld_error_type_shift = 8;
	msm_gld_data->reg_masks_shifts->gld_len1_mask = 0xFFF;
	msm_gld_data->reg_masks_shifts->gld_len1_shift = 16;
	msm_gld_data->reg_masks_shifts->gld_trans_sourceid_mask = 0x7;
	msm_gld_data->reg_masks_shifts->gld_trans_sourceid_shift = 0;
	msm_gld_data->reg_masks_shifts->gld_trans_targetid_mask = 0x7;
	msm_gld_data->reg_masks_shifts->gld_trans_targetid_shift = 0;
	msm_gld_data->reg_masks_shifts->gld_errlog_error = 0x7;
	msm_gld_data->reg_masks_shifts->gld_errlog5_error_type_mask =
								0xFF000000;
	msm_gld_data->reg_masks_shifts->gld_errlog5_error_type_shift = 24;
	msm_gld_data->reg_masks_shifts->gld_ace_port_parity_mask = 0xc000;
	msm_gld_data->reg_masks_shifts->gld_ace_port_parity_shift = 14;
	msm_gld_data->reg_masks_shifts->gld_ace_port_disconnect_mask = 0xf0000;
	msm_gld_data->reg_masks_shifts->gld_ace_port_disconnect_shift = 16;
	msm_gld_data->reg_masks_shifts->gld_ace_port_directory_mask = 0xf00000;
	msm_gld_data->reg_masks_shifts->gld_ace_port_directory_shift = 20;
	msm_gld_data->reg_masks_shifts->gld_index_parity_mask = 0x1FFF;
	msm_gld_data->reg_masks_shifts->gld_index_parity_shift = 0;
	msm_gld_data->reg_masks_shifts->obs_trans_opcode_mask = 0x1E;
	msm_gld_data->reg_masks_shifts->obs_trans_opcode_shift = 1;
	msm_gld_data->reg_masks_shifts->obs_error_type_mask = 0x700;
	msm_gld_data->reg_masks_shifts->obs_error_type_shift = 8;
	msm_gld_data->reg_masks_shifts->obs_len1_mask = 0x7F0;
	msm_gld_data->reg_masks_shifts->obs_len1_shift = 16;
}

static void init_offsets_and_masks_v3(struct msm_gladiator_data *msm_gld_data)
{
	msm_gld_data->reg_offs->gladiator_id_coreid	= 0x0;
	msm_gld_data->reg_offs->gladiator_id_revisionid = 0x4;
	msm_gld_data->reg_offs->gladiator_faulten	= 0x1010;
	msm_gld_data->reg_offs->gladiator_errvld	= 0x1014;
	msm_gld_data->reg_offs->gladiator_errclr	= 0x1018;
	msm_gld_data->reg_offs->gladiator_errlog0	= 0x101C;
	msm_gld_data->reg_offs->gladiator_errlog1	= 0x1020;
	msm_gld_data->reg_offs->gladiator_errlog2	= 0x1024;
	msm_gld_data->reg_offs->gladiator_errlog3	= 0x1028;
	msm_gld_data->reg_offs->gladiator_errlog4	= 0x102C;
	msm_gld_data->reg_offs->gladiator_errlog5	= 0x1030;
	msm_gld_data->reg_offs->gladiator_errlog6	= 0x1034;
	msm_gld_data->reg_offs->gladiator_errlog7	= 0x1038;
	msm_gld_data->reg_offs->gladiator_errlog8	= 0x103C;
	msm_gld_data->reg_offs->observer_0_id_coreid	= INVALID_NUM;
	msm_gld_data->reg_offs->observer_0_id_revisionid = INVALID_NUM;
	msm_gld_data->reg_offs->observer_0_faulten	= 0x2008;
	msm_gld_data->reg_offs->observer_0_errvld	= 0x2010;
	msm_gld_data->reg_offs->observer_0_errclr	= 0x2018;
	msm_gld_data->reg_offs->observer_0_errlog0	= 0x2020;
	msm_gld_data->reg_offs->observer_0_errlog1	= 0x2024;
	msm_gld_data->reg_offs->observer_0_errlog2	= 0x2028;
	msm_gld_data->reg_offs->observer_0_errlog3	= 0x202C;
	msm_gld_data->reg_offs->observer_0_errlog4	= 0x2030;
	msm_gld_data->reg_offs->observer_0_errlog5	= 0x2034;
	msm_gld_data->reg_offs->observer_0_errlog6	= 0x2038;
	msm_gld_data->reg_offs->observer_0_errlog7	= 0x203C;
	msm_gld_data->reg_offs->observer_0_errlog8	= INVALID_NUM;
	msm_gld_data->reg_offs->observer_0_stallen	= INVALID_NUM;

	msm_gld_data->reg_masks_shifts->gld_trans_opcode_mask = 0xE;
	msm_gld_data->reg_masks_shifts->gld_trans_opcode_shift = 1;
	msm_gld_data->reg_masks_shifts->gld_error_type_mask = 0x700;
	msm_gld_data->reg_masks_shifts->gld_error_type_shift = 8;
	msm_gld_data->reg_masks_shifts->gld_len1_mask = 0xFFF0000;
	msm_gld_data->reg_masks_shifts->gld_len1_shift = 16;
	msm_gld_data->reg_masks_shifts->gld_trans_sourceid_mask = 0x7;
	msm_gld_data->reg_masks_shifts->gld_trans_sourceid_shift = 0;
	msm_gld_data->reg_masks_shifts->gld_trans_targetid_mask = 0x7;
	msm_gld_data->reg_masks_shifts->gld_trans_targetid_shift = 0;
	msm_gld_data->reg_masks_shifts->gld_errlog_error = 0x7;
	msm_gld_data->reg_masks_shifts->gld_errlog5_error_type_mask =
								0xFF000000;
	msm_gld_data->reg_masks_shifts->gld_errlog5_error_type_shift = 24;
	msm_gld_data->reg_masks_shifts->gld_ace_port_parity_mask = 0xc000;
	msm_gld_data->reg_masks_shifts->gld_ace_port_parity_shift = 14;
	msm_gld_data->reg_masks_shifts->gld_ace_port_disconnect_mask = 0xf0000;
	msm_gld_data->reg_masks_shifts->gld_ace_port_disconnect_shift = 16;
	msm_gld_data->reg_masks_shifts->gld_ace_port_directory_mask = 0xf00000;
	msm_gld_data->reg_masks_shifts->gld_ace_port_directory_shift = 20;
	msm_gld_data->reg_masks_shifts->gld_index_parity_mask = 0x1FFF;
	msm_gld_data->reg_masks_shifts->gld_index_parity_shift = 0;
	msm_gld_data->reg_masks_shifts->obs_trans_opcode_mask = 0x70;
	msm_gld_data->reg_masks_shifts->obs_trans_opcode_shift = 4;
	msm_gld_data->reg_masks_shifts->obs_error_type_mask = 0x700;
	msm_gld_data->reg_masks_shifts->obs_error_type_shift = 8;
	msm_gld_data->reg_masks_shifts->obs_len1_mask = 0x1FF;
	msm_gld_data->reg_masks_shifts->obs_len1_shift = 0;
}

static int gladiator_erp_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct msm_gladiator_data *msm_gld_data;

	msm_gld_data = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_gladiator_data), GFP_KERNEL);
	if (!msm_gld_data) {
		ret = -ENOMEM;
		goto bail;
	}

	msm_gld_data->reg_offs = devm_kzalloc(&pdev->dev,
			sizeof(struct reg_off), GFP_KERNEL);
	msm_gld_data->reg_masks_shifts = devm_kzalloc(&pdev->dev,
			sizeof(struct reg_masks_shift), GFP_KERNEL);

	if (!msm_gld_data->reg_offs || !msm_gld_data->reg_masks_shifts) {
		ret = -ENOMEM;
		goto bail;
	}

	msm_gld_data->glad_v2 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,msm-gladiator-v2");
	msm_gld_data->glad_v3 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,msm-gladiator-v3");

	if (msm_gld_data->glad_v2)
		init_offsets_and_masks_v2(msm_gld_data);
	else if (msm_gld_data->glad_v3)
		init_offsets_and_masks_v3(msm_gld_data);

	if (msm_gld_data->glad_v2) {
		if (of_property_match_string(pdev->dev.of_node,
					"clock-names", "atb_clk") >= 0) {
			msm_gld_data->qdss_clk = devm_clk_get(&pdev->dev,
								"atb_clk");
			if (IS_ERR(msm_gld_data->qdss_clk)) {
				dev_err(&pdev->dev, "Failed to get QDSS ATB clock\n");
				goto bail;
			}
		} else {
			dev_err(&pdev->dev, "No matching string of QDSS ATB clock\n");
			goto bail;
		}

		ret = clk_prepare_enable(msm_gld_data->qdss_clk);
		if (ret)
			goto err_atb_clk;
	}

	ret = parse_dt_node(pdev, msm_gld_data);
	if (ret)
		goto bail;
	msm_gld_data->pm_notifier_block.notifier_call =
		gladiator_erp_pm_callback;

	gladiator_irq_init(msm_gld_data->gladiator_virt_base,
			msm_gld_data->reg_offs);
	platform_set_drvdata(pdev, msm_gld_data);
	cpu_pm_register_notifier(&msm_gld_data->pm_notifier_block);
#ifdef CONFIG_PANIC_ON_GLADIATOR_ERROR
	enable_panic_on_error = 1;
#endif
	dev_info(&pdev->dev, "MSM Gladiator Error Reporting Initialized\n");
	return ret;

err_atb_clk:
	clk_disable_unprepare(msm_gld_data->qdss_clk);

bail:
	dev_err(&pdev->dev, "Probe failed bailing out\n");
	return ret;
}

static int gladiator_erp_remove(struct platform_device *pdev)
{
	struct msm_gladiator_data *msm_gld_data = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	cpu_pm_unregister_notifier(&msm_gld_data->pm_notifier_block);
	clk_disable_unprepare(msm_gld_data->qdss_clk);
	return 0;
}

static struct platform_driver gladiator_erp_driver = {
	.probe = gladiator_erp_probe,
	.remove = gladiator_erp_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = gladiator_erp_match_table,
	},
};

static int __init init_gladiator_erp(void)
{
	int ret;

	ret = scm_is_secure_device();
	if (ret == 0) {
		pr_info("Gladiator Error Reporting not available\n");
		return -ENODEV;
	}

	return platform_driver_register(&gladiator_erp_driver);
}
module_init(init_gladiator_erp);

static void __exit exit_gladiator_erp(void)
{
	return platform_driver_unregister(&gladiator_erp_driver);
}
module_exit(exit_gladiator_erp);

MODULE_DESCRIPTION("Gladiator Error Reporting");
MODULE_LICENSE("GPL v2");
