#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

obj-$(CONFIG_COMMON_CLK_MEDIATEK_NEW) += clk-mtk.o clk-pll.o clk-gate.o clk-mux.o clk-cpumux.o clk-apmixed.o
obj-$(CONFIG_COMMON_CLK_MT2701) += clk-mt2701.o clk-mt2701-pg.o clk-hdmi-mt2701.o
obj-$(CONFIG_COMMON_CLK_MT8127) += clk-mt8127.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT8135) += clk-mt8135.o
obj-$(CONFIG_COMMON_CLK_MT8163) += clk-mt8163.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clk-mt8173.o
obj-$(CONFIG_RESET_CONTROLLER) += reset.o

ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MEDIATEK_NEW) += clkdbg.o
obj-$(CONFIG_COMMON_CLK_MT2701) += clkdbg_mt2701.o clkchk_mt2701.o
obj-$(CONFIG_COMMON_CLK_MT8127) += clkdbg_mt8127.o clkchk_mt8127.o
obj-$(CONFIG_COMMON_CLK_MT8163) += clkdbg_mt8163.o clkchk_mt8163.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clkdbg_mt8173.o clkchk_mt8173.o
endif

obj-$(CONFIG_COMMON_CLK_MEDIATEK_V1) += clk-mtk-v1.o clk-pll-v1.o clk-gate-v1.o clk-mux.o
obj-$(CONFIG_COMMON_CLK_MT6735) += clk-mt6735.o clk-mt6735-pll.o clk-mt6735-pg.o
obj-$(CONFIG_COMMON_CLK_MT6755) += clk-mt6755.o clk-mt6755-pll.o clk-mt6755-pg.o
obj-$(CONFIG_COMMON_CLK_MT6797) += clk-mt6797.o clk-mt6797-pll.o clk-mt6797-pg.o
obj-$(CONFIG_COMMON_CLK_MT6757) += clk-mt6757.o clk-mt6757-pll.o clk-mt6757-pg.o
obj-$(CONFIG_COMMON_CLK_ELBRUS) += clk-elbrus.o clk-elbrus-pll.o clk-elbrus-pg.o