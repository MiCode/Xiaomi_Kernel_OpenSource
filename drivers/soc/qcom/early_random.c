/* Copyright (c) 2013-2014, 2016, The Linux Foundation. All rights reserved.
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
#include <linux/random.h>

#include <soc/qcom/scm.h>

#include <asm/io.h>
#include <asm/cacheflush.h>

#define TZ_SVC_CRYPTO	10
#define PRNG_CMD_ID	0x01

struct tz_prng_data {
	uint8_t		*out_buf;
	uint32_t	out_buf_sz;
} __packed;

DEFINE_SCM_BUFFER(common_scm_buf)
#define RANDOM_BUFFER_SIZE	PAGE_SIZE
char random_buffer[RANDOM_BUFFER_SIZE] __aligned(PAGE_SIZE);

void __init init_random_pool(void)
{
	struct tz_prng_data data;
	int ret;
	u32 resp;
	struct scm_desc desc;

	data.out_buf = (uint8_t *) virt_to_phys(random_buffer);
	desc.args[0] = (unsigned long) data.out_buf;
	desc.args[1] = data.out_buf_sz = SZ_512;
	desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);

	dmac_flush_range(random_buffer, random_buffer + RANDOM_BUFFER_SIZE);

	if (!is_scm_armv8())
		ret = scm_call_noalloc(TZ_SVC_CRYPTO, PRNG_CMD_ID, &data,
				sizeof(data), &resp, sizeof(resp),
				common_scm_buf,
				SCM_BUFFER_SIZE(common_scm_buf));
	else
		ret = scm_call2(SCM_SIP_FNID(TZ_SVC_CRYPTO, PRNG_CMD_ID),
					&desc);

	if (!ret) {
		dmac_inv_range(random_buffer, random_buffer +
						RANDOM_BUFFER_SIZE);
		add_device_randomness(random_buffer, SZ_512);
	}
}

