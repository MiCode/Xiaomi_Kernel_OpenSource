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
EXTRA_CFLAGS += -DDEBUG
EXTRA_CFLAGS += -Werror

obj-y += snd-soc-tfa98xx.o

snd-soc-tfa98xx-objs += tfa98xx.o
snd-soc-tfa98xx-objs += tfa_container.o
snd-soc-tfa98xx-objs += tfa_dsp.o
snd-soc-tfa98xx-objs += tfa9888_init.o
snd-soc-tfa98xx-objs += tfa9891_init.o
snd-soc-tfa98xx-objs += tfa9897_init.o
snd-soc-tfa98xx-objs += tfa9890_init.o
snd-soc-tfa98xx-objs += tfa9887B_init.o
snd-soc-tfa98xx-objs += tfa9887_init.o

ifdef TFA_DEBUG
EXTRA_CFLAGS += -DTFA_DEBUG -DDEBUG
snd-soc-tfa98xx-objs += tfa_debug.o
endif
