/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _sp_map_h_
#define _sp_map_h_


#ifndef _hrt_dummy_use_blob_sp
#define _hrt_dummy_use_blob_sp()
#endif

#define _hrt_cell_load_program_sp(proc) _hrt_cell_load_program_embedded(proc, sp)

/* function longjmp: 640E */

/* function tmpmem_init_dmem: 61AB */

/* function ia_css_dmaproxy_sp_set_addr_B: 38CE */

/* function debug_buffer_set_ddr_addr: DD */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_mipi
#define HIVE_MEM_vbuf_mipi scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_mipi 0x7248
#define HIVE_SIZE_vbuf_mipi 12
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_mipi scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_mipi 0x7248
#define HIVE_SIZE_sp_vbuf_mipi 12

/* function ia_css_event_sp_decode: 3AA3 */

/* function ia_css_queue_get_size: 4D5C */

/* function ia_css_queue_load: 539D */

/* function setjmp: 6417 */

/* function ia_css_pipeline_sp_sfi_get_current_frame: 26B2 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_sp2host_isys_event_queue
#define HIVE_MEM_sem_for_sp2host_isys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_sp2host_isys_event_queue 0x5620
#define HIVE_SIZE_sem_for_sp2host_isys_event_queue 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_sp2host_isys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_sp2host_isys_event_queue 0x5620
#define HIVE_SIZE_sp_sem_for_sp2host_isys_event_queue 20

/* function ia_css_dmaproxy_sp_wait_for_ack: 6991 */

/* function ia_css_sp_rawcopy_func: 5508 */

/* function ia_css_tagger_buf_sp_pop_marked: 303C */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_N_CSI_RX_BE_SID_WIDTH
#define HIVE_MEM_N_CSI_RX_BE_SID_WIDTH scalar_processor_2400_dmem
#define HIVE_ADDR_N_CSI_RX_BE_SID_WIDTH 0x1D4
#define HIVE_SIZE_N_CSI_RX_BE_SID_WIDTH 12
#else
#endif
#endif
#define HIVE_MEM_sp_N_CSI_RX_BE_SID_WIDTH scalar_processor_2400_dmem
#define HIVE_ADDR_sp_N_CSI_RX_BE_SID_WIDTH 0x1D4
#define HIVE_SIZE_sp_N_CSI_RX_BE_SID_WIDTH 12

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_stage
#define HIVE_MEM_isp_stage scalar_processor_2400_dmem
#define HIVE_ADDR_isp_stage 0x6B48
#define HIVE_SIZE_isp_stage 832
#else
#endif
#endif
#define HIVE_MEM_sp_isp_stage scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_stage 0x6B48
#define HIVE_SIZE_sp_isp_stage 832

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_raw
#define HIVE_MEM_vbuf_raw scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_raw 0x37C
#define HIVE_SIZE_vbuf_raw 4
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_raw scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_raw 0x37C
#define HIVE_SIZE_sp_vbuf_raw 4

/* function ia_css_sp_bin_copy_func: 54E9 */

/* function ia_css_queue_item_store: 50EB */

/* function input_system_reset: 128E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_metadata_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_metadata_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_metadata_bufs 0x59F8
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_metadata_bufs 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_metadata_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_metadata_bufs 0x59F8
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_metadata_bufs 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_buffer_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_buffer_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_buffer_bufs 0x5A0C
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_buffer_bufs 160
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_buffer_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_buffer_bufs 0x5A0C
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_buffer_bufs 160

/* function sp_start_isp: 39C */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_binary_group
#define HIVE_MEM_sp_binary_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_binary_group 0x6F38
#define HIVE_SIZE_sp_binary_group 32
#else
#endif
#endif
#define HIVE_MEM_sp_sp_binary_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_binary_group 0x6F38
#define HIVE_SIZE_sp_sp_binary_group 32

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_sw_state
#define HIVE_MEM_sp_sw_state scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sw_state 0x71F4
#define HIVE_SIZE_sp_sw_state 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_sw_state scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_sw_state 0x71F4
#define HIVE_SIZE_sp_sp_sw_state 4

/* function ia_css_thread_sp_main: 13FF */

/* function ia_css_ispctrl_sp_init_internal_buffers: 3CA9 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp2host_psys_event_queue_handle
#define HIVE_MEM_sp2host_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp2host_psys_event_queue_handle 0x5AAC
#define HIVE_SIZE_sp2host_psys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_sp2host_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp2host_psys_event_queue_handle 0x5AAC
#define HIVE_SIZE_sp_sp2host_psys_event_queue_handle 12

/* function pixelgen_unit_test: E70 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_sp2host_psys_event_queue
#define HIVE_MEM_sem_for_sp2host_psys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_sp2host_psys_event_queue 0x5634
#define HIVE_SIZE_sem_for_sp2host_psys_event_queue 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_sp2host_psys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_sp2host_psys_event_queue 0x5634
#define HIVE_SIZE_sp_sem_for_sp2host_psys_event_queue 20

/* function ia_css_tagger_sp_propagate_frame: 2B53 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_stop_copy_preview
#define HIVE_MEM_sp_stop_copy_preview scalar_processor_2400_dmem
#define HIVE_ADDR_sp_stop_copy_preview 0x71D8
#define HIVE_SIZE_sp_stop_copy_preview 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_stop_copy_preview scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_stop_copy_preview 0x71D8
#define HIVE_SIZE_sp_sp_stop_copy_preview 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_handles
#define HIVE_MEM_vbuf_handles scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_handles 0x7254
#define HIVE_SIZE_vbuf_handles 960
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_handles scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_handles 0x7254
#define HIVE_SIZE_sp_vbuf_handles 960

/* function ia_css_queue_store: 5251 */

/* function ia_css_sp_flash_register: 3256 */

/* function ia_css_sp_rawcopy_dummy_function: 5952 */

/* function ia_css_pipeline_sp_init: 1F35 */

/* function ia_css_tagger_sp_configure: 2A50 */

/* function ia_css_ispctrl_sp_end_binary: 3AEC */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs
#define HIVE_MEM_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 0x5AB8
#define HIVE_SIZE_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 0x5AB8
#define HIVE_SIZE_sp_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 20

/* function pixelgen_tpg_run: F26 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_event_is_pending_mask
#define HIVE_MEM_event_is_pending_mask scalar_processor_2400_dmem
#define HIVE_ADDR_event_is_pending_mask 0x5C
#define HIVE_SIZE_event_is_pending_mask 44
#else
#endif
#endif
#define HIVE_MEM_sp_event_is_pending_mask scalar_processor_2400_dmem
#define HIVE_ADDR_sp_event_is_pending_mask 0x5C
#define HIVE_SIZE_sp_event_is_pending_mask 44

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cb_elems_frame
#define HIVE_MEM_sp_all_cb_elems_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cb_elems_frame 0x5648
#define HIVE_SIZE_sp_all_cb_elems_frame 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cb_elems_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cb_elems_frame 0x5648
#define HIVE_SIZE_sp_sp_all_cb_elems_frame 16

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp2host_isys_event_queue_handle
#define HIVE_MEM_sp2host_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp2host_isys_event_queue_handle 0x5ACC
#define HIVE_SIZE_sp2host_isys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_sp2host_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp2host_isys_event_queue_handle 0x5ACC
#define HIVE_SIZE_sp_sp2host_isys_event_queue_handle 12

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host_sp_com
#define HIVE_MEM_host_sp_com scalar_processor_2400_dmem
#define HIVE_ADDR_host_sp_com 0x3D44
#define HIVE_SIZE_host_sp_com 220
#else
#endif
#endif
#define HIVE_MEM_sp_host_sp_com scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host_sp_com 0x3D44
#define HIVE_SIZE_sp_host_sp_com 220

/* function ia_css_queue_get_free_space: 4EB0 */

/* function exec_image_pipe: 5E3 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_init_dmem_data
#define HIVE_MEM_sp_init_dmem_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_init_dmem_data 0x71F8
#define HIVE_SIZE_sp_init_dmem_data 24
#else
#endif
#endif
#define HIVE_MEM_sp_sp_init_dmem_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_init_dmem_data 0x71F8
#define HIVE_SIZE_sp_sp_init_dmem_data 24

/* function ia_css_sp_metadata_start: 5A2C */

/* function ia_css_bufq_sp_init_buffer_queues: 32A7 */

/* function ia_css_pipeline_sp_stop: 1F18 */

/* function ia_css_tagger_sp_connect_pipes: 2EC5 */

/* function sp_isys_copy_wait: 641 */

/* function is_isp_debug_buffer_full: 337 */

/* function ia_css_dmaproxy_sp_configure_channel_from_info: 3851 */

/* function encode_and_post_timer_event: A97 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_input_system_bz2788_active
#define HIVE_MEM_input_system_bz2788_active scalar_processor_2400_dmem
#define HIVE_ADDR_input_system_bz2788_active 0x250C
#define HIVE_SIZE_input_system_bz2788_active 4
#else
#endif
#endif
#define HIVE_MEM_sp_input_system_bz2788_active scalar_processor_2400_dmem
#define HIVE_ADDR_sp_input_system_bz2788_active 0x250C
#define HIVE_SIZE_sp_input_system_bz2788_active 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_N_IBUF_CTRL_PROCS
#define HIVE_MEM_N_IBUF_CTRL_PROCS scalar_processor_2400_dmem
#define HIVE_ADDR_N_IBUF_CTRL_PROCS 0x200
#define HIVE_SIZE_N_IBUF_CTRL_PROCS 12
#else
#endif
#endif
#define HIVE_MEM_sp_N_IBUF_CTRL_PROCS scalar_processor_2400_dmem
#define HIVE_ADDR_sp_N_IBUF_CTRL_PROCS 0x200
#define HIVE_SIZE_sp_N_IBUF_CTRL_PROCS 12

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_per_frame_data
#define HIVE_MEM_sp_per_frame_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_per_frame_data 0x3E20
#define HIVE_SIZE_sp_per_frame_data 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_per_frame_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_per_frame_data 0x3E20
#define HIVE_SIZE_sp_sp_per_frame_data 4

/* function ia_css_rmgr_sp_vbuf_dequeue: 5F07 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_psys_event_queue_handle
#define HIVE_MEM_host2sp_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_psys_event_queue_handle 0x5AD8
#define HIVE_SIZE_host2sp_psys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_psys_event_queue_handle 0x5AD8
#define HIVE_SIZE_sp_host2sp_psys_event_queue_handle 12

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_xmem_bin_addr
#define HIVE_MEM_xmem_bin_addr scalar_processor_2400_dmem
#define HIVE_ADDR_xmem_bin_addr 0x3E24
#define HIVE_SIZE_xmem_bin_addr 4
#else
#endif
#endif
#define HIVE_MEM_sp_xmem_bin_addr scalar_processor_2400_dmem
#define HIVE_ADDR_sp_xmem_bin_addr 0x3E24
#define HIVE_SIZE_sp_xmem_bin_addr 4

/* function tmr_clock_init: B2C */

/* function ia_css_pipeline_sp_run: 1A8E */

/* function memcpy: 64B7 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_N_ISYS2401_DMA_CHANNEL_PROCS
#define HIVE_MEM_N_ISYS2401_DMA_CHANNEL_PROCS scalar_processor_2400_dmem
#define HIVE_ADDR_N_ISYS2401_DMA_CHANNEL_PROCS 0x218
#define HIVE_SIZE_N_ISYS2401_DMA_CHANNEL_PROCS 4
#else
#endif
#endif
#define HIVE_MEM_sp_N_ISYS2401_DMA_CHANNEL_PROCS scalar_processor_2400_dmem
#define HIVE_ADDR_sp_N_ISYS2401_DMA_CHANNEL_PROCS 0x218
#define HIVE_SIZE_sp_N_ISYS2401_DMA_CHANNEL_PROCS 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_GP_DEVICE_BASE
#define HIVE_MEM_GP_DEVICE_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_GP_DEVICE_BASE 0x384
#define HIVE_SIZE_GP_DEVICE_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_GP_DEVICE_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_GP_DEVICE_BASE 0x384
#define HIVE_SIZE_sp_GP_DEVICE_BASE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_thread_sp_ready_queue
#define HIVE_MEM_ia_css_thread_sp_ready_queue scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_thread_sp_ready_queue 0x27C
#define HIVE_SIZE_ia_css_thread_sp_ready_queue 12
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_thread_sp_ready_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_thread_sp_ready_queue 0x27C
#define HIVE_SIZE_sp_ia_css_thread_sp_ready_queue 12

/* function stream2mmio_send_command: E12 */

/* function ia_css_uds_sp_scale_params: 61C1 */

/* function ia_css_circbuf_increase_size: 14E4 */

/* function __divu: 6435 */

/* function ia_css_thread_sp_get_state: 1327 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_cont_capt_stop
#define HIVE_MEM_sem_for_cont_capt_stop scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_cont_capt_stop 0x5658
#define HIVE_SIZE_sem_for_cont_capt_stop 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_cont_capt_stop scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_cont_capt_stop 0x5658
#define HIVE_SIZE_sp_sem_for_cont_capt_stop 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_N_SHORT_PACKET_LUT_ENTRIES
#define HIVE_MEM_N_SHORT_PACKET_LUT_ENTRIES scalar_processor_2400_dmem
#define HIVE_ADDR_N_SHORT_PACKET_LUT_ENTRIES 0x1B0
#define HIVE_SIZE_N_SHORT_PACKET_LUT_ENTRIES 12
#else
#endif
#endif
#define HIVE_MEM_sp_N_SHORT_PACKET_LUT_ENTRIES scalar_processor_2400_dmem
#define HIVE_ADDR_sp_N_SHORT_PACKET_LUT_ENTRIES 0x1B0
#define HIVE_SIZE_sp_N_SHORT_PACKET_LUT_ENTRIES 12

/* function thread_fiber_sp_main: 14DD */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_isp_pipe_thread
#define HIVE_MEM_sp_isp_pipe_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_pipe_thread 0x579C
#define HIVE_SIZE_sp_isp_pipe_thread 340
#else
#endif
#endif
#define HIVE_MEM_sp_sp_isp_pipe_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_isp_pipe_thread 0x579C
#define HIVE_SIZE_sp_sp_isp_pipe_thread 340

/* function ia_css_parambuf_sp_handle_parameter_sets: 1927 */

/* function ia_css_spctrl_sp_set_state: 5A48 */

/* function ia_css_thread_sem_sp_signal: 669A */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_IRQ_BASE
#define HIVE_MEM_IRQ_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_IRQ_BASE 0x2C
#define HIVE_SIZE_IRQ_BASE 16
#else
#endif
#endif
#define HIVE_MEM_sp_IRQ_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_IRQ_BASE 0x2C
#define HIVE_SIZE_sp_IRQ_BASE 16

/* function ia_css_virtual_isys_sp_isr_init: 5AE7 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_TIMED_CTRL_BASE
#define HIVE_MEM_TIMED_CTRL_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_TIMED_CTRL_BASE 0x40
#define HIVE_SIZE_TIMED_CTRL_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_TIMED_CTRL_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_TIMED_CTRL_BASE 0x40
#define HIVE_SIZE_sp_TIMED_CTRL_BASE 4

/* function ia_css_isys_sp_generate_exp_id: 5D97 */

/* function ia_css_rmgr_sp_init: 5E02 */

/* function ia_css_thread_sem_sp_init: 676B */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_is_isp_requested
#define HIVE_MEM_is_isp_requested scalar_processor_2400_dmem
#define HIVE_ADDR_is_isp_requested 0x390
#define HIVE_SIZE_is_isp_requested 4
#else
#endif
#endif
#define HIVE_MEM_sp_is_isp_requested scalar_processor_2400_dmem
#define HIVE_ADDR_sp_is_isp_requested 0x390
#define HIVE_SIZE_sp_is_isp_requested 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_reading_cb_frame
#define HIVE_MEM_sem_for_reading_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_reading_cb_frame 0x566C
#define HIVE_SIZE_sem_for_reading_cb_frame 40
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_reading_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_reading_cb_frame 0x566C
#define HIVE_SIZE_sp_sem_for_reading_cb_frame 40

/* function ia_css_dmaproxy_sp_execute: 37B6 */

/* function csi_rx_backend_rst: CEE */

/* function ia_css_queue_is_empty: 4D97 */

/* function ia_css_pipeline_sp_has_stopped: 1F0E */

/* function ia_css_circbuf_extract: 15E4 */

/* function ia_css_tagger_buf_sp_is_locked_from_start: 3183 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_current_sp_thread
#define HIVE_MEM_current_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_current_sp_thread 0x278
#define HIVE_SIZE_current_sp_thread 4
#else
#endif
#endif
#define HIVE_MEM_sp_current_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_current_sp_thread 0x278
#define HIVE_SIZE_sp_current_sp_thread 4

/* function ia_css_spctrl_sp_get_spid: 5A4F */

/* function ia_css_dmaproxy_sp_read_byte_addr: 69BF */

/* function ia_css_rmgr_sp_uninit: 5DFB */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_threads_stack
#define HIVE_MEM_sp_threads_stack scalar_processor_2400_dmem
#define HIVE_ADDR_sp_threads_stack 0x168
#define HIVE_SIZE_sp_threads_stack 24
#else
#endif
#endif
#define HIVE_MEM_sp_sp_threads_stack scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_threads_stack 0x168
#define HIVE_SIZE_sp_sp_threads_stack 24

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_N_STREAM2MMIO_SID_PROCS
#define HIVE_MEM_N_STREAM2MMIO_SID_PROCS scalar_processor_2400_dmem
#define HIVE_ADDR_N_STREAM2MMIO_SID_PROCS 0x21C
#define HIVE_SIZE_N_STREAM2MMIO_SID_PROCS 12
#else
#endif
#endif
#define HIVE_MEM_sp_N_STREAM2MMIO_SID_PROCS scalar_processor_2400_dmem
#define HIVE_ADDR_sp_N_STREAM2MMIO_SID_PROCS 0x21C
#define HIVE_SIZE_sp_N_STREAM2MMIO_SID_PROCS 12

/* function ia_css_circbuf_peek: 15C6 */

/* function ia_css_parambuf_sp_wait_for_in_param: 16EF */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cb_elems_param
#define HIVE_MEM_sp_all_cb_elems_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cb_elems_param 0x5694
#define HIVE_SIZE_sp_all_cb_elems_param 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cb_elems_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cb_elems_param 0x5694
#define HIVE_SIZE_sp_sp_all_cb_elems_param 16

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_pipeline_sp_curr_binary_id
#define HIVE_MEM_pipeline_sp_curr_binary_id scalar_processor_2400_dmem
#define HIVE_ADDR_pipeline_sp_curr_binary_id 0x288
#define HIVE_SIZE_pipeline_sp_curr_binary_id 4
#else
#endif
#endif
#define HIVE_MEM_sp_pipeline_sp_curr_binary_id scalar_processor_2400_dmem
#define HIVE_ADDR_sp_pipeline_sp_curr_binary_id 0x288
#define HIVE_SIZE_sp_pipeline_sp_curr_binary_id 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_frame_desc
#define HIVE_MEM_sp_all_cbs_frame_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_frame_desc 0x56A4
#define HIVE_SIZE_sp_all_cbs_frame_desc 8
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_frame_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_frame_desc 0x56A4
#define HIVE_SIZE_sp_sp_all_cbs_frame_desc 8

/* function sp_isys_copy_func_v2: 626 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_reading_cb_param
#define HIVE_MEM_sem_for_reading_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_reading_cb_param 0x56AC
#define HIVE_SIZE_sem_for_reading_cb_param 40
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_reading_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_reading_cb_param 0x56AC
#define HIVE_SIZE_sp_sem_for_reading_cb_param 40

/* function ia_css_queue_get_used_space: 4E64 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_cont_capt_start
#define HIVE_MEM_sem_for_cont_capt_start scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_cont_capt_start 0x56D4
#define HIVE_SIZE_sem_for_cont_capt_start 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_cont_capt_start scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_cont_capt_start 0x56D4
#define HIVE_SIZE_sp_sem_for_cont_capt_start 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_tmp_heap
#define HIVE_MEM_tmp_heap scalar_processor_2400_dmem
#define HIVE_ADDR_tmp_heap 0x6F58
#define HIVE_SIZE_tmp_heap 640
#else
#endif
#endif
#define HIVE_MEM_sp_tmp_heap scalar_processor_2400_dmem
#define HIVE_ADDR_sp_tmp_heap 0x6F58
#define HIVE_SIZE_sp_tmp_heap 640

/* function ia_css_rmgr_sp_get_num_vbuf: 6103 */

/* function ia_css_ispctrl_sp_output_compute_dma_info: 440D */

/* function ia_css_tagger_sp_lock_exp_id: 287E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_s3a_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_s3a_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_s3a_bufs 0x5AE4
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_s3a_bufs 60
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_s3a_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_s3a_bufs 0x5AE4
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_s3a_bufs 60

/* function ia_css_queue_is_full: 4EFB */

/* function debug_buffer_init_isp: E4 */

/* function ia_css_tagger_sp_exp_id_is_locked: 27CC */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_rmgr_sp_mipi_frame_sem
#define HIVE_MEM_ia_css_rmgr_sp_mipi_frame_sem scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_rmgr_sp_mipi_frame_sem 0x7614
#define HIVE_SIZE_ia_css_rmgr_sp_mipi_frame_sem 60
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_rmgr_sp_mipi_frame_sem scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_rmgr_sp_mipi_frame_sem 0x7614
#define HIVE_SIZE_sp_ia_css_rmgr_sp_mipi_frame_sem 60

/* function ia_css_rmgr_sp_refcount_dump: 5EE2 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_isp_parameters_id
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_isp_parameters_id scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_isp_parameters_id 0x5B20
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_isp_parameters_id 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_isp_parameters_id scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_isp_parameters_id 0x5B20
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_isp_parameters_id 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_pipe_threads
#define HIVE_MEM_sp_pipe_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_pipe_threads 0x154
#define HIVE_SIZE_sp_pipe_threads 20
#else
#endif
#endif
#define HIVE_MEM_sp_sp_pipe_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_pipe_threads 0x154
#define HIVE_SIZE_sp_sp_pipe_threads 20

/* function sp_event_proxy_func: 780 */

/* function ibuf_ctrl_run: D87 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_isys_event_queue_handle
#define HIVE_MEM_host2sp_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_isys_event_queue_handle 0x5B34
#define HIVE_SIZE_host2sp_isys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_isys_event_queue_handle 0x5B34
#define HIVE_SIZE_sp_host2sp_isys_event_queue_handle 12

/* function ia_css_thread_sp_yield: 6613 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_param_desc
#define HIVE_MEM_sp_all_cbs_param_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_param_desc 0x56E8
#define HIVE_SIZE_sp_all_cbs_param_desc 8
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_param_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_param_desc 0x56E8
#define HIVE_SIZE_sp_sp_all_cbs_param_desc 8

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_dmaproxy_sp_invalidate_tlb
#define HIVE_MEM_ia_css_dmaproxy_sp_invalidate_tlb scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb 0x6B38
#define HIVE_SIZE_ia_css_dmaproxy_sp_invalidate_tlb 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_dmaproxy_sp_invalidate_tlb scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_dmaproxy_sp_invalidate_tlb 0x6B38
#define HIVE_SIZE_sp_ia_css_dmaproxy_sp_invalidate_tlb 4

/* function ia_css_thread_sp_fork: 13B4 */

/* function ia_css_tagger_sp_destroy: 2ECF */

/* function ia_css_dmaproxy_sp_vmem_read: 3756 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_N_LONG_PACKET_LUT_ENTRIES
#define HIVE_MEM_N_LONG_PACKET_LUT_ENTRIES scalar_processor_2400_dmem
#define HIVE_ADDR_N_LONG_PACKET_LUT_ENTRIES 0x1BC
#define HIVE_SIZE_N_LONG_PACKET_LUT_ENTRIES 12
#else
#endif
#endif
#define HIVE_MEM_sp_N_LONG_PACKET_LUT_ENTRIES scalar_processor_2400_dmem
#define HIVE_ADDR_sp_N_LONG_PACKET_LUT_ENTRIES 0x1BC
#define HIVE_SIZE_sp_N_LONG_PACKET_LUT_ENTRIES 12

/* function initialize_sp_group: 5F3 */

/* function ia_css_thread_sp_init: 13E0 */

/* function ia_css_isys_sp_reset_exp_id: 5D8E */

/* function ia_css_ispctrl_sp_set_stream_base_addr: 4AD5 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ISP_DMEM_BASE
#define HIVE_MEM_ISP_DMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_ISP_DMEM_BASE 0x10
#define HIVE_SIZE_ISP_DMEM_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_ISP_DMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ISP_DMEM_BASE 0x10
#define HIVE_SIZE_sp_ISP_DMEM_BASE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_SP_DMEM_BASE
#define HIVE_MEM_SP_DMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_SP_DMEM_BASE 0x4
#define HIVE_SIZE_SP_DMEM_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_SP_DMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_SP_DMEM_BASE 0x4
#define HIVE_SIZE_sp_SP_DMEM_BASE 4

/* function ibuf_ctrl_transfer: D6F */

/* function ia_css_dmaproxy_sp_read: 37CF */

/* function virtual_isys_stream_is_capture_done: 5B0B */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_raw_copy_line_count
#define HIVE_MEM_raw_copy_line_count scalar_processor_2400_dmem
#define HIVE_ADDR_raw_copy_line_count 0x360
#define HIVE_SIZE_raw_copy_line_count 4
#else
#endif
#endif
#define HIVE_MEM_sp_raw_copy_line_count scalar_processor_2400_dmem
#define HIVE_ADDR_sp_raw_copy_line_count 0x360
#define HIVE_SIZE_sp_raw_copy_line_count 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_tag_cmd_queue_handle
#define HIVE_MEM_host2sp_tag_cmd_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_tag_cmd_queue_handle 0x5B40
#define HIVE_SIZE_host2sp_tag_cmd_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_tag_cmd_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_tag_cmd_queue_handle 0x5B40
#define HIVE_SIZE_sp_host2sp_tag_cmd_queue_handle 12

/* function ia_css_queue_peek: 4DDA */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_frame_cnt
#define HIVE_MEM_ia_css_flash_sp_frame_cnt scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_frame_cnt 0x59EC
#define HIVE_SIZE_ia_css_flash_sp_frame_cnt 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_frame_cnt scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_frame_cnt 0x59EC
#define HIVE_SIZE_sp_ia_css_flash_sp_frame_cnt 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_event_can_send_token_mask
#define HIVE_MEM_event_can_send_token_mask scalar_processor_2400_dmem
#define HIVE_ADDR_event_can_send_token_mask 0x88
#define HIVE_SIZE_event_can_send_token_mask 44
#else
#endif
#endif
#define HIVE_MEM_sp_event_can_send_token_mask scalar_processor_2400_dmem
#define HIVE_ADDR_sp_event_can_send_token_mask 0x88
#define HIVE_SIZE_sp_event_can_send_token_mask 44

/* function csi_rx_frontend_stop: C19 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_thread
#define HIVE_MEM_isp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_isp_thread 0x6E88
#define HIVE_SIZE_isp_thread 4
#else
#endif
#endif
#define HIVE_MEM_sp_isp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_thread 0x6E88
#define HIVE_SIZE_sp_isp_thread 4

/* function encode_and_post_sp_event_non_blocking: ADF */

/* function is_ddr_debug_buffer_full: 2CC */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_threads_fiber
#define HIVE_MEM_sp_threads_fiber scalar_processor_2400_dmem
#define HIVE_ADDR_sp_threads_fiber 0x198
#define HIVE_SIZE_sp_threads_fiber 24
#else
#endif
#endif
#define HIVE_MEM_sp_sp_threads_fiber scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_threads_fiber 0x198
#define HIVE_SIZE_sp_sp_threads_fiber 24

/* function encode_and_post_sp_event: A68 */

/* function debug_enqueue_ddr: EE */

/* function ia_css_rmgr_sp_refcount_init_vbuf: 5E9D */

/* function dmaproxy_sp_read_write: 6A4A */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_dmaproxy_isp_dma_cmd_buffer
#define HIVE_MEM_ia_css_dmaproxy_isp_dma_cmd_buffer scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_dmaproxy_isp_dma_cmd_buffer 0x6B3C
#define HIVE_SIZE_ia_css_dmaproxy_isp_dma_cmd_buffer 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_dmaproxy_isp_dma_cmd_buffer scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_dmaproxy_isp_dma_cmd_buffer 0x6B3C
#define HIVE_SIZE_sp_ia_css_dmaproxy_isp_dma_cmd_buffer 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_buffer_queue_handle
#define HIVE_MEM_host2sp_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_buffer_queue_handle 0x5B4C
#define HIVE_SIZE_host2sp_buffer_queue_handle 480
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_buffer_queue_handle 0x5B4C
#define HIVE_SIZE_sp_host2sp_buffer_queue_handle 480

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_in_service
#define HIVE_MEM_ia_css_flash_sp_in_service scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_in_service 0x2FC4
#define HIVE_SIZE_ia_css_flash_sp_in_service 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_in_service scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_in_service 0x2FC4
#define HIVE_SIZE_sp_ia_css_flash_sp_in_service 4

/* function ia_css_dmaproxy_sp_process: 6793 */

/* function ia_css_tagger_buf_sp_mark_from_end: 3230 */

/* function ia_css_ispctrl_sp_init_cs: 3BD9 */

/* function ia_css_spctrl_sp_init: 5A5D */

/* function sp_event_proxy_init: 795 */

/* function input_system_input_port_close: 10A3 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_previous_clock_tick
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_previous_clock_tick scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_previous_clock_tick 0x5D2C
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_previous_clock_tick 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_previous_clock_tick scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_previous_clock_tick 0x5D2C
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_previous_clock_tick 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_output
#define HIVE_MEM_sp_output scalar_processor_2400_dmem
#define HIVE_ADDR_sp_output 0x3E28
#define HIVE_SIZE_sp_output 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_output scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_output 0x3E28
#define HIVE_SIZE_sp_sp_output 16

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_sems_for_host2sp_buf_queues
#define HIVE_MEM_ia_css_bufq_sp_sems_for_host2sp_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_sems_for_host2sp_buf_queues 0x5D40
#define HIVE_SIZE_ia_css_bufq_sp_sems_for_host2sp_buf_queues 800
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_sems_for_host2sp_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_sems_for_host2sp_buf_queues 0x5D40
#define HIVE_SIZE_sp_ia_css_bufq_sp_sems_for_host2sp_buf_queues 800

/* function pixelgen_prbs_config: E9B */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ISP_CTRL_BASE
#define HIVE_MEM_ISP_CTRL_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_ISP_CTRL_BASE 0x8
#define HIVE_SIZE_ISP_CTRL_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_ISP_CTRL_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ISP_CTRL_BASE 0x8
#define HIVE_SIZE_sp_ISP_CTRL_BASE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_INPUT_FORMATTER_BASE
#define HIVE_MEM_INPUT_FORMATTER_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_INPUT_FORMATTER_BASE 0x4C
#define HIVE_SIZE_INPUT_FORMATTER_BASE 16
#else
#endif
#endif
#define HIVE_MEM_sp_INPUT_FORMATTER_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_INPUT_FORMATTER_BASE 0x4C
#define HIVE_SIZE_sp_INPUT_FORMATTER_BASE 16

/* function sp_dma_proxy_reset_channels: 3A0D */

/* function ia_css_tagger_sp_update_size: 2FB1 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_host_sp_queue
#define HIVE_MEM_ia_css_bufq_host_sp_queue scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_host_sp_queue 0x6060
#define HIVE_SIZE_ia_css_bufq_host_sp_queue 2008
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_host_sp_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_host_sp_queue 0x6060
#define HIVE_SIZE_sp_ia_css_bufq_host_sp_queue 2008

/* function thread_fiber_sp_create: 144C */

/* function ia_css_dmaproxy_sp_set_increments: 38BB */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_writing_cb_frame
#define HIVE_MEM_sem_for_writing_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_writing_cb_frame 0x56F0
#define HIVE_SIZE_sem_for_writing_cb_frame 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_writing_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_writing_cb_frame 0x56F0
#define HIVE_SIZE_sp_sem_for_writing_cb_frame 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_writing_cb_param
#define HIVE_MEM_sem_for_writing_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_writing_cb_param 0x5704
#define HIVE_SIZE_sem_for_writing_cb_param 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_writing_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_writing_cb_param 0x5704
#define HIVE_SIZE_sp_sem_for_writing_cb_param 20

/* function pixelgen_tpg_is_done: F15 */

/* function ia_css_isys_stream_capture_indication: 5C11 */

/* function sp_start_isp_entry: 392 */
#ifndef HIVE_MULTIPLE_PROGRAMS
#ifdef HIVE_ADDR_sp_start_isp_entry
#endif
#define HIVE_ADDR_sp_start_isp_entry 0x392
#endif
#define HIVE_ADDR_sp_sp_start_isp_entry 0x392

/* function ia_css_tagger_buf_sp_unmark_all: 31C9 */

/* function ia_css_tagger_buf_sp_unmark_from_start: 320A */

/* function ia_css_dmaproxy_sp_channel_acquire: 3A39 */

/* function ia_css_rmgr_sp_add_num_vbuf: 60DF */

/* function ibuf_ctrl_config: D93 */

/* function ia_css_isys_stream_stop: 5C89 */

/* function __ia_css_dmaproxy_sp_wait_for_ack_text: 3721 */

/* function ia_css_bufq_sp_is_dynamic_buffer: 360A */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_group
#define HIVE_MEM_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_group 0x3E38
#define HIVE_SIZE_sp_group 6116
#else
#endif
#endif
#define HIVE_MEM_sp_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_group 0x3E38
#define HIVE_SIZE_sp_sp_group 6116

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_event_proxy_thread
#define HIVE_MEM_sp_event_proxy_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_event_proxy_thread 0x58F0
#define HIVE_SIZE_sp_event_proxy_thread 68
#else
#endif
#endif
#define HIVE_MEM_sp_sp_event_proxy_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_event_proxy_thread 0x58F0
#define HIVE_SIZE_sp_sp_event_proxy_thread 68

/* function ia_css_thread_sp_kill: 137A */

/* function ia_css_tagger_sp_create: 2F74 */

/* function tmpmem_acquire_dmem: 618C */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_MMU_BASE
#define HIVE_MEM_MMU_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_MMU_BASE 0x24
#define HIVE_SIZE_MMU_BASE 8
#else
#endif
#endif
#define HIVE_MEM_sp_MMU_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_MMU_BASE 0x24
#define HIVE_SIZE_sp_MMU_BASE 8

/* function ia_css_dmaproxy_sp_channel_release: 3A25 */

/* function pixelgen_prbs_run: E89 */

/* function ia_css_dmaproxy_sp_is_idle: 3A05 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_qos_start
#define HIVE_MEM_sem_for_qos_start scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_qos_start 0x5718
#define HIVE_SIZE_sem_for_qos_start 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_qos_start scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_qos_start 0x5718
#define HIVE_SIZE_sp_sem_for_qos_start 20

/* function isp_hmem_load: B6B */

/* function ia_css_eventq_sp_send: 3A7B */

/* function ia_css_tagger_buf_sp_unlock_from_start: 313F */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_debug_buffer_ddr_address
#define HIVE_MEM_debug_buffer_ddr_address scalar_processor_2400_dmem
#define HIVE_ADDR_debug_buffer_ddr_address 0xBC
#define HIVE_SIZE_debug_buffer_ddr_address 4
#else
#endif
#endif
#define HIVE_MEM_sp_debug_buffer_ddr_address scalar_processor_2400_dmem
#define HIVE_ADDR_sp_debug_buffer_ddr_address 0xBC
#define HIVE_SIZE_sp_debug_buffer_ddr_address 4

/* function sp_isys_copy_request: 6E0 */

/* function ia_css_rmgr_sp_refcount_retain_vbuf: 5F77 */

/* function ia_css_thread_sp_set_priority: 1372 */

/* function sizeof_hmem: C12 */

/* function input_system_channel_open: 1249 */

/* function pixelgen_tpg_stop: F03 */

/* function tmpmem_release_dmem: 617B */

/* function ia_css_dmaproxy_sp_set_width_exception: 38A6 */

/* function sp_event_assert: 91C */

/* function ia_css_flash_sp_init_internal_params: 329C */

/* function ia_css_tagger_buf_sp_pop_unmarked_and_unlocked: 2FCF */

/* function __modu: 647B */

/* function ia_css_dmaproxy_sp_init_isp_vector: 3728 */

/* function input_system_channel_transfer: 1232 */

/* function isp_vamem_store: 0 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_GDC_BASE
#define HIVE_MEM_GDC_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_GDC_BASE 0x44
#define HIVE_SIZE_GDC_BASE 8
#else
#endif
#endif
#define HIVE_MEM_sp_GDC_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_GDC_BASE 0x44
#define HIVE_SIZE_sp_GDC_BASE 8

/* function ia_css_queue_local_init: 50C5 */

/* function sp_event_proxy_callout_func: 6548 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_thread_sp_num_ready_threads
#define HIVE_MEM_ia_css_thread_sp_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_thread_sp_num_ready_threads 0x5938
#define HIVE_SIZE_ia_css_thread_sp_num_ready_threads 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_thread_sp_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_thread_sp_num_ready_threads 0x5938
#define HIVE_SIZE_sp_ia_css_thread_sp_num_ready_threads 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_threads_stack_size
#define HIVE_MEM_sp_threads_stack_size scalar_processor_2400_dmem
#define HIVE_ADDR_sp_threads_stack_size 0x180
#define HIVE_SIZE_sp_threads_stack_size 24
#else
#endif
#endif
#define HIVE_MEM_sp_sp_threads_stack_size scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_threads_stack_size 0x180
#define HIVE_SIZE_sp_sp_threads_stack_size 24

/* function ia_css_ispctrl_sp_isp_done_row_striping: 43F3 */

/* function __ia_css_virtual_isys_sp_isr_text: 5AA0 */

/* function ia_css_queue_dequeue: 4F43 */

/* function ia_css_dmaproxy_sp_configure_channel: 69D6 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_current_thread_fiber_sp
#define HIVE_MEM_current_thread_fiber_sp scalar_processor_2400_dmem
#define HIVE_ADDR_current_thread_fiber_sp 0x5940
#define HIVE_SIZE_current_thread_fiber_sp 4
#else
#endif
#endif
#define HIVE_MEM_sp_current_thread_fiber_sp scalar_processor_2400_dmem
#define HIVE_ADDR_sp_current_thread_fiber_sp 0x5940
#define HIVE_SIZE_sp_current_thread_fiber_sp 4

/* function ia_css_circbuf_pop: 1674 */

/* function memset: 64FA */

/* function irq_raise_set_token: B6 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_GPIO_BASE
#define HIVE_MEM_GPIO_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_GPIO_BASE 0x3C
#define HIVE_SIZE_GPIO_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_GPIO_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_GPIO_BASE 0x3C
#define HIVE_SIZE_sp_GPIO_BASE 4

/* function pixelgen_prbs_stop: E77 */

/* function ia_css_pipeline_acc_stage_enable: 1EEE */

/* function ia_css_tagger_sp_unlock_exp_id: 27F1 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_ph
#define HIVE_MEM_isp_ph scalar_processor_2400_dmem
#define HIVE_ADDR_isp_ph 0x7210
#define HIVE_SIZE_isp_ph 28
#else
#endif
#endif
#define HIVE_MEM_sp_isp_ph scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_ph 0x7210
#define HIVE_SIZE_sp_isp_ph 28

/* function ia_css_ispctrl_sp_init_ds: 3D38 */

/* function get_xmem_base_addr_raw: 40C8 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_param
#define HIVE_MEM_sp_all_cbs_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_param 0x572C
#define HIVE_SIZE_sp_all_cbs_param 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_param 0x572C
#define HIVE_SIZE_sp_sp_all_cbs_param 16

/* function pixelgen_tpg_config: F38 */

/* function ia_css_circbuf_create: 16C2 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_sp_group
#define HIVE_MEM_sem_for_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_sp_group 0x573C
#define HIVE_SIZE_sem_for_sp_group 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_sp_group 0x573C
#define HIVE_SIZE_sp_sem_for_sp_group 20

/* function csi_rx_frontend_run: C2A */

/* function ia_css_framebuf_sp_wait_for_in_frame: 610A */

/* function ia_css_isys_stream_open: 5D3E */

/* function ia_css_sp_rawcopy_tag_frame: 58CC */

/* function input_system_channel_configure: 1265 */

/* function isp_hmem_clear: B3B */

/* function ia_css_framebuf_sp_release_in_frame: 614D */

/* function stream2mmio_config: E23 */

/* function ia_css_ispctrl_sp_start_binary: 3BB7 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_h_pipe_private_ddr_ptrs
#define HIVE_MEM_ia_css_bufq_sp_h_pipe_private_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 0x6838
#define HIVE_SIZE_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_h_pipe_private_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 0x6838
#define HIVE_SIZE_sp_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 20

/* function ia_css_eventq_sp_recv: 3A4D */

/* function csi_rx_frontend_config: C82 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_pool
#define HIVE_MEM_isp_pool scalar_processor_2400_dmem
#define HIVE_ADDR_isp_pool 0x370
#define HIVE_SIZE_isp_pool 4
#else
#endif
#endif
#define HIVE_MEM_sp_isp_pool scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_pool 0x370
#define HIVE_SIZE_sp_isp_pool 4

/* function ia_css_rmgr_sp_rel_gen: 5E44 */

/* function css_get_frame_processing_time_end: 27BC */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_event_any_pending_mask
#define HIVE_MEM_event_any_pending_mask scalar_processor_2400_dmem
#define HIVE_ADDR_event_any_pending_mask 0x388
#define HIVE_SIZE_event_any_pending_mask 8
#else
#endif
#endif
#define HIVE_MEM_sp_event_any_pending_mask scalar_processor_2400_dmem
#define HIVE_ADDR_sp_event_any_pending_mask 0x388
#define HIVE_SIZE_sp_event_any_pending_mask 8

/* function ia_css_pipeline_sp_get_pipe_io_status: 1A87 */

/* function sh_css_decode_tag_descr: 352 */

/* function debug_enqueue_isp: 27B */

/* function ia_css_spctrl_sp_uninit: 5A56 */

/* function csi_rx_backend_run: C70 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_dis_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_dis_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_dis_bufs 0x684C
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_dis_bufs 140
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_dis_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_dis_bufs 0x684C
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_dis_bufs 140

/* function ia_css_tagger_buf_sp_lock_from_start: 3161 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_isp_idle
#define HIVE_MEM_sem_for_isp_idle scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_isp_idle 0x5750
#define HIVE_SIZE_sem_for_isp_idle 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_isp_idle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_isp_idle 0x5750
#define HIVE_SIZE_sp_sem_for_isp_idle 20

/* function ia_css_dmaproxy_sp_write_byte_addr: 3785 */

/* function ia_css_dmaproxy_sp_init: 36FB */

/* function ia_css_bufq_sp_release_dynamic_buf_clock_tick: 332E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ISP_VAMEM_BASE
#define HIVE_MEM_ISP_VAMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_ISP_VAMEM_BASE 0x14
#define HIVE_SIZE_ISP_VAMEM_BASE 12
#else
#endif
#endif
#define HIVE_MEM_sp_ISP_VAMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ISP_VAMEM_BASE 0x14
#define HIVE_SIZE_sp_ISP_VAMEM_BASE 12

/* function input_system_channel_sync: 11AC */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_rawcopy_sp_tagger
#define HIVE_MEM_ia_css_rawcopy_sp_tagger scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_rawcopy_sp_tagger 0x71DC
#define HIVE_SIZE_ia_css_rawcopy_sp_tagger 24
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_rawcopy_sp_tagger scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_rawcopy_sp_tagger 0x71DC
#define HIVE_SIZE_sp_ia_css_rawcopy_sp_tagger 24

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_exp_ids
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_exp_ids scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_exp_ids 0x68D8
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_exp_ids 70
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_exp_ids scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_exp_ids 0x68D8
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_exp_ids 70

/* function ia_css_queue_item_load: 51B7 */

/* function ia_css_spctrl_sp_get_state: 5A41 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_callout_sp_thread
#define HIVE_MEM_callout_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_callout_sp_thread 0x5934
#define HIVE_SIZE_callout_sp_thread 4
#else
#endif
#endif
#define HIVE_MEM_sp_callout_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_callout_sp_thread 0x5934
#define HIVE_SIZE_sp_callout_sp_thread 4

/* function thread_fiber_sp_init: 14D3 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_SP_PMEM_BASE
#define HIVE_MEM_SP_PMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_SP_PMEM_BASE 0x0
#define HIVE_SIZE_SP_PMEM_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_SP_PMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_SP_PMEM_BASE 0x0
#define HIVE_SIZE_sp_SP_PMEM_BASE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_isp_input_stream_format
#define HIVE_MEM_sp_isp_input_stream_format scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_input_stream_format 0x3D28
#define HIVE_SIZE_sp_isp_input_stream_format 20
#else
#endif
#endif
#define HIVE_MEM_sp_sp_isp_input_stream_format scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_isp_input_stream_format 0x3D28
#define HIVE_SIZE_sp_sp_isp_input_stream_format 20

/* function __mod: 6467 */

/* function ia_css_dmaproxy_sp_init_dmem_channel: 37E9 */

/* function ia_css_thread_sp_join: 13A3 */

/* function ia_css_dmaproxy_sp_add_command: 6AB7 */

/* function ia_css_sp_metadata_thread_func: 5A3A */

/* function __sp_event_proxy_func_critical: 6535 */

/* function ia_css_pipeline_sp_wait_for_isys_stream_N: 5BAE */

/* function ia_css_sp_metadata_wait: 5A33 */

/* function ia_css_circbuf_peek_from_start: 15A8 */

/* function ia_css_event_sp_encode: 3AD8 */

/* function ia_css_thread_sp_run: 1416 */

/* function sp_isys_copy_func: 615 */

/* function ia_css_sp_isp_param_init_isp_memories: 4C40 */

/* function register_isr: 914 */

/* function irq_raise: C8 */

/* function ia_css_dmaproxy_sp_mmu_invalidate: 36C2 */

/* function csi_rx_backend_disable: C3C */

/* function pipeline_sp_initialize_stage: 201B */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_N_CSI_RX_FE_CTRL_DLANES
#define HIVE_MEM_N_CSI_RX_FE_CTRL_DLANES scalar_processor_2400_dmem
#define HIVE_ADDR_N_CSI_RX_FE_CTRL_DLANES 0x1C8
#define HIVE_SIZE_N_CSI_RX_FE_CTRL_DLANES 12
#else
#endif
#endif
#define HIVE_MEM_sp_N_CSI_RX_FE_CTRL_DLANES scalar_processor_2400_dmem
#define HIVE_ADDR_sp_N_CSI_RX_FE_CTRL_DLANES 0x1C8
#define HIVE_SIZE_sp_N_CSI_RX_FE_CTRL_DLANES 12

/* function ia_css_dmaproxy_sp_read_byte_addr_mmio: 69A8 */

/* function ia_css_ispctrl_sp_done_ds: 3D1F */

/* function csi_rx_backend_config: C93 */

/* function ia_css_sp_isp_param_get_mem_inits: 4C1B */

/* function ia_css_parambuf_sp_init_buffer_queues: 1A6D */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_pfp_spref
#define HIVE_MEM_vbuf_pfp_spref scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_pfp_spref 0x378
#define HIVE_SIZE_vbuf_pfp_spref 4
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_pfp_spref scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_pfp_spref 0x378
#define HIVE_SIZE_sp_vbuf_pfp_spref 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ISP_HMEM_BASE
#define HIVE_MEM_ISP_HMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_ISP_HMEM_BASE 0x20
#define HIVE_SIZE_ISP_HMEM_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_ISP_HMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ISP_HMEM_BASE 0x20
#define HIVE_SIZE_sp_ISP_HMEM_BASE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_frames
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_frames scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_frames 0x6920
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_frames 280
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_frames scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_frames 0x6920
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_frames 280

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp2host_buffer_queue_handle
#define HIVE_MEM_sp2host_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp2host_buffer_queue_handle 0x6A38
#define HIVE_SIZE_sp2host_buffer_queue_handle 96
#else
#endif
#endif
#define HIVE_MEM_sp_sp2host_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp2host_buffer_queue_handle 0x6A38
#define HIVE_SIZE_sp_sp2host_buffer_queue_handle 96

/* function ia_css_ispctrl_sp_init_isp_vars: 493A */

/* function ia_css_isys_stream_start: 5C6B */

/* function sp_warning: 946 */

/* function ia_css_rmgr_sp_vbuf_enqueue: 5F37 */

/* function ia_css_tagger_sp_tag_exp_id: 28E8 */

/* function ia_css_pipeline_sp_sfi_release_current_frame: 265E */

/* function ia_css_dmaproxy_sp_write: 379C */

/* function ia_css_isys_stream_start_async: 5CE5 */

/* function ia_css_parambuf_sp_release_in_param: 18ED */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_irq_sw_interrupt_token
#define HIVE_MEM_irq_sw_interrupt_token scalar_processor_2400_dmem
#define HIVE_ADDR_irq_sw_interrupt_token 0x3D24
#define HIVE_SIZE_irq_sw_interrupt_token 4
#else
#endif
#endif
#define HIVE_MEM_sp_irq_sw_interrupt_token scalar_processor_2400_dmem
#define HIVE_ADDR_sp_irq_sw_interrupt_token 0x3D24
#define HIVE_SIZE_sp_irq_sw_interrupt_token 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_isp_addresses
#define HIVE_MEM_sp_isp_addresses scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_addresses 0x6E8C
#define HIVE_SIZE_sp_isp_addresses 172
#else
#endif
#endif
#define HIVE_MEM_sp_sp_isp_addresses scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_isp_addresses 0x6E8C
#define HIVE_SIZE_sp_sp_isp_addresses 172

/* function ia_css_rmgr_sp_acq_gen: 5E5C */

/* function input_system_input_port_open: 10F5 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isps
#define HIVE_MEM_isps scalar_processor_2400_dmem
#define HIVE_ADDR_isps 0x722C
#define HIVE_SIZE_isps 28
#else
#endif
#endif
#define HIVE_MEM_sp_isps scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isps 0x722C
#define HIVE_SIZE_sp_isps 28

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host_sp_queues_initialized
#define HIVE_MEM_host_sp_queues_initialized scalar_processor_2400_dmem
#define HIVE_ADDR_host_sp_queues_initialized 0x3D3C
#define HIVE_SIZE_host_sp_queues_initialized 4
#else
#endif
#endif
#define HIVE_MEM_sp_host_sp_queues_initialized scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host_sp_queues_initialized 0x3D3C
#define HIVE_SIZE_sp_host_sp_queues_initialized 4

/* function ia_css_queue_uninit: 5083 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_ispctrl_sp_isp_started
#define HIVE_MEM_ia_css_ispctrl_sp_isp_started scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_ispctrl_sp_isp_started 0x6B40
#define HIVE_SIZE_ia_css_ispctrl_sp_isp_started 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_ispctrl_sp_isp_started scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_ispctrl_sp_isp_started 0x6B40
#define HIVE_SIZE_sp_ia_css_ispctrl_sp_isp_started 4

/* function ia_css_bufq_sp_release_dynamic_buf: 3382 */

/* function ia_css_dmaproxy_sp_set_height_exception: 3897 */

/* function ia_css_dmaproxy_sp_init_vmem_channel: 381C */

/* function csi_rx_backend_stop: C5F */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_num_ready_threads
#define HIVE_MEM_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_num_ready_threads 0x593C
#define HIVE_SIZE_num_ready_threads 4
#else
#endif
#endif
#define HIVE_MEM_sp_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_num_ready_threads 0x593C
#define HIVE_SIZE_sp_num_ready_threads 4

/* function ia_css_dmaproxy_sp_write_byte_addr_mmio: 376E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_spref
#define HIVE_MEM_vbuf_spref scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_spref 0x374
#define HIVE_SIZE_vbuf_spref 4
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_spref scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_spref 0x374
#define HIVE_SIZE_sp_vbuf_spref 4

/* function ia_css_queue_enqueue: 4FCD */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_request
#define HIVE_MEM_ia_css_flash_sp_request scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_request 0x59F0
#define HIVE_SIZE_ia_css_flash_sp_request 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_request scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_request 0x59F0
#define HIVE_SIZE_sp_ia_css_flash_sp_request 4

/* function ia_css_dmaproxy_sp_vmem_write: 373F */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_tagger_frames
#define HIVE_MEM_tagger_frames scalar_processor_2400_dmem
#define HIVE_ADDR_tagger_frames 0x5944
#define HIVE_SIZE_tagger_frames 168
#else
#endif
#endif
#define HIVE_MEM_sp_tagger_frames scalar_processor_2400_dmem
#define HIVE_ADDR_sp_tagger_frames 0x5944
#define HIVE_SIZE_sp_tagger_frames 168

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_reading_if
#define HIVE_MEM_sem_for_reading_if scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_reading_if 0x5764
#define HIVE_SIZE_sem_for_reading_if 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_reading_if scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_reading_if 0x5764
#define HIVE_SIZE_sp_sem_for_reading_if 20

/* function sp_generate_interrupts: 9C5 */

/* function ia_css_pipeline_sp_start: 1F20 */

/* function csi_rx_backend_enable: C4D */

/* function ia_css_sp_rawcopy_init: 54F0 */

/* function input_system_input_port_configure: 1147 */

/* function tmr_clock_read: B22 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ISP_BAMEM_BASE
#define HIVE_MEM_ISP_BAMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_ISP_BAMEM_BASE 0x380
#define HIVE_SIZE_ISP_BAMEM_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_ISP_BAMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ISP_BAMEM_BASE 0x380
#define HIVE_SIZE_sp_ISP_BAMEM_BASE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_sems_for_sp2host_buf_queues
#define HIVE_MEM_ia_css_bufq_sp_sems_for_sp2host_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_sems_for_sp2host_buf_queues 0x6A98
#define HIVE_SIZE_ia_css_bufq_sp_sems_for_sp2host_buf_queues 160
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_sems_for_sp2host_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_sems_for_sp2host_buf_queues 0x6A98
#define HIVE_SIZE_sp_ia_css_bufq_sp_sems_for_sp2host_buf_queues 160

/* function isys2401_dma_config_legacy: DE8 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ibuf_ctrl_master_ports
#define HIVE_MEM_ibuf_ctrl_master_ports scalar_processor_2400_dmem
#define HIVE_ADDR_ibuf_ctrl_master_ports 0x20C
#define HIVE_SIZE_ibuf_ctrl_master_ports 12
#else
#endif
#endif
#define HIVE_MEM_sp_ibuf_ctrl_master_ports scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ibuf_ctrl_master_ports 0x20C
#define HIVE_SIZE_sp_ibuf_ctrl_master_ports 12

/* function css_get_frame_processing_time_start: 27C4 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_frame
#define HIVE_MEM_sp_all_cbs_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_frame 0x5778
#define HIVE_SIZE_sp_all_cbs_frame 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_frame 0x5778
#define HIVE_SIZE_sp_sp_all_cbs_frame 16

/* function ia_css_virtual_isys_sp_isr: 6AD3 */

/* function thread_sp_queue_print: 1433 */

/* function sp_notify_eof: 971 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_str2mem
#define HIVE_MEM_sem_for_str2mem scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_str2mem 0x5788
#define HIVE_SIZE_sem_for_str2mem 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_str2mem scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_str2mem 0x5788
#define HIVE_SIZE_sp_sem_for_str2mem 20

/* function ia_css_tagger_buf_sp_is_marked_from_start: 31A6 */

/* function ia_css_bufq_sp_acquire_dynamic_buf: 3537 */

/* function ia_css_pipeline_sp_sfi_mode_is_enabled: 27B1 */

/* function ia_css_circbuf_destroy: 16B9 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ISP_PMEM_BASE
#define HIVE_MEM_ISP_PMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_ISP_PMEM_BASE 0xC
#define HIVE_SIZE_ISP_PMEM_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_ISP_PMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ISP_PMEM_BASE 0xC
#define HIVE_SIZE_sp_ISP_PMEM_BASE 4

/* function ia_css_sp_isp_param_mem_load: 4BAE */

/* function __div: 641F */

/* function ia_css_rmgr_sp_refcount_release_vbuf: 5F56 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_in_use
#define HIVE_MEM_ia_css_flash_sp_in_use scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_in_use 0x59F4
#define HIVE_SIZE_ia_css_flash_sp_in_use 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_in_use scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_in_use 0x59F4
#define HIVE_SIZE_sp_ia_css_flash_sp_in_use 4

/* function ia_css_thread_sem_sp_wait: 66E5 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_sleep_mode
#define HIVE_MEM_sp_sleep_mode scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sleep_mode 0x3D40
#define HIVE_SIZE_sp_sleep_mode 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_sleep_mode scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_sleep_mode 0x3D40
#define HIVE_SIZE_sp_sp_sleep_mode 4

/* function ia_css_tagger_buf_sp_push: 30BF */

/* function mmu_invalidate_cache: D3 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_max_cb_elems
#define HIVE_MEM_sp_max_cb_elems scalar_processor_2400_dmem
#define HIVE_ADDR_sp_max_cb_elems 0x14C
#define HIVE_SIZE_sp_max_cb_elems 8
#else
#endif
#endif
#define HIVE_MEM_sp_sp_max_cb_elems scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_max_cb_elems 0x14C
#define HIVE_SIZE_sp_sp_max_cb_elems 8

/* function ia_css_queue_remote_init: 50A5 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_stop_req
#define HIVE_MEM_isp_stop_req scalar_processor_2400_dmem
#define HIVE_ADDR_isp_stop_req 0x561C
#define HIVE_SIZE_isp_stop_req 4
#else
#endif
#endif
#define HIVE_MEM_sp_isp_stop_req scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_stop_req 0x561C
#define HIVE_SIZE_sp_isp_stop_req 4

/* function ia_css_pipeline_sp_sfi_request_next_frame: 2674 */

#define HIVE_ICACHE_sp_critical_SEGMENT_START 0
#define HIVE_ICACHE_sp_critical_NUM_SEGMENTS  1

#endif /* _sp_map_h_ */
extern void sh_css_dump_sp_dmem(void);
void sh_css_dump_sp_dmem(void)
{
}
