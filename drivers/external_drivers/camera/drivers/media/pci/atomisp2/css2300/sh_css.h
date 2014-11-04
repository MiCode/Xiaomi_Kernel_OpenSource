/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*
*/

/*! \file */

/*! \mainpage Introduction
This document describes the API that controls the software that is part of the
 HiveGo CSS 3016 series Camera System Solution. We will refer to this API as
 the CSS API.
Silicon Hive also provides a 3A library which comes with its own APIs; these
 will be described in a separate document.
This camera subsystem consists of 2 parts, the hardware subsystem and the
 software stack that implements the ISP functionality on top of this hardware.
 In this chapter we introduce both briefly. For more detailed information about
 the hardware or ISP features we refer to the ISP Datasheet.

\section css_hw_sec CSS Hardware
The following is a block diagram of the CSS hardware.
\image latex doc_hiveblockdia.png "HiveGo CSS 3016 Block Diagram" width=16cm
This diagram shows the following hardware blocks.

\subsection css_hw_isp_sec ISP2300 Vector Processor
This is a fully programmable 10 issue slot VLIW processor. Of these 10 issue
 slots, 9 are vector issue slots which contain 64-way vector (SIMD) operations.
 This processor is where all the ISP processing is implemented. Each ISP mode
 is implemented as one or more executables which run on this processor. This
 processor has local memory which is used as a line buffer. This memory can be
 access by the input formatters, DMA, GDC and Scalar Processor (SP).
\subsection css_hw_sp_sec Scalar Processor
The Scalar Processor (SP) is a control processor with a very small silicon
 footprint. It is used to program the subsystem. Next to full access to the
 local configuration bus this processor also has direct connections to several
 other blocks in the subsystem such as input formatters, DMA and GDC engine.
 Typically, this processor is used as a micro controller to hide hardware
 details from the host CPU code. It can also be used to offload control
 oriented tasks from the ISP.
\subsection css_hw_mipi_sec MIPI CSI2 Receiver
This is a hardwired MIPI CSI2 receiver. It is capable of receiving MIPI CSI2
 data streams via its 1lane or 4lane CSI2 ports. CSI2 decompression has been
 implemented here. The output of this receiver gets sent to one or more input
 formatters.
\subsection css_hw_if_sec Input Formatters
There are 3 input formatters in the subsystem. These input formatters convert a
 stream of incoming pixels into lines of pixel data in the ISP (vector) memory.
 These input formatters can be programmed via their control slave ports. This
 can be done either from the Scalar Processor or from the host CPU.
\subsection css_hw_dma_sec DMA
The DMA in the CSS subsystem is used to transfer large amounts of data such as
 input and output image data or large parameter sets. This DMA can be configured
 by the ISP or SP and has support for sign and zero extension while transferring
 data.
\subsection css_hw_mmu_sec MMU
The CSS subsystem contains an MMU to access external memory. This allows us to
 share page tables with the host CPU OS. The MMU is programmed by the host OS
 after memory has been allocated. It has 2 registers: one to set the L1 page
 table base address and one to invalidate the TLB inside the MMU.
 DEPRECATED: MMU programming is an HRT abckend function outside the scope of CSS
\subsection css_hw_gdc_sec GDC / Scaling Accelerator
Commonly referred to as "GDC block" this hardwired accelerator can perform
 the following functionality:
-# Scaling (upscaling, downscaling, digital zoom).
-# Geometric Distortion and Chromatic Abberation Correction.
This block is configured via its 32 bit slave port as well as the FIFO based
 connection with the ISP and SP.
\section css_sw_sec CSS Software
On top of the fully programmable CSS subsystem, we created an ISP software
 stack.
\subsection css_sw_isp_sec System integration
The CSS software is split into FW running on the ISP sub-system and a stub
on a (remote)host. The CSS API is the interface of the stub to the driver
the CSS DLI connects it to the FW. Since the stub can be integrated on different
systems. the connectivity to the ISP sub-system is not known. The integrator is
responsible for providing the connectivity functions, known as HRT backend
there are four HRT backend functions
-# Device access, the CSS assumes an interface as defined in "device_access.h"
-# Memory access, the CSS assumes an interface as defined in "memory_access.h"
-# Program load and start control. Undefined
-# Threads. Undefined
\subsection css_sw_isp_sec ISP Modes
This software ISP provides the following features:
-# Preview mode: a low-power movie mode used for preview. This produces one
 output stream of up to 720p resolution. This output stream is typically
 displayed on the LCD to represent the viewfinder.
-# Still capture modes: we provide 3 still capture modes. Each mode outputs
 2 data streams, one for the viewfinder (sometimes referred to as post-view)
 and one for the full resolution captures.
  -# Raw/YUV capture. In this mode, we capture the sensor output data directly
  to external memory, without any processing. This can be applied to RAW bayer
  data, YUV data and RGB data. In this mode we do provide some format
  conversions such as YUYV to YUV420 where data is only rearranged, not
  processed.
  -# Primary capture. This is the main still image capture mode. This mode
  contains image enhancement features especially selected for still capture.
  -# Advanced capture. This more advanced still capture mode contains the same
  features as the primary capture mode plus more advanced features such as
  Geometric Distortion and Chromatic Aberration Correction (GDCAC), support
  for Digital Image Stabilization (DIS) and Multi-Axis Color Correction (MACC).
-# Video capture mode: this mode is an enhanced version of the preview mode.
 It produces 2 output streams (viewfinder and full resolution output) and
 supports a capture resolution of up to 1080p. In addition to the features
 contained in the preview mode, it supports Temporal Noise Reduction (TNR)
 , Digital Video Stabilization (DVS) and other video specific ISP optimizations.
All modes support Digital Zoom, implemented on the GDC accelerator block and a
 range of ISP parameters to fine tune functionality such as edge enhancement,
 noise reduction, black level compensation and more.
\subsection css_sw_ctrl_sec Control Flow
Under normal circumstances, the ISP processes streaming input data (coming from
 a MIPI CSI2 compatible image sensor) and writes the output to a frame buffer in
 DDR. The following steps are taken to start this process:
-# The atomisp driver configures the CSS API regarding mode of operation, output
 resolution, data formats and parameters.
-# The atomisp driver starts the CSS API and then waits for interrupts. This is
 done for each frame.
-# The CSS API configures the CSS hardware and firmware by writing into
 registers and the SP data memory respectively.
-# The CSS API starts the SP by writing the start and run bits into its
 status & control register.
-# The SP firmware configures the rest of the CSS hardware and uses the DMA
 to download the ISP executable. Then it configures the ISP firmware by writing
 into the ISP data memory and starts the ISP processor.
-# While the ISP executes the firmware, the SP is used as a DMA proxy. The main
 reason for this is to save on the number of program instructions needed by the
 ISP firmware. The ISP has a local program memory of only 2048 instructions
 which means we can only execute a limited amount of code from this program
 memory. Configuring the DMA takes quite a number of instructions; by executing
 these from the SP we save program memory on the ISP.
-# While executing, the ISP continuously requests input data from the input
 formatter, processes this and requests the DMA to write the output data to
 external memory.
-# When the ISP is done processing the frame, it sends a stop token to the SP
 firmware. The SP then exits from its main function which brings the SP into
 the idle state. When this state transition occurs, the SP generates an
 interrupt which is handled on the host CPU.
-# The interrupt handler on the host CPU asks the CSS API what to do next. For
 certain modes, we need to execute multiple ISP executables in a row. When this
 is the case, the CSS API will tell the interrupt handler to start the next
 stage. This next stage is executed in the same way as the first one, just with
 a different ISP executable.
-# When the CSS API tells the interrupt handler that the frame is done (after
 all stages have been executed), the atomisp driver can send the output frame
 back to the application.
\subsection css_sw_data_sec Data Flow
The data flow depends on the mode of operation. In general though, we can say
 that the sensor output stream is stripped from all CSI markers and only the
 pixel data is sent to the input formatter(s). The input formatter converts the
 incoming stream into vectors (ISP SIMD words) and writes these vectors into the
 ISP vector memory.
The ISP receives a signal (token) from the input formatter and reads the new
 input data from its local vector memory into registers. The content of these
 registers is subsequently modified by instructions in the ISP data path. At
 the end of the processing cycle, the processed pixels are written to a second
 buffer in the same local vector memory. The DMA is then instructed to send this
 output data into external memory (into a buffer allocated on the host CPU).
In certain cases, this output data is then loaded from external memory by the
 application where it is displayed, encoded, processed or written to file.
In other cases, this data is further processed by other ISP executables. ISP
 executables that read input data from memory rather than from the CSI receiver
 are referred to as offline executables (or offline binaries).
\subsection css_sw_preview_sec Preview Mode
When running the ISP in preview mode, we execute 2 stages for each frame.
-# Preview stage: this does all line based image processing and writes the
 output frame to external memory. The input is acquired from the input
 formatter(s).
-# Vf_pp stage: this stage performs downscaling, digital zoom and format
 conversions. Since these are block based operations, implemented
 on the GDC block, we cannot combine it with the online line based image
 processing.
\section css_video_sec Video Mode
The video mode also consists of 2 stages:
-# Video stage. Similar to the preview stage in the preview mode, this stage
 performs line based image processing (including bayer downscaling when
 enabled). During this stage, the previous frame is read as a reference frame
 for temporal noise reduction. The input data is acquired from the input
 formatter(s) and the output frame is written to external memory. Since the
 reference frame must be created before digital zoom, this stage writes out
 2 frames, one is the output frame (possibly zoomed) and the other is the
 reference frame for the next run.
-# Vf_pp stage: this performs the same tasks as the vf_pp stage in the preview
 mode.
\section css_stillcap_sec Still Capture Mode
Here we distinguish 3 types.
\subsection css_stillcap_raw_sec RAW Capture
This mode captures RAW data from the sensor output to external memory. The only
 processing done here is re-ordering of pixels (for example to convert from
 yuv420 input data to nv12 output data). We only execute one stage in this mode
 which performs all processing necessary. Input is read from the input
 formatter(s) and output is written to external memory. This mode also produces
 a viewfinder output frame.
\subsection css_stillcap_primary_sec Primary Capture
In primary capture mode, we execute 2 or 3 stages, depending on which
 functionality is enabled:
-# Primary stage: this runs all line based image processing on input from either
 memory or the input formatter(s). This can be determined at runtime and is used
 to provide RAW+JPEG functionality where we capture both the RAW bayer
 frame and a processed YUV frame which is subsequently encoded to JPEG.
-# Capture_pp stage: this stage performs post-processing on the capture frame.
 This includes XNR, digital zoom and YUV downscaling. This stage is only
 executed when one or more of these features are enabled.
-# Vf_pp stage: this performs the same tasks as the vf_pp in preview and video
 mode.
\subsection css_stillcap_advanced_sec Advanced Capture
The advanced capture mode adds GDCAC capabilities. This is a block based
 algorithm which means we need additional stages.
-# Copy stage: this captures the RAW input data from the input formatter(s) into
 external memory.
-# Pre-GDC stage: this implements all line based image processing that needs to
 be done before the GDCAC stage.
-# GDC stage: this stage executes the GDCAC functionality on the
 GDC accelerator. It reads input data from external memory and writes output
 data back to external memory.
-# Post-GDC stage: this contains all line-based image processing that needs to
 be done after the GDCAC stage.
-# Capture_pp stage: this stage performs post-processing on the capture frame.
 This includes XNR, digital zoom and YUV downscaling. This stage is only
 executed when one or more of these features are enabled.
-# Vf_pp stage: this performs the same tasks as the vf_pp in preview and video
 mode.
 */

/*! \page css_intro API Introduction
\section css_intro_overview_sec Overview
The CSS API contains functions to control all software running on the CSS
 hardware. This software implements all features commonly found in a digital
 camera: preview, still capture and video capture.
This document is organized as follows:
- Common Data Types and Functions
- Preview
- Still Capture
- Video Capture
- ISP Parameters
- Example Applications
- API Reference

\section css_intro_versioning_sec Versioning
The API version is also embedded in the API header file. It is advised that
 applications check against this version such that version changes are
 automatically detected. Especially since the API is still in beta phase,
 differences between versions can be significant. It is also advised to check
 the version of this document against the actual API version in the header file,
 both should match.
\section css_intro_conventions_sec Notational Conventions
This document contains numerous code snippets and examples, all of these are
 printed like this:
\code
enum sh_css_err sh_css_preview_start(struct sh_css_frame *output_frame);
\endcode
When showing declarations that are part of the API, the declared symbol (type or
 function) will be printed in bold (as shown here for the function
 ::sh_css_preview_start).
\section css_intro_reference_sec API Reference
Each section will explain the use of several types and/or functions, the
 reference documentation for these is included at the end of each section. For
 each function, the prototype is shown, followed by the explanation of the
 function and a list of arguments and the return value. When the argument list
 or the return value is empty, it will be omitted.
\section css_intro_integration_sec Software Integration
The CSS API and its implementation do not depend on any particular environment
 or Operating System (OS). It does not use any features from external libraries
 such as libc, this means it can run in any OS and in kernel or user space mode.
 It is expected however that this API will normally be part of a kernel level
 driver.
*/

#ifndef _SH_CSS_H_
#define _SH_CSS_H_

#include <stdint.h>
#include <system_types.h>	/* hrt_vaddress, HAS_IRQ_MAP_VERSION_# */

#include "sh_css_types.h"

#if defined(__HOST__)
/*
 * For some reason the ISP code depends on the interface to the driver
 */
#include "input_system.h"	/* mipi_compressor_t */
#endif

struct sh_css_pipe;

/** Input modes, these enumerate all supported input modes.
 *  Note that not all ISP modes support all input modes.
 */
/* deprecated */
enum sh_css_input_mode {
	SH_CSS_INPUT_MODE_SENSOR, /**< data from sensor */
	SH_CSS_INPUT_MODE_FIFO,   /**< data from input-fifo */
	SH_CSS_INPUT_MODE_TPG,    /**< data from test-pattern generator */
	SH_CSS_INPUT_MODE_PRBS,   /**< data from pseudo-random bit stream */
	SH_CSS_INPUT_MODE_MEMORY  /**< data from a frame in memory */
};

/** The ISP streaming input interface supports the following formats.
 *  These match the corresponding MIPI formats.
 */
enum sh_css_input_format {
	SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY,    /**< 8 bits per subpixel */
	SH_CSS_INPUT_FORMAT_YUV420_8,  /**< 8 bits per subpixel */
	SH_CSS_INPUT_FORMAT_YUV420_10, /**< 10 bits per subpixel */
	SH_CSS_INPUT_FORMAT_YUV422_8,  /**< UYVY..UVYV, 8 bits per subpixel */
	SH_CSS_INPUT_FORMAT_YUV422_10, /**< UYVY..UVYV, 10 bits per subpixel */
	SH_CSS_INPUT_FORMAT_RGB_444,  /**< BGR..BGR, 4 bits per subpixel */
	SH_CSS_INPUT_FORMAT_RGB_555,  /**< BGR..BGR, 5 bits per subpixel */
	SH_CSS_INPUT_FORMAT_RGB_565,  /**< BGR..BGR, 5 bits B and $, 6 bits G */
	SH_CSS_INPUT_FORMAT_RGB_666,  /**< BGR..BGR, 6 bits per subpixel */
	SH_CSS_INPUT_FORMAT_RGB_888,  /**< BGR..BGR, 8 bits per subpixel */
	SH_CSS_INPUT_FORMAT_RAW_6,    /**< RAW data, 6 bits per pixel */
	SH_CSS_INPUT_FORMAT_RAW_7,    /**< RAW data, 7 bits per pixel */
	SH_CSS_INPUT_FORMAT_RAW_8,    /**< RAW data, 8 bits per pixel */
	SH_CSS_INPUT_FORMAT_RAW_10,   /**< RAW data, 10 bits per pixel */
	SH_CSS_INPUT_FORMAT_RAW_12,   /**< RAW data, 12 bits per pixel */
	SH_CSS_INPUT_FORMAT_RAW_14,   /**< RAW data, 14 bits per pixel */
	SH_CSS_INPUT_FORMAT_RAW_16,   /**< RAW data, 16 bits per pixel */
	SH_CSS_INPUT_FORMAT_BINARY_8, /**< Binary byte stream. */
	N_SH_CSS_INPUT_FORMAT
};

/** For RAW input, the bayer order needs to be specified separately. There
 *  are 4 possible orders. The name is constructed by taking the first two
 *  colors on the first line and the first two colors from the second line.
 */
enum sh_css_bayer_order {
	sh_css_bayer_order_grbg, /**< GRGRGRGRGR .. BGBGBGBGBG */
	sh_css_bayer_order_rggb, /**< RGRGRGRGRG .. GBGBGBGBGB */
	sh_css_bayer_order_bggr, /**< BGBGBGBGBG .. GRGRGRGRGR */
	sh_css_bayer_order_gbrg, /**< GBGBGBGBGB .. RGRGRGRGRG */
	N_sh_css_bayer_order
};

/** The still capture mode, this can be RAW (simply copy sensor input to DDR),
 *  Primary ISP, the Advanced ISP (GDC) or the low-light ISP (ANR).
 */
enum sh_css_capture_mode {
	SH_CSS_CAPTURE_MODE_RAW,      /**< no processing, copy data only */
	SH_CSS_CAPTURE_MODE_BAYER,    /**< pre ISP (bayer capture) */
	SH_CSS_CAPTURE_MODE_PRIMARY,  /**< primary ISP */
	SH_CSS_CAPTURE_MODE_ADVANCED, /**< advanced ISP (GDC) */
	SH_CSS_CAPTURE_MODE_LOW_LIGHT /**< low light ISP (ANR) */
};

/** Interrupt request type.
 *  When the CSS hardware generates an interrupt, a function in this API
 *  needs to be called to retrieve information about the interrupt.
 *  This interrupt type is part of this information and indicates what
 *  type of information the interrupt signals.
 *
 *  Note that one interrupt can carry multiple interrupt types. For
 *  example: the online video ISP will generate only 2 interrupts, one to
 *  signal that the statistics (3a and DIS) are ready and one to signal
 *  that all output frames are done (output and viewfinder).
 *
 * DEPRECATED, this interface is not portable it should only define user
 * (SW) interrupts
 */
#if defined(HAS_IRQ_MAP_VERSION_2)

enum sh_css_interrupt_info {
	/**< the current frame is done and a new one can be started */
	SH_CSS_IRQ_INFO_STATISTICS_READY = 1 << 0,
	/**< 3A + DIS statistics are ready. */

	SH_CSS_IRQ_INFO_CSS_RECEIVER_SOF = 1 << 9,
	/**< the css receiver received the start of frame */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_EOF = 1 << 10,

	/**< the css receiver received the end of frame */
	/* the input system in in error */
	SH_CSS_IRQ_INFO_INPUT_SYSTEM_ERROR = 1 << 3,
	/* the input formatter in in error */
	SH_CSS_IRQ_INFO_IF_ERROR = 1 << 4,
	/* the dma in in error */
	SH_CSS_IRQ_INFO_DMA_ERROR = 1 << 5,

	SH_CSS_IRQ_INFO_BUFFER_DONE	 = 1 << 7,
	/**< Either VF or VFOUT is done */
	SH_CSS_IRQ_INFO_PIPELINE_DONE	 = 1 << 8,
	/**< A pipeline is done */

	SH_CSS_IRQ_INFO_SW_0 = 1 << 17,
	/**< software interrupt 0 */
	SH_CSS_IRQ_INFO_SW_1 = 1 << 18,
	/**< software interrupt 1 */

	SH_CSS_IRQ_INFO_INVALID_FIRST_FRAME = 1 << 20,
	/**< Inform the ISR that there is an invalid first frame */
	SH_CSS_IRQ_INFO_OUTPUT_FRAME_DONE	= 1 << 21,
	/**< Output frame done */
	SH_CSS_IRQ_INFO_VF_OUTPUT_FRAME_DONE	= 1 << 22,
	/**< Viewfinder output frame done */
	SH_CSS_IRQ_INFO_RAW_OUTPUT_FRAME_DONE	= 1 << 23,
	/**< RAW output frame done */
	SH_CSS_IRQ_INFO_3A_STATISTICS_DONE	= 1 << 24,
	/**< 3A statistics are ready */
	SH_CSS_IRQ_INFO_DIS_STATISTICS_DONE	= 1 << 25,
	/**< DIS statistics are ready */
	SH_CSS_IRQ_INFO_INPUT_FRAME_DONE	= 1 << 26,
	/**< Input frame will no longer be accessed */
	SH_CSS_IRQ_INFO_CUSTOM_INPUT_DONE	= 1 << 27,
	/**< Custom input buffer will no longer be accessed */
	SH_CSS_IRQ_INFO_CUSTOM_OUTPUT_DONE	= 1 << 28,
	/**< Custom output buffer is available */
	SH_CSS_IRQ_INFO_ISP_BINARY_STATISTICS_READY = 1 << 29,
	/**< ISP binary statistics are ready */
};

#elif defined(HAS_IRQ_MAP_VERSION_1) || defined(HAS_IRQ_MAP_VERSION_1_DEMO)

enum sh_css_interrupt_info {
	/**< the current frame is done and a new one can be started */
	SH_CSS_IRQ_INFO_STATISTICS_READY = 1 << 0,
	/**< 3A + DIS statistics are ready. */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_ERROR = 1 << 1,
	/**< the css receiver has encountered an error */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW = 1 << 2,
	/**< the FIFO in the csi receiver has overflown */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_SOF = 1 << 3,
	/**< the css receiver received the start of frame */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_EOF = 1 << 4,
	/**< the css receiver received the end of frame */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_SOL = 1 << 6,
	/**< the css receiver received the start of line */
	SH_CSS_IRQ_INFO_BUFFER_DONE	 = 1 << 7,
	/**< Either VF or VFOUT is done */
	SH_CSS_IRQ_INFO_PIPELINE_DONE	 = 1 << 8,
	/**< A pipeline is done */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_EOL = 1 << 9,
	/**< the css receiver received the end of line */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_SIDEBAND_CHANGED = 1 << 10,
	/**< the css receiver received a change in side band signals */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_0 = 1 << 11,
	/**< generic short packets (0) */
	SH_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_1 = 1 << 12,
	/**< generic short packets (1) */
	SH_CSS_IRQ_INFO_IF_PRIM_ERROR = 1 << 13,
	/**< the primary input formatter (A) has encountered an error */
	SH_CSS_IRQ_INFO_IF_PRIM_B_ERROR = 1 << 14,
	/**< the primary input formatter (B) has encountered an error */
	SH_CSS_IRQ_INFO_IF_SEC_ERROR = 1 << 15,
	/**< the secondary input formatter has encountered an error */
	SH_CSS_IRQ_INFO_STREAM_TO_MEM_ERROR = 1 << 16,
	/**< the stream-to-memory device has encountered an error */
	SH_CSS_IRQ_INFO_SW_0 = 1 << 17,
	/**< software interrupt 0 */
	SH_CSS_IRQ_INFO_SW_1 = 1 << 18,
	/**< software interrupt 1 */
	SH_CSS_IRQ_INFO_SW_2 = 1 << 19,
	/**< software interrupt 2 */
	SH_CSS_IRQ_INFO_INVALID_FIRST_FRAME = 1 << 20,
	/**< Inform the ISR that there is an invalid first frame */
	SH_CSS_IRQ_INFO_OUTPUT_FRAME_DONE	= 1 << 21,
	/**< Output frame done */
	SH_CSS_IRQ_INFO_VF_OUTPUT_FRAME_DONE	= 1 << 22,
	/**< Viewfinder output frame done */
	SH_CSS_IRQ_INFO_RAW_OUTPUT_FRAME_DONE	= 1 << 23,
	/**< RAW output frame done */
	SH_CSS_IRQ_INFO_3A_STATISTICS_DONE	= 1 << 24,
	/**< 3A statistics are ready */
	SH_CSS_IRQ_INFO_DIS_STATISTICS_DONE	= 1 << 25,
	/**< DIS statistics are ready */
	SH_CSS_IRQ_INFO_INPUT_FRAME_DONE	= 1 << 26,
	/**< Input frame will no longer be accessed */
	SH_CSS_IRQ_INFO_CUSTOM_INPUT_DONE	= 1 << 27,
	/**< Custom input buffer will no longer be accessed */
	SH_CSS_IRQ_INFO_CUSTOM_OUTPUT_DONE	= 1 << 28,
	/**< Custom output buffer is available */
	SH_CSS_IRQ_INFO_ISP_BINARY_STATISTICS_READY = 1 << 29,
	/**< ISP binary statistics are ready */
};

#else
#error "sh_css.h: IRQ MAP must be one of \
	{IRQ_MAP_VERSION_1, IRQ_MAP_VERSION_1_DEMO, IRQ_MAP_VERSION_2}"
#endif

/** CSS receiver error types. Whenever the CSS receiver has encountered
 *  an error, this enumeration is used to indicate which errors have occurred.
 *
 *  Note that multiple error flags can be enabled at once and that this is in
 *  fact common (whenever an error occurs, it usually results in multiple
 *  errors).
 *
 * DEPRECATED: This interface is not portable, different systems have
 * different receiver types, or possibly none in case of tests systems.
 */
enum sh_css_rx_irq_info {
	SH_CSS_RX_IRQ_INFO_BUFFER_OVERRUN   = 1U << 0, /**< buffer overrun */
	SH_CSS_RX_IRQ_INFO_ENTER_SLEEP_MODE = 1U << 1, /**< entering sleep mode */
	SH_CSS_RX_IRQ_INFO_EXIT_SLEEP_MODE  = 1U << 2, /**< exited sleep mode */
	SH_CSS_RX_IRQ_INFO_ECC_CORRECTED    = 1U << 3, /**< ECC corrected */
	SH_CSS_RX_IRQ_INFO_ERR_SOT          = 1U << 4,
						/**< Start of transmission */
	SH_CSS_RX_IRQ_INFO_ERR_SOT_SYNC     = 1U << 5, /**< SOT sync (??) */
	SH_CSS_RX_IRQ_INFO_ERR_CONTROL      = 1U << 6, /**< Control (??) */
	SH_CSS_RX_IRQ_INFO_ERR_ECC_DOUBLE   = 1U << 7, /**< Double ECC */
	SH_CSS_RX_IRQ_INFO_ERR_CRC          = 1U << 8, /**< CRC error */
	SH_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ID   = 1U << 9, /**< Unknown ID */
	SH_CSS_RX_IRQ_INFO_ERR_FRAME_SYNC   = 1U << 10,/**< Frame sync error */
	SH_CSS_RX_IRQ_INFO_ERR_FRAME_DATA   = 1U << 11,/**< Frame data error */
	SH_CSS_RX_IRQ_INFO_ERR_DATA_TIMEOUT = 1U << 12,/**< Timeout occurred */
	SH_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ESC  = 1U << 13,/**< Unknown escape seq. */
	SH_CSS_RX_IRQ_INFO_ERR_LINE_SYNC    = 1U << 14,/**< Line Sync error */
#if defined(HAS_RX_VERSION_2)
	SH_CSS_RX_IRQ_INFO_INIT_TIMEOUT     = 1U << 15,
#endif
};

/** Errors, these values are used as the return value for most
 *  functions in this API.
 */
enum sh_css_err {
	sh_css_success,
	sh_css_err_internal_error,
	sh_css_err_conflicting_mipi_settings,
	sh_css_err_unsupported_configuration,
	sh_css_err_mode_does_not_have_viewfinder,
	sh_css_err_input_resolution_not_set,
	sh_css_err_unsupported_input_mode,
	sh_css_err_cannot_allocate_memory,
	sh_css_err_invalid_arguments,
	sh_css_err_too_may_colors,
	sh_css_err_unsupported_frame_format,
	sh_css_err_frames_mismatch,
	sh_css_err_not_implemented,
	sh_css_err_invalid_frame_format,
	sh_css_err_unsupported_resolution,
	sh_css_err_scaling_factor_out_of_range,
	sh_css_err_cannot_obtain_shading_table,
	sh_css_err_interrupt_error,
	sh_css_err_unexpected_interrupt,
	sh_css_err_interrupts_not_enabled,
	sh_css_err_system_not_idle,
	sh_css_err_unsupported_input_format,
	sh_css_err_not_enough_input_lines,
	sh_css_err_not_enough_input_columns,
	sh_css_err_illegal_resolution,
	sh_css_err_effective_input_resolution_not_set,
	sh_css_err_viewfinder_resolution_too_wide,
	sh_css_err_viewfinder_resolution_exceeds_output,
	sh_css_err_mode_does_not_have_grid,
	sh_css_err_mode_does_not_have_raw_output,
	sh_css_err_unknown_event,
	sh_css_err_event_queue_is_empty,
	sh_css_err_buffer_queue_is_full,
	sh_css_err_buffer_queue_is_empty,
	sh_css_err_invalid_tag_description,
	sh_css_err_tag_queue_is_full,
	sh_css_err_isp_binary_header_mismatch
};

/** Generic resolution structure.
 */
struct sh_css_resolution {
	unsigned int width;  /**< Width */
	unsigned int height; /**< Height */
};

/** Frame plane structure. This describes one plane in an image
 *  frame buffer.
 */
struct sh_css_frame_plane {
	unsigned int height; /**< height of a plane in lines */
	unsigned int width;  /**< width of a line, in DMA elements, note that
				  for RGB565 the three subpixels are stored in
				  ne element. For all other formats this is
				   the number of subpixels per line. */
	unsigned int stride; /**< stride of a line in bytes */
	unsigned int offset; /**< offset in bytes to start of frame data.
				  offset is wrt data field in sh_css_frame */
};

/** Binary "plane". This is used to story binary streams such as jpeg
 *  images. This is not actually a real plane.
 */
struct sh_css_frame_binary_plane {
	unsigned int		  size; /**< number of bytes in the stream */
	struct sh_css_frame_plane data; /**< plane */
};

/** Container for planar YUV frames. This contains 3 planes.
 */
struct sh_css_frame_yuv_planes {
	struct sh_css_frame_plane y; /**< Y plane */
	struct sh_css_frame_plane u; /**< U plane */
	struct sh_css_frame_plane v; /**< V plane */
};

/** Container for semi-planar YUV frames.
  */
struct sh_css_frame_nv_planes {
	struct sh_css_frame_plane y;  /**< Y plane */
	struct sh_css_frame_plane uv; /**< UV plane */
};

/** Container for planar RGB frames. Each color has its own plane.
 */
struct sh_css_frame_rgb_planes {
	struct sh_css_frame_plane r; /**< Red plane */
	struct sh_css_frame_plane g; /**< Green plane */
	struct sh_css_frame_plane b; /**< Blue plane */
};

/** Container for 6-plane frames. These frames are used internally
 *  in the advanced ISP only.
 */
struct sh_css_frame_plane6_planes {
	struct sh_css_frame_plane r;	  /**< Red plane */
	struct sh_css_frame_plane r_at_b; /**< Red at blue plane */
	struct sh_css_frame_plane gr;	  /**< Red-green plane */
	struct sh_css_frame_plane gb;	  /**< Blue-green plane */
	struct sh_css_frame_plane b;	  /**< Blue plane */
	struct sh_css_frame_plane b_at_r; /**< Blue at red plane */
};

/** Frame info struct. This describes the contents of an image frame buffer.
  */
struct sh_css_frame_info {
	unsigned int width;  /**< width of valid data in pixels */
	unsigned int height; /**< Height of valid data in lines */
	unsigned int padded_width; /**< stride of line in memory (in pixels) */
	enum sh_css_frame_format format; /**< format of the frame data */
	unsigned int raw_bit_depth; /**< number of valid bits per pixel,
					 only valid for RAW bayer frames */
	enum sh_css_bayer_order raw_bayer_order; /**< bayer order, only valid
						      for RAW bayer frames */
};

enum sh_css_frame_flash_state {
	SH_CSS_FRAME_NO_FLASH = 0,
	SH_CSS_FRAME_PARTIAL_FLASH,
	SH_CSS_FRAME_FULLY_FLASH
};

/** Frame structure. This structure describes an image buffer or frame.
 *  This is the main structure used for all input and output images.
 */
struct sh_css_frame {
	struct sh_css_frame_info info; /**< info struct describing the frame */
	hrt_vaddress data;	       /**< pointer to start of image data */
	unsigned int data_bytes;       /**< size of image data in bytes */
	/* LA: move this to sh_css_buffer */
	/*
	 * -1 if data address is static during life time of pipeline
	 * >=0 if data address can change per pipeline/frame iteration
	 *     index to dynamic data: sh_css_frame_in, sh_css_frame_out
	 *                            sh_css_frame_out_vf
	 */
	int dynamic_data_index;
	enum sh_css_frame_flash_state flash_state;
	unsigned int exp_id; /**< exposure id, only valid for continuous
				capture cases */
	bool contiguous; /**< memory is allocated physically contiguously */
	union {
		unsigned int	_initialisation_dummy;
		struct sh_css_frame_plane raw;
		struct sh_css_frame_plane rgb;
		struct sh_css_frame_rgb_planes planar_rgb;
		struct sh_css_frame_plane yuyv;
		struct sh_css_frame_yuv_planes yuv;
		struct sh_css_frame_nv_planes nv;
		struct sh_css_frame_plane6_planes plane6;
		struct sh_css_frame_binary_plane binary;
	} planes; /**< frame planes, select the right one based on
		       info.format */
};

/** CSS firmware package structure.
 */
struct sh_css_fw {
	void	    *data;  /**< pointer to the firmware data */
	unsigned int bytes; /**< length in bytes of firmware data */
};

union sh_css_s3a_data {
	struct {
		hrt_vaddress s3a_tbl;
	} dmem;
	struct {
		hrt_vaddress s3a_tbl_hi;
		hrt_vaddress s3a_tbl_lo;
	} vmem;
};

struct sh_css_dis_data {
	hrt_vaddress sdis_hor_proj;
	hrt_vaddress sdis_ver_proj;
};

/** Enumeration of buffer types. Buffers can be queued and de-queued
 *  to hand them over between IA and ISP.
 */
enum sh_css_buffer_type {
	SH_CSS_BUFFER_TYPE_3A_STATISTICS,
	SH_CSS_BUFFER_TYPE_DIS_STATISTICS,
	SH_CSS_BUFFER_TYPE_INPUT_FRAME,
	SH_CSS_BUFFER_TYPE_OUTPUT_FRAME,
	SH_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME,
	SH_CSS_BUFFER_TYPE_RAW_OUTPUT_FRAME,
	SH_CSS_BUFFER_TYPE_CUSTOM_INPUT,
	SH_CSS_BUFFER_TYPE_CUSTOM_OUTPUT,
	SH_CSS_BUFFER_TYPE_PARAMETER_SET,
	SH_CSS_BUFFER_TYPE_NR_OF_TYPES,		/* must be last */
};



/** Environment with function pointers for local IA memory allocation.
 *  This provides the CSS code with environment specific functionality
 *  for memory allocation of small local buffers such as local data structures.
 *  This is never expected to allocate more than one page of memory (4K bytes).
 */
struct sh_css_sh_env {
	void *(*alloc)(size_t bytes, bool zero_mem);
	/**< Allocation function with boolean argument to indicate whether
	     the allocated memory should be zeroed out or not. */
	void (*free)(void *ptr); /**< Corresponding free function. */
	void (*flush) (struct sh_css_acc_fw *fw);
	/**< Flush function to flush the cache for given accelerator. */
};


/** Environment with function pointers to print error and debug messages.
 */
struct sh_css_print_env {
	int (*debug_print)(const char *fmt, ...); /**< Print a debug message. */
	int (*error_print)(const char *fmt, ...); /**< Print an error message.*/
};

/** Environment structure. This includes function pointers to access several
 *  features provided by the environment in which the CSS API is used.
 *  This is used to run the camera IP in multiple platforms such as Linux,
 *  Windows and several simulation environments.
 */
struct sh_css_env {
	struct sh_css_sh_env	     sh_env; /**< local malloc and free. */
	struct sh_css_print_env      print_env; /**< Message printing env. */
};

/** Buffer structure. This is a container structure that enables content
 *  independent buffer queues and access functions.
 */
#if 1
struct sh_css_buffer {
	enum sh_css_buffer_type type; /**< Buffer type. */
	union {
		struct sh_css_frame	     *frame;    /**< Frame buffer. */
	} data; /**< Buffer data pointer. */
};
#else
struct sh_css_buffer {
	enum sh_css_buffer_type type; /**< Buffer type. */
	union {
		struct sh_css_3a_statistics  *stats_3a; /**< 3A statistics. */
		struct sh_css_dis_statistics *stats_dis;/**< DIS statistics. */
		struct sh_css_frame	     *frame;    /**< Frame buffer. */
		struct sh_css_acc_param      *custom_data; /**< Custom buffer. */
	} data; /**< Buffer data pointer. */
};

#endif

/** The event type, distinguishes the kind of events that
 * can are generated by the CSS system.
 */
enum sh_css_event_type {
  SH_CSS_EVENT_NULL,
  SH_CSS_EVENT_OUTPUT_FRAME_DONE,
  SH_CSS_EVENT_VF_OUTPUT_FRAME_DONE,
  SH_CSS_EVENT_3A_STATISTICS_DONE,
  SH_CSS_EVENT_DIS_STATISTICS_DONE,
  SH_CSS_EVENT_PIPELINE_DONE,
  SH_CSS_NR_OF_EVENTS,
};

/**
 * Type used with sh_css_event_set_irq_mask()
 */
enum sh_css_event_irq_mask_type {
	SH_CSS_EVENT_IRQ_MASK_NONE			= 0,
	SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE		= 1 << 0,
	SH_CSS_EVENT_IRQ_MASK_VF_OUTPUT_FRAME_DONE	= 1 << 1,
	SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE	= 1 << 2,
	SH_CSS_EVENT_IRQ_MASK_DIS_STATISTICS_DONE	= 1 << 3,
	SH_CSS_EVENT_IRQ_MASK_PIPELINE_DONE		= 1 << 4,
	SH_CSS_EVENT_IRQ_MASK_UNUSED			= 1 << 15,
	SH_CSS_EVENT_IRQ_MASK_ALL			= 0xFFFF
};


/** The pipe id type, distinguishes the kind of pipes that
 *  can be run in parallel.
 */
enum sh_css_pipe_id {
  SH_CSS_PREVIEW_PIPELINE,
  SH_CSS_COPY_PIPELINE,
  SH_CSS_VIDEO_PIPELINE,
  SH_CSS_CAPTURE_PIPELINE,
  SH_CSS_ACC_PIPELINE,
  SH_CSS_NR_OF_PIPELINES,	/* must be last */
};


/** Interrupt info structure. This structure contains information about an
 *  interrupt. This needs to be used after an interrupt is received on the IA
 *  to perform the correct action.
 */
struct sh_css_irq_info {
	enum sh_css_interrupt_info type; /**< Interrupt type. */
	unsigned int sw_irq_0_val; /**< In case of SW interrupt 0, value. */
	unsigned int sw_irq_1_val; /**< In case of SW interrupt 1, value. */
	unsigned int sw_irq_2_val; /**< In case of SW interrupt 2, value. */
	enum sh_css_pipe_id pipe;
	/**< The image pipe that generated the interrupt. */
};

/* ===== GENERIC ===== */

void sh_css_set_stop_timeout(unsigned int timeout);

/** @brief Return the default environment.
 *
 * @return		the default environment.
 *
 * This function return a default environemnt in which
 * isp_buffer_env and css_hw_env are fully defined,
 * sh_env and print_env are undefined.
*/
struct sh_css_env sh_css_default_env(void);

/** @brief Initialize the CSS API.
 * @param[in]	env		Environment, provides functions to access the
 *				environment in which the CSS code runs. This is
 *				used for host side memory access and message
 *				printing.
 * @param[in]	fw_data		Firmware package containing the firmware for all
 *				predefined ISP binaries.
 * @param[in]	fw_size		Size of the irmware package in bytes.
 * @return			Returns SH_CSS_ERR_INTERNAL_ERROR in case of any
 *				errors and SH_CSS_SUCCESS otherwise.
 *
 * This function initializes the API which includes allocating and initializing
 * internal data structures. This also interprets the firmware package. All
 * contents of this firmware package are copied into local data structures, so
 * this pointer could be freed after this function completes.
 */
enum sh_css_err sh_css_init(
	const struct sh_css_env *env,
	const char			*fw_data,
	const unsigned int	fw_size);

#if defined(__HOST__)

/** @brief Un-initialize the CSS API.
 *
 * This function deallocates all memory that has been allocated by the CSS
 * API. After this function is called, no other CSS functions should be called
 * with the exception of sh_css_init which will re-initialize the CSS code.
 *
 * @return None
 */
void
sh_css_uninit(void);

/** @brief Suspend CSS API for power down.
 *
 * This function prepares the CSS API for a power down of the CSS hardware.
 * This will make sure the hardware is idle. After this function is called,
 * always call ia_css_resume before calling any other CSS functions.
 * This assumes that all buffers allocated in DDR will remain alive during
 * power down. If this is not the case, use ia_css_unit() followed by
 * ia_css_init() at power up.
 */
void
sh_css_suspend(void);

/** @brief Resume CSS API from power down
 *
 * After a power cycle, this function will bring the CSS API back into
 * a state where it can be started. This will re-initialize the hardware.
 * Call this function only after ia_css_suspend() has been called.
 */
void
sh_css_resume(void);

/** @brief Obtain interrupt information.
 *
 * @param[out] info	Pointer to the interrupt info. The interrupt
 *			information wil be written to this info.
 * @return		If an error is encountered during the interrupt info
 *			and no interrupt could be translated successfully, this
 *			will return SH_CSS_INTERNAL_ERROR. Otherwise
 *			SH_CSS_SUCCESS.
 *
 * This function is expected to be executed after an interrupt has been sent
 * to the IA from the CSS. This function returns information about the interrupt
 * which is needed by the IA code to properly handle the interrupt. This
 * information includes the image pipe, buffer type etc.
 */
enum sh_css_err
sh_css_translate_interrupt(unsigned int *info);

/** @brief Get CSI receiver error info.
 *
 * @param[out] irq_bits	Pointer to the interrupt bits. The interrupt
 *			bits will be written this info.
 *			This will be the error bits that are enabled in the CSI
 *			receiver error register.
 * This function should be used whenever a CSI receiver error interrupt is
 * generated. It provides the detailed information (bits) on the exact error
 * that occurred.
 */
void
sh_css_rx_get_interrupt_info(unsigned int *irq_bits);

/** @brief Clear CSI receiver error info.
 *
 * @param[in] irq_bits	The bits that should be cleared from the CSI receiver
 *			interrupt bits register.
 *
 * This function should be called after ia_css_rx_get_irq_info has been called
 * and the error bits have been interpreted. It is advised to use the return
 * value of that function as the argument to this function to make sure no new
 * error bits get overwritten.
 */
void
sh_css_rx_clear_interrupt_info(unsigned int irq_bits);

/** @brief Enable or disable specific interrupts.
 *
 * @param[in] type	The interrupt type that will be enabled/disabled.
 * @param[in] enable	enable or disable.
 * @return		Returns SH_CSS_INTERNAL_ERROR if this interrupt
 *			type cannot be enabled/disabled which is true for
 *			CSS internal interrupts. Otherwise returns
 *			SH_CSS_SUCCESS.
 */
enum sh_css_err
sh_css_enable_interrupt(enum sh_css_interrupt_info type, bool enable);

/** @brief Return the value of a SW interrupt.
 *
 * @param[in] irq	The software interrupt id.
 * @return		The value for the software interrupt.
 */
unsigned int
sh_css_get_sw_interrupt_value(unsigned int irq);


/** @brief Set the address of the L1 page table for the CSS MMU.
 *
 * @param[in] base_index	The base address of the L1 page table.
 *
 * This address will be used by the CSS MMU to retrieve page mapping
 * information to translate virtual addresses to physical addresses.
 *
 * DEPRECATED
 */
void
sh_css_mmu_set_page_table_base_index(hrt_data base_index);

/** @brief Get the current L1 page table base address.
 *
 * @return	The currently set L1 page table base address.
 *
 * DEPRECATED
 */
hrt_data
sh_css_mmu_get_page_table_base_index(void);

/** @brief Invalidate the MMU internal cache.
 *
 * This function triggers an invalidation of the translate-look-aside
 * buffer (TLB) that's inside the CSS MMU. This function should be called
 * every time the page tables used by the MMU change.
 *
 * DEPRECATED, use the MMU access function
 */
void
sh_css_mmu_invalidate_cache(void);

#if 1
/** @brief Starts the CSS system.
 *
 * Starts the CSS system. The provided buffers are only used to construct
 * the static part of the pipeline with the stages. The dynamic data in
 * the buffers are not used and need to be queued with a seperate call
 * to sh_css_queue_buffer. The type of the buffers is still CSS V1.0 style
 */
enum sh_css_err
sh_css_start(enum sh_css_pipe_id   pipe);
#endif

void
sh_css_update_isp_params(void);

void
sh_css_queue_free_stat_bufs2sp(enum sh_css_pipe_id pipe);

/* CSS 1.5: to be implemented */
/** @brief Queue a buffer for an image pipe.
 *
 * @param[in] pipe	The pipe that will own the buffer.
 * @param[in] buffer	Pointer to the buffer.
 * @return		IA_CSS_INTERNAL_ERROR in case of unexpected errors,
 *			IA_CSS_SUCCESS otherwise.
 *
 * This function adds a buffer (which has a certain buffer type) to the queue
 * for this type. This queue is owned by the image pipe. After this function
 * completes successfully, the buffer is now owned by the image pipe and should
 * no longer be accessed by any other code until it gets dequeued. The image
 * pipe will dequeue buffers from this queue, use them and return them to the
 * host code via an interrupt. Buffers will be consumed in the same order they
 * get queued, but may be returned to the host out of order.
 */
#if 1
/* NOTE: The type of the buffers is still CSS V1.0 style. The buffer type is
 * provided as a seperate parameter
 */
enum sh_css_err
sh_css_queue_buffer(enum sh_css_pipe_id   pipe,
			enum sh_css_buffer_type buf_type,
			void *buffer);
#else
enum sh_css_err
sh_css_queue_buffer(enum sh_css_pipe_id   pipe,
		    struct sh_css_buffer *buffer);
#endif

/* CSS 1.5: to be implemented */
/** @brief Dequeue a buffer from an image pipe.
 *
 * @param[in]	pipe	The pipeline that the buffer queue belongs to.
 * @param	type	The type of buffer that should be dequeued.
 * @param[out]	buffer	Pointer to the buffer that was dequeued.
 * @return		IA_CSS_ERR_NO_BUFFER if the queue is empty or
 *			IA_CSS_SUCCESS otherwise.
 *
 * This function dequeues a buffer from a buffer queue. The queue is indicated
 * by the buffer type argument. This function can be called after an interrupt
 * has been generated that signalled that a new buffer was available and can
 * be used in a polling-like situation where the NO_BUFFER return value is used
 * to determine whether a buffer was available or not.
 */
#if 1
/* NOTE: The type of the buffers is still CSS V1.0 style. The buffer type is
 * provided as a seperate parameter
 */
enum sh_css_err
sh_css_dequeue_buffer(enum sh_css_pipe_id   pipe,
			enum sh_css_buffer_type buf_type,
			void **buffer);
#else
enum sh_css_err
sh_css_dequeue_buffer(enum sh_css_pipe_id	pipe,
		      enum sh_css_buffer_type	type,
		      struct sh_css_buffer    **buffer);
#endif

/* CSS 1.5: to be implemented */
/** @brief Dequeue an event from the CSS system. An event consists of pipe_id
 * and event_id.
 *
 * @param[out]	pipe_id	Pointer to the pipe_id of the event that was dequeued.
 *			Must be non-Null.
 * @param[out]	event_id Pointer to the event_id of the event that was dequeued.
 *			Must be non-Null.
 * @return		sh_css_err_event_queue_is_empty if no events are
 *			available or
 *			sh_css_success otherwise.
 *
 * This function dequeues an event from an event queue. The queue is inbetween
 * the Host (i.e. the Atom processosr) and the CSS system. This function can be
 * called after an interrupt has been generated that signalled that a new event
 * was available and can be used in a polling-like situation where the NO_EVENT
 * return value is used to determine whether an event was available or not.
 */
enum sh_css_err
sh_css_dequeue_event(enum sh_css_pipe_id *pipe_id,
			enum sh_css_event_type *event_id);

/** @brief Controls when the Event generator raises an IRQ to the Host.
 *
 * @param[in]	pipe_id	Id of the pipe e.g. SH_CSS_PREVIEW_PIPELINE.
 * @param[in]	or_mask	Binary or of enum sh_css_event_irq_mask_type. Each event
			that is part of this mask will directly raise an IRQ to
			the Host when the event occurs in the CSS.
 * @param[in]	and_mask Binary or of enum sh_css_event_irq_mask_type. An event
			IRQ for the Host is only raised after all events have
			occurred at least once for all the active pipes. Events
			are remembered and don't need to occure at the same
			moment in time. There is no control over the order of
			these events. Once an IRQ has been raised all
			remembered events are reset.
 * @return		sh_css_success.
 *
 Controls when the Event generator in the CSS raises an IRQ to the Host.
 The main purpose of this function is to reduce the amount of interrupts
 between the CSS and the Host. This will help saving power as it wakes up the
 Host less often. In case both or_mask and and_mask are
 SH_CSS_EVENT_IRQ_MASK_NONE for all pipes, no event IRQ's will be raised.
 Note that events are still queued and the Host can poll for them. The
 or_mask and and_mask may be be active at the same time\n
 \n
 Default values, for all pipe id's, after sh_css_init:\n
 or_mask = SH_CSS_EVENT_IRQ_MASK_ALL\n
 and_mask = SH_CSS_EVENT_IRQ_MASK_NONE\n
 \n
 Precondition:\n
 sh_css_init_buffer_queues() must be executed first\n
 \n
 Examples\n
 \code
 sh_css_event_set_irq_mask(SH_CSS_PREVIEW_PIPELINE,
 SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE |
 SH_CSS_EVENT_IRQ_MASK_DIS_STATISTICS_DONE ,
 SH_CSS_EVENT_IRQ_MASK_NONE);
 \endcode
 The event generator will only raise an interrupt to the Host when there are
 3A or DIS statistics available from the preview pipe. It will not generate
 an interrupt for any other event of the preview pipe e.g when there is an
 output frame available.

 \code
 sh_css_event_set_irq_mask(SH_CSS_PREVIEW_PIPELINE,
	SH_CSS_EVENT_IRQ_MASK_NONE,
	SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE |
	SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE );

 sh_css_event_set_irq_mask(SH_CSS_CAPTURE_PIPELINE,
	SH_CSS_EVENT_IRQ_MASK_NONE,
	SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE );
 \endcode
 The event generator will only raise an interrupt to the Host when there is
 both a frame done and 3A event available from the preview pipe AND when there
 is a frame done available from the capture pipe. Note that these events
 may occur at different moments in time. Also the order of the events is not
 relevant.

 \code
 sh_css_event_set_irq_mask(SH_CSS_PREVIEW_PIPELINE,
	SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE,
	SH_CSS_EVENT_IRQ_MASK_ALL );

 sh_css_event_set_irq_mask(SH_CSS_COPY_PIPELINE,
	SH_CSS_EVENT_IRQ_MASK_NONE,
	SH_CSS_EVENT_IRQ_MASK_NONE );

 sh_css_event_set_irq_mask(SH_CSS_CAPTURE_PIPELINE,
	SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE,
	SH_CSS_EVENT_IRQ_MASK_ALL );
 \endcode
 The event generator will only raise an interrupt to the Host when there is an
 output frame from the preview pipe OR an output frame from the capture pipe.
 All other events (3A, VF output, pipeline done) will not raise an interrupt
 to the Host. These events are not lost but always stored in the event queue.
 */
enum sh_css_err
sh_css_event_set_irq_mask(
	enum sh_css_pipe_id pipe_id,
	unsigned int or_mask,
	unsigned int and_mask);

/** @brief Reads the current event IRQ mask from the CSS.
 *
 * @param[in]	pipe_id	Id of the pipe e.g. SH_CSS_PREVIEW_PIPELINE.
 * @param[out]	or_mask	Current or_mask. The bits in this mask are a binary or
		of enum sh_css_event_irq_mask_type. Pointer may be NULL.
 * @param[out]	and_mask Current and_mask.The bits in this mask are a binary or
		of enum sh_css_event_irq_mask_type. Pointer may be NULL.
 * @return	sh_css_success.
 *
 Reads the current event IRQ mask from the CSS. Reading returns the actual
 values as used by the SP and not any mirrored values stored at the Host.\n
\n
Precondition:\n
sh_css_init_buffer_queues() must be executed first\n

*/
enum sh_css_err
sh_css_event_get_irq_mask(
	enum sh_css_pipe_id pipe_id,
	unsigned int *or_mask,
	unsigned int *and_mask);

void
sh_css_event_init_irq_mask(void);


/** @brief Return whether UV range starts at 0.
 *
 * @param[out]	uv_offset_is_zero	Pointer to the result value.

 *  Return true if UV values range from 0 to 255 and false if UV values
 *  range from -127 to 128.
 */
void
sh_css_uv_offset_is_zero(bool *uv_offset_is_zero);

/** @brief Wait for completion of an ISP mode.
 *
 * When interrupts are disabled, use this function to wait for a particular
 * ISP mode to complete.
 */
enum sh_css_err
sh_css_wait_for_completion(enum sh_css_pipe_id pipe_id);

/** @brief Set the current input resolution.
 *
 * @param[in]	width	Input resolution width in pixels.
 * @param[in]	height	Input resolution height in pixels.
 *
 * Set the current input resolution. This needs to be called every time the
 * sensor resolution changes.
 */
enum sh_css_err
sh_css_input_set_resolution(unsigned int width, unsigned int height);

/** @brief Set the effective input resolution.
 *
 * @param[in]	width	Effective input resolution width in pixels.
 * @param[in]	height	Effective input resolution height in pixels.
 *
 * Set the part of the input resolution that will be the input to the ISP.
 * The difference between the input resolution and effective input resolution
 * will be cropped off. When the effective input resolution is exceeds the
 * output resolution, the ISP will downscale the input to the output resolution
 * in the domain.
 * Note that the effective input resolution cannot be smaller than the output
 * resolution.
 */
enum sh_css_err
sh_css_input_set_effective_resolution(unsigned int width, unsigned int height);

/** @brief Set the input format.
 *
 * @param[in]	format	The new input format.
 *
 * Specify the format of the input data. This format is used for all input
 * sources except memory (mipi receiver, prbs, tpg, fifo).
 */
void
sh_css_input_set_format(enum sh_css_input_format format);

/** @brief Get the input format.
 *
 * @param[out]	format	Pointer to hold the current input format.
 *
 * Return the last set format of the input data.
 */
void
sh_css_input_get_format(enum sh_css_input_format *format);

/** @brief Set the binning factor.
 *
 * @param[in]	binning_factor	The binning factor.
 *
 * Set the current binning factor of the sensor.
 */
void
sh_css_input_set_binning_factor(unsigned int binning_factor);

/** @brief Translate format and compression to format type.
 *
 * @param[in]	input_format	The input format.
 * @param[in]	compression	The compression scheme.
 * @param[out]	fmt_type	Pointer to the resulting format type.
 * @return			Error code.
 *
 * Translate an input format and mipi compression pair to the fmt_type.
 * This is normally done by the sensor, but when using the input fifo, this
 * format type must be sumitted correctly by the application.
 */
enum sh_css_err sh_css_input_format_type(
	enum sh_css_input_format input_format,
	mipi_predictor_t	compression,
	unsigned int		*fmt_type);

/** @brief Enable/disable two pixels per clock.
 *.
 * @param[in]	two_pixels_per_clock	Enable/disable value.
 *
 * Specify that the input will be sent as 2 pixels per clock.
 * The default is one pixel per clock.
 */
void
sh_css_input_set_two_pixels_per_clock(bool two_pixels_per_clock);

/** @brief Get the two pixels per clock value.
 *
 * @param[out]	two_pixels_per_clock	Two pixels per clock value.
 *
 * Return the last set "2 pixels per clock" setting
 */
void
sh_css_input_get_two_pixels_per_clock(bool *two_pixels_per_clock);

/** @brief Set the bayer order of the input.
 *
 * @param[in]	bayer_order	The bayer order of the input data.
 *
 * Specify the bayer order of the input. The default is grbg.
*/
void
sh_css_input_set_bayer_order(enum sh_css_bayer_order bayer_order);

/** @brief Get the extra pixels for the sensor driver.
 *
 * @param[out]	extra_rows	The horizontal extra pixels.
 * @param[out]	extra_columns	The vertical extra pixels.
 *
 * Get the number of extra rows and columns needed to program the
 * sensor driver with the correct resolution.
 * This is dependent upon the bayer order which is assumed to have
 * been already set using the API ::sh_css_input_set_bayer_order
 */
void
sh_css_get_extra_pixels_count(int *extra_rows, int *extra_columns);

/** @brief Set the input channel id.
 *
 * @param[in]	channel_id	The input channel id.
 *
 * Specify the channel on which the input will come into the CSS receiver. Each
 sensor can submit its output data on
 * several channels. The channel used by the sensor must correspond to the
 channel set here, otherwise the input data
 * will be discarded by the CSS.
 */
void
sh_css_input_set_channel(unsigned int channel_id);

/** @brief Set the input mode.
 *
 * @param[in]	mode	The input mode.
 *
 * Set the input mode to be used.
 *
 * @return None
 */
void
sh_css_input_set_mode(enum sh_css_input_mode mode);

/** @brief Set the left padding.
 *
 * @param[in]	padding	The amount of left padding (pixels).
 *
 * Set the left padding to 2*NWAY-padding.
 *
 * @return None
 */
void
sh_css_input_set_left_padding(unsigned int padding);

/** @brief Configure the CSI-2 receiver.
 *
 * @param[in]	port		port ID.
 * @param[in]	num_lanes	Number of lanes.
 * @param[in]	timeout		Timeout value.
 *
 * Configure the MIPI receiver:
 *  The num_lanes argument is only valid for the 4lane port. It specifies
 *  how many of these 4 lanes are in use. Valid values are 1, 2, 3 or 4.
 *  The timeout argument specifies the timeout after which a timeout interrupt
 *  is generated.
 *  The timeout is specified in terms of <TO BE CLARIFIED>.
 *
 * This interface is deprecated, it is not portable -> move to input system API
 */
enum sh_css_err sh_css_input_configure_port(
	const mipi_port_ID_t	port,
	const unsigned int		num_lanes,
	const unsigned int		timeout);

/** @brief Configure the CSI-2 compression scheme.
 *
 * @param[in]	comp				The predictor type.
 * @param[in]	compressed_bits_per_pixel	Number of compressed bits.
 * @param[in]	uncompressed_bits_per_pixel	Number of uncompressed bits.
 *
 * Specify the number of bits per compressed and uncompressed pixel and
 * the compression predictor mode.
 *
 * This interface is deprecated, it is not portable -> move to input system API
 *
 * @return Error code: indicates success or error.
 */
enum sh_css_err sh_css_input_set_compression(
	const mipi_predictor_t	comp,
	const unsigned int		compressed_bits_per_pixel,
	const unsigned int		uncompressed_bits_per_pixel);

/** @brief Configure the test pattern generator.
 *
 * @param[in]	x_mask				See code-snippet.
 * @param[in]	x_delta				See code-snippet.
 * @param[in]	y_mask				See code-snippet.
 * @param[in]	y_delta				See code-snippet.
 * @param[in]	xy_mask				See code-snippet.
 *
 * Configure the Test Pattern Generator, the way these values are used to
 * generate the pattern can be seen in the HRT extension for the test pattern
 * generator:
 * devices/test_pat_gen/hrt/include/test_pat_gen.h: hrt_calc_tpg_data().
 *
 * This interface is deprecated, it is not portable -> move to input system API
 *
@code
unsigned int test_pattern_value(unsigned int x, unsigned int y)
{
 unsigned int x_val, y_val;
 if (x_delta > 0) (x_val = (x << x_delta) & x_mask;
 else (x_val = (x >> -x_delta) & x_mask;
 if (y_delta > 0) (y_val = (y << y_delta) & y_mask;
 else (y_val = (y >> -y_delta) & x_mask;
 return (x_val + y_val) & xy_mask;
}
@endcode
 */
void
sh_css_tpg_configure(unsigned int x_mask, int x_delta,
		     unsigned int y_mask, int y_delta, unsigned int xy_mask);

/** @brief Set the PRBS seed.
 *
 * @param[in]	seed	The random generator seed.
 *
 * Seed the for the Pseudo Random Bit Sequence.
 *
 * This interface is deprecated, it is not portable -> move to input system API
 */
void
sh_css_prbs_set_seed(int seed);

void
sh_css_set_shading_table(const struct sh_css_shading_table *table);

/* ===== FRAMES ===== */

/** @brief Fill a frame with zeros
 *
 * @param	frame		The frame.
 *
 * Fill a frame with pixel values of zero
 */
extern void sh_css_frame_zero(struct sh_css_frame *frame);

/** @brief Allocate a CSS frame structure
 *
 * @param	frame		The allocated frame.
 * @param	width		The width (in pixels) of the frame.
 * @param	height		The height (in lines) of the frame.
 * @param	format		The frame format.
 * @param	stride		The padded stride, in pixels.
 * @param	raw_bit_depth	The raw bit depth, in bits.
 * @return			The error code.
 *
 * Allocate a CSS frame structure. The memory for the frame data will be
 * allocated in the CSS address space.
 */
enum sh_css_err
sh_css_frame_allocate(struct sh_css_frame **frame,
		      unsigned int width,
		      unsigned int height,
		      enum sh_css_frame_format format,
		      unsigned int stride,
		      unsigned int raw_bit_depth);

/** @brief Allocate a CSS frame structure using a frame info structure.
 *
 * @param	frame	The allocated frame.
 * @param[in]	info	The frame info structure.
 * @return		The error code.
 *
 * Allocate a frame using the resolution and format from a frame info struct.
 * This is a convenience function, implemented on top of
 * ia_css_frame_allocate().
 */
enum sh_css_err
sh_css_frame_allocate_from_info(struct sh_css_frame **frame,
				const struct sh_css_frame_info *info);

/** @brief Free a CSS frame structure.
 *
 * @param[in]	frame	Pointer to the frame.
 *
 * Free a CSS frame structure. This will free both the frame structure
 * and the pixel data pointer contained within the frame structure.
 */
void
sh_css_frame_free(struct sh_css_frame *frame);

/* ===== FPGA display frames ====== */

/** @brief Allocate a contiguous CSS frame structure
 *
 * @param	frame		The allocated frame.
 * @param	width		The width (in pixels) of the frame.
 * @param	height		The height (in lines) of the frame.
 * @param	format		The frame format.
 * @param	stride		The padded stride, in pixels.
 * @param	raw_bit_depth	The raw bit depth, in bits.
 * @return			The error code.
 *
 * Contiguous frame allocation, only for FPGA display driver which needs
 * physically contiguous memory.
 */
enum sh_css_err
sh_css_frame_allocate_contiguous(struct sh_css_frame **frame,
				 unsigned int width,
				 unsigned int height,
				 enum sh_css_frame_format format,
				 unsigned int stride,
				 unsigned int raw_bit_depth);

/** @brief Allocate a contiguous CSS frame from a frame info structure.
 *
 * @param	frame	The allocated frame.
 * @param[in]	info	The frame info structure.
 * @return		The error code.
 *
 * Allocate a frame using the resolution and format from a frame info struct.
 * This is a convenience function, implemented on top of
 * ia_css_frame_allocate_contiguous().
 * Only for FPGA display driver which needs physically contiguous memory.
 */
enum sh_css_err
sh_css_frame_allocate_contiguous_from_info(struct sh_css_frame **frame,
					  const struct sh_css_frame_info *info);

/** @brief Map an existing frame data pointer to a CSS frame.
 *
 * @param[in]	info		The frame info.
 * @param[in]	data		Pointer to the allocated frame data.
 * @param[in]	attribute	Attributes to be passed to mmgr_mmap.
 * @param[in]	context		Pointer to the a context to be passed to mmgr_mmap.
 * @return			The allocated frame structure.
 *
 * This function maps a pre-allocated pointer into a CSS frame. This can be
 * used when an upper software layer is responsible for allocating the frame
 * data and it wants to share that frame pointer with the CSS code.
 * This function will fill the CSS frame structure just like
 * ia_css_frame_allocate() does, but instead of allocating the memory, it will
 * map the pre-allocated memory into the CSS address space.
 */
enum sh_css_err
sh_css_frame_map(struct sh_css_frame **frame,
                 const struct sh_css_frame_info *info,
                 const void *data,
                 uint16_t attribute,
                 void *context);

/** @brief Unmap a CSS frame structure.
 *
 * @param[in]	frame	Pointer to the CSS frame.
 *
 * This function unmaps the frame data pointer within a CSS frame and
 * then frees the CSS frame structure. Use this for frame pointers created
 * using ia_css_frame_map().
 */
void
sh_css_frame_unmap(struct sh_css_frame *frame);

#if 0
/* ===== PREVIEW ===== */

/** @brief Start preview mode.
 *
 * @param[in]	raw_out_frame	The optional raw input frame.
 * @param[out]	out_frame	The preview output frame.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Start the ISP in preview mode, this will run the preview ISP on one frame.
 * After this has completed, it needs to be started again for the next frame.
 */
 enum sh_css_err
sh_css_preview_start(struct sh_css_frame *raw_out_frame,
		     struct sh_css_frame *out_frame,
			 union sh_css_s3a_data *s3a_data,
			 struct sh_css_dis_data *sdis_data);
#endif

/** @brief Stop the preview pipe
 *
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * This function stops the preview pipe that's running. Note that any
 * dependent pipes will also be stopped automatically since otherwise
 * they would starve because they no longer receive input data.
 */
enum sh_css_err
sh_css_preview_stop(void);

/** @brief Enable online mode for preview.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable online binaries if available. Default is enabled.
 */
void
sh_css_preview_enable_online(bool enable);


/** @brief Enable/disable online mode for video.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable online mode. Default is enabled.
 */
void
sh_css_video_enable_online(bool enable);

/** @brief Enable yuv ds mode for video.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable yuv ds mode. Default is disabled.
 */
void
sh_css_video_enable_yuv_ds(bool enable);

/** @brief Enable high speed mode for video.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable high speed mode. Default is disabled.
 */
void
sh_css_video_enable_high_speed(bool enable);

/** @brief Enable 6-axis DVS mode for video.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable 6-axis DVS mode. Default is disabled.
 */
void
sh_css_video_enable_dvs_6axis(bool enable);

/** @brief Enable reduced pipe for video.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable reduced pipe (version 2). Default is disabled.
 */
void
sh_css_video_enable_reduced_pipe(bool enable);

/** @brief Enable/disable viewfinder for video.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable viewfinder. Default is enabled.
 */
void
sh_css_video_enable_viewfinder(bool enable);

/** @brief Enable continuous mode.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable continuous binaries if available. Default is disabled.
 */
void
sh_css_enable_continuous(bool enable);

/** @brief Enable cont_capt mode (continuous preview+capture running together).
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable continuous binaries if available. Default is disabled.
 */
void
sh_css_enable_cont_capt(bool enable, bool stop_copy_preview);

/** @brief Return whether continuous mode is enabled.
 *
 * @return	Enabling value.
 *
 * Return whether continuous binaries are enabled.
 */
bool
sh_css_continuous_is_enabled(void);

/** @brief Return max nr of continuous RAW frames.
 *
 * @return	Max nr of continuous RAW frames.
 *
 * Return the maximum nr of continuous RAW frames the system can support.
 */
int
sh_css_continuous_get_max_raw_frames(void);

/** @brief Set nr of continuous RAW frames to use.
 *
 * @param 	num_frames	Number of frames.
 * @return	SH_CSS_SUCCESS or error code upon error.
 *
 * Set the number of continuous frames to use during continuous modes.
 */
enum sh_css_err
sh_css_continuous_set_num_raw_frames(int num_frames);

/** @brief Get nr of continuous RAW frames to use.
 *
 * @return 	Number of frames to use.
 *
 * Get the currently set number of continuous frames
 * to use during continuous modes.
 */
int
sh_css_continuous_get_num_raw_frames(void);

/** @brief Enable raw binning mode.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable raw binning if available. Default is disabled.
 */
void
sh_css_enable_raw_binning(bool enable);

/** @brief Disable vf_pp.
 *
 * @param	disable	Disabling value.
 *
 * Disable vf_pp: used to replace by dynamic binary for testing purposes.
 *		  OR
 *		  to disable vf_pp. E.g. in case vf_pp processing does not
 *		  fit in the vblank interval and capture frame will be used
 *		  as vf frame (with some optional external post processing)
 */
void
sh_css_disable_vf_pp(bool disable);

/** @brief Disable capture_pp.
 *
 * @param	disable	Disabling value.
 *
 * Disable capture_pp: used to replace by dynamic binary for testing purposes.
 */
void
sh_css_disable_capture_pp(bool disable);

/** @brief Specify the preview output resolution.
 *
 * @param	width	The output width.
 * @param	height	The output height.
 * @param	format	The output format.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Specify the output resolution to be used by the preview ISP.
 */
enum sh_css_err
sh_css_preview_configure_output(unsigned int width,
				unsigned int height,
				unsigned int min_padded_width,
				enum sh_css_frame_format format);

/** @brief Get the preview output information
 *
 * @param[out]	info	Pointer to the output information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Get the information about the output frames, this contains the resolution
 * and the stride. To allocate frames, use the information returned here.
 */
enum sh_css_err
sh_css_preview_get_output_frame_info(struct sh_css_frame_info *info);

/** @brief Get the preview grid information
 *
 * @param[out]	info	Pointer to the grid information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Get the information about the preview grid.
 */
enum sh_css_err
sh_css_preview_get_grid_info(struct sh_css_grid_info *info);

/** @brief Get the preview input resolution.
 *
 * @param[out]	width	Pointer to the input width.
 * @param[out]	height	Pointer to the input height.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Get the resolution for the preview input frames.
 */
enum sh_css_err
sh_css_preview_get_input_resolution(unsigned int *width,
				    unsigned int *height);

/** @brief Specify the vf_pp input resolution.
 *
 * @param	width	The output width.
 * @param	height	The output height.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Set the input resolution for viewfinder post processing.
 * When this is not set, the input resolution is equal to the input
 * resolution of the preview pipeline. When this is set, the YUV scaler
 * in the viewfinder post processing step will be activated.
 */
enum sh_css_err
sh_css_preview_configure_pp_input(unsigned int width, unsigned int height);

#if 0
/* ===== CAPTURE ===== */

/** @brief Start capture mode.
 *
 * @param[in]	raw_out_frame	The optional raw input frame.
 * @param[out]	out_frame	The capture output frame.
 * @param[out]	vf_frame	The capture viewfinder frame.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Start the ISP in capture mode, this will run a capture ISP on one frame.
 * After this has completed, it needs to be started again for the next frame.
 */
enum sh_css_err
sh_css_capture_start(struct sh_css_frame *raw_out_frame,
		     struct sh_css_frame *out_frame,
		     struct sh_css_frame *vf_frame,
			 union sh_css_s3a_data *s3a_data,
			 struct sh_css_dis_data *sdis_data);
#endif

/** @brief Stop the capture pipe
 *
 * @return		IA_CSS_SUCCESS or error code upon error.
 *
 * This function stops the capture pipe that's running. Note that any
 * dependent pipes will also be stopped automatically since otherwise
 * they would starve because they no longer receive input data.
 */
enum sh_css_err
sh_css_capture_stop(void);

/** @brief Configure the continuous capture
 *
 * @param	num_captures	The number of RAW frames to be processed to
 *                              YUV. Setting this to -1 will make continuous
 *                              capture run until it is stopped.
 *                              This number will also be used to allocate RAW
 *                              buffers. To allow the viewfinder to also
 *                              keep operating, 2 extra buffers will always be
 *                              allocated.
 *                              If the offset is negative and the skip setting
 *                              is greater than 0, additional buffers may be
 *                              needed.
 * @param	skip		Skip N frames in between captures. This can be
 *                              used to select a slower capture frame rate than
 *                              the sensor output frame rate.
 * @param	offset		Start the RAW-to-YUV processing at RAW buffer
 *                              with this offset. This allows the user to
 *                              process RAW frames that were captured in the
 *                              past or future.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 *  For example, to capture the current frame plus the 2 previous
 *  frames and 2 subsequent frames, you would call
 *  sh_css_offline_capture_configure(5, 0, -2).
 */
enum sh_css_err
sh_css_offline_capture_configure(int num_captures,
				 unsigned int skip,
				 int offset);

/** @brief Specify which raw frame to tag based on exp_id found in frame info
 *
 * @param	exp_id	The exposure id of the raw frame to tag.
 *
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * This function allows the user to tag a raw frame based on the exposure id
 * found in the viewfinder frames' frame info.
 */
enum sh_css_err
sh_css_offline_capture_tag_frame(unsigned int exp_id);

/** @brief Specify the mode used for capturing.
 *
 * @param	mode	The capture mode.
 *
 * Specify the mode used for capturing.
 */
void
sh_css_capture_set_mode(enum sh_css_capture_mode mode);

/** @brief Enable or disable XNR.
 *
 * @param	enable	Enabling value.
 *
 * Enable the eXtra Noise Reduction as a post processing step. This will be
 * run on both the captured output and the viewfinder output.
 */
void
sh_css_capture_enable_xnr(bool enable);

/** @brief Specify the capture output resolution.
 *
 * @param	width	The output width.
 * @param	height	The output height.
 * @param	format	The output format.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Specify the output resolution for captured images.
 */
enum sh_css_err
sh_css_capture_configure_output(unsigned int width,
				unsigned int height,
				unsigned int min_padded_width,
				enum sh_css_frame_format format);

/** @brief Specify the capture viewfinder resolution.
 *
 * @param	width	The viewfinder width.
 * @param	height	The viewfinder height.
 * @param	format	The viewfinder format.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Specify the viewfinder resolution. Note that this resolution currently
 * has to be a division of the captured output by a power of 2. The API will
 * automatically select the resolution that's closest to the one requested
 * here.
 */
enum sh_css_err
sh_css_capture_configure_viewfinder(unsigned int width,
				    unsigned int height,
				    unsigned int min_padded_width,
				    enum sh_css_frame_format format);

/** @brief Specify the capture post processing input resolution.
 *
 * @param	width	The width.
 * @param	height	The height.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * For non-raw still captures, downscaling in the YUV domain can be done
 * during post processing. This function specifies the input resolution
 * for the YUV downscaling step. If this resolution is not set, the YUV
 * downscaling will not be done. The output resolution of the YUV
 * downscaling is taken from the configure_output function above.
 */
enum sh_css_err
sh_css_capture_configure_pp_input(unsigned int width,
				  unsigned int height);

/** @brief Enable online mode for capture.
 *
 * @param	enable	Enabling value.
 *
 * Enable or disable online binaries if available. Default is enabled.
 */
void
sh_css_capture_enable_online(bool enable);

/** @brief Get the capture output information
 *
 * @param[out]	info	Pointer to the output information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Retrieve the format and resolution of the output frames. Note that this
 * can differ from the requested resolution.
 */
enum sh_css_err
sh_css_capture_get_output_frame_info(struct sh_css_frame_info *info);

/** @brief Get the capture viewfinder information
 *
 * @param[out]	info	Pointer to the viewfinder information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Retrieve the format and resolution of the viewfinder frames. Note that this
 * can differ from the requested resolution.
 */
enum sh_css_err
sh_css_capture_get_viewfinder_frame_info(struct sh_css_frame_info *info);

/** @brief Get the capture raw output information
 *
 * @param[out]	info	Pointer to the raw information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Retrieve the format and resolution of the RAW frame. This is only available
 * when the capture is set to offline or continuous using:
 * sh_css_capture_enable_online(false) or
 * sh_css_enable_continuous(true).
 */
enum sh_css_err
sh_css_capture_get_output_raw_frame_info(struct sh_css_frame_info *info);

/** @brief Get the capture grid information
 *
 * @param[out]	info	Pointer to the grid information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Get the information about the capture grid.
 */
enum sh_css_err
sh_css_capture_get_grid_info(struct sh_css_grid_info *info);

/** @brief Get the capture input resolution.
 *
 * @param[out]	width	Pointer to the input width.
 * @param[out]	height	Pointer to the input height.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Get the resolution for the capture input frames.
 */
enum sh_css_err
sh_css_capture_get_input_resolution(unsigned int *width,
				    unsigned int *height);

#if 0
/* ===== VIDEO ===== */

/** @brief Start video mode.
 *
 * @param[in]	in_frame	The optional raw input frame.
 * @param[in]	out_frame	The video output frame.
 * @param[in]	vf_frame	The video viewfinder frame.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Start the ISP in video mode, this will run a video ISP on one frame.
 * After this has completed, it needs to be started again for the next frame.
 * The input frame, this argument is only used if the input_mode is set to
 * sh_css_input_mode_memory.
 */
enum sh_css_err
sh_css_video_start(struct sh_css_frame *in_frame,
		   struct sh_css_frame *out_frame,
		   struct sh_css_frame *vf_frame,
		   union sh_css_s3a_data *s3a_data,
		   struct sh_css_dis_data *sdis_data);
#endif
/** @brief Stop the video pipe
 *
 * @return		IA_CSS_SUCCESS or error code upon error.
 *
 * This function stops the video pipe that's running. Note that any
 * dependent pipes will also be stopped automatically since otherwise
 * they would starve because they no longer receive input data.
 */
enum sh_css_err
sh_css_video_stop(void);

/** @brief Specify the video output resolution.
 *
 * @param	width	The output width.
 * @param	height	The output height.
 * @param	format	The output format.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Specify the output resolution for video images.
 */
enum sh_css_err
sh_css_video_configure_output(unsigned int width,
			      unsigned int height,
			      unsigned int min_padded_width,
			      enum sh_css_frame_format format);

/** @brief Specify the video viewfinder resolution.
 *
 * @param	width	The viewfinder width.
 * @param	height	The viewfinder height.
 * @param	format	The viewfinder format.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Specify the viewfinder resolution. Note that this resolution currently
 * has to be a division of the video output by a power of 2. The API will
 * automatically select the resolution that's closest to the one requested
 * here.
 */
enum sh_css_err
sh_css_video_configure_viewfinder(unsigned int width,
				  unsigned int height,
				  unsigned int min_padded_width,
				  enum sh_css_frame_format format);

/** @brief Set the envelope for DVS.
 *
 * @param	width	Envelope width.
 * @param	height	Envelope height.
 *
 * Specify the envelope to be used for DVS.
 */
void
sh_css_video_set_dis_envelope(unsigned int width, unsigned int height);

/** @brief Retrieve the envelope for DVS.
 *
 * @param[out]	width	Envelope width.
 * @param[out]	height	Envelope height.
 *
 * Retrieve the envelope to be used for DVS.
 */
void
sh_css_video_get_dis_envelope(unsigned int *width, unsigned int *height);

/** @brief Set the flag for using DZ.
 *
 * @param[in]	enable_dz	Enable digital zoom.
 *
 * Set the flag for using digital zoom for video. By default this is enabled.
 */
void
sh_css_video_set_enable_dz(bool enable_dz);

/** @brief Retrieve the flag for using DZ.
 *
 * @param[out]	enable_dz	Digital zoom enabled.
 *
 * Retrieve the flag for using digital zoom for video. By default it is enabled.
 */
void
sh_css_video_get_enable_dz(bool *enable_dz);

/** @brief Get the video output information
 *
 * @param[out]	info	Pointer to the output information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Retrieve the format and resolution of the output frames. Note that this
 * can differ from the requested resolution.
 */
enum sh_css_err
sh_css_video_get_output_frame_info(struct sh_css_frame_info *info);

/** @brief Get the video raw output information
 *
 * @param[out]	info	Pointer to the output information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Retrieve the format and resolution of the raw output frame. Note that this
 * can differ from the requested resolution.
 */
enum sh_css_err
sh_css_video_get_output_raw_frame_info(struct sh_css_frame_info *info);

/** @brief Get the video viewfinder information
 *
 * @param[out]	info	Pointer to the viewfinder information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Retrieve the format and resolution of the viewfinder frames. Note that this
 * can differ from the requested resolution.
 */
enum sh_css_err
sh_css_video_get_viewfinder_frame_info(struct sh_css_frame_info *info);

/** @brief Get the video grid information
 *
 * @param[out]	info	Pointer to the grid information.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Get the information about the video grid.
 */
enum sh_css_err
sh_css_video_get_grid_info(struct sh_css_grid_info *info);

/** @brief Get the video input resolution.
 *
 * @param[out]	width	Pointer to the input width.
 * @param[out]	height	Pointer to the input height.
 * @return		SH_CSS_SUCCESS or error code upon error.
 *
 * Get the resolution for the video input frames.
 */
enum sh_css_err
sh_css_video_get_input_resolution(unsigned int *width,
				  unsigned int *height);

/** @brief Send streaming data into the css input FIFO
 *
 * @param	data	Pointer to the pixels to be send.
 * @param	width	Width of the input frame.
 * @param	height	Height of the input frame.
 *
 * Send streaming data into the css input FIFO. This is for testing purposes
 * only. This uses the channel ID and input format as set by the user with
 * the regular functions for this.
 * This function blocks until the entire frame has been written into the
 * input FIFO.
 */
void
sh_css_send_input_frame(unsigned short *data,
			unsigned int width,
			unsigned int height);

/*
 * For higher flexibility the sh_css_send_input_frame is replaced by
 * three seperate functions:
 * 1) sh_css_streaming_to_mipi_start_frame
 * 2) sh_css_streaming_to_mipi_send_line
 * 3) sh_css_streaming_to_mipi_end_frame
 * In this way it is possible to stream multiple frames on different
 * channel ID's on a line basis. It will be possible to simulate
 * line-interleaved Stereo 3D muxed on 1 mipi port.
 * These 3 functions are for testing purpose only and can be used in
 * conjunction with sh_css_send_input_frame
 */

/** @brief Start an input frame on the CSS input FIFO.
 *
 * @param[in]	channel_id		The channel id.
 * @param[in]	input_format		The input format.
 * @param[in]	two_pixels_per_clock	Use 2 pixels per clock.
 *
 * Starts the streaming to mipi frame by sending SoF for channel channel_id.
 * It will use the input_format and two_pixels_per_clock as provided by
 * the user.
 * For the "correct" use-case, input_format and two_pixels_per_clock must match
 * with the values as set by the user with the regular functions.
 * To simulate an error, the user can provide "incorrect" values for
 * input_format and/or two_pixels_per_clock.
 */
void
sh_css_streaming_to_mipi_start_frame(unsigned int channel_id,
				enum sh_css_input_format input_format,
				bool two_pixels_per_clock);


/** @brief Send a line of input data into the CSS input FIFO.
 *
 * @param[in]	channel_id		The channel id.
 * @param[in]	data	Array of the first line of image data.
 * @param	width	The width (in pixels) of the first line.
 * @param[in]	data2	Array of the second line of image data.
 * @param	width2	The width (in pixels) of the second line.
 *
 * Sends 1 frame line. Start with SoL followed by width bytes of data, followed
 * by width2 bytes of data2 and followed by and EoL
 * It will use the input_format and two_pixels_per_clock settings as provided
 * with the sh_css_streaming_to_mipi_start_frame function call.
 *
 * This function blocks until the entire line has been written into the
 * input FIFO.
 */
void
sh_css_streaming_to_mipi_send_line(unsigned int channel_id,
						unsigned short *data,
						unsigned int width,
						unsigned short *data2,
						unsigned int width2);


/** @brief End an input frame on the CSS input FIFO.
 *
 * @param[in]	channel_id	The channel id.
 *
 * Send the end-of-frame signal into the CSS input FIFO.
 */
void
sh_css_streaming_to_mipi_end_frame(unsigned int channel_id);

/** @brief Test whether the ISP has started.
 *
 * @return	The ISP has started.
 *
 * Temporary function to poll whether the ISP has been started. Once it has,
 * the sensor can also be started. */
bool
sh_css_isp_has_started(void);

/** @brief Test whether the SP has initialized.
 *
 * @return	The SP has initialized.
 *
 * Temporary function to poll whether the SP has been initilized. Once it has,
 * we can enqueue buffers. */
bool
sh_css_sp_has_booted(void);

/** @brief Test whether the SP has terminated.
 *
 * @return	The SP has terminated.
 *
 * Temporary function to poll whether the SP has been terminated. Once it has,
 * we can switch mode. */
bool
sh_css_sp_has_terminated(void);

/** @brief Load firmware for acceleration.
 *
 * @param	firmware	Firmware to be loaded.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Load firmware for acceleration.
 */
enum sh_css_err
sh_css_load_acceleration(struct sh_css_acc_fw *firmware);

/** @brief Unload firmware for acceleration.
 *
 * @param	firmware	Firmware to be unloaded.
 *
 * Unload firmware for acceleration.
 */
void
sh_css_unload_acceleration(struct sh_css_acc_fw *firmware);

/** @brief Load firmware for extension.
 *
 * @param	firmware	Firmware to be loaded.
 * @param	pipe_id		Pipeline type to be loaded on.
 * @param	acc_type	Output to be conneted to.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Load firmware for extension to a designated pipeline
 */
extern enum sh_css_err sh_css_load_extension(
	struct sh_css_fw_info	*fw,
	enum sh_css_pipe_id		pipe_id,
	enum sh_css_acc_type	acc_type);


/** @brief Unload firmware for extension.
 *
 * @param	firmware	Firmware to be unloaded.
 * @param	pipe_id		Pipeline type to be unloaded from.
 *
 * Unload firmware for extension from a designated pipeline.
 */
extern enum sh_css_err sh_css_unload_extension(
	struct sh_css_fw_info *fw,
	enum sh_css_pipe_id		pipe_id);

/** @brief Set parameter for acceleration.
 *
 * @param	firmware	Firmware of acceleration.
 * @param	val		Parameter value.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Set acceleration parameter to value <val>.
 * The parameter value is an isp pointer, i.e. allocated in DDR and mapped
 * to the CSS virtual address space.
 */
enum sh_css_err
sh_css_set_acceleration_parameter(struct sh_css_acc_fw *firmware,
				  hrt_vaddress val, size_t size);

/** @brief Set isp dmem parameters for acceleration.
 *
 * @param	firmware	Firmware of acceleration.
 * @param	val		Parameter value.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Set acceleration parameter to value <val>.
 * The parameter value is an isp pointer, i.e. allocated in DDR and mapped
 * to the CSS virtual address space.
 */
enum sh_css_err
sh_css_set_firmware_dmem_parameters(struct sh_css_fw_info *firmware,
				    enum sh_css_isp_memories mem,
				    hrt_vaddress val, size_t size);

/** @brief Start acceleration.
 *
 * @param	firmware	Firmware of acceleration.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Start acceleration of firmware.
 * Load the firmware if not yet loaded.
 */
enum sh_css_err
sh_css_start_acceleration(struct sh_css_acc_fw *firmware);

/** @brief Signal termination of acceleration.
 *
 * @param	firmware	Firmware of acceleration.
 *
 * To be called when acceleration has terminated.
 */
void
sh_css_acceleration_done(struct sh_css_acc_fw *firmware);

/** @brief Abort current acceleration.
 *
 * @param	firmware	Firmware of acceleration.
 * @param	deadline	Deadline in microseconds.
 *
 * Abort acceleration within <deadline> microseconds
 */
void
sh_css_abort_acceleration(struct sh_css_acc_fw *firmware, unsigned deadline);

/** @brief Stop the acceleration pipe
 *
 * @return	       IA_CSS_SUCCESS or error code upon error.
 *
 * This function stops the acceleration pipe that's running. Note that any
 * dependent pipes will also be stopped automatically since otherwise
 * they would starve because they no longer receive input data.
 */
enum sh_css_err
sh_css_acceleration_stop(void);

/** @brief Append a stage to pipeline.
 *
 * @param	pipeline	Pointer to the pipeline to be extended.
 * @param[in]	isp_fw		ISP firmware of new stage.
 * @param[in]	in		The input frame to the stage.
 * @param[in]	out		The output frame of the stage.
 * @param[in]	vf		The viewfinder frame of the stage.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Append a new stage to *pipeline. When *pipeline is NULL, it will be created.
 * The stage consists of an ISP binary <isp_fw> and input and output arguments.
*/
enum sh_css_err
sh_css_append_stage(void **pipeline,
		    const char *isp_fw,
		    struct sh_css_frame *in,
		    struct sh_css_frame *out,
		    struct sh_css_frame *vf);

/** @brief Create an empty pipeline.
 */
extern void *sh_css_create_pipeline(void);

/** @brief Add an accelerator stage to a pipeline.
 *
 * @param	pipeline	The pipeline to be appended to.
 * @param	acc_fw		The fw descriptor of the new stage
 */
extern enum sh_css_err sh_css_pipeline_add_acc_stage(
	void		*pipeline,
	const void	*acc_fw);

/** @brief Destroy a pipeline.
 *
 * @param	pipeline	The pipeline to be destroyed.
 *
 * Recursively destroys the pipeline stages as well
 */
extern void sh_css_destroy_pipeline(
	void		*pipeline);

/** @brief Start a pipeline.
 *
 * @param	pipe_id		The pipe id where to run the pipeline. (Huh ?)
 * @param	pipeline	The pipeline to be executed.
 *
 * Start a pipeline, does not wait until the pipeline completes.
 */
void
sh_css_start_pipeline(enum sh_css_pipe_id pipe_id, void *pipeline);

/** @brief Close a pipeline.
 *
 * @param	pipeline	The pipeline to be closed.
 *
 * Close a pipeline and free all memory allocated to it.
 */
void
sh_css_close_pipeline(void *pipeline);

/** @brief Run a single stage pipeline.
 *
 * @param[in]	isp_fw	ISP firmware of new pipeline.
 * @param[in]	in	The input frame to the pipeline.
 * @param[in]	out	The output frame of the pipeline.
 * @param[in]	vf	The viewfinder frame of the pipeline.
 * @return		IA_CSS_SUCCESS or error code upon error.
 *
 * Run an isp binary <isp_fw> with input, output and vf frames.
 * This creates a temporary single stage pipeline.
 */
enum sh_css_err
sh_css_run_isp_firmware(const char *isp_fw,
			struct sh_css_frame *in,
			struct sh_css_frame *out,
			struct sh_css_frame *vf);

/** @brief Initialize the buffer queues in SP dmem
 *
 * This function needs to be called after sh_css_start() and before
 * sh_css_queue_buffers()
 */

void
sh_css_init_buffer_queues(void);

/** @brief send a invalidate mmu tlb command to SP
 *
 * As we allocate buffers in hmm space at runtime, we need to force updating
 * the TLB inside mmu by invalidating it atleast after each allocation. This
 * function will be called in the hmm library of driver.
 */

void
sh_css_enable_sp_invalidate_tlb(void);

/** @brief send a request flash command to SP
 *
 * Driver needs to call this function to send a flash request command
 * to SP, SP will be responsible for switching on/off the flash at proper
 * time. Due to the SP multi-threading environment, this request may have
 * one-frame delay, the driver needs to check the flashed flag in frame info
 * to determine which frame is being flashed.
 */
void
sh_css_request_flash(void);

/** @brief Set the isp pipe version for video.
 *
 * @param	version	Version of isp pipe (1 or 2).
 *
 * Set the version of isp pipe for video. Default is 1.
 */
void
sh_css_video_set_isp_pipe_version(unsigned int version);

/** @brief initialize host-sp control variables.
 *
 */
void
sh_css_init_host_sp_control_vars(void);

/** @brief allocate continuous raw frames for continuous capture
 *
 *  because this allocation takes a long time (around 120ms per frame),
 *  we separate the allocation part and update part to let driver call
 *  this function without locking. This function is the allocation part
 *  and next one is update part
 */
enum sh_css_err
sh_css_allocate_continuous_frames(
	bool init_time);

/** @brief allocate continuous raw frames for continuous capture
 *
 *  because this allocation takes a long time (around 120ms per frame),
 *  we separate the allocation part and update part to let driver call
 *  this function without locking. This function is the update part
 */
void
sh_css_update_continuous_frames(void);

void
sh_css_set_cont_prev_start_time(unsigned int overlap);

/* For convenience, so users only need to include sh_css.h
 * To be removed: the remaining sh_css_params functions should move to here.
 */
#include "sh_css_params.h"

#endif /* __HOST__ */

#endif /* _SH_CSS_H_ */
