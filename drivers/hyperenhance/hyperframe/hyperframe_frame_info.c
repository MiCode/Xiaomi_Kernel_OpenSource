// MIUI ADD: Performance_FramePredictBoost
#include "hyperframe_frame_info.h"

#define MEMBER_NAME_MAX_SIZE 64
#define RECORD_FRAME_SIZE 7

static HLIST_HEAD(frame_infos);

static DEFINE_MUTEX(finfo_lock);

t_time vsync_period;

struct frame_thread_id* recent_list = NULL;
int remove_pid = -1;

size_t hyperframe_update_vsync_period(t_time time)
{
	int ret = 0;
	vsync_period = time;
	htrace_c_fi_debug(-1, vsync_period, "vsync");
	return ret;
}

int find_recent_idx(int pid)
{
	int ret = -1;
	int i;

	for (i = 0; i < RECORD_FRAME_SIZE; i++) {
		if (recent_list[i].pid == pid) {
			ret = i;
			break;
		}
	}
	return ret;
}

size_t update_recent_queue(int pid, int tid_render)
{
	int ret = -1;
	int i;
	int tmp_pid;
	int tmp_tid_render;
	int idx;

	if (recent_list == NULL) return -1;

	idx = find_recent_idx(pid);

	if (idx == RECORD_FRAME_SIZE -1) {
		ret = 0;
	} else if (idx < RECORD_FRAME_SIZE -1 && idx >= 0) {
		for (i = idx; i < RECORD_FRAME_SIZE - 1; i++) {
			tmp_pid = recent_list[i].pid;
			tmp_tid_render = recent_list[i].tid_render;
			recent_list[i].pid = recent_list[i + 1].pid;
			recent_list[i].tid_render = recent_list[i + 1].tid_render;
			recent_list[i + 1].pid = tmp_pid;
			recent_list[i + 1].tid_render = tmp_tid_render;
		}
		ret = 1;
	} else {
		remove_pid = recent_list[0].pid;
		recent_list[0].pid = pid;
		recent_list[0].tid_render = tid_render;
		for (i = 0; i < RECORD_FRAME_SIZE - 1; i++) {
			tmp_pid = recent_list[i].pid;
			tmp_tid_render = recent_list[i].tid_render;
			recent_list[i].pid = recent_list[i + 1].pid;
			recent_list[i].tid_render = recent_list[i + 1].tid_render;
			recent_list[i + 1].pid = tmp_pid;
			recent_list[i + 1].tid_render = tmp_tid_render;
		}
		ret = 2;
	}

	htrace_c_fi(current->tgid, recent_list[RECORD_FRAME_SIZE - 1].pid, "recent_pid");
	htrace_c_fi(current->tgid, recent_list[RECORD_FRAME_SIZE - 1].tid_render, "recent_tid_render");

	return ret;
}

size_t check_is_latest_thread_id(int thread_id)
{
	int ret = 0;

	if (recent_list == NULL)
		return -1;
	if (thread_id == recent_list[RECORD_FRAME_SIZE - 1].pid)
		ret = 1;
	if (thread_id == recent_list[RECORD_FRAME_SIZE - 1].tid_render)
		ret = 2;

	return ret;
}

ssize_t hyperframe_update_time(int pid, int vsync_id, t_time time, int type, bool started)
{
	int ret = 0;
	struct frame_info* iter = NULL;
	struct frame_info* node = NULL;
	struct timepair* time_ptr = NULL;
	char period_name[MEMBER_NAME_MAX_SIZE];
	int pos = 0;
	int length = 0;
	int idx = 0;
	int id_type = 0;

	mutex_lock(&finfo_lock);

	if (pid <= 0 || vsync_id <=0) goto out;

	id_type = check_is_latest_thread_id(pid);
	if (id_type <= 0)
		goto out;

	hlist_for_each_entry(iter, &frame_infos, hlist) {
		if (iter) {
			if (iter->pid == pid) {
				node = iter;
				break;
			}
			if (iter->render_tid == pid) {
				node = iter;
				break;
			}
		}
	}

	if (node == NULL) {
		node = kmalloc(sizeof(struct frame_info), GFP_KERNEL);
		node->pid = recent_list[RECORD_FRAME_SIZE - 1].pid;
		node->render_tid = recent_list[RECORD_FRAME_SIZE - 1].tid_render;
		node->latest_idx = 0;
		hlist_add_head(&node->hlist, &frame_infos);
	}

	switch (type) {
		case TIME_DOFRAME:
			time_ptr = node->t_doframe;
			length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "doframe");
			break;
		case TIME_RECOREVIEW:
			time_ptr = node->t_recordview;
			length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "recordview");
			break;
		case TIME_DRAWFRAME:
			time_ptr = node->t_drawframes;
			length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "drawframes");
			break;
		case TIME_DEQUEUE:
			time_ptr = node->t_dequeue;
			length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "dequeue");
			break;
		case TIME_QUEUE:
			time_ptr = node->t_queue;
			length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "queue");
			break;
		case TIME_GPU:
			time_ptr = node->t_gpu;
			length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "gpu");
			break;
		default:
			break;
	}
	pos += length;
	if (time_ptr == NULL) {
		ret = -1;
		goto out;
	}

	htrace_c_fi(node->pid, node->latest_idx, "current_idx");
	htrace_c_fi(node->pid, vsync_id, "vsync_id");

	idx = get_frame_idx_by_vsyncid_inter(node, vsync_id);

	if (idx < 0 || idx >= WINDOW_LENGTH)
		goto out;

	time = nsec_to_100usec(time);

	if (started) {
		time_ptr[idx].t_start = time;
		if (type == TIME_DOFRAME) {
			node->ts_latest_frame = time;
		}
		length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "_start");
	} else {
		time_ptr[idx].t_end = time;
		if (type == TIME_DRAWFRAME) {
			if (remove_pid > 0) {
				hyperframe_remove_info_inter(remove_pid);
				remove_pid = -1;
			}
		}
		length = scnprintf(period_name + pos, MEMBER_NAME_MAX_SIZE - pos, "%s", "_end");
	}
	pos += length;

	htrace_c_fi(node->pid, (int)node->latest_idx, "latest_idx");
	htrace_c_fi(node->pid, (int)time, period_name);

out:
	mutex_unlock(&finfo_lock);

	return 0;
}

size_t hyperframe_remove_info_inter(int pid)
{
	int ret = 0;
	struct frame_info* iter = NULL;
	bool is_find = false;

	hlist_for_each_entry(iter, &frame_infos, hlist) {
		if (iter) {
			if (iter->pid == pid)
				break;
		}
	}

	if (is_find) {
		hlist_del(&iter->hlist);
		kfree(iter);
		ret = 1;
	}

	return ret;
}

struct frame_info* get_frame_info(int id)
{
	struct frame_info* iter = NULL;
	struct frame_info* ret = NULL;

	hlist_for_each_entry(iter, &frame_infos, hlist) {
		if (iter) {
			if (iter->pid == id) {
				ret = iter;
				break;
			}
			if (iter->render_tid == id) {
				ret = iter;
				break;
			}
		}
	}
	return ret;
}

int get_frame_idx_by_vsyncid(struct frame_info* frame, int vsync_id)
{
	int ret = -1;
	int i;

	for (i = 0; i < WINDOW_LENGTH; i++) {
		if (frame->vsync_id_arr[i] == vsync_id) {
			ret = i;
			break;
		}
	}

	return ret;
}

int get_frame_idx_by_vsyncid_inter(struct frame_info* frame, int vsync_id)
{
	int ret = -1;
	int i;

	if (frame == NULL)
		return -1;

	for (i = 0; i < WINDOW_LENGTH; i++) {
		if (frame->vsync_id_arr[i] == vsync_id) {
			ret = i;
			break;
		}
	}

	if (ret == -1) {
		frame->latest_idx++;
		if (frame->latest_idx >= WINDOW_LENGTH)
			frame->latest_idx = 0;

		ret = frame->latest_idx;
		frame->vsync_id_arr[frame->latest_idx] = vsync_id;
		htrace_c_fi_debug(frame->pid, frame->latest_idx, "new_idx");
	}
	return ret;
}

int hyperframe_get_target_fps(void)
{
	int ret = 60;

	return ret;
}

size_t hyperframe_frameinfo_init(void)
{
	int i;
	recent_list = kcalloc(RECORD_FRAME_SIZE, sizeof(struct frame_thread_id), GFP_KERNEL);
	for (i = 0; i < RECORD_FRAME_SIZE; i++) {
		recent_list[i].pid = 0;
		recent_list[i].tid_render = 0;
	}

	return 0;
}
// END Performance_FramePredictBoost