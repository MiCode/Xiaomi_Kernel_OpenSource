/* arch/arm/mach-msm/qdsp6/dal.h
 *
 * Copyright (C) 2009 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_MSM_DAL_
#define _MACH_MSM_DAL_

struct dal_client;

struct dal_info {
	uint32_t size;
	uint32_t version;
	char name[32];
};

typedef void (*dal_event_func_t)(void *data, int len, void *cookie);

struct dal_client *dal_attach(uint32_t device_id, const char *name,
			uint32_t cpu, dal_event_func_t func, void *cookie);

int dal_detach(struct dal_client *client);

int dal_call(struct dal_client *client,
	     unsigned ddi, unsigned prototype,
	     void *data, int data_len,
	     void *reply, int reply_max);

void dal_trace(struct dal_client *client);
void dal_trace_dump(struct dal_client *client);

/* function to call before panic on stalled dal calls */
void dal_set_oops(struct dal_client *client, void (*oops)(void));

/* convenience wrappers */
int dal_call_f0(struct dal_client *client, uint32_t ddi,
		uint32_t arg1);
int dal_call_f1(struct dal_client *client, uint32_t ddi,
		uint32_t arg1, uint32_t arg2);
int dal_call_f5(struct dal_client *client, uint32_t ddi,
		void *ibuf, uint32_t ilen);
int dal_call_f6(struct dal_client *client, uint32_t ddi,
		uint32_t s1, void *ibuf, uint32_t ilen);
int dal_call_f9(struct dal_client *client, uint32_t ddi,
		void *obuf, uint32_t olen);
int dal_call_f11(struct dal_client *client, uint32_t ddi,
		uint32_t s1, void *obuf, uint32_t olen);
int dal_call_f13(struct dal_client *client, uint32_t ddi, void *ibuf1,
		 uint32_t ilen1, void *ibuf2, uint32_t ilen2, void *obuf,
		 uint32_t olen);
int dal_call_f14(struct dal_client *client, uint32_t ddi, void *ibuf,
		 uint32_t ilen, void *obuf1, uint32_t olen1, void *obuf2,
		 uint32_t olen2, uint32_t *oalen2);

/* common DAL operations */
enum {
	DAL_OP_ATTACH = 0,
	DAL_OP_DETACH,
	DAL_OP_INIT,
	DAL_OP_DEINIT,
	DAL_OP_OPEN,
	DAL_OP_CLOSE,
	DAL_OP_INFO,
	DAL_OP_POWEREVENT,
	DAL_OP_SYSREQUEST,
	DAL_OP_FIRST_DEVICE_API,
};

static inline int check_version(struct dal_client *client, uint32_t version)
{
	struct dal_info info;
	int res;

	res = dal_call_f9(client, DAL_OP_INFO, &info, sizeof(struct dal_info));
	if (!res) {
		if (((info.version & 0xFFFF0000) != (version & 0xFFFF0000)) ||
		((info.version & 0x0000FFFF) <
		(version & 0x0000FFFF))) {
			res = -EINVAL;
		}
	}
	return res;
}

#endif
