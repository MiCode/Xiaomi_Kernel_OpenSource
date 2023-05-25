###
# Copyright (C) Fourier Semiconductor Inc. 2016-2021. All rights reserved.
#

# TOPLEVEL=$(PWD)/..
# EXTRA_CFLAGS += -I$(TOPLEVEL)

# EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG
EXTRA_CFLAGS += -Wall -Werror
MODFLAGS = -fno-pic
CFLAGS_MODULE = $(MODFLAGS)
AFLAGS_MODULE = $(MODFLAGS)

snd-soc-fs1599-objs := fsm_core.o
snd-soc-fs1599-objs += fs1599.o
snd-soc-fs1599-objs += fsm_i2c.o
obj-$(CONFIG_SND_SOC_FS1599) += snd-soc-fs1599.o
