// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */
#include <linux/device.h>
#include <linux/arm-smccc.h>
#include <soc/qcom/qseecom_scm.h>

int qcom_scm_qseecom_call(u32 cmd_id, struct scm_desc *desc)
{
	return __qcom_scm_qseecom_do(cmd_id, desc, true);
}
EXPORT_SYMBOL(qcom_scm_qseecom_call);

int qcom_scm_qseecom_call_noretry(u32 cmd_id, struct scm_desc *desc)
{
	return __qcom_scm_qseecom_do(cmd_id, desc, false);
}
EXPORT_SYMBOL(qcom_scm_qseecom_call_noretry);
