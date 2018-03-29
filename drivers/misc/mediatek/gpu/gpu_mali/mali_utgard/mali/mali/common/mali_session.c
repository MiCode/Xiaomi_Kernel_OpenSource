/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_osk.h"
#include "mali_osk_list.h"
#include "mali_session.h"
#include "mali_ukk.h"
#ifdef MALI_MEM_SWAP_TRACKING
#include "mali_memory_swap_alloc.h"
#endif
#ifdef ENABLE_MTK_MEMINFO
#include "mtk_gpu_meminfo.h"
extern int g_mtk_gpu_total_memory_usage_in_pages_debugfs;
#endif

_MALI_OSK_LIST_HEAD(mali_sessions);
static u32 mali_session_count = 0;

_mali_osk_spinlock_irq_t *mali_sessions_lock = NULL;
wait_queue_head_t pending_queue;

_mali_osk_errcode_t mali_session_initialize(void)
{
	_MALI_OSK_INIT_LIST_HEAD(&mali_sessions);
	/* init wait queue for big varying job */
	init_waitqueue_head(&pending_queue);

	mali_sessions_lock = _mali_osk_spinlock_irq_init(
				     _MALI_OSK_LOCKFLAG_ORDERED,
				     _MALI_OSK_LOCK_ORDER_SESSIONS);
	if (NULL == mali_sessions_lock) {
		return _MALI_OSK_ERR_NOMEM;
	}

	return _MALI_OSK_ERR_OK;
}

void mali_session_terminate(void)
{
	if (NULL != mali_sessions_lock) {
		_mali_osk_spinlock_irq_term(mali_sessions_lock);
		mali_sessions_lock = NULL;
	}
}

void mali_session_add(struct mali_session_data *session)
{
	mali_session_lock();
	_mali_osk_list_add(&session->link, &mali_sessions);
	mali_session_count++;
	mali_session_unlock();
}

void mali_session_remove(struct mali_session_data *session)
{
	mali_session_lock();
	_mali_osk_list_delinit(&session->link);
	mali_session_count--;
	mali_session_unlock();
}

u32 mali_session_get_count(void)
{
	return mali_session_count;
}

wait_queue_head_t *mali_session_get_wait_queue(void)
{
	return &pending_queue;
}

/*
 * Get the max completed window jobs from all active session,
 * which will be used in window render frame per sec calculate
 */
#if defined(CONFIG_MALI_DVFS)
u32 mali_session_max_window_num(void)
{
	struct mali_session_data *session, *tmp;
	u32 max_window_num = 0;
	u32 tmp_number = 0;

	mali_session_lock();

	MALI_SESSION_FOREACH(session, tmp, link) {
		tmp_number = _mali_osk_atomic_xchg(
				     &session->number_of_window_jobs, 0);
		if (max_window_num < tmp_number) {
			max_window_num = tmp_number;
		}
	}

	mali_session_unlock();

	return max_window_num;
}
#endif

void mali_session_memory_tracking(_mali_osk_print_ctx *print_ctx)
{
	struct mali_session_data *session, *tmp;
	u32 mali_mem_usage;
	u32 total_mali_mem_size;
#ifdef MALI_MEM_SWAP_TRACKING
	u32 swap_pool_size;
	u32 swap_unlock_size;
#endif
#ifdef ENABLE_MTK_MEMINFO
    u32 mtk_kbase_gpu_meminfo_index = 0;
#endif /* ENABLE_MTK_MEMINFO */

	MALI_DEBUG_ASSERT_POINTER(print_ctx);
	mali_session_lock();
	MALI_SESSION_FOREACH(session, tmp, link) {
#ifdef MALI_MEM_SWAP_TRACKING
		_mali_osk_ctxprintf(print_ctx, "  %-25s  %-10u  %-10u  %-15u  %-15u  %-10u  %-10u  %-10u\n",
				    session->comm, session->pid,
				    (atomic_read(&session->mali_mem_allocated_pages)) * _MALI_OSK_MALI_PAGE_SIZE,
				    (unsigned int)session->max_mali_mem_allocated_size,
				    (unsigned int)((atomic_read(&session->mali_mem_array[MALI_MEM_EXTERNAL])) * _MALI_OSK_MALI_PAGE_SIZE),
				    (unsigned int)((atomic_read(&session->mali_mem_array[MALI_MEM_UMP])) * _MALI_OSK_MALI_PAGE_SIZE),
				    (unsigned int)((atomic_read(&session->mali_mem_array[MALI_MEM_DMA_BUF])) * _MALI_OSK_MALI_PAGE_SIZE),
				    (unsigned int)((atomic_read(&session->mali_mem_array[MALI_MEM_SWAP])) * _MALI_OSK_MALI_PAGE_SIZE)
				   );
#else
		_mali_osk_ctxprintf(print_ctx, "  %-25s  %-10u  %-10u  %-15u  %-15u  %-10u  %-10u  \n",
				    session->comm, session->pid,
				    (unsigned int)((atomic_read(&session->mali_mem_allocated_pages)) * _MALI_OSK_MALI_PAGE_SIZE),
				    (unsigned int)session->max_mali_mem_allocated_size,
				    (unsigned int)((atomic_read(&session->mali_mem_array[MALI_MEM_EXTERNAL])) * _MALI_OSK_MALI_PAGE_SIZE),
				    (unsigned int)((atomic_read(&session->mali_mem_array[MALI_MEM_UMP])) * _MALI_OSK_MALI_PAGE_SIZE),
				    (unsigned int)((atomic_read(&session->mali_mem_array[MALI_MEM_DMA_BUF])) * _MALI_OSK_MALI_PAGE_SIZE)
				   );
#endif
#ifdef ENABLE_MTK_MEMINFO
		mtk_gpu_meminfo_set(mtk_kbase_gpu_meminfo_index, session->pid, (atomic_read(&session->mali_mem_allocated_pages)));
		mtk_kbase_gpu_meminfo_index++;
#endif /* ENABLE_MTK_MEMINFO */
	}
	mali_session_unlock();
	mali_mem_usage  = _mali_ukk_report_memory_usage();
#ifdef ENABLE_MTK_MEMINFO
    g_mtk_gpu_total_memory_usage_in_pages_debugfs = mali_mem_usage/4096;
#endif /* ENABLE_MTK_MEMINFO */
	total_mali_mem_size = _mali_ukk_report_total_memory_size();
	_mali_osk_ctxprintf(print_ctx, "Mali mem usage: %u\nMali mem limit: %u\n", mali_mem_usage, total_mali_mem_size);
#ifdef MALI_MEM_SWAP_TRACKING
	mali_mem_swap_tracking(&swap_pool_size, &swap_unlock_size);
	_mali_osk_ctxprintf(print_ctx, "Mali swap mem pool : %u\nMali swap mem unlock: %u\n", swap_pool_size, swap_unlock_size);
#endif
}
