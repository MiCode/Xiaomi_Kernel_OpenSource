#include "ia_css_i_rmgr.h"

#include <stdbool.h>
#include <assert_support.h>


void ia_css_i_host_rmgr_init(void)
{
	ia_css_i_host_rmgr_init_vbuf(vbuf_ref);
	ia_css_i_host_rmgr_init_vbuf(vbuf_write);
	ia_css_i_host_rmgr_init_vbuf(hmm_buffer_pool);
}

void ia_css_i_host_rmgr_uninit(void)
{
	ia_css_i_host_rmgr_uninit_vbuf(hmm_buffer_pool);
	ia_css_i_host_rmgr_uninit_vbuf(vbuf_write);
	ia_css_i_host_rmgr_uninit_vbuf(vbuf_ref);
}

