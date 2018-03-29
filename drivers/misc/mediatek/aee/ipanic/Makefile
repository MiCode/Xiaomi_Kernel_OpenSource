#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

obj-$(CONFIG_MTK_AEE_IPANIC)	+= ipanic.o
ipanic-y	:= ipanic_log.o ipanic_rom.o

ifdef CONFIG_MTK_EMMC_SUPPORT
ipanic-y	+= ipanic_emmc.o
else
ipanic-y	+= ipanic_mtd.o
endif

ipanic-y	+= ipanic_vfs.o
ipanic-y	+= ipanic_pstore.o
