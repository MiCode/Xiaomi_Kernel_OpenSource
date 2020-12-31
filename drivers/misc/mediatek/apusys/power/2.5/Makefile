ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.5/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/debug/
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/soc/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
SRC := common/apu-clk.o common/apu-common.o common/apu-regulator.o common/apu-rpc.o common/apu-of.o common/apu-plat.o  common/apu-driver.o common/apu-dbg.o common/apu-trace.o
SRC += governor/gov-apusys.o governor/gov-user.o governor/common.o governor/gov-constrain.o
SRC += devices/dev-freq-core.o devices/dev-freq-conn.o devices/dev-freq-vpu.o devices/dev-freq-mdla.o devices/dev-cb.o devices/dev-freq-iommu.o

obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apu_top.o
apu_top-objs = $(SRC)
