// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#include <soc/qcom/watchdog.h>
#include <linux/arm-smccc.h>
#include <linux/haven/hh_errno.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#define VIRT_WDT_CONTROL \
		ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,\
				ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0x0005)
#define VIRT_WDT_STATUS	\
		ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,\
				ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0x0006)
#define VIRT_WDT_PET \
		ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,\
				ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0x0007)
#define VIRT_WDT_SET_TIME \
		ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,\
				ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0x0008)
#define VIRT_WDT_NO_CHANGE 0xFFFF

/**
 *  hh_wdt_call() - Sends ARM SMCCC 1.1 Calls to the hypervisor
 *
 *  @smc_id: The smc id needed to interact with the watchdog in the hypervisor
 *  @arg1:   A u32 value to be sent to the the hypervisor
 *  @arg2:   A u16 value to be sent to the the hypervisor
 *  @arg3:   A u16 value to be sent to the the hypervisor
 *
 *  The hypervisor takes input via  ARM SMCCC Calls. The position of
 *  these values matter. u16 values are needed to set both the bark
 *  and bite time. In all other cases only the u32 (arg1) value is required.
 *
 *  return: 0 on success, negative errno on failure.
 */
static struct arm_smccc_res hh_wdt_call(u32 smc_id, u32 arg1,
					u16 arg2, u16 arg3)
{
	struct arm_smccc_res res;

	if (smc_id == VIRT_WDT_SET_TIME)
		/* virtual watchdog expecting u16 values for bark and bite */
		arm_smccc_1_1_smc(smc_id, arg2, arg3,  &res);
	else
		arm_smccc_1_1_smc(smc_id, arg1, &res);

	return res;
}

/**
 *  hh_set_wdt_bark() - Sets the bark time for the virtual watchdog
 *
 *  @time:    A u32 value to be converted to milliseconds (u16)
 *  @wdog_dd: The qcom watchdog data structure
 *
 *  The hypervisor requires both the bark and the bite time in the same
 *  call. To update one and not the other, the value VIRT_WDT_NO_CHANGE
 *  is used.
 *
 *  return: 0 on success, negative errno on failure.
 */
static int hh_set_wdt_bark(u32 time, struct msm_watchdog_data *wdog_dd)
{
	struct arm_smccc_res res;
	int hret, ret;
	u16 bark_time;

	bark_time = (u16) time;
	res = hh_wdt_call(VIRT_WDT_SET_TIME, 0, bark_time, VIRT_WDT_NO_CHANGE);
	hret = res.a0;
	ret = hh_remap_error(hret);
	if (hret) {
		dev_err(wdog_dd->dev, "failed to set bark time for vDOG, hret = %d ret = %d\n",
			hret, ret);
	}

	return ret;
}

/**
 *  hh_set_wdt_bite() - Sets the bite time for the virtual watchdog
 *
 *  @time:    A u32 value to be converted to milliseconds (u16)
 *  @wdog_dd: The qcom watchdog data structure
 *
 *  The hypervisor requires both the bark and the bite time in the same
 *  call. To update one and not the other, the value VIRT_WDT_NO_CHANGE
 *  is used.
 *
 *  return: 0 on success, negative errno on failure.
 */
static int hh_set_wdt_bite(u32 time, struct msm_watchdog_data *wdog_dd)
{
	struct arm_smccc_res res;
	int hret, ret;
	u16 bite_time;

	bite_time = (u16) time;
	res = hh_wdt_call(VIRT_WDT_SET_TIME, 0, VIRT_WDT_NO_CHANGE, bite_time);
	hret = res.a0;
	ret = hh_remap_error(hret);
	if (hret) {
		dev_err(wdog_dd->dev, "failed to set bite time for vWDOG, hret = %d ret = %d\n",
			hret, ret);
	}

	return ret;
}

/**
 *  hh_reset_wdt() - Resets the virtual watchdog timer
 *
 *  @wdog_dd: The qcom watchdog data structure
 *
 *  VIRT_WDT_PET is used to reset the virtual watchdog.
 *
 *  return: 0 on success, negative errno on failure.
 */
static int hh_reset_wdt(struct msm_watchdog_data *wdog_dd)
{
	struct arm_smccc_res res;
	int hret, ret;

	res = hh_wdt_call(VIRT_WDT_PET, 0, 0, 0);
	hret = res.a0;
	ret = hh_remap_error(hret);
	if (hret) {
		dev_err(wdog_dd->dev, "failed to reset vWDOG, hret = %d ret = %d\n",
			hret, ret);
	}

	return ret;
}

/**
 *  hh_enable_wdt() - Enables the virtual watchdog
 *
 *  @wdog_dd: The qcom watchdog data structure
 *  @state:   state value to send to watchdog
 *
 *  VIRT_WDT_CONTROL is used to enable the virtual watchdog.
 *  Bit 0 is used to enable the watchdog. When this Bit is set to
 *  1 the watchdog is enabled. NOTE: Bit 1 must always be set to
 *  1 as this bit is reserved in the hypervisor and Bit 1 is
 *  expected to be 1. If this Bit is not set, the hypervisor will
 *  return an error. So to enable the watchdog you must use the value 3.
 *  An error from the hypervisor is expected if you try to enable the
 *  watchdog when its already enabled.
 *
 *  return: 0 on success, negative errno on failure.
 */
static int hh_enable_wdt(u32 state, struct msm_watchdog_data *wdog_dd)
{
	struct arm_smccc_res res;
	int hret, ret;

	if (wdog_dd->enabled) {
		dev_err(wdog_dd->dev, "vWDT already enabled\n");
		return 0;
	}
	res = hh_wdt_call(VIRT_WDT_CONTROL, 3, 0, 0);
	hret = res.a0;
	ret = hh_remap_error(hret);
	if (hret) {
		dev_err(wdog_dd->dev, "failed enabling vWDOG, hret = %d ret = %d\n",
			hret, ret);
	}

	return ret;
}

/**
 *  hh_disable_wdt() - Disables the virtual watchdog
 *
 *  @wdog_dd: The qcom watchdog data structure
 *
 *  VIRT_WDT_CONTROL is used to disable the virtual watchdog.
 *  Bit 0 is used to disable the watchdog. When this Bit is set to
 *  0 the watchdog is disabled. NOTE: Bit 1 must always be set to
 *  1 as this bit is reserved in the hypervisor and Bit 1 is
 *  expected to be 1. If this Bit is not set, the hypervisor will
 *  return an error. So to disable the watchdog you must use the value 2.
 *  An error from the hypervisor is expected if you try to disable the
 *  watchdog when its already disabled.
 *
 *  return: 0 on success, negative errno on failure.
 */
static int hh_disable_wdt(struct msm_watchdog_data *wdog_dd)
{
	struct arm_smccc_res res;
	int hret, ret;

	if (!wdog_dd->enabled) {
		dev_err(wdog_dd->dev, "vWDT already disabled\n");
		return 0;
	}
	res = hh_wdt_call(VIRT_WDT_CONTROL, 2, 0, 0);
	hret = res.a0;
	ret = hh_remap_error(hret);
	if (hret) {
		dev_err(wdog_dd->dev, "failed disabling VDOG, hret = %d ret = %d\n",
			hret, ret);
	}

	return ret;
}

/**
 *  hh_get_wdt_status() - Displays the status of the virtual watchdog
 *
 *  @wdog_dd: The qcom watchdog data structure
 *
 *  VIRT_WDT_STATUS is used to display status of the virtual  watchdog.
 *
 *  return: 0 on success, negative errno on failure.
 */
static int hh_show_wdt_status(struct msm_watchdog_data *wdog_dd)
{
	struct arm_smccc_res res;
	int hret, ret;

	res = hh_wdt_call(VIRT_WDT_STATUS, 0, 0, 0);
	hret = res.a0;
	ret = hh_remap_error(hret);
	if (hret) {
		dev_err(wdog_dd->dev, "failed to get vWDOG status, hret = %d ret = %d\n",
			hret, ret);
	} else {
		dev_err(wdog_dd->dev,
			"vWdog-CTL: %d, vWdog-time since last pet: %d, vWdog-expired status: %d\n",
			res.a1 & 1, res.a2, (res.a1 >> 31) & 1);
	}

	return ret;
}

static struct qcom_wdt_ops hh_wdt_ops = {
	.set_bark_time  = hh_set_wdt_bark,
	.set_bite_time  = hh_set_wdt_bite,
	.reset_wdt      = hh_reset_wdt,
	.enable_wdt     = hh_enable_wdt,
	.disable_wdt    = hh_disable_wdt,
	.show_wdt_status = hh_show_wdt_status
};

static int hh_wdt_probe(struct platform_device *pdev)
{
	struct msm_watchdog_data *wdog_dd;

	wdog_dd = devm_kzalloc(&pdev->dev, sizeof(*wdog_dd), GFP_KERNEL);
	if (!wdog_dd)
		return -ENOMEM;
	wdog_dd->ops = &hh_wdt_ops;

	return qcom_wdt_register(pdev, wdog_dd, "hh-watchdog");
}

static const struct dev_pm_ops hh_wdt_dev_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend_late = qcom_wdt_pet_suspend,
	.resume_early = qcom_wdt_pet_resume,
#endif
};

static const struct of_device_id hh_wdt_match_table[] = {
	{ .compatible = "qcom,hh-watchdog" },
	{}
};

static struct platform_driver hh_wdt_driver = {
	.probe = hh_wdt_probe,
	.remove = qcom_wdt_remove,
	.driver = {
		.name = "hh-watchdog",
		.pm = &hh_wdt_dev_pm_ops,
		.of_match_table = hh_wdt_match_table,
	},
};

static int __init init_watchdog(void)
{
	return platform_driver_register(&hh_wdt_driver);
}

#if IS_MODULE(CONFIG_HH_VIRT_WATCHDOG)
module_init(init_watchdog);
#else
pure_initcall(init_watchdog);
#endif

static __exit void exit_watchdog(void)
{
	platform_driver_unregister(&hh_wdt_driver);
}
module_exit(exit_watchdog);
MODULE_DESCRIPTION("QCOM Haven Watchdog Driver");
MODULE_LICENSE("GPL v2");
