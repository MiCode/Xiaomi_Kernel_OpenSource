#
#HDCP crypto ca
#

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/tee/gud/Makefile.include
endif
ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(srctree)/drivers/tee/teei/Makefile.include
endif

obj-y += tlcDpHdcp.o
#ccflags-y += -DHDCP_SETKEY_FROM_KERNEL
