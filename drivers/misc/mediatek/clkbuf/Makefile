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
	clkbuf-y += v1/src/mtk_clkbuf_common.o
	ccflags-y += -I$(srctree)/$(src)/v1/inc
endif

clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk_clkbuf_ctl.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-pmif.o

clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6359p.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6685.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6366.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6358.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6357.o

clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-srclken-rc-hw.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-srclken-rc-hw-v1.o
