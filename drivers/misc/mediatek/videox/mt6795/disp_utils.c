#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/time.h>
#include <mach/m4u.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "disp_drv_log.h"
#include "disp_utils.h"

int disp_sw_mutex_lock(struct mutex*m)
{
	mutex_lock(m);
	return 0;
}

int disp_mutex_trylock(struct mutex*m)
{
	int ret = 0;
	ret = mutex_trylock(m);
	return ret;
}


int disp_sw_mutex_unlock(struct mutex*m)
{
	mutex_unlock(m);
	return 0;
}

int disp_msleep(unsigned int ms)
{
	msleep(ms);
	return 0;
}

long int disp_get_time_us(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

unsigned int disp_allocate_mva(unsigned int pa, unsigned int size,  M4U_PORT_ID port)
{
	int ret = 0;
	unsigned int mva = 0;
	m4u_client_t *client = NULL;
	struct sg_table *sg_table = kzalloc(sizeof(struct sg_table), GFP_ATOMIC);		
	sg_alloc_table(sg_table, 1, GFP_KERNEL);
	
	sg_dma_address(sg_table->sgl) = pa;
	sg_dma_len(sg_table->sgl) = size;
	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
	{
		DISPMSG("create client fail!\n");
	}
	
	mva = pa;
	ret = m4u_alloc_mva(client, port, 0, sg_table, size, M4U_PROT_READ |M4U_PROT_WRITE, M4U_FLAGS_FIX_MVA, &mva);
	//m4u_alloc_mva(M4U_PORT_DISP_OVL0, pa_start, (pa_end - pa_start + 1), 0, 0, mva);
	if(ret)
	{
		DISPMSG("m4u_alloc_mva returns fail: %d\n", ret);
	}
	DISPMSG("[DISPHAL] FB MVA is 0x%08X PA is 0x%08X\n", mva, pa);

	return mva;
}
