subdir-ccflags-y += -Werror

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-mtk.o clk-pll.o clk-gate.o clk-mux.o clk-apmixed.o
obj-$(CONFIG_RESET_CONTROLLER) += reset.o
obj-$(CONFIG_COMMON_CLK_MT2701) += clk-mt2701.o
obj-$(CONFIG_COMMON_CLK_MT2701_BDPSYS) += clk-mt2701-bdp.o
obj-$(CONFIG_COMMON_CLK_MT2701_ETHSYS) += clk-mt2701-eth.o
obj-$(CONFIG_COMMON_CLK_MT2701_HIFSYS) += clk-mt2701-hif.o
obj-$(CONFIG_COMMON_CLK_MT2701_IMGSYS) += clk-mt2701-img.o
obj-$(CONFIG_COMMON_CLK_MT2701_MMSYS) += clk-mt2701-mm.o
obj-$(CONFIG_COMMON_CLK_MT2701_VDECSYS) += clk-mt2701-vdec.o
obj-$(CONFIG_COMMON_CLK_MT2712) += clk-mt2712.o clkchk-mt2712.o
obj-$(CONFIG_COMMON_CLK_MT2712_BDPSYS) += clk-mt2712-bdp.o
obj-$(CONFIG_COMMON_CLK_MT2712_IMGSYS) += clk-mt2712-img.o
obj-$(CONFIG_COMMON_CLK_MT2712_JPGDECSYS) += clk-mt2712-jpgdec.o
obj-$(CONFIG_COMMON_CLK_MT2712_MFGCFG) += clk-mt2712-mfg.o
obj-$(CONFIG_COMMON_CLK_MT2712_MMSYS) += clk-mt2712-mm.o
obj-$(CONFIG_COMMON_CLK_MT2712_VDECSYS) += clk-mt2712-vdec.o
obj-$(CONFIG_COMMON_CLK_MT2712_VENCSYS) += clk-mt2712-venc.o
obj-$(CONFIG_COMMON_CLK_MT3967) += clk-mt3967.o clk-mt3967-pg.o clk-mtk-v1.o clkchk-mt3967.o mt3967_clkmgr.o
obj-$(CONFIG_COMMON_CLK_MT6758) += clk-mt6758.o clk-mt6758-pg.o clk-mtk-v1.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clk-mt6765.o clk-mt6765-pg.o clk-mtk-v1.o
obj-$(CONFIG_COMMON_CLK_MT6757) += clk-mt6757.o clk-mt6757-pll.o clk-mt6757-pg.o clk-pll-v1.o clk-mtk-v1.o
obj-$(CONFIG_COMMON_CLK_MT6763) += clk-mt6763.o clk-mt6763-pg.o mt6763_clkmgr.o clk-mtk-v1.o
obj-$(CONFIG_COMMON_CLK_MT6761) += clk-mt6761.o clk-mt6761-pg.o clk-mtk-v1.o
obj-$(CONFIG_COMMON_CLK_MT6779) += clk-mt6779.o clk-mt6779-pg.o clk-mtk-v1.o mt6779_clkmgr.o
obj-$(CONFIG_COMMON_CLK_MT8135) += clk-mt8135.o
obj-$(CONFIG_COMMON_CLK_MT8163) += clk-mt8163.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clk-mt8173.o
obj-y += clk-bringup.o

ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clkdbg.o clkchk.o
obj-$(CONFIG_COMMON_CLK_MT2712) += clkdbg-mt2712.o
obj-$(CONFIG_COMMON_CLK_MT3967) += clkdbg-mt3967.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clkdbg_mt6765.o clkchk-mt6765.o
obj-$(CONFIG_COMMON_CLK_MT6763) += clkdbg_mt6763.o
obj-$(CONFIG_COMMON_CLK_MT6761) += clkdbg_mt6761.o clkchk-mt6761.o
obj-$(CONFIG_COMMON_CLK_MT6779) += clkdbg-mt6779.o clkchk-mt6779.o
obj-$(CONFIG_COMMON_CLK_MT8163) += clkdbg_mt8163.o clkchk_mt8163.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clkdbg_mt8173.o clkchk_mt8173.o
endif
