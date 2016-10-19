/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>

#include <mach/scm.h>

#include <asm/io.h>
#include <asm/cacheflush.h>

#define TZ_SVC_CRYPTO	10
#define PRNG_CMD_ID	0x01

static int use_arch_random = 1;
struct tz_prng_data {
	uint8_t		*out_buf;
	uint32_t	out_buf_sz;
} __packed;

DEFINE_SCM_BUFFER(common_scm_buf)
DEFINE_MUTEX(arch_random_lock);
#define RANDOM_BUFFER_SIZE	PAGE_SIZE
char random_buffer[RANDOM_BUFFER_SIZE] __aligned(PAGE_SIZE);

int arch_get_random_common(void *v, size_t size)
{
	struct tz_prng_data data;
	int ret;
	u32 resp;

	if (!use_arch_random)
		return 0;

	if (size > sizeof(random_buffer))
		return 0;

	mutex_lock(&arch_random_lock);
	data.out_buf = (uint8_t *) virt_to_phys(random_buffer);
	data.out_buf_sz = size;

	ret = scm_call_noalloc(TZ_SVC_CRYPTO, PRNG_CMD_ID, &data,
			sizeof(data), &resp, sizeof(resp),
			common_scm_buf, SCM_BUFFER_SIZE(common_scm_buf));
	if (!ret) {
		dmac_inv_range(random_buffer, random_buffer +
						RANDOM_BUFFER_SIZE);
		outer_inv_range(
			(unsigned long) virt_to_phys(random_buffer),
			(unsigned long) virt_to_phys(random_buffer) +
						RANDOM_BUFFER_SIZE);
		memcpy(v, random_buffer, size);
	}
	mutex_unlock(&arch_random_lock);
	return !ret;
}

int arch_get_random_long(unsigned long *v)
{
	return arch_get_random_common(v, sizeof(unsigned long));
}

int arch_get_random_int(unsigned int *v)
{
	return arch_get_random_common(v, sizeof(unsigned int));
}

int arch_random_init(void)
{
	use_arch_random = 0;

	return 0;
}
module_init(arch_random_init);
