obj-$(CONFIG_COMMON_CLK_MSM)	+= clock.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-dummy.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-generic.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-local2.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-pll.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-alpha-pll.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-rpm.o
obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-voter.o

obj-$(CONFIG_MSM_CLK_CONTROLLER_V2)	+= msm-clock-controller.o

obj-$(CONFIG_COMMON_CLK_MSM)	+= clock-debug.o

# MSM 8996
ifeq ($(CONFIG_COMMON_CLK_MSM), y)
	obj-$(CONFIG_ARCH_QCOM) += clock-gcc-8996.o
	obj-$(CONFIG_ARCH_QCOM) += clock-mmss-8996.o
	obj-$(CONFIG_ARCH_QCOM) += clock-cpu-8996.o
endif

# MSM COBALT
ifeq ($(CONFIG_COMMON_CLK_MSM), y)
	obj-$(CONFIG_ARCH_MSMCOBALT)	+= clock-gcc-cobalt.o
	obj-$(CONFIG_ARCH_MSMCOBALT)	+= clock-gpu-cobalt.o
	obj-$(CONFIG_ARCH_MSMCOBALT)	+= clock-mmss-cobalt.o
	obj-$(CONFIG_ARCH_MSMCOBALT)	+= clock-osm.o
endif

obj-$(CONFIG_COMMON_CLK_MSM)	+= gdsc.o
obj-$(CONFIG_COMMON_CLK_MSM)-y	+= mdss/
