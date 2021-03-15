#include "xiaomi_frame_stat.h"

struct frame_stat fm_stat = {
	.enabled = true,
	.last_fps = 0,
	.fps_record = {0, 0, 0},
	.enable_idle = true,
	.fps_filter = FPS_FILTER_AVERAGE,
};

ssize_t smart_fps_value_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
			g_panel->dfps_caps.smart_fps_value);
}

ssize_t settings_fps_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fm_stat.settings_fps);
}

ssize_t settings_fps_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	u32 fps = 0;
	ssize_t ret = kstrtou32(buf, 10, &fps);
	if (ret)
		return ret;

	fm_stat.settings_fps = fps;

	return count;
}

ssize_t smart_fps_filter_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fm_stat.fps_filter);
}

ssize_t smart_fps_filter_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	u32 filter = 0;
	ssize_t ret = kstrtou32(buf, 10, &filter);
	if (ret)
		return ret;

	if (filter > FPS_FILTER_MAX)
		filter = FPS_FILTER_MAX;

	fm_stat.fps_filter = filter;

	return count;
}

void frame_stat_notify(u32 data)
{
	struct dsi_display *display = NULL;

	if (!g_panel || !g_panel->host)
		return;

	display = container_of(g_panel->host, struct dsi_display, host);
	if (!display) {
		pr_err("%s: invalid param.\n");
		return;
	}

	g_panel->dfps_caps.smart_fps_value = data;
	sysfs_notify(&display->drm_conn->kdev->kobj, NULL, "smart_fps_value");
	pr_debug("%s: fps = %d\n", __func__, g_panel->dfps_caps.smart_fps_value);
	fm_stat.skip_count = 0;
	fm_stat.last_fps = data;

	return;
}

void calc_fps(u64 duration, int input_event)
{
	ktime_t current_time_us;
	u64 fps, diff_us, diff;
	u32 curr_fps;
	static bool restore_fps;

	if (!g_panel || !g_panel->dfps_caps.smart_fps_support || !fm_stat.enabled)
		return;

	if (input_event) {
		restore_fps = true;
		return;
	}

	/* input event */
	if (restore_fps) {
		restore_fps = 0;
		curr_fps = fm_stat.settings_fps ? fm_stat.settings_fps :
			g_panel->dfps_caps.max_refresh_rate;
		if (fm_stat.last_fps != curr_fps || g_panel->mi_cfg.smart_fps_restore) {
			g_panel->mi_cfg.smart_fps_restore = false;
			pr_debug("%s: last fps = %u, restore to %u by input event.\n",
					__func__, fm_stat.last_fps, curr_fps);
			frame_stat_notify(curr_fps);
		}
		frame_stat_reset();
		goto final;
	}

	current_time_us = ktime_get();
	if (idle_status) {
		pr_debug("%s: exit fps calc due to idle mode\n", __func__);
		goto exit;
	}

	if (!fm_stat.start) {
		fm_stat.last_sampled_time_us = current_time_us;
		fm_stat.start = true;
	}
	diff_us = (u64)ktime_us_delta(current_time_us, fm_stat.last_sampled_time_us);

	fm_stat.frame_count++;

	if (fm_stat.monitor_long_frame && fm_stat.last_frame_commit_time_us > 0) {
		diff = (u64)ktime_us_delta(current_time_us, fm_stat.last_frame_commit_time_us);
		fm_stat.last_frame_commit_time_us = current_time_us;
		if (diff > LONG_FRAME_INTERVAL) {
			fm_stat.skip_count++;
			pr_debug("%s: Longer frame interval[%lld ms], count[%d]\n",
					__func__, diff/NANO_TO_MICRO, fm_stat.skip_count);
			if (fm_stat.skip_count >= LONG_INTERVAL_FRAME_COUNT) {
				/* Sometime app refresh in low fps, here set 50hz */
				if (IDLE_FPS != fm_stat.last_fps)
					frame_stat_notify(IDLE_FPS);
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

		fm_stat.fps_record[2] = fm_stat.fps_record[1];
		fm_stat.fps_record[1] = fm_stat.fps_record[0];
		fm_stat.fps_record[0] = (u32)(fps / 10);

		pr_debug("%s: FPS:%u.%u max_frame_duration = %lld(us) max_input_fence_duration = %lld(us)\n",
				__func__,
				(u32)(fps / 10),
				(u32)(fps % 10),
				fm_stat.max_frame_duration / NANO_TO_MICRO,
				fm_stat.max_input_fence_duration / NANO_TO_MICRO);

		if (fm_stat.fps_filter &&
				(!fm_stat.fps_record[0] || !fm_stat.fps_record[1] || !fm_stat.fps_record[2]))
			goto exit;

		switch (fm_stat.fps_filter) {
		case FPS_FILTER_MIN:
			curr_fps = min(min(fm_stat.fps_record[0], fm_stat.fps_record[1]), fm_stat.fps_record[2]);
			break;
		case FPS_FILTER_AVERAGE:
			curr_fps = (fm_stat.fps_record[0] + fm_stat.fps_record[1] + fm_stat.fps_record[2]) / 3;
			break;
		case FPS_FILTER_MEDIAN:
		{
			u32 max_fps = max(max(fm_stat.fps_record[0], fm_stat.fps_record[1]), fm_stat.fps_record[2]);
			u32 min_fps = min(min(fm_stat.fps_record[0], fm_stat.fps_record[1]), fm_stat.fps_record[2]);
			curr_fps = fm_stat.fps_record[0] + fm_stat.fps_record[1] + fm_stat.fps_record[2] - max_fps - min_fps;
		}
			break;
		case FPS_FILTER_MAX:
			curr_fps = max(max(fm_stat.fps_record[0], fm_stat.fps_record[1]), fm_stat.fps_record[2]);
			break;
		case FPS_FILTER_NONE:
			/* fall through */
		default:
			curr_fps = fm_stat.fps_record[0];
			break;
		}

		if (curr_fps > fm_stat.settings_fps)
			curr_fps = fm_stat.settings_fps;

		if (curr_fps != fm_stat.last_fps)
			frame_stat_notify(curr_fps);

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

void frame_stat_reset(void)
{
	ktime_t current_time_us = ktime_get();
	fm_stat.last_sampled_time_us = current_time_us;
	fm_stat.last_frame_commit_time_us = current_time_us;
	fm_stat.start = false;
	fm_stat.frame_count = 0;
	fm_stat.max_frame_duration = 0;
	fm_stat.max_input_fence_duration = 0;
	fm_stat.skip_count = 0;
	fm_stat.fps_record[0] = 0;
	pr_debug("%s: reset frame statistics.\n", __func__);
}
