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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    pmic_ipi.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines PMIC functions
 *
 * Author:
 * -------
 * Wilma Wu
 *
 ****************************************************************************/
#include <mt-plat/upmu_common.h>
#include <sspm_ipi_pin.h>
#include <sspm_ipi.h>
#include <include/pmic_ipi.h>
#include <include/pmic_ipi_service_id.h>


unsigned int pmic_ipi_to_sspm(void *buffer, void *retbuf)
{
	int ret_val = 0;
	unsigned int cmd = ((struct pmic_ipi_cmds *)buffer)->cmd[0];

	switch (cmd) {

	case MAIN_PMIC_WRITE_REGISTER:
		ret_val =
		    sspm_ipi_send_sync_ex(IPI_ID_PMIC, IPI_OPT_DEFAUT, buffer,
					  PMIC_IPI_SEND_SLOT_SIZE, retbuf, PMIC_IPI_ACK_SLOT_SIZE);
		if (ret_val >= 0) {
			/* Real PMIC service execution result, by each PMIC service */
			ret_val = ((struct pmic_ipi_ret_datas *)retbuf)->data[0];
		}
		break;

	case MAIN_PMIC_READ_REGISTER:
		ret_val =
		    sspm_ipi_send_sync_ex(IPI_ID_PMIC, IPI_OPT_DEFAUT, buffer,
					  PMIC_IPI_SEND_SLOT_SIZE, retbuf, PMIC_IPI_ACK_SLOT_SIZE);
		if (ret_val >= 0) {
			/* Real PMIC service execution result, by each PMIC service */
			ret_val = ((struct pmic_ipi_ret_datas *)retbuf)->data[0];
		}
		break;

	default:
		pr_err("%s(%d) cmd(%d) wrong!!!\n", __func__, __LINE__, cmd);

		break;
	}

	return ret_val;

}

unsigned int pmic_ipi_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK,
				     unsigned int SHIFT)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };
	unsigned int ret = 0;

	send.cmd[0] = MAIN_PMIC_READ_REGISTER;
	send.cmd[1] = RegNum;
	send.cmd[2] = MASK;
	send.cmd[3] = SHIFT;

	ret = pmic_ipi_to_sspm(&send, &recv);

	if (ret >= 0)
		*val = recv.data[1];

	return ret;
}

unsigned int pmic_ipi_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK,
				       unsigned int SHIFT)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };
	unsigned int ret = 0;

	send.cmd[0] = MAIN_PMIC_WRITE_REGISTER;
	send.cmd[1] = RegNum;
	send.cmd[2] = val;
	send.cmd[3] = MASK;
	send.cmd[4] = SHIFT;

	ret = pmic_ipi_to_sspm(&send, &recv);

	return ret;
}

unsigned int pmic_ipi_test_code(void)
{
	unsigned int ret_val = 0;
	unsigned int rdata = 0;
	unsigned int error = 0;
	int i = 0;
	unsigned int test_data[30] = {
	    0x6996, 0x9669, 0x6996, 0x9669, 0x6996, 0x9669, 0x6996, 0x9669, 0x6996, 0x9669,
		0x5AA5, 0xA55A, 0x5AA5, 0xA55A, 0x5AA5, 0xA55A, 0x5AA5, 0xA55A, 0x5AA5, 0xA55A,
		0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27
	};

	for (i = 0; i < 30; i++) {
		ret_val =
		    pmic_ipi_config_interface(PMIC_DEW_WRITE_TEST_ADDR, test_data[i], 0xffff, 0);

		if (ret_val != 0) {
			pr_err("%s config failed: test_data[%d]=%x ret_val=%x\n", __func__, i,
			       test_data[i], ret_val);
			error++;
			break;
		}

		ret_val = pmic_ipi_read_interface(PMIC_DEW_WRITE_TEST_ADDR, &rdata, 0xffff, 0);

		if (ret_val != 0 || rdata != test_data[i]) {
			pr_err("%s read failed: test_data[%d]=%x rdata =%x ret_val=%x\n", __func__,
			       i, test_data[i], rdata, ret_val);
			error++;
			break;
		}

		pr_debug("%s ok: test_data[%d]=%x rdata=%x\n", __func__, i, test_data[i], rdata);
	}

	return error;

}
