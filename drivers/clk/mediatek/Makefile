# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-common.o
clk-common-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-mtk.o clk-pll.o clk-gate.o clk-apmixed.o clk-cpumux.o reset.o clk-mux.o clkchk.o clk-fmeter.o mtk-pd-chk.o clkdbg.o
obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-disable-unused.o

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6765.c),)
obj-$(CONFIG_COMMON_CLK_MT6765) += clk-mt6765.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clkchk-mt6765.o
clk-chk-mt6768-$(CONFIG_COMMON_CLK_MT6765) += clkchk-mt6765.o
obj-$(CONFIG_COMMON_CLK_MT6765) += mt6765_clkmgr.o
clk-clkmgr-mt6765-$(CONFIG_COMMON_CLK_MT6765) += mt6765_clkmgr.o
obj-$(CONFIG_COMMON_CLK_MT6765) += clk-fmeter-mt6765.o
clk-fmeter-mt6765-$(CONFIG_COMMON_CLK_MT6765) += clk-fmeter-mt6765.o
$(warning go this for clk CONFIG_COMMON_CLK_MT6765="$(CONFIG_COMMON_CLK_MT6765)")
$(warning go this for clk CONFIG_MTK_HANG_DETECT="$(CONFIG_MTK_HANG_DETECT)")

ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6765) += clkdbg-mt6765.o
clk-dbg-mt6765-$(CONFIG_COMMON_CLK_MT6765) += clkdbg-mt6765.o
endif
endif


ifneq ($(wildcard $(srctree)/$(src)/clk-mt6768.c),)
obj-$(CONFIG_COMMON_CLK_MT6768) += clk-mt6768.o
obj-$(CONFIG_COMMON_CLK_MT6768) += clkchk-mt6768.o
clk-chk-mt6768-$(CONFIG_COMMON_CLK_MT6768) += clkchk-mt6768.o
obj-$(CONFIG_COMMON_CLK_MT6768) += mt6768_clkmgr.o
clk-clkmgr-mt6768-$(CONFIG_COMMON_CLK_MT6768) += mt6768_clkmgr.o
obj-$(CONFIG_COMMON_CLK_MT6768) += clk-fmeter-mt6768.o
clk-fmeter-mt6768-$(CONFIG_COMMON_CLK_MT6768) += clk-fmeter-mt6768.o
$(warning go this for clk CONFIG_COMMON_CLK_MT6768="$(CONFIG_COMMON_CLK_MT6768)")
$(warning go this for clk CONFIG_MTK_HANG_DETECT="$(CONFIG_MTK_HANG_DETECT)")

ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6768) += clkdbg-mt6768.o
clk-dbg-mt6768-$(CONFIG_COMMON_CLK_MT6768) += clkdbg-mt6768.o
endif
endif

obj-$(CONFIG_COMMON_CLK_MT6779) += clk-mt6779.o
obj-$(CONFIG_COMMON_CLK_MT6779_MMSYS) += clk-mt6779-mm.o
obj-$(CONFIG_COMMON_CLK_MT6779_IMGSYS) += clk-mt6779-img.o
obj-$(CONFIG_COMMON_CLK_MT6779_IPESYS) += clk-mt6779-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6779_CAMSYS) += clk-mt6779-cam.o
obj-$(CONFIG_COMMON_CLK_MT6779_VDECSYS) += clk-mt6779-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6779_VENCSYS) += clk-mt6779-venc.o
obj-$(CONFIG_COMMON_CLK_MT6779_MFGCFG) += clk-mt6779-mfg.o
obj-$(CONFIG_COMMON_CLK_MT6779_AUDSYS) += clk-mt6779-aud.o
obj-$(CONFIG_COMMON_CLK_MT6797) += clk-mt6797.o
obj-$(CONFIG_COMMON_CLK_MT6797_IMGSYS) += clk-mt6797-img.o
obj-$(CONFIG_COMMON_CLK_MT6797_MMSYS) += clk-mt6797-mm.o
obj-$(CONFIG_COMMON_CLK_MT6797_VDECSYS) += clk-mt6797-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6797_VENCSYS) += clk-mt6797-venc.o
obj-$(CONFIG_COMMON_CLK_MT2701) += clk-mt2701.o
obj-$(CONFIG_COMMON_CLK_MT2701_AUDSYS) += clk-mt2701-aud.o
obj-$(CONFIG_COMMON_CLK_MT2701_BDPSYS) += clk-mt2701-bdp.o
obj-$(CONFIG_COMMON_CLK_MT2701_ETHSYS) += clk-mt2701-eth.o
obj-$(CONFIG_COMMON_CLK_MT2701_G3DSYS) += clk-mt2701-g3d.o
obj-$(CONFIG_COMMON_CLK_MT2701_HIFSYS) += clk-mt2701-hif.o
obj-$(CONFIG_COMMON_CLK_MT2701_IMGSYS) += clk-mt2701-img.o
obj-$(CONFIG_COMMON_CLK_MT2701_MMSYS) += clk-mt2701-mm.o
obj-$(CONFIG_COMMON_CLK_MT2701_VDECSYS) += clk-mt2701-vdec.o
obj-$(CONFIG_COMMON_CLK_MT2712) += clk-mt2712.o
obj-$(CONFIG_COMMON_CLK_MT2712_BDPSYS) += clk-mt2712-bdp.o
obj-$(CONFIG_COMMON_CLK_MT2712_IMGSYS) += clk-mt2712-img.o
obj-$(CONFIG_COMMON_CLK_MT2712_JPGDECSYS) += clk-mt2712-jpgdec.o
obj-$(CONFIG_COMMON_CLK_MT2712_MFGCFG) += clk-mt2712-mfg.o
obj-$(CONFIG_COMMON_CLK_MT2712_MMSYS) += clk-mt2712-mm.o
obj-$(CONFIG_COMMON_CLK_MT2712_VDECSYS) += clk-mt2712-vdec.o
obj-$(CONFIG_COMMON_CLK_MT2712_VENCSYS) += clk-mt2712-venc.o
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6789.c),)
obj-$(CONFIG_COMMON_CLK_MT6789) += clk-mt6789.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6789_AUDIOSYS) += clk-mt6789-audiosys.o
obj-$(CONFIG_COMMON_CLK_MT6789_CAM) += clk-mt6789-cam.o
obj-$(CONFIG_COMMON_CLK_MT6789_MMSYS) += clk-mt6789-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6789_DISP_DSC) += clk-mt6789-disp_dsc.o
obj-$(CONFIG_COMMON_CLK_MT6789_IMG) += clk-mt6789-img.o
obj-$(CONFIG_COMMON_CLK_MT6789_I2C) += clk-mt6789-i2c.o
obj-$(CONFIG_COMMON_CLK_MT6789_IMP_IIC_WRAP_EN) += clk-mt6789-impen.o
obj-$(CONFIG_COMMON_CLK_MT6789_BUS) += clk-mt6789-bus.o
obj-$(CONFIG_COMMON_CLK_MT6789_MFG) += clk-mt6789-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6789_VCODEC) += clk-mt6789-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6833.c),)
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-mt6833.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6833_AUDIOSYS) += clk-mt6833-audiosys.o
obj-$(CONFIG_COMMON_CLK_MT6833_CAM) += clk-mt6833-cam.o
obj-$(CONFIG_COMMON_CLK_MT6833_MMSYS) += clk-mt6833-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMG) += clk-mt6833-img.o
obj-$(CONFIG_COMMON_CLK_MT6833_I2C) += clk-mt6833-i2c.o
obj-$(CONFIG_COMMON_CLK_MT6833_BUS) += clk-mt6833-bus.o
obj-$(CONFIG_COMMON_CLK_MT6833_MFG) += clk-mt6833-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6833_VCODEC) += clk-mt6833-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6853.c),)
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-mt6853.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU0) += clk-mt6853-apu0.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU1) += clk-mt6853-apu1.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_VCORE) += clk-mt6853-apuv.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_CONN) += clk-mt6853-apuc.o
obj-$(CONFIG_COMMON_CLK_MT6853_AUDIOSYS) += clk-mt6853-audsys.o
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
obj-$(CONFIG_COMMON_CLK_MT6853_MFGCFG) += clk-mt6853-mfg.o
obj-$(CONFIG_COMMON_CLK_MT6853_MMSYS_CONFIG) += clk-mt6853-mm.o
obj-$(CONFIG_COMMON_CLK_MT6853_SCP_ADSP) += clk-mt6853-scp_par.o
obj-$(CONFIG_COMMON_CLK_MT6853_VDEC_GCON) += clk-mt6853-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6853_VENC_GCON) += clk-mt6853-venc.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6855.c),)
obj-$(CONFIG_COMMON_CLK_MT6855) += clk-mt6855.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6855_ADSP) += clk-mt6855-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6855_CAMSYS_MAIN) += clk-mt6855-cam_m.o
obj-$(CONFIG_COMMON_CLK_MT6855_CAM) += clk-mt6855-cam.o
obj-$(CONFIG_COMMON_CLK_MT6855_MMSYS) += clk-mt6855-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6855_IMGSYS1) += clk-mt6855-imgsys1.o
obj-$(CONFIG_COMMON_CLK_MT6855_IMGSYS2) += clk-mt6855-imgsys2.o
obj-$(CONFIG_COMMON_CLK_MT6855_PERI) += clk-mt6855-peri.o
obj-$(CONFIG_COMMON_CLK_MT6855_BUS) += clk-mt6855-bus.o
obj-$(CONFIG_COMMON_CLK_MT6855_IMG) += clk-mt6855-img.o
obj-$(CONFIG_COMMON_CLK_MT6855_MDPSYS1_CONFIG) += clk-mt6855-mdp1.o
obj-$(CONFIG_COMMON_CLK_MT6855_MDPSYS) += clk-mt6855-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6855_VCODEC) += clk-mt6855-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6879.c),)
obj-$(CONFIG_COMMON_CLK_MT6879) += clk-mt6879.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6879_ADSP) += clk-mt6879-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6879_CAM) += clk-mt6879-cam.o
obj-$(CONFIG_COMMON_CLK_MT6879_CCU) += clk-mt6879-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6879_IMG) += clk-mt6879-img.o
obj-$(CONFIG_COMMON_CLK_MT6879_MMSYS) += clk-mt6879-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6879_PERI) += clk-mt6879-peri.o
obj-$(CONFIG_COMMON_CLK_MT6879_BUS) += clk-mt6879-bus.o
obj-$(CONFIG_COMMON_CLK_MT6879_MDPSYS) += clk-mt6879-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6879_MFG) += clk-mt6879-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6879_VCODEC) += clk-mt6879-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6893.c),)
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-mt6893.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU0) += clk-mt6893-apu0.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU1) += clk-mt6893-apu1.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU2) += clk-mt6893-apu2.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_VCORE) += clk-mt6893-apuv.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_CONN) += clk-mt6893-apuc.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_MDLA0) += clk-mt6893-apum0.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_MDLA1) += clk-mt6893-apum1.o
obj-$(CONFIG_COMMON_CLK_MT6893_AUDIOSYS) += clk-mt6893-audsys.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_MAIN) += clk-mt6893-cam_m.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_RAWA) += clk-mt6893-cam_ra.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_RAWB) += clk-mt6893-cam_rb.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_RAWC) += clk-mt6893-cam_rc.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMGSYS1) += clk-mt6893-imgsys1.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMGSYS2) += clk-mt6893-imgsys2.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_C) += clk-mt6893-impc.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_E) += clk-mt6893-impe.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_N) += clk-mt6893-impn.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_S) += clk-mt6893-imps.o
obj-$(CONFIG_COMMON_CLK_MT6893_IPESYS) += clk-mt6893-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6893_MDPSYS) += clk-mt6893-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6893_MFGCFG) += clk-mt6893-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6893_MMSYS) += clk-mt6893-mm.o
obj-$(CONFIG_COMMON_CLK_MT6893_SCP_ADSP) += clk-mt6893-scp_adsp.o
obj-$(CONFIG_COMMON_CLK_MT6893_VDECSYS) += clk-mt6893-vde2.o
obj-$(CONFIG_COMMON_CLK_MT6893_VDECSYS_SOC) += clk-mt6893-vde1.o
obj-$(CONFIG_COMMON_CLK_MT6893_VENCSYS_C1) += clk-mt6893-ven2.o
obj-$(CONFIG_COMMON_CLK_MT6893_VENCSYS) += clk-mt6893-ven1.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6895.c),)
obj-$(CONFIG_COMMON_CLK_MT6895) += clk-mt6895.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6895_ADSP) += clk-mt6895-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6895_CAM) += clk-mt6895-cam.o
obj-$(CONFIG_COMMON_CLK_MT6895_CCU) += clk-mt6895-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6895_IMG) += clk-mt6895-img.o
obj-$(CONFIG_COMMON_CLK_MT6895_MMSYS) += clk-mt6895-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6895_PERI) += clk-mt6895-peri.o
obj-$(CONFIG_COMMON_CLK_MT6895_BUS) += clk-mt6895-bus.o
obj-$(CONFIG_COMMON_CLK_MT6895_MDPSYS) += clk-mt6895-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6895_MFG) += clk-mt6895-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6895_VCODEC) += clk-mt6895-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6983.c),)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-mt6983.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6983_ADSP) += clk-mt6983-adsp_grp.o
obj-$(CONFIG_COMMON_CLK_MT6983_CCU) += clk-mt6983-ccu_main.o
obj-$(CONFIG_COMMON_CLK_MT6983_IMG) += clk-mt6983-img.o
obj-$(CONFIG_COMMON_CLK_MT6983_IMP_IIC_WRAP) += clk-mt6983-imp_iic_wrap.o
obj-$(CONFIG_COMMON_CLK_MT6983_VCODEC) += clk-mt6983-vcodec.o
obj-$(CONFIG_COMMON_CLK_MT6983_MDPSYS) += clk-mt6983-mdp_grp.o
obj-$(CONFIG_COMMON_CLK_MT6983_MFG) += clk-mt6983-mfg_top_config.o
obj-$(CONFIG_COMMON_CLK_MT6983_MMSYS) += clk-mt6983-mm.o
obj-$(CONFIG_COMMON_CLK_MT6983_CAM) += clk-mt6983-cam.o
endif
obj-$(CONFIG_COMMON_CLK_MT7622) += clk-mt7622.o
obj-$(CONFIG_COMMON_CLK_MT7622_ETHSYS) += clk-mt7622-eth.o
obj-$(CONFIG_COMMON_CLK_MT7622_HIFSYS) += clk-mt7622-hif.o
obj-$(CONFIG_COMMON_CLK_MT7622_AUDSYS) += clk-mt7622-aud.o
obj-$(CONFIG_COMMON_CLK_MT7629) += clk-mt7629.o
obj-$(CONFIG_COMMON_CLK_MT7629_ETHSYS) += clk-mt7629-eth.o
obj-$(CONFIG_COMMON_CLK_MT7629_HIFSYS) += clk-mt7629-hif.o
obj-$(CONFIG_COMMON_CLK_MT8135) += clk-mt8135.o
obj-$(CONFIG_COMMON_CLK_MT8167) += clk-mt8167.o
obj-$(CONFIG_COMMON_CLK_MT8167_AUDSYS) += clk-mt8167-aud.o
obj-$(CONFIG_COMMON_CLK_MT8167_IMGSYS) += clk-mt8167-img.o
obj-$(CONFIG_COMMON_CLK_MT8167_MFGCFG) += clk-mt8167-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT8167_MMSYS) += clk-mt8167-mm.o
obj-$(CONFIG_COMMON_CLK_MT8167_VDECSYS) += clk-mt8167-vdec.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clk-mt8173.o
obj-$(CONFIG_COMMON_CLK_MT8173_MMSYS) += clk-mt8173-mm.o
obj-$(CONFIG_COMMON_CLK_MT8183) += clk-mt8183.o
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
obj-$(CONFIG_COMMON_CLK_MT8192) += clk-mt8192.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU0) += clk-mt8192-apu0.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU1) += clk-mt8192-apu1.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_VCORE) += clk-mt8192-apu_vcore.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_CONN) += clk-mt8192-apu_conn.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_MDLA0) += clk-mt8192-apu_mdla0.o
obj-$(CONFIG_COMMON_CLK_MT8192_AUDSYS) += clk-mt8192-aud.o
obj-$(CONFIG_COMMON_CLK_MT8192_CAMSYS) += clk-mt8192-cam.o
obj-$(CONFIG_COMMON_CLK_MT8192_CAMSYS_RAWA) += clk-mt8192-cam_rawa.o
obj-$(CONFIG_COMMON_CLK_MT8192_CAMSYS_RAWB) += clk-mt8192-cam_rawb.o
obj-$(CONFIG_COMMON_CLK_MT8192_CAMSYS_RAWC) += clk-mt8192-cam_rawc.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMGSYS) += clk-mt8192-img.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMGSYS2) += clk-mt8192-img2.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMP_IIC_WRAP_C) += clk-mt8192-imp_iic_wrap_c.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMP_IIC_WRAP_E) += clk-mt8192-imp_iic_wrap_e.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMP_IIC_WRAP_N) += clk-mt8192-imp_iic_wrap_n.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMP_IIC_WRAP_S) += clk-mt8192-imp_iic_wrap_s.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMP_IIC_WRAP_W) += clk-mt8192-imp_iic_wrap_w.o
obj-$(CONFIG_COMMON_CLK_MT8192_IMP_IIC_WRAP_WS) += clk-mt8192-imp_iic_wrap_ws.o
obj-$(CONFIG_COMMON_CLK_MT8192_IPESYS) += clk-mt8192-ipe.o
obj-$(CONFIG_COMMON_CLK_MT8192_MDPSYS) += clk-mt8192-mdp.o
obj-$(CONFIG_COMMON_CLK_MT8192_MFGCFG) += clk-mt8192-mfg.o
obj-$(CONFIG_COMMON_CLK_MT8192_MMSYS) += clk-mt8192-mm.o
obj-$(CONFIG_COMMON_CLK_MT8192_MSDC_TOP) += clk-mt8192-msdc_top.o
obj-$(CONFIG_COMMON_CLK_MT8192_MSDC) += clk-mt8192-msdc.o
obj-$(CONFIG_COMMON_CLK_MT8192_SCP_ADSP) += clk-mt8192-scp_adsp.o
obj-$(CONFIG_COMMON_CLK_MT8192_VDECSYS) += clk-mt8192-vdec.o
obj-$(CONFIG_COMMON_CLK_MT8192_VDECSYS_SOC) += clk-mt8192-vdec_soc.o
obj-$(CONFIG_COMMON_CLK_MT8192_VENCSYS) += clk-mt8192-venc.o
obj-$(CONFIG_COMMON_CLK_MT8516) += clk-mt8516.o
obj-$(CONFIG_COMMON_CLK_MT8516_AUDSYS) += clk-mt8516-aud.o
ifeq ($(CONFIG_MEDIATEK_FHCTL_V1),m)
obj-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING) += fhctl_v1/
else ifeq ($(CONFIG_MEDIATEK_FHCTL_V1),y)
obj-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING) += fhctl_v1/
else
obj-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING) += fhctl.o
fhctl-y += clk-fhctl.o
fhctl-y += clk-fhctl-ap.o
fhctl-y += clk-fhctl-pll.o
ifeq ($(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_MCUPM),y)
fhctl-y += clk-fhctl-mcupm.o
ccflags-y += -DUSE_FHCTL_MCUPM
endif
ifeq ($(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_GPUEB),y)
fhctl-y += clk-fhctl-gpueb.o
ccflags-y += -DUSE_FHCTL_GPUEB
endif
fhctl-$(CONFIG_DEBUG_FS) += clk-fhctl-debug.o
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/include/linux/soc/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpueb/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6789.c),)
obj-$(CONFIG_COMMON_CLK_MT6789) += clk-chk-mt6789.o
clk-chk-mt6789-$(CONFIG_COMMON_CLK_MT6789) += clkchk-mt6789.o
obj-$(CONFIG_COMMON_CLK_MT6789) += clk-fmeter-mt6789.o
clk-fmeter-mt6789-$(CONFIG_COMMON_CLK_MT6789) += clk-fmeter-mt6789.o
obj-$(CONFIG_COMMON_CLK_MT6789) += pd-chk-mt6789.o
pd-chk-mt6789-$(CONFIG_COMMON_CLK_MT6789) += mtk-pd-chk-mt6789.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6789) += clk-dbg-mt6789.o
clk-dbg-mt6789-$(CONFIG_COMMON_CLK_MT6789) += clkdbg-mt6789.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6833.c),)
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-chk-mt6833.o
clk-chk-mt6833-$(CONFIG_COMMON_CLK_MT6833) += clkchk-mt6833.o
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-fmeter-mt6833.o
clk-fmeter-mt6833-$(CONFIG_COMMON_CLK_MT6833) += clk-fmeter-mt6833.o
obj-$(CONFIG_COMMON_CLK_MT6833) += pd-chk-mt6833.o
pd-chk-mt6833-$(CONFIG_COMMON_CLK_MT6833) += mtk-pd-chk-mt6833.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-dbg-mt6833.o
clk-dbg-mt6833-$(CONFIG_COMMON_CLK_MT6833) += clkdbg-mt6833.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6853.c),)
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-chk-mt6853.o
clk-chk-mt6853-$(CONFIG_COMMON_CLK_MT6853) += clkchk-mt6853.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-dbg-mt6853.o
clk-dbg-mt6853-$(CONFIG_COMMON_CLK_MT6853) += clkdbg-mt6853.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6879.c),)
obj-$(CONFIG_COMMON_CLK_MT6879) += clk-chk-mt6879.o
clk-chk-mt6879-$(CONFIG_COMMON_CLK_MT6879) += clkchk-mt6879.o
obj-$(CONFIG_COMMON_CLK_MT6879) += clk-fmeter-mt6879.o
clk-fmeter-mt6879-$(CONFIG_COMMON_CLK_MT6879) += clk-fmeter-mt6879.o
obj-$(CONFIG_COMMON_CLK_MT6879) += pd-chk-mt6879.o
pd-chk-mt6879-$(CONFIG_COMMON_CLK_MT6879) += mtk-pd-chk-mt6879.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6879) += clk-dbg-mt6879.o
clk-dbg-mt6879-$(CONFIG_COMMON_CLK_MT6879) += clkdbg-mt6879.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6893.c),)
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-chk-mt6893.o
clk-chk-mt6893-$(CONFIG_COMMON_CLK_MT6893) += clkchk-mt6893.o
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-fmeter-mt6893.o
clk-fmeter-mt6893-$(CONFIG_COMMON_CLK_MT6893) += clk-fmeter-mt6893.o
obj-$(CONFIG_COMMON_CLK_MT6893) += pd-chk-mt6893.o
pd-chk-mt6893-$(CONFIG_COMMON_CLK_MT6893) += mtk-pd-chk-mt6893.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-dbg-mt6893.o
clk-dbg-mt6893-$(CONFIG_COMMON_CLK_MT6893) += clkdbg-mt6893.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6895.c),)
obj-$(CONFIG_COMMON_CLK_MT6895) += clk-chk-mt6895.o
clk-chk-mt6895-$(CONFIG_COMMON_CLK_MT6895) += clkchk-mt6895.o
obj-$(CONFIG_COMMON_CLK_MT6895) += clk-fmeter-mt6895.o
clk-fmeter-mt6895-$(CONFIG_COMMON_CLK_MT6895) += clk-fmeter-mt6895.o
obj-$(CONFIG_COMMON_CLK_MT6895) += pd-chk-mt6895.o
pd-chk-mt6895-$(CONFIG_COMMON_CLK_MT6895) += mtk-pd-chk-mt6895.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6895) += clk-dbg-mt6895.o
clk-dbg-mt6895-$(CONFIG_COMMON_CLK_MT6895) += clkdbg-mt6895.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6983.c),)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-chk-mt6983.o
clk-chk-mt6983-$(CONFIG_COMMON_CLK_MT6983) += clkchk-mt6983.o
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-fmeter-mt6983.o
clk-fmeter-mt6983-$(CONFIG_COMMON_CLK_MT6983) += clk-fmeter-mt6983.o
obj-$(CONFIG_COMMON_CLK_MT6983) += pd-chk-mt6983.o
pd-chk-mt6983-$(CONFIG_COMMON_CLK_MT6983) += mtk-pd-chk-mt6983.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-dbg-mt6983.o
clk-dbg-mt6983-$(CONFIG_COMMON_CLK_MT6983) += clkdbg-mt6983.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6855.c),)
obj-$(CONFIG_COMMON_CLK_MT6855) += clk-chk-mt6855.o
clk-chk-mt6855-$(CONFIG_COMMON_CLK_MT6855) += clkchk-mt6855.o
obj-$(CONFIG_COMMON_CLK_MT6855) += clk-fmeter-mt6855.o
clk-fmeter-mt6855-$(CONFIG_COMMON_CLK_MT6855) += clk-fmeter-mt6855.o
obj-$(CONFIG_COMMON_CLK_MT6855) += pd-chk-mt6855.o
pd-chk-mt6855-$(CONFIG_COMMON_CLK_MT6855) += mtk-pd-chk-mt6855.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6855) += clk-dbg-mt6855.o
clk-dbg-mt6855-$(CONFIG_COMMON_CLK_MT6855) += clkdbg-mt6855.o
endif

endif
