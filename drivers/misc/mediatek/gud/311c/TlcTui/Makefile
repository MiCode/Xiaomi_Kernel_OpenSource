#
# Makefile for the <t-base trusted UI driver
#

#GUD_ROOT_FOLDER := drivers/gud/

# add our modules to kernel.
obj-$(CONFIG_TRUSTONIC_TRUSTED_UI) += TlcTui.o

TlcTui-y := main.o tlcTui.o trustedui.o tui-hal_$(MTK_PLATFORM).o

# Release mode by default
#ccflags-y += -DNDEBUG
ccflags-y += -Wno-declaration-after-statement

ccflags-$(CONFIG_TRUSTONIC_TEE_DEBUG) += -DDEBUG

# MobiCore Driver includes
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver/public

# MobiCore TlcTui required includes
ccflags-y += -I$(GUD_ROOT_FOLDER)/TlcTui/inc \
             -I$(GUD_ROOT_FOLDER)/TlcTui/public

ccflags-y += -DTUI_ENABLE_DISPLAY
ccflags-y += -DTUI_ENABLE_TOUCH
ccflags-y += -DTUI_LOCK_I2C
ccflags-y += -DTUI_ENABLE_MPU
