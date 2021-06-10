#
# Copyright (C) 2017 MediaTek Inc.
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
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

FPSGO_TOP = $(srctree)/drivers/misc/mediatek/performance/fpsgo_v3
FSTB_TOP = $(FPSGO_TOP)/fstb
EARA_TOP = $(FPSGO_TOP)/eara_job

obj-y += src/eara_job.o

ccflags-y += \
        -Iinclude/ \
        -I$(FPSGO_TOP)/base/include/ \
        -I$(srctree)/drivers/misc/mediatek/base/power/include \
        -I$(FSTB_TOP)/ \
        -I$(EARA_TOP)/include/ \
        -I$(srctree)/drivers/misc/mediatek/vpu/$(MTK_PLATFORM) \
        -I$(srctree)/drivers/misc/mediatek/mdla/1.0 \
        -I$(srctree)/drivers/misc/mediatek/performance/observer \
        -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/ \
        -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(MTK_PLATFORM)
