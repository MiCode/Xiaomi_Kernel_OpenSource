# File: Makefile
# Copyright (C) 2007-2012, Marvell International Ltd.
#

CC=		$(CROSS_COMPILE)gcc
LD=		$(CROSS_COMPILE)ld

BACKUP=		/root/backup
YMD=		`date +%Y%m%d%H%M`

#############################################################################
# Select Platform Tools
#############################################################################

MODEXT = ko

ifeq ($(CONFIG_64BIT), y)
	EXTRA_CFLAGS += -DMBT_64BIT
endif

EXTRA_CFLAGS += -I$(M)/../mbtchar_src
EXTRA_CFLAGS += -I$(M)/bt
LD += -S

#############################################################################
# Compiler Flags
#############################################################################
	EXTRA_CFLAGS += -DFPNUM='"27"'

EXTRA_CFLAGS += -DDEBUG_LEVEL1
EXTRA_CFLAGS += -DSDIO_SUSPEND_RESUME

ifeq ($(CONFIG_SD8897_BT_DEBUG),2)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
	EXTRA_CFLAGS += -DDEBUG_LEVEL2
	DBG=	-dbg
endif

#############################################################################
# Make Targets
#############################################################################

BTOBJS = bt/bt_main.o bt/bt_sdiommc.o bt/bt_proc.o bt/mbt_char.o

BTOBJS += bt/bt_init.o

obj-m := mbt8897.o
mbt8897-objs := $(BTOBJS)

# End of file;
