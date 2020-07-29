MFD_TOP = $(srctree)/drivers/mfd

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