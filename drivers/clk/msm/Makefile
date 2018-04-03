obj-$(CONFIG_COMMON_CLK_MSM)	+= clock.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-dummy.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-generic.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-local2.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-pll.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-alpha-pll.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-rpm.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-voter.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= reset.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-debug.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= gdsc.o

obj-$(CONFIG_MSM_CLK_CONTROLLER_V2)	+= msm-clock-controller.o

ifeq ($(CONFIG_COMMON_CLK_MSM), y)
obj-$(CONFIG_ARCH_MSM8953)	+= clock-gcc-8953.o
obj-$(CONFIG_ARCH_MSM8953)	+= clock-cpu-8953.o
obj-$(CONFIG_ARCH_MSM8953)	+= clock-rcgwr.o
obj-$(CONFIG_ARCH_MSM8937)	+= clock-gcc-8952.o
obj-$(CONFIG_ARCH_MSM8937)	+= clock-cpu-8939.o
obj-$(CONFIG_ARCH_MSM8937)	+= clock-rcgwr.o
obj-$(CONFIG_ARCH_MSM8953)	+= clock-cpu-sdm632.o

# MDM9607
obj-$(CONFIG_ARCH_MDM9607)      +=clock-gcc-mdm9607.o

# MDM9650
obj-$(CONFIG_ARCH_MDM9650)      += clock-gcc-mdm9650.o

# MSM8909
obj-$(CONFIG_ARCH_MSM8909)	+= clock-rpm-8909.o
obj-$(CONFIG_ARCH_MSM8909)	+= clock-gcc-8909.o

# ACPU clock
obj-$(CONFIG_ARCH_MSM8909)	+= clock-a7.o
obj-$(CONFIG_ARCH_MDM9607)	+= clock-a7.o
obj-$(CONFIG_ARCH_MDM9650)	+= clock-a7.o
endif

obj-y				+= mdss/

