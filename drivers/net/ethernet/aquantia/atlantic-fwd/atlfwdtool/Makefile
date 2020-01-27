################################################################################
#
# Copyright (C) 2019 Aquantia Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#
################################################################################

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
