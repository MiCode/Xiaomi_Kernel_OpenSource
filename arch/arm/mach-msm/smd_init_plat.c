/* arch/arm/mach-msm/smd_init_plat.c
 *
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef CONFIG_OF
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/msm_iomap.h>
#include <mach/msm_ipc_logging.h>
#include <smd_private.h>

#define MODULE_NAME "msm_smd"
#define IPC_LOG(level, x...) do { \
	if (smd_log_ctx) \
		ipc_log_string(smd_log_ctx, x); \
	else \
		printk(level x); \
	} while (0)

#if defined(CONFIG_MSM_SMD_DEBUG)
#define SMD_DBG(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMD_DEBUG) \
			IPC_LOG(KERN_DEBUG, x);		\
	} while (0)

#define SMD_INFO(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMD_INFO)	\
			IPC_LOG(KERN_INFO, x);		\
	} while (0)
#else
#define SMD_DBG(x...) do { } while (0)
#define SMD_INFO(x...) do { } while (0)
#endif

#if defined(CONFIG_ARCH_QSD8X50) || defined(CONFIG_ARCH_MSM8X60) \
	|| defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_ARCH_FSM9XXX) \
	|| defined(CONFIG_ARCH_MSM9615)	|| defined(CONFIG_ARCH_APQ8064)
#define CONFIG_QDSP6 1
#endif

#if defined(CONFIG_ARCH_MSM8X60) || defined(CONFIG_ARCH_MSM8960) \
	|| defined(CONFIG_ARCH_APQ8064)
#define CONFIG_DSPS 1
#endif

#if defined(CONFIG_ARCH_MSM8960) \
	|| defined(CONFIG_ARCH_APQ8064)
#define CONFIG_WCNSS 1
#define CONFIG_DSPS_SMSM 1
#endif

#if defined(CONFIG_ARCH_MSM7X30)
#define MSM_CFG_A2M_SMD_INT     \
		(smd_cfg_smd_intr(SMD_MODEM, 1 << 0, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2Q6_SMD_INT    \
		(smd_cfg_smd_intr(SMD_Q6, 1 << 8, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2M_SMSM_INT    \
		(smd_cfg_smsm_intr(SMD_MODEM, 1 << 5, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2Q6_SMSM_INT   \
		(smd_cfg_smsm_intr(SMD_Q6, 1 << 8, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2DSPS_SMD_INT
#define MSM_CFG_A2DSPS_SMSM_INT
#define MSM_CFG_A2WCNSS_SMD_INT
#define MSM_CFG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM8X60)
#define MSM_CFG_A2M_SMD_INT     \
		(smd_cfg_smd_intr(SMD_MODEM, 1 << 3, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2Q6_SMD_INT    \
		(smd_cfg_smd_intr(SMD_Q6, 1 << 15, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2M_SMSM_INT    \
		(smd_cfg_smsm_intr(SMD_MODEM, 1 << 4, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2Q6_SMSM_INT   \
		(smd_cfg_smsm_intr(SMD_Q6, 1 << 14, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2DSPS_SMD_INT  \
		(smd_cfg_smd_intr(SMD_DSPS, 1,		\
					MSM_SIC_NON_SECURE_BASE + 0x4080))
#define MSM_CFG_A2DSPS_SMSM_INT
#define MSM_CFG_A2WCNSS_SMD_INT
#define MSM_CFG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM9615)
#define MSM_CFG_A2M_SMD_INT     \
		(smd_cfg_smd_intr(SMD_MODEM, 1 << 3, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2Q6_SMD_INT    \
		(smd_cfg_smd_intr(SMD_Q6, 1 << 15, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2M_SMSM_INT    \
		(smd_cfg_smsm_intr(SMD_MODEM, 1 << 4, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2Q6_SMSM_INT   \
		(smd_cfg_smsm_intr(SMD_Q6, 1 << 14, MSM_APCS_GCC_BASE + 0x8))
#define MSM_CFG_A2DSPS_SMD_INT
#define MSM_CFG_A2DSPS_SMSM_INT
#define MSM_CFG_A2WCNSS_SMD_INT
#define MSM_CFG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_FSM9XXX)
#define MSM_CFG_A2Q6_SMD_INT	\
		(smd_cfg_smd_intr(SMD_Q6, 1 << 10, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2Q6_SMSM_INT	\
		(smd_cfg_smsm_intr(SMD_Q6, 1 << 10, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2M_SMD_INT	\
		(smd_cfg_smd_intr(SMD_MODEM, 1 << 0, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2M_SMSM_INT	\
		(smd_cfg_smsm_intr(SMD_MODEM, 1 << 5, MSM_GCC_BASE + 0x8))
#define MSM_CFG_A2DSPS_SMD_INT
#define MSM_CFG_A2DSPS_SMSM_INT
#define MSM_CFG_A2WCNSS_SMD_INT
#define MSM_CFG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM7X01A) || defined(CONFIG_ARCH_MSM7x25)
#define MSM_CFG_A2M_SMD_INT     \
		(smd_cfg_smd_intr(SMD_MODEM, 1, MSM_CSR_BASE + 0x400 + (0) * 4))
#define MSM_CFG_A2Q6_SMD_INT
#define MSM_CFG_A2M_SMSM_INT    \
		(smd_cfg_smsm_intr(SMD_MODEM, 1,	\
					MSM_CSR_BASE + 0x400 + (5) * 4))
#define MSM_CFG_A2Q6_SMSM_INT
#define MSM_CFG_A2DSPS_SMD_INT
#define MSM_CFG_A2DSPS_SMSM_INT
#define MSM_CFG_A2WCNSS_SMD_INT
#define MSM_CFG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM7X27) || defined(CONFIG_ARCH_MSM7X27A)
#define MSM_CFG_A2M_SMD_INT     \
		(smd_cfg_smd_intr(SMD_MODEM, 1, MSM_CSR_BASE + 0x400 + (0) * 4))
#define MSM_CFG_A2Q6_SMD_INT
#define MSM_CFG_A2M_SMSM_INT    \
		(smd_cfg_smsm_intr(SMD_MODEM, 1,	\
					MSM_CSR_BASE + 0x400 + (5) * 4))
#define MSM_CFG_A2Q6_SMSM_INT
#define MSM_CFG_A2DSPS_SMD_INT
#define MSM_CFG_A2DSPS_SMSM_INT
#define MSM_CFG_A2WCNSS_SMD_INT
#define MSM_CFG_A2WCNSS_SMSM_INT
#else /* use platform device / device tree configuration */
#define MSM_CFG_A2M_SMD_INT
#define MSM_CFG_A2Q6_SMD_INT
#define MSM_CFG_A2M_SMSM_INT
#define MSM_CFG_A2Q6_SMSM_INT
#define MSM_CFG_A2DSPS_SMD_INT
#define MSM_CFG_A2DSPS_SMSM_INT
#define MSM_CFG_A2WCNSS_SMD_INT
#define MSM_CFG_A2WCNSS_SMSM_INT
#endif

/*
 * stub out legacy macros if they are not being used so that the legacy
 * code compiles even though it is not used
 *
 * these definitions should not be used in active code and will cause
 * an early failure
 */
#ifndef INT_A9_M2A_0
#define INT_A9_M2A_0 -1
#endif
#ifndef INT_A9_M2A_5
#define INT_A9_M2A_5 -1
#endif
#ifndef INT_ADSP_A11
#define INT_ADSP_A11 -1
#endif
#ifndef INT_ADSP_A11_SMSM
#define INT_ADSP_A11_SMSM -1
#endif
#ifndef INT_DSPS_A11
#define INT_DSPS_A11 -1
#endif
#ifndef INT_DSPS_A11_SMSM
#define INT_DSPS_A11_SMSM -1
#endif
#ifndef INT_WCNSS_A11
#define INT_WCNSS_A11 -1
#endif
#ifndef INT_WCNSS_A11_SMSM
#define INT_WCNSS_A11_SMSM -1
#endif

static int intr_init(struct interrupt_config_item *private_irq,
			struct smd_irq_config *platform_irq,
			struct platform_device *pdev
			)
{
	int irq_id;
	int ret;
	int ret_wake;

	private_irq->out_bit_pos = platform_irq->out_bit_pos;
	private_irq->out_offset = platform_irq->out_offset;
	private_irq->out_base = platform_irq->out_base;

	irq_id = platform_get_irq_byname(
					pdev,
					platform_irq->irq_name
				);
	SMD_DBG("smd: %s: register irq: %s id: %d\n", __func__,
				platform_irq->irq_name, irq_id);
	ret = request_irq(irq_id,
				private_irq->irq_handler,
				platform_irq->flags,
				platform_irq->device_name,
				(void *)platform_irq->dev_id
			);
	if (ret < 0) {
		platform_irq->irq_id = ret;
		private_irq->irq_id = ret;
	} else {
		platform_irq->irq_id = irq_id;
		private_irq->irq_id = irq_id;
		ret_wake = enable_irq_wake(irq_id);
		if (ret_wake < 0) {
			pr_err("smd: enable_irq_wake failed on %s",
					platform_irq->irq_name);
		}
	}

	return ret;
}

int smd_core_init(void)
{
	int r;
	unsigned long flags = IRQF_TRIGGER_RISING;
	SMD_INFO("smd_core_init()\n");

	MSM_CFG_A2M_SMD_INT;
	MSM_CFG_A2Q6_SMD_INT;
	MSM_CFG_A2M_SMSM_INT;
	MSM_CFG_A2Q6_SMSM_INT;
	MSM_CFG_A2DSPS_SMD_INT;
	MSM_CFG_A2DSPS_SMSM_INT;
	MSM_CFG_A2WCNSS_SMD_INT;
	MSM_CFG_A2WCNSS_SMSM_INT;

	r = request_irq(INT_A9_M2A_0, smd_modem_irq_handler,
			flags, "smd_dev", 0);
	if (r < 0)
		return r;
	interrupt_stats[SMD_MODEM].smd_interrupt_id = INT_A9_M2A_0;
	r = enable_irq_wake(INT_A9_M2A_0);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_A9_M2A_0\n",
			__func__);

	r = request_irq(INT_A9_M2A_5, smsm_modem_irq_handler,
			flags, "smsm_dev", 0);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		return r;
	}
	interrupt_stats[SMD_MODEM].smsm_interrupt_id = INT_A9_M2A_5;
	r = enable_irq_wake(INT_A9_M2A_5);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_A9_M2A_5\n",
			__func__);

#if defined(CONFIG_QDSP6)
#if (INT_ADSP_A11 == INT_ADSP_A11_SMSM)
		flags |= IRQF_SHARED;
#endif
	r = request_irq(INT_ADSP_A11, smd_dsp_irq_handler,
			flags, "smd_dev", smd_dsp_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		return r;
	}

	interrupt_stats[SMD_Q6].smd_interrupt_id = INT_ADSP_A11;
	r = request_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler,
			flags, "smsm_dev", smsm_dsp_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		return r;
	}

	interrupt_stats[SMD_Q6].smsm_interrupt_id = INT_ADSP_A11_SMSM;
	r = enable_irq_wake(INT_ADSP_A11);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_ADSP_A11\n",
			__func__);

#if (INT_ADSP_A11 != INT_ADSP_A11_SMSM)
	r = enable_irq_wake(INT_ADSP_A11_SMSM);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_ADSP_A11_SMSM\n",
			__func__);
#endif
	flags &= ~IRQF_SHARED;
#endif

#if defined(CONFIG_DSPS)
	r = request_irq(INT_DSPS_A11, smd_dsps_irq_handler,
			flags, "smd_dev", smd_dsps_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		return r;
	}

	interrupt_stats[SMD_DSPS].smd_interrupt_id = INT_DSPS_A11;
	r = enable_irq_wake(INT_DSPS_A11);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_ADSP_A11\n",
			__func__);
#endif

#if defined(CONFIG_WCNSS)
	r = request_irq(INT_WCNSS_A11, smd_wcnss_irq_handler,
			flags, "smd_dev", smd_wcnss_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		return r;
	}

	interrupt_stats[SMD_WCNSS].smd_interrupt_id = INT_WCNSS_A11;
	r = enable_irq_wake(INT_WCNSS_A11);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_WCNSS_A11\n",
			__func__);

	r = request_irq(INT_WCNSS_A11_SMSM, smsm_wcnss_irq_handler,
			flags, "smsm_dev", smsm_wcnss_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		free_irq(INT_WCNSS_A11, smd_wcnss_irq_handler);
		return r;
	}

	interrupt_stats[SMD_WCNSS].smsm_interrupt_id = INT_WCNSS_A11_SMSM;
	r = enable_irq_wake(INT_WCNSS_A11_SMSM);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_WCNSS_A11_SMSM\n",
			__func__);
#endif

#if defined(CONFIG_DSPS_SMSM)
	r = request_irq(INT_DSPS_A11_SMSM, smsm_dsps_irq_handler,
			flags, "smsm_dev", smsm_dsps_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		free_irq(INT_WCNSS_A11, smd_wcnss_irq_handler);
		free_irq(INT_WCNSS_A11_SMSM, smsm_wcnss_irq_handler);
		return r;
	}

	interrupt_stats[SMD_DSPS].smsm_interrupt_id = INT_DSPS_A11_SMSM;
	r = enable_irq_wake(INT_DSPS_A11_SMSM);
	if (r < 0)
		pr_err("%s: enable_irq_wake failed for INT_DSPS_A11_SMSM\n",
			__func__);
#endif
	SMD_INFO("smd_core_init() done\n");

	return 0;
}

int smd_core_platform_init(struct platform_device *pdev)
{
	int i;
	int ret;
	uint32_t num_ss;
	struct smd_platform *smd_platform_data;
	struct smd_subsystem_config *smd_ss_config_list;
	struct smd_subsystem_config *cfg;
	struct interrupt_config *private_intr_config;
	int err_ret = 0;

	smd_platform_data = pdev->dev.platform_data;
	num_ss = smd_platform_data->num_ss_configs;
	smd_ss_config_list = smd_platform_data->smd_ss_configs;

	if (smd_platform_data->smd_ssr_config)
		disable_smsm_reset_handshake = smd_platform_data->
			   smd_ssr_config->disable_smsm_reset_handshake;

	for (i = 0; i < num_ss; i++) {
		cfg = &smd_ss_config_list[i];
		private_intr_config = smd_get_intr_config(cfg->edge);
		if (!private_intr_config) {
			pr_err("%s: invalid edge\n", __func__);
			goto intr_failed;
		}

		ret = intr_init(
			&private_intr_config->smd,
			&cfg->smd_int,
			pdev
			);

		if (ret < 0) {
			err_ret = ret;
			pr_err("smd: register irq failed on %s\n",
				cfg->smd_int.irq_name);
			goto intr_failed;
		}

		interrupt_stats[cfg->irq_config_id].smd_interrupt_id
						 = cfg->smd_int.irq_id;
		/* only init smsm structs if this edge supports smsm */
		if (cfg->smsm_int.irq_id)
			ret = intr_init(
				&private_intr_config->smsm,
				&cfg->smsm_int,
				pdev
				);

		if (ret < 0) {
			err_ret = ret;
			pr_err("smd: register irq failed on %s\n",
				cfg->smsm_int.irq_name);
			goto intr_failed;
		}

		if (cfg->smsm_int.irq_id)
			interrupt_stats[cfg->irq_config_id].smsm_interrupt_id
						 = cfg->smsm_int.irq_id;
		if (cfg->subsys_name)
			smd_set_edge_subsys_name(cfg->edge, cfg->subsys_name);

		smd_set_edge_initialized(cfg->edge);
	}

	SMD_INFO("smd_core_platform_init() done\n");

	return 0;

intr_failed:
	pr_err("smd: deregistering IRQs\n");
	for (i = 0; i < num_ss; ++i) {
		cfg = &smd_ss_config_list[i];

		if (cfg->smd_int.irq_id >= 0)
			free_irq(cfg->smd_int.irq_id,
				(void *)cfg->smd_int.dev_id
				);
		if (cfg->smsm_int.irq_id >= 0)
			free_irq(cfg->smsm_int.irq_id,
				(void *)cfg->smsm_int.dev_id
				);
	}
	return err_ret;
}

static int msm_smd_probe_legacy(struct platform_device *pdev)
{
	int ret;

	if (!smem_initialized_check())
		return -ENODEV;

	SMD_INFO("smd probe\n");
	if (pdev) {
		if (pdev->dev.of_node) {
			pr_err("%s: invalid device tree init\n", __func__);
			return -ENODEV;
		} else if (pdev->dev.platform_data) {
			ret = smd_core_platform_init(pdev);
			if (ret) {
				pr_err(
				"SMD: smd_core_platform_init() failed\n");
				return -ENODEV;
			}
		} else {
			ret = smd_core_init();
			if (ret) {
				pr_err("smd_core_init() failed\n");
				return -ENODEV;
			}
		}
	} else {
		pr_err("SMD: PDEV not found\n");
		return -ENODEV;
	}

	ret = smsm_post_init();
	if (ret) {
		pr_err("smd_post_init() failed ret = %d\n", ret);
		return ret;
	}
	smd_post_init(1, 0);

	return 0;
}

static struct platform_driver msm_smd_driver_legacy = {
	.probe = msm_smd_probe_legacy,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

int msm_smd_driver_register(void)
{

	int rc;

	rc = platform_driver_register(&msm_smd_driver_legacy);
	if (rc) {
		pr_err("%s: smd_driver register failed %d\n",
			__func__, rc);
		return rc;
	}
	return 0;
}
EXPORT_SYMBOL(msm_smd_driver_register);

MODULE_DESCRIPTION("MSM SMD Legacy/Platform Device Init");
MODULE_LICENSE("GPL v2");
#endif
