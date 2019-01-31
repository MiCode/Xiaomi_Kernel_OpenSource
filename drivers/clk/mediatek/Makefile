subdir-ccflags-y += -Werror

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-mtk.o clk-pll.o clk-gate.o clk-apmixed.o
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
obj-$(CONFIG_COMMON_CLK_MT6758) += clk-mt6758.o clk-mt6758-pg.o clk-mtk-v1.o clk-mux.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clk-mt6765.o clk-mt6765-pg.o clk-mtk-v1.o clk-mux.o mt6765_clkmgr.o
obj-$(CONFIG_COMMON_CLK_MT8135) += clk-mt8135.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clk-mt8173.o
obj-y += clk-bringup.o

ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clkdbg.o
obj-$(CONFIG_COMMON_CLK_MT2712) += clkdbg-mt2712.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clkdbg_mt6765.o clkchk-mt6765.o
endif
