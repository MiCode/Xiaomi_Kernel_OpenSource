ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/

mtk-dvfsrc-help-objs = dvfsrc.o dvfsrc-opp.o dvfsrc-sysfs.o dvfsrc-ipi.o
mtk-dvfsrc-help-objs += dvfsrc-mt6779.o dvfsrc-mt6779-met.o
mtk-dvfsrc-help-objs += dvfsrc-mt6761.o dvfsrc-mt6761-met.o

obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc-help.o
obj-$(CONFIG_MTK_DVFSRC) += dvfsrc-boost.o
