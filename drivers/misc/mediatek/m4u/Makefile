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

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

# For test
# $(info GM-T-m4u-$(CONFIG_MTK_PLATFORM))
# ifeq ($(CONFIG_MTK_PLATFORM),"mt6761")
# $(info GM-T-with-quotation marks)
# endif
# ifeq ($(CONFIG_MTK_PLATFORM),6761)
# $(info GM-T-without-quotation marks)
# endif

ifneq (,$(filter $(strip $(CONFIG_MTK_PLATFORM)), "mt6761"))
obj-y += 2.0/
endif
