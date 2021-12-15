/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <include/pmic_ipi.h>
#include <include/pmic_ipi_service_id.h>
#include <linux/kernel.h>
#include <mach/mtk_pmic_ipi.h>
#include <sspm_define.h>
#include <sspm_ipi_id.h>

#define PMIC_IPI_TIMEOUT 2000

static bool is_ipi_register;
static int sspm_pmic_ack;

static int pmic_ipi_to_sspm(struct pmic_ipi_cmds *ipi_cmd)
{
	int ret, cmd_len;

	if (!is_ipi_register) {
		ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_PMIC, NULL, NULL,
				       (void *)&sspm_pmic_ack);
		if (ret) {
			pr_notice("[PMIC] ipi_register fail, ret=%d\n", ret);
			return ret;
		}
		is_ipi_register = true;
	}

	cmd_len = sizeof(struct pmic_ipi_cmds) / SSPM_MBOX_SLOT_SIZE;
	ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_PMIC, IPI_SEND_POLLING,
				 ipi_cmd, cmd_len, PMIC_IPI_TIMEOUT);
	if (ret) {
		pr_notice("[PMIC] IPI fail ret=%d\n", ret);
		return ret;
	}
	if (sspm_pmic_ack) {
		pr_notice("[PMIC] IPI fail, ackdata=%d\n", sspm_pmic_ack);
		return sspm_pmic_ack;
	}
	return 0;
}

unsigned int pmic_ipi_config_interface(unsigned int RegNum, unsigned int val,
				       unsigned int MASK, unsigned int SHIFT,
				       unsigned char _unused)
{
	struct pmic_ipi_cmds ipi_cmd;
	int ret;

	ipi_cmd.cmd[0] = MAIN_PMIC_WRITE_REGISTER;
	ipi_cmd.cmd[1] = RegNum;
	ipi_cmd.cmd[2] = val;
	ipi_cmd.cmd[3] = MASK;
	ipi_cmd.cmd[4] = SHIFT;

	ret = pmic_ipi_to_sspm(&ipi_cmd);

	return abs(ret);
}

