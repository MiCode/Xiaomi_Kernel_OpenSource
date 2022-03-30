# common part
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/soc/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/

# apusys power driver v2.5
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.5/inc/
SRC := 2.5/common/apu-clk.o 2.5/common/apu-common.o 2.5/common/apu-regulator.o 2.5/common/apu-rpc.o 2.5/common/apu-of.o 2.5/common/apu-plat.o  2.5/common/apu-driver.o 2.5/common/apu-dbg.o 2.5/common/apu-trace.o
SRC += 2.5/governor/gov-apusys.o 2.5/governor/gov-user.o 2.5/governor/common.o 2.5/governor/gov-constrain.o 2.5/governor/gov-apusys-pe.o
SRC += 2.5/devices/dev-freq-core.o 2.5/devices/dev-freq-conn.o 2.5/devices/dev-freq-vpu.o 2.5/devices/dev-freq-mdla.o 2.5/devices/dev-cb.o 2.5/devices/dev-freq-iommu.o

# apusys power driver v3.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/3.0/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/3.0/inc/platform/
SRC += 3.0/aputop_drv.o
SRC += 3.0/aputop_rpmsg.o
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6983_aputop.c),)
subdir-ccflags-y += -DMT6983_PLAT_DATA
SRC += 3.0/platform/mt6983_aputop.o
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6983_pwr_func.c),)
SRC += 3.0/platform/mt6983_pwr_func.o
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6879_aputop.c),)
subdir-ccflags-y += -DMT6879_PLAT_DATA
SRC += 3.0/platform/mt6879_aputop.o
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6879_pwr_func.c),)
SRC += 3.0/platform/mt6879_pwr_func.o
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6895_aputop.c),)
subdir-ccflags-y += -DMT6895_PLAT_DATA
SRC += 3.0/platform/mt6895_aputop.o
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6895_pwr_func.c),)
SRC += 3.0/platform/mt6895_pwr_func.o
endif

SRC += apu_top_entry.o

ifneq (,$(findstring aging,$(PROJECT_DTB_NAMES)))
subdir-ccflags-y += -DAGING_MARGIN
endif

obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apu_top.o
apu_top-objs = $(SRC)
