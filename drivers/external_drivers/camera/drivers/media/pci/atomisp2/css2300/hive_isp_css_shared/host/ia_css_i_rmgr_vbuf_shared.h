#ifndef __IA_CSS_I_RMGR_VBUF_HOST_SHARED_H_INCLUDED__
#define __IA_CSS_I_RMGR_VBUF_HOST_SHARED_H_INCLUDED__

#include "ia_css_i_rmgr_vbuf_public.h"

#include <stdint.h>

#include "sh_css_internal.h"

/* complete the types for the sp implementation of the dma resource pool */

struct ia_css_i_host_rmgr_vbuf_handle {
	hrt_vaddress vptr;
	uint8_t count;
	uint32_t size;
};

struct ia_css_i_host_rmgr_vbuf_pool {
	uint8_t copy_on_write;
	uint8_t recycle;
	uint32_t size;
	uint32_t index;
	struct ia_css_i_host_rmgr_vbuf_handle **handles;
};

extern struct ia_css_i_host_rmgr_vbuf_pool *vbuf_ref;
extern struct ia_css_i_host_rmgr_vbuf_pool *vbuf_write;
extern struct ia_css_i_host_rmgr_vbuf_pool *hmm_buffer_pool;

#endif /* __IA_CSS_I_RMGR_VBUF_HOST_SHARED_H_INCLUDED__ */
