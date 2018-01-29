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
endif

obj-y               += mdss/

