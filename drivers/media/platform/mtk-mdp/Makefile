mtk-mdp-y += mtk_mdp_core.o
mtk-mdp-y += mtk_mdp_comp.o
mtk-mdp-y += mtk_mdp_m2m.o
mtk-mdp-y += mtk_mdp_regs.o
mtk-mdp-y += mtk_mdp_vpu.o

obj-$(CONFIG_VIDEO_MEDIATEK_MDP) += mtk-mdp.o

ccflags-y += -I$(srctree)/drivers/media/platform/mtk-vpu

obj-$(CONFIG_VIDEO_MEDIATEK_MDP_VCU) += mtk-mdp.o
ccflags-$(CONFIG_VIDEO_MEDIATEK_VCU) += -I$(srctree)/drivers/media/platform/mtk-vcu

obj-$(CONFIG_VIDEO_MEDIATEK_MDP_FRVC) += fast-rvc/
