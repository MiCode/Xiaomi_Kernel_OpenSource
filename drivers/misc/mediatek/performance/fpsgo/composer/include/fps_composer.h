/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __FPS_COMPOSER_H__
#define __FPS_COMPOSER_H__

#include <linux/rbtree.h>

enum FPSGO_COM_ERROR {
	FPSGO_COM_IS_RENDER,
	FPSGO_COM_TASK_NOT_EXIST,
	FPSGO_COM_IS_SF,
};

struct ui_pid_info {
	struct rb_node rb_node;
	struct list_head render_list;
	int ui_pid;
};

struct connect_api_info {
	struct rb_node rb_node;
	struct list_head render_list;
	int pid;
	int tgid;
	unsigned long long buffer_id;
	int buffer_key;
	int api;
};

int fpsgo_composer_init(void);
void fpsgo_composer_exit(void);

void fpsgo_ctrl2comp_vysnc_aligned_frame_done
	(int pid, int ui_pid, unsigned long long frame_time,
	int render, unsigned long long t_frame_done,
	int render_method, unsigned long long id);
void fpsgo_ctrl2comp_vysnc_aligned_frame_start
	(int pid, unsigned long long t_frame_start, unsigned long long id);
void fpsgo_ctrl2comp_vysnc_aligned_no_render
	(int pid, int render,
	 unsigned long long t_frame_done, unsigned long long id);
void fpsgo_ctrl2comp_dequeue_end
	(int pid, unsigned long long dequeue_end_time,
	 unsigned long long bufferID, int queue_SF);
void fpsgo_ctrl2comp_dequeue_start
	(int pid, unsigned long long dequeue_start_time,
	 unsigned long long bufferID, int queue_SF);
void fpsgo_ctrl2comp_enqueue_end
	(int pid, unsigned long long enqueue_end_time,
	 unsigned long long bufferID, int queue_SF);
void fpsgo_ctrl2comp_enqueue_start
	(int pid, unsigned long long enqueue_start_time,
	 unsigned long long bufferID, int queue_SF);
void fpsgo_fbt2comp_destroy_frame_info(int pid);
void fpsgo_ctrl2comp_connect_api(int pid, unsigned long long bufferID, int api);
void fpsgo_ctrl2comp_disconnect_api
	(int pid, unsigned long long bufferID, int api);
void fpsgo_fstb2comp_check_connect_api(void);
void fpsgo_base2com_delete_ui_pid_info(int ui_pid);
void fpsgo_base2com_clear_ui_pid_info(void);
void fpsgo_ctrl2comp_vysnc_aligned_draw_start(int pid, unsigned long long id);


#endif

