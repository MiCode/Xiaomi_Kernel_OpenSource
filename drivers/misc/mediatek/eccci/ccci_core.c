/*
 * CCCI common service and routine. Consider it as a "logical" layer.
 *
 * V0.1: Xiao Wang <xiao.wang@mediatek.com>
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kobject.h>

#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>
#include "ccci_platform.h"

#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_support.h"
#include "port_cfg.h"
static LIST_HEAD(modem_list); // don't use array, due to MD index may not be continuous
static void *dev_class = NULL;

// common sub-system
extern int ccci_subsys_bm_init(void);
extern int ccci_subsys_sysfs_init(void);
extern int ccci_subsys_dfo_init(void);
// per-modem sub-system
extern int ccci_subsys_char_init(struct ccci_modem *md);
extern void md_ex_monitor_func(unsigned long data);
extern void md_ex_monitor2_func(unsigned long data);
extern void md_bootup_timeout_func(unsigned long data);
extern void md_status_poller_func(unsigned long data);
extern void md_status_timeout_func(unsigned long data);

//used for throttling feature - start
unsigned long ccci_modem_boot_count[5];
unsigned long ccci_get_md_boot_count(int md_id)
{
	return ccci_modem_boot_count[md_id];
}
//used for throttling feature - end

int boot_md_show(int md_id, char *buf, int size)
{
	int curr = 0;
	struct ccci_modem *md;

	list_for_each_entry(md, &modem_list, entry) {
		if(md->index==md_id)
			curr += snprintf(&buf[curr], size, "md%d:%d/%d/%d", md->index+1, 
						md->md_state, md->boot_stage, md->ex_stage);
	}
    return curr;
}

int boot_md_store(int md_id)
{
	struct ccci_modem *md;

	CCCI_INF_MSG(-1, CORE, "ccci core boot md%d\n", md_id+1);
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id && md->md_state == GATED) {
			md->ops->start(md);
			return 0;
		}
	}
	return -1;
}

/*
 * most of this file is copied from mtk_ccci_helper.c, we use this function to
 * translate legacy data structure into current CCCI core.
 */
void ccci_config_modem(struct ccci_modem *md)
{
	phys_addr_t md_resv_mem_addr=0, md_resv_smem_addr=0;
	//void __iomem *smem_base_vir;
	unsigned int md_resv_mem_size=0, md_resv_smem_size=0;
	
	// setup config
	md->config.load_type = get_modem_support_cap(md->index);
	if(get_modem_is_enabled(md->index))
		md->config.setting |= MD_SETTING_ENABLE;
	else
		md->config.setting &= ~MD_SETTING_ENABLE;

	// Get memory info
	get_md_resv_mem_info(md->index, &md_resv_mem_addr, &md_resv_mem_size, &md_resv_smem_addr, &md_resv_smem_size);
	// setup memory layout
	// MD image
	md->mem_layout.md_region_phy = md_resv_mem_addr;
	md->mem_layout.md_region_size = md_resv_mem_size;
	md->mem_layout.md_region_vir = ioremap_nocache(md->mem_layout.md_region_phy, MD_IMG_DUMP_SIZE); // do not remap whole region, consume too much vmalloc space 
	// DSP image
	md->mem_layout.dsp_region_phy = 0;
	md->mem_layout.dsp_region_size = 0;
	md->mem_layout.dsp_region_vir = 0;
	// Share memory
	md->mem_layout.smem_region_phy = md_resv_smem_addr;
	md->mem_layout.smem_region_size = md_resv_smem_size;
	md->mem_layout.smem_region_vir = ioremap_nocache(md->mem_layout.smem_region_phy, md->mem_layout.smem_region_size);
	memset(md->mem_layout.smem_region_vir, 0, md->mem_layout.smem_region_size);

	// exception region
	md->smem_layout.ccci_exp_smem_base_phy = md->mem_layout.smem_region_phy;
	md->smem_layout.ccci_exp_smem_base_vir = md->mem_layout.smem_region_vir;
	md->smem_layout.ccci_exp_smem_size = CCCI_SMEM_SIZE_EXCEPTION;
	md->smem_layout.ccci_exp_dump_size = CCCI_SMEM_DUMP_SIZE;
	// dump region
	md->smem_layout.ccci_exp_smem_ccci_debug_vir = md->smem_layout.ccci_exp_smem_base_vir+CCCI_SMEM_OFFSET_CCCI_DEBUG;
	md->smem_layout.ccci_exp_smem_ccci_debug_size = CCCI_SMEM_CCCI_DEBUG_SIZE;
	md->smem_layout.ccci_exp_smem_mdss_debug_vir = md->smem_layout.ccci_exp_smem_base_vir+CCCI_SMEM_OFFSET_MDSS_DEBUG;
	md->smem_layout.ccci_exp_smem_mdss_debug_size = CCCI_SMEM_MDSS_DEBUG_SIZE;
	// exception record start address
	md->smem_layout.ccci_exp_rec_base_vir = md->smem_layout.ccci_exp_smem_base_vir+CCCI_SMEM_OFFSET_EXREC;
	
	// updae image info
	md->img_info[IMG_MD].type = IMG_MD;
	md->img_info[IMG_MD].address = md->mem_layout.md_region_phy;
	md->img_info[IMG_DSP].type = IMG_DSP;
	md->img_info[IMG_DSP].address = md->mem_layout.dsp_region_phy;

	if(md->config.setting&MD_SETTING_ENABLE)
		ccci_set_mem_remap(md, md_resv_smem_addr-md_resv_mem_addr, 
			ALIGN(md_resv_mem_addr+md_resv_mem_size+md_resv_smem_size, 0x2000000));
}

//==================================================================
// MD relate sys
//==================================================================
unsigned int ccci_debug_enable = 0; // 0 to disable; 1 for print to ram; 2 for print to uart
static void ccci_md_obj_release(struct kobject *kobj)
{
	struct ccci_modem *md = container_of(kobj, struct ccci_modem, kobj);
	CCCI_DBG_MSG(md->index, SYSFS, "md kobject release\n");
}

static ssize_t ccci_md_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr, struct ccci_md_attribute, attr);

	if (a->show)
		len = a->show(a->modem, buf);

	return len;
}

static ssize_t ccci_md_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr, struct ccci_md_attribute, attr);

	if (a->store)
		len = a->store(a->modem, buf, count);

	return len;
}

static struct sysfs_ops ccci_md_sysfs_ops = {
	.show  = ccci_md_attr_show,
	.store = ccci_md_attr_store
};

static struct attribute *ccci_md_default_attrs[] = {
	NULL
};

static struct kobj_type ccci_md_ktype = {
	.release		= ccci_md_obj_release,
	.sysfs_ops 		= &ccci_md_sysfs_ops,
	.default_attrs 	= ccci_md_default_attrs
};
//-------------------------------------------------------------------------
static int __init ccci_init(void)
{
	CCCI_INF_MSG(-1, CORE, "ccci core init\n");
	dev_class = class_create(THIS_MODULE, "ccci_node");
	// init common sub-system
	//ccci_subsys_sysfs_init();
	ccci_subsys_bm_init();
	ccci_plat_common_init();
	return 0;
}

// setup function is only for data structure initialization
struct ccci_modem *ccci_allocate_modem(int private_size)
{
	struct ccci_modem* md = kzalloc(sizeof(struct ccci_modem), GFP_KERNEL);
	int i;
	if(!md) {
		CCCI_ERR_MSG(-1, CORE, "fail to allocate memory for modem structure\n");
		goto out;
	}
	
	md->private_data = kzalloc(private_size, GFP_KERNEL);
	md->sim_type = 0xEEEEEEEE; //sim_type(MCC/MNC) sent by MD wouldn't be 0xEEEEEEEE
	md->config.setting |= MD_SETTING_FIRST_BOOT;
	md->md_state = INVALID;
	md->boot_stage = MD_BOOT_STAGE_0;
	md->ex_stage = EX_NONE;
	atomic_set(&md->wakeup_src, 0);
	INIT_LIST_HEAD(&md->entry);
	ccci_reset_seq_num(md);
	
	init_timer(&md->bootup_timer);	
	md->bootup_timer.function = md_bootup_timeout_func;
	md->bootup_timer.data = (unsigned long)md;
	init_timer(&md->ex_monitor);
	md->ex_monitor.function = md_ex_monitor_func;
	md->ex_monitor.data = (unsigned long)md;
	init_timer(&md->ex_monitor2);
	md->ex_monitor2.function = md_ex_monitor2_func;
	md->ex_monitor2.data = (unsigned long)md;
	init_timer(&md->md_status_poller);
	md->md_status_poller.function = md_status_poller_func;
	md->md_status_poller.data = (unsigned long)md;
	init_timer(&md->md_status_timeout);
	md->md_status_timeout.function = md_status_timeout_func;
	md->md_status_timeout.data = (unsigned long)md;
	
	spin_lock_init(&md->ctrl_lock);
	for(i=0; i<ARRAY_SIZE(md->rx_ch_ports); i++) {
		INIT_LIST_HEAD(&md->rx_ch_ports[i]);
	}
out:
	return md;
}
EXPORT_SYMBOL(ccci_allocate_modem);

int ccci_register_modem(struct ccci_modem *modem)
{
    int ret;
	
    CCCI_INF_MSG(modem->index, CORE, "register modem %d\n", modem->major);
    md_port_cfg(modem);
    // init modem
    // TODO: check modem->ops for all must-have functions
    ret = modem->ops->init(modem);
    if(ret<0)
        return ret;
    ccci_config_modem(modem);
    list_add_tail(&modem->entry, &modem_list);
    // init per-modem sub-system
    ccci_subsys_char_init(modem);
    ccci_sysfs_add_modem(modem->index, (void*)&modem->kobj, (void*)&ccci_md_ktype, boot_md_show, boot_md_store);
    ccci_platform_init(modem);
    return 0;
}
EXPORT_SYMBOL(ccci_register_modem);

struct ccci_modem *ccci_get_modem_by_id(int md_id)
{
	struct ccci_modem *md = NULL;
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id)
			return md;
	}
	return NULL;
}

int ccci_get_modem_state(int md_id)
{
	struct ccci_modem *md = NULL;
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id)
			return md->md_state;
	}
	return -CCCI_ERR_MD_INDEX_NOT_FOUND;
}

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len)
{
	struct ccci_modem *md = NULL;
	int ret = 0;
	
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id) {
			ret = 1;
			break;
		}
	}
	if(!ret)
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;

	CCCI_DBG_MSG(md->index, CORE, "%ps execuste function %d\n", __builtin_return_address(0), id);
	switch(id) {
	case ID_GET_MD_WAKEUP_SRC:		
		atomic_set(&md->wakeup_src, 1);
		break;
	case ID_GET_TXPOWER:
		if(buf[0] == 0) {
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_TX_POWER, 0, 0);
		} else if(buf[0] == 1) {
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_RF_TEMPERATURE, 0, 0);
		} else if(buf[0] == 2) {
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_RF_TEMPERATURE_3G, 0, 0);
		}
		break;
	case ID_PAUSE_LTE:
		/*
		 * MD booting/flight mode/exception mode: return >0 to DVFS.
		 * MD ready: return 0 if message delivered, return <0 if get error.
		 * DVFS will call this API with IRQ disabled.
		 */
		if(md->md_state != READY)
			ret = 1;
		else
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_PAUSE_LTE, *((int *)buf), 1);
		break;
   	case ID_STORE_SIM_SWITCH_MODE:
        {
            int simmode = *((int*)buf);
            ccci_store_sim_switch_mode(md, simmode);
        }
        break;
    case ID_GET_SIM_SWITCH_MODE:
        {
            int simmode = ccci_get_sim_switch_mode();
            memcpy(buf, &simmode, sizeof(int));
        }
        break;
	case ID_GET_MD_STATE:
		ret = md->boot_stage;
		break;
	//used for throttling feature - start
	case ID_THROTTLING_CFG:
		ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_THROTTLING, *((int *)buf), 1);
		break;
	//used for throttling feature - end
#if defined(CONFIG_MTK_SWITCH_TX_POWER)
	case ID_UPDATE_TX_POWER:
	{
		unsigned int msg_id = (md_id==0)?MD_SW_MD1_TX_POWER:MD_SW_MD2_TX_POWER;
		unsigned int mode = *((unsigned int*)buf);

		ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, msg_id, mode, 0);            
	}
	break;  
#endif                
	default:
		ret = -CCCI_ERR_FUNC_ID_ERROR;
		break;
	};
	return ret;
}

int aee_dump_ccci_debug_info(int md_id, void **addr, int *size)
{
	struct ccci_modem *md = NULL;
	int ret = 0;
	md_id--; // EE string use 1 and 2, not 0 and 1
	
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id) {
			ret = 1;
			break;
		}
	}
	if(!ret)
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	
	*addr = md->smem_layout.ccci_exp_smem_ccci_debug_vir;
	*size = md->smem_layout.ccci_exp_smem_ccci_debug_size;
	return 0;
}

struct ccci_port *ccci_get_port_for_node(int major, int minor)
{
	struct ccci_modem *md = NULL;
	struct ccci_port *port = NULL;
	
	list_for_each_entry(md, &modem_list, entry) {
		if(md->major == major) {			
			port = md->ops->get_port_by_minor(md, minor);
			break;
		}
	}
	return port;
}

int ccci_register_dev_node(const char *name, int major_id, int minor)
{
	int ret = 0;
	dev_t dev_n;
	struct device *dev;

	dev_n = MKDEV(major_id, minor);
	dev = device_create(dev_class, NULL, dev_n, NULL, "%s", name);

	if(IS_ERR(dev)) {
		ret = PTR_ERR(dev);
	}
	
	return ret;
}

/*
 * kernel inject CCCI message to modem.
 */
int ccci_send_msg_to_md(struct ccci_modem *md, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv, int blocking)
{
	struct ccci_port *port = NULL;
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;
	int ret;

	if(md->md_state!=BOOTING && md->md_state!=READY && md->md_state!=EXCEPTION)
		return -CCCI_ERR_MD_NOT_READY;
	if(ch==CCCI_SYSTEM_TX && md->md_state!=READY)
		return -CCCI_ERR_MD_NOT_READY;
	
	port = md->ops->get_port_by_channel(md, ch);
	if(port) {
		if(!blocking)
			req = ccci_alloc_req(OUT, sizeof(struct ccci_header), 0, 0);
		else
			req = ccci_alloc_req(OUT, sizeof(struct ccci_header), 1, 1);
		if(req) {
			req->policy = RECYCLE;
			ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
			ccci_h->data[0] = CCCI_MAGIC_NUM;
			ccci_h->data[1] = msg;
			ccci_h->channel = ch;
			ccci_h->reserved = resv;
			ret = ccci_port_send_request(port, req);
			if(ret)
				ccci_free_req(req);
			return ret;
		} else {
			return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
		}
	}
	return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
}

/*
 * kernel inject message to user space daemon, this function may sleep
 */
int ccci_send_virtual_md_msg(struct ccci_modem *md, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv)
{
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;
	int ret=0, count=0;

	if(unlikely(ch != CCCI_MONITOR_CH)) {
		CCCI_ERR_MSG(md->index, CORE, "invalid channel %x for sending virtual msg\n", ch);
		return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
	}
	if(unlikely(in_interrupt() || in_atomic())) {
		CCCI_ERR_MSG(md->index, CORE, "sending virtual msg from IRQ context %ps\n", __builtin_return_address(0));
		return -CCCI_ERR_ASSERT_ERR;
	}

	req = ccci_alloc_req(IN, sizeof(struct ccci_header), 1, 0);
	// request will be recycled in char device's read function
	ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
	ccci_h->data[0] = CCCI_MAGIC_NUM;
	ccci_h->data[1] = msg;
	ccci_h->channel = ch;
#ifdef FEATURE_SEQ_CHECK_EN
	ccci_h->assert_bit = 0;
#endif
	ccci_h->reserved = resv;
	INIT_LIST_HEAD(&req->entry);  // as port will run list_del
retry:
	ret = ccci_port_recv_request(md, req);
	if(ret>=0 || ret==-CCCI_ERR_DROP_PACKET) {
		return ret;
	} else {
		msleep(100);
		if(count++<20) {
			goto retry;
		} else {
			CCCI_ERR_MSG(md->index, CORE, "fail to send virtual msg %x for %ps\n", msg, __builtin_return_address(0));
			list_del(&req->entry);
			req->policy = RECYCLE;
			ccci_free_req(req);
		}
	}
	return ret;
}

#if defined(CONFIG_MTK_SWITCH_TX_POWER)
static int switch_Tx_Power(int md_id, unsigned int mode)
{
	int ret = 0;
	unsigned int   resv = mode;
	unsigned int msg_id = (md_id==0)?MD_SW_MD1_TX_POWER:MD_SW_MD2_TX_POWER;

#if 1
    ret = exec_ccci_kern_func_by_md_id(md_id, ID_UPDATE_TX_POWER, (char *)&resv, sizeof(resv));
#else
	ret = ccci_send_msg_to_md(md_id, CCCI_SYSTEM_TX, msg_id, resv, 0);
#endif	
	printk("[swtp] switch_MD%d_Tx_Power(%d): ret[%d]\n", md_id+1, resv, ret);
		
	CCCI_DBG_MSG(md_id,  "ctl", "switch_MD%d_Tx_Power(%d): %d\n", md_id+1, resv, ret);

	return ret;
}

int switch_MD1_Tx_Power(unsigned int mode)
{
	return switch_Tx_Power(0, mode);
}
EXPORT_SYMBOL(switch_MD1_Tx_Power);

int switch_MD2_Tx_Power(unsigned int mode)
{
	return switch_Tx_Power(1, mode);
}
EXPORT_SYMBOL(switch_MD2_Tx_Power);
#endif

subsys_initcall(ccci_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Unified CCCI driver v0.1");
MODULE_LICENSE("GPL");
