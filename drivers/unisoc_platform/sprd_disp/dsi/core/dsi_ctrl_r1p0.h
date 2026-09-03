/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _DSI_CTRL_R1P0_H_
#define _DSI_CTRL_R1P0_H_

#include <asm/types.h>

struct dsi_reg {
	union _0x00 {
		u32 val;
		struct _DSI_VERSION {
		u32 dsi_version: 16;
		u32 reserved: 16;
		} bits;
	} DSI_VERSION;

	union _0x04 {
		u32 val;
		struct _SOFT_RESET {
		/*
		 * This bit configures the core either to work normal or to
		 * reset. It's default value is 0. After the core configur-
		 * ation, to enable the mipi_dsi_host, set this register to 1.
		 * 1: power up     0: reset core
		 */
		u32 dsi_soft_reset: 1;

		u32 reserved: 31;
		} bits;
	} SOFT_RESET;

	union _0x08 {
		u32 val;
		struct _PROTOCOL_INT_STS {
		/* ErrEsc escape entry error from Lane 0 */
		u32 dphy_errors_0: 1;

		/* ErrSyncEsc low-power data transmission synchronization
		 * error from Lane 0
		 */
		u32 dphy_errors_1: 1;

		/* ErrControl error from Lane 0 */
		u32 dphy_errors_2: 1;

		/* ErrContentionLP0 LP0 contention error from Lane 0 */
		u32 dphy_errors_3: 1;

		/* ErrContentionLP1 LP1 contention error from Lane 0 */
		u32 dphy_errors_4: 1;

		/* debug mode protocol errors */
		u32 protocol_debug_err: 11;

		/* SoT error from the Acknowledge error report */
		u32 ack_with_err_0: 1;

		/* SoT Sync error from the Acknowledge error report */
		u32 ack_with_err_1: 1;

		/* EoT Sync error from the Acknowledge error report */
		u32 ack_with_err_2: 1;

		/* Escape Mode Entry Command error from the Acknowledge
		 * error report
		 */
		u32 ack_with_err_3: 1;

		/* LP Transmit Sync error from the Acknowledge error report */
		u32 ack_with_err_4: 1;

		/* Peripheral Timeout error from the Acknowledge error report */
		u32 ack_with_err_5: 1;

		/* False Control error from the Acknowledge error report */
		u32 ack_with_err_6: 1;

		/* reserved (specific to device) from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_7: 1;

		/* ECC error, single-bit (detected and corrected) from the
		 * Acknowledge error report
		 */
		u32 ack_with_err_8: 1;

		/* ECC error, multi-bit (detected, not corrected) from the
		 * Acknowledge error report
		 */
		u32 ack_with_err_9: 1;

		/* checksum error (long packet only) from the Acknowledge
		 * error report
		 */
		u32 ack_with_err_10: 1;

		/* not recognized DSI data type from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_11: 1;

		/* DSI VC ID Invalid from the Acknowledge error report */
		u32 ack_with_err_12: 1;

		/* invalid transmission length from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_13: 1;

		/* reserved (specific to device) from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_14: 1;

		/* DSI protocol violation from the Acknowledge error report */
		u32 ack_with_err_15: 1;

		} bits;
	} PROTOCOL_INT_STS;

	union _0x0C {
		u32 val;
		struct _MASK_PROTOCOL_INT {
		u32 mask_dphy_errors_0: 1;
		u32 mask_dphy_errors_1: 1;
		u32 mask_dphy_errors_2: 1;
		u32 mask_dphy_errors_3: 1;
		u32 mask_dphy_errors_4: 1;
		u32 mask_protocol_debug_err: 11;
		u32 mask_ack_with_err_0: 1;
		u32 mask_ack_with_err_1: 1;
		u32 mask_ack_with_err_2: 1;
		u32 mask_ack_with_err_3: 1;
		u32 mask_ack_with_err_4: 1;
		u32 mask_ack_with_err_5: 1;
		u32 mask_ack_with_err_6: 1;
		u32 mask_ack_with_err_7: 1;
		u32 mask_ack_with_err_8: 1;
		u32 mask_ack_with_err_9: 1;
		u32 mask_ack_with_err_10: 1;
		u32 mask_ack_with_err_11: 1;
		u32 mask_ack_with_err_12: 1;
		u32 mask_ack_with_err_13: 1;
		u32 mask_ack_with_err_14: 1;
		u32 mask_ack_with_err_15: 1;
		} bits;
	} MASK_PROTOCOL_INT;

	union _0x10 {
		u32 val;
		struct _INTERNAL_INT_STS {
		/* This bit indicates that the packet size error is detected
		 * during the packet reception.
		 */
		u32 receive_pkt_size_err: 1;

		/* This bit indicates that the EoTp packet is not received at
		 * the end of the incoming peripheral transmission
		 */
		u32 eotp_not_receive_err: 1;

		/* This bit indicates that the system tried to write a command
		 * through the Generic interface and the FIFO is full. There-
		 * fore, the command is not written.
		 */
		u32 gen_cmd_cmd_fifo_wr_err: 1;

		/* This bit indicates that during a DCS read data, the payload
		 * FIFO becomes	empty and the data sent to the interface is
		 * corrupted.
		 */
		u32 gen_cmd_rdata_fifo_rd_err: 1;

		/* This bit indicates that during a generic interface packet
		 * read back, the payload FIFO becomes full and the received
		 * data is corrupted.
		 */
		u32 gen_cmd_rdata_fifo_wr_err: 1;

		/* This bit indicates that the system tried to write a payload
		 * data through the Generic interface and the FIFO is full.
		 * Therefore, the payload is not written.
		 */
		u32 gen_cmd_wdata_fifo_wr_err: 1;

		/* This bit indicates that during a Generic interface packet
		 * build, the payload FIFO becomes empty and corrupt data is
		 * sent.
		 */
		u32 gen_cmd_wdata_fifo_rd_err: 1;

		/* This bit indicates that during a DPI pixel line storage,
		 * the payload FIFO becomes full and the data stored is
		 * corrupted.
		 */
		u32 dpi_pix_fifo_wr_err: 1;

		/* internal debug error	*/
		u32 internal_debug_err: 19;

		/* This bit indicates that the ECC single error is detected
		 * and corrected in a received packet.
		 */
		u32 ecc_single_err: 1;

		/* This bit indicates that the ECC multiple error is detected
		 * in a received packet.
		 */
		u32 ecc_multi_err: 1;

		/* This bit indicates that the CRC error is detected in the
		 * received packet payload.
		 */
		u32 crc_err: 1;

		/* This bit indicates that the high-speed transmission timeout
		 * counter reached the end and contention is detected.
		 */
		u32 hs_tx_timeout: 1;

		/* This bit indicates that the low-power reception timeout
		 * counter reached the end and contention is detected.
		 */
		u32 lp_rx_timeout: 1;

		} bits;
	} INTERNAL_INT_STS;

	union _0x14 {
		u32 val;
		struct _MASK_INTERNAL_INT {
		u32 mask_receive_pkt_size_err: 1;
		u32 mask_eopt_not_receive_err: 1;
		u32 mask_gen_cmd_cmd_fifo_wr_err: 1;
		u32 mask_gen_cmd_rdata_fifo_rd_err: 1;
		u32 mask_gen_cmd_rdata_fifo_wr_err: 1;
		u32 mask_gen_cmd_wdata_fifo_wr_err: 1;
		u32 mask_gen_cmd_wdata_fifo_rd_err: 1;
		u32 mask_dpi_pix_fifo_wr_err: 1;
		u32 mask_internal_debug_err: 19;
		u32 mask_ecc_single_err: 1;
		u32 mask_ecc_multi_err: 1;
		u32 mask_crc_err: 1;
		u32 mask_hs_tx_timeout: 1;
		u32 mask_lp_rx_timeout: 1;
		} bits;
	} MASK_INTERNAL_INT;

	union _0x18 {
		u32 val;
		struct _DSI_MODE_CFG {
		/* This bit configures the operation mode
		 * 0: Video mode ;   1: Command mode
		 */
		u32 cmd_video_mode: 1;

		u32 reserved: 31;

		} bits;
	} DSI_MODE_CFG;

	union _0x1C {
		u32 val;
		struct _VIRTUAL_CHANNEL_ID {
		/* This field indicates the Generic interface read-back
		 * virtual channel identification
		 */
		u32 gen_rx_vcid: 2;

		/* This field configures the DPI virtual channel id that
		 * is indexed to the VIDEO mode packets
		 */
		u32 video_pkt_vcid: 2;

		u32 reserved: 28;

		} bits;
	} VIRTUAL_CHANNEL_ID;

	union _0x20 {
		u32 val;
		struct _DPI_VIDEO_FORMAT {
		/*
		 * This field configures the DPI color coding as follows:
		 * 0000: 16-bit configuration 1
		 * 0001: 16-bit configuration 2
		 * 0010: 16-bit configuration 3
		 * 0011: 18-bit configuration 1
		 * 0100: 18-bit configuration 2
		 * 0101: 24-bit
		 * 0110: 20-bit YCbCr 4:2:2 loosely packed
		 * 0111: 24-bit YCbCr 4:2:2
		 * 1000: 16-bit YCbCr 4:2:2
		 * 1001: 30-bit
		 * 1010: 36-bit
		 * 1011: 12-bit YCbCr 4:2:0
		 * 1100: Compression Display Stream
		 * 1101-1111: 12-bit YCbCr 4:2:0
		 */
		u32 dpi_video_mode_format: 6;

		/* When set to 1, this bit activates loosely packed
		 * variant to 18-bit configurations
		 */
		u32 loosely18_en: 1;

		u32 reserved: 25;

		} bits;
	} DPI_VIDEO_FORMAT;

	union _0x24 {
		u32 val;
		struct _VIDEO_PKT_CONFIG {
		/*
		 * This field configures the number of pixels in a single
		 * video packet. For 18-bit not loosely packed data types,
		 * this number must be a multiple of 4. For YCbCr data
		 * types, it must be a multiple of 2, as described in the
		 * DSI specification.
		 */
		u32 video_pkt_size: 16;

		/*
		 * This register configures the number of chunks to be
		 * transmitted during a Line period (a chunk consists of
		 * a video packet and a null packet). If set to 0 or 1,
		 * the video line is transmitted in a single packet. If
		 * set to 1, the packet is part of a chunk, so a null packet
		 * follows it if vid_null_size > 0. Otherwise, multiple chunks
		 * are used to transmit each video line.
		 */
		u32 video_line_chunk_num: 16;

		} bits;
	} VIDEO_PKT_CONFIG;

	union _0x28 {
		u32 val;
		struct _VIDEO_LINE_HBLK_TIME {
		/* This field configures the Horizontal Back Porch period
		 * in lane byte clock cycles
		 */
		u32 video_line_hbp_time: 16;

		/* This field configures the Horizontal Synchronism Active
		 * period in lane byte clock cycles
		 */
		u32 video_line_hsa_time: 16;

		} bits;
	} VIDEO_LINE_HBLK_TIME;

	union _0x2C {
		u32 val;
		struct _VIDEO_LINE_TIME {
		/* This field configures the size of the total line time
		 * (HSA+HBP+HACT+HFP) counted in lane byte clock cycles
		 */
		u32 video_line_time: 16;

		u32 reserved: 16;

		} bits;
	} VIDEO_LINE_TIME;

	union _0x30 {
		u32 val;
		struct _VIDEO_VBLK_LINES {
		/* This field configures the Vertical Front Porch period
		 * measured in number of horizontal lines
		 */
		u32 vfp_lines: 10;

		/* This field configures the Vertical Back Porch period
		 * measured in number of horizontal lines
		 */
		u32 vbp_lines: 10;

		/* This field configures the Vertical Synchronism Active
		 * period measured in number of horizontal lines
		 */
		u32 vsa_lines: 10;

		u32 reserved: 2;

		} bits;
	} VIDEO_VBLK_LINES;

	union _0x34 {
		u32 val;
		struct _VIDEO_VACTIVE_LINES {
		/* This field configures the Vertical Active period measured
		 * in number of horizontal lines
		 */
		u32 vactive_lines: 14;

		u32 reserved: 18;

		} bits;
	} VIDEO_VACTIVE_LINES;

	union _0x38 {
		u32 val;
		struct _VID_MODE_CFG {
		/*
		 * This field indicates the video mode transmission type as
		 * follows:
		 * 00: Non-burst with sync pulses
		 * 01: Non-burst with sync events
		 * 10 and 11: Burst mode
		 */
		u32 vid_mode_type: 2;

		u32 reserved_0: 6;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VSA period when timing allows.
		 */
		u32 lp_vsa_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VBP period when timing allows.
		 */
		u32 lp_vbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VFP period when timing allows.
		 */
		u32 lp_vfp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VACT period when timing allows.
		 */
		u32 lp_vact_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HBP period when timing allows.
		 */
		u32 lp_hbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HFP period when timing allows.
		 */
		u32 lp_hfp_en: 1;

		/* When set to 1, this bit enables the request for an ack-
		 * nowledge response at the end of a frame.
		 */
		u32 frame_bta_ack_en: 1;

		/* When set to 1, this bit enables the command transmission
		 * only in low-power mode.
		 */
		u32 lp_cmd_en: 1;

		u32 reserved_1: 16;

		} bits;
	} VID_MODE_CFG;

	union _0x3C {
		u32 val;
		struct _SDF_MODE_CONFIG {
		/*
		 * This field defines the 3D mode on/off & display orientation:
		 * 00: 3D mode off (2D mode on)
		 * 01: 3D mode on, portrait orientation
		 * 10: 3D mode on, landscape orientation
		 * 11: Reserved
		 */
		u32 rf_3d_mode: 2;

		/*
		 * This field defines the 3D image format:
		 * 00: Line (alternating lines of left and right data)
		 * 01: Frame (alternating frames of left and right data)
		 * 10: Pixel (alternating pixels of left and right data)
		 * 11: Reserved
		 */
		u32 rf_3d_format: 2;

		/*
		 * This field defines whether there is a second VSYNC pulse
		 * between Left and Right Images, when 3D Image Format is
		 * Frame-based:
		 * 0: No sync pulses between left and right data
		 * 1: Sync pulse (HSYNC, VSYNC, blanking) between left and
		 *    right data
		 */
		u32 second_vsync_en: 1;

		/*
		 * This bit defines the left or right order:
		 * 0: Left eye data is sent first, and then the right eye data
		 *    is sent.
		 * 1: Right eye data is sent first, and then the left eye data
		 *    is sent.
		 */
		u32 left_right_order: 1;

		u32 reserved_0: 2;

		/*
		 * When set, causes the next VSS packet to include 3D control
		 * payload in every VSS packet.
		 */
		u32 rf_3d_payload_en: 1;

		u32 reserved_1: 23;

		} bits;
	} SDF_MODE_CONFIG;

	union _0x40 {
		u32 val;
		struct _TIMEOUT_CNT_CLK_CONFIG {
		/*
		 * This field indicates the division factor for the Time Out
		 * clock used as the timing unit in the configuration of HS to
		 * LP and LP to HS transition error.
		 */
		u32 timeout_cnt_clk_config: 16;

		u32 reserved: 16;

		} bits;
	} TIMEOUT_CNT_CLK_CONFIG;

	union _0x44 {
		u32 val;
		struct _HTX_TO_CONFIG {
		/*
		 * This field configures the timeout counter that triggers
		 * a high speed transmission timeout contention detection
		 * (measured in TO_CLK_DIVISION cycles).
		 *
		 * If using the non-burst mode and there is no sufficient
		 * time to switch from HS to LP and back in the period which
		 * is from one line data finishing to the next line sync
		 * start, the DSI link returns the LP state once per frame,
		 * then you should configure the TO_CLK_DIVISION and
		 * hstx_to_cnt to be in accordance with:
		 * hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the
		 * time of one FRAME data transmission * (1 + 10%)
		 *
		 * In burst mode, RGB pixel packets are time-compressed,
		 * leaving more time during a scan line. Therefore, if in
		 * burst mode and there is sufficient time to switch from HS
		 * to LP and back in the period of time from one line data
		 * finishing to the next line sync start, the DSI link can
		 * return LP mode and back in this time interval to save power.
		 * For this, configure the TO_CLK_DIVISION and hstx_to_cnt
		 * to be in accordance with:
		 * hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the
		 * time of one LINE data transmission * (1 + 10%)
		 */
		u32 htx_to_cnt_limit: 32;
		} bits;
	} HTX_TO_CONFIG;

	union _0x48 {
		u32 val;
		struct _LRX_H_TO_CONFIG {
		/*
		 * This field configures the timeout counter that triggers
		 * a low-power reception timeout contention detection (measured
		 * in TO_CLK_DIVISION cycles).
		 */
		u32 lrx_h_to_cnt_limit: 32;
		} bits;
	} LRX_H_TO_CONFIG;

	union _0x4C {
		u32 val;
		struct _RD_PRESP_TO_CONFIG {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a low-power read oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 lprd_presp_to_cnt_limit: 16;

		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a high-speed read oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 hsrd_presp_to_cnt_limit: 16;

		} bits;
	} RD_PRESP_TO_CONFIG;

	union _0x50 {
		u32 val;
		struct _HSWR_PRESP_TO_CONFIG {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link inactive after sending a high-speed write
		 * operation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 hswr_presp_to_cnt_limit: 16;

		u32 reserved_0: 8;

		/*
		 * When set to 1, this bit ensures that the peripheral response
		 * timeout caused by hs_wr_to_cnt is used only once per eDPI
		 * frame, when both the following conditions are met:
		 * dpivsync_edpiwms has risen and fallen.
		 * Packets originated from eDPI have been transmitted and its
		 * FIFO is empty again In this scenario no non-eDPI requests
		 * are sent to the D-PHY, even if there is traffic from generic
		 * or DBI ready to be sent, making it return to stop state.
		 * When it does so, PRESP_TO counter is activated and only when
		 * it finishes does the controller send any other traffic that
		 * is ready.
		 */
		u32 hswr_presp_to_mode: 1;

		u32 reserved_1: 7;

		} bits;
	} HSWR_PRESP_TO_CONFIG;

	union _0x54 {
		u32 val;
		struct _LPWR_PRESP_TO_CONFIG {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a low-power write oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 lpwr_presp_to_cnt_limit: 16;

		u32 reserved: 16;

		} bits;
	} LPWR_PRESP_TO_CONFIG;

	union _0x58 {
		u32 val;
		struct _BTA_PRESP_TO_CONFIG {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after completing a Bus Turn-Around.
		 * This period is measured in cycles of lanebyteclk. The
		 * counting starts when the D-PHY enters the Stop state and
		 * causes no interrupts.
		 */
		u32 bta_presp_to_cnt_limit: 16;

		u32 reserved: 16;

		} bits;
	} BTA_PRESP_TO_CONFIG;

	union _0x5C {
		u32 val;
		struct _TX_ESC_CLK_CONFIG {
		/*
		 * This field indicates the division factor for the TX Escape
		 * clock source (lanebyteclk). The values 0 and 1 stop the
		 * TX_ESC clock generation.
		 */
		u32 tx_esc_clk_config: 16;

		u32 reserved: 16;

		} bits;
	} TX_ESC_CLK_CONFIG;

	union _0x60 {
		u32 val;
		struct _VACT_CMD_TRANS_LIMIT {
		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VACT
		 * region.
		 */
		u32 vact_cmd_trans_limit: 8;

		u32 reserved: 24;

		} bits;
	} VACT_CMD_TRANS_LIMIT;

	union _0x64 {
		u32 val;
		struct _VBLK_CMD_TRANS_LIMIT {
		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VSA, VBP,
		 * and VFP regions.
		 */
		u32 vblk_cmd_trans_limit: 8;

		u32 reserved: 24;

		} bits;
	} VBLK_CMD_TRANS_LIMIT;

	union _0x68 {
		u32 val;
		struct _CMD_MODE_CFG {
		/*
		 * When set to 1, this bit enables the tearing effect
		 * acknowledge request.
		 */
		u32 tear_fx_en: 1;

		/*
		 * When set to 1, this bit enables the acknowledge request
		 * after each packet transmission.
		 */
		u32 ack_rqst_en: 1;

		u32 reserved_0: 3;

		u32 pps_tx: 1;
		u32 exq_tx: 1;
		u32 cmc_tx: 1;

		/*
		 * This bit configures the Generic short write packet with
		 * zero parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sw_0p_tx: 1;

		/*
		 * This bit configures the Generic short write packet with
		 * one parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sw_1p_tx: 1;

		/*
		 * This bit configures the Generic short write packet with
		 * two parameters command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sw_2p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * zero parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sr_0p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * one parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sr_1p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * two parameters command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sr_2p_tx: 1;

		/*
		 * This bit configures the Generic long write packet command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_lw_tx: 1;

		u32 reserved_1: 1;

		/*
		 * This bit configures the DCS short write packet with zero
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_sw_0p_tx: 1;

		/*
		 * This bit configures the DCS short write packet with one
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_sw_1p_tx: 1;

		/*
		 * This bit configures the DCS short read packet with zero
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_sr_0p_tx: 1;

		/*
		 * This bit configures the DCS long write packet command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_lw_tx: 1;

		u32 reserved_2: 4;

		/*
		 * This bit configures the maximum read packet size command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 max_rd_pkt_size: 1;

		u32 reserved_3: 7;

		} bits;
	} CMD_MODE_CFG;

	union _0x6C {
		u32 val;
		struct _GEN_HDR {
		/*
		 * This field configures the packet data type of the header
		 * packet.
		 */
		u32 gen_dt: 6;

		/*
		 * This field configures the virtual channel id of the header
		 * packet.
		 */
		u32 gen_vc: 2;

		/*
		 * This field configures the least significant byte of the
		 * header packet's Word count for long packets or data 0 for
		 * short packets.
		 */
		u32 gen_wc_lsbyte: 8;

		/*
		 * This field configures the most significant byte of the
		 * header packet's word count for long packets or data 1 for
		 * short packets.
		 */
		u32 gen_wc_msbyte: 8;

		u32 reserved: 8;

		} bits;
	} GEN_HDR;

	union _0x70 {
		u32 val;
		struct _GEN_PLD_DATA {
		/* This field indicates byte 1 of the packet payload. */
		u32 gen_pld_b1: 8;

		/* This field indicates byte 2 of the packet payload. */
		u32 gen_pld_b2: 8;

		/* This field indicates byte 3 of the packet payload. */
		u32 gen_pld_b3: 8;

		/* This field indicates byte 4 of the packet payload. */
		u32 gen_pld_b4: 8;

		} bits;
	} GEN_PLD_DATA;

	union _0x74 {
		u32 val;
		struct _PHY_CLK_LANE_LP_CTRL {
		/* This bit controls the D-PHY PPI txrequestclkhs signal */
		u32 phy_clklane_tx_req_hs: 1;

		/* This bit enables the automatic mechanism to stop providing
		 * clock in the clock lane when time allows.
		 */
		u32 auto_clklane_ctrl_en: 1;

		u32 reserved: 30;
		} bits;
	} PHY_CLK_LANE_LP_CTRL;

	union _0x78 {
		u32 val;
		struct _PHY_INTERFACE_CTRL {
		/* When set to 0, this bit places the D-PHY macro in power-
		 * down state.
		 */
		u32 rf_phy_shutdown: 1;

		/* When set to 0, this bit places the digital section of the
		 * D-PHY in the reset state.
		 */
		u32 rf_phy_reset_n: 1;

		/* When set to 1, this bit enables the D-PHY Clock Lane
		 * module.
		 */
		u32 rf_phy_clk_en: 1;

		/* When the D-PHY is in ULPS, this bit enables the D-PHY PLL. */
		u32 rf_phy_force_pll: 1;

		/* ULPS mode Request on clock lane */
		u32 rf_phy_clk_txrequlps: 1;

		/* ULPS mode Exit on clock lane */
		u32 rf_phy_clk_txexitulps: 1;

		/* ULPS mode Request on all active data lanes */
		u32 rf_phy_data_txrequlps: 1;

		/* ULPS mode Exit on all active data lanes */
		u32 rf_phy_data_txexitulps: 1;

		u32 reserved: 24;
		} bits;
	} PHY_INTERFACE_CTRL;

	union _0x7C {
		u32 val;
		struct _PHY_TX_TRIGGERS {
		/* This field controls the trigger transmissions. */
		u32 phy_tx_triggers: 4;

		u32 reserved: 28;
		} bits;
	} PHY_TX_TRIGGERS;

	union _0x80 {
		u32 val;
		struct _DESKEW_START {
		u32 deskew_start: 1;
		u32 reserved: 31;
		} bits;
	} DESKEW_START;

	union _0x84 {
		u32 val;
		struct _DESKEW_MODE {
		u32 deskew_mode: 2;
		u32 reserved: 30;
		} bits;
	} DESKEW_MODE;

	union _0x88 {
		u32 val;
		struct _DESKEW_TIME {
		u32 deskew_time: 32;
		} bits;
	} DESKEW_TIME;

	union _0x8C {
		u32 val;
		struct _DESKEW_PERIOD {
		u32 deskew_period: 32;
		} bits;
	} DESKEW_PERIOD;

	union _0x90 {
		u32 val;
		struct _DESKEW_BUSY {
		u32 deskew_busy: 1;
		u32 reserved: 31;
		} bits;
	} DESKEW_BUSY;

	union _0x94 {
		u32 val;
		struct _DESKEW_LANE_MASK {
		u32 deskew_lane0_mask: 1;
		u32 deskew_lane1_mask: 1;
		u32 deskew_lane2_mask: 1;
		u32 deskew_lane3_mask: 1;
		u32 reserved: 28;
		} bits;
	} DESKEW_LANE_MASK;

	union _0x98 {
		u32 val;
		struct _CMD_MODE_STATUS {
		/*
		 * This bit is set when a read command is issued and cleared
		 * when the entire response is stored in the FIFO.
		 * Value after reset: 0x0
		 *
		 * NOTE:
		 * For mipi-dsi-r1p0 IP, this bit is set immediately when
		 *     the read cmd is set to the GEN_HDR register.
		 *
		 * For dsi-ctrl-r1p0 IP, this bit is set only after the read
		 *     cmd was actually sent out from the controller.
		 */
		u32 gen_cmd_rdcmd_ongoing: 1;

		/*
		 * This bit indicates the empty status of the generic read
		 * payload FIFO.
		 * Value after reset: 0x1
		 */
		u32 gen_cmd_rdata_fifo_empty: 1;

		/*
		 * This bit indicates the full status of the generic read
		 * payload FIFO.
		 * Value after reset: 0x0
		 */
		u32 gen_cmd_rdata_fifo_full: 1;

		/*
		 * This bit indicates the empty status of the generic write
		 * payload FIFO.
		 * Value after reset: 0x1
		 */
		u32 gen_cmd_wdata_fifo_empty: 1;

		/*
		 * This bit indicates the full status of the generic write
		 * payload FIFO.
		 * Value after reset: 0x0
		 */
		u32 gen_cmd_wdata_fifo_full: 1;

		/*
		 * This bit indicates the empty status of the generic
		 * command FIFO.
		 * Value after reset: 0x1
		 */
		u32 gen_cmd_cmd_fifo_empty: 1;

		/*
		 * This bit indicates the full status of the generic
		 * command FIFO.
		 * Value after reset: 0x0
		 */
		u32 gen_cmd_cmd_fifo_full: 1;

		/*
		 * This bit is set when the entire response of read is
		 * stored in the rx payload FIFO. And it will be cleared
		 * automaticlly after read this bit each time.
		 * Value after reset: 0x0
		 *
		 * NOTE: this bit is just supported for dsi-ctrl-r1p0 IP
		 */
		u32 gen_cmd_rdcmd_done: 1;

		u32 reserved : 24;

		} bits;
	} CMD_MODE_STATUS;

	union _0x9C {
		u32 val;
		struct _PHY_STATUS {
		/* the status of phydirection D-PHY signal */
		u32 phy_direction: 1;

		/* the status of phylock D-PHY signal */
		u32 phy_lock: 1;

		/* the status of rxulpsesc0lane D-PHY signal */
		u32 phy_rxulpsesc0lane: 1;

		/* the status of phystopstateclklane D-PHY signal */
		u32 phy_stopstateclklane: 1;

		/* the status of phystopstate0lane D-PHY signal */
		u32 phy_stopstate0lane: 1;

		/* the status of phystopstate1lane D-PHY signal */
		u32 phy_stopstate1lane: 1;

		/* the status of phystopstate2lane D-PHY signal */
		u32 phy_stopstate2lane: 1;

		/* the status of phystopstate3lane D-PHY signal */
		u32 phy_stopstate3lane: 1;

		/* the status of phyulpsactivenotclk D-PHY signal */
		u32 phy_ulpsactivenotclk: 1;

		/* the status of ulpsactivenot0lane D-PHY signal */
		u32 phy_ulpsactivenot0lane: 1;

		/* the status of ulpsactivenot1lane D-PHY signal */
		u32 phy_ulpsactivenot1lane: 1;

		/* the status of ulpsactivenot2lane D-PHY signal */
		u32 phy_ulpsactivenot2lane: 1;

		/* the status of ulpsactivenot3lane D-PHY signal */
		u32 phy_ulpsactivenot3lane: 1;

		u32 reserved: 19;

		} bits;
	} PHY_STATUS;

	union _0xA0 {
		u32 val;
		struct _PHY_MIN_STOP_TIME {
		/* This field configures the minimum wait period to request
		 * a high-speed transmission after the Stop state.
		 */
		u32 phy_min_stop_time: 8;

		u32 reserved: 24;
		} bits;
	} PHY_MIN_STOP_TIME;

	union _0xA4 {
		u32 val;
		struct _PHY_LANE_NUM_CONFIG {
		/*
		 * This field configures the number of active data lanes:
		 * 00: One data lane (lane 0)
		 * 01: Two data lanes (lanes 0 and 1)
		 * 10: Three data lanes (lanes 0, 1, and 2)
		 * 11: Four data lanes (lanes 0, 1, 2, and 3)
		 */
		u32 phy_lane_num: 2;

		u32 reserved: 30;

		} bits;
	} PHY_LANE_NUM_CONFIG;

	union _0xA8 {
		u32 val;
		struct _PHY_CLKLANE_TIME_CONFIG {
		/*
		 * This field configures the maximum time that the D-PHY
		 * clock lane takes to go from low-power to high-speed
		 * transmission measured in lane byte clock cycles.
		 */
		u32 phy_clklane_lp_to_hs_time: 16;

		/*
		 * This field configures the maximum time that the D-PHY
		 * clock lane takes to go from high-speed to low-power
		 * transmission measured in lane byte clock cycles.
		 */
		u32 phy_clklane_hs_to_lp_time: 16;

		} bits;
	} PHY_CLKLANE_TIME_CONFIG;

	union _0xAC {
		u32 val;
		struct _PHY_DATALANE_TIME_CONFIG {
		/*
		 * This field configures the maximum time that the D-PHY data
		 * lanes take to go from low-power to high-speed transmission
		 * measured in lane byte clock cycles.
		 */
		u32 phy_datalane_lp_to_hs_time: 16;

		/*
		 * This field configures the maximum time that the D-PHY data
		 * lanes take to go from high-speed to low-power transmission
		 * measured in lane byte clock cycles.
		 */
		u32 phy_datalane_hs_to_lp_time: 16;

		} bits;
	} PHY_DATALANE_TIME_CONFIG;

	union _0xB0 {
		u32 val;
		struct _MAX_READ_TIME {
		/*
		 * This field configures the maximum time required to perform
		 * a read command in lane byte clock cycles. This register can
		 * only be modified when no read command is in progress.
		 */
		u32 max_rd_time: 16;

		u32 reserved: 16;

		} bits;
	} MAX_READ_TIME;

	union _0xB4 {
		u32 val;
		struct _RX_PKT_CHECK_CONFIG {
		/* When set to 1, this bit enables the ECC reception, error
		 * correction, and reporting.
		 */
		u32 rx_pkt_ecc_en: 1;

		/* When set to 1, this bit enables the CRC reception and error
		 * reporting.
		 */
		u32 rx_pkt_crc_en: 1;

		u32 reserved: 30;

		} bits;
	} RX_PKT_CHECK_CONFIG;

	union _0xB8 {
		u32 val;
		struct _TA_EN {
		/* When set to 1, this bit enables the Bus Turn-Around (BTA)
		 * request.
		 */
		u32 ta_en: 1;

		u32 reserved: 31;

		} bits;
	} TA_EN;

	union _0xBC {
		u32 val;
		struct _EOTP_EN {
		/* When set to 1, this bit enables the EoTp transmission */
		u32 tx_eotp_en: 1;

		/* When set to 1, this bit enables the EoTp reception. */
		u32 rx_eotp_en: 1;

		u32 reserved: 30;

		} bits;
	} EOTP_EN;

	union _0xC0 {
		u32 val;
		struct _VIDEO_NULLPKT_SIZE {
		/*
		 * This register configures the number of bytes inside a null
		 * packet. Setting it to 0 disables the null packets.
		 */
		u32 video_nullpkt_size: 13;

		u32 reserved: 19;

		} bits;
	} VIDEO_NULLPKT_SIZE;

	union _0xC4 {
		u32 val;
		struct _DCS_WM_PKT_SIZE {
		/*
		 * This field configures the maximum allowed size for an eDPI
		 * write memory command, measured in pixels. Automatic parti-
		 * tioning of data obtained from eDPI is permanently enabled.
		 */
		u32 dcs_wm_pkt_size: 16;

		u32 reserved: 16;
		} bits;
	} DCS_WM_PKT_SIZE;

	union _0xC8 {
		u32 val;
		struct _PROTOCOL_INT_CLR {
		u32 clr_dphy_errors_0: 1;
		u32 clr_dphy_errors_1: 1;
		u32 clr_dphy_errors_2: 1;
		u32 clr_dphy_errors_3: 1;
		u32 clr_dphy_errors_4: 1;
		u32 clr_protocol_debug_err: 11;
		u32 clr_ack_with_err_0: 1;
		u32 clr_ack_with_err_1: 1;
		u32 clr_ack_with_err_2: 1;
		u32 clr_ack_with_err_3: 1;
		u32 clr_ack_with_err_4: 1;
		u32 clr_ack_with_err_5: 1;
		u32 clr_ack_with_err_6: 1;
		u32 clr_ack_with_err_7: 1;
		u32 clr_ack_with_err_8: 1;
		u32 clr_ack_with_err_9: 1;
		u32 clr_ack_with_err_10: 1;
		u32 clr_ack_with_err_11: 1;
		u32 clr_ack_with_err_12: 1;
		u32 clr_ack_with_err_13: 1;
		u32 clr_ack_with_err_14: 1;
		u32 clr_ack_with_err_15: 1;
		} bits;
	} PROTOCOL_INT_CLR;

	union _0xCC {
		u32 val;
		struct _INTERNAL_INT_CLR {
		u32 clr_receive_pkt_size_err: 1;
		u32 clr_eopt_not_receive_err: 1;
		u32 clr_gen_cmd_cmd_fifo_wr_err: 1;
		u32 clr_gen_cmd_rdata_fifo_rd_err: 1;
		u32 clr_gen_cmd_rdata_fifo_wr_err: 1;
		u32 clr_gen_cmd_wdata_fifo_wr_err: 1;
		u32 clr_gen_cmd_wdata_fifo_rd_err: 1;
		u32 clr_dpi_pix_fifo_wr_err: 1;
		u32 clr_internal_debug_err: 19;
		u32 clr_ecc_single_err: 1;
		u32 clr_ecc_multi_err: 1;
		u32 clr_crc_err: 1;
		u32 clr_hs_tx_timeout: 1;
		u32 clr_lp_rx_timeout: 1;
		} bits;
	} INTERNAL_INT_CLR;

	union _0xD0 {
		u32 val;
		struct _VIDEO_SIG_DELAY_CONFIG {

		/*
		 * DPI interface signal delay to be used in clk lanebyte
		 * domain for control logic to read video data from pixel
		 * memory in mannal mode, measured in clk_lanebyte cycles
		 */
		u32 video_sig_delay: 24;

		/*
		 * 1'b1: mannal mode
		 *       dsi controller will use video_sig_delay value as
		 *       the delay for the packet handle logic to read video
		 *       data from pixel memory.
		 *
		 * 1'b0: auto mode
		 *       dsi controller will auto calculate the delay for
		 *       the packet handle logic to read video data from
		 *       pixel memory.
		 */
		u32 video_sig_delay_mode: 1;

		u32 reserved: 7;
		} bits;
	} VIDEO_SIG_DELAY_CONFIG;

	u32 reservedD4_EC[7];

	union _0xF0 {
		u32 val;
		struct _PHY_TST_CTRL0 {
		/* PHY test interface clear (active high) */
		u32 phy_testclr: 1;

		/* This bit is used to clock the TESTDIN bus into the D-PHY */
		u32 phy_testclk: 1;

		u32 reserved: 30;
		} bits;
	} PHY_TST_CTRL0;

	union _0xF4 {
		u32 val;
		struct _PHY_TST_CTRL1 {
		/* PHY test interface input 8-bit data bus for internal
		 * register programming and test functionalities access.
		 */
		u32 phy_testdin: 8;

		/* PHY output 8-bit data bus for read-back and internal
		 * probing functionalities.
		 */
		u32 phy_testdout: 8;

		/*
		 * PHY test interface operation selector:
		 * 1: The address write operation is set on the falling edge
		 *    of the testclk signal.
		 * 0: The data write operation is set on the rising edge of
		 *    the testclk signal.
		 */
		u32 phy_testen: 1;

		u32 reserved: 15;
		} bits;
	} PHY_TST_CTRL1;

	u32 reservedF8_1FC[66];

	union _0x200 {
		u32 val;
		struct _INT_PLL_STS {
		u32 int_pll_sts: 1;
		u32 reserved: 31;
		} bits;
	} INT_PLL_STS;

	union _0x204 {
		u32 val;
		struct _INT_PLL_MSK {
		u32 int_pll_msk: 1;
		u32 reserved: 31;
		} bits;
	} INT_PLL_MSK;

	union _0x208 {
		u32 val;
		struct _INT_PLL_CLR {
		u32 int_pll_clr: 1;
		u32 reserved: 31;
		} bits;
	} INT_PLL_CLR;

};

#endif /* _DSI_CTRL_R1P0_H_ */
