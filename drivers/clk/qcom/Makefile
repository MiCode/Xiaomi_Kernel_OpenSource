obj-y				+= clock.o
obj-y				+= clock-dummy.o
obj-y				+= clock-generic.o
obj-y				+= clock-local2.o
obj-y				+= clock-pll.o
obj-y				+= clock-rpm.o
obj-y				+= clock-voter.o

obj-$(CONFIG_DEBUG_FS)		+= clock-debug.o

# MDM9630
obj-$(CONFIG_ARCH_MDM9630)	+= clock-alpha-pll.o

# MSM8916
obj-$(CONFIG_ARCH_MSM8916)	+= clock-rpm-8916.o
obj-$(CONFIG_ARCH_MSM8916)	+= clock-gcc-8916.o

# MSMPLUTONIUM
obj-$(CONFIG_ARCH_MSM)		+= clock-alpha-pll.o
obj-$(CONFIG_ARCH_MSM)		+= clock-rpm-plutonium.o
obj-$(CONFIG_ARCH_MSM)		+= clock-gcc-plutonium.o
obj-$(CONFIG_ARCH_MSM)		+= clock-mmss-plutonium.o

obj-y				+= gdsc.o
obj-y				+= mdss/
