# SPDX-License-Identifier: GPL-2.0-only
# Atlantic Network Driver
#
# Copyright (C) 2019 aQuantia Corporation
# Copyright (C) 2019-2020 Marvell International Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

PKGCONFIG?=$(shell which pkg-config)
ifeq ($(PKGCONFIG),)
$(info Do you have pkg-config installed? If not, install it, e.g. on Ubuntu)
$(info $$ apt install pkg-config)
$(error pkg-config not found)
endif
LIBMNL_EXISTS=$(shell $(PKGCONFIG) --exists libmnl && echo "yes" || echo "no")
ifeq ($(LIBMNL_EXISTS),no)
$(info Do you have libmnl-dev installed? If not, install it, e.g. on Ubuntu)
$(info $$ apt install libmnl-dev)
$(error libmnl not found)
endif

CFLAGS+=-Wall -Werror -Wfatal-errors $(shell $(PKGCONFIG) --cflags libmnl) -I..
LDLIBS+=$(shell $(PKGCONFIG) --libs libmnl)

.PHONY: all clean

all: atlfwdtool

atlfwdtool: atlfwdtool.c atlfwd_args.c atlfwd_msg.c atlfwd_reply.c
	$(LINK.c) $^ $(LDLIBS) -o $@

clean:
	$(RM) atlfwdtool
	$(RM) *.o
