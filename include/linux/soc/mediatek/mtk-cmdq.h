/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_CMDQ_H__
#define __MTK_CMDQ_H__

#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>

/* display events in command queue(CMDQ) */
enum cmdq_event {
	/* MDP start frame */
	CMDQ_EVENT_MDP_RDMA0_SOF,			/* 0 */
	CMDQ_EVENT_MDP_RDMA1_SOF,			/* 1 */
	CMDQ_EVENT_MDP_RSZ0_SOF,			/* 2 */
	CMDQ_EVENT_MDP_RSZ1_SOF,			/* 3 */
	CMDQ_EVENT_MDP_RSZ2_SOF,			/* 4 */
	CMDQ_EVENT_MDP_TDSHP_SOF,			/* 5 */
	CMDQ_EVENT_MDP_TDSHP0_SOF,			/* 6 */
	CMDQ_EVENT_MDP_TDSHP1_SOF,			/* 7 */
	CMDQ_EVENT_MDP_WDMA_SOF,			/* 8 */
	CMDQ_EVENT_MDP_WROT_SOF,			/* 9 */
	CMDQ_EVENT_MDP_WROT0_SOF,			/* 10 */
	CMDQ_EVENT_MDP_WROT1_SOF,			/* 11 */
	CMDQ_EVENT_MDP_COLOR_SOF,			/* 12 */
	CMDQ_EVENT_MDP_MVW_SOF,				/* 13 */
	CMDQ_EVENT_MDP_CROP_SOF,			/* 14 */
	CMDQ_EVENT_MDP_AAL_SOF,				/* 15 */

	/* Display start frame */
	CMDQ_EVENT_DISP_OVL0_SOF,			/* 16 */
	CMDQ_EVENT_DISP_OVL1_SOF,			/* 17 */
	CMDQ_EVENT_DISP_2L_OVL0_SOF,			/* 18 */
	CMDQ_EVENT_DISP_2L_OVL1_SOF,			/* 19 */
	CMDQ_EVENT_DISP_RDMA0_SOF,			/* 20 */
	CMDQ_EVENT_DISP_RDMA1_SOF,			/* 21 */
	CMDQ_EVENT_DISP_RDMA2_SOF,			/* 22 */
	CMDQ_EVENT_DISP_WDMA0_SOF,			/* 23 */
	CMDQ_EVENT_DISP_WDMA1_SOF,			/* 24 */
	CMDQ_EVENT_DISP_COLOR_SOF,			/* 25 */
	CMDQ_EVENT_DISP_COLOR0_SOF,			/* 26 */
	CMDQ_EVENT_DISP_COLOR1_SOF,			/* 27 */
	CMDQ_EVENT_DISP_CCORR_SOF,			/* 28 */
	CMDQ_EVENT_DISP_CCORR0_SOF,			/* 29 */
	CMDQ_EVENT_DISP_CCORR1_SOF,			/* 30 */
	CMDQ_EVENT_DISP_AAL_SOF,			/* 31 */
	CMDQ_EVENT_DISP_AAL0_SOF,			/* 32 */
	CMDQ_EVENT_DISP_AAL1_SOF,			/* 33 */
	CMDQ_EVENT_DISP_GAMMA_SOF,			/* 34 */
	CMDQ_EVENT_DISP_GAMMA0_SOF,			/* 35 */
	CMDQ_EVENT_DISP_GAMMA1_SOF,			/* 36 */
	CMDQ_EVENT_DISP_DITHER_SOF,			/* 37 */
	CMDQ_EVENT_DISP_DITHER0_SOF,			/* 38 */
	CMDQ_EVENT_DISP_DITHER1_SOF,			/* 39 */
	CMDQ_EVENT_DISP_UFOE_SOF,			/* 40 */
	CMDQ_EVENT_DISP_PWM0_SOF,			/* 41 */
	CMDQ_EVENT_DISP_PWM1_SOF,			/* 42 */
	CMDQ_EVENT_DISP_OD_SOF,				/* 43 */
	CMDQ_EVENT_DISP_DSC_SOF,			/* 44 */

	CMDQ_EVENT_UFOD_RAMA0_L0_SOF,			/* 45 */
	CMDQ_EVENT_UFOD_RAMA0_L1_SOF,			/* 46 */
	CMDQ_EVENT_UFOD_RAMA0_L2_SOF,			/* 47 */
	CMDQ_EVENT_UFOD_RAMA0_L3_SOF,			/* 48 */
	CMDQ_EVENT_UFOD_RAMA1_L0_SOF,			/* 49 */
	CMDQ_EVENT_UFOD_RAMA1_L1_SOF,			/* 50 */
	CMDQ_EVENT_UFOD_RAMA1_L2_SOF,			/* 51 */
	CMDQ_EVENT_UFOD_RAMA1_L3_SOF,			/* 52 */

	/* MDP frame done */
	CMDQ_EVENT_MDP_RDMA0_EOF,			/* 53 */
	CMDQ_EVENT_MDP_RDMA1_EOF,			/* 54 */
	CMDQ_EVENT_MDP_RSZ0_EOF,			/* 55 */
	CMDQ_EVENT_MDP_RSZ1_EOF,			/* 56 */
	CMDQ_EVENT_MDP_RSZ2_EOF,			/* 57 */
	CMDQ_EVENT_MDP_TDSHP_EOF,			/* 58 */
	CMDQ_EVENT_MDP_TDSHP0_EOF,			/* 59 */
	CMDQ_EVENT_MDP_TDSHP1_EOF,			/* 60 */
	CMDQ_EVENT_MDP_WDMA_EOF,			/* 61 */
	CMDQ_EVENT_MDP_WROT_WRITE_EOF,			/* 62 */
	CMDQ_EVENT_MDP_WROT_READ_EOF,			/* 63 */
	CMDQ_EVENT_MDP_WROT0_WRITE_EOF,			/* 64 */
	CMDQ_EVENT_MDP_WROT0_READ_EOF,			/* 65 */
	CMDQ_EVENT_MDP_WROT1_WRITE_EOF,			/* 66 */
	CMDQ_EVENT_MDP_WROT1_READ_EOF,			/* 67 */
	CMDQ_EVENT_MDP_WROT0_W_EOF,			/* 68 */
	CMDQ_EVENT_MDP_WROT0_R_EOF,			/* 69 */
	CMDQ_EVENT_MDP_WROT1_W_EOF,			/* 70 */
	CMDQ_EVENT_MDP_WROT1_R_EOF,			/* 71 */
	CMDQ_EVENT_MDP_COLOR_EOF,			/* 72 */
	CMDQ_EVENT_MDP_CROP_EOF,			/* 73 */

	/* Display frame done */
	CMDQ_EVENT_DISP_OVL0_EOF,			/* 74 */
	CMDQ_EVENT_DISP_OVL1_EOF,			/* 75 */
	CMDQ_EVENT_DISP_2L_OVL0_EOF,			/* 76 */
	CMDQ_EVENT_DISP_2L_OVL1_EOF,			/* 77 */
	CMDQ_EVENT_DISP_RDMA0_EOF,			/* 78 */
	CMDQ_EVENT_DISP_RDMA1_EOF,			/* 79 */
	CMDQ_EVENT_DISP_RDMA2_EOF,			/* 80 */
	CMDQ_EVENT_DISP_WDMA0_EOF,			/* 81 */
	CMDQ_EVENT_DISP_WDMA1_EOF,			/* 82 */
	CMDQ_EVENT_DISP_COLOR_EOF,			/* 83 */
	CMDQ_EVENT_DISP_COLOR0_EOF,			/* 84 */
	CMDQ_EVENT_DISP_COLOR1_EOF,			/* 85 */
	CMDQ_EVENT_DISP_CCORR_EOF,			/* 86 */
	CMDQ_EVENT_DISP_CCORR0_EOF,			/* 87 */
	CMDQ_EVENT_DISP_CCORR1_EOF,			/* 88 */
	CMDQ_EVENT_DISP_AAL_EOF,			/* 89 */
	CMDQ_EVENT_DISP_AAL0_EOF,			/* 90 */
	CMDQ_EVENT_DISP_AAL1_EOF,			/* 91 */
	CMDQ_EVENT_DISP_GAMMA_EOF,			/* 92 */
	CMDQ_EVENT_DISP_GAMMA0_EOF,			/* 93 */
	CMDQ_EVENT_DISP_GAMMA1_EOF,			/* 94 */
	CMDQ_EVENT_DISP_DITHER_EOF,			/* 95 */
	CMDQ_EVENT_DISP_DITHER0_EOF,			/* 96 */
	CMDQ_EVENT_DISP_DITHER1_EOF,			/* 97 */
	CMDQ_EVENT_DISP_UFOE_EOF,			/* 98 */
	CMDQ_EVENT_DISP_OD_EOF,				/* 99 */
	CMDQ_EVENT_DISP_OD_RDMA_EOF,			/* 100 */
	CMDQ_EVENT_DISP_OD_WDMA_EOF,			/* 101 */
	CMDQ_EVENT_DISP_DSC_EOF,			/* 102 */
	CMDQ_EVENT_DISP_DSI0_EOF,			/* 103 */
	CMDQ_EVENT_DISP_DSI1_EOF,			/* 104 */
	CMDQ_EVENT_DISP_DPI0_EOF,			/* 105 */

	CMDQ_EVENT_UFOD_RAMA0_L0_EOF,			/* 106 */
	CMDQ_EVENT_UFOD_RAMA0_L1_EOF,			/* 107 */
	CMDQ_EVENT_UFOD_RAMA0_L2_EOF,			/* 108 */
	CMDQ_EVENT_UFOD_RAMA0_L3_EOF,			/* 109 */
	CMDQ_EVENT_UFOD_RAMA1_L0_EOF,			/* 110 */
	CMDQ_EVENT_UFOD_RAMA1_L1_EOF,			/* 111 */
	CMDQ_EVENT_UFOD_RAMA1_L2_EOF,			/* 112 */
	CMDQ_EVENT_UFOD_RAMA1_L3_EOF,			/* 113 */

	/* Mutex frame done */
	/* DISPSYS */
	CMDQ_EVENT_MUTEX0_STREAM_EOF,			/* 114 */
	/* DISPSYS */
	CMDQ_EVENT_MUTEX1_STREAM_EOF,			/* 115 */
	/* DISPSYS */
	CMDQ_EVENT_MUTEX2_STREAM_EOF,			/* 116 */
	/* DISPSYS */
	CMDQ_EVENT_MUTEX3_STREAM_EOF,			/* 117 */
	/* DISPSYS, please refer to disp_hal.h */
	CMDQ_EVENT_MUTEX4_STREAM_EOF,			/* 118 */
	/* DpFramework */
	CMDQ_EVENT_MUTEX5_STREAM_EOF,			/* 119 */
	/* DpFramework */
	CMDQ_EVENT_MUTEX6_STREAM_EOF,			/* 120 */
	/* DpFramework */
	CMDQ_EVENT_MUTEX7_STREAM_EOF,			/* 121 */
	/* DpFramework */
	CMDQ_EVENT_MUTEX8_STREAM_EOF,			/* 122 */
	/* DpFramework via CMDQ_IOCTL_LOCK_MUTEX */
	CMDQ_EVENT_MUTEX9_STREAM_EOF,			/* 123 */
	CMDQ_EVENT_MUTEX10_STREAM_EOF,			/* 124 */
	CMDQ_EVENT_MUTEX11_STREAM_EOF,			/* 125 */
	CMDQ_EVENT_MUTEX12_STREAM_EOF,			/* 126 */
	CMDQ_EVENT_MUTEX13_STREAM_EOF,			/* 127 */
	CMDQ_EVENT_MUTEX14_STREAM_EOF,			/* 128 */
	CMDQ_EVENT_MUTEX15_STREAM_EOF,			/* 129 */

	/* Display underrun */
	CMDQ_EVENT_DISP_RDMA0_UNDERRUN,			/* 130 */
	CMDQ_EVENT_DISP_RDMA1_UNDERRUN,			/* 131 */
	CMDQ_EVENT_DISP_RDMA2_UNDERRUN,			/* 132 */

	/* Display TE */
	CMDQ_EVENT_DSI_TE,				/* 133 */
	CMDQ_EVENT_DSI0_TE,				/* 134 */
	CMDQ_EVENT_DSI1_TE,				/* 135 */
	CMDQ_EVENT_MDP_DSI0_TE_SOF,			/* 136 */
	CMDQ_EVENT_MDP_DSI1_TE_SOF,			/* 137 */
	CMDQ_EVENT_DISP_DSI0_SOF,			/* 138 */
	CMDQ_EVENT_DISP_DSI1_SOF,			/* 139 */
	CMDQ_EVENT_DSI0_TO_GCE_MMCK0,			/* 140 */
	CMDQ_EVENT_DSI0_TO_GCE_MMCK1,			/* 141 */
	CMDQ_EVENT_DSI0_TO_GCE_MMCK2,			/* 142 */
	CMDQ_EVENT_DSI0_TO_GCE_MMCK3,			/* 143 */
	CMDQ_EVENT_DSI0_TO_GCE_MMCK4,			/* 144 */
	CMDQ_EVENT_DSI1_TO_GCE_MMCK0,			/* 145 */
	CMDQ_EVENT_DSI1_TO_GCE_MMCK1,			/* 146 */
	CMDQ_EVENT_DSI1_TO_GCE_MMCK2,			/* 147 */
	CMDQ_EVENT_DSI1_TO_GCE_MMCK3,			/* 148 */
	CMDQ_EVENT_DSI1_TO_GCE_MMCK4,			/* 149 */
	CMDQ_EVENT_DSI0_IRQ_EVENT,			/* 150 */
	CMDQ_EVENT_DSI0_DONE_EVENT,			/* 151 */
	CMDQ_EVENT_DSI1_IRQ_EVENT,			/* 152 */
	CMDQ_EVENT_DSI1_DONE_EVENT,			/* 153 */

	/* Reset Event */
	CMDQ_EVENT_DISP_WDMA0_RST_DONE,			/* 154 */
	CMDQ_EVENT_DISP_WDMA1_RST_DONE,			/* 155 */
	CMDQ_EVENT_MDP_WROT0_RST_DONE,			/* 156 */
	CMDQ_EVENT_MDP_WROT1_RST_DONE,			/* 157 */
	CMDQ_EVENT_MDP_WDMA_RST_DONE,			/* 158 */
	CMDQ_EVENT_MDP_RDMA0_RST_DONE,			/* 159 */
	CMDQ_EVENT_MDP_RDMA1_RST_DONE,			/* 160 */

	/* Display Mutex */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD0,		/* 161 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD1,		/* 162 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD2,		/* 163 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD3,		/* 164 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD4,		/* 165 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD5,		/* 166 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD6,		/* 167 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD7,		/* 168 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD8,		/* 169 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD9,		/* 170 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD10,		/* 171 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD11,		/* 172 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD12,		/* 173 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD13,		/* 174 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD14,		/* 175 */
	CMDQ_EVENT_DISP_MUTEX_ALL_MODULE_UPD15,		/* 176 */

	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE0,	/* 177 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE1,	/* 178 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE2,	/* 179 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE3,	/* 180 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE4,	/* 181 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE5,	/* 182 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE6,	/* 183 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE7,	/* 184 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE8,	/* 185 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE9,	/* 186 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE10,	/* 187 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE11,	/* 188 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE12,	/* 189 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE13,	/* 190 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE14,	/* 191 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE15,	/* 192 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE16,	/* 193 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE17,	/* 194 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE18,	/* 195 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE19,	/* 196 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE20,	/* 197 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE21,	/* 198 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE22,	/* 199 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE23,	/* 200 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE24,	/* 201 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE25,	/* 202 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE26,	/* 203 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE27,	/* 204 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE28,	/* 205 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE29,	/* 206 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE30,	/* 207 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE31,	/* 208 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE32,	/* 209 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE33,	/* 210 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE34,	/* 211 */

	/* ISP frame done */
	CMDQ_EVENT_ISP_PASS2_2_EOF,			/* 212 */
	CMDQ_EVENT_ISP_PASS2_1_EOF,			/* 213 */
	CMDQ_EVENT_ISP_PASS2_0_EOF,			/* 214 */
	CMDQ_EVENT_ISP_PASS1_1_EOF,			/* 215 */
	CMDQ_EVENT_ISP_PASS1_0_EOF,			/* 216 */

	/* ISP (IMGSYS) frame done */
	CMDQ_EVENT_DIP_CQ_THREAD0_EOF,			/* 217 */
	CMDQ_EVENT_DIP_CQ_THREAD1_EOF,			/* 218 */
	CMDQ_EVENT_DIP_CQ_THREAD2_EOF,			/* 219 */
	CMDQ_EVENT_DIP_CQ_THREAD3_EOF,			/* 220 */
	CMDQ_EVENT_DIP_CQ_THREAD4_EOF,			/* 221 */
	CMDQ_EVENT_DIP_CQ_THREAD5_EOF,			/* 222 */
	CMDQ_EVENT_DIP_CQ_THREAD6_EOF,			/* 223 */
	CMDQ_EVENT_DIP_CQ_THREAD7_EOF,			/* 224 */
	CMDQ_EVENT_DIP_CQ_THREAD8_EOF,			/* 225 */
	CMDQ_EVENT_DIP_CQ_THREAD9_EOF,			/* 226 */
	CMDQ_EVENT_DIP_CQ_THREAD10_EOF,			/* 227 */
	CMDQ_EVENT_DIP_CQ_THREAD11_EOF,			/* 228 */
	CMDQ_EVENT_DIP_CQ_THREAD12_EOF,			/* 229 */
	CMDQ_EVENT_DIP_CQ_THREAD13_EOF,			/* 230 */
	CMDQ_EVENT_DIP_CQ_THREAD14_EOF,			/* 231 */
	CMDQ_EVENT_DIP_CQ_THREAD15_EOF,			/* 232 */
	CMDQ_EVENT_DIP_CQ_THREAD16_EOF,			/* 233 */
	CMDQ_EVENT_DIP_CQ_THREAD17_EOF,			/* 234 */
	CMDQ_EVENT_DIP_CQ_THREAD18_EOF,			/* 235 */
	CMDQ_EVENT_DPE_EOF,				/* 236 */
	CMDQ_EVENT_DVE_EOF,				/* 237 */
	CMDQ_EVENT_WMF_EOF,				/* 238 */
	CMDQ_EVENT_GEPF_EOF,				/* 239 */
	CMDQ_EVENT_GEPF_TEMP_EOF,			/* 240 */
	CMDQ_EVENT_GEPF_BYPASS_EOF,			/* 241 */
	CMDQ_EVENT_RSC_EOF,				/* 242 */

	/* ISP (IMGSYS) engine events */
	CMDQ_EVENT_ISP_SENINF_CAM1_2_3_FULL,		/* 243 */
	CMDQ_EVENT_ISP_SENINF_CAM0_FULL,		/* 244 */

	/* VENC frame done */
	CMDQ_EVENT_VENC_EOF,				/* 245 */

	/* JPEG frame done */
	CMDQ_EVENT_JPEG_ENC_EOF,			/* 246 */
	CMDQ_EVENT_JPEG_ENC_PASS2_EOF,			/* 247 */
	CMDQ_EVENT_JPEG_ENC_PASS1_EOF,			/* 248 */
	CMDQ_EVENT_JPEG_DEC_EOF,			/* 249 */

	/* VENC engine events */
	CMDQ_EVENT_VENC_MB_DONE,			/* 250 */
	CMDQ_EVENT_VENC_128BYTE_CNT_DONE,		/* 251 */

	/* ISP (CAMSYS) frame done */
	CMDQ_EVENT_ISP_FRAME_DONE_A,			/* 252 */
	CMDQ_EVENT_ISP_FRAME_DONE_B,			/* 253 */
	CMDQ_EVENT_ISP_CAMSV_0_PASS1_DONE,		/* 254 */
	CMDQ_EVENT_ISP_CAMSV_1_PASS1_DONE,		/* 255 */
	CMDQ_EVENT_ISP_CAMSV_2_PASS1_DONE,		/* 256 */
	CMDQ_EVENT_ISP_TSF_DONE,			/* 257 */

	/* ISP (CAMSYS) engine events */
	CMDQ_EVENT_SENINF_0_FIFO_FULL,			/* 258 */
	CMDQ_EVENT_SENINF_1_FIFO_FULL,			/* 259 */
	CMDQ_EVENT_SENINF_2_FIFO_FULL,			/* 260 */
	CMDQ_EVENT_SENINF_3_FIFO_FULL,			/* 261 */
	CMDQ_EVENT_SENINF_4_FIFO_FULL,			/* 262 */
	CMDQ_EVENT_SENINF_5_FIFO_FULL,			/* 263 */
	CMDQ_EVENT_SENINF_6_FIFO_FULL,			/* 264 */
	CMDQ_EVENT_SENINF_7_FIFO_FULL,			/* 265 */

	/* 6799 New Event */
	CMDQ_EVENT_DISP_DSC1_SOF,			/* 266 */
	CMDQ_EVENT_DISP_DSC2_SOF,			/* 267 */
	CMDQ_EVENT_DISP_RSZ0_SOF,			/* 268 */
	CMDQ_EVENT_DISP_RSZ1_SOF,			/* 269 */
	CMDQ_EVENT_DISP_DSC0_EOF,			/* 270 */
	CMDQ_EVENT_DISP_DSC1_EOF,			/* 271 */
	CMDQ_EVENT_DISP_RSZ0_EOF,			/* 272 */
	CMDQ_EVENT_DISP_RSZ1_EOF,			/* 273 */
	CMDQ_EVENT_DISP_OVL0_RST_DONE,			/* 274 */
	CMDQ_EVENT_DISP_OVL1_RST_DONE,			/* 275 */
	CMDQ_EVENT_DISP_OVL0_2L_RST_DONE,		/* 276 */
	CMDQ_EVENT_DISP_OVL1_2L_RST_DONE,		/* 277 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE35,	/* 278 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE36,	/* 279 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE37,	/* 280 */
	CMDQ_EVENT_DISP_MUTEX_REG_UPD_FOR_MODULE38,	/* 281 */
	CMDQ_EVENT_WPE_A_EOF,				/* 282 */
	CMDQ_EVENT_EAF_EOF,				/* 283 */
	CMDQ_EVENT_VENC_BSDMA_FULL,			/* 284 */
	CMDQ_EVENT_IPU0_EOF,				/* 285 */
	CMDQ_EVENT_IPU1_EOF,				/* 286 */
	CMDQ_EVENT_IPU2_EOF,				/* 287 */
	CMDQ_EVENT_IPU3_EOF,				/* 288 */

	/* 6759 New Event */
	CMDQ_EVENT_DISP_SPLIT_SOF,			/* 289 */
	CMDQ_EVENT_DISP_SPLIT_FRAME_DONE,		/* 290 */
	CMDQ_EVENT_AMD_FRAME_DONE,			/* 291 */

	CMDQ_EVENT_DISP_DPI0_SOF,			/* 292 */
	CMDQ_EVENT_DSI0_TE_INFRA,			/* 293 */

	/* 6739 New Event*/
	CMDQ_EVENT_DISP_DBI0_SOF,			/* 294 */
	CMDQ_EVENT_DISP_DBI0_EOF,			/* 295 */

	/* 6775 New Event*/
	CMDQ_EVENT_MDP_CCORR_SOF,			/* 296 */
	CMDQ_EVENT_MDP_CCORR_FRAME_DONE,		/* 297 */
	CMDQ_EVENT_MDP_AAL_FRAME_DONE,			/* 298 */
	CMDQ_EVENT_WPE_B_FRAME_DONE,			/* 299 */
	CMDQ_EVENT_MFB_DONE,				/* 300 */
	CMDQ_EVENT_OCC_DONE,				/* 301 */
	CMDQ_EVENT_IPU_DONE_1_1,			/* 302 */
	CMDQ_EVENT_IPU_DONE_1_2,			/* 303 */
	CMDQ_EVENT_IPU_DONE_1_0,			/* 304 */
	CMDQ_EVENT_IPU_DONE_1_3,			/* 305 */
	CMDQ_EVENT_IPU_DONE_2_0,			/* 306 */
	CMDQ_EVENT_IPU_DONE_2_1,			/* 307 */
	CMDQ_EVENT_IPU_DONE_2_3,			/* 308 */
	CMDQ_EVENT_IPU_DONE_2_2,			/* 309 */

	/* 6765 New Event */
	CMDQ_EVENT_MDP_CCORR0_SOF,			/* 310 */
	CMDQ_EVENT_MDP_CCORR0_FRAME_DONE,		/* 311 */
	CMDQ_EVENT_IMG_DL_RELAY_SOF,			/* 312 */

	/* Keep this at the end of HW events */
	CMDQ_MAX_HW_EVENT_COUNT = 400,

	/* SW Sync Tokens (Pre-defined) */
	/* Config thread notify trigger thread */
	CMDQ_SYNC_TOKEN_CONFIG_DIRTY = 401,
	/* Trigger thread notify config thread */
	CMDQ_SYNC_TOKEN_STREAM_EOF = 402,
	/* Block Trigger thread until the ESD check finishes. */
	CMDQ_SYNC_TOKEN_ESD_EOF = 403,
	/* check CABC setup finish */
	CMDQ_SYNC_TOKEN_CABC_EOF = 404,
	/* Block Trigger thread until the path freeze finishes */
	CMDQ_SYNC_TOKEN_FREEZE_EOF = 405,
	/* Pass-2 notifies VENC frame is ready to be encoded */
	CMDQ_SYNC_TOKEN_VENC_INPUT_READY = 406,
	/* VENC notifies Pass-2 encode done so next frame may start */
	CMDQ_SYNC_TOKEN_VENC_EOF = 407,

	/* Notify normal CMDQ there are some secure task done */
	CMDQ_SYNC_SECURE_THR_EOF = 408,
	/* Lock WSM resource */
	CMDQ_SYNC_SECURE_WSM_LOCK = 409,

	/* SW Sync Tokens (User-defined) */
	CMDQ_SYNC_TOKEN_USER_0 = 410,
	CMDQ_SYNC_TOKEN_USER_1 = 411,
	CMDQ_SYNC_TOKEN_POLL_MONITOR = 412,

	/* SW Sync Tokens (Pre-defined) */
	/* Config thread notify trigger thread for external display */
	CMDQ_SYNC_TOKEN_EXT_CONFIG_DIRTY = 415,
	/* Trigger thread notify config thread */
	CMDQ_SYNC_TOKEN_EXT_STREAM_EOF = 416,
	/* Check CABC setup finish */
	CMDQ_SYNC_TOKEN_EXT_CABC_EOF = 417,

	/* Secure video path notify SW token */
	CMDQ_SYNC_DISP_OVL0_2NONSEC_END = 420,
	CMDQ_SYNC_DISP_OVL1_2NONSEC_END = 421,
	CMDQ_SYNC_DISP_2LOVL0_2NONSEC_END = 422,
	CMDQ_SYNC_DISP_2LOVL1_2NONSEC_END = 423,
	CMDQ_SYNC_DISP_RDMA0_2NONSEC_END = 424,
	CMDQ_SYNC_DISP_RDMA1_2NONSEC_END = 425,
	CMDQ_SYNC_DISP_WDMA0_2NONSEC_END = 426,
	CMDQ_SYNC_DISP_WDMA1_2NONSEC_END = 427,
	CMDQ_SYNC_DISP_EXT_STREAM_EOF = 428,

	/**
	 * Event for CMDQ to block executing command when append command
	 * Plz sync CMDQ_SYNC_TOKEN_APPEND_THR(id) in cmdq_core source file.
	 */
	CMDQ_SYNC_TOKEN_APPEND_THR0 = 432,
	CMDQ_SYNC_TOKEN_APPEND_THR1 = 433,
	CMDQ_SYNC_TOKEN_APPEND_THR2 = 434,
	CMDQ_SYNC_TOKEN_APPEND_THR3 = 435,
	CMDQ_SYNC_TOKEN_APPEND_THR4 = 436,
	CMDQ_SYNC_TOKEN_APPEND_THR5 = 437,
	CMDQ_SYNC_TOKEN_APPEND_THR6 = 438,
	CMDQ_SYNC_TOKEN_APPEND_THR7 = 439,
	CMDQ_SYNC_TOKEN_APPEND_THR8 = 440,
	CMDQ_SYNC_TOKEN_APPEND_THR9 = 441,
	CMDQ_SYNC_TOKEN_APPEND_THR10 = 442,
	CMDQ_SYNC_TOKEN_APPEND_THR11 = 443,
	CMDQ_SYNC_TOKEN_APPEND_THR12 = 444,
	CMDQ_SYNC_TOKEN_APPEND_THR13 = 445,
	CMDQ_SYNC_TOKEN_APPEND_THR14 = 446,
	CMDQ_SYNC_TOKEN_APPEND_THR15 = 447,
	CMDQ_SYNC_TOKEN_APPEND_THR16 = 448,
	CMDQ_SYNC_TOKEN_APPEND_THR17 = 449,
	CMDQ_SYNC_TOKEN_APPEND_THR18 = 450,
	CMDQ_SYNC_TOKEN_APPEND_THR19 = 451,
	CMDQ_SYNC_TOKEN_APPEND_THR20 = 452,
	CMDQ_SYNC_TOKEN_APPEND_THR21 = 453,
	CMDQ_SYNC_TOKEN_APPEND_THR22 = 454,
	CMDQ_SYNC_TOKEN_APPEND_THR23 = 455,
	CMDQ_SYNC_TOKEN_APPEND_THR24 = 456,
	CMDQ_SYNC_TOKEN_APPEND_THR25 = 457,
	CMDQ_SYNC_TOKEN_APPEND_THR26 = 458,
	CMDQ_SYNC_TOKEN_APPEND_THR27 = 459,
	CMDQ_SYNC_TOKEN_APPEND_THR28 = 460,
	CMDQ_SYNC_TOKEN_APPEND_THR29 = 461,
	CMDQ_SYNC_TOKEN_APPEND_THR30 = 462,
	CMDQ_SYNC_TOKEN_APPEND_THR31 = 463,

	/* GPR access tokens (for HW register backup)
	 * There are 15 32-bit GPR, 3 GPR form a set
	 * (64-bit for address, 32-bit for value)
	 */
	CMDQ_SYNC_TOKEN_GPR_SET_0 = 470,
	CMDQ_SYNC_TOKEN_GPR_SET_1 = 471,
	CMDQ_SYNC_TOKEN_GPR_SET_2 = 472,
	CMDQ_SYNC_TOKEN_GPR_SET_3 = 473,
	CMDQ_SYNC_TOKEN_GPR_SET_4 = 474,

	/* Resource lock event to control resource in GCE thread */
	CMDQ_SYNC_RESOURCE_WROT0 = 480,
	CMDQ_SYNC_RESOURCE_WROT1 = 481,

	/**
	 * Event for CMDQ delay implement
	 * Plz sync CMDQ_SYNC_TOKEN_DELAY_THR(id) in cmdq_core source file.
	 */
	CMDQ_SYNC_TOKEN_TIMER = 485,
	CMDQ_SYNC_TOKEN_DELAY_SET0 = 486,
	CMDQ_SYNC_TOKEN_DELAY_SET1 = 487,
	CMDQ_SYNC_TOKEN_DELAY_SET2 = 488,

	/* GCE HW TPR Event*/
	CMDQ_EVENT_TIMER_00 = 962,
	CMDQ_EVENT_TIMER_01 = 963,
	CMDQ_EVENT_TIMER_02 = 964,
	CMDQ_EVENT_TIMER_03 = 965,
	CMDQ_EVENT_TIMER_04 = 966,
	/* 5: 1us */
	CMDQ_EVENT_TIMER_05 = 967,
	CMDQ_EVENT_TIMER_06 = 968,
	CMDQ_EVENT_TIMER_07 = 969,
	/* 8: 10us */
	CMDQ_EVENT_TIMER_08 = 970,
	CMDQ_EVENT_TIMER_09 = 971,
	CMDQ_EVENT_TIMER_10 = 972,
	/* 11: 100us */
	CMDQ_EVENT_TIMER_11 = 973,
	CMDQ_EVENT_TIMER_12 = 974,
	CMDQ_EVENT_TIMER_13 = 975,
	CMDQ_EVENT_TIMER_14 = 976,
	/* 15: 1ms */
	CMDQ_EVENT_TIMER_15 = 977,
	CMDQ_EVENT_TIMER_16 = 978,
	CMDQ_EVENT_TIMER_17 = 979,
	/* 18: 10ms */
	CMDQ_EVENT_TIMER_18 = 980,
	CMDQ_EVENT_TIMER_19 = 981,
	CMDQ_EVENT_TIMER_20 = 982,
	/* 21: 100ms */
	CMDQ_EVENT_TIMER_21 = 983,
	CMDQ_EVENT_TIMER_22 = 984,
	CMDQ_EVENT_TIMER_23 = 985,
	CMDQ_EVENT_TIMER_24 = 986,
	CMDQ_EVENT_TIMER_25 = 987,
	CMDQ_EVENT_TIMER_26 = 988,
	CMDQ_EVENT_TIMER_27 = 989,
	CMDQ_EVENT_TIMER_28 = 990,
	CMDQ_EVENT_TIMER_29 = 991,
	CMDQ_EVENT_TIMER_30 = 992,
	CMDQ_EVENT_TIMER_31 = 993,

	/* event id is 9 bit */
	CMDQ_SYNC_TOKEN_MAX = 0x3FF,
	CMDQ_MAX_EVENT = 0x3FF,
	CMDQ_SYNC_TOKEN_INVALID = -1,
};

/* General Purpose Register */
enum cmdq_gpr_reg {
	/* Value Reg, we use 32-bit */
	/* Address Reg, we use 64-bit */
	/* Note that R0-R15 and P0-P7 actullay share same memory */
	/* and R1 cannot be used. */

	CMDQ_DATA_REG_JPEG = 0x00,	/* R0 */
	CMDQ_DATA_REG_JPEG_DST = 0x11,	/* P1 */

	CMDQ_DATA_REG_PQ_COLOR = 0x04,	/* R4 */
	CMDQ_DATA_REG_PQ_COLOR_DST = 0x13,	/* P3 */

	CMDQ_DATA_REG_2D_SHARPNESS_0 = 0x05,	/* R5 */
	CMDQ_DATA_REG_2D_SHARPNESS_0_DST = 0x14,	/* P4 */

	CMDQ_DATA_REG_2D_SHARPNESS_1 = 0x0a,	/* R10 */
	CMDQ_DATA_REG_2D_SHARPNESS_1_DST = 0x16,	/* P6 */

	CMDQ_DATA_REG_DEBUG = 0x0b,	/* R11 */
	CMDQ_DATA_REG_DEBUG_DST = 0x17,	/* P7 */

	/* sentinel value for invalid register ID */
	CMDQ_DATA_REG_INVALID = -1,
};

struct cmdq_pkt;

struct cmdq_base {
	int	subsys;
	u32	base;
};

struct cmdq_client {
	struct mbox_client client;
	struct mbox_chan *chan;
};

/**
 * cmdq_register_device() - register device which needs CMDQ
 * @dev:	device for CMDQ to access its registers
 *
 * Return: cmdq_base pointer or NULL for failed
 */
struct cmdq_base *cmdq_register_device(struct device *dev);

/**
 * cmdq_mbox_create() - create CMDQ mailbox client and channel
 * @dev:	device of CMDQ mailbox client
 * @index:	index of CMDQ mailbox channel
 *
 * Return: CMDQ mailbox client pointer
 */
struct cmdq_client *cmdq_mbox_create(struct device *dev, int index);

/**
 * cmdq_mbox_destroy() - destroy CMDQ mailbox client and channel
 * @client:	the CMDQ mailbox client
 */
void cmdq_mbox_destroy(struct cmdq_client *client);

/**
 * cmdq_pkt_create() - create a CMDQ packet
 * @pkt_ptr:	CMDQ packet pointer to retrieve cmdq_pkt
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_create(struct cmdq_pkt **pkt_ptr);

/**
 * cmdq_pkt_destroy() - destroy the CMDQ packet
 * @pkt:	the CMDQ packet
 */
void cmdq_pkt_destroy(struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_realloc_cmd_buffer() - reallocate command buffer for CMDQ packet
 * @pkt:	the CMDQ packet
 * @size:	the request size
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_realloc_cmd_buffer(struct cmdq_pkt *pkt, size_t size);

/**
 * cmdq_pkt_read() - append read command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 * @writeAddress:
 * @valueRegId:
 * @destRegId:
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_read(struct cmdq_pkt *pkt,
			struct cmdq_base *base, u32 offset, u32 writeAddress,
			enum cmdq_gpr_reg valueRegId,
			enum cmdq_gpr_reg destRegId);

/**
 * cmdq_pkt_write() - append write command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write(struct cmdq_pkt *pkt, u32 value,
		   struct cmdq_base *base, u32 offset);

/**
 * cmdq_pkt_write_mask() - append write command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_write_mask(struct cmdq_pkt *pkt, u32 value,
			struct cmdq_base *base, u32 offset, u32 mask);

/**
 * cmdq_pkt_poll() - append polling command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll(struct cmdq_pkt *pkt, u32 value,
			 struct cmdq_base *base, u32 offset);

/**
 * cmdq_pkt_poll_t() - append polling command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @base:	the CMDQ base
 * @offset:	register offset from module base
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_poll_mask(struct cmdq_pkt *pkt, u32 value,
			struct cmdq_base *base, uint32_t offset, uint32_t mask);

/**
 * cmdq_pkt_wfe() - append wait for event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event type to "wait and CLEAR"
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_wfe(struct cmdq_pkt *pkt, enum cmdq_event event);

/**
 * cmdq_pkt_clear_event() - append clear event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event to be cleared
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_clear_event(struct cmdq_pkt *pkt, enum cmdq_event event);

/**
 * cmdq_pkt_flush() - trigger CMDQ to execute the CMDQ packet
 * @client:	the CMDQ mailbox client
 * @pkt:	the CMDQ packet
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to execute the CMDQ packet. Note that this is a
 * synchronous flush function. When the function returned, the recorded
 * commands have been done.
 */
int cmdq_pkt_flush(struct cmdq_client *client, struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_flush_async() - trigger CMDQ to asynchronously execute the CMDQ
 *                          packet and call back at the end of done packet
 * @client:	the CMDQ mailbox client
 * @pkt:	the CMDQ packet
 * @cb:		called at the end of done packet
 * @data:	this data will pass back to cb
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to asynchronously execute the CMDQ packet and call back
 * at the end of done packet. Note that this is an ASYNC function. When the
 * function returned, it may or may not be finished.
 */
int cmdq_pkt_flush_async(struct cmdq_client *client, struct cmdq_pkt *pkt,
			 cmdq_async_flush_cb cb, void *data);

#ifdef CMDQ_MEMORY_JUMP
u64 *cmdq_pkt_get_va_by_offset(struct cmdq_pkt *pkt, size_t offset);
dma_addr_t cmdq_pkt_get_pa_by_offset(struct cmdq_pkt *pkt, u32 offset);
#endif

/* debug */
#if 0
extern const u32 cmdq_event_value_8173[CMDQ_MAX_EVENT];
extern const u32 cmdq_event_value_2712[CMDQ_MAX_EVENT];
#endif
extern const u32 cmdq_event_value_common[CMDQ_MAX_EVENT];
extern const u32 *cmdq_event_value;

u32 cmdq_subsys_id_to_base(int id);


struct cmdq_thread_task_info {
	dma_addr_t		pa_base;
	struct cmdq_pkt		*pkt;
	struct list_head	list_entry;
};

struct cmdq_timeout_info {
	u32 irq;
	u32 irq_en;
	dma_addr_t curr_pc;
	u32 *curr_pc_va;
	dma_addr_t end_addr;
	u32 task_num;
	struct cmdq_thread_task_info *timeout_task;
	struct list_head task_list;
};
#endif	/* __MTK_CMDQ_H__ */
