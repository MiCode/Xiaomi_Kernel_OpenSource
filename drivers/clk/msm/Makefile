obj-y				+= clock.o
obj-y				+= clock-dummy.o
obj-y				+= clock-generic.o
obj-y				+= clock-local2.o
obj-y				+= clock-pll.o
obj-y				+= clock-alpha-pll.o
obj-y				+= clock-rpm.o
obj-y				+= clock-voter.o

obj-$(CONFIG_MSM_CLK_CONTROLLER_V2)	+= msm-clock-controller.o

obj-y		+= clock-debug.o

# MSM 8996
obj-$(CONFIG_ARCH_QCOM) += clock-gcc-8996.o
obj-$(CONFIG_ARCH_QCOM) += clock-mmss-8996.o
obj-$(CONFIG_ARCH_QCOM) += clock-cpu-8996.o


# MSM COBALT
obj-$(CONFIG_ARCH_QCOM) += clock-gcc-cobalt.o

# CPU clocks

obj-y				+= gdsc.o
obj-y				+= mdss/
