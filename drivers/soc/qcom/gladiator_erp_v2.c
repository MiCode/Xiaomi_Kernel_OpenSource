/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define MODULE_NAME "gladiator-v2_error_reporting"

/* Register Offsets */
#define GLADIATOR_ID_COREID	0x0
#define GLADIATOR_ID_REVISIONID	0x4
#define GLADIATOR_FAULTEN	0x1010
#define GLADIATOR_ERRVLD	0x1014
#define GLADIATOR_ERRCLR	0x1018
#define GLADIATOR_ERRLOG0	0x101C
#define GLADIATOR_ERRLOG1	0x1020
#define GLADIATOR_ERRLOG2	0x1024
#define GLADIATOR_ERRLOG3	0x1028
#define GLADIATOR_ERRLOG4	0x102C
#define GLADIATOR_ERRLOG5	0x1030
#define GLADIATOR_ERRLOG6	0x1034
#define GLADIATOR_ERRLOG7	0x1038
#define GLADIATOR_ERRLOG8	0x103C
#define OBSERVER_0_ID_COREID	0x8000
#define OBSERVER_0_FAULTEN	0x8008
#define OBSERVER_0_ERRVLD	0x800C
#define OBSERVER_0_ERRCLR	0x8010
#define OBSERVER_0_ERRLOG0	0x8014
#define OBSERVER_0_ERRLOG1	0x8018
#define OBSERVER_0_ERRLOG2	0x801C
#define OBSERVER_0_ERRLOG3	0x8020
#define OBSERVER_0_ERRLOG4	0x8024
#define OBSERVER_0_ERRLOG5	0x8028
#define OBSERVER_0_ERRLOG6	0x802C
#define OBSERVER_0_ERRLOG7	0x8030
#define OBSERVER_0_ERRLOG8	0x8034
#define OBSERVER_0_STALLEN	0x8038
#define OBSERVER_0_REVISIONID	0x8004

#define GLD_TRANS_OPCODE_MASK		0xE
#define GLD_TRANS_OPCODE_SHIFT		1
#define GLD_ERROR_TYPE_MASK		0x700
#define GLD_ERROR_TYPE_SHIFT		8
#define GLD_LEN1_MASK			0xFFF0000
#define GLD_LEN1_SHIFT			16
#define	GLD_TRANS_SOURCEID_MASK		0x7
#define	GLD_TRANS_SOURCEID_SHIFT	0
#define	GLD_TRANS_TARGETID_MASK		0x7
#define	GLD_TRANS_TARGETID_SHIFT	0
#define	GLD_ERRLOG_ERROR		0x7
#define GLD_ERRLOG5_ERROR_TYPE_MASK 0xFF000000
#define GLD_ERRLOG5_ERROR_TYPE_SHIFT 24
#define GLD_ACE_PORT_PARITY_MASK 0xc000
#define GLD_ACE_PORT_PARITY_SHIFT 14
#define GLD_ACE_PORT_DISCONNECT_MASK 0xf0000
#define GLD_ACE_PORT_DISCONNECT_SHIFT 16
#define GLD_ACE_PORT_DIRECTORY_MASK 0xf00000
#define GLD_ACE_PORT_DIRECTORY_SHIFT 20
#define GLD_INDEX_PARITY_MASK 0x1FFF
#define GLD_INDEX_PARITY_SHIFT 0
#define OBS_TRANS_OPCODE_MASK		0x1E
#define OBS_TRANS_OPCODE_SHIFT		1
#define OBS_ERROR_TYPE_MASK		0x700
#define OBS_ERROR_TYPE_SHIFT		8
#define OBS_LEN1_MASK			0x7F0000
#define OBS_LEN1_SHIFT			16

struct msm_gladiator_data {
	void __iomem *gladiator_virt_base;
	int erp_irq;
	struct notifier_block pm_notifier_block;
	struct clk *qdss_clk;
	bool atb_clock_on;
};

static int enable_panic_on_error;
module_param(enable_panic_on_error, int, 0);

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

static void clear_gladiator_error(struct msm_gladiator_data *data)
{
	void __iomem *gladiator_virt_base = data->gladiator_virt_base;

	writel_relaxed(1, gladiator_virt_base + GLADIATOR_ERRCLR);
	if (data->atb_clock_on)
		writel_relaxed(1, gladiator_virt_base + OBSERVER_0_ERRCLR);
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

static void decode_gld_errlog0(u32 err_reg)
{
	unsigned int opc, errtype, len1;

	opc = (err_reg & GLD_TRANS_OPCODE_MASK) >> GLD_TRANS_OPCODE_SHIFT;
	errtype = (err_reg & GLD_ERROR_TYPE_MASK) >> GLD_ERROR_TYPE_SHIFT;
	len1 = (err_reg & GLD_LEN1_MASK) >> GLD_LEN1_SHIFT;

	print_gld_transaction(opc);
	print_gld_errtype(errtype);
	pr_alert("number of payload bytes: %d\n", len1 + 1);
}

static void decode_gld_errlog1(u32 err_reg)
{
	if ((err_reg & GLD_ERRLOG_ERROR) == GLD_ERRLOG_ERROR)
		pr_alert("Transaction issued on IO target generic interface\n");
	else
		pr_alert("Transaction source ID: %d\n",
				(err_reg & GLD_TRANS_SOURCEID_MASK)
				>> GLD_TRANS_SOURCEID_SHIFT);
}

static void decode_gld_errlog2(u32 err_reg)
{
	if ((err_reg & GLD_ERRLOG_ERROR) == GLD_ERRLOG_ERROR)
		pr_alert("Error response coming from: external DVM network\n");
	else
		pr_alert("Error response coming from: Target ID: %d\n",
				(err_reg & GLD_TRANS_TARGETID_MASK)
				>> GLD_TRANS_TARGETID_SHIFT);
}

static void decode_ace_port_index(u32 type, u32 error)
{
	unsigned port;

	switch (type) {
	case DISCONNECT_ERROR:
		port = (error & GLD_ACE_PORT_DISCONNECT_MASK)
			>> GLD_ACE_PORT_DISCONNECT_SHIFT;
		pr_alert("ACE port index: %d\n", port);
		break;
	case DIRECTORY_ERROR:
		port = (error & GLD_ACE_PORT_DIRECTORY_MASK)
			>> GLD_ACE_PORT_DIRECTORY_SHIFT;
		pr_alert("ACE port index: %d\n", port);
		break;
	case PARITY_ERROR:
		port = (error & GLD_ACE_PORT_PARITY_MASK)
			>> GLD_ACE_PORT_PARITY_SHIFT;
		pr_alert("ACE port index: %d\n", port);
	}
}

static void decode_index_parity(u32 error)
{
	pr_alert("Index: %d\n",
			(error & GLD_INDEX_PARITY_MASK)
			>> GLD_INDEX_PARITY_SHIFT);
}

static void decode_gld_logged_error(u32 err_reg5)
{
	unsigned int log_err_type, i, value;

	log_err_type = (err_reg5 & GLD_ERRLOG5_ERROR_TYPE_MASK)
		>> GLD_ERRLOG5_ERROR_TYPE_SHIFT;
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
					err_reg5);
			break;
		case DIRECTORY_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: Directory error\n");
			decode_ace_port_index(
					DIRECTORY_ERROR,
					err_reg5);
			break;
		case PARITY_ERROR:
			if (value == 0)
				continue;
			pr_alert("Error type: Parity error\n");
			decode_ace_port_index(PARITY_ERROR, err_reg5);
			decode_index_parity(err_reg5);
			break;
		}
		log_err_type = log_err_type >> 1;
	}
}

static void decode_gld_errlog(u32 err_reg, unsigned int err_log)
{
	switch (err_log) {
	case ERR_LOG0:
		decode_gld_errlog0(err_reg);
		break;
	case ERR_LOG1:
		decode_gld_errlog1(err_reg);
		break;
	case ERR_LOG2:
		decode_gld_errlog2(err_reg);
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

static void decode_obs_errlog0(u32 err_reg)
{
	unsigned int opc, errcode, len1;

	opc = (err_reg & OBS_TRANS_OPCODE_MASK) >> OBS_TRANS_OPCODE_SHIFT;
	errcode = (err_reg & OBS_ERROR_TYPE_MASK) >> OBS_ERROR_TYPE_SHIFT;
	len1 = (err_reg & OBS_LEN1_MASK) >> OBS_LEN1_SHIFT;

	print_obs_transaction(opc);
	print_obs_errcode(errcode);
	pr_alert("number of payload bytes: %d\n", len1 + 1);
}

static void decode_obs_errlog(u32 err_reg, unsigned int err_log)
{
	switch (err_log) {
	case ERR_LOG0:
		decode_obs_errlog0(err_reg);
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

static u32 get_gld_offset(unsigned int err_log)
{
	u32 offset = 0;

	switch (err_log) {
	case ERR_LOG0:
		offset = GLADIATOR_ERRLOG0;
		break;
	case ERR_LOG1:
		offset = GLADIATOR_ERRLOG1;
		break;
	case ERR_LOG2:
		offset = GLADIATOR_ERRLOG2;
		break;
	case ERR_LOG3:
		offset = GLADIATOR_ERRLOG3;
		break;
	case ERR_LOG4:
		offset = GLADIATOR_ERRLOG4;
		break;
	case ERR_LOG5:
		offset = GLADIATOR_ERRLOG5;
		break;
	case ERR_LOG6:
		offset = GLADIATOR_ERRLOG6;
		break;
	case ERR_LOG7:
		offset = GLADIATOR_ERRLOG7;
		break;
	case ERR_LOG8:
		offset = GLADIATOR_ERRLOG8;
		break;
	default:
		pr_alert("Invalid gladiator error register; reg num:%u\n",
				err_log);
	}
	return offset;
}

static u32 get_obs_offset(unsigned int err_log)
{
	u32 offset = 0;

	switch (err_log) {
	case ERR_LOG0:
		offset = OBSERVER_0_ERRLOG0;
		break;
	case ERR_LOG1:
		offset = OBSERVER_0_ERRLOG1;
		break;
	case ERR_LOG2:
		offset = OBSERVER_0_ERRLOG2;
		break;
	case ERR_LOG3:
		offset = OBSERVER_0_ERRLOG3;
		break;
	case ERR_LOG4:
		offset = OBSERVER_0_ERRLOG4;
		break;
	case ERR_LOG5:
		offset = OBSERVER_0_ERRLOG5;
		break;
	case ERR_LOG6:
		offset = OBSERVER_0_ERRLOG6;
		break;
	case ERR_LOG7:
		offset = OBSERVER_0_ERRLOG7;
		break;
	case ERR_LOG8:
		offset = OBSERVER_0_ERRLOG8;
		break;
	case STALLEN:
		offset = OBSERVER_0_STALLEN;
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

	err_reg0 = readl_relaxed(msm_gld_data->gladiator_virt_base +
			get_gld_offset(ERR_LOG0));
	err_reg5 = readl_relaxed(msm_gld_data->gladiator_virt_base +
			get_gld_offset(ERR_LOG5));

	errtype = (err_reg0 & GLD_ERROR_TYPE_MASK) >> GLD_ERROR_TYPE_SHIFT;
	if (errtype == 3)
		decode_gld_logged_error(err_reg5);
	else if (errtype == 0 || errtype == 1)
		pr_alert("Lower 32-bits of user: %08x\n", err_reg5);
	else
		pr_alert("Error type: Unknown; value:%u\n", errtype);
}

static irqreturn_t msm_gladiator_isr(int irq, void *dev_id)
{
	u32 err_reg;
	unsigned int err_log;
	bool obsrv_err_valid;

	struct msm_gladiator_data *msm_gld_data = dev_id;

	/* Check validity */
	bool gld_err_valid = readl_relaxed(msm_gld_data->gladiator_virt_base +
			GLADIATOR_ERRVLD);

	if (msm_gld_data->atb_clock_on)
		obsrv_err_valid = readl_relaxed(
			msm_gld_data->gladiator_virt_base + OBSERVER_0_ERRVLD);
	else
		obsrv_err_valid = 0;

	if (!gld_err_valid && !obsrv_err_valid) {
		pr_err("%s Invalid Gladiator error reported, clear it\n",
				__func__);
		/* Clear IRQ */
		clear_gladiator_error(msm_gld_data->gladiator_virt_base);
		return IRQ_HANDLED;
	}
	pr_alert("GLADIATOR ERROR DETECTED\n");
	if (gld_err_valid) {
		pr_alert("GLADIATOR error log register data:\n");
		for (err_log = ERR_LOG0; err_log <= ERR_LOG8; err_log++) {
			/* skip log register 7 as its reserved */
			if (err_log == ERR_LOG7)
				continue;
			if (err_log == ERR_LOG5) {
				decode_gld_errlog5(msm_gld_data);
				continue;
			}
			err_reg = readl_relaxed(
					msm_gld_data->gladiator_virt_base +
					get_gld_offset(err_log));
			decode_gld_errlog(err_reg, err_log);
		}
	}
	if (obsrv_err_valid) {
		pr_alert("Observor error log register data:\n");
		for (err_log = ERR_LOG0; err_log <= STALLEN; err_log++)	{
			/* skip log register 2, 6 and 8 as they are reserved */
			if ((err_log == ERR_LOG2) || (err_log == ERR_LOG6)
					|| (err_log == ERR_LOG8))
				continue;
			err_reg = readl_relaxed(
					msm_gld_data->gladiator_virt_base +
					get_obs_offset(err_log));
			decode_obs_errlog(err_reg, err_log);
		}
	}
	/* Clear IRQ */
	clear_gladiator_error(msm_gld_data->gladiator_virt_base);
	if (enable_panic_on_error)
		BUG_ON(1);
	else
		WARN(1, "Gladiator Cache Interconnect Error Detected\n");

	return IRQ_HANDLED;
}

static const struct of_device_id gladiator_erp_v2_match_table[] = {
	{ .compatible = "qcom,msm-gladiator-v2" },
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
	clear_gladiator_error(msm_gld_data->gladiator_virt_base);
	ret = devm_request_irq(&pdev->dev, msm_gld_data->erp_irq,
			msm_gladiator_isr, IRQF_TRIGGER_HIGH,
			"gladiator-error", msm_gld_data);
	if (ret)
		dev_err(&pdev->dev, "Failed to register irq handler\n");

	return ret;
}

static inline void gladiator_irq_init(struct msm_gladiator_data *data)
{
	void __iomem *gladiator_virt_base = data->gladiator_virt_base;

	writel_relaxed(1, gladiator_virt_base + GLADIATOR_FAULTEN);

	if (data->atb_clock_on)
		writel_relaxed(1, gladiator_virt_base + OBSERVER_0_FAULTEN);
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
		gladiator_irq_init(msm_gld_data->gladiator_virt_base);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static int gladiator_erp_v2_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_gladiator_data *msm_gld_data;

	msm_gld_data = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_gladiator_data), GFP_KERNEL);
	if (!msm_gld_data) {
		ret = -ENOMEM;
		goto bail;
	}

	ret = parse_dt_node(pdev, msm_gld_data);
	if (ret)
		goto bail;
	msm_gld_data->pm_notifier_block.notifier_call =
		gladiator_erp_pm_callback;

	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "atb_clk") >= 0) {
		msm_gld_data->qdss_clk = devm_clk_get(&pdev->dev, "atb_clk");
		if (IS_ERR(msm_gld_data->qdss_clk)) {
			dev_err(&pdev->dev, "Failed to get QDSS ATB clock\n");
			msm_gld_data->atb_clock_on = false;
			goto clk_finish;
		}
	} else {
		dev_err(&pdev->dev, "No matching string of QDSS ATB clock\n");
		goto bail;
	}

	ret = clk_prepare_enable(msm_gld_data->qdss_clk);

	if (ret) {
		clk_disable_unprepare(msm_gld_data->qdss_clk);
		msm_gld_data->atb_clock_on = false;
	} else
		msm_gld_data->atb_clock_on = true;

clk_finish:
	gladiator_irq_init(msm_gld_data->gladiator_virt_base);
	platform_set_drvdata(pdev, msm_gld_data);
	cpu_pm_register_notifier(&msm_gld_data->pm_notifier_block);
#ifdef CONFIG_PANIC_ON_GLADIATOR_ERROR_V2
	enable_panic_on_error = 1;
#endif
	dev_info(&pdev->dev, "MSM Gladiator Error Reporting V2 Initialized\n");
	return 0;

bail:
	dev_err(&pdev->dev, "Probe failed bailing out\n");
	return ret;
}

static int gladiator_erp_v2_remove(struct platform_device *pdev)
{
	struct msm_gladiator_data *msm_gld_data = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	cpu_pm_unregister_notifier(&msm_gld_data->pm_notifier_block);
	clk_disable_unprepare(msm_gld_data->qdss_clk);
	return 0;
}

static struct platform_driver gladiator_erp_driver = {
	.probe = gladiator_erp_v2_probe,
	.remove = gladiator_erp_v2_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = gladiator_erp_v2_match_table,
	},
};

static int __init init_gladiator_erp_v2(void)
{
	int ret;

	ret = scm_is_secure_device();
	if (ret != 0) {
		pr_info("Gladiator Error Reporting not available\n");
		return -ENODEV;
	}

	return platform_driver_register(&gladiator_erp_driver);
}
module_init(init_gladiator_erp_v2);

static void __exit exit_gladiator_erp_v2(void)
{
	return platform_driver_unregister(&gladiator_erp_driver);
}
module_exit(exit_gladiator_erp_v2);

MODULE_DESCRIPTION("Gladiator Error Reporting V2");
MODULE_LICENSE("GPL v2");
