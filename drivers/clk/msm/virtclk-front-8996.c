/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk/msm-clock-generic.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <dt-bindings/clock/msm-clocks-8996.h>

static struct virtclk_front gcc_blsp1_ahb_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup1_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup1_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart1_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup2_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup2_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart2_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup3_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup3_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart3_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup4_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup4_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart4_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup5_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup5_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart5_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart5_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup6_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup6_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart6_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart6_apps_clk.c),
	},
};


static struct virtclk_front gcc_blsp2_ahb_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_ahb_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup1_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup1_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup1_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup1_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart1_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart1_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart1_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup2_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup2_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup2_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup2_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart2_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart2_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart2_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup3_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup3_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup3_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup3_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart3_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart3_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart3_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup4_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup4_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup4_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup4_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart4_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart4_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart4_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup5_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup5_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup5_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup5_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart5_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart5_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart5_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup6_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup6_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup6_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup6_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart6_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart6_apps_clk.c),
	},
};

static struct virtclk_front gcc_sdcc2_ahb_clk = {
	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_sdcc2_ahb_clk.c),
	},
};

static struct virtclk_front gcc_sdcc2_apps_clk = {
	.c = {
		.dbg_name = "gcc_sdcc2_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_sdcc2_apps_clk.c),
	},
};

static struct clk_lookup msm_clocks_8996[] = {
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart1_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart2_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart3_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart4_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart5_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart6_apps_clk),
	CLK_LIST(gcc_blsp2_ahb_clk),
	CLK_LIST(gcc_blsp2_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart1_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart2_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart3_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart4_apps_clk),
	CLK_LIST(gcc_blsp2_qup5_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup5_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart5_apps_clk),
	CLK_LIST(gcc_blsp2_qup6_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup6_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart6_apps_clk),
	CLK_LIST(gcc_sdcc2_ahb_clk),
	CLK_LIST(gcc_sdcc2_apps_clk),
};

static const struct of_device_id msm8996_virtclk_front_match_table[] = {
	{ .compatible = "qcom,virtclk-frontend-8996" },
	{}
};

static int msm8996_virtclk_front_probe(struct platform_device *pdev)
{
	return msm_virtclk_front_probe(pdev, msm_clocks_8996,
			ARRAY_SIZE(msm_clocks_8996));
}

static struct platform_driver msm8996_virtclk_front_driver = {
	.probe = msm8996_virtclk_front_probe,
	.driver = {
		.name = "virtclk-front-8996",
		.of_match_table = msm8996_virtclk_front_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm8996_virtclk_front_init(void)
{
	return platform_driver_register(&msm8996_virtclk_front_driver);
}
arch_initcall(msm8996_virtclk_front_init);
