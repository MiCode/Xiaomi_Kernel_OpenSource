/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_misc.h"
#include "ist30xx_tracking.h"

void ist30xx_tracking_init(struct ist30xx_data *data)
{
	if (data->tracking_initialize)
		return;

	data->pTrackBuf = &data->TrackBuf;

	data->pTrackBuf->RingBufCtr = 0;
	data->pTrackBuf->RingBufInIdx = 0;
	data->pTrackBuf->RingBufOutIdx = 0;

	data->tracking_initialize = true;
}

void ist30xx_tracking_deinit(void)
{
}

int ist30xx_get_track(struct ist30xx_data *data, u32 *track, int cnt)
{
	int i;
	u8 *buf = (u8 *)track;
	unsigned long flags;
	spinlock_t mr_lock = __SPIN_LOCK_UNLOCKED();

	cnt *= sizeof(track[0]);

	if (data->pTrackBuf->RingBufCtr < (u16)cnt)
		return IST30XX_RINGBUF_NOT_ENOUGH;

	spin_lock_irqsave(&mr_lock, flags);

	for (i = 0; i < cnt; i++) {
		if (data->pTrackBuf->RingBufOutIdx == IST30XX_MAX_LOG_SIZE)
			data->pTrackBuf->RingBufOutIdx = 0;

		*buf++ = (u8)data->pTrackBuf->LogBuf[data->pTrackBuf->RingBufOutIdx++];
		data->pTrackBuf->RingBufCtr--;
	}

	spin_unlock_irqrestore(&mr_lock, flags);

	return IST30XX_RINGBUF_NO_ERR;
}

int ist30xx_get_track_cnt(struct ist30xx_data *data)
{
	return data->pTrackBuf->RingBufCtr;
}

#if IST30XX_TRACKING_MODE
int ist30xx_put_track(struct ist30xx_data *data, u32 *track, int cnt)
{
	int i;
	u8 *buf = (u8 *)track;
	unsigned long flags;
	spinlock_t mr_lock = __SPIN_LOCK_UNLOCKED();

	spin_lock_irqsave(&mr_lock, flags);

	cnt *= sizeof(track[0]);

	data->pTrackBuf->RingBufCtr += cnt;
	if (data->pTrackBuf->RingBufCtr > IST30XX_MAX_LOG_SIZE) {
		data->pTrackBuf->RingBufOutIdx +=
			(data->pTrackBuf->RingBufCtr - IST30XX_MAX_LOG_SIZE);
		if (data->pTrackBuf->RingBufOutIdx >= IST30XX_MAX_LOG_SIZE)
			data->pTrackBuf->RingBufOutIdx -= IST30XX_MAX_LOG_SIZE;

		data->pTrackBuf->RingBufCtr = IST30XX_MAX_LOG_SIZE;
	}

	for (i = 0; i < cnt; i++) {
		if (data->pTrackBuf->RingBufInIdx == IST30XX_MAX_LOG_SIZE)
			data->pTrackBuf->RingBufInIdx = 0;
		data->pTrackBuf->LogBuf[data->pTrackBuf->RingBufInIdx++] = *buf++;
	}

	spin_unlock_irqrestore(&mr_lock, flags);

	return IST30XX_RINGBUF_NO_ERR;
}

int ist30xx_put_track_ms(struct ist30xx_data *data, u32 ms)
{
	ms &= 0x0000FFFF;
	ms |= IST30XX_TRACKING_MAGIC;

	return ist30xx_put_track(data, &ms, 1);
}

static struct timespec t_track;
int ist30xx_tracking(struct ist30xx_data *data, u32 status)
{
	u32 ms;

	if (!data->tracking_initialize)
		ist30xx_tracking_init(data);

	ktime_get_ts(&t_track);
	ms = t_track.tv_sec * 1000 + t_track.tv_nsec / 1000000;

	ist30xx_put_track_ms(data, ms);
	ist30xx_put_track(data, &status, 1);

	return 0;
}
#else
int ist30xx_put_track(struct ist30xx_data *data, u32 *track, int cnt)
{
	return 0;
}
int ist30xx_put_track_ms(struct ist30xx_data *data, u32 ms)
{
	return 0;
}
int ist30xx_tracking(struct ist30xx_data *data, u32 status)
{
	return 0;
}
#endif // IST30XX_TRACKING_MODE

#define MAX_TRACKING_COUNT      (1024)
struct timespec t_curr;      // ns

/* sysfs: /sys/class/touch/tracking/track_frame */
ssize_t ist30xx_track_frame_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	int i, buf_cnt = 0;
	u32 track_cnt = MAX_TRACKING_COUNT;
	u32 track;
	char msg[10];
	struct ist30xx_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->ist30xx_mutex);

	buf[0] = '\0';

	if (track_cnt > ist30xx_get_track_cnt(data))
		track_cnt = ist30xx_get_track_cnt(data);

	track_cnt /= sizeof(track);

	tsp_verb("num: %d of %d\n", track_cnt, ist30xx_get_track_cnt(data));

	for (i = 0; i < track_cnt; i++) {
		ist30xx_get_track(data, &track, 1);

		tsp_verb("%08X\n", track);

		buf_cnt += sprintf(msg, "%08x", track);
		strcat(buf, msg);
	}

	mutex_unlock(&data->ist30xx_mutex);

	return buf_cnt;
}

/* sysfs: /sys/class/touch/tracking/track_cnt */
ssize_t ist30xx_track_cnt_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	u32 val = (u32)ist30xx_get_track_cnt(data);

	tsp_verb("tracking cnt: %d\n", val);

	return sprintf(buf, "%08x", val);
}


/* sysfs  */
static DEVICE_ATTR(track_frame, S_IRUGO, ist30xx_track_frame_show, NULL);
static DEVICE_ATTR(track_cnt, S_IRUGO, ist30xx_track_cnt_show, NULL);

static struct attribute *tracking_attributes[] = {
	&dev_attr_track_frame.attr,
	&dev_attr_track_cnt.attr,
	NULL,
};

static struct attribute_group tracking_attr_group = {
	.attrs	= tracking_attributes,
};

int ist30xx_init_tracking_sysfs(struct ist30xx_data *data)
{
	/* /sys/class/touch/tracking */
	data->tracking_dev = device_create(data->ist30xx_class, NULL, 0, data, "tracking");

	/* /sys/class/touch/tracking/... */
	if (sysfs_create_group(&data->tracking_dev->kobj, &tracking_attr_group))
		tsp_err("[ TSP ] Failed to create sysfs group(%s)!\n", "tracking");

	ist30xx_tracking_init(data);

	return 0;
}
