###
# Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
#

# TOPLEVEL=$(PWD)/..
# EXTRA_CFLAGS += -I$(TOPLEVEL)

# EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG
EXTRA_CFLAGS += -Wall -Werror
MODFLAGS = -fno-pic
CFLAGS_MODULE = $(MODFLAGS)
AFLAGS_MODULE = $(MODFLAGS)

obj-$(CONFIG_SND_SOC_FS16XX) += fsm_core.o fs1815.o fsm_i2c.o
