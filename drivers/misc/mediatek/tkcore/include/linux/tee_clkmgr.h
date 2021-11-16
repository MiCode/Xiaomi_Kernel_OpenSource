/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TEE_CLKMGR_H
#define TEE_CLKMGR_H

/* TKCORE clk manager framework assumes that
 * the clock enable/disable operations are
 * symmetric, that is, a clock enabling operation
 * requires the same amount of arguments as
 * the clock disabling operation.
 *
 * TKCORE Clk mgr supports at the most 3 arguments
 * for a clk operation
 */

int tee_clkmgr_register(const char *clkname, int master_id,
			void *enable_fn, void *disable_fn,
			void *p1, void *p2, void *p3, size_t argnum);

#define tee_clkmgr_register0(clkname, id, e, d) \
	tee_clkmgr_register((clkname), (id), \
		(void *) (e), (void *) (d), \
		NULL, NULL, NULL, 0)

#define tee_clkmgr_register1(clkname, id, e, d, p0) \
	tee_clkmgr_register((clkname), (id), \
		(void *) (e), (void *) (d), \
		(void *) (p0), NULL, NULL, 1)

#define tee_clkmgr_register2(clkname, id, e, d, p0, p1)\
	tee_clkmgr_register((clkname), (id), \
		(void *) (e), (void *) (d), \
		(void *) (p0), (void *) (p1), NULL, 2)

#define tee_clkmgr_register3(clkname, id, e, d, p0, p1, p2)\
	tee_clkmgr_register((clkname), (id), \
		(void *) (e), (void *) (d), \
		(void *) (p0), (void *) (p1), (void *) (p2), 3)

#define TEE_CLKMGR_TOKEN_NOT_LEGACY	(0x1)

#define TEE_CLKMGR_TOKEN_ID_SHIFT	(1)
#define TEE_CLKMGR_TOKEN_TYPE_MASK	(0xffffu)
#define TEE_CLKMGR_TOKEN_TYPE_SHIFT	(16)
#define TEE_CLKMGR_TOKEN(type, id)	\
	(((type) << TEE_CLKMGR_TOKEN_TYPE_SHIFT) | \
	((id) << TEE_CLKMGR_TOKEN_ID_SHIFT) | \
	TEE_CLKMGR_TOKEN_NOT_LEGACY)

#define TEE_CLKMGR_OP_ENABLE		(0x1)

int tee_clkmgr_handle(uint32_t token, uint32_t op);

#endif
