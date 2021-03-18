// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <media/rc-core.h>

#include <mt-plat/mtk_pwm.h>
#include <mach/mtk_pwm_hal.h>

#define DRIVER_NAME	"mtk-pwm-ir-tx"
#define DEVICE_NAME	"MTK PWM IR Transmitter"
#define IRTX_PWM_CLOCK (26000000)
//#define IRTX_DEBUG

struct mtk_pwm_ir {
	struct regulator *regulator;
	unsigned int pwm_ch;
	unsigned int pwm_data_invert;
	unsigned int carrier;
	unsigned int duty_cycle;
	unsigned int cycle;
	struct platform_device *pdev;
};

static struct pwm_spec_config irtx_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_MEMORY,
	.clk_div = CLK_DIV1,
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
	/* 1 microseconds, assume clock source is 26M */
	.PWM_MODE_MEMORY_REGS.HDURATION = 229,
	.PWM_MODE_MEMORY_REGS.LDURATION = 229,
	.PWM_MODE_MEMORY_REGS.GDURATION = 0,
	.PWM_MODE_MEMORY_REGS.WAVE_NUM = 1,
};

static int mtk_pwm_ir_tx(struct rc_dev *rcdev, unsigned int *txbuf,
			 unsigned int count)
{
	struct mtk_pwm_ir *pwm_ir = rcdev->priv;
	dma_addr_t wave_phy;
	unsigned int *wave_vir;
	int ret, i, h_l_period, cycle_unit_us;
	int buf_size = 0;
	int total_time = 0;
	int len = 0;
	int cur_bit = 0;
	int regulator_enabled = 0;
#ifdef IRTX_DEBUG
	static char logbuf[4096];
	int cur_idx = 0;
	char *dbglog = logbuf;
#endif

	pr_info("%s() irtx len=0x%x, pwm=%d\n", __func__,
		(unsigned int)count, (unsigned int)pwm_ir->pwm_ch);

	/* lirc txbuf is odd, the last one is null appeneded by userspace */
	if (--count == 0)
		return 0;

	// pwm_ir.cycle: whole cycle,  pwm_ir.duty_cycle: high period
	h_l_period = DIV_ROUND_UP(IRTX_PWM_CLOCK*pwm_ir->duty_cycle,
			pwm_ir->carrier*pwm_ir->cycle);
	cycle_unit_us = DIV_ROUND_UP(NSEC_PER_SEC/1000*pwm_ir->duty_cycle,
			pwm_ir->carrier*pwm_ir->cycle);

	for (i = 0; i < count; i++) {
		buf_size += ALIGN(DIV_ROUND_UP(txbuf[i], cycle_unit_us), pwm_ir->cycle);
		total_time += txbuf[i];
	}

	buf_size = ALIGN(buf_size, BITS_PER_BYTE * sizeof(unsigned int));
	buf_size = buf_size / BITS_PER_BYTE; /* byte size */

	wave_vir = (unsigned int *) dma_alloc_coherent(&pwm_ir->pdev->dev, buf_size,
		&wave_phy, GFP_KERNEL);
	if (!wave_vir) {
		pr_notice("%s() IRTX alloc memory fail\n", __func__);
		return -ENOMEM;
	}
	memset(wave_vir, 0, buf_size);
	/* convert the pulse/space signal to raw binary signal */
	cur_bit = 0;
	for (i = 0; i < count; i++) {
		unsigned int periods;
		int j, cur_cycle = 0;

		periods = ALIGN(DIV_ROUND_UP(txbuf[i], cycle_unit_us), pwm_ir->cycle);

		for (j = 0; j < periods; j++) {
			cur_cycle = (j % pwm_ir->cycle)+1;
			if (cur_cycle > pwm_ir->duty_cycle  || (i % 2)) {
				if (pwm_ir->pwm_data_invert)
					wave_vir[len] |= (1 << cur_bit);
				else
					wave_vir[len] &= ~(1 << cur_bit);
			} else {
				if (pwm_ir->pwm_data_invert)
					wave_vir[len] &= ~(1 << cur_bit);
				else
					wave_vir[len] |= (1 << cur_bit);
			}
			cur_bit++;
			if (cur_bit == 32) {
				cur_bit = 0;
				len++;
			}
		}
	}
	if (cur_bit > 0)
		len++;

	irtx_pwm_config.PWM_MODE_MEMORY_REGS.HDURATION = h_l_period-1;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.LDURATION = h_l_period-1;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = wave_phy;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = len;

#ifdef IRTX_DEBUG
	dbglog = logbuf;
	pr_info("h_l_period = %d, cycle_unit_us = %d\n",
		h_l_period, cycle_unit_us);
	pr_info("irtx len = %d, buf_size = %d, total_time = %d\n",
		len, buf_size, total_time);
	for (i = 0; i < len; i++) {
		if (i && (i % 16 == 0)) {
			pr_info("[%d] %s\n", cur_idx++, logbuf);
			memset(logbuf, 0, sizeof(logbuf));
			dbglog = logbuf;
		}
		dbglog += sprintf(dbglog, "0x%08x ", wave_vir[i]);
	}
	if (dbglog != logbuf)
		pr_info("[%d] %s\n", cur_idx++, logbuf);
#endif

	if (pwm_ir->regulator != NULL) {
		if (!regulator_is_enabled(pwm_ir->regulator)) {
			ret = regulator_enable(pwm_ir->regulator);
			if (ret < 0) {
				pr_err("%s:%d regulator_enable fail!\n",
						__func__, __LINE__);
				goto exit_free;
			} else {
				regulator_enabled = 1;
			}
		}
	}

	ret = pwm_set_spec_config(&irtx_pwm_config);
	if (ret < 0) {
		pr_err("pwm_set_spec_config fail, ret: %d\n", ret);
		goto exit_free;
	}

	usleep_range(total_time, total_time + 100);

	pr_info("[IRTX] done, clean up\n");
	mt_pwm_disable(irtx_pwm_config.pwm_no, irtx_pwm_config.pmic_pad);

	if (pwm_ir->regulator != NULL) {
		if (regulator_enabled && regulator_is_enabled(pwm_ir->regulator)) {
			ret = regulator_disable(pwm_ir->regulator);
			if (ret < 0) {
				pr_err("%s:%d regulator_disable fail!\n",
					__func__, __LINE__);
				goto exit_free;
			}
		}
	}
	ret = count;

exit_free:
	dma_free_coherent(&pwm_ir->pdev->dev, buf_size, wave_vir, wave_phy);
	return ret;
}

static int mtk_pwm_ir_set_duty_cycle(struct rc_dev *dev, u32 duty_cycle)
{
	struct mtk_pwm_ir *pwm_ir = dev->priv;

	if (duty_cycle < 40) {
		pwm_ir->cycle = 3;
		pwm_ir->duty_cycle = 1;
	}

	return 0;
}

static int mtk_pwm_ir_set_carrier(struct rc_dev *dev, u32 carrier)
{
	struct mtk_pwm_ir *pwm_ir = dev->priv;

	if (!carrier)
		return -EINVAL;

	pwm_ir->carrier = carrier;

	return 0;
}

static int mtk_pwm_ir_probe(struct platform_device *pdev)
{
	struct mtk_pwm_ir *pwm_ir;
	struct rc_dev *rcdev;
	int rc;

	pwm_ir = devm_kmalloc(&pdev->dev, sizeof(*pwm_ir), GFP_KERNEL);
	if (!pwm_ir)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node, "pwm_ch",
		&pwm_ir->pwm_ch);
	of_property_read_u32(pdev->dev.of_node, "pwm_data_invert",
		&pwm_ir->pwm_data_invert);

	pwm_ir->regulator = devm_regulator_get(&pdev->dev, "vio28");
	if (IS_ERR(pwm_ir->regulator))
		return PTR_ERR(pwm_ir->regulator);

	pwm_ir->pdev = pdev;

	rc = regulator_set_voltage(pwm_ir->regulator, 2800000, 2800000);
	if (rc < 0)
		return rc;

	rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW_TX);
	if (!rcdev)
		return -ENOMEM;

	rcdev->priv = pwm_ir;
	rcdev->driver_name = DRIVER_NAME;
	rcdev->device_name = DEVICE_NAME;
	rcdev->tx_ir = mtk_pwm_ir_tx;
	rcdev->s_tx_duty_cycle = mtk_pwm_ir_set_duty_cycle;
	rcdev->s_tx_carrier = mtk_pwm_ir_set_carrier;

	rc = devm_rc_register_device(&pdev->dev, rcdev);
	if (rc < 0)
		dev_err(&pdev->dev, "failed to register rc device\n");

	return rc;
}

static const struct of_device_id mtk_pwm_ir_of_match[] = {
	{.compatible = "mediatek,irtx-pwm",},
	{}
};

static struct platform_driver pwm_ir_driver = {
	.probe = mtk_pwm_ir_probe,
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(mtk_pwm_ir_of_match),
	},
};

module_platform_driver(pwm_ir_driver);

MODULE_DESCRIPTION("MTK PWM IR Transmitter");
MODULE_AUTHOR("Chang-An Chen <chang-an.chen@mediatek.com>");
MODULE_LICENSE("GPL");
