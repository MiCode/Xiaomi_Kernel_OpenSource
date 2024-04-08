/* SPDX-License-Identifier: GPL-2.0
 */
#ifndef _RESERVE_DYNAMIC_PAGE_POOL_H
#define _RESERVE_DYNAMIC_PAGE_POOL_H

extern inline bool can_do_shrink(struct dynamic_page_pool *pool, bool high);
extern inline bool is_reserve_vmid(int vmid);

#endif /* _RESERVE_DYNAMIC_PAGE_POOL_H */
