#include "xiaomi_frame_stat.h"

struct frame_stat fm_stat = {.enabled = true};

ssize_t smart_fps_value_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_panel->dfps_caps.smart_fps_value);
}

void frame_stat_notify(int data)
{
	char *envp[2];
	struct dsi_display *display = NULL;
	struct mipi_dsi_host *host = NULL;

	if (g_panel)
		host = g_panel->host;

	if (host)
		display = container_of(host, struct dsi_display, host);

	if (!display) {
		pr_err("%s: invalid param.\n");
		return;
	}

	envp[0] = "SOURCE=sysfs";
	envp[1] = NULL;

	g_panel->dfps_caps.smart_fps_value = data;

	kobject_uevent_env(&display->drm_conn->kdev->kobj, KOBJ_CHANGE, envp);
	sysfs_notify(&display->drm_conn->kdev->kobj, NULL, "smart_fps_value");
	pr_debug("%s: fps = %d\n", __func__, g_panel->dfps_caps.smart_fps_value);

	return;
}

void calc_fps(u64 duration, int input_event)
{
	ktime_t current_time_us;
	u64 fps, diff_us, diff, curr_fps;
	static bool restore_fps;

	if (!g_panel->dfps_caps.smart_fps_support || !fm_stat.enabled)
		return;

	if (input_event) {
		restore_fps = true;
		return;
	}

	/* input event */
	if (restore_fps) {
		restore_fps = 0;
		frame_stat_notify(g_panel->dfps_caps.max_refresh_rate);
		fm_stat.last_fps = g_panel->dfps_caps.max_refresh_rate;
		pr_debug("%s: input event restore fps.\n", __func__);
		fm_stat.skip_count = 0;
		fm_stat.skip_once = true;
		goto exit;
	}

	current_time_us = ktime_get();
	if (idle_status == true) {
		if (fm_stat.last_fps != IDLE_FPS) {
			fm_stat.last_fps = IDLE_FPS;
			pr_debug("%s: exit fps calc due to idle mode\n", __func__);
		}
		goto exit;
	}

	if(!fm_stat.start) {
		fm_stat.last_sampled_time_us = current_time_us;
		fm_stat.start = true;
	}
	diff_us = (u64)ktime_us_delta(current_time_us, fm_stat.last_sampled_time_us);

	fm_stat.frame_count++;

	if (fm_stat.last_frame_commit_time_us > 0) {
		diff = (u64)ktime_us_delta(current_time_us, fm_stat.last_frame_commit_time_us);
		fm_stat.last_frame_commit_time_us = current_time_us;
		if (diff > LONG_FRAME_INTERVAL) {
			fm_stat.skip_count++;
			pr_debug("%s: Longer frame interval[%lld ms], count[%d]\n", __func__, diff/NANO_TO_MICRO, fm_stat.skip_count);
			if (fm_stat.skip_count >= LONG_INTERVAL_FRAME_COUNT) {
				/* Some apps will load resource in low fps, here set 60hz for safety */
				frame_stat_notify(DEFAULT_FPS);
				fm_stat.last_fps = DEFAULT_FPS;
				fm_stat.skip_count = 0;
			}
			goto exit;
		} else
			fm_stat.skip_count = 0;
	}

	if (diff_us >= FPS_PERIOD_1_SEC) {
		/* skip once after input events */
		if (fm_stat.skip_once) {
			fm_stat.skip_once = false;
			goto exit;
		}

		/* Multiplying with 10 to get fps in floating point */
		fps = fm_stat.frame_count * FPS_PERIOD_1_SEC * 10;
		do_div(fps, diff_us);
		curr_fps = (unsigned int)fps/10;
		if (curr_fps != fm_stat.last_fps) {
			frame_stat_notify(curr_fps);
			fm_stat.last_fps = curr_fps;
		}
		pr_debug("FPS is %d.%d  max_frame_duration = %lld(us) max_input_fence_duration = %lld(us)\n",
				(unsigned int)fps/10,
				(unsigned int)fps%10,
				fm_stat.max_frame_duration/NANO_TO_MICRO,
				fm_stat.max_input_fence_duration/NANO_TO_MICRO);
		goto exit;
	} else {
		fm_stat.delta_commit_duration = duration;
		if (fm_stat.max_frame_duration < fm_stat.delta_commit_duration)
			fm_stat.max_frame_duration = fm_stat.delta_commit_duration;

		fm_stat.delta_input_duration = fm_stat.input_fence_duration;
		if (fm_stat.max_input_fence_duration < fm_stat.delta_input_duration)
			fm_stat.max_input_fence_duration = fm_stat.delta_input_duration;

		fm_stat.last_frame_commit_time_us = current_time_us;
		goto final;
	}

exit:
	fm_stat.last_sampled_time_us = current_time_us;
	fm_stat.frame_count = 0;
	fm_stat.start = false;
	fm_stat.max_frame_duration = 0;
	fm_stat.max_input_fence_duration = 0;
	fm_stat.last_frame_commit_time_us = current_time_us;
final:
	return;
}

void frame_stat_collector(u64 duration, enum stat_item item)
{
	ktime_t now = ktime_get();

	switch (item) {
	case COMMIT_START_TS:
		fm_stat.commit_start_ts = now;
		pr_debug("%s: commit start ts = %lld\n", __func__, fm_stat.commit_start_ts);
		break;
	case GET_INPUT_FENCE_TS:
		fm_stat.get_input_fence_ts = now;
		pr_debug("%s: get_input_fence_ts = %lld, duration = %lld \n", __func__, fm_stat.get_input_fence_ts, duration);
		fm_stat.input_fence_duration = duration;
		break;
	case VBLANK_TS:
		fm_stat.commit_start_ts = now;
		pr_debug("vblank ts = %lld\n", fm_stat.get_input_fence_ts);
		break;
	case RETIRE_FENCE_TS:
		fm_stat.retire_fence_ts = now;
		pr_debug("%s: retire fence ts = %lld\n", __func__, fm_stat.retire_fence_ts);
		break;
	case COMMIT_END_TS:
		fm_stat.commit_end_ts = now;
		if (fm_stat.input_fence_duration > duration/10)
			pr_debug("%s: long wait for input fence might cause frame miss!\n", __func__);
		calc_fps(duration, 0);
		break;
	default:
		break;
	}

	return;
}
