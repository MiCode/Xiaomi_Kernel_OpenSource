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

# common source and includes
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

obj-y += mtk_cpuhp_core.o
obj-y += mtk_cpuhp_ppm.o
obj-y += mtk_cpuhp_platform.o


# platform dependent source and includes

# MT6771
ccflags-$(CONFIG_MACH_MT6771) += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_MACH_MT6771) += mtk_cpuhp_mt6771.o

# MT6775
ccflags-$(CONFIG_MACH_MT6775) += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(CONFIG_MTK_PLATFORM)
obj-$(CONFIG_MACH_MT6775) += mtk_cpuhp_mt6771.o
