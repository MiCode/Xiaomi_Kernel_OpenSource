MFD_TOP = $(srctree)/drivers/mfd
PLAT_TOP = $(srctree)/drivers/misc/mediatek/include/mt-plat

ifeq ($(CONFIG_MTK_CLKBUF_BBLPM),y)
    ccflags-y += -D BBLPM_SUPPORT=1
else
    ccflags-y += -D BBLPM_SUPPORT=0
endif

ifeq ($(CONFIG_MTK_CLKBUF_NFC),y)
    ccflags-y += -D NFC_CLKBUF_SUPPORT=1
else
    ccflags-y += -D NFC_CLKBUF_SUPPORT=0
endif

ifneq (,$(filter y m, $(CONFIG_MTK_CLK_BUFFER)))
    ccflags-y += -I$(src)/src \
	     -I$(srctree)/include/ \
	     -I$(MFD_TOP)/ \
	     -I$(PLAT_TOP)/

    obj-$(CONFIG_MTK_CLK_BUFFER) += clkbuf.o

    clkbuf-$(CONFIG_MTK_CLK_BUFFER) += mtk-clkbuf-bridge.o \
                src/mtk-clkbuf-ctl.o \
		src/mtk-clkbuf-hw.o

else
    ccflags-y += -I$(srctree)/include/ \
	     -I$(PLAT_TOP)/
    obj-y += mtk-clkbuf-bridge.o
endif
