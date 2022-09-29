/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DDP_COMP_H
#define MTK_DRM_DDP_COMP_H

#include <linux/io.h>
#include <linux/kernel.h>
#include "mtk_log.h"
#include "mtk_rect.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_ddp_addon.h"
#include <linux/pm_runtime.h>
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct mtk_plane_state;
struct drm_crtc_state;

#define ALIGN_TO(x, n)  (((x) + ((n) - 1)) & ~((n) - 1))

enum mtk_ddp_comp_type {
	MTK_DISP_OVL,
	MTK_DISP_RDMA,
	MTK_DISP_MDP_RDMA,
	MTK_DISP_WDMA,
	MTK_DISP_COLOR,
	MTK_DISP_DITHER,
	MTK_DISP_CCORR,
	MTK_DISP_AAL,
	MTK_DISP_GAMMA,
	MTK_DISP_UFOE,
	MTK_DSI,
	MTK_DPI,
	MTK_DISP_PWM,
	MTK_DISP_MUTEX,
	MTK_DISP_OD,
	MTK_DISP_BLS,
	MTK_DISP_RSZ,
	MTK_DISP_POSTMASK,
	MTK_DMDP_RDMA,
	MTK_DMDP_HDR,
	MTK_DMDP_AAL,
	MTK_DMDP_RSZ,
	MTK_DMDP_TDSHP,
	MTK_DISP_CM,
	MTK_DISP_SPR,
	MTK_DISP_POSTALIGN,
	MTK_DISP_DSC,
	MTK_DISP_VDCM,
	MTK_DP_INTF,
	MTK_DISP_MERGE,
	MTK_DISP_DPTX,
	MTK_DISP_RDMA_OUT_RELAY,
	MTK_DISP_VIRTUAL,
	MTK_DISP_CHIST,
	MTK_DISP_C3D,
	MTK_DISP_TDSHP,
	MTK_DISP_Y2R,
	MTK_DISP_DLO_ASYNC,
	MTK_DISP_DLI_ASYNC,
	MTK_DISP_INLINE_ROTATE,
	MTK_MMLSYS_BYPASS,
	MTK_MML_RSZ,
	MTK_MML_HDR,
	MTK_MML_AAL,
	MTK_MML_TDSHP,
	MTK_MML_COLOR,
	MTK_MML_MML,
	MTK_MML_MUTEX,
	MTK_MML_WROT,
	MTK_DISP_ODDMR,
	MTK_DDP_COMP_TYPE_MAX,
};

#define DECLARE_DDP_COMP(EXPR)                                                 \
	EXPR(DDP_COMPONENT_AAL0)                                            \
	EXPR(DDP_COMPONENT_AAL1)                                            \
	EXPR(DDP_COMPONENT_BLS)                                             \
	EXPR(DDP_COMPONENT_CHIST0)                                          \
	EXPR(DDP_COMPONENT_CHIST1)                                          \
/*5*/	EXPR(DDP_COMPONENT_CHIST2)                                          \
	EXPR(DDP_COMPONENT_CHIST3)                                          \
	EXPR(DDP_COMPONENT_TDSHP0)                                          \
	EXPR(DDP_COMPONENT_TDSHP1)                                          \
	EXPR(DDP_COMPONENT_C3D0)                                            \
/*10*/	EXPR(DDP_COMPONENT_C3D1)                                            \
	EXPR(DDP_COMPONENT_CCORR0)                                          \
	EXPR(DDP_COMPONENT_CCORR1)                                          \
	EXPR(DDP_COMPONENT_CCORR2)                                          \
	EXPR(DDP_COMPONENT_CCORR3)                                          \
/*15*/	EXPR(DDP_COMPONENT_COLOR0)                                          \
	EXPR(DDP_COMPONENT_COLOR1)                                          \
	EXPR(DDP_COMPONENT_COLOR2)                                          \
	EXPR(DDP_COMPONENT_DITHER0)                                         \
	EXPR(DDP_COMPONENT_DITHER1)                                         \
/*20*/	EXPR(DDP_COMPONENT_DPI0)                                            \
	EXPR(DDP_COMPONENT_DPI1)                                            \
	EXPR(DDP_COMPONENT_DSI0)                                            \
	EXPR(DDP_COMPONENT_DSI1)                                            \
	EXPR(DDP_COMPONENT_GAMMA0)                                          \
/*25*/	EXPR(DDP_COMPONENT_GAMMA1)                                          \
	EXPR(DDP_COMPONENT_OD)                                              \
	EXPR(DDP_COMPONENT_OD1)                                             \
	EXPR(DDP_COMPONENT_OVL0)                                            \
	EXPR(DDP_COMPONENT_OVL1)                                            \
/*30*/	EXPR(DDP_COMPONENT_OVL2)                                            \
	EXPR(DDP_COMPONENT_OVL0_2L)                                         \
	EXPR(DDP_COMPONENT_OVL1_2L)                                         \
	EXPR(DDP_COMPONENT_OVL2_2L)                                         \
	EXPR(DDP_COMPONENT_OVL3_2L)                                         \
/*35*/	EXPR(DDP_COMPONENT_OVL0_2L_NWCG)                                    \
	EXPR(DDP_COMPONENT_OVL1_2L_NWCG)                                    \
	EXPR(DDP_COMPONENT_OVL2_2L_NWCG)                                         \
	EXPR(DDP_COMPONENT_OVL3_2L_NWCG)                                         \
	EXPR(DDP_COMPONENT_OVL4_2L)                                   \
/*40*/	EXPR(DDP_COMPONENT_OVL5_2L)                                   \
	EXPR(DDP_COMPONENT_OVL6_2L)                                   \
	EXPR(DDP_COMPONENT_OVL7_2L)                                   \
	EXPR(DDP_COMPONENT_OVL0_2L_VIRTUAL0)                                \
	EXPR(DDP_COMPONENT_OVL1_2L_VIRTUAL0)                                \
/*45*/	EXPR(DDP_COMPONENT_OVL2_2L_VIRTUAL0)                                \
	EXPR(DDP_COMPONENT_OVL3_2L_VIRTUAL0)                                \
	EXPR(DDP_COMPONENT_OVL0_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_OVL0_VIRTUAL1)                                   \
	EXPR(DDP_COMPONENT_OVL1_VIRTUAL0)                                   \
/*50*/	EXPR(DDP_COMPONENT_OVL1_VIRTUAL1)                               \
	EXPR(DDP_COMPONENT_OVL0_2L_NWCG_VIRTUAL0)                           \
	EXPR(DDP_COMPONENT_OVL2_2L_NWCG_VIRTUAL0)                           \
	EXPR(DDP_COMPONENT_OVL0_OVL0_2L_VIRTUAL0)                           \
	EXPR(DDP_COMPONENT_PWM0)                                            \
/*55*/	EXPR(DDP_COMPONENT_PWM1)                                            \
	EXPR(DDP_COMPONENT_PWM2)                                            \
	EXPR(DDP_COMPONENT_RDMA0)                                           \
	EXPR(DDP_COMPONENT_RDMA1)                                           \
	EXPR(DDP_COMPONENT_RDMA2)                                           \
/*60*/	EXPR(DDP_COMPONENT_RDMA3)                                           \
	EXPR(DDP_COMPONENT_RDMA4)                                           \
	EXPR(DDP_COMPONENT_RDMA5)                                           \
	EXPR(DDP_COMPONENT_RDMA0_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA1_VIRTUAL0)                                  \
/*65*/	EXPR(DDP_COMPONENT_RDMA2_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RSZ0)                                            \
	EXPR(DDP_COMPONENT_RSZ1)                                            \
	EXPR(DDP_COMPONENT_RSZ2)                                      \
	EXPR(DDP_COMPONENT_RSZ3)                                      \
/*70*/	EXPR(DDP_COMPONENT_UFOE)                                            \
	EXPR(DDP_COMPONENT_WDMA0)                                           \
	EXPR(DDP_COMPONENT_WDMA1)                                           \
	EXPR(DDP_COMPONENT_WDMA2)                                           \
	EXPR(DDP_COMPONENT_WDMA3)                                           \
/*75*/	EXPR(DDP_COMPONENT_WDMA4)                                     \
	EXPR(DDP_COMPONENT_WDMA5)                                     \
	EXPR(DDP_COMPONENT_UFBC_WDMA0)                                 \
	EXPR(DDP_COMPONENT_UFBC_WDMA1)                                 \
	EXPR(DDP_COMPONENT_UFBC_WDMA2)                                \
/*80*/	EXPR(DDP_COMPONENT_UFBC_WDMA3)                                \
	EXPR(DDP_COMPONENT_WDMA_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_WDMA_VIRTUAL1)                                   \
	EXPR(DDP_COMPONENT_POSTMASK0)                                       \
	EXPR(DDP_COMPONENT_POSTMASK1)                                       \
/*85*/	EXPR(DDP_COMPONENT_DMDP_RDMA0)                                      \
	EXPR(DDP_COMPONENT_DMDP_HDR0)                                       \
	EXPR(DDP_COMPONENT_DMDP_AAL0)                                       \
	EXPR(DDP_COMPONENT_DMDP_RSZ0)                                       \
	EXPR(DDP_COMPONENT_DMDP_TDSHP0)                                     \
/*90*/	EXPR(DDP_COMPONENT_DMDP_RDMA1)                                      \
	EXPR(DDP_COMPONENT_DMDP_HDR1)                                       \
	EXPR(DDP_COMPONENT_DMDP_AAL1)                                       \
	EXPR(DDP_COMPONENT_DMDP_RSZ1)                                       \
	EXPR(DDP_COMPONENT_DMDP_TDSHP1)                                     \
/*95*/	EXPR(DDP_COMPONENT_CM0)                                             \
	EXPR(DDP_COMPONENT_CM1)                                             \
	EXPR(DDP_COMPONENT_SPR0)                                            \
	EXPR(DDP_COMPONENT_SPR1)                                            \
	EXPR(DDP_COMPONENT_DSC0)                                            \
/*100*/	EXPR(DDP_COMPONENT_DSC1)                                            \
	EXPR(DDP_COMPONENT_VDCM0)                                      \
	EXPR(DDP_COMPONENT_VDCM1)                                     \
	EXPR(DDP_COMPONENT_DLO_ASYNC0)                                      \
	EXPR(DDP_COMPONENT_DLO_ASYNC1)                                      \
/*105*/	EXPR(DDP_COMPONENT_DLO_ASYNC2)                                      \
	EXPR(DDP_COMPONENT_DLO_ASYNC3)                                      \
	EXPR(DDP_COMPONENT_DLO_ASYNC4)                                      \
	EXPR(DDP_COMPONENT_DLO_ASYNC5)                                      \
	EXPR(DDP_COMPONENT_DLO_ASYNC6)                                      \
/*110*/	EXPR(DDP_COMPONENT_DLO_ASYNC7)                                      \
	EXPR(DDP_COMPONENT_DLI_ASYNC0)                                      \
	EXPR(DDP_COMPONENT_DLI_ASYNC1)                                      \
	EXPR(DDP_COMPONENT_DLI_ASYNC2)                                      \
	EXPR(DDP_COMPONENT_DLI_ASYNC3)                                      \
/*115*/	EXPR(DDP_COMPONENT_DLI_ASYNC4)                                      \
	EXPR(DDP_COMPONENT_DLI_ASYNC5)                                      \
	EXPR(DDP_COMPONENT_DLI_ASYNC6)                                      \
	EXPR(DDP_COMPONENT_DLI_ASYNC7)                                      \
	EXPR(DDP_COMPONENT_DPTX)                                            \
/*120*/	EXPR(DDP_COMPONENT_DP_INTF0)                                        \
	EXPR(DDP_COMPONENT_DP_INTF1)                                        \
	EXPR(DDP_COMPONENT_RDMA4_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA5_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_MERGE0)                                          \
/*125*/	EXPR(DDP_COMPONENT_MERGE1)                                          \
	EXPR(DDP_COMPONENT_MERGE2)                                     \
	EXPR(DDP_COMPONENT_MERGE3)                                    \
	EXPR(DDP_COMPONENT_SPR0_VIRTUAL)                                    \
	EXPR(DDP_COMPONENT_RDMA0_OUT_RELAY)                                 \
/*130*/	EXPR(DDP_COMPONENT_RDMA2_OUT_RELAY)                                 \
	EXPR(DDP_COMPONENT_PQ0_VIRTUAL)                                     \
	EXPR(DDP_COMPONENT_PQ1_VIRTUAL)                                     \
	EXPR(DDP_COMPONENT_TV0_VIRTUAL)                                     \
	EXPR(DDP_COMPONENT_TV1_VIRTUAL)                                     \
/*135*/	EXPR(DDP_COMPONENT_MAIN0_VIRTUAL)                                   \
	EXPR(DDP_COMPONENT_MAIN1_VIRTUAL)                                   \
	EXPR(DDP_COMPONENT_SUB0_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_SUB1_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_SUB0_VIRTUAL1)                                   \
/*140*/	EXPR(DDP_COMPONENT_SUB1_VIRTUAL1)                                   \
	EXPR(DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL)                           \
	EXPR(DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL)                       \
	EXPR(DDP_COMPONENT_Y2R0)                                            \
	EXPR(DDP_COMPONENT_Y2R1)                                            \
/*145*/	EXPR(DDP_COMPONENT_Y2R2)                                       \
	EXPR(DDP_COMPONENT_Y2R3)                                      \
	EXPR(DDP_COMPONENT_Y2R0_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_Y2R1_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_DLO_ASYNC)                                       \
/*150*/	EXPR(DDP_COMPONENT_DLI_ASYNC)                                       \
	EXPR(DDP_COMPONENT_INLINE_ROTATE0)                                  \
	EXPR(DDP_COMPONENT_INLINE_ROTATE1)                                  \
	EXPR(DDP_COMPONENT_MMLSYS_BYPASS)                                   \
	EXPR(DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL)                     \
/*155*/	EXPR(DDP_COMPONENT_MAIN_OVL_DISP1_WDMA_VIRTUAL)                     \
	EXPR(DDP_COMPONENT_SUB_OVL_DISP0_PQ0_VIRTUAL)                  \
	EXPR(DDP_COMPONENT_SUB_OVL_DISP1_PQ0_VIRTUAL)					\
	EXPR(DDP_COMPONENT_MML_RSZ0)					\
	EXPR(DDP_COMPONENT_MML_RSZ1)					\
/*160*/	EXPR(DDP_COMPONENT_MML_RSZ2)					\
	EXPR(DDP_COMPONENT_MML_RSZ3)					\
	EXPR(DDP_COMPONENT_MML_HDR0)					\
	EXPR(DDP_COMPONENT_MML_HDR1)					\
	EXPR(DDP_COMPONENT_MML_AAL0)					\
/*165*/	EXPR(DDP_COMPONENT_MML_AAL1)					\
	EXPR(DDP_COMPONENT_MML_TDSHP0)					\
	EXPR(DDP_COMPONENT_MML_TDSHP1)					\
	EXPR(DDP_COMPONENT_MML_COLOR0)					\
	EXPR(DDP_COMPONENT_MML_COLOR1)					\
/*170*/	EXPR(DDP_COMPONENT_MML_MML0)					\
	EXPR(DDP_COMPONENT_MDP_RDMA0)                   \
	EXPR(DDP_COMPONENT_MDP_RDMA1)                   \
	EXPR(DDP_COMPONENT_MML_DLI0)					\
	EXPR(DDP_COMPONENT_MML_DLI1)					\
/*175*/	EXPR(DDP_COMPONENT_MML_DLO0)					\
	EXPR(DDP_COMPONENT_MML_DLO1)					\
	EXPR(DDP_COMPONENT_MML_MUTEX0)					\
	EXPR(DDP_COMPONENT_MML_WROT0)					\
	EXPR(DDP_COMPONENT_MML_WROT1)					\
/*180*/	EXPR(DDP_COMPONENT_MML_WROT2)					\
	EXPR(DDP_COMPONENT_MML_WROT3)					\
	EXPR(DDP_COMPONENT_OVL0_BLEND_CB0)					\
	EXPR(DDP_COMPONENT_OVL0_BLEND_CB1)					\
	EXPR(DDP_COMPONENT_OVL0_BLEND_CB2)					\
/*185*/	EXPR(DDP_COMPONENT_OVL0_BLEND_CB3)					\
	EXPR(DDP_COMPONENT_OVL1_BLEND_CB0)					\
	EXPR(DDP_COMPONENT_OVL1_BLEND_CB1)					\
	EXPR(DDP_COMPONENT_OVL1_BLEND_CB2)					\
	EXPR(DDP_COMPONENT_OVL1_BLEND_CB3)					\
/*190*/	EXPR(DDP_COMPONENT_OVL0_BG_CB0)					\
	EXPR(DDP_COMPONENT_OVL0_BG_CB1)					\
	EXPR(DDP_COMPONENT_OVL0_BG_CB2)					\
	EXPR(DDP_COMPONENT_OVL0_BG_CB3)					\
	EXPR(DDP_COMPONENT_OVL1_BG_CB0)					\
/*195*/	EXPR(DDP_COMPONENT_OVL1_BG_CB1)					\
	EXPR(DDP_COMPONENT_OVL1_BG_CB2)					\
	EXPR(DDP_COMPONENT_OVL1_BG_CB3)					\
	EXPR(DDP_COMPONENT_OVL0_PQ_OUT_CB0)					\
	EXPR(DDP_COMPONENT_OVL0_PQ_OUT_CB1)					\
/*200*/	EXPR(DDP_COMPONENT_OVL0_PQ_OUT_CB2)					\
	EXPR(DDP_COMPONENT_OVL0_PQ_OUT_CB3)					\
	EXPR(DDP_COMPONENT_OVL1_PQ_OUT_CB0)					\
	EXPR(DDP_COMPONENT_OVL1_PQ_OUT_CB1)					\
	EXPR(DDP_COMPONENT_OVL1_PQ_OUT_CB2)					\
/*205*/	EXPR(DDP_COMPONENT_OVL1_PQ_OUT_CB3)					\
	EXPR(DDP_COMPONENT_OVL0_PQ_IN_CB0)					\
	EXPR(DDP_COMPONENT_OVL0_PQ_IN_CB1)					\
	EXPR(DDP_COMPONENT_OVL0_PQ_IN_CB2)					\
	EXPR(DDP_COMPONENT_OVL1_PQ_IN_CB0)					\
/*210*/	EXPR(DDP_COMPONENT_OVL1_PQ_IN_CB1)					\
	EXPR(DDP_COMPONENT_OVL1_PQ_IN_CB2)					\
	EXPR(DDP_COMPONENT_PQ0_IN_CB0)					\
	EXPR(DDP_COMPONENT_PQ0_IN_CB1)					\
	EXPR(DDP_COMPONENT_PQ0_IN_CB2)					\
/*215*/	EXPR(DDP_COMPONENT_PQ0_IN_CB3)					\
	EXPR(DDP_COMPONENT_PQ0_IN_CB4)					\
	EXPR(DDP_COMPONENT_PQ1_IN_CB0)					\
	EXPR(DDP_COMPONENT_PQ1_IN_CB1)					\
	EXPR(DDP_COMPONENT_PQ1_IN_CB2)					\
/*220*/	EXPR(DDP_COMPONENT_PQ1_IN_CB3)					\
	EXPR(DDP_COMPONENT_PQ1_IN_CB4)					\
	EXPR(DDP_COMPONENT_PQ0_OUT_CB0)					\
	EXPR(DDP_COMPONENT_PQ0_OUT_CB1)					\
	EXPR(DDP_COMPONENT_PQ0_OUT_CB2)					\
/*225*/	EXPR(DDP_COMPONENT_PQ0_OUT_CB3)					\
	EXPR(DDP_COMPONENT_PQ0_OUT_CB4)					\
	EXPR(DDP_COMPONENT_PQ1_OUT_CB0)					\
	EXPR(DDP_COMPONENT_PQ1_OUT_CB1)					\
	EXPR(DDP_COMPONENT_PQ1_OUT_CB2)					\
/*230*/	EXPR(DDP_COMPONENT_PQ1_OUT_CB3)					\
	EXPR(DDP_COMPONENT_PQ1_OUT_CB4)					\
	EXPR(DDP_COMPONENT_PANEL0_COMP_OUT_CB0)					\
	EXPR(DDP_COMPONENT_PANEL0_COMP_OUT_CB1)					\
	EXPR(DDP_COMPONENT_PANEL0_COMP_OUT_CB2)					\
/*235*/	EXPR(DDP_COMPONENT_PANEL0_COMP_OUT_CB3)					\
	EXPR(DDP_COMPONENT_PANEL0_COMP_OUT_CB4)					\
	EXPR(DDP_COMPONENT_PANEL1_COMP_OUT_CB0)					\
	EXPR(DDP_COMPONENT_PANEL1_COMP_OUT_CB1)					\
	EXPR(DDP_COMPONENT_PANEL1_COMP_OUT_CB2)					\
/*240*/	EXPR(DDP_COMPONENT_PANEL1_COMP_OUT_CB3)					\
	EXPR(DDP_COMPONENT_PANEL1_COMP_OUT_CB4)					\
	EXPR(DDP_COMPONENT_COMP0_OUT_CB0)					\
	EXPR(DDP_COMPONENT_COMP0_OUT_CB1)					\
	EXPR(DDP_COMPONENT_COMP0_OUT_CB2)					\
/*245*/	EXPR(DDP_COMPONENT_COMP0_OUT_CB3)					\
	EXPR(DDP_COMPONENT_COMP0_OUT_CB4)					\
	EXPR(DDP_COMPONENT_COMP0_OUT_CB5)					\
	EXPR(DDP_COMPONENT_COMP0_OUT_CB6)					\
	EXPR(DDP_COMPONENT_COMP0_OUT_CB7)					\
/*250*/	EXPR(DDP_COMPONENT_COMP1_OUT_CB0)					\
	EXPR(DDP_COMPONENT_COMP1_OUT_CB1)					\
	EXPR(DDP_COMPONENT_COMP1_OUT_CB2)					\
	EXPR(DDP_COMPONENT_COMP1_OUT_CB3)					\
	EXPR(DDP_COMPONENT_COMP1_OUT_CB4)					\
/*255*/	EXPR(DDP_COMPONENT_COMP1_OUT_CB5)					\
	EXPR(DDP_COMPONENT_COMP1_OUT_CB6)					\
	EXPR(DDP_COMPONENT_COMP1_OUT_CB7)					\
	EXPR(DDP_COMPONENT_MERGE0_OUT_CB0)					\
	EXPR(DDP_COMPONENT_MERGE0_OUT_CB1)					\
/*260*/	EXPR(DDP_COMPONENT_MERGE0_OUT_CB2)					\
	EXPR(DDP_COMPONENT_MERGE0_OUT_CB3)					\
	EXPR(DDP_COMPONENT_MERGE0_OUT_CB4)					\
	EXPR(DDP_COMPONENT_MERGE0_OUT_CB5)					\
	EXPR(DDP_COMPONENT_MERGE0_OUT_CB6)					\
/*265*/	EXPR(DDP_COMPONENT_MERGE0_OUT_CB7)					\
	EXPR(DDP_COMPONENT_MERGE1_OUT_CB0)					\
	EXPR(DDP_COMPONENT_MERGE1_OUT_CB1)					\
	EXPR(DDP_COMPONENT_MERGE1_OUT_CB2)					\
	EXPR(DDP_COMPONENT_MERGE1_OUT_CB3)					\
/*270*/	EXPR(DDP_COMPONENT_MERGE1_OUT_CB4)					\
	EXPR(DDP_COMPONENT_MERGE1_OUT_CB5)					\
	EXPR(DDP_COMPONENT_MERGE1_OUT_CB6)					\
	EXPR(DDP_COMPONENT_MERGE1_OUT_CB7)					\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC0)				\
/*275*/	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC1)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC2)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC3)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC4)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC5)				\
/*280*/ EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC6)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC7)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC8)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC9)			\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC10)				\
/*285*/ EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC11)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC12)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLO_ASYNC13)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLI_ASYNC0)			\
	EXPR(DDP_COMPONENT_OVLSYS_DLI_ASYNC1)				\
/*290*/ EXPR(DDP_COMPONENT_OVLSYS_DLI_ASYNC2)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLI_ASYNC3)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLI_ASYNC4)				\
	EXPR(DDP_COMPONENT_OVLSYS_DLI_ASYNC5)				\
	EXPR(DDP_COMPONENT_ODDMR0)							\
/*295*/ EXPR(DDP_COMPONENT_ODDMR1)							\
	EXPR(DDP_COMPONENT_POSTALIGN0)						\
	EXPR(DDP_COMPONENT_POSTALIGN1)						\
	EXPR(DDP_COMPONENT_RELAY0)							\
	EXPR(DDP_COMPONENT_RELAY1)							\
/*300*/ EXPR(DDP_COMPONENT_DITHER2)							\
	EXPR(DDP_COMPONENT_DITHER3)							\
	EXPR(DDP_COMPONENT_TDSHP2)							\
	EXPR(DDP_COMPONENT_TDSHP3)							\
	EXPR(DDP_COMPONENT_DLI_ASYNC8)							\
/*305*/ EXPR(DDP_COMPONENT_DLI_ASYNC9)							\
	EXPR(DDP_COMPONENT_DLI_ASYNC10)							\
	EXPR(DDP_COMPONENT_DLI_ASYNC11)							\
	EXPR(DDP_COMPONENT_DSC2)							\
	EXPR(DDP_COMPONENT_DSC3)							\
/*310*/ EXPR(DDP_COMPONENT_OVLSYS_RSZ1)							\
	EXPR(DDP_COMPONENT_OVLSYS_RSZ2)							\
	EXPR(DDP_COMPONENT_OVLSYS_WDMA0)							\
	EXPR(DDP_COMPONENT_OVLSYS_WDMA1)							\
	EXPR(DDP_COMPONENT_OVLSYS_WDMA2)							\
/*315*/ EXPR(DDP_COMPONENT_OVLSYS_WDMA3)							\
	EXPR(DDP_COMPONENT_OVLSYS_UFBC_WDMA0)							\
	EXPR(DDP_COMPONENT_OVLSYS_UFBC_WDMA1)							\
	EXPR(DDP_COMPONENT_OVLSYS_Y2R0)							\
	EXPR(DDP_COMPONENT_OVLSYS_Y2R1)							\
/*320*/	EXPR(DDP_COMPONENT_OVLSYS_Y2R2)							\
	EXPR(DDP_COMPONENT_OVLSYS_Y2R3)							\
	EXPR(DDP_COMPONENT_OVL0_BG_CB4)							\
	EXPR(DDP_COMPONENT_OVL1_BG_CB4)							\
	EXPR(DDP_COMPONENT_TDSHP0_VIRTUAL)							\
/*325*/	EXPR(DDP_COMPONENT_OVL7_2L_VIRTUAL0)			\
	EXPR(DDP_COMPONENT_ID_MAX)
#define DECLARE_NUM(ENUM) ENUM,
#define DECLARE_STR(STR) #STR,

enum mtk_ddp_comp_id { DECLARE_DDP_COMP(DECLARE_NUM) };

#ifdef IF_ZERO /* Origin enum define */
enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_COLOR2,
	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_OVL2,
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL1_VIRTUAL0,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_PWM2,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_Y2R0,
	DDP_COMPONENT_Y2R1,
	DDP_COMPONENT_Y2R0_VIRTUAL0,
	DDP_COMPONENT_Y2R1_VIRTUAL0,
	DDP_COMPONENT_DLO_ASYNC,
	DDP_COMPONENT_DLI_ASYNC,
	DDP_COMPONENT_INLINE_ROTATE0,
	DDP_COMPONENT_INLINE_ROTATE1,
	DDP_COMPONENT_MMLSYS_BYPASS,
	DDP_COMPONENT_ID_MAX,
};
#endif

struct mtk_ddp_comp;
struct cmdq_pkt;
enum mtk_ddp_comp_trigger_flag {
	MTK_TRIG_FLAG_PRE_TRIGGER,
	MTK_TRIG_FLAG_TRIGGER,
	MTK_TRIG_FLAG_EOF,
	MTK_TRIG_FLAG_LAYER_REC,
};

enum mtk_ddp_io_cmd {
	REQ_PANEL_EXT,
	MTK_IO_CMD_RDMA_GOLDEN_SETTING,
	MTK_IO_CMD_OVL_GOLDEN_SETTING,
	DSI_START_VDO_MODE,
	DSI_STOP_VDO_MODE,
	ESD_CHECK_READ,
	ESD_CHECK_CMP,
	REQ_ESD_EINT_COMPAT,
	COMP_REG_START,
	CONNECTOR_ENABLE,
	CONNECTOR_DISABLE,
	CONNECTOR_RESET,
	CONNECTOR_READ_EPILOG,
	CONNECTOR_IS_ENABLE,
	CONNECTOR_PANEL_ENABLE,
	CONNECTOR_PANEL_DISABLE,
	OVL_ALL_LAYER_OFF,
	IRQ_LEVEL_ALL,
	IRQ_LEVEL_NORMAL,
	IRQ_LEVEL_IDLE,
	DSI_VFP_IDLE_MODE,
	DSI_VFP_DEFAULT_MODE,
	DSI_GET_TIMING,
	DSI_GET_MODE_BY_MAX_VREFRESH,
	DSI_FILL_MODE_BY_CONNETOR,
	PMQOS_SET_BW,
	PMQOS_SET_HRT_BW,
	PMQOS_UPDATE_BW,
	OVL_REPLACE_BOOTUP_MVA,
	BACKUP_INFO_CMP,
	LCM_RESET,
	LCM_CUST_FUNC,
	DSI_SEND_DDIC_CMD_PACK,
	DSI_SET_BL,
	DSI_SET_BL_AOD,
	DSI_SET_BL_GRP,
	DSI_SET_BL_ELVSS,
	DSI_HBM_SET,
	DSI_HBM_GET_STATE,
	DSI_HBM_GET_WAIT_STATE,
	DSI_HBM_SET_WAIT_STATE,
	DSI_HBM_WAIT,
	LCM_ATA_CHECK,
	DSI_SET_CRTC_AVAIL_MODES,
	DSI_TIMING_CHANGE,
	GET_PANEL_NAME,
	GET_ALL_CONNECTOR_PANEL_NAME,
	DSI_CHANGE_MODE,
	BACKUP_OVL_STATUS,
	MIPI_HOPPING,
	PANEL_OSC_HOPPING,
	MODE_SWITCH_INDEX,
	SET_MMCLK_BY_DATARATE,
	GET_FRAME_HRT_BW_BY_DATARATE,
	GET_FRAME_HRT_BW_BY_MODE,
	DSI_SEND_DDIC_CMD,
	DSI_READ_DDIC_CMD,
	DSI_GET_VIRTUAL_HEIGH,
	DSI_GET_VIRTUAL_WIDTH,
	FRAME_DIRTY,
	PQ_DIRTY,
	DSI_LFR_SET,
	DSI_LFR_UPDATE,
	DSI_LFR_STATUS_CHECK,
	WDMA_WRITE_DST_ADDR0,
	WDMA_READ_DST_SIZE,
	/*Msync 2.0 cmd start*/
	DSI_MSYNC_SEND_DDIC_CMD,
	DSI_MSYNC_SWITCH_TE_LEVEL,
	DSI_MSYNC_SWITCH_TE_LEVEL_GRP,
	DSI_MSYNC_CMD_SET_MIN_FPS,
	DSI_ADD_VFP_FOR_MSYNC,
	DSI_VFP_EARLYSTOP,
	DSI_RESTORE_VFP_FOR_MSYNC,
	DSI_READ_VFP_PERIOD,
	DSI_INIT_VFP_EARLY_STOP,
	DSI_DISABLE_VFP_EALRY_STOP,
	/*Msync 2.0 cmd end*/
	DUAL_TE_INIT,
	OVL_GET_SOURCE_BPC,
	DSI_GET_CMD_MODE_LINE_TIME,
	DSI_DUMP_LCM_INFO,
	DSI_SET_TARGET_LINE,
	DSI_READ_PANELID,
	ODDMR_BL_CHG,
	ODDMR_TIMING_CHG,
	ODDMR_TRIG_CTL,
	COMP_ADD_HRT,
	COMP_SEC_CFG,
	OVL_GET_SELFLOOP_SUPPORT,
	RSZ_GET_TILE_LENGTH,
	RSZ_GET_IN_MAX_HEIGHT,
	DSI_GET_BPC,
};

struct golden_setting_context {
	unsigned int is_vdo_mode;
	unsigned int is_dc;
	unsigned int dst_width;
	unsigned int dst_height;
	// add for rdma default goden setting
	unsigned int vrefresh;
};

struct mtk_ddp_config {
	void *pa;
	unsigned int w;
	unsigned int h;
	unsigned int x;
	unsigned int y;
	unsigned int vrefresh;
	unsigned int clock;
	unsigned int bpc;
	struct golden_setting_context *p_golden_setting_context;
	unsigned int source_bpc;
};

struct mtk_oddmr_timing {
	unsigned int bl_level;
	unsigned int mode_chg_index;
	unsigned int vrefresh;
	u16 hdisplay;
	u16 vdisplay;
};

struct mtk_ddp_fb_info {
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	size_t size;
	phys_addr_t fb_pa;
	struct mtk_drm_gem_obj *fb_gem;
};

struct mtk_ddp_comp_funcs {
	void (*config)(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg,
		       struct cmdq_pkt *handle);
	void (*prepare)(struct mtk_ddp_comp *comp);
	void (*unprepare)(struct mtk_ddp_comp *comp);
	void (*start)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*stop)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*enable_vblank)(struct mtk_ddp_comp *comp, struct drm_crtc *crtc,
			      struct cmdq_pkt *handle);
	void (*disable_vblank)(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle);
	void (*layer_on)(struct mtk_ddp_comp *comp, unsigned int idx,
			 unsigned int ext_idx, struct cmdq_pkt *handle);
	void (*layer_off)(struct mtk_ddp_comp *comp, unsigned int idx,
			  unsigned int ext_idx, struct cmdq_pkt *handle);
	void (*layer_config)(struct mtk_ddp_comp *comp, unsigned int idx,
			     struct mtk_plane_state *state,
			     struct cmdq_pkt *handle);
	void (*gamma_set)(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state,
			  struct cmdq_pkt *handle);
	void (*first_cfg)(struct mtk_ddp_comp *comp,
		       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle);
	void (*bypass)(struct mtk_ddp_comp *comp, int bypass,
		struct cmdq_pkt *handle);
	void (*config_trigger)(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle,
			       enum mtk_ddp_comp_trigger_flag trig_flag);
	void (*addon_config)(struct mtk_ddp_comp *comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *handle);
	void (*config_begin)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	int (*io_cmd)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		      enum mtk_ddp_io_cmd cmd, void *params);
	int (*user_cmd)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		      unsigned int cmd, void *params);
	void (*connect)(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			enum mtk_ddp_comp_id next);
	int (*is_busy)(struct mtk_ddp_comp *comp);
	void (*mml_calc_cfg)(struct mtk_ddp_comp *comp,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *handle);
	void (*dump)(struct mtk_ddp_comp *comp);
	void (*reset)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
};

struct mtk_ddp_comp {
	struct clk *clk;
	void __iomem *regs;
	resource_size_t regs_pa;
	resource_size_t *larb_cons;
	int irq;
	struct device *larb_dev;
	struct device **larb_devs;
	struct device *dev;
	struct mtk_drm_crtc *mtk_crtc;
	u32 larb_id;
	u32 *larb_ids;
	int larb_num;
	enum mtk_ddp_comp_id id;
	u32 sub_idx;
	struct drm_framebuffer *fb;
	const struct mtk_ddp_comp_funcs *funcs;
	void *comp_mode;
	struct cmdq_base *cmdq_base;
#ifdef IF_ZERO
	u8 cmdq_subsys;
#endif
	struct icc_path *qos_req;
	struct icc_path *qos_req_other;
	struct icc_path *fbdc_qos_req;
	struct icc_path *hrt_qos_req;
	struct icc_path *hrt_qos_req_other;
	bool blank_mode;
	u32 qos_bw;
	u32 qos_bw_other;
	u32 last_qos_bw;
	u32 last_qos_bw_other;
	u32 fbdc_bw;
	u32 hrt_bw;
};

static inline void mtk_ddp_comp_config(struct mtk_ddp_comp *comp,
				       struct mtk_ddp_config *cfg,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->config && !comp->blank_mode)
		comp->funcs->config(comp, cfg, handle);
}

static inline void mtk_ddp_comp_prepare(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->prepare && !comp->blank_mode)
		comp->funcs->prepare(comp);
}

static inline void mtk_ddp_comp_unprepare(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->unprepare && !comp->blank_mode)
		comp->funcs->unprepare(comp);
}

static inline void mtk_ddp_comp_start(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->start && !comp->blank_mode)
		comp->funcs->start(comp, handle);
}

static inline void mtk_ddp_comp_stop(struct mtk_ddp_comp *comp,
				     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->stop && !comp->blank_mode)
		comp->funcs->stop(comp, handle);
}

static inline void mtk_ddp_comp_reset(struct mtk_ddp_comp *comp,
				     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->reset && !comp->blank_mode)
		comp->funcs->reset(comp, handle);
}

static inline void mtk_ddp_comp_enable_vblank(struct mtk_ddp_comp *comp,
					      struct drm_crtc *crtc,
					      struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->enable_vblank &&
			!comp->blank_mode)
		comp->funcs->enable_vblank(comp, crtc, handle);
}

static inline void mtk_ddp_comp_disable_vblank(struct mtk_ddp_comp *comp,
					       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->disable_vblank &&
			!comp->blank_mode)
		comp->funcs->disable_vblank(comp, handle);
}

static inline void mtk_ddp_comp_layer_on(struct mtk_ddp_comp *comp,
					 unsigned int idx, unsigned int ext_idx,
					 struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_on && !comp->blank_mode)
		comp->funcs->layer_on(comp, idx, ext_idx, handle);
}

static inline void mtk_ddp_comp_layer_off(struct mtk_ddp_comp *comp,
					  unsigned int idx,
					  unsigned int ext_idx,
					  struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_off && !comp->blank_mode)
		comp->funcs->layer_off(comp, idx, ext_idx, handle);
}

static inline void mtk_ddp_comp_layer_config(struct mtk_ddp_comp *comp,
					     unsigned int idx,
					     struct mtk_plane_state *state,
					     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_config &&
			!comp->blank_mode) {
		DDPDBG("[DRM]func:%s, line:%d ==>\n",
			__func__, __LINE__);
		DDPDBG("comp_funcs:0x%p, layer_config:0x%p\n",
			comp->funcs, comp->funcs->layer_config);

		comp->funcs->layer_config(comp, idx, state, handle);
	}
}

static inline void mtk_ddp_gamma_set(struct mtk_ddp_comp *comp,
				     struct drm_crtc_state *state,
				     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->gamma_set && !comp->blank_mode)
		comp->funcs->gamma_set(comp, state, handle);
}

static inline void mtk_ddp_comp_bypass(struct mtk_ddp_comp *comp, int bypass,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->bypass && !comp->blank_mode)
		comp->funcs->bypass(comp, bypass, handle);
}

static inline void mtk_ddp_comp_first_cfg(struct mtk_ddp_comp *comp,
				       struct mtk_ddp_config *cfg,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->first_cfg && !comp->blank_mode)
		comp->funcs->first_cfg(comp, cfg, handle);
}

static inline void
mtk_ddp_comp_config_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			    enum mtk_ddp_comp_trigger_flag flag)
{
	if (comp && comp->funcs && comp->funcs->config_trigger &&
			!comp->blank_mode)
		comp->funcs->config_trigger(comp, handle, flag);
}

static inline void
mtk_ddp_comp_addon_config(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			  enum mtk_ddp_comp_id next,
			  union mtk_addon_config *addon_config,
			  struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->addon_config &&
			!comp->blank_mode)
		comp->funcs->addon_config(comp, prev, next, addon_config,
				handle);
}

static inline void
mtk_ddp_comp_config_begin(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->config_begin &&
			!comp->blank_mode)
		comp->funcs->config_begin(comp, handle);
}

static inline void
mtk_ddp_comp_mml_calc_cfg(struct mtk_ddp_comp *comp,
			  union mtk_addon_config *addon_config)
{
	if (comp && comp->funcs && comp->funcs->addon_config &&
		!comp->blank_mode)
		comp->funcs->mml_calc_cfg(comp, addon_config, NULL);
}

static inline void
mtk_ddp_comp_dump(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->dump && !comp->blank_mode)
		comp->funcs->dump(comp);
}

static inline int mtk_ddp_comp_io_cmd(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle,
				      enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = -EINVAL;

	if (comp && comp->funcs && comp->funcs->io_cmd && !comp->blank_mode)
		ret = comp->funcs->io_cmd(comp, handle, io_cmd, params);

	return ret;
}

static inline int
mtk_ddp_comp_is_busy(struct mtk_ddp_comp *comp)
{
	int ret = 0;

	if (comp && comp->funcs && comp->funcs->is_busy && !comp->blank_mode)
		ret = comp->funcs->is_busy(comp);

	return ret;
}

static inline void mtk_ddp_cpu_mask_write(struct mtk_ddp_comp *comp,
					  unsigned int off, unsigned int val,
					  unsigned int mask)
{
	unsigned int v = (readl(comp->regs + off) & (~mask));

	v += (val & mask);
	writel_relaxed(v, comp->regs + off);
}

enum mtk_ddp_comp_id mtk_ddp_comp_get_id(struct device_node *node,
					 enum mtk_ddp_comp_type comp_type);
struct mtk_ddp_comp *mtk_ddp_comp_find_by_id(struct drm_crtc *crtc,
					     enum mtk_ddp_comp_id comp_id);
unsigned int mtk_drm_find_possible_crtc_by_comp(struct drm_device *drm,
						struct mtk_ddp_comp ddp_comp);
int mtk_ddp_comp_init(struct device *dev, struct device_node *comp_node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs);
int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);
int mtk_ddp_comp_get_type(enum mtk_ddp_comp_id comp_id);
bool mtk_dsi_is_cmd_mode(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_output(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_get_name(struct mtk_ddp_comp *comp, char *buf, int buf_len);
int mtk_ovl_layer_num(struct mtk_ddp_comp *comp);
void mtk_ddp_write(struct mtk_ddp_comp *comp, unsigned int value,
		   unsigned int offset, void *handle);
void mtk_ddp_write_relaxed(struct mtk_ddp_comp *comp, unsigned int value,
			   unsigned int offset, void *handle);
void mtk_ddp_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, unsigned int mask, void *handle);
void mtk_ddp_write_mask_cpu(struct mtk_ddp_comp *comp,
			unsigned int value, unsigned int offset,
			unsigned int mask);
void mtk_ddp_comp_clk_prepare(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_clk_unprepare(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_iommu_enable(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle);
void mt6779_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6885_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6983_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6985_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6895_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6873_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6853_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6833_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6879_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6855_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
int mtk_ddp_comp_helper_get_opt(struct mtk_ddp_comp *comp,
				enum MTK_DRM_HELPER_OPT option);

void mtk_ddp_comp_pm_enable(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_pm_disable(struct mtk_ddp_comp *comp);

#endif /* MTK_DRM_DDP_COMP_H */
