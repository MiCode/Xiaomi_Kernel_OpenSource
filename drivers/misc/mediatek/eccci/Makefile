ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE), y)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

# common services
ccci-y := ccci_core.o ccci_bm.o ccci_support.o

# port routines
ccci-y += port_char.o port_net.o port_kernel.o port_ipc.o port_cfg.o

# modem drivers
ifeq ($(CONFIG_MTK_ECCCI_CCIF), y)
ccci-y += ccci_ringbuf.o modem_ccif.o
endif

ifeq ($(CONFIG_MTK_ECCCI_CLDMA), y)
ccci-y += modem_cldma.o
endif

# modem_xxx.o and modem_ut.o should not be enabled both, unless you checked code (device number. etc.).
ifeq ($(CONFIG_MTK_ECCCI_UT), y)
ccci-y += modem_ut.o
endif

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

else
obj-  := dummy.o # avoid build error
endif


