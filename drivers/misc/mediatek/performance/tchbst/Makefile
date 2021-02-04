# Copyright (C) 2018 MediaTek Inc.
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


obj-y += tchbst_main.o

obj-$(CONFIG_MTK_FPSGO) += user/
obj-$(CONFIG_MTK_FPSGO_V3) += user/

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6757"))
	obj-y += legacy/
endif

#ifeq ($(CONFIG_MTK_PERFMGR_TOUCH_BOOST),y)
obj-y += kernel/
#endif # CONFIG_MTK_PERFMGR_TOUCH_BOOST
