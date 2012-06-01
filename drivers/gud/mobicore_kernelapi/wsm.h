/** @addtogroup MCD_MCDIMPL_DAEMON_SRV
 * @{
 * @file
 *
 * World shared memory definitions.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef WSM_H_
#define WSM_H_

#include "common.h"
#include <linux/list.h>

struct wsm {
	void *virt_addr;
	uint32_t len;
	uint32_t handle;
	void *phys_addr;
	struct list_head list;
};

struct wsm *wsm_create(
	void	*virt_addr,
	uint32_t  len,
	uint32_t  handle,
	void	*phys_addr /*= NULL this may be unknown, so is can be omitted.*/
);
#endif /* WSM_H_ */

/** @} */
