/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
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

#ifndef __CSS_API_VERSION_H
#define __CSS_API_VERSION_H

/** @file
 * CSS API version file. This file contains the version number of the CSS-API.
 *
 * This file is generated from a set of input files describing the CSS-API
 * changes. Don't edit this file directly.
 */


/**

The version string has four dot-separated numbers, read left to right:
  The first two are the API version, and should not be changed.
  The third number is incremented by a CSS firmware developer when the
    API change is not backwards compatible.
  The fourth number is incremented by the a CSS firmware developer for
    every API change.
    It should be zeroed when the third number changes.

*/

#define CSS_API_VERSION_STRING	"2.1.9.1"

/*
Change log

v2.0.1.0, initial version:
- added API versioning

v2.0.1.1, activate CSS-API versioning:
- added description of major and minor version numbers

v2.0.1.2, modified struct ia_css_frame_info:
- added new member ia_css_crop_info

v2.0.1.3, added IA_CSS_ERR_NOT_SUPPORTED

v2.1.0.0
- moved version number to 2.1.0.0
- created new files for refactoring the code

v2.1.1.0, modified struct ia_css_pipe_config and struct ia_css_pipe_info and struct ia_css_pipe:
- use array to handle multiple output ports

v2.1.1.1
- added api to lock/unlock of RAW Buffers to Support HALv3 Feature

v2.1.1.2, modified struct ia_css_stream_config:
- to support multiple isys streams in one virtual channel, keep the old one for backward compatibility

v2.1.2.0, modify ia_css_stream_config:
- add isys_config and input_config to support multiple isys stream within one virtual channel

v2.1.2.1, add IA_CSS_STREAM_FORMAT_NUM
- add IA_CSS_STREAM_FORMAT_NUM definition to reflect the number of ia_css_stream_format enums

v2.1.2.2, modified enum ia_css_stream_format
- Add 16bit YUV formats to ia_css_stream_format enum:
- IA_CSS_STREAM_FORMAT_YUV420_16 (directly after IA_CSS_STREAM_FORMAT_YUV420_10)
- IA_CSS_STREAM_FORMAT_YUV422_16 (directly after IA_CSS_STREAM_FORMAT_YUV422_10)

v2.1.2.3
- added api to enable/disable digital zoom for capture pipe.

v2.1.2.4, change CSS API to generate the shading table which should be directly sent to ISP:
- keep the old CSS API (which uses the conversion of the shading table in CSS) for backward compatibility

v2.1.2.5
- Added SP frame time measurement (in ticks) and result is sent on a new member
- in ia_css_buffer.h.

v2.1.2.6, add function ia_css_check_firmware_version()
- the function ia_css_check_firmware_version() returns true when the firmware version matches and returns false otherwise.

v2.1.2.7
- rename dynamic_data_index to dynamic_queue_id in struct ia_css_frame.
- update IA_CSS_PIPE_MODE_NUM

v2.1.2.8
- added flag for video full range

v2.1.2.9
- add public parameters for xnr3 kernel

v2.1.2.10
- add new interface to enable output mirroring

v2.1.2.11, MIPI buffers optimization
- modified struct ia_css_mipi_buffer_config, added number of MIPI buffers needed for the stream
- backwards compatible, need another patch to remove legacy function and code

v2.1.2.12
- create consolidated  firmware package for 2400, 2401, csi2p, bxtpoc

v2.1.3.0
- rename ia_css_output_config.enable_mirror
- add new interface to enable vertical output flipping

v2.1.3.1
- deprecated ia_css_rx_get_irq_info and ia_css_rx_clear_irq_info because both are hardcoded to work on CSI port 1.
- added new functions ia_css_rx_port_get_irq_info and ia_css_rx_port_clear_irq_info, both have a port ID as extra argument.

v2.1.3.2
- reverted v2.1.3.0 change

v2.1.3.3
- Added isys event queue.
- Renamed ia_css_dequeue_event to ia_css_dequeue_psys_event
- Made ia_css_dequeue_event deprecated

v2.1.3.4
- added new interface to support ACC extension QoS feature.
- added IA_CSS_EVENT_TYPE_ACC_STAGE_COMPLETE.

v2.1.3.5
- added tiled frame format IA_CSS_FRAME_FORMAT_NV12_TILEY

v2.1.3.6
- added functions ia_css_host_data_allocate and ia_css_host_data_free

v2.1.4.0, default pipe config change
- disable enable_dz param by default

v2.1.5.0
- removed mix_range field from yuvp1_y_ee_nr_frng_public_config

v2.1.5.1, exposure IDs per stream
- added MIN/MAX exposure ID macros
- made exposure ID sequence per-stream instead of global (across all streams)

v2.1.6.0, Interface for vertical output flip
- add new interface to enable vertical output flipping
- rename ia_css_output_config.enable_mirror

v2.1.6.1, Effective res on pipe
- Added input_effective_res to struct ia_css_pipe_config in ia_css_pipe_public.h.

v2.1.6.2, CSS-API version file generated from individual changes
- Avoid merge-conflicts by generating version file from individual CSS-API changes.
- Parallel CSS-API changes can map to the same version number after this change.
- Version numbers for a change could increase due to parallel changes being merged.
- The version number would not decrease for a change.

v2.1.6.5 (2 changes parallel), Add SP FW error event
- Added FW error event. This gets raised when the SP FW runs into an
- error situation from which it cannot recover.

v2.1.6.5 (2 changes parallel), expose bnr FF enable bits in bnr public API
- Added ff enable bits to bnr_public_config_dn_detect_ctrl_config_t struct

v2.1.6.5 (2 changes parallel), ISP configuration per pipe 
- Added ISP configuration per pipe support: p_isp_config field in
- struct ia_css_pipe_config and ia_css_pipe_set_isp_config_on_pipe
- and ia_css_pipe_set_isp_config functions

v2.1.7.0, removed css_version.h
- Removed css_version.h that was used for versioning in manual (non-CI) releases.

v2.1.7.1, Add helpers (get and set) for ISP cfg per pipe
- Add helpers (get and set) for ISP configuration per pipe

v2.1.7.2, Add feature to lock all RAW buffers
- This API change adds a boolean flag (lock_all) in the stream_config struct.
- If this flag is set to true, then all frames will be locked if locking is
- enabled. By default this flag is set to false.
- When this flag is false, then only buffers that are sent to the preview pipe
- will be locked. If continuous viewfinder is disabled, the flag should be set
- to true.

v2.1.8.0 (2 changes parallel), Various changes to support ACC configuration per pipe
- Add ia_css_pipe_get_isp_config()
- Remove ia_css_pipe_set_isp_config_on_pipe (duplicated
- by ia_css_pipe_set_isp_config)
- Add isp configuration as parameter for
- ia_css_pipe_set_isp_config
- Remove ia_css_pipe_isp_config_set()
- Remove ia_css_pipe_isp_config_get()

v2.1.8.2 (2 changes parallel), Added member num_invalid_frames to ia_css_pipe_info structure.
- Added member num_invalid_frames to ia_css_pipe_info structure.
- This helps the driver make sure that the first valid output
- frame goes into the first user-supplied output buffer.

v2.1.8.4 (2 changes parallel), ISYS EOF timestamp for output buffers
- driver gets EOF timer to every out frame . ia_css_buffer modified to accomodate same.

v2.1.8.4 (4 changes parallel), display_config
- Added formats- and output config parameters for configuration of the (optional) display output.

v2.1.8.4 (2 changes parallel), Adding zoom region parameters to CSS API
- Adding ia_css_point and ia_css_region structures to css-api.
- Adding zoom_region(type ia_css_region) parameter to ia_css_dz_config structure.
- By using this user can do the zoom based on zoom region and
- the center of the zoom region is not restricted at the center of the input frame.

v2.1.8.6 (1 changes parallel), Add new ia_css_fw_warning type
- Add IA_CSS_FW_WARNING_TAG_EXP_ID_FAILED enum to ia_css_fw_warning type
- Extend sp_warning() with exp_id parameter

v2.1.8.6 (1 changes parallel), Add includes in GC, GC2 kernel interface files
- add ia_css_ctc_types.h includes in ia_css_gc_types.h and ia_css_gc2_types.h. Needed to get ia_css_vamem_type.

v2.1.9.0 (1 changes parallel), Introduce sp assert event.
- Add IA_CSS_EVENT_TYPE_FW_ASSERT. The FW sends the event in case an assert goes off.

v2.1.9.1 (1 changes parallel), Exclude driver part from ia_css_buffer.h as it is also used by SP
- Excluded driver part of the interface from SP/ISP code
- Driver I/F is not affected

*/

#endif /*__CSS_API_VERSION_H*/
