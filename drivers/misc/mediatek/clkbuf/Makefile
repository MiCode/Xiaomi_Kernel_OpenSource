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
# Built-in common clkbuf kernel
obj-$(CONFIG_MTK_CLKBUF) += clkbuf.o

ifneq ($(CONFIG_MTK_CLKBUF_V1),)
	clkbuf-y += v1/src/mtk_clkbuf_common.o
	ccflags-y += -I$(srctree)/$(src)/v1/inc
endif

clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk_clkbuf_ctl.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-pmif.o
clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo.o
clkbuf-$(CONFIG_MTK_SRCLKEN_RC_V1) += v1/src/mtk-srclken-rc-hw.o
clkbuf-$(CONFIG_MTK_SRCLKEN_RC_V1) += v1/src/mtk-srclken-rc-hw-v1.o
ccflags-$(CONFIG_MTK_SRCLKEN_RC_V1) += -D SRCLKEN_RC_SUPPORT

# .ko PMIC platform code for clkbuf kernel
obj-$(CONFIG_MTK_CLKBUF) += mt6359p-clkbuf.o
obj-$(CONFIG_MTK_CLKBUF) += mt6685-clkbuf.o
obj-$(CONFIG_MTK_CLKBUF) += mt6366-clkbuf.o
obj-$(CONFIG_MTK_CLKBUF) += mt6377-clkbuf.o
mt6359p-clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6359p.o
mt6685-clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6685.o
mt6366-clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6366.o
mt6377-clkbuf-$(CONFIG_MTK_CLKBUF_V1) += v1/src/mtk-clkbuf-dcxo-6377.o
