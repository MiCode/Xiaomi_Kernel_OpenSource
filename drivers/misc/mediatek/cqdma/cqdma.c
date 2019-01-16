#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#ifdef CONFIG_MTK_GIC
#include <linux/irqchip/mt-gic.h>
#endif
#include <asm/io.h>
#include <mach/dma.h>
#include <mach/sync_write.h>
#include <mach/mt_clkmgr.h>
#include <mach/emi_mpu.h>

struct cqdma_env_info {
	void __iomem *base;
	u32 irq;
	u32 nr_channel;
};

static struct cqdma_env_info env_info;

#define LDVT

/* 
 * DMA information
 */

#define GDMA_START          (0)

/*
 * General DMA channel register mapping
 */
#define DMA_INT_FLAG           IOMEM((env_info.base + 0x0000))
#define DMA_INT_EN             IOMEM((env_info.base + 0x0004))
#define DMA_START              IOMEM((env_info.base + 0x0008))
#define DMA_RESET              IOMEM((env_info.base + 0x000C))
#define DMA_STOP               IOMEM((env_info.base + 0x0010))
#define DMA_FLUSH              IOMEM((env_info.base + 0x0014))
#define DMA_CON                IOMEM((env_info.base + 0x0018))
#define DMA_SRC                IOMEM((env_info.base + 0x001C))
#define DMA_DST                IOMEM((env_info.base + 0x0020))
#define DMA_LEN1               IOMEM((env_info.base + 0x0024))
#define DMA_LEN2               IOMEM((env_info.base + 0x0028))
#define DMA_JUMP_ADDR          IOMEM((env_info.base + 0x002C))
#define DMA_IBUFF_SIZE         IOMEM((env_info.base + 0x0030))
#define DMA_CONNECT            IOMEM((env_info.base + 0x0034))
#define DMA_AXIATTR            IOMEM((env_info.base + 0x0038))
#define DMA_DBG_STAT           IOMEM((env_info.base + 0x0050))

#define DMA_SRC_4G_SUPPORT           (env_info.base + 0x0040)
#define DMA_DST_4G_SUPPORT       (env_info.base + 0x0044)
#define DMA_JUMP_4G_SUPPORT       (env_info.base + 0x0048)
#define DMA_VIO_DBG1           (env_info.base + 0x003c)
#define DMA_VIO_DBG           (env_info.base + 0x0060)
#define DMA_GDMA_SEC_EN           (env_info.base + 0x0058)

/*
 * Register Setting
 */

#define DMA_GDMA_LEN_MAX_MASK   (0x000FFFFF)

#define DMA_CON_DIR             (0x00000001)
#define DMA_CON_FPEN            (0x00000002)    /* Use fix pattern. */
#define DMA_CON_SLOW_EN         (0x00000004)
#define DMA_CON_DFIX            (0x00000008)
#define DMA_CON_SFIX            (0x00000010)
#define DMA_CON_WPEN            (0x00008000)
#define DMA_CON_WPSD            (0x00100000)
#define DMA_CON_WSIZE_1BYTE     (0x00000000)
#define DMA_CON_WSIZE_2BYTE     (0x01000000)
#define DMA_CON_WSIZE_4BYTE     (0x02000000)
#define DMA_CON_RSIZE_1BYTE     (0x00000000)
#define DMA_CON_RSIZE_2BYTE     (0x10000000)
#define DMA_CON_RSIZE_4BYTE     (0x20000000)
#define DMA_CON_BURST_MASK      (0x00070000)
#define DMA_CON_SLOW_OFFSET     (5)
#define DMA_CON_SLOW_MAX_MASK   (0x000003FF)

#define DMA_START_BIT           (0x00000001)
#define DMA_STOP_BIT            (0x00000000)
#define DMA_INT_FLAG_BIT        (0x00000001)
#define DMA_INT_FLAG_CLR_BIT    (0x00000000)
#define DMA_INT_EN_BIT          (0x00000001)
#define DMA_FLUSH_BIT           (0x00000001)
#define DMA_FLUSH_CLR_BIT       (0x00000000)
#define DMA_UART_RX_INT_EN_BIT  (0x00000003)
#define DMA_INT_EN_CLR_BIT      (0x00000000)
#define DMA_WARM_RST_BIT        (0x00000001)
#define DMA_HARD_RST_BIT        (0x00000002)
#define DMA_HARD_RST_CLR_BIT    (0x00000000)
#define DMA_READ_COHER_BIT      (0x00000010)
#define DMA_WRITE_COHER_BIT     (0x00100000)
#define DMA_GSEC_EN_BIT         (0x00000001)
#define DMA_SEC_EN_BIT          (0x00000001)



/*
 * Register Limitation
 */

#define MAX_TRANSFER_LEN1   (0xFFFFF)
#define MAX_TRANSFER_LEN2   (0xFFFFF)
#define MAX_SLOW_DOWN_CNTER (0x3FF)

/*
 * channel information structures
 */

struct dma_ctrl
{
    int in_use;
    void (*isr_cb)(void *);
    void *data;
};

/*
 * global variables
 */

#define CQDMA_MAX_CHANNEL			(8)

static struct dma_ctrl dma_ctrl[CQDMA_MAX_CHANNEL];
static DEFINE_SPINLOCK(dma_drv_lock);

#define PDN_APDMA_MODULE_NAME ("CQDMA")
#define GDMA_WARM_RST_TIMEOUT   (100) // ms
volatile unsigned int DMA_INT_DONE;
/*
 * mt_req_gdma: request a general DMA.
 * @chan: specify a channel or not
 * Return channel number for success; return negative errot code for failure.
 */
int mt_req_gdma(DMA_CHAN chan)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&dma_drv_lock, flags);

	if (chan == GDMA_ANY) {
		for (i = GDMA_START; i < env_info.nr_channel; i++) {
			if (dma_ctrl[i].in_use) {
				continue;
			} else {
				dma_ctrl[i].in_use = 1;
				break;
			}
		}
	} else {
		if (dma_ctrl[chan].in_use) {
			i = env_info.nr_channel;
		} else {
			i = chan;
			dma_ctrl[chan].in_use = 1;
		}
	}

	spin_unlock_irqrestore(&dma_drv_lock, flags);

	if (i < env_info.nr_channel) {
		mt_reset_gdma_conf(i);
		return i;
	} else {
		return -DMA_ERR_NO_FREE_CH;
	}
}
EXPORT_SYMBOL(mt_req_gdma);

/*
 * mt_start_gdma: start the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to start
 * Return 0 for success; return negative errot code for failure.
 */
int mt_start_gdma(int channel)
{
	if ((channel < GDMA_START) || (channel >= (GDMA_START + env_info.nr_channel))) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG);
	mt_reg_sync_writel(DMA_START_BIT, DMA_START);

	return 0;
}
EXPORT_SYMBOL(mt_start_gdma);

/*
 * mt_polling_gdma: wait the DMA to finish for the specified GDMA channel
 * @channel: GDMA channel to polling
 * @timeout: polling timeout in ms
 * Return 0 for success; 
 * Return 1 for timeout
 * return negative errot code for failure.
 */
int mt_polling_gdma(int channel, unsigned long timeout)
{
	if (channel < GDMA_START) {
		return -DMA_ERR_INVALID_CH;
	}

	if (channel >= (GDMA_START + env_info.nr_channel)) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	timeout = jiffies + ((HZ * timeout) / 1000); 

	do {
		if (time_after(jiffies, timeout)) {
			pr_err("GDMA_%d polling timeout !!\n", channel);
			mt_dump_gdma(channel);
			return 1;
		}
	} while (readl(DMA_START));

	return 0;
}
EXPORT_SYMBOL(mt_polling_gdma);

/*
 * mt_stop_gdma: stop the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to stop
 * Return 0 for success; return negative errot code for failure.
 */
int mt_stop_gdma(int channel)
{
	if (channel < GDMA_START) {
		return -DMA_ERR_INVALID_CH;
	}

	if (channel >= (GDMA_START + env_info.nr_channel)) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	mt_reg_sync_writel(DMA_FLUSH_BIT, DMA_FLUSH);
	while (readl(DMA_START));
	mt_reg_sync_writel(DMA_FLUSH_CLR_BIT, DMA_FLUSH);
	mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG);

	return 0;
}
EXPORT_SYMBOL(mt_stop_gdma);

/*
 * mt_config_gdma: configure the given GDMA channel.
 * @channel: GDMA channel to configure
 * @config: pointer to the mt_gdma_conf structure in which the GDMA configurations store
 * @flag: ALL, SRC, DST, or SRC_AND_DST.
 * Return 0 for success; return negative errot code for failure.
 */
int mt_config_gdma(int channel, struct mt_gdma_conf *config, DMA_CONF_FLAG flag)
{
	unsigned int dma_con = 0x0, limiter = 0;

	if ((channel < GDMA_START) || (channel >= (GDMA_START + env_info.nr_channel))) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	if (!config) {
		return -DMA_ERR_INV_CONFIG;
	}

	if (config->sfix) {
		pr_notice("GMDA fixed address mode doesn't support\n");
		return -DMA_ERR_INV_CONFIG;
	}

	if (config->dfix) {
		pr_notice("GMDA fixed address mode doesn't support\n");
		return -DMA_ERR_INV_CONFIG;
	}

	if (config->count > MAX_TRANSFER_LEN1) {
		pr_notice("GDMA transfer length cannot exceeed 0x%x.\n", MAX_TRANSFER_LEN1);
		return -DMA_ERR_INV_CONFIG;
	}

	if (config->limiter > MAX_SLOW_DOWN_CNTER) {
		pr_notice("GDMA slow down counter cannot exceeed 0x%x.\n", MAX_SLOW_DOWN_CNTER);
		return -DMA_ERR_INV_CONFIG;
	}

	switch (flag) {
		case ALL:
			/* Control Register */
			mt_reg_sync_writel((u32)config->src, DMA_SRC);
			mt_reg_sync_writel((u32)config->dst, DMA_DST);
			mt_reg_sync_writel((config->wplen) & DMA_GDMA_LEN_MAX_MASK, DMA_LEN2);
			mt_reg_sync_writel(config->wpto, DMA_JUMP_ADDR);
			mt_reg_sync_writel((config->count) & DMA_GDMA_LEN_MAX_MASK, DMA_LEN1);

			/*setup security channel */
			if (config->sec){
				pr_notice("1:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
				mt_reg_sync_writel((DMA_SEC_EN_BIT|readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
				pr_notice("2:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
			} else {
				pr_notice("1:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
				mt_reg_sync_writel(((~DMA_SEC_EN_BIT)&readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
				pr_notice("2:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
			}

			/*setup domain_cfg */
			if (config->domain){
				pr_notice("1:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
				mt_reg_sync_writel(((config->domain << 1) | readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
				pr_notice("2:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
			} else {
				pr_notice("1:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
				mt_reg_sync_writel((0x1 & readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
				pr_notice("2:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
			}

			if (config->wpen) {
				dma_con |= DMA_CON_WPEN;
			}

			if (config->wpsd) {
				dma_con |= DMA_CON_WPSD;
			}

			if (config->iten) {
				dma_ctrl[channel].isr_cb = config->isr_cb;
				dma_ctrl[channel].data = config->data;
				mt_reg_sync_writel(DMA_INT_EN_BIT, DMA_INT_EN);
			} else {
				dma_ctrl[channel].isr_cb = NULL;
				dma_ctrl[channel].data = NULL;
				mt_reg_sync_writel(DMA_INT_EN_CLR_BIT, DMA_INT_EN);
			}

			if (!(config->dfix) && !(config->sfix)) {
				dma_con |= (config->burst & DMA_CON_BURST_MASK);
			} else {
				if (config->dfix) {
					dma_con |= DMA_CON_DFIX;
					dma_con |= DMA_CON_WSIZE_1BYTE;
				}

				if (config->sfix) {
					dma_con |= DMA_CON_SFIX;
					dma_con |= DMA_CON_RSIZE_1BYTE;
				}

				// fixed src/dst mode only supports burst type SINGLE
				dma_con |= DMA_CON_BURST_SINGLE;
			}

			if (config->limiter) {
				limiter = (config->limiter) & DMA_CON_SLOW_MAX_MASK;
				dma_con |= limiter << DMA_CON_SLOW_OFFSET;
				dma_con |= DMA_CON_SLOW_EN;
			}

			mt_reg_sync_writel(dma_con, DMA_CON);
			break;

		case SRC:
			mt_reg_sync_writel((u32)config->src, DMA_SRC);
			break;

		case DST:
			mt_reg_sync_writel((u32)config->dst, DMA_DST);
			break;

		case SRC_AND_DST:
			mt_reg_sync_writel((u32)config->src, DMA_SRC);
			mt_reg_sync_writel((u32)config->dst, DMA_DST);
			break;

		default:
			break;
	}

	/* use the data synchronization barrier to ensure that all writes are completed */
	dsb();

	return 0;
}
EXPORT_SYMBOL(mt_config_gdma);

/*
 * mt_free_gdma: free a general DMA.
 * @channel: channel to free
 * Return 0 for success; return negative errot code for failure.
 */
int mt_free_gdma(int channel)
{
	if (channel < GDMA_START) {
		return -DMA_ERR_INVALID_CH;
	}

	if (channel >= (GDMA_START + env_info.nr_channel)) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	mt_stop_gdma(channel);

	dma_ctrl[channel].isr_cb = NULL;
	dma_ctrl[channel].data = NULL;
	dma_ctrl[channel].in_use = 0;

	return 0;
}
EXPORT_SYMBOL(mt_free_gdma);

/*
 * mt_dump_gdma: dump registers for the specified GDMA channel
 * @channel: GDMA channel to dump registers
 * Return 0 for success; return negative errot code for failure.
 */
int mt_dump_gdma(int channel)
{
	unsigned int i;
	pr_notice("Channel 0x%x\n",channel);
	for (i = 0; i < 96; i++) {
		pr_notice("addr:%p, value:%x\n", env_info.base + i * 4, readl(env_info.base + i * 4));
	}

	return 0;
}
EXPORT_SYMBOL(mt_dump_gdma);

/*
 * mt_warm_reset_gdma: warm reset the specified GDMA channel
 * @channel: GDMA channel to warm reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_warm_reset_gdma(int channel)
{
	if (channel < GDMA_START) {
		return -DMA_ERR_INVALID_CH;
	}

	if (channel >= (GDMA_START + env_info.nr_channel)) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	mt_reg_sync_writel(DMA_WARM_RST_BIT, DMA_RESET);

	if (mt_polling_gdma(channel, GDMA_WARM_RST_TIMEOUT) != 0) {
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(mt_warm_reset_gdma);

/*
 * mt_hard_reset_gdma: hard reset the specified GDMA channel
 * @channel: GDMA channel to hard reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_hard_reset_gdma(int channel)
{
	if (channel < GDMA_START) {
		return -DMA_ERR_INVALID_CH;
	}

	if (channel >= (GDMA_START + env_info.nr_channel)) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	pr_notice("GDMA_%d Hard Reset !!\n", channel);

	mt_reg_sync_writel(DMA_HARD_RST_BIT, DMA_RESET);
	mt_reg_sync_writel(DMA_HARD_RST_CLR_BIT, DMA_RESET);

	return 0;
}
EXPORT_SYMBOL(mt_hard_reset_gdma);

/*
 * mt_reset_gdma: reset the specified GDMA channel
 * @channel: GDMA channel to reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_reset_gdma(int channel)
{
	if (channel < GDMA_START) {
		return -DMA_ERR_INVALID_CH;
	}

	if (channel >= (GDMA_START + env_info.nr_channel)) {
		return -DMA_ERR_INVALID_CH;
	}

	if (dma_ctrl[channel].in_use == 0) {
		return -DMA_ERR_CH_FREE;
	}

	if (mt_warm_reset_gdma(channel) != 0) {
		mt_hard_reset_gdma(channel);
	}

	return 0;
}
EXPORT_SYMBOL(mt_reset_gdma);

/*
 * gdma1_irq_handler: general DMA channel 1 interrupt service routine.
 * @irq: DMA IRQ number
 * @dev_id:
 * Return IRQ returned code.
 */
static irqreturn_t gdma1_irq_handler(int irq, void *dev_id)
{
	volatile unsigned glbsta = readl(DMA_INT_FLAG);

	if (glbsta & 0x1){
		if (dma_ctrl[G_DMA_1].isr_cb) {
			dma_ctrl[G_DMA_1].isr_cb(dma_ctrl[G_DMA_1].data);
		}

		mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG);
	} else {
		pr_debug("[CQDMA] discard interrupt\n");
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

/*
 * mt_reset_gdma_conf: reset the config of the specified DMA channel
 * @iChannel: channel number of the DMA channel to reset
 */
void mt_reset_gdma_conf(const unsigned int channel)
{
	struct mt_gdma_conf conf;

	memset(&conf, 0, sizeof(struct mt_gdma_conf));

	if (mt_config_gdma(channel, &conf, ALL) != 0){
		return;
	}

	return;
}

#if defined(LDVT)

unsigned int *dma_dst_array_v;
unsigned int *dma_src_array_v;
dma_addr_t dma_dst_array_p;
dma_addr_t dma_src_array_p;

#define TEST_LEN 4000
#define LEN (TEST_LEN / sizeof(int))

static void irq_dma_handler(void * data)
{
	long channel = (long)data;
	int i = 0;

	for(i = 0; i < LEN; i++) {
		if(dma_dst_array_v[i] != dma_src_array_v[i]) {
			pr_err("DMA failed, src = %d, dst = %d, i = %d\n", dma_src_array_v[i], dma_dst_array_v[i], i);
			break;
		}
	}
	DMA_INT_DONE=1;
	if (i == LEN) {
		pr_notice("DMA verified ok\n");
	}
	mt_free_gdma(channel);
}

static u64 apdma_test_dev_dma_mask = 0xffffffff;
static struct device apdma_test_dev = {
	.dma_mask = &apdma_test_dev_dma_mask,
	.coherent_dma_mask = 0xffffffff
};

static void APDMA_test_transfer(int testcase)
{
	int i = 0;
	long channel = 0;
	int start_dma = 0;
	struct mt_gdma_conf dma_conf;

	channel = mt_req_gdma(GDMA_ANY);

	pr_notice("GDMA channel:%ld\n",channel);
	if(channel < 0 ){
		pr_err("[CQDMA] ERROR Register DMA\n");
		return;
	}

	mt_reset_gdma_conf(channel);
  
	dma_dst_array_v = dma_alloc_coherent(&apdma_test_dev, TEST_LEN, &dma_dst_array_p, GFP_KERNEL ); // 25 unsinged int
	if (!dma_dst_array_v) {
		pr_err("allooc dst memory failed\n");
		return;
	}

	dma_src_array_v = dma_alloc_coherent(&apdma_test_dev, TEST_LEN, &dma_src_array_p, GFP_KERNEL );
	if (!dma_src_array_v) {
		pr_err("alloc src memory failed\n");
		return;
	}

	dma_conf.count = TEST_LEN;
	dma_conf.src = dma_src_array_p;
        dma_conf.dst = dma_dst_array_p;
        dma_conf.iten = (testcase == 2) ? DMA_FALSE : DMA_TRUE;
        dma_conf.isr_cb = (testcase == 2) ? NULL : irq_dma_handler;
        dma_conf.data = (void *)channel;
        dma_conf.burst = DMA_CON_BURST_SINGLE;
        dma_conf.dfix = DMA_FALSE;
        dma_conf.sfix = DMA_FALSE;
        //.cohen = DMA_TRUE, //enable coherence bus
        dma_conf.sec = DMA_FALSE;// non-security channel
        dma_conf.domain = 0;
        dma_conf.limiter = (testcase == 3 || testcase == 4) ? 0x3FF : 0;
 
	/* init src & dest buffer */
	for(i = 0; i < LEN; i++) {
		dma_dst_array_v[i] = 0;
		dma_src_array_v[i] = i;
	}
    
	if (mt_config_gdma(channel, &dma_conf, ALL) != 0) {
		pr_err("ERROR set DMA\n");
		goto _exit;
	}
    
	start_dma = mt_start_gdma(channel);

	switch(testcase) {
	case 1:
		while(!DMA_INT_DONE);
		DMA_INT_DONE=0;
		pr_notice("CQDMA INT mode PASS!!\n");
		break;
	case 2:
		if (mt_polling_gdma(channel, GDMA_WARM_RST_TIMEOUT) != 0) {
                	pr_err("Polling transfer failed\n");
			break;
		}
            
		for(i = 0; i < LEN; i++) {
                	if(dma_dst_array_v[i] != dma_src_array_v[i]) {
				pr_err("fails at %d\n", i);
				goto _exit;
                	}
		}
		pr_notice("Polling succeeded\n");
		break;
	case 3:
		mt_warm_reset_gdma(channel);
		for(i = 0; i < LEN; i++) {
			if(dma_dst_array_v[i] != dma_src_array_v[i]) {
				pr_notice("Warm reset succeeded\n");
				break;
			}
		}

		if (i == LEN) {
			pr_err("Warm reset failed\n");
		}
		break;
            
	case 4:
		mt_hard_reset_gdma(channel);
		for(i = 0; i < LEN; i++) {
			if(dma_dst_array_v[i] != dma_src_array_v[i]) {
				pr_notice("Hard reset succeeded\n");
				break;
			}
		}

		if (i == LEN) {
			pr_err("Hard reset failed\n");
		}
		break;
            
	default:
		break;
	}
	mt_free_gdma(channel);

_exit:
	if(dma_dst_array_v){
		dma_free_coherent(&apdma_test_dev, TEST_LEN, dma_dst_array_v, dma_dst_array_p);
		dma_dst_array_v = NULL;
		dma_dst_array_p = 0;
	}

	if(dma_src_array_v){
		dma_free_coherent(&apdma_test_dev, TEST_LEN, dma_src_array_v, dma_src_array_p);
		dma_src_array_v = NULL;
		dma_src_array_p = 0;
	}

	return;
}

static ssize_t cqdma_dvt_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "==CQDMA test==\n"
                                   "1.CQDMA transfer (interrupt mode)\n"
                                   "2.CQDMA transfer (polling mode)\n"
                                   "3.CQDMA warm reset\n"
                                   "4.CQDMA hard reset\n"); 
}

static ssize_t cqdma_dvt_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int num;

	num = simple_strtoul(p, &p, 10);
        switch(num){
            /* Test APDMA Normal Function */
            case 1:
                APDMA_test_transfer(1);
                break;
            case 2:
                APDMA_test_transfer(2);
                break;
            case 3:
                APDMA_test_transfer(3);
                break;
            case 4:
                APDMA_test_transfer(4);
                break;
            default:
                break;
        }

	return count;
}

DRIVER_ATTR(cqdma_dvt, 0664, cqdma_dvt_show, cqdma_dvt_store);

#endif	//!LDVT

struct mt_cqdma_driver{
    struct device_driver driver;
    const struct platform_device_id *id_table;
};

static struct mt_cqdma_driver mt_cqdma_drv = {
	.driver = {
		.name = "cqdma",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
};

static void cqdma_reset(int nr_channel)
{
	int i = 0;

	for (i = 0; i < nr_channel; i++) {
		mt_reset_gdma_conf(i);
    	}
}

static int __init init_cqdma(void)
{
	int ret = 0;
	int irq = 0;
	unsigned int dma_info[3] = {0, 0, 0};
	struct device_node *node = NULL;
	u32 nr_channel = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,CQDMA");
	if (!node) {
		pr_err("find CQDMA node failed!!!\n");
		return -ENODEV;
	}
    
	env_info.base = of_iomap(node, 0);
	if (!env_info.base) {
		pr_warn("unable to map CQDMA base registers!!!\n");
		return -ENODEV;
	}
	pr_notice("[CQDMA] vbase = 0x%p\n", env_info.base );

	irq = irq_of_parse_and_map(node, 0);
	pr_notice("[CQDMA] irq = %d\n", irq);

	/* get the interrupt line behaviour */
	if (of_property_read_u32_array(node, "interrupts", dma_info, ARRAY_SIZE(dma_info))){
		pr_err("[CQDMA] get irq flags from DTS fail!!\n");
		return -ENODEV;
	}
	pr_notice("[CQDMA] int attr = %x\n", dma_info[2]);

	
	of_property_read_u32(node, "nr_channel", &nr_channel);
	if (!nr_channel) {
		pr_err("[CQDMA] no channel found\n");
		return -ENODEV;
	}
	pr_notice("[CQDMA] DMA channel = %d\n", nr_channel);
	cqdma_reset(nr_channel);

	ret = request_irq(irq, gdma1_irq_handler, dma_info[2] | IRQF_SHARED, "CQDMA", &dma_ctrl);
	if (ret > 0) {
		pr_err("GDMA1 IRQ LINE NOT AVAILABLE,ret 0x%x!!\n",ret);
	}

	ret = driver_register(&mt_cqdma_drv.driver);
	if (ret) {
		pr_err("CQDMA init FAIL, ret 0x%x!!!\n", ret);
	}
#ifdef LDVT
	ret = driver_create_file(&mt_cqdma_drv.driver, &driver_attr_cqdma_dvt);
    	if (ret) {
		pr_err("CQDMA create sysfs file init FAIL, ret 0x%x!!!\n", ret);
		return -ENODEV;
	}
#endif
  
#ifdef CONFIG_ARM_LPAE
	mt_reg_sync_writel(0x1, DMA_SRC_4G_SUPPORT);
	mt_reg_sync_writel(0x1, DMA_DST_4G_SUPPORT);
	mt_reg_sync_writel(0x1, DMA_JUMP_4G_SUPPORT);
#endif
	env_info.irq = irq;
	env_info.nr_channel = nr_channel;

	return 0;
}

late_initcall(init_cqdma);
