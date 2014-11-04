#include "ia_css_i_rmgr.h"

#include <stdbool.h>
#include <assert_support.h>

#include "memory_access.h"

#include "sh_css_debug.h"

#define NUM_HANDLES 1000
struct ia_css_i_host_rmgr_vbuf_handle handle_table[NUM_HANDLES];

struct ia_css_i_host_rmgr_vbuf_pool refpool = {
	.copy_on_write = false,
	.recycle = false,
};
struct ia_css_i_host_rmgr_vbuf_pool writepool = {
	.copy_on_write = true,
	.recycle = false,
	.size = 0,
};
struct ia_css_i_host_rmgr_vbuf_pool hmmbufferpool = {
	.copy_on_write = true,
	.recycle = true,
	.size = 20,
};

struct ia_css_i_host_rmgr_vbuf_pool *vbuf_ref = &refpool;
struct ia_css_i_host_rmgr_vbuf_pool *vbuf_write = &writepool;
struct ia_css_i_host_rmgr_vbuf_pool *hmm_buffer_pool = &hmmbufferpool;



static void ia_css_i_host_refcount_init_vbuf(void)
{
	/* initialize the refcount table */
	memset(&handle_table, 0, sizeof(handle_table));
}

void ia_css_i_host_refcount_retain_vbuf(
		struct ia_css_i_host_rmgr_vbuf_handle **handle)
{
	int i;
	struct ia_css_i_host_rmgr_vbuf_handle *h;
	assert(handle != NULL);
	if(handle == NULL)
		return;
	assert(*handle != NULL);
	if(*handle == NULL)
		return;
	/* new vbuf to count on */
	if ((*handle)->count == 0) {
		h = *handle;
		*handle = NULL;
		for (i = 0; i < NUM_HANDLES; i++) {
			if (handle_table[i].count == 0) {
				*handle = &handle_table[i];
				break;
			}
		}
		assert(*handle != NULL);
		/* Klockwork pacifier */
		if(*handle == NULL)
			return;
		(*handle)->vptr = h->vptr;
		(*handle)->size = h->size;
	}
	(*handle)->count++;
}


void ia_css_i_host_refcount_release_vbuf(
		struct ia_css_i_host_rmgr_vbuf_handle **handle)
{
	assert(handle != NULL);
	if(handle == NULL)
		return;
	assert(*handle != NULL);
	if(*handle == NULL)
		return;
	assert((*handle)->count != 0);
	/* decrease reference count */
	(*handle)->count--;
	/* remove from admin */
	if ((*handle)->count == 0) {
		(*handle)->vptr = 0x0;
		(*handle)->size = 0;
		*handle = NULL;
	}
}

void ia_css_i_host_rmgr_init_vbuf(struct ia_css_i_host_rmgr_vbuf_pool *pool)
{
	size_t bytes_needed;
	ia_css_i_host_refcount_init_vbuf();
	assert(pool != NULL);
	if (pool == NULL)
		return;
	/* initialize the recycle pool if used */
	if (pool->recycle && pool->size) {
		/* allocate memory for storing the handles */
		bytes_needed =
			sizeof(struct ia_css_i_host_rmgr_vbuf_handle *) *
			pool->size;
		pool->handles = sh_css_malloc(bytes_needed);
		assert(pool->handles != NULL);
		if (pool->handles == NULL) {
			pool->size = 0;
			return;
		}
		memset(pool->handles, 0, bytes_needed);
	}
	else {
		/* just in case, set the size to 0 */
		pool->size = 0;
		pool->handles = NULL;
	}
}

void ia_css_i_host_rmgr_uninit_vbuf(struct ia_css_i_host_rmgr_vbuf_pool *pool)
{
	uint32_t i;
	sh_css_dtrace(SH_DBG_TRACE,
		"ia_css_i_host_rmgr_uninit_vbuf()\n");
	assert(pool != NULL);
	if(pool == NULL)
		return;
	if (pool->handles != NULL) {
		/* free the hmm buffers */
		for (i = 0; i < pool->size; i++) {
			if (pool->handles[i] != NULL) {
				sh_css_dtrace(SH_DBG_TRACE,
					"   freeing/releasing %x (count=%d)\n",
					pool->handles[i]->vptr,
					pool->handles[i]->count);
				/* free memory */
				mmgr_free(pool->handles[i]->vptr);
				/* remove from refcount admin*/
				ia_css_i_host_refcount_release_vbuf(
						&pool->handles[i]);
			}
		}
		/* now free the pool handles list */
		sh_css_free(pool->handles);
		pool->handles = NULL;
	}
}

static
void ia_css_i_host_rmgr_push_handle(
	struct ia_css_i_host_rmgr_vbuf_pool *pool,
	struct ia_css_i_host_rmgr_vbuf_handle **handle)
{
	uint32_t i;
	bool succes = false;
	assert(pool != NULL);
	if(pool == NULL)
		return;
	assert(pool->recycle);
	assert(pool->handles != NULL);
	if(pool->handles == NULL)
		return;
	assert(handle != NULL);
	if(handle == NULL)
		return;
	for (i = 0; i < pool->size; i++) {
		if (pool->handles[i] == NULL) {
			ia_css_i_host_refcount_retain_vbuf(handle);
			pool->handles[i] = *handle;
			succes = true;
			break;
		}
	}
	assert(succes);
}

static
void ia_css_i_host_rmgr_pop_handle(
	struct ia_css_i_host_rmgr_vbuf_pool *pool,
	struct ia_css_i_host_rmgr_vbuf_handle **handle)
{
	uint32_t i;
	bool succes = false;
	assert(pool != NULL);
	if(pool == NULL)
		return;
	assert(pool->recycle);
	assert(pool->handles != NULL);
	if(pool->handles == NULL)
		return;
	assert(handle != NULL);
	if(handle == NULL)
		return;
	for (i = 0; i < pool->size; i++) {
		if (pool->handles[i] != NULL && pool->handles[i]->size == (*handle)->size) {
			*handle = pool->handles[i];
			pool->handles[i] = NULL;
			/* dont release, we are returning it...
			   ia_css_i_host_refcount_release_vbuf(handle); */
			succes = true;
			break;
		}
	}
}

void ia_css_i_host_rmgr_acq_vbuf(
	struct ia_css_i_host_rmgr_vbuf_pool *pool,
	struct ia_css_i_host_rmgr_vbuf_handle **handle)
{
	struct ia_css_i_host_rmgr_vbuf_handle h;
	assert(pool != NULL);
	if(pool == NULL)
		return;
	assert(handle != NULL);
	if(handle == NULL)
		return;
	if (pool->copy_on_write) {
		/* only one reference, reuse (no new retain) */
		if ((*handle)->count == 1)
			return;
		/* more than one reference, release current buffer */
		if ((*handle)->count > 1) {
			/* store current values */
			h.vptr = 0x0;
			h.size = (*handle)->size;
			/* release ref to current buffer */
			ia_css_i_host_refcount_release_vbuf(handle);
			*handle = &h;
		}
		/* get new buffer for needed size */
		if ((*handle)->vptr == 0x0) {
			if (pool->recycle) {
				/* try and pop from pool */
				ia_css_i_host_rmgr_pop_handle(pool, handle);
				if ((*handle)->vptr != 0x0) {
					/* we popped a buffer */
					return;
				}
			}
			/* we need to allocate */
			(*handle)->vptr = mmgr_alloc_attr((*handle)->size, 0);
		}
	}
	/* Note that handle will change to an internally maintained one */
	ia_css_i_host_refcount_retain_vbuf(handle);
}

void ia_css_i_host_rmgr_rel_vbuf(
	struct ia_css_i_host_rmgr_vbuf_pool *pool,
	struct ia_css_i_host_rmgr_vbuf_handle **handle)
{
	assert(pool != NULL);
	if(pool == NULL)
		return;
	assert(handle != NULL);
	if(handle == NULL)
		return;
	assert(*handle != NULL);
	if(*handle == NULL)
		return;
	/* release the handle */
	if ((*handle)->count == 1) {
		if (!pool->recycle) {
			/* non recycling pool, free mem */
			mmgr_free((*handle)->vptr);
		}
		else {
			/* recycle to pool */
			ia_css_i_host_rmgr_push_handle(pool, handle);
		}
	}
	ia_css_i_host_refcount_release_vbuf(handle);
	*handle = NULL;
}

