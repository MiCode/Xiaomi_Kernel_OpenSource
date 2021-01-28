// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "dvfsrc.h"
#include "dvfsrc-opp.h"
#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>

enum {
	IPI_DVFSRC_ENABLE,
	IPI_OPP_TABLE,
	IPI_VCORE_OPP,
	IPI_DDR_OPP,
};

struct dvfsrc_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int dvfsrc_en;
		} dvfsrc_enable;
		struct {
			unsigned int vcore_dvfs_opp;
			unsigned int vcore_uv;
			unsigned int ddr_khz;
		} opp_table;
		struct {
			unsigned int vcore_dvfs_opp;
			unsigned int vcore_opp;
		} vcore_opp;
		struct {
			unsigned int vcore_dvfs_opp;
			unsigned int ddr_opp;
		} ddr_opp;
	} u;
};

static const int mt6761_qos_ipi_pin[] = {
	[IPI_DVFSRC_ENABLE] = 0,
	[IPI_OPP_TABLE] = 1,
	[IPI_VCORE_OPP] = 2,
	[IPI_DDR_OPP] = 3,
};

static const int mt6779_qos_ipi_pin[] = {
	[IPI_DVFSRC_ENABLE] = 1,
	[IPI_OPP_TABLE] = 2,
	[IPI_VCORE_OPP] = 3,
	[IPI_DDR_OPP] = 4,
};

static int mt6779_qos_ipi_to_sspm(void *buffer, int slot)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int ack_data = 0;

	return sspm_ipi_send_sync(IPI_ID_QOS, IPI_OPT_POLLING,
			buffer, slot, &ack_data, 1);
#else
	return 0;
#endif
}

static int mt6779_qos_dvfsrc_init(struct mtk_dvfsrc *dvfsrc)
{
	int i;
	const int *ipi_pin;
	struct dvfsrc_opp_desc *opp_desc;
	struct dvfsrc_opp *opp;
	struct dvfsrc_ipi_data ipi_d;
	unsigned int opp_idx, v_opp_idx, d_opp_idx;

	ipi_pin = dvfsrc->dvd->qos->ipi_pin;
	opp_desc = dvfsrc->opp_desc;

	for (i = 0; i < dvfsrc->opp_desc->num_opp; i++) {
		opp = &opp_desc->opps[i];
		opp_idx = opp_desc->num_opp - (i + 1);
		v_opp_idx = opp_desc->num_vcore_opp - (opp->vcore_opp + 1);
		d_opp_idx = opp_desc->num_dram_opp - (opp->dram_opp + 1);

		ipi_d.cmd = ipi_pin[IPI_VCORE_OPP];
		ipi_d.u.vcore_opp.vcore_dvfs_opp = opp_idx;
		ipi_d.u.vcore_opp.vcore_opp = v_opp_idx;
		mt6779_qos_ipi_to_sspm(&ipi_d, 3);

		ipi_d.cmd = ipi_pin[IPI_DDR_OPP];
		ipi_d.u.ddr_opp.vcore_dvfs_opp = opp_idx;
		ipi_d.u.ddr_opp.ddr_opp = d_opp_idx;
		mt6779_qos_ipi_to_sspm(&ipi_d, 3);


		ipi_d.cmd = ipi_pin[IPI_OPP_TABLE];
		ipi_d.u.opp_table.vcore_dvfs_opp = opp_idx;
		ipi_d.u.opp_table.vcore_uv = opp->vcore_uv;
		ipi_d.u.opp_table.ddr_khz = opp->dram_khz;
		mt6779_qos_ipi_to_sspm(&ipi_d, 4);
	}

	ipi_d.cmd = ipi_pin[IPI_DVFSRC_ENABLE];
	ipi_d.u.dvfsrc_enable.dvfsrc_en = 1;
	mt6779_qos_ipi_to_sspm(&ipi_d, 2);

	return 0;
}


const struct dvfsrc_qos_config mt6779_qos_config = {
	.ipi_pin = mt6779_qos_ipi_pin,
	.qos_dvfsrc_init = mt6779_qos_dvfsrc_init,
};

const struct dvfsrc_qos_config mt6761_qos_config = {
	.ipi_pin = mt6761_qos_ipi_pin,
	.qos_dvfsrc_init = mt6779_qos_dvfsrc_init,
};

