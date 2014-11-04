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

#ifndef _sp_map_h_
#define _sp_map_h_


#ifndef _hrt_dummy_use_blob_sp
#define _hrt_dummy_use_blob_sp()
#endif

#define _hrt_cell_load_program_sp(proc) _hrt_cell_load_program_embedded(proc, sp)

/* function input_system_acquisition_stop: AD2 */

/* function longjmp: 683C */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_raw_buff_pool_lock_frame_handles
#define HIVE_MEM_raw_buff_pool_lock_frame_handles scalar_processor_2400_dmem
#define HIVE_ADDR_raw_buff_pool_lock_frame_handles 0x6198
#define HIVE_SIZE_raw_buff_pool_lock_frame_handles 60
#else
#endif
#endif
#define HIVE_MEM_sp_raw_buff_pool_lock_frame_handles scalar_processor_2400_dmem
#define HIVE_ADDR_sp_raw_buff_pool_lock_frame_handles 0x6198
#define HIVE_SIZE_sp_raw_buff_pool_lock_frame_handles 60

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_HIVE_IF_SRST_MASK
#define HIVE_MEM_HIVE_IF_SRST_MASK scalar_processor_2400_dmem
#define HIVE_ADDR_HIVE_IF_SRST_MASK 0x1CC
#define HIVE_SIZE_HIVE_IF_SRST_MASK 16
#else
#endif
#endif
#define HIVE_MEM_sp_HIVE_IF_SRST_MASK scalar_processor_2400_dmem
#define HIVE_ADDR_sp_HIVE_IF_SRST_MASK 0x1CC
#define HIVE_SIZE_sp_HIVE_IF_SRST_MASK 16

/* function tmpmem_init_dmem: 65EF */

/* function ia_css_isys_sp_token_map_receive_ack: 5F04 */

/* function ia_css_dmaproxy_sp_set_addr_B: 3073 */

/* function debug_buffer_set_ddr_addr: F8 */

/* function receiver_port_reg_load: AB6 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_mipi
#define HIVE_MEM_vbuf_mipi scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_mipi 0x629C
#define HIVE_SIZE_vbuf_mipi 12
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_mipi scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_mipi 0x629C
#define HIVE_SIZE_sp_vbuf_mipi 12

/* function ia_css_event_sp_decode: 327F */

/* function ia_css_queue_get_size: 478E */

/* function ia_css_queue_load: 4E59 */

/* function setjmp: 6845 */

/* function __dmaproxy_sp_read_write_text: 318B */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_sp2host_isys_event_queue
#define HIVE_MEM_sem_for_sp2host_isys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_sp2host_isys_event_queue 0x4664
#define HIVE_SIZE_sem_for_sp2host_isys_event_queue 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_sp2host_isys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_sp2host_isys_event_queue 0x4664
#define HIVE_SIZE_sp_sem_for_sp2host_isys_event_queue 20

/* function ia_css_dmaproxy_sp_wait_for_ack: 6E6D */

/* function ia_css_sp_rawcopy_func: 50B8 */

/* function ia_css_tagger_buf_sp_pop_marked: 26E1 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_stage
#define HIVE_MEM_isp_stage scalar_processor_2400_dmem
#define HIVE_ADDR_isp_stage 0x5B10
#define HIVE_SIZE_isp_stage 824
#else
#endif
#endif
#define HIVE_MEM_sp_isp_stage scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_stage 0x5B10
#define HIVE_SIZE_sp_isp_stage 824

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_raw
#define HIVE_MEM_vbuf_raw scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_raw 0x2E0
#define HIVE_SIZE_vbuf_raw 4
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_raw scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_raw 0x2E0
#define HIVE_SIZE_sp_vbuf_raw 4

/* function ia_css_sp_bin_copy_func: 4FE0 */

/* function ia_css_queue_item_store: 4B2F */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_metadata_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_metadata_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_metadata_bufs 0x49D8
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_metadata_bufs 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_metadata_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_metadata_bufs 0x49D8
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_metadata_bufs 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_buffer_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_buffer_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_buffer_bufs 0x49EC
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_buffer_bufs 160
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_buffer_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_buffer_bufs 0x49EC
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_buffer_bufs 160

/* function sp_start_isp: 501 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_binary_group
#define HIVE_MEM_sp_binary_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_binary_group 0x5EF8
#define HIVE_SIZE_sp_binary_group 32
#else
#endif
#endif
#define HIVE_MEM_sp_sp_binary_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_binary_group 0x5EF8
#define HIVE_SIZE_sp_sp_binary_group 32

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_sw_state
#define HIVE_MEM_sp_sw_state scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sw_state 0x622C
#define HIVE_SIZE_sp_sw_state 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_sw_state scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_sw_state 0x622C
#define HIVE_SIZE_sp_sp_sw_state 4

/* function sp_error: 90A */

/* function ia_css_thread_sp_main: D76 */

/* function ia_css_ispctrl_sp_init_internal_buffers: 34C6 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp2host_psys_event_queue_handle
#define HIVE_MEM_sp2host_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp2host_psys_event_queue_handle 0x4A8C
#define HIVE_SIZE_sp2host_psys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_sp2host_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp2host_psys_event_queue_handle 0x4A8C
#define HIVE_SIZE_sp_sp2host_psys_event_queue_handle 12

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_sp2host_psys_event_queue
#define HIVE_MEM_sem_for_sp2host_psys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_sp2host_psys_event_queue 0x4678
#define HIVE_SIZE_sem_for_sp2host_psys_event_queue 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_sp2host_psys_event_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_sp2host_psys_event_queue 0x4678
#define HIVE_SIZE_sp_sem_for_sp2host_psys_event_queue 20

/* function ia_css_tagger_sp_propagate_frame: 20CA */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_stop_copy_preview
#define HIVE_MEM_sp_stop_copy_preview scalar_processor_2400_dmem
#define HIVE_ADDR_sp_stop_copy_preview 0x61D4
#define HIVE_SIZE_sp_stop_copy_preview 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_stop_copy_preview scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_stop_copy_preview 0x61D4
#define HIVE_SIZE_sp_sp_stop_copy_preview 4

/* function input_system_reg_load: B18 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_handles
#define HIVE_MEM_vbuf_handles scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_handles 0x62A8
#define HIVE_SIZE_vbuf_handles 960
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_handles scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_handles 0x62A8
#define HIVE_SIZE_sp_vbuf_handles 960

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_max_mm_tracer
#define HIVE_MEM_max_mm_tracer scalar_processor_2400_dmem
#define HIVE_ADDR_max_mm_tracer 0x1B8
#define HIVE_SIZE_max_mm_tracer 4
#else
#endif
#endif
#define HIVE_MEM_sp_max_mm_tracer scalar_processor_2400_dmem
#define HIVE_ADDR_sp_max_mm_tracer 0x1B8
#define HIVE_SIZE_sp_max_mm_tracer 4

/* function ia_css_queue_store: 4CD2 */

/* function ia_css_sp_flash_register: 28BB */

/* function ia_css_isys_sp_backend_create: 5B5B */

/* function ia_css_pipeline_sp_init: 17B9 */

/* function ia_css_tagger_sp_configure: 1FAF */

/* function ia_css_ispctrl_sp_end_binary: 32C9 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs
#define HIVE_MEM_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 0x4A98
#define HIVE_SIZE_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 0x4A98
#define HIVE_SIZE_sp_ia_css_bufq_sp_h_pipe_private_per_frame_ddr_ptrs 20

/* function receiver_port_reg_store: ABD */

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
#define HIVE_ADDR_sp_all_cb_elems_frame 0x468C
#define HIVE_SIZE_sp_all_cb_elems_frame 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cb_elems_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cb_elems_frame 0x468C
#define HIVE_SIZE_sp_sp_all_cb_elems_frame 16

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp2host_isys_event_queue_handle
#define HIVE_MEM_sp2host_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp2host_isys_event_queue_handle 0x4AAC
#define HIVE_SIZE_sp2host_isys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_sp2host_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp2host_isys_event_queue_handle 0x4AAC
#define HIVE_SIZE_sp_sp2host_isys_event_queue_handle 12

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host_sp_com
#define HIVE_MEM_host_sp_com scalar_processor_2400_dmem
#define HIVE_ADDR_host_sp_com 0x4130
#define HIVE_SIZE_host_sp_com 220
#else
#endif
#endif
#define HIVE_MEM_sp_host_sp_com scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host_sp_com 0x4130
#define HIVE_SIZE_sp_host_sp_com 220

/* function ia_css_queue_get_free_space: 48E6 */

/* function exec_image_pipe: 718 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_init_dmem_data
#define HIVE_MEM_sp_init_dmem_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_init_dmem_data 0x6230
#define HIVE_SIZE_sp_init_dmem_data 24
#else
#endif
#endif
#define HIVE_MEM_sp_sp_init_dmem_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_init_dmem_data 0x6230
#define HIVE_SIZE_sp_sp_init_dmem_data 24

/* function ia_css_sp_metadata_start: 5977 */

/* function ia_css_tagger_buf_sp_is_marked: 2808 */

/* function ia_css_bufq_sp_init_buffer_queues: 293A */

/* function ia_css_pipeline_sp_stop: 179C */

/* function ia_css_tagger_sp_connect_pipes: 24DB */

/* function sp_isys_copy_wait: 761 */

/* function is_isp_debug_buffer_full: 3BC */

/* function ia_css_dmaproxy_sp_configure_channel_from_info: 2FF0 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_per_frame_data
#define HIVE_MEM_sp_per_frame_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_per_frame_data 0x420C
#define HIVE_SIZE_sp_per_frame_data 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_per_frame_data scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_per_frame_data 0x420C
#define HIVE_SIZE_sp_sp_per_frame_data 4

/* function ia_css_rmgr_sp_vbuf_dequeue: 633F */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_psys_event_queue_handle
#define HIVE_MEM_host2sp_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_psys_event_queue_handle 0x4AB8
#define HIVE_SIZE_host2sp_psys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_psys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_psys_event_queue_handle 0x4AB8
#define HIVE_SIZE_sp_host2sp_psys_event_queue_handle 12

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_xmem_bin_addr
#define HIVE_MEM_xmem_bin_addr scalar_processor_2400_dmem
#define HIVE_ADDR_xmem_bin_addr 0x4210
#define HIVE_SIZE_xmem_bin_addr 4
#else
#endif
#endif
#define HIVE_MEM_sp_xmem_bin_addr scalar_processor_2400_dmem
#define HIVE_ADDR_sp_xmem_bin_addr 0x4210
#define HIVE_SIZE_sp_xmem_bin_addr 4

/* function tmr_clock_init: AA0 */

/* function ia_css_sp_rawcopy_unlock_buffer: 5610 */

/* function ia_css_pipeline_sp_run: 142E */

/* function memcpy: 68E5 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_GP_DEVICE_BASE
#define HIVE_MEM_GP_DEVICE_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_GP_DEVICE_BASE 0x2EC
#define HIVE_SIZE_GP_DEVICE_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_GP_DEVICE_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_GP_DEVICE_BASE 0x2EC
#define HIVE_SIZE_sp_GP_DEVICE_BASE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_thread_sp_ready_queue
#define HIVE_MEM_ia_css_thread_sp_ready_queue scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_thread_sp_ready_queue 0x1E4
#define HIVE_SIZE_ia_css_thread_sp_ready_queue 12
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_thread_sp_ready_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_thread_sp_ready_queue 0x1E4
#define HIVE_SIZE_sp_ia_css_thread_sp_ready_queue 12

/* function input_system_reg_store: B1F */

/* function sp_dma_proxy_set_width_ab: 2E8D */

/* function ia_css_isys_sp_frontend_start: 5D85 */

/* function ia_css_uds_sp_scale_params: 6609 */

/* function ia_css_circbuf_increase_size: E5B */

/* function __divu: 6863 */

/* function ia_css_thread_sp_get_state: C9E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_cont_capt_stop
#define HIVE_MEM_sem_for_cont_capt_stop scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_cont_capt_stop 0x469C
#define HIVE_SIZE_sem_for_cont_capt_stop 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_cont_capt_stop scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_cont_capt_stop 0x469C
#define HIVE_SIZE_sp_sem_for_cont_capt_stop 20

/* function thread_fiber_sp_main: E54 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_isp_pipe_thread
#define HIVE_MEM_sp_isp_pipe_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_pipe_thread 0x47E0
#define HIVE_SIZE_sp_isp_pipe_thread 340
#else
#endif
#endif
#define HIVE_MEM_sp_sp_isp_pipe_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_isp_pipe_thread 0x47E0
#define HIVE_SIZE_sp_sp_isp_pipe_thread 340

/* function ia_css_parambuf_sp_handle_parameter_sets: 12D1 */

/* function ia_css_spctrl_sp_set_state: 59A6 */

/* function ia_css_thread_sem_sp_signal: 6ABB */

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

/* function ia_css_isys_sp_isr: 701E */

/* function ia_css_isys_sp_generate_exp_id: 6137 */

/* function ia_css_rmgr_sp_init: 622E */

/* function ia_css_thread_sem_sp_init: 6B8E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_is_isp_requested
#define HIVE_MEM_is_isp_requested scalar_processor_2400_dmem
#define HIVE_ADDR_is_isp_requested 0x2F8
#define HIVE_SIZE_is_isp_requested 4
#else
#endif
#endif
#define HIVE_MEM_sp_is_isp_requested scalar_processor_2400_dmem
#define HIVE_ADDR_sp_is_isp_requested 0x2F8
#define HIVE_SIZE_sp_is_isp_requested 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_reading_cb_frame
#define HIVE_MEM_sem_for_reading_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_reading_cb_frame 0x46B0
#define HIVE_SIZE_sem_for_reading_cb_frame 40
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_reading_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_reading_cb_frame 0x46B0
#define HIVE_SIZE_sp_sem_for_reading_cb_frame 40

/* function ia_css_dmaproxy_sp_execute: 2F48 */

/* function ia_css_queue_is_empty: 47C9 */

/* function ia_css_pipeline_sp_has_stopped: 1792 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_verbosity
#define HIVE_MEM_verbosity scalar_processor_2400_dmem
#define HIVE_ADDR_verbosity 0x2680
#define HIVE_SIZE_verbosity 4
#else
#endif
#endif
#define HIVE_MEM_sp_verbosity scalar_processor_2400_dmem
#define HIVE_ADDR_sp_verbosity 0x2680
#define HIVE_SIZE_sp_verbosity 4

/* function ia_css_circbuf_extract: F69 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_current_sp_thread
#define HIVE_MEM_current_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_current_sp_thread 0x1E0
#define HIVE_SIZE_current_sp_thread 4
#else
#endif
#endif
#define HIVE_MEM_sp_current_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_current_sp_thread 0x1E0
#define HIVE_SIZE_sp_current_sp_thread 4

/* function ia_css_spctrl_sp_get_spid: 59AD */

/* function ia_css_dmaproxy_sp_read_byte_addr: 6E9E */

/* function ia_css_rmgr_sp_uninit: 6227 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_threads_stack
#define HIVE_MEM_sp_threads_stack scalar_processor_2400_dmem
#define HIVE_ADDR_sp_threads_stack 0x15C
#define HIVE_SIZE_sp_threads_stack 28
#else
#endif
#endif
#define HIVE_MEM_sp_sp_threads_stack scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_threads_stack 0x15C
#define HIVE_SIZE_sp_sp_threads_stack 28

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_trace_buffer_sp
#define HIVE_MEM_trace_buffer_sp scalar_processor_2400_dmem
#define HIVE_ADDR_trace_buffer_sp 0x1B4
#define HIVE_SIZE_trace_buffer_sp 4
#else
#endif
#endif
#define HIVE_MEM_sp_trace_buffer_sp scalar_processor_2400_dmem
#define HIVE_ADDR_sp_trace_buffer_sp 0x1B4
#define HIVE_SIZE_sp_trace_buffer_sp 4

/* function ia_css_circbuf_peek: F48 */

/* function ia_css_parambuf_sp_wait_for_in_param: 107F */

/* function ia_css_isys_sp_token_map_get_exp_id: 5FF2 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cb_elems_param
#define HIVE_MEM_sp_all_cb_elems_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cb_elems_param 0x46D8
#define HIVE_SIZE_sp_all_cb_elems_param 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cb_elems_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cb_elems_param 0x46D8
#define HIVE_SIZE_sp_sp_all_cb_elems_param 16

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_pipeline_sp_curr_binary_id
#define HIVE_MEM_pipeline_sp_curr_binary_id scalar_processor_2400_dmem
#define HIVE_ADDR_pipeline_sp_curr_binary_id 0x1F0
#define HIVE_SIZE_pipeline_sp_curr_binary_id 4
#else
#endif
#endif
#define HIVE_MEM_sp_pipeline_sp_curr_binary_id scalar_processor_2400_dmem
#define HIVE_ADDR_sp_pipeline_sp_curr_binary_id 0x1F0
#define HIVE_SIZE_sp_pipeline_sp_curr_binary_id 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_frame_desc
#define HIVE_MEM_sp_all_cbs_frame_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_frame_desc 0x46E8
#define HIVE_SIZE_sp_all_cbs_frame_desc 8
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_frame_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_frame_desc 0x46E8
#define HIVE_SIZE_sp_sp_all_cbs_frame_desc 8

/* function sp_isys_copy_func_v2: 75A */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_reading_cb_param
#define HIVE_MEM_sem_for_reading_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_reading_cb_param 0x46F0
#define HIVE_SIZE_sem_for_reading_cb_param 40
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_reading_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_reading_cb_param 0x46F0
#define HIVE_SIZE_sp_sem_for_reading_cb_param 40

/* function ia_css_queue_get_used_space: 4898 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_cont_capt_start
#define HIVE_MEM_sem_for_cont_capt_start scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_cont_capt_start 0x4718
#define HIVE_SIZE_sem_for_cont_capt_start 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_cont_capt_start scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_cont_capt_start 0x4718
#define HIVE_SIZE_sp_sem_for_cont_capt_start 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_tmp_heap
#define HIVE_MEM_tmp_heap scalar_processor_2400_dmem
#define HIVE_ADDR_tmp_heap 0x5F18
#define HIVE_SIZE_tmp_heap 640
#else
#endif
#endif
#define HIVE_MEM_sp_tmp_heap scalar_processor_2400_dmem
#define HIVE_ADDR_sp_tmp_heap 0x5F18
#define HIVE_SIZE_sp_tmp_heap 640

/* function ia_css_tagger_buf_sp_mark: 2894 */

/* function ia_css_rmgr_sp_get_num_vbuf: 6544 */

/* function ia_css_ispctrl_sp_output_compute_dma_info: 3CDE */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_s3a_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_s3a_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_s3a_bufs 0x4AC4
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_s3a_bufs 60
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_s3a_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_s3a_bufs 0x4AC4
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_s3a_bufs 60

/* function ia_css_queue_is_full: 4935 */

/* function debug_buffer_init_isp: 105 */

/* function ia_css_isys_sp_frontend_uninit: 5D36 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_rmgr_sp_mipi_frame_sem
#define HIVE_MEM_ia_css_rmgr_sp_mipi_frame_sem scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_rmgr_sp_mipi_frame_sem 0x6668
#define HIVE_SIZE_ia_css_rmgr_sp_mipi_frame_sem 60
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_rmgr_sp_mipi_frame_sem scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_rmgr_sp_mipi_frame_sem 0x6668
#define HIVE_SIZE_sp_ia_css_rmgr_sp_mipi_frame_sem 60

/* function ia_css_rmgr_sp_refcount_dump: 631A */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_isp_parameters_id
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_isp_parameters_id scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_isp_parameters_id 0x4B00
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_isp_parameters_id 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_isp_parameters_id scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_isp_parameters_id 0x4B00
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_isp_parameters_id 20

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_pipe_threads
#define HIVE_MEM_sp_pipe_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_pipe_threads 0x148
#define HIVE_SIZE_sp_pipe_threads 20
#else
#endif
#endif
#define HIVE_MEM_sp_sp_pipe_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_pipe_threads 0x148
#define HIVE_SIZE_sp_sp_pipe_threads 20

/* function sp_event_proxy_func: 76F */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_isys_event_queue_handle
#define HIVE_MEM_host2sp_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_isys_event_queue_handle 0x4B14
#define HIVE_SIZE_host2sp_isys_event_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_isys_event_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_isys_event_queue_handle 0x4B14
#define HIVE_SIZE_sp_host2sp_isys_event_queue_handle 12

/* function ia_css_thread_sp_yield: 6A31 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_param_desc
#define HIVE_MEM_sp_all_cbs_param_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_param_desc 0x472C
#define HIVE_SIZE_sp_all_cbs_param_desc 8
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_param_desc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_param_desc 0x472C
#define HIVE_SIZE_sp_sp_all_cbs_param_desc 8

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_dmaproxy_sp_invalidate_tlb
#define HIVE_MEM_ia_css_dmaproxy_sp_invalidate_tlb scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb 0x5B04
#define HIVE_SIZE_ia_css_dmaproxy_sp_invalidate_tlb 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_dmaproxy_sp_invalidate_tlb scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_dmaproxy_sp_invalidate_tlb 0x5B04
#define HIVE_SIZE_sp_ia_css_dmaproxy_sp_invalidate_tlb 4

/* function ia_css_thread_sp_fork: D2B */

/* function ia_css_tagger_sp_destroy: 24E5 */

/* function ia_css_dmaproxy_sp_vmem_read: 2ED1 */

/* function ia_css_ifmtr_sp_init: 6185 */

/* function initialize_sp_group: 728 */

/* function ia_css_thread_sp_init: D57 */

/* function ia_css_ispctrl_sp_set_stream_base_addr: 44D6 */

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

/* function ia_css_dmaproxy_sp_read: 2F69 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_raw_copy_line_count
#define HIVE_MEM_raw_copy_line_count scalar_processor_2400_dmem
#define HIVE_ADDR_raw_copy_line_count 0x2B4
#define HIVE_SIZE_raw_copy_line_count 4
#else
#endif
#endif
#define HIVE_MEM_sp_raw_copy_line_count scalar_processor_2400_dmem
#define HIVE_ADDR_sp_raw_copy_line_count 0x2B4
#define HIVE_SIZE_sp_raw_copy_line_count 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_tag_cmd_queue_handle
#define HIVE_MEM_host2sp_tag_cmd_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_tag_cmd_queue_handle 0x4B20
#define HIVE_SIZE_host2sp_tag_cmd_queue_handle 12
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_tag_cmd_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_tag_cmd_queue_handle 0x4B20
#define HIVE_SIZE_sp_host2sp_tag_cmd_queue_handle 12

/* function ia_css_queue_peek: 480D */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_frame_cnt
#define HIVE_MEM_ia_css_flash_sp_frame_cnt scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_frame_cnt 0x49CC
#define HIVE_SIZE_ia_css_flash_sp_frame_cnt 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_frame_cnt scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_frame_cnt 0x49CC
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

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_started
#define HIVE_MEM_started scalar_processor_2400_dmem
#define HIVE_ADDR_started 0x267C
#define HIVE_SIZE_started 4
#else
#endif
#endif
#define HIVE_MEM_sp_started scalar_processor_2400_dmem
#define HIVE_ADDR_sp_started 0x267C
#define HIVE_SIZE_sp_started 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_thread
#define HIVE_MEM_isp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_isp_thread 0x5E48
#define HIVE_SIZE_isp_thread 4
#else
#endif
#endif
#define HIVE_MEM_sp_isp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_thread 0x5E48
#define HIVE_SIZE_sp_isp_thread 4

/* function encode_and_post_sp_event_non_blocking: A57 */

/* function ia_css_isys_sp_frontend_destroy: 5E1E */

/* function is_ddr_debug_buffer_full: 33E */

/* function ia_css_isys_sp_frontend_stop: 5D4E */

/* function ia_css_isys_sp_token_map_init: 60D2 */

/* function sp_dma_proxy_isp_write_addr: 2EE9 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_threads_fiber
#define HIVE_MEM_sp_threads_fiber scalar_processor_2400_dmem
#define HIVE_ADDR_sp_threads_fiber 0x194
#define HIVE_SIZE_sp_threads_fiber 28
#else
#endif
#endif
#define HIVE_MEM_sp_sp_threads_fiber scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_threads_fiber 0x194
#define HIVE_SIZE_sp_sp_threads_fiber 28

/* function encode_and_post_sp_event: A28 */

/* function debug_enqueue_ddr: 114 */

/* function ia_css_rmgr_sp_refcount_init_vbuf: 62D5 */

/* function dmaproxy_sp_read_write: 6F49 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_dmaproxy_isp_dma_cmd_buffer
#define HIVE_MEM_ia_css_dmaproxy_isp_dma_cmd_buffer scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_dmaproxy_isp_dma_cmd_buffer 0x5B08
#define HIVE_SIZE_ia_css_dmaproxy_isp_dma_cmd_buffer 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_dmaproxy_isp_dma_cmd_buffer scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_dmaproxy_isp_dma_cmd_buffer 0x5B08
#define HIVE_SIZE_sp_ia_css_dmaproxy_isp_dma_cmd_buffer 4

/* function ia_css_dmaproxy_sp_ack: 6BB6 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host2sp_buffer_queue_handle
#define HIVE_MEM_host2sp_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_host2sp_buffer_queue_handle 0x4B2C
#define HIVE_SIZE_host2sp_buffer_queue_handle 480
#else
#endif
#endif
#define HIVE_MEM_sp_host2sp_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host2sp_buffer_queue_handle 0x4B2C
#define HIVE_SIZE_sp_host2sp_buffer_queue_handle 480

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_in_service
#define HIVE_MEM_ia_css_flash_sp_in_service scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_in_service 0x3214
#define HIVE_SIZE_ia_css_flash_sp_in_service 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_in_service scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_in_service 0x3214
#define HIVE_SIZE_sp_ia_css_flash_sp_in_service 4

/* function ia_css_dmaproxy_sp_process: 6BE5 */

/* function ia_css_isys_sp_backend_rcv_acquire_ack: 5A0A */

/* function ia_css_isys_sp_backend_pre_acquire_request: 5A20 */

/* function ia_css_ispctrl_sp_init_cs: 33CF */

/* function ia_css_spctrl_sp_init: 59BB */

/* function sp_event_proxy_init: 784 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_output
#define HIVE_MEM_sp_output scalar_processor_2400_dmem
#define HIVE_ADDR_sp_output 0x4214
#define HIVE_SIZE_sp_output 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_output scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_output 0x4214
#define HIVE_SIZE_sp_sp_output 16

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_sems_for_host2sp_buf_queues
#define HIVE_MEM_ia_css_bufq_sp_sems_for_host2sp_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_sems_for_host2sp_buf_queues 0x4D0C
#define HIVE_SIZE_ia_css_bufq_sp_sems_for_host2sp_buf_queues 800
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_sems_for_host2sp_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_sems_for_host2sp_buf_queues 0x4D0C
#define HIVE_SIZE_sp_ia_css_bufq_sp_sems_for_host2sp_buf_queues 800

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

/* function sp_dma_proxy_reset_channels: 31E3 */

/* function ia_css_bufq_set_clock_tick: 2932 */

/* function ia_css_isys_sp_backend_acquire: 5B31 */

/* function ia_css_tagger_sp_update_size: 25E7 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_host_sp_queue
#define HIVE_MEM_ia_css_bufq_host_sp_queue scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_host_sp_queue 0x502C
#define HIVE_SIZE_ia_css_bufq_host_sp_queue 2008
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_host_sp_queue scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_host_sp_queue 0x502C
#define HIVE_SIZE_sp_ia_css_bufq_host_sp_queue 2008

/* function thread_fiber_sp_create: DC3 */

/* function ia_css_dmaproxy_sp_set_increments: 305E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_writing_cb_frame
#define HIVE_MEM_sem_for_writing_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_writing_cb_frame 0x4734
#define HIVE_SIZE_sem_for_writing_cb_frame 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_writing_cb_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_writing_cb_frame 0x4734
#define HIVE_SIZE_sp_sem_for_writing_cb_frame 20

/* function receiver_reg_store: ACB */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_writing_cb_param
#define HIVE_MEM_sem_for_writing_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_writing_cb_param 0x4748
#define HIVE_SIZE_sem_for_writing_cb_param 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_writing_cb_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_writing_cb_param 0x4748
#define HIVE_SIZE_sp_sem_for_writing_cb_param 20

/* function sp_start_isp_entry: 4F7 */
#ifndef HIVE_MULTIPLE_PROGRAMS
#ifdef HIVE_ADDR_sp_start_isp_entry
#endif
#define HIVE_ADDR_sp_start_isp_entry 0x4F7
#endif
#define HIVE_ADDR_sp_sp_start_isp_entry 0x4F7

/* function ia_css_tagger_buf_sp_unmark_all: 282C */

/* function ia_css_dmaproxy_sp_channel_acquire: 3212 */

/* function ia_css_rmgr_sp_add_num_vbuf: 6520 */

/* function ia_css_isys_sp_token_map_create: 611E */

/* function __ia_css_dmaproxy_sp_wait_for_ack_text: 2E84 */

/* function ia_css_tagger_buf_sp_push_marked: 2756 */

/* function ia_css_bufq_sp_is_dynamic_buffer: 2CB1 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_group
#define HIVE_MEM_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_group 0x4224
#define HIVE_SIZE_sp_group 1084
#else
#endif
#endif
#define HIVE_MEM_sp_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_group 0x4224
#define HIVE_SIZE_sp_sp_group 1084

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_event_proxy_thread
#define HIVE_MEM_sp_event_proxy_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_event_proxy_thread 0x4934
#define HIVE_SIZE_sp_event_proxy_thread 68
#else
#endif
#endif
#define HIVE_MEM_sp_sp_event_proxy_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_event_proxy_thread 0x4934
#define HIVE_SIZE_sp_sp_event_proxy_thread 68

/* function ia_css_thread_sp_kill: CF1 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_cur_loc
#define HIVE_MEM_cur_loc scalar_processor_2400_dmem
#define HIVE_ADDR_cur_loc 0x2674
#define HIVE_SIZE_cur_loc 4
#else
#endif
#endif
#define HIVE_MEM_sp_cur_loc scalar_processor_2400_dmem
#define HIVE_ADDR_sp_cur_loc 0x2674
#define HIVE_SIZE_sp_cur_loc 4

/* function ia_css_tagger_sp_create: 259E */

/* function tmpmem_acquire_dmem: 65D0 */

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

/* function ia_css_dmaproxy_sp_channel_release: 31FB */

/* function ia_css_dmaproxy_sp_is_idle: 31CE */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_qos_start
#define HIVE_MEM_sem_for_qos_start scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_qos_start 0x475C
#define HIVE_SIZE_sem_for_qos_start 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_qos_start scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_qos_start 0x475C
#define HIVE_SIZE_sp_sem_for_qos_start 20

/* function isp_hmem_load: B5F */

/* function ia_css_eventq_sp_send: 3257 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_isys_sp_error_cnt
#define HIVE_MEM_ia_css_isys_sp_error_cnt scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_isys_sp_error_cnt 0x6254
#define HIVE_SIZE_ia_css_isys_sp_error_cnt 16
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_isys_sp_error_cnt scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_isys_sp_error_cnt 0x6254
#define HIVE_SIZE_sp_ia_css_isys_sp_error_cnt 16

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

/* function sp_isys_copy_request: 768 */

/* function ia_css_rmgr_sp_refcount_retain_vbuf: 63AF */

/* function ia_css_thread_sp_set_priority: CE9 */

/* function sizeof_hmem: C15 */

/* function tmpmem_release_dmem: 65BD */

/* function cnd_input_system_cfg: 419 */

/* function __ia_css_sp_rawcopy_func_critical: 6FA7 */

/* function __ia_css_dmaproxy_sp_process_text: 2DC0 */

/* function ia_css_dmaproxy_sp_set_width_exception: 3048 */

/* function ia_css_flash_sp_init_internal_params: 2927 */

/* function ia_css_tagger_buf_sp_pop_unmarked_and_unlocked: 260C */

/* function __modu: 68A9 */

/* function ia_css_dmaproxy_sp_init_isp_vector: 2EA3 */

/* function isp_vamem_store: 0 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_counter
#define HIVE_MEM_counter scalar_processor_2400_dmem
#define HIVE_ADDR_counter 0x2678
#define HIVE_SIZE_counter 2
#else
#endif
#endif
#define HIVE_MEM_sp_counter scalar_processor_2400_dmem
#define HIVE_ADDR_sp_counter 0x2678
#define HIVE_SIZE_sp_counter 2

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_major_masks
#define HIVE_MEM_major_masks scalar_processor_2400_dmem
#define HIVE_ADDR_major_masks 0x1B0
#define HIVE_SIZE_major_masks 4
#else
#endif
#endif
#define HIVE_MEM_sp_major_masks scalar_processor_2400_dmem
#define HIVE_ADDR_sp_major_masks 0x1B0
#define HIVE_SIZE_sp_major_masks 4

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

/* function ia_css_queue_local_init: 4B09 */

/* function sp_event_proxy_callout_func: 693B */

/* function ia_css_dmaproxy_sp_deregister_channel_from_port: 2E6B */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_thread_sp_num_ready_threads
#define HIVE_MEM_ia_css_thread_sp_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_thread_sp_num_ready_threads 0x49C0
#define HIVE_SIZE_ia_css_thread_sp_num_ready_threads 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_thread_sp_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_thread_sp_num_ready_threads 0x49C0
#define HIVE_SIZE_sp_ia_css_thread_sp_num_ready_threads 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_threads_stack_size
#define HIVE_MEM_sp_threads_stack_size scalar_processor_2400_dmem
#define HIVE_ADDR_sp_threads_stack_size 0x178
#define HIVE_SIZE_sp_threads_stack_size 28
#else
#endif
#endif
#define HIVE_MEM_sp_sp_threads_stack_size scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_threads_stack_size 0x178
#define HIVE_SIZE_sp_sp_threads_stack_size 28

/* function ia_css_ispctrl_sp_isp_done_row_striping: 3CC4 */

/* function __ia_css_isys_sp_isr_text: 5E49 */

/* function ia_css_queue_dequeue: 497D */

/* function ia_css_dmaproxy_sp_configure_channel: 6EB5 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_current_thread_fiber_sp
#define HIVE_MEM_current_thread_fiber_sp scalar_processor_2400_dmem
#define HIVE_ADDR_current_thread_fiber_sp 0x49C8
#define HIVE_SIZE_current_thread_fiber_sp 4
#else
#endif
#endif
#define HIVE_MEM_sp_current_thread_fiber_sp scalar_processor_2400_dmem
#define HIVE_ADDR_sp_current_thread_fiber_sp 0x49C8
#define HIVE_SIZE_sp_current_thread_fiber_sp 4

/* function ia_css_circbuf_pop: 1001 */

/* function irq_raise_set_token: C1 */

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

/* function ia_css_pipeline_acc_stage_enable: 1772 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_ph
#define HIVE_MEM_isp_ph scalar_processor_2400_dmem
#define HIVE_ADDR_isp_ph 0x6264
#define HIVE_SIZE_isp_ph 28
#else
#endif
#endif
#define HIVE_MEM_sp_isp_ph scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_ph 0x6264
#define HIVE_SIZE_sp_isp_ph 28

/* function ia_css_isys_sp_token_map_flush: 605B */

/* function ia_css_ispctrl_sp_init_ds: 356D */

/* function get_xmem_base_addr_raw: 3927 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_param
#define HIVE_MEM_sp_all_cbs_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_param 0x4770
#define HIVE_SIZE_sp_all_cbs_param 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_param scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_param 0x4770
#define HIVE_SIZE_sp_sp_all_cbs_param 16

/* function ia_css_circbuf_create: 1052 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_sp_group
#define HIVE_MEM_sem_for_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_sp_group 0x4780
#define HIVE_SIZE_sem_for_sp_group 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_sp_group scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_sp_group 0x4780
#define HIVE_SIZE_sp_sem_for_sp_group 20

/* function ia_css_framebuf_sp_wait_for_in_frame: 654B */

/* function ia_css_tagger_buf_sp_push_unmarked: 267F */

/* function isp_hmem_clear: B26 */

/* function ia_css_framebuf_sp_release_in_frame: 658E */

/* function ia_css_isys_sp_backend_snd_acquire_request: 5A83 */

/* function ia_css_isys_sp_token_map_is_full: 5ED0 */

/* function input_system_acquisition_run: AF4 */

/* function ia_css_ispctrl_sp_start_binary: 33AD */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_h_pipe_private_ddr_ptrs
#define HIVE_MEM_ia_css_bufq_sp_h_pipe_private_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 0x5804
#define HIVE_SIZE_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 20
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_h_pipe_private_ddr_ptrs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 0x5804
#define HIVE_SIZE_sp_ia_css_bufq_sp_h_pipe_private_ddr_ptrs 20

/* function ia_css_eventq_sp_recv: 3229 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_pool
#define HIVE_MEM_isp_pool scalar_processor_2400_dmem
#define HIVE_ADDR_isp_pool 0x2D4
#define HIVE_SIZE_isp_pool 4
#else
#endif
#endif
#define HIVE_MEM_sp_isp_pool scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_pool 0x2D4
#define HIVE_SIZE_sp_isp_pool 4

/* function ia_css_rmgr_sp_rel_gen: 6270 */

/* function css_get_frame_processing_time_end: 1F41 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_event_any_pending_mask
#define HIVE_MEM_event_any_pending_mask scalar_processor_2400_dmem
#define HIVE_ADDR_event_any_pending_mask 0x2F0
#define HIVE_SIZE_event_any_pending_mask 8
#else
#endif
#endif
#define HIVE_MEM_sp_event_any_pending_mask scalar_processor_2400_dmem
#define HIVE_ADDR_sp_event_any_pending_mask 0x2F0
#define HIVE_SIZE_sp_event_any_pending_mask 8

/* function ia_css_isys_sp_backend_push: 5A34 */

/* function sh_css_decode_tag_descr: 3D7 */

/* function debug_enqueue_isp: 2E9 */

/* function ia_css_spctrl_sp_uninit: 59B4 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_HIVE_IF_SWITCH_CODE
#define HIVE_MEM_HIVE_IF_SWITCH_CODE scalar_processor_2400_dmem
#define HIVE_ADDR_HIVE_IF_SWITCH_CODE 0x1DC
#define HIVE_SIZE_HIVE_IF_SWITCH_CODE 4
#else
#endif
#endif
#define HIVE_MEM_sp_HIVE_IF_SWITCH_CODE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_HIVE_IF_SWITCH_CODE 0x1DC
#define HIVE_SIZE_sp_HIVE_IF_SWITCH_CODE 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_dis_bufs
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_dis_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_dis_bufs 0x5818
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_dis_bufs 140
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_dis_bufs scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_dis_bufs 0x5818
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_dis_bufs 140

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_isp_idle
#define HIVE_MEM_sem_for_isp_idle scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_isp_idle 0x4794
#define HIVE_SIZE_sem_for_isp_idle 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_isp_idle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_isp_idle 0x4794
#define HIVE_SIZE_sp_sem_for_isp_idle 20

/* function ia_css_dmaproxy_sp_write_byte_addr: 2F17 */

/* function ia_css_dmaproxy_sp_init: 2E3D */

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

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_rawcopy_sp_tagger
#define HIVE_MEM_ia_css_rawcopy_sp_tagger scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_rawcopy_sp_tagger 0x61D8
#define HIVE_SIZE_ia_css_rawcopy_sp_tagger 24
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_rawcopy_sp_tagger scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_rawcopy_sp_tagger 0x61D8
#define HIVE_SIZE_sp_ia_css_rawcopy_sp_tagger 24

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_pipe_private_exp_ids
#define HIVE_MEM_ia_css_bufq_sp_pipe_private_exp_ids scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_exp_ids 0x58A4
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_exp_ids 70
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_exp_ids scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_exp_ids 0x58A4
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_exp_ids 70

/* function ia_css_queue_item_load: 4C1E */

/* function ia_css_spctrl_sp_get_state: 599F */

/* function ia_css_isys_sp_token_map_uninit: 607A */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_callout_sp_thread
#define HIVE_MEM_callout_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_callout_sp_thread 0x49BC
#define HIVE_SIZE_callout_sp_thread 4
#else
#endif
#endif
#define HIVE_MEM_sp_callout_sp_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_callout_sp_thread 0x49BC
#define HIVE_SIZE_sp_callout_sp_thread 4

/* function thread_fiber_sp_init: E4A */

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

/* function ia_css_isys_sp_token_map_snd_acquire_req: 5FD9 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_isp_input_stream_format
#define HIVE_MEM_sp_isp_input_stream_format scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_input_stream_format 0x3FF8
#define HIVE_SIZE_sp_isp_input_stream_format 20
#else
#endif
#endif
#define HIVE_MEM_sp_sp_isp_input_stream_format scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_isp_input_stream_format 0x3FF8
#define HIVE_SIZE_sp_sp_isp_input_stream_format 20

/* function __mod: 6895 */

/* function ia_css_dmaproxy_sp_init_dmem_channel: 2F83 */

/* function ia_css_thread_sp_join: D1A */

/* function ia_css_dmaproxy_sp_add_command: 6F8B */

/* function ia_css_sp_metadata_thread_func: 5815 */

/* function __sp_event_proxy_func_critical: 6928 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_dmaproxy_sp_proxy_status
#define HIVE_MEM_dmaproxy_sp_proxy_status scalar_processor_2400_dmem
#define HIVE_ADDR_dmaproxy_sp_proxy_status 0x208
#define HIVE_SIZE_dmaproxy_sp_proxy_status 4
#else
#endif
#endif
#define HIVE_MEM_sp_dmaproxy_sp_proxy_status scalar_processor_2400_dmem
#define HIVE_ADDR_sp_dmaproxy_sp_proxy_status 0x208
#define HIVE_SIZE_sp_dmaproxy_sp_proxy_status 4

/* function ia_css_sp_metadata_wait: 5966 */

/* function ia_css_circbuf_peek_from_start: F27 */

/* function ia_css_sp_rawcopy_buffer_is_locked: 56B0 */

/* function ia_css_event_sp_encode: 32B5 */

/* function ia_css_thread_sp_run: D8D */

/* function sp_isys_copy_func: 74A */

/* function ia_css_isys_sp_backend_flush: 5AA3 */

/* function ia_css_sp_isp_param_init_isp_memories: 4658 */

/* function register_isr: 8CE */

/* function irq_raise: D3 */

/* function ia_css_dmaproxy_sp_mmu_invalidate: 2D7B */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_HIVE_IF_SRST_ADDRESS
#define HIVE_MEM_HIVE_IF_SRST_ADDRESS scalar_processor_2400_dmem
#define HIVE_ADDR_HIVE_IF_SRST_ADDRESS 0x1BC
#define HIVE_SIZE_HIVE_IF_SRST_ADDRESS 16
#else
#endif
#endif
#define HIVE_MEM_sp_HIVE_IF_SRST_ADDRESS scalar_processor_2400_dmem
#define HIVE_ADDR_sp_HIVE_IF_SRST_ADDRESS 0x1BC
#define HIVE_SIZE_sp_HIVE_IF_SRST_ADDRESS 16

/* function pipeline_sp_initialize_stage: 1891 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_isys_sp_frontend_states
#define HIVE_MEM_ia_css_isys_sp_frontend_states scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_isys_sp_frontend_states 0x6248
#define HIVE_SIZE_ia_css_isys_sp_frontend_states 12
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_isys_sp_frontend_states scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_isys_sp_frontend_states 0x6248
#define HIVE_SIZE_sp_ia_css_isys_sp_frontend_states 12

/* function ia_css_dmaproxy_sp_read_byte_addr_mmio: 6E87 */

/* function ia_css_ispctrl_sp_done_ds: 3550 */

/* function ia_css_sp_isp_param_get_mem_inits: 4633 */

/* function ia_css_parambuf_sp_init_buffer_queues: 1416 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_pfp_spref
#define HIVE_MEM_vbuf_pfp_spref scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_pfp_spref 0x2DC
#define HIVE_SIZE_vbuf_pfp_spref 4
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_pfp_spref scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_pfp_spref 0x2DC
#define HIVE_SIZE_sp_vbuf_pfp_spref 4

/* function input_system_cfg: AAF */

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
#define HIVE_ADDR_ia_css_bufq_sp_pipe_private_frames 0x58EC
#define HIVE_SIZE_ia_css_bufq_sp_pipe_private_frames 280
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_pipe_private_frames scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_pipe_private_frames 0x58EC
#define HIVE_SIZE_sp_ia_css_bufq_sp_pipe_private_frames 280

/* function ia_css_isys_sp_backend_release: 5B18 */

/* function ia_css_isys_sp_backend_destroy: 5B42 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp2host_buffer_queue_handle
#define HIVE_MEM_sp2host_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp2host_buffer_queue_handle 0x5A04
#define HIVE_SIZE_sp2host_buffer_queue_handle 96
#else
#endif
#endif
#define HIVE_MEM_sp_sp2host_buffer_queue_handle scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp2host_buffer_queue_handle 0x5A04
#define HIVE_SIZE_sp_sp2host_buffer_queue_handle 96

/* function ia_css_isys_sp_token_map_check_mipi_frame_size: 5F9D */

/* function ia_css_ispctrl_sp_init_isp_vars: 4339 */

/* function ia_css_isys_sp_frontend_has_empty_mipi_buffer_cb: 5B95 */

/* function sp_warning: 8E3 */

/* function ia_css_rmgr_sp_vbuf_enqueue: 636F */

/* function ia_css_tagger_sp_tag_exp_id: 1F53 */

/* function ia_css_dmaproxy_sp_write: 2F2E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_raw_buff_pool_lock_params_handles
#define HIVE_MEM_raw_buff_pool_lock_params_handles scalar_processor_2400_dmem
#define HIVE_ADDR_raw_buff_pool_lock_params_handles 0x61F0
#define HIVE_SIZE_raw_buff_pool_lock_params_handles 60
#else
#endif
#endif
#define HIVE_MEM_sp_raw_buff_pool_lock_params_handles scalar_processor_2400_dmem
#define HIVE_ADDR_sp_raw_buff_pool_lock_params_handles 0x61F0
#define HIVE_SIZE_sp_raw_buff_pool_lock_params_handles 60

/* function ia_css_parambuf_sp_release_in_param: 1296 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_irq_sw_interrupt_token
#define HIVE_MEM_irq_sw_interrupt_token scalar_processor_2400_dmem
#define HIVE_ADDR_irq_sw_interrupt_token 0x3FF4
#define HIVE_SIZE_irq_sw_interrupt_token 4
#else
#endif
#endif
#define HIVE_MEM_sp_irq_sw_interrupt_token scalar_processor_2400_dmem
#define HIVE_ADDR_sp_irq_sw_interrupt_token 0x3FF4
#define HIVE_SIZE_sp_irq_sw_interrupt_token 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_isp_addresses
#define HIVE_MEM_sp_isp_addresses scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_addresses 0x5E4C
#define HIVE_SIZE_sp_isp_addresses 172
#else
#endif
#endif
#define HIVE_MEM_sp_sp_isp_addresses scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_isp_addresses 0x5E4C
#define HIVE_SIZE_sp_sp_isp_addresses 172

/* function ia_css_rmgr_sp_acq_gen: 6294 */

/* function receiver_reg_load: AC4 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isps
#define HIVE_MEM_isps scalar_processor_2400_dmem
#define HIVE_ADDR_isps 0x6280
#define HIVE_SIZE_isps 28
#else
#endif
#endif
#define HIVE_MEM_sp_isps scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isps 0x6280
#define HIVE_SIZE_sp_isps 28

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_host_sp_queues_initialized
#define HIVE_MEM_host_sp_queues_initialized scalar_processor_2400_dmem
#define HIVE_ADDR_host_sp_queues_initialized 0x400C
#define HIVE_SIZE_host_sp_queues_initialized 4
#else
#endif
#endif
#define HIVE_MEM_sp_host_sp_queues_initialized scalar_processor_2400_dmem
#define HIVE_ADDR_sp_host_sp_queues_initialized 0x400C
#define HIVE_SIZE_sp_host_sp_queues_initialized 4

/* function ia_css_queue_uninit: 4AC7 */

/* function ia_css_tagger_buf_sp_is_locked: 27C4 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_ispctrl_sp_isp_started
#define HIVE_MEM_ia_css_ispctrl_sp_isp_started scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_ispctrl_sp_isp_started 0x5B0C
#define HIVE_SIZE_ia_css_ispctrl_sp_isp_started 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_ispctrl_sp_isp_started scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_ispctrl_sp_isp_started 0x5B0C
#define HIVE_SIZE_sp_ia_css_ispctrl_sp_isp_started 4

/* function ia_css_bufq_sp_release_dynamic_buf: 29C2 */

/* function ia_css_sp_metadata_thread_terminate: 594E */

/* function ia_css_dmaproxy_sp_set_height_exception: 3036 */

/* function ia_css_dmaproxy_sp_init_vmem_channel: 2FB9 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_num_ready_threads
#define HIVE_MEM_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_num_ready_threads 0x49C4
#define HIVE_SIZE_num_ready_threads 4
#else
#endif
#endif
#define HIVE_MEM_sp_num_ready_threads scalar_processor_2400_dmem
#define HIVE_ADDR_sp_num_ready_threads 0x49C4
#define HIVE_SIZE_sp_num_ready_threads 4

/* function ia_css_dmaproxy_sp_write_byte_addr_mmio: 2F00 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_vbuf_spref
#define HIVE_MEM_vbuf_spref scalar_processor_2400_dmem
#define HIVE_ADDR_vbuf_spref 0x2D8
#define HIVE_SIZE_vbuf_spref 4
#else
#endif
#endif
#define HIVE_MEM_sp_vbuf_spref scalar_processor_2400_dmem
#define HIVE_ADDR_sp_vbuf_spref 0x2D8
#define HIVE_SIZE_sp_vbuf_spref 4

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_metadata_thread
#define HIVE_MEM_sp_metadata_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_metadata_thread 0x4978
#define HIVE_SIZE_sp_metadata_thread 68
#else
#endif
#endif
#define HIVE_MEM_sp_sp_metadata_thread scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_metadata_thread 0x4978
#define HIVE_SIZE_sp_sp_metadata_thread 68

/* function ia_css_queue_enqueue: 4A07 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_request
#define HIVE_MEM_ia_css_flash_sp_request scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_request 0x49D0
#define HIVE_SIZE_ia_css_flash_sp_request 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_request scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_request 0x49D0
#define HIVE_SIZE_sp_ia_css_flash_sp_request 4

/* function ia_css_dmaproxy_sp_vmem_write: 2EBA */

/* function ia_css_tagger_buf_sp_unmark: 286D */

/* function ia_css_isys_sp_token_map_snd_capture_req: 5FFF */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_reading_if
#define HIVE_MEM_sem_for_reading_if scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_reading_if 0x47A8
#define HIVE_SIZE_sem_for_reading_if 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_reading_if scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_reading_if 0x47A8
#define HIVE_SIZE_sp_sem_for_reading_if 20

/* function sp_generate_interrupts: 985 */

/* function ia_css_pipeline_sp_start: 17A4 */

/* function ia_css_sp_rawcopy_init: 50A2 */

/* function tmr_clock_read: A96 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ISP_BAMEM_BASE
#define HIVE_MEM_ISP_BAMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_ISP_BAMEM_BASE 0x2E8
#define HIVE_SIZE_ISP_BAMEM_BASE 4
#else
#endif
#endif
#define HIVE_MEM_sp_ISP_BAMEM_BASE scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ISP_BAMEM_BASE 0x2E8
#define HIVE_SIZE_sp_ISP_BAMEM_BASE 4

/* function ia_css_isys_sp_frontend_rcv_capture_ack: 5C4D */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_bufq_sp_sems_for_sp2host_buf_queues
#define HIVE_MEM_ia_css_bufq_sp_sems_for_sp2host_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_bufq_sp_sems_for_sp2host_buf_queues 0x5A64
#define HIVE_SIZE_ia_css_bufq_sp_sems_for_sp2host_buf_queues 160
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_bufq_sp_sems_for_sp2host_buf_queues scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_bufq_sp_sems_for_sp2host_buf_queues 0x5A64
#define HIVE_SIZE_sp_ia_css_bufq_sp_sems_for_sp2host_buf_queues 160

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_mem_map
#define HIVE_MEM_mem_map scalar_processor_2400_dmem
#define HIVE_ADDR_mem_map 0x4010
#define HIVE_SIZE_mem_map 284
#else
#endif
#endif
#define HIVE_MEM_sp_mem_map scalar_processor_2400_dmem
#define HIVE_ADDR_sp_mem_map 0x4010
#define HIVE_SIZE_sp_mem_map 284

/* function css_get_frame_processing_time_start: 1F4A */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_all_cbs_frame
#define HIVE_MEM_sp_all_cbs_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_all_cbs_frame 0x47BC
#define HIVE_SIZE_sp_all_cbs_frame 16
#else
#endif
#endif
#define HIVE_MEM_sp_sp_all_cbs_frame scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_all_cbs_frame 0x47BC
#define HIVE_SIZE_sp_sp_all_cbs_frame 16

/* function thread_sp_queue_print: DAA */

/* function sp_notify_eof: 931 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sem_for_str2mem
#define HIVE_MEM_sem_for_str2mem scalar_processor_2400_dmem
#define HIVE_ADDR_sem_for_str2mem 0x47CC
#define HIVE_SIZE_sem_for_str2mem 20
#else
#endif
#endif
#define HIVE_MEM_sp_sem_for_str2mem scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sem_for_str2mem 0x47CC
#define HIVE_SIZE_sp_sem_for_str2mem 20

/* function ia_css_bufq_sp_acquire_dynamic_buf: 2BCF */

/* function ia_css_circbuf_destroy: 1049 */

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

/* function ia_css_sp_isp_param_mem_load: 45BB */

/* function __div: 684D */

/* function ia_css_isys_sp_frontend_create: 5E2F */

/* function ia_css_rmgr_sp_refcount_release_vbuf: 638E */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_ia_css_flash_sp_in_use
#define HIVE_MEM_ia_css_flash_sp_in_use scalar_processor_2400_dmem
#define HIVE_ADDR_ia_css_flash_sp_in_use 0x49D4
#define HIVE_SIZE_ia_css_flash_sp_in_use 4
#else
#endif
#endif
#define HIVE_MEM_sp_ia_css_flash_sp_in_use scalar_processor_2400_dmem
#define HIVE_ADDR_sp_ia_css_flash_sp_in_use 0x49D4
#define HIVE_SIZE_sp_ia_css_flash_sp_in_use 4

/* function ia_css_thread_sem_sp_wait: 6B06 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_sleep_mode
#define HIVE_MEM_sp_sleep_mode scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sleep_mode 0x412C
#define HIVE_SIZE_sp_sleep_mode 4
#else
#endif
#endif
#define HIVE_MEM_sp_sp_sleep_mode scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_sleep_mode 0x412C
#define HIVE_SIZE_sp_sp_sleep_mode 4

/* function mmu_invalidate_cache: EA */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_sp_max_cb_elems
#define HIVE_MEM_sp_max_cb_elems scalar_processor_2400_dmem
#define HIVE_ADDR_sp_max_cb_elems 0x140
#define HIVE_SIZE_sp_max_cb_elems 8
#else
#endif
#endif
#define HIVE_MEM_sp_sp_max_cb_elems scalar_processor_2400_dmem
#define HIVE_ADDR_sp_sp_max_cb_elems 0x140
#define HIVE_SIZE_sp_sp_max_cb_elems 8

/* function ia_css_dmaproxy_sp_register_channel_to_port: 2E78 */

/* function ia_css_queue_remote_init: 4AE9 */

#ifndef HIVE_MULTIPLE_PROGRAMS
#ifndef HIVE_MEM_isp_stop_req
#define HIVE_MEM_isp_stop_req scalar_processor_2400_dmem
#define HIVE_ADDR_isp_stop_req 0x4660
#define HIVE_SIZE_isp_stop_req 4
#else
#endif
#endif
#define HIVE_MEM_sp_isp_stop_req scalar_processor_2400_dmem
#define HIVE_ADDR_sp_isp_stop_req 0x4660
#define HIVE_SIZE_sp_isp_stop_req 4

#define HIVE_ICACHE_sp_critical_SEGMENT_START 0
#define HIVE_ICACHE_sp_critical_NUM_SEGMENTS  1

#endif /* _sp_map_h_ */
extern void sh_css_dump_sp_dmem(void);
void sh_css_dump_sp_dmem(void)
{
}
