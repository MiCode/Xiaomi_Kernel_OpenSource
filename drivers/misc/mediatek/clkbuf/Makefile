MFD_TOP = $(srctree)/drivers/mfd

#Old GKI
ifneq (,$(filter y m, $(CONFIG_SCSI_UFS_MEDIATEK)))
    ccflags-y += -D UFS_CLKBUF_SUPPORT=1
else
    ccflags-y += -D UFS_CLKBUF_SUPPORT=0
endif

ccflags-y += -I$(src)/src \
     -I$(MFD_TOP)/

obj-$(CONFIG_MTK_CLK_BUFFER) += clkbuf.o

clkbuf-$(CONFIG_MTK_CLK_BUFFER) +=  src/mtk-clkbuf-ctl.o \
    src/mtk-clkbuf-hw.o

obj-$(CONFIG_MTK_CLK_BUFFER) += clkbuf-pmic.o

clkbuf-pmic-$(CONFIG_MTK_CLK_BUFFER) +=  src/mtk-clkbuf-pmic.o

# New GKI
# common clkbuf driver
obj-$(CONFIG_MTK_CLKBUF) += clkbuf.o
ifneq ($(CONFIG_MTK_CLKBUF),)
	clkbuf-y += merged/src/mtk_clkbuf_common.o
	ccflags-y += -I$(srctree)/$(src)/merged/inc
endif

clkbuf-$(CONFIG_MTK_CLKBUF_V1) += merged/src/mtk_clkbuf_ctl.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += merged/src/mtk-clkbuf-pmif.o

clkbuf-$(CONFIG_MT6359P_CLKBUF) += merged/src/mtk-clkbuf-dcxo.o
clkbuf-$(CONFIG_MT6359P_CLKBUF) += merged/src/mtk-clkbuf-dcxo-6359p.o
ccflags-$(CONFIG_MT6359P_CLKBUF) += -D PMIC_CLKBUF_SUPPORT
ccflags-$(CONFIG_MT6359P_CLKBUF) += -D LEGACY_DCXO_REG
ccflags-$(CONFIG_MT6359P_CLKBUF) += -D DCXO_HWBBLPM_SUPPORT
ccflags-$(CONFIG_MT6359P_CLKBUF) += -D CONN_FROM_SRCLKEN_I3

clkbuf-$(CONFIG_MTK_SRCLKEN_RC_V1) += merged/src/mtk-srclken-rc-hw.o
clkbuf-$(CONFIG_MTK_SRCLKEN_RC_V1) += merged/src/mtk-srclken-rc-hw-v1.o
ccflags-$(CONFIG_MTK_SRCLKEN_RC_V1) += -D SRCLKEN_RC_SUPPORT
