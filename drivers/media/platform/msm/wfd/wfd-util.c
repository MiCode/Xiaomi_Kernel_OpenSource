/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include "wfd-util.h"

static struct dentry *wfd_debugfs_root;

int wfd_stats_setup()
{
	wfd_debugfs_root = debugfs_create_dir("wfd", NULL);

	if (wfd_debugfs_root == ERR_PTR(-ENODEV))
		return -ENODEV;
	else if (!wfd_debugfs_root)
		return -ENOMEM;
	else
		return 0;
}

void wfd_stats_teardown()
{
	if (wfd_debugfs_root)
		debugfs_remove_recursive(wfd_debugfs_root);
}

int wfd_stats_init(struct wfd_stats *stats, int device)
{
	char device_str[NAME_MAX] = "";
	int rc = 0;

	if (!stats) {
		rc = -EINVAL;
		goto wfd_stats_init_fail;
	} else if (!wfd_debugfs_root) {
		WFD_MSG_ERR("wfd debugfs root does not exist\n");
		rc = -ENOENT;
		goto wfd_stats_init_fail;
	}

	memset(stats, 0, sizeof(*stats));
	INIT_LIST_HEAD(&stats->enc_queue);
	mutex_init(&stats->mutex);

	snprintf(device_str, sizeof(device_str), "%d", device);
	stats->d_parent = debugfs_create_dir(device_str, wfd_debugfs_root);
	if (IS_ERR(stats->d_parent)) {
		rc = PTR_ERR(stats->d_parent);
		stats->d_parent = NULL;
		goto wfd_stats_init_fail;
	}

	stats->d_v4l2_buf_count = debugfs_create_u32("v4l2_buf_count", S_IRUGO,
			stats->d_parent, &stats->v4l2_buf_count);
	if (IS_ERR(stats->d_v4l2_buf_count)) {
		rc = PTR_ERR(stats->d_v4l2_buf_count);
		stats->d_v4l2_buf_count = NULL;
		goto wfd_stats_init_fail;
	}

	stats->d_mdp_buf_count = debugfs_create_u32("mdp_buf_count", S_IRUGO,
			stats->d_parent, &stats->mdp_buf_count);
	if (IS_ERR(stats->d_mdp_buf_count)) {
		rc = PTR_ERR(stats->d_mdp_buf_count);
		stats->d_mdp_buf_count = NULL;
		goto wfd_stats_init_fail;
	}

	stats->d_vsg_buf_count = debugfs_create_u32("vsg_buf_count", S_IRUGO,
			stats->d_parent, &stats->vsg_buf_count);
	if (IS_ERR(stats->d_vsg_buf_count)) {
		rc = PTR_ERR(stats->d_vsg_buf_count);
		stats->d_vsg_buf_count = NULL;
		goto wfd_stats_init_fail;
	}

	stats->d_enc_buf_count = debugfs_create_u32("enc_buf_count", S_IRUGO,
			stats->d_parent, &stats->enc_buf_count);
	if (IS_ERR(stats->d_enc_buf_count)) {
		rc = PTR_ERR(stats->d_enc_buf_count);
		stats->d_enc_buf_count = NULL;
		goto wfd_stats_init_fail;
	}

	stats->d_frames_encoded = debugfs_create_u32("frames_encoded", S_IRUGO,
			stats->d_parent, &stats->frames_encoded);
	if (IS_ERR(stats->d_frames_encoded)) {
		rc = PTR_ERR(stats->d_frames_encoded);
		stats->d_frames_encoded = NULL;
		goto wfd_stats_init_fail;
	}

	stats->d_mdp_updates = debugfs_create_u32("mdp_updates", S_IRUGO,
			stats->d_parent, &stats->mdp_updates);
	if (IS_ERR(stats->d_mdp_updates)) {
		rc = PTR_ERR(stats->d_mdp_updates);
		stats->d_mdp_updates = NULL;
		goto wfd_stats_init_fail;
	}

	stats->d_enc_avg_latency = debugfs_create_u32("enc_avg_latency",
			S_IRUGO, stats->d_parent, &stats->enc_avg_latency);
	if (IS_ERR(stats->d_enc_avg_latency)) {
		rc = PTR_ERR(stats->d_enc_avg_latency);
		stats->d_enc_avg_latency = NULL;
		goto wfd_stats_init_fail;
	}

	return rc;
wfd_stats_init_fail:
	return rc;
}

int wfd_stats_update(struct wfd_stats *stats, enum wfd_stats_event event)
{
	int rc = 0;

	mutex_lock(&stats->mutex);
	switch (event) {
	case WFD_STAT_EVENT_CLIENT_QUEUE:
		stats->v4l2_buf_count++;
		break;
	case WFD_STAT_EVENT_CLIENT_DEQUEUE:
		stats->v4l2_buf_count--;

		break;
	case WFD_STAT_EVENT_MDP_QUEUE:
		stats->mdp_buf_count++;
		break;
	case WFD_STAT_EVENT_MDP_DEQUEUE:
		stats->mdp_buf_count--;
		stats->mdp_updates++;
		break;
	case WFD_STAT_EVENT_ENC_QUEUE: {
		struct wfd_stats_encode_sample *sample = NULL;

		stats->enc_buf_count++;
		stats->frames_encoded++;

		sample = kzalloc(sizeof(*sample), GFP_KERNEL);
		if (sample) {
			INIT_LIST_HEAD(&sample->list);
			sample->encode_start_ts = ktime_get();
			list_add_tail(&sample->list, &stats->enc_queue);
		} else {
			WFD_MSG_WARN("Unable to measure latency\n");
		}
		break;
	}
	case WFD_STAT_EVENT_ENC_DEQUEUE:
	{
		struct wfd_stats_encode_sample *sample = NULL;

		stats->enc_buf_count--;

		if (!list_empty(&stats->enc_queue))
			sample = list_first_entry(&stats->enc_queue,
					struct wfd_stats_encode_sample,
					list);

		if (sample) {
			ktime_t kdiff = ktime_sub(ktime_get(),
						sample->encode_start_ts);
			uint32_t diff = ktime_to_ms(kdiff);

			stats->enc_cumulative_latency += diff;
			stats->enc_latency_samples++;
			stats->enc_avg_latency = stats->enc_cumulative_latency /
				stats->enc_latency_samples;

			list_del(&sample->list);
			kfree(sample);
			sample = NULL;
		}
		break;
	}
	case WFD_STAT_EVENT_VSG_QUEUE:
		stats->vsg_buf_count++;
		break;
	case WFD_STAT_EVENT_VSG_DEQUEUE:
		stats->vsg_buf_count--;
		break;
	default:
		rc = -ENOTSUPP;
	}
	mutex_unlock(&stats->mutex);

	return rc;
}

int wfd_stats_deinit(struct wfd_stats *stats)
{
	WFD_MSG_DBG("Latencies: avg enc. latency %d",
			stats->enc_avg_latency);
	/* Delete all debugfs files in one shot :) */
	if (stats->d_parent)
		debugfs_remove_recursive(stats->d_parent);

	stats->d_parent =
	stats->d_v4l2_buf_count =
	stats->d_mdp_buf_count =
	stats->d_vsg_buf_count =
	stats->d_enc_buf_count =
	stats->d_frames_encoded =
	stats->d_mdp_updates =
	stats->d_enc_avg_latency = NULL;

	return 0;
}
