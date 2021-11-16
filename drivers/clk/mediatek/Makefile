# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/drivers/devfreq/

obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-fixup-div.o clk-mtk.o clk-pll.o clk-gate.o clk-apmixed.o clk-cpumux.o clk-mux.o
obj-$(CONFIG_RESET_CONTROLLER) += reset.o
ifneq ($(CONFIG_FPGA_EARLY_PORTING), y)
obj-$(CONFIG_COMMON_CLK_MT6785) += clk-mt6785.o clk-mt6785-pg.o clk-mtk-v1.o clkdbg.o clkchk.o clkdbg-mt6785.o clkchk-mt6785.o
obj-$(CONFIG_COMMON_CLK_MT6873) += clk-mt6873.o clk-mt6873-pg.o clk-mtk-v1.o clkdbg.o clkdbg-mt6873.o clkchk.o clkchk-mt6873.o clk-fmeter-mt6873.o
obj-$(CONFIG_COMMON_CLK_MT6885) += clk-mt6885.o clk-mt6885-pg.o clk-mtk-v1.o clkdbg.o clkdbg-mt6885.o clkchk.o clkchk-mt6885.o clk-fmeter-mt6885.o
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-mt6885.o clk-mt6885-pg.o clk-mtk-v1.o clkdbg.o clkdbg-mt6885.o clkchk.o clkchk-mt6885.o clk-fmeter-mt6885.o
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-mt6833.o clk-mt6833-pg.o clk-mtk-v1.o clkdbg.o clkdbg-mt6833.o clkchk.o clkchk-mt6833.o clk-bringup.o  clk-fmeter-mt6833.o
obj-$(CONFIG_COMMON_CLK_MT6833_AUDIO) += clk-mt6833-audsys.o
obj-$(CONFIG_COMMON_CLK_MT6833_CAMSYS_MAIN) += clk-mt6833-cam_m.o
obj-$(CONFIG_COMMON_CLK_MT6833_CAMSYS_RAWA) += clk-mt6833-cam_ra.o
obj-$(CONFIG_COMMON_CLK_MT6833_CAMSYS_RAWB) += clk-mt6833-cam_rb.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMGSYS1) += clk-mt6833-imgsys1.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMGSYS2) += clk-mt6833-imgsys2.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMP_IIC_WRAP_C) += clk-mt6833-impc.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMP_IIC_WRAP_E) += clk-mt6833-impe.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMP_IIC_WRAP_N) += clk-mt6833-impn.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMP_IIC_WRAP_S) += clk-mt6833-imps.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMP_IIC_WRAP_W) += clk-mt6833-impw.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMP_IIC_WRAP_WS) += clk-mt6833-impws.o
obj-$(CONFIG_COMMON_CLK_MT6833_IPESYS) += clk-mt6833-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6833_MDPSYS_CONFIG) += clk-mt6833-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6833_MFGSYS) += clk-mt6833-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6833_MMSYS_CONFIG) += clk-mt6833-mm.o
obj-$(CONFIG_COMMON_CLK_MT6833_VDEC_GCON) += clk-mt6833-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6833_VENC_GCON) += clk-mt6833-venc.o
endif
obj-$(CONFIG_COMMON_CLK_MT6797) += clk-mt6797.o
obj-$(CONFIG_COMMON_CLK_MT6797_IMGSYS) += clk-mt6797-img.o
obj-$(CONFIG_COMMON_CLK_MT6797_MMSYS) += clk-mt6797-mm.o
obj-$(CONFIG_COMMON_CLK_MT6797_VDECSYS) += clk-mt6797-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6797_VENCSYS) += clk-mt6797-venc.o
obj-$(CONFIG_COMMON_CLK_MT2701) += clk-mt2701.o
obj-$(CONFIG_COMMON_CLK_MT2701_BDPSYS) += clk-mt2701-bdp.o
obj-$(CONFIG_COMMON_CLK_MT2701_ETHSYS) += clk-mt2701-eth.o
obj-$(CONFIG_COMMON_CLK_MT2701_HIFSYS) += clk-mt2701-hif.o
obj-$(CONFIG_COMMON_CLK_MT2701_IMGSYS) += clk-mt2701-img.o
obj-$(CONFIG_COMMON_CLK_MT2701_MMSYS) += clk-mt2701-mm.o
obj-$(CONFIG_COMMON_CLK_MT2701_VDECSYS) += clk-mt2701-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6771) += clk-mt6771.o clk-mt6771-pg.o clk-mtk-v1.o clkdbg.o
obj-$(CONFIG_COMMON_CLK_MT6768) += clk-mt6768.o clk-mtk-v1.o clk-mt6768-pg.o clkdbg.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clk-mt6765.o clk-mt6765-pg.o clk-mtk-v1.o clkdbg.o
obj-$(CONFIG_COMMON_CLK_MT8135) += clk-mt8135.o
obj-$(CONFIG_COMMON_CLK_MT8167) += clk-mt8167.o clkchk_mt8167.o clkdbg.o clkdbg_mt8167.o
obj-$(CONFIG_COMMON_CLK_MT8168) += clk-mt8168.o clk-bringup.o clkdbg.o clkdbg-mt8168.o clkchk-mt8168.o
obj-$(CONFIG_COMMON_CLK_MT8168_APU) += clk-mt8168-apu.o
obj-$(CONFIG_COMMON_CLK_MT8168_CAMSYS) += clk-mt8168-cam.o
obj-$(CONFIG_COMMON_CLK_MT8168_MFGCFG) += clk-mt8168-mfg.o
obj-$(CONFIG_COMMON_CLK_MT8168_MMSYS) += clk-mt8168-mm.o
obj-$(CONFIG_COMMON_CLK_MT8168_VDECSYS) += clk-mt8168-vdec.o
obj-$(CONFIG_COMMON_CLK_MT8168_VENCSYS) += clk-mt8168-venc.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clk-mt8173.o
obj-$(CONFIG_COMMON_CLK_MT8183) += clk-mt8183.o clk-bringup.o clkdbg.o clkdbg-mt8183.o
obj-$(CONFIG_COMMON_CLK_MT8183_AUDIOSYS) += clk-mt8183-audio.o
obj-$(CONFIG_COMMON_CLK_MT8183_CAMSYS) += clk-mt8183-cam.o
obj-$(CONFIG_COMMON_CLK_MT8183_IMGSYS) += clk-mt8183-img.o
obj-$(CONFIG_COMMON_CLK_MT8183_IPU_CORE0) += clk-mt8183-ipu0.o
obj-$(CONFIG_COMMON_CLK_MT8183_IPU_CORE1) += clk-mt8183-ipu1.o
obj-$(CONFIG_COMMON_CLK_MT8183_IPU_ADL) += clk-mt8183-ipu_adl.o
obj-$(CONFIG_COMMON_CLK_MT8183_IPU_CONN) += clk-mt8183-ipu_conn.o
obj-$(CONFIG_COMMON_CLK_MT8183_MFGCFG) += clk-mt8183-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT8183_MMSYS) += clk-mt8183-mm.o
obj-$(CONFIG_COMMON_CLK_MT8183_VDECSYS) += clk-mt8183-vdec.o
obj-$(CONFIG_COMMON_CLK_MT8183_VENCSYS) += clk-mt8183-venc.o
obj-$(CONFIG_COMMON_CLK_MT6739) += clk-mt6739.o clk-mt6739-pg.o mt6739_clkmgr.o clk-mtk-v1.o clk-fixed-factor.o
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-mt6853.o clk-mt6853-pg.o clk-mtk-v1.o clkdbg.o clkdbg-mt6853.o clkchk.o clkchk-mt6853.o clk-bringup.o  clk-fmeter-mt6853.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU0) += clk-mt6853-apu0.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU1) += clk-mt6853-apu1.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_VCORE) += clk-mt6853-apuv.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_CONN) += clk-mt6853-apuc.o
obj-$(CONFIG_COMMON_CLK_MT6853_AUDIO) += clk-mt6853-audsys.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_MAIN) += clk-mt6853-cam_m.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_RAWA) += clk-mt6853-cam_ra.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_RAWB) += clk-mt6853-cam_rb.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMGSYS1) += clk-mt6853-imgsys1.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMGSYS2) += clk-mt6853-imgsys2.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_C) += clk-mt6853-impc.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_E) += clk-mt6853-impe.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_N) += clk-mt6853-impn.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_S) += clk-mt6853-imps.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_W) += clk-mt6853-impw.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_WS) += clk-mt6853-impws.o
obj-$(CONFIG_COMMON_CLK_MT6853_IPESYS) += clk-mt6853-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6853_MDPSYS_CONFIG) += clk-mt6853-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6853_MFGSYS) += clk-mt6853-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6853_MMSYS_CONFIG) += clk-mt6853-mm.o
obj-$(CONFIG_COMMON_CLK_MT6853_SCP) += clk-mt6853-scp_par.o
obj-$(CONFIG_COMMON_CLK_MT6853_VDEC_GCON) += clk-mt6853-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6853_VENC_GCON) += clk-mt6853-venc.o

ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6768) +=  clkchk-mt6768.o clkdbg-mt6768.o mt6768_clkmgr.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clkdbg_mt6765.o clkchk-mt6765.o
endif
