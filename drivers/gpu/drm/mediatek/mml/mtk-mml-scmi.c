// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/of.h>
#include <linux/scmi_protocol.h>

#include "mtk-mml-core.h"
#include "tinysys-scmi.h"

#ifndef MML_FPGA
struct mml_scmi_support {
	struct scmi_tinysys_info_st *tinfo;
	u32 feature_id;
};

static bool mml_check_scmi_status(struct mml_scmi_support *scmi)
{
	if (scmi->tinfo)
		return true;

	scmi->tinfo = get_scmi_tinysys_info();
	if (IS_ERR_OR_NULL(scmi->tinfo)) {
		mml_err("%s tinfo wrong %pe", __func__, scmi->tinfo);
		scmi->tinfo = NULL;
		return false;
	}

	if (IS_ERR_OR_NULL(scmi->tinfo->ph)) {
		mml_err("%s tinfo->ph wrong %pe", __func__, scmi->tinfo->ph);
		scmi->tinfo = NULL;
		return false;
	}

	of_property_read_u32(scmi->tinfo->sdev->dev.of_node, "scmi-mminfra",
		&scmi->feature_id);
	mml_log("%s scmi_smi succeed id %u",
		__func__, scmi->feature_id);
	return true;
}

void mml_set_uid(void **mml_scmi)
{
	/* 0 for mdp, 1 for mml */
	const unsigned int id = 1;
	struct mml_scmi_support *scmi = (struct mml_scmi_support *)*mml_scmi;
	int err;

	if (!scmi) {
		scmi = kzalloc(sizeof(*scmi), GFP_KERNEL);
		if (!scmi)
			return;
		*mml_scmi = scmi;
	}

	if (!mml_check_scmi_status(scmi))
		return;

	err = scmi_tinysys_common_set(scmi->tinfo->ph, scmi->feature_id, 3, id, 0, 0, 0);
	if (err)
		mml_err("%s call scmi_tinysys_common_set %d err %d",
			__func__, id, err);
}

#else	/* MML_FPGA */

void mml_set_uid(void **mml_scmi)
{
}

#endif	/* MML_FPGA */
