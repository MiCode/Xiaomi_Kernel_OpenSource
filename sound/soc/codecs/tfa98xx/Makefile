# make sure that the environment variables ARCH and CROSS_COMPILE
# are set for your architecture and cross compiler
#
# e.g. for Beaglebone black:
# export ARCH := arm
# export CROSS_COMPILE := arm-linux-gnueabihf-
#
# e.g. Samsung Galaxy S6:
# export ARCH := arm64
# export CROSS_COMPILE := aarch64-linux-android-

#MODULE_NAME = snd-soc-tfa98xx

# additional debugging
#TFA_DEBUG=1

# debugging support (also enables trace_printk)
EXTRA_CFLAGS += -Isound/soc/codecs/tfa98xx/inc
EXTRA_CFLAGS += -DDEBUG
EXTRA_CFLAGS += -Werror
EXTRA_CFLAGS += $(call cc-option,-Wno-date-time,)

snd-soc-tfa98xx-objs += src/tfa98xx.o
snd-soc-tfa98xx-objs += src/tfa_container.o
snd-soc-tfa98xx-objs += src/tfa_dsp.o
snd-soc-tfa98xx-objs += src/tfa_init.o

ifdef TFA_DEBUG
EXTRA_CFLAGS += -DTFA_DEBUG -DDEBUG
snd-soc-tfa98xx-objs += src/tfa_debug.o
endif

obj-$(CONFIG_SND_SOC_TFA98XX) += snd-soc-tfa98xx.o

