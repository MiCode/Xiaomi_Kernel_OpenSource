obj-y				+= clock.o
obj-y				+= clock-dummy.o
obj-y				+= clock-generic.o
obj-y				+= clock-local2.o
obj-y				+= clock-pll.o
ifndef CONFIG_ARM64
obj-y				+= clock-rpm.o
endif
obj-y				+= clock-voter.o

obj-$(CONFIG_DEBUG_FS)		+= clock-debug.o

# MDM9630
obj-$(CONFIG_ARCH_MDM9630)	+= clock-alpha-pll.o

obj-y				+= gdsc.o
obj-y				+= mdss/
