// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

//#include <mtk_lpm_trace.h>
#include <sspm_ipi.h>

#include <mtk_ipi_sspm_v1.h>

#include "mt6779_ipi_sspm.h"
#include "mt6779_common.h"

static int _mt6779_ipi_sspm_send(int flags, unsigned int cmd)
{
	int ret;

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct plat_spm_data spm_d;

	if ((flags == SSPM_NOTIFY_ENTER)
		|| (flags == SSPM_NOTIFY_ENTER_ASYNC)) {
		spm_d.cmd = (cmd & PLAT_SUSPEND) ?
			     SSPM_SPM_SUSPEND :
			     (cmd & PLAT_PMIC_VCORE_SRCLKEN0) ?
			     SSPM_SPM_ENTER_SODI3 :
			     (cmd & PLAT_PMIC_VCORE_SRCLKEN2) ?
			     SSPM_SPM_DPIDLE_ENTER : SSPM_SPM_ENTER_SODI;
	} else if ((flags == SSPM_NOTIFY_LEAVE)
		|| (flags == SSPM_NOTIFY_LEAVE_ASYNC)) {
		spm_d.cmd = (cmd & PLAT_SUSPEND) ?
			     SSPM_SPM_RESUME :
			     (cmd & PLAT_PMIC_VCORE_SRCLKEN0) ?
			     SSPM_SPM_LEAVE_SODI3 :
			     (cmd & PLAT_PMIC_VCORE_SRCLKEN2) ?
			     SSPM_SPM_DPIDLE_LEAVE : SSPM_SPM_LEAVE_SODI;
	}

	spm_d.u.suspend.spm_opt = (cmd & PLAT_VCORE_LP_MODE) ?
				   PLAT_PWR_OPT_VCORE_LP_MODE : 0;
	spm_d.u.suspend.spm_opt |= (cmd & PLAT_XO_UFS_OFF) ?
				   PLAT_PWR_OPT_XO_UFS_OFF : 0;
	spm_d.u.suspend.spm_opt |= (cmd & PLAT_CLKBUF_ENTER_BBLPM) ?
				   PLAT_PWR_OPT_CLKBUF_ENTER_BBLPM : 0;

	if ((flags == SSPM_NOTIFY_ENTER_ASYNC)
	   || (flags == SSPM_NOTIFY_LEAVE_ASYNC)) {
		ret = -1;
		//ret = sspm_ipi_send_async(IPI_ID_SPM_SUSPEND, IPI_OPT_DEFAUT,
		//			&spm_d, SSPM_SPM_DATA_LEN);
		if (ret != 0)
			pr_info("[name:spm&]#@# %s(%d) async(cmd:0x%x) ret %d\n",
					__func__, __LINE__, spm_d.cmd, ret);
	} else {
		int ack_data = 0;

		ret = -1;
		//ret = sspm_ipi_send_sync(IPI_ID_SPM_SUSPEND, IPI_OPT_POLLING,
		//			&spm_d, SSPM_SPM_DATA_LEN,
		//			&ack_data, 1);

		if (ret != 0)
			pr_info("[name:spm&]#@# %s(%d) sync(cmd:0x%x) ret %d\n",
					__func__, __LINE__, spm_d.cmd, ret);
		else if (ack_data < 0) {
			ret = ack_data;
			pr_info("[name:spm&]#@# %s(%d) sync cmd:0x%x ret %d\n",
				__func__, __LINE__, spm_d.cmd, ret);
		}
	}
#else
	pr_info("[name:spm&]#@# %s(%d) TINYSYS_SSPM_SUPPORT is not support\n",
			__func__, __LINE__);
#endif /* if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) */

	return ret;
}

static int _mt6779_ipi_sspm_response(int flags, unsigned int cmd)
{
	int ret = 0;

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int ack_data = 0;

	if ((flags == SSPM_NOTIFY_ENTER_ASYNC)
	   || (flags == SSPM_NOTIFY_LEAVE_ASYNC))
		ret = -1;
		//ret = sspm_ipi_send_async_wait(IPI_ID_SPM_SUSPEND,
		//			       IPI_OPT_DEFAUT, &ack_data);
	else
		return 0;

	if (ret != 0)
		pr_info("[name:spm&]#@# %s(%d) async_wait(cmd:0x%x) ret %d\n",
				__func__, __LINE__, cmd, ret);
	else if (ack_data < 0) {
		ret = ack_data;
		pr_info("[name:spm&]#@# %s(%d) async_waitcmd(%d) return %d\n",
			__func__, __LINE__, cmd, ret);
	}
#else
	pr_info("[name:spm&]#@# %s(%d) TINYSYS_SSPM_SUPPORT is not support\n",
			__func__, __LINE__);
#endif /* if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) */
	return ret;
}

int mt6779_sspm_notify_enter(unsigned int cmd)
{
	return _mt6779_ipi_sspm_send(SSPM_NOTIFY_ENTER, cmd);
}

int mt6779_sspm_notify_leave(unsigned int cmd)
{
	return _mt6779_ipi_sspm_send(SSPM_NOTIFY_LEAVE, cmd);
}

int mt6779_sspm_notify_ansyc_enter(unsigned int cmd)
{
	return _mt6779_ipi_sspm_send(SSPM_NOTIFY_ENTER_ASYNC, cmd);
}

int mt6779_sspm_notify_ansyc_enter_respone(void)
{
	return _mt6779_ipi_sspm_response(SSPM_NOTIFY_ENTER_ASYNC, 0);
}

int mt6779_sspm_notify_ansyc_leave(unsigned int cmd)
{
	return _mt6779_ipi_sspm_send(SSPM_NOTIFY_LEAVE_ASYNC, cmd);
}

int mt6779_sspm_notify_ansyc_leave_respone(void)
{
	return _mt6779_ipi_sspm_response(SSPM_NOTIFY_LEAVE_ASYNC, 0);
}
