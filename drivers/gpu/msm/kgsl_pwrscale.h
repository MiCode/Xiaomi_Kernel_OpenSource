/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#ifndef __KGSL_PWRSCALE_H
#define __KGSL_PWRSCALE_H

struct kgsl_pwrscale;

struct kgsl_pwrscale_policy  {
	const char *name;
	int (*init)(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale);
	void (*close)(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale);
	void (*idle)(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale);
	void (*busy)(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale);
	void (*sleep)(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale);
	void (*wake)(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale);
};

struct kgsl_pwrscale {
	struct kgsl_pwrscale_policy *policy;
	struct kobject kobj;
	void *priv;
	int gpu_busy;
	int enabled;
};

struct kgsl_pwrscale_policy_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale, char *buf);
	ssize_t (*store)(struct kgsl_device *device,
			 struct kgsl_pwrscale *pwrscale, const char *buf,
			 size_t count);
};

#define PWRSCALE_POLICY_ATTR(_name, _mode, _show, _store)          \
	struct kgsl_pwrscale_policy_attribute policy_attr_##_name = \
		__ATTR(_name, _mode, _show, _store)

extern struct kgsl_pwrscale_policy kgsl_pwrscale_policy_tz;
extern struct kgsl_pwrscale_policy kgsl_pwrscale_policy_idlestats;
extern struct kgsl_pwrscale_policy kgsl_pwrscale_policy_msm;

int kgsl_pwrscale_init(struct kgsl_device *device);
void kgsl_pwrscale_close(struct kgsl_device *device);

int kgsl_pwrscale_attach_policy(struct kgsl_device *device,
	struct kgsl_pwrscale_policy *policy);
void kgsl_pwrscale_detach_policy(struct kgsl_device *device);

void kgsl_pwrscale_idle(struct kgsl_device *device);
void kgsl_pwrscale_busy(struct kgsl_device *device);
void kgsl_pwrscale_sleep(struct kgsl_device *device);
void kgsl_pwrscale_wake(struct kgsl_device *device);

void kgsl_pwrscale_enable(struct kgsl_device *device);
void kgsl_pwrscale_disable(struct kgsl_device *device);

int kgsl_pwrscale_policy_add_files(struct kgsl_device *device,
				   struct kgsl_pwrscale *pwrscale,
				   struct attribute_group *attr_group);

void kgsl_pwrscale_policy_remove_files(struct kgsl_device *device,
				       struct kgsl_pwrscale *pwrscale,
				       struct attribute_group *attr_group);
#endif
