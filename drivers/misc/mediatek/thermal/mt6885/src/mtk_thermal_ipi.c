/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include "mtk_thermal_ipi.h"
#include "mach/mtk_thermal.h"
#include "tscpu_settings.h"
#include "linux/mutex.h"

#if THERMAL_ENABLE_TINYSYS_SSPM
/* ipi_send() return code
 * IPI_DONE 0
 * IPI_RETURN 1
 * IPI_BUSY -1
 * IPI_TIMEOUT_AVL -2
 * IPI_TIMEOUT_ACK -3
 * IPI_MODULE_ID_ERROR -4
 * IPI_HW_ERROR -5
 */
static DEFINE_MUTEX(thermo_sspm_mutex);
static int is_thermal_ipi_registered;
static int ack_data;

static int register_thermal_ipi(void)
{
	int ret;

	ret = mtk_ipi_register(&sspm_ipi_dev, IPIS_C_THERMAL, NULL, NULL,
		(void *)&ack_data);
	if (ret != 0) {
		tscpu_printk("%s error ret:%d\n", __func__, ret);
		return -1;
	}

	is_thermal_ipi_registered = 1;

	return 0;
}

unsigned int thermal_to_sspm(
	unsigned int cmd, struct thermal_ipi_data *thermal_data)
{
	int ackData = -1;
	int ret;

	mutex_lock(&thermo_sspm_mutex);

	if (!is_thermal_ipi_registered) {
		if (register_thermal_ipi() != 0)
			goto end;
	}

	switch (cmd) {
	case THERMAL_IPI_INIT_GRP1:
	case THERMAL_IPI_INIT_GRP2:
	case THERMAL_IPI_INIT_GRP3:
	case THERMAL_IPI_INIT_GRP4:
	case THERMAL_IPI_INIT_GRP5:
	case THERMAL_IPI_INIT_GRP6:
		thermal_data->cmd = cmd;
		ret = sspm_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_WAIT, thermal_data, THERMAL_SLOT_NUM, 10);
		if (ret != 0)
			tscpu_printk("send init cmd(%d) error ret:%d\n",
				cmd, ret);
		else if (ack_data < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ack_data);

		ackData = ack_data;

		break;

	case THERMAL_IPI_GET_TEMP:
		thermal_data->cmd = cmd;
		ret = sspm_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_WAIT, thermal_data, THERMAL_SLOT_NUM, 10);
		if (ret != 0)
			tscpu_printk("send get_temp cmd(%d) error ret:%d\n",
				cmd, ret);
		else if (ack_data < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ack_data);

		ackData = ack_data;

		break;

	default:
		tscpu_printk("cmd(%d) wrong!!\n", cmd);
		break;
	}

end:
	mutex_unlock(&thermo_sspm_mutex);

	return ackData; /** It's weird here. What should be returned? */
}

/* ipi_send() return code
 * IPI_DONE 0
 * IPI_RETURN 1
 * IPI_BUSY -1
 * IPI_TIMEOUT_AVL -2
 * IPI_TIMEOUT_ACK -3
 * IPI_MODULE_ID_ERROR -4
 * IPI_HW_ERROR -5
 */
int atm_to_sspm(unsigned int cmd, int data_len,
struct thermal_ipi_data *thermal_data, int *ackData)
{
	int ret = -1;

	if (data_len < 1 || data_len > 3) {
		*ackData = -1;
		return ret;
	}

	mutex_lock(&thermo_sspm_mutex);

	if (!is_thermal_ipi_registered) {
		if (register_thermal_ipi() != 0)
			goto end;
	}

	switch (cmd) {
	case THERMAL_IPI_SET_ATM_CFG_GRP1:
	case THERMAL_IPI_SET_ATM_CFG_GRP2:
	case THERMAL_IPI_SET_ATM_CFG_GRP3:
	case THERMAL_IPI_SET_ATM_CFG_GRP4:
	case THERMAL_IPI_SET_ATM_CFG_GRP5:
	case THERMAL_IPI_SET_ATM_CFG_GRP6:
	case THERMAL_IPI_SET_ATM_CFG_GRP7:
	case THERMAL_IPI_SET_ATM_CFG_GRP8:
	case THERMAL_IPI_SET_ATM_TTJ:
	case THERMAL_IPI_SET_ATM_EN:
	case THERMAL_IPI_GET_ATM_CPU_LIMIT:
	case THERMAL_IPI_GET_ATM_GPU_LIMIT:
		thermal_data->cmd = cmd;
		ret = sspm_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_WAIT, thermal_data, (data_len+1), 10);
		if ((ret != 0) || (ack_data < 0))
			tscpu_printk("%s cmd %d ret %d ack %d\n",
				__func__, cmd, ret, ack_data);

		*ackData = ack_data;

		break;

	default:
		tscpu_printk("%s cmd %d err!\n", __func__, cmd);
		break;
	}

end:
	mutex_unlock(&thermo_sspm_mutex);

	return ret;
}

#endif
