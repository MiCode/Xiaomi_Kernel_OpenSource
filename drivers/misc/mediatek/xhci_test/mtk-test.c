#include <linux/init.h>
#include <linux/irq.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <linux/kernel.h>       /* printk() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
//#include <linux/pci.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <asm/unaligned.h>
#include <linux/usb/ch9.h>
#include <asm/uaccess.h>
#include "mtk-test.h"
#include "xhci.h"
#include "mtk-usb-hcd.h"
//#include <linux/usb/hcd.h>
#include "mtk-test-lib.h"
#include "mtk-protocol.h"
//#include "xhci-hub.c"
#include <linux/dma-mapping.h>
#include "xhci-mtk-scheduler.h"
#include <linux/mu3phy/mtk-phy.h>
#include "xhci-mtk-power.h"
#include <linux/dmapool.h>

#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>


#include "xhci.h"
//extern u32 mtktest_xhci_port_state_to_neutral(u32 state);

struct file_operations xhci_mtk_test_fops_host;

static struct miscdevice mu3h_test_uevent_device = {
         .minor = MISC_DYNAMIC_MINOR,
         .name = "usbif_u3h_test_uevent",
         .fops = NULL,
};

/********************************************************
 *	slt.disable
 *		Send disable slot command, release current slot id
 *	args:
*********************************************************/
static int t_slot_disable_slot(int argc, char** argv);
/********************************************************
 *	slt.configep
 *		Config an bulk EP to let slot go to configured state
 *	args:
*********************************************************/
//static int t_slot_config_ep(int argc, char** argv);
static int t_slot_reset_device(int argc, char** argv);
/********************************************************
 *	slt.reconfig
 *		After reset device, set address command and getConfigure, setConfigure
 *	args:
*********************************************************/
/********************************************************
 *	slt.enable
 *		Attach device, send enable slot command
 *	args:
*********************************************************/
static int t_slot_enable_slot(int argc, char** argv);
/********************************************************
 *	slt.addslt
 *		Send address command to current slot
 *	args:
*********************************************************/
static int t_slot_address(int argc, char** argv);
/********************************************************
 *	slt.getdesc
 *		Do GetDescriptor control transfer
 *	args:
*********************************************************/
static int t_slot_getdescriptor(int argc, char** argv);
static int t_slot_forcegetdescriptor(int argc, char** argv);
static int t_slot_getbos(int argc, char** argv);
static int t_slot_setconf(int argc, char** argv);
static int t_slot_setu1u2(int argc, char** argv);
//static int t_slot_setsel(int argc, char** argv);
static int t_slot_getdevstatus(int argc, char** argv);
static int t_slt_resetport(int argc, char** argv);
//static int t_slot_stress_ctrl(int argc, char** argv);
/********************************************************
 *	slt.getdesc
 *		Wait device to desicon, disable slot
 *	args:
*********************************************************/
static int t_slot_discon(int argc, char** argv);
static int t_slot_evaluate_context(int argc, char** argv);
static int t_slt_ped(int argc, char** argv);
//loopback
static int t_loopback_loop(int argc, char** argv);
static int t_loopback_configep(int argc, char** argv);
static int t_loopback_deconfigep(int argc, char** argv);

/********************************************************
 *	pw.suspend
 *		suspend U2 device
 *	args:
*********************************************************/
static int t_power_suspend(int argc, char** argv);
/********************************************************
 *	pw.resume
 *		resume U2 device
 *	args:
*********************************************************/
static int t_power_resume(int argc, char** argv);
/********************************************************
 *	pw.wakeup
 *		When in suspend(U3) state, wait for device to wakeup
 *		After got remote wakeup signal, back to U0 state
 *	args:
*********************************************************/
static int t_power_remotewakeup(int argc, char** argv);
/********************************************************
 *	pw.u1u2
 *		Adjust U1/U2 timeout regs value
 *	args:
 *		1. u?(1/2): 1=u1, 2=u2
 *		2. value: u1(0~127, 255), u2(0~254, 255)
*********************************************************/
static int t_power_u1u2(int argc, char** argv);
static int t_power_suspendport(int argc, char** argv);
static int t_power_resumeport(int argc, char** argv);
static int t_power_fla(int argc, char** argv);
static int t_power_occ(int argc, char** argv);
static int t_power_u2_lpm(int argc, char** argv);
static int t_power_u2_swlpm(int argc, char** argv);
static int t_power_random_access_regs(int argc, char** argv);
/********************************************************
 *	ring.erfull
 *		Test event ring full error event
 *	args:
*********************************************************/
static int t_ring_er_full(int argc, char** argv);
/********************************************************
 *	ring.stopcmd
 *		Stop command ring
 *	args:
*********************************************************/
static int t_ring_stop_cmd(int argc, char** argv);
/********************************************************
 *	ring.abortcmd
 *		Abort command when executing address device command
 *	args:
*********************************************************/
static int t_ring_abort_cmd_add(int argc, char** argv);
static int t_ring_stop_ep(int argc, char** argv);
static int t_ring_random_ring_doorbell(int argc, char** argv);
static int t_ring_random_stop_ep(int argc, char** argv);
static int t_ring_enlarge(int argc, char** argv);
static int t_ring_shrink(int argc, char**argv);
static int t_ring_intr_moderation(int argc, char** argv);
static int t_ring_bei(int argc, char** argv);
static int t_ring_idt(int argc, char** argv);
static int t_ring_noop_transfer(int argc, char** argv);

static int t_u3auto_ctrl_loopback(int argc, char** argv);
static int t_u3auto_loopback(int argc, char** argv);
static int t_u3auto_loopback_scan(int argc, char** argv);
static int t_u3auto_loopback_sg(int argc, char** argv);
static int t_u3auto_loopback_scan_sg(int argc, char** argv);
static int t_u3auto_random_suspend(int argc, char** argv);
static int t_u3auto_random_wakeup(int argc, char**argv);
static int t_u3auto_randomstop_dev(int argc, char** argv);
static int t_u3auto_stress(int argc, char** argv);
static int t_u3auto_isoc_frame_id(int argc, char** argv);
static int t_u3auto_concurrent_remotewakeup(int argc, char** argv);
static int t_u3auto_concurrent_u1u2_enter(int argc, char** argv);
static int t_u3auto_concurrent_u1u2_exit(int argc, char** argv);

static int t_hub_configurehub(int argc, char** argv);
static int t_hub_configuresubhub(int argc, char** argv);
static int t_hub_configuredevice(int argc, char** argv);
static int t_hub_ixia_stress(int argc, char** argv);
static int t_hub_loop_stress(int argc, char** argv);
static int t_hub_loop(int argc, char** argv);
static int t_hub_reset_dev(int argc, char** argv);
static int t_hub_remotewakeup_dev(int argc, char** argv);
static int t_hub_selsuspend(int argc, char** argv);
static int t_hub_selresume(int argc, char** argv);
static int t_hub_configure_eth_device(int argc, char** argv);
static int t_hub_queue_intr(int argc, char** argv);
static int t_hub_set_u1u2(int argc, char** argv);
static int t_hub_force_pm(int argc, char** argv);

static int t_u3h_otg_dev_A_host(int argc,char** argv);
static int t_u3h_otg_dev_B_host(int argc,char** argv);
static int t_u3h_otg_dev_A_srp(int argc,char** argv);
static int t_u3h_otg_dev_A_hnp(int argc,char** argv);
static int t_u3h_otg_dev_A_hnp_back(int argc,char** argv);
static int t_u3h_otg_dev_B_hnp(int argc,char** argv);
static int t_u3h_otg_dev_B_hnp_back(int argc, char** argv);
static int t_u3h_otg_uut_A(int argc, char** argv);
static int t_u3h_otg_uut_B(int argc,char * * argv);
static int t_u3h_otg_pet_uutA(int argc, char** argv);
static int t_u3h_otg_pet_uutB(int argc, char** argv);

static int t_hcd_init(int argc, char** argv);
static int t_hcd_cleanup(int argc, char** argv);

static int t_drv_vbus(int argc, char** argv);
static int t_stop_vbus(int argc, char** argv);

static int t_start_host_test_mode(int argc, char** argv);
static int t_stop_host_test_mode(int argc, char** argv);

/********************************************************
 *	dbg.portstatus
 *		Print current port status register value
 *	args:
*********************************************************/
static int dbg_printportstatus(int argc, char** argv);
/********************************************************
 *	dbg.dbgslt
 *		Print current slot context content
 *	args:
*********************************************************/
static int dbg_printslotcontext(int argc, char** argv);
/********************************************************
 *	dbg.hccparams
 *		Print HCCPARAM values
 *	args:
*********************************************************/
static int dbg_printhccparams(int argc, char** argv);
/********************************************************
 *	dbg.r
 *		Print xhci related register value
 *	args:
 *		1. address: offset
 *		2. length
*********************************************************/
static int dbg_read_xhci(int argc, char** argv);
/********************************************************
 *	dbg.dr
 *		print all xhci registers
 *	args:
*********************************************************/
static int dbg_dump_regs(int argc, char** argv);

static int dbg_port_set_pls(int argc, char** argv);
static int dbg_port_set_ped(int argc, char** argv);
static int dbg_port_reset(int argc, char** argv);

static int dbg_delayms(int argc, char** argv);
static int dbg_u3w(int argc, char**argv);
static int dbg_u3r(int argc, char**argv);
static int dbg_u3PHY_init(int argc, char**argv);
static int dbg_u3_calibration(int argc, char** argv);
static int dbg_phy_eyescan(int argc, char** argv);
static int dbg_u2_testmode(int argc, char** argv);

static int dbg_u3ct_lecroy(int argc, char** argv);
static int dbg_u3ct_ellisys(int argc, char** argv);
static int dbg_u2ct(int argc, char** argv);

static int dbg_memorywrite(int argc, char** argv);
static int dbg_memoryread(int argc, char** argv);
static int dbg_sch_algorithm(int argc, char** argv);
static int dbg_reg_ewe(int argc, char** argv);

static int t_class_keyboard(int argc, char** argv);
static int t_ellysis_TD7_36(int argc, char** argv);

static int t_dev_config_ep(int argc,char * * argv);
static int t_dev_polling_status(int argc,char * * argv);
static int t_dev_query_status(int argc,char * * argv);
static int t_dev_remotewakeup(int argc,char * * argv);
static int t_dev_reset(int argc,char * * argv);
static int t_dev_init(int argc, char** argv);
static int t_dev_notification(int argc, char** argv);
static int t_dev_u1u2(int argc, char** argv);
static int t_dev_lpm(int argc, char** argv);


////////////////////////////////////////////////////////////////////////////
// USBIF
//#define CLI_MAGIC 'CLI'
#define CLI_MAGIC 'CLI_MU3H'
#define IOCTL_READ _IOR(CLI_MAGIC, 0, int)
#define IOCTL_WRITE _IOW(CLI_MAGIC, 1, int)

#define BUF_SIZE 200
#define MAX_ARG_SIZE 20

#define DMA_BURST_SIZE_TEST

////////////////////////////////////////////////////////////////////////////

int u2_initialize(int argc, char** argv);

////////////////////////////////////////////////////////////////////////////

typedef struct
{
	char name[256];
	int (*cb_func)(int argc, char** argv);
} CMD_TBL_T;

CMD_TBL_T _arPCmdTbl_host[] =
{
	{"hcd.init", &t_hcd_init},
	{"hcd.cleanup", &t_hcd_cleanup},
	{"hcd.drvvbus", &t_drv_vbus},
	{"hcd.stopvbus", &t_stop_vbus},
	{"hcd.starthosttestmode", &t_start_host_test_mode},
	{"hcd.stophosttestmode", &t_stop_host_test_mode},	
	{"slt.discon", &t_slot_discon},
	{"slt.disable", &t_slot_disable_slot},
	{"slt.reset", &t_slot_reset_device},
	{"slt.resetp", &t_slt_resetport},
	{"slt.enable", &t_slot_enable_slot},
	{"slt.addslt", &t_slot_address},
	{"slt.getdesc", &t_slot_forcegetdescriptor},
	{"slt.getbos", &t_slot_getbos},
	{"slt.setconf", &t_slot_setconf},
	{"slt.setu1u2", &t_slot_setu1u2},
	{"slt.devstat", &t_slot_getdevstatus},
	{"slt.evalctx", &t_slot_evaluate_context},
	{"slt.ped", &t_slt_ped},
	{"lb.config", &t_loopback_configep},
	{"lb.deconfig", &t_loopback_deconfigep},
	{"lb.loop", &t_loopback_loop},
	{"pw.suspend", &t_power_suspend},
	{"pw.suspendp", &t_power_suspendport},
	{"pw.resume", &t_power_resume},
	{"pw.resumep", &t_power_resumeport},
	{"pw.wakeup", &t_power_remotewakeup},
	{"pw.u1u2", &t_power_u1u2},
	{"pw.fla", &t_power_fla},
	{"pw.occ", &t_power_occ},
	{"pw.lpm", &t_power_u2_lpm},
	{"pw.swlpm", &t_power_u2_swlpm},
	{"pw.rdnreg", &t_power_random_access_regs},
	{"ring.erfull", &t_ring_er_full},
	{"ring.stopcmd", &t_ring_stop_cmd},
	{"ring.abortcmd", &t_ring_abort_cmd_add},
	{"ring.stopep", &t_ring_stop_ep},
	{"ring.rrd", &t_ring_random_ring_doorbell},
	{"ring.rstp", &t_ring_random_stop_ep},
	{"ring.enlarge", &t_ring_enlarge},
	{"ring.shrink", &t_ring_shrink},
	{"ring.intrmod", &t_ring_intr_moderation},
	{"ring.bei", &t_ring_bei},
	{"ring.idt", &t_ring_idt},
	{"ring.noop", &t_ring_noop_transfer},
	{"auto.lbctrl", &t_u3auto_ctrl_loopback},
	{"auto.lb", &t_u3auto_loopback},
	{"auto.lbscan", &t_u3auto_loopback_scan},
	{"auto.lbsg", &t_u3auto_loopback_sg},
	{"auto.lbsgscan", &t_u3auto_loopback_scan_sg},
	{"auto.randomsuspend", &t_u3auto_random_suspend},
	{"auto.randomwakeup", &t_u3auto_random_wakeup},
	{"auto.devrandomstop", &t_u3auto_randomstop_dev},
	{"auto.stress", &t_u3auto_stress},
	{"auto.isofrm", &t_u3auto_isoc_frame_id},
	{"auto.conresume", &t_u3auto_concurrent_remotewakeup},
	{"auto.conu1u2", &t_u3auto_concurrent_u1u2_enter},
	{"auto.conu1u2exit", &t_u3auto_concurrent_u1u2_exit},
	{"hub.config", &t_hub_configurehub},
	{"hub.subhub", &t_hub_configuresubhub},
	{"hub.dev", &t_hub_configuredevice},
	{"hub.ixia", &t_hub_ixia_stress},
	{"hub.lbstress", &t_hub_loop_stress},
	{"hub.loop", &t_hub_loop},
	{"hub.reset", &t_hub_reset_dev},
	{"hub.wakeup", &t_hub_remotewakeup_dev},
	{"hub.selsuspend", &t_hub_selsuspend},
	{"hub.selresume", &t_hub_selresume},
	{"hub.deveth", &t_hub_configure_eth_device},
	{"hub.intr", &t_hub_queue_intr},
	{"hub.u1u2", &t_hub_set_u1u2},
	{"hub.forcepm", &t_hub_force_pm},
	{"otg.deva", &t_u3h_otg_dev_A_host},
	{"otg.devb", &t_u3h_otg_dev_B_host},
	{"otg.srp", &t_u3h_otg_dev_A_srp},
	{"otg.hnpa", &t_u3h_otg_dev_A_hnp},
	{"otg.hnpabackh", &t_u3h_otg_dev_A_hnp_back},
	{"otg.hnpb", &t_u3h_otg_dev_B_hnp},
	{"otg.hnpbbackd", &t_u3h_otg_dev_B_hnp_back},
	//{"otg.uuta", &t_u3h_otg_uut_A},
	//{"otg.uutb", &t_u3h_otg_uut_B},
	{"otg.uuta", &t_u3h_otg_pet_uutA},
	{"otg.uutb", &t_u3h_otg_pet_uutB},
	{"dbg.portstatus", &dbg_printportstatus},
	{"dbg.dbgslt", &dbg_printslotcontext},
	{"dbg.hccparams", &dbg_printhccparams},
	{"dbg.r", &dbg_read_xhci},
	{"dbg.dr", &dbg_dump_regs},
	{"dbg.setpls", &dbg_port_set_pls},
	{"dbg.setped", &dbg_port_set_ped},
	{"dbg.portreset", &dbg_port_reset},
	{"dbg.mdelay", &dbg_delayms},
	{"dbg.u3w", &dbg_u3w},
	{"dbg.u3r", &dbg_u3r},
	{"dbg.u3i", &dbg_u3PHY_init},
	{"dbg.u3c", &dbg_u3_calibration},
	{"dbg.u3eyescan", &dbg_phy_eyescan},
	{"dbg.mw", &dbg_memorywrite},
	{"dbg.mr", &dbg_memoryread},
	{"dbg.kb", &t_ellysis_TD7_36},
	{"dbg.sch", &dbg_sch_algorithm},
	{"dbg.ewe", &dbg_reg_ewe},
	{"dbg.u2t", &dbg_u2_testmode},
	{"dbg.u3lect", &dbg_u3ct_lecroy},
	{"dbg.u3elct", &dbg_u3ct_ellisys},
	{"dbg.u2ct", &dbg_u2ct},
	{"dev.reset", &t_dev_reset},
	{"dev.pollstatus", &t_dev_polling_status},
	{"dev.qrystatus", &t_dev_query_status},
	{"dev.configep", &t_dev_config_ep},
	{"dev.wakeup", &t_dev_remotewakeup},
	{"dev.note", &t_dev_notification},
	{"dev.u1u2", &t_dev_u1u2},
	{"dev.lpm", &t_dev_lpm},
	{"dev.init", &t_dev_init},
	{NULL, NULL}
};

////////////////////////////////////////////////////////////////////////////

char w_buf_host[BUF_SIZE];
char r_buf_host[BUF_SIZE] = "this is a test";

////////////////////////////////////////////////////////////////////////////

int call_function_host(char *buf)
{
	int i;
	int argc;
	char *argv[MAX_ARG_SIZE];

	argc = 0;
	do
	{
		argv[argc] = strsep(&buf, " ");
		mtk_test_dbg( "[%d] %s\r\n", argc, argv[argc]);
		argc++;
	} while (buf);

	for (i = 0; i < sizeof(_arPCmdTbl_host)/sizeof(CMD_TBL_T); i++)
	{
		if ((!strcmp(_arPCmdTbl_host[i].name, argv[0])) && (_arPCmdTbl_host[i].cb_func != NULL))
			return _arPCmdTbl_host[i].cb_func(argc, argv);
	}

	return -1;
}

struct numsection
{
	int min;
	int max;
	int current_value;
	struct numsection *next;
};

struct numsection *init_num_sec(int min, int max){
	struct numsection *tmp;
	tmp = kmalloc(sizeof(struct numsection), GFP_NOIO);
	tmp->min = min;
	tmp->max = max;
	tmp->current_value = min;
	tmp->next = NULL;
	return tmp;
}

void clear_num_secs(struct numsection *num_sec){
	struct numsection *next;
	struct numsection *cur;
	cur = num_sec;
	while(cur != NULL){
		next = cur->next;
		kfree(cur);
		cur = next;
	}
}

void add_num_sec(int min, int max, struct numsection *sec){
	struct numsection *tmp, *cur;
	cur = sec;
	while(cur->next != NULL){
		cur = cur->next;
	}
	tmp = kmalloc(sizeof(struct numsection), GFP_NOIO);
	tmp->min = min;
	tmp->max = max;
	tmp->current_value = min;
	tmp->next = NULL;
	cur->next = tmp;
}

struct numsection *find_next_num(struct numsection *sec){
	struct numsection *cur;
	cur = sec;
	cur->current_value++;
	if(cur->current_value > cur->max){
		cur->current_value = cur->min;
		cur = cur->next;
	}
	return cur;
}

static int xhci_mtk_test_open(struct inode *inode, struct file *file)
{

    mtk_test_dbg( "xhci_mtk_test open: successful\n");
    return 0;
}

static int xhci_mtk_test_release(struct inode *inode, struct file *file)
{

    mtk_test_dbg( "xhci_mtk_test release: successful\n");
    return 0;
}

static ssize_t xhci_mtk_test_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{

    mtk_test_dbg( "xhci_mtk_test read: returning zero bytes\n");
    return 0;
}

static ssize_t xhci_mtk_test_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{

    mtk_test_dbg( "xhci_mtk_test write: accepting zero bytes\n");
    return 0;
}

static int xhci_mtk_test_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

    int len = BUF_SIZE;

	switch (cmd) {
		case IOCTL_READ:
			copy_to_user((char *) arg, r_buf_host, len);
			mtk_test_dbg( "IOCTL_READ: %s\r\n", r_buf_host);
			break;
		case IOCTL_WRITE:
			copy_from_user(w_buf_host, (char *) arg, len);
			mtk_test_dbg( "IOCTL_WRITE: %s\r\n", w_buf_host);

			//invoke function
			return call_function_host(w_buf_host);
			break;
		default:
			return -ENOTTY;
	}

	return len;
}

static int xhci_mtk_test_unlock_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

    int len = BUF_SIZE;

	switch (cmd) {
		case IOCTL_READ:
			copy_to_user((char *) arg, r_buf_host, len);
			mtk_test_dbg( "IOCTL_READ: %s\r\n", r_buf_host);
			break;
		case IOCTL_WRITE:
			copy_from_user(w_buf_host, (char *) arg, len);
			mtk_test_dbg( "IOCTL_WRITE: %s\r\n", w_buf_host);

			//invoke function
			return call_function_host(w_buf_host);
			break;
		default:
			return -ENOTTY;
	}

	return len;
}


static void print_slot_state(int slot_state){
	switch (slot_state) {
	case 0:
		mtk_test_dbg( "slot state: enabled/disabled\n");
		break;
	case 1:
		mtk_test_dbg( "slot state: default\n");
		break;
	case 2:
		mtk_test_dbg( "slot state: addressed\n");
		break;
	case 3:
		mtk_test_dbg( "slot state: configured\n");
		break;
	default:
		mtk_test_dbg( "slot state: reserved\n");
		break;
	}
}

extern int xhci_usbif_resource_get(void) ;

static int t_hcd_init(int argc, char** argv){

	xhci_usbif_resource_get() ;
	f_test_lib_init();
	return 0 ;
}

static int t_hcd_cleanup(int argc, char** argv){
	return f_test_lib_cleanup();
}

void framecount_disabe(){
    writel(0x01, 0xf1270944);
    mtk_test_dbg("disable frame counter\n");
}

void framecount_enable(){
    writel(0x00, 0xf1270944);
    mtk_test_dbg("enable frame counter\n");
}
int u3auto_hcd_reset(void)
{
	if(f_test_lib_init() != RET_SUCCESS)
		return RET_FAIL;

    #if 0
    framecount_disabe();
    #endif

	if(f_enable_port(0) != RET_SUCCESS)
		return RET_FAIL;
	if(f_enable_slot(NULL) != RET_SUCCESS)
		return RET_FAIL;

    #if 0
    msleep(1000);
    framecount_enable();
    msleep(30);
    #endif

	if(f_address_slot(false, NULL) != RET_SUCCESS)
		return RET_FAIL;

}

static int t_slot_discon(int argc, char** argv){
	int ret;

	ret = f_disconnect_port(0);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	ret = f_disable_slot();
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
}

static int t_slot_disable_slot(int argc, char** argv){
	int ret;
	struct xhci_hcd *xhci;
	ret = 0;
	if(my_hcd == NULL){
		mtk_test_dbg( "[ERROR]host controller driver not initiated\n");
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);

	ret = f_disable_slot();

	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int t_slot_evaluate_context(int argc, char** argv){
	int ret;
	int max_exit_latency, maxp0, preping_mode, preping;
	int besld, besl;

	ret = 0;
	max_exit_latency = 0;
	maxp0 = 64;
	preping_mode = 0;
	preping = 0;

	if(my_hcd == NULL){
		mtk_test_dbg( "[ERROR]host controller driver not initiated\n");
		return RET_FAIL;
	}
	if(argc > 1){
		max_exit_latency = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	if(argc > 2){
		maxp0 = (int)simple_strtol(argv[2], &argv[2], 10);
	}
	if(argc > 3){
		preping_mode = (int)simple_strtol(argv[3], &argv[3], 10);
	}
	if(argc > 4){
		preping = (int)simple_strtol(argv[4], &argv[4], 10);
	}
	if(argc > 5){
		besl = (int)simple_strtol(argv[5], &argv[5], 10);
	}
	if(argc > 6){
		besld = (int)simple_strtol(argv[6], &argv[6], 10);
	}
	ret = f_evaluate_context(max_exit_latency, maxp0, preping_mode, preping, besl, besld);

	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int t_slot_getbos(int argc, char** argv){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	void *buf;
	short *tmp_length, total_length;
	ret = 0;

	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	//get total length in bos
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_BOS << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_BOS_SIZE);
	buf = kmalloc(USB_DT_BOS_SIZE, GFP_KERNEL);
	memset(buf, 0, USB_DT_BOS_SIZE);

	urb = alloc_ctrl_urb(dr, buf, udev);
	ret = f_ctrlrequest(urb,udev);

	tmp_length = urb->transfer_buffer+2;
	total_length = *tmp_length;
	xhci_dbg(xhci, "total_length: %d\n", total_length);

	kfree(dr);
	kfree(buf);
	usb_free_urb(urb);
//	kfree(urb);

	//get bos
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_BOS << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(total_length);
	buf = kmalloc(total_length, GFP_KERNEL);
	memset(buf, 0 , total_length);
	urb = alloc_ctrl_urb(dr, buf, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	kfree(buf);
	usb_free_urb(urb);
//	kfree(urb);
	return ret;
}

static int t_slot_forcegetdescriptor(int argc, char** argv){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);

	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	if(urb->status == -EINPROGRESS){
		//timeout, stop endpoint, set TR dequeue pointer
		f_ring_stop_ep(g_slot_id, 0);
		f_ring_set_tr_dequeue_pointer(g_slot_id, 0, urb);
	}
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
//	kfree(urb);

	return ret;
}

static int t_slot_getdescriptor(int argc, char** argv){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);

	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);

	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
//	kfree(urb);

	return ret;
}

static int t_slot_setconf(int argc, char** argv){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	int i;
	char *tmp;
	char isConfigMouse;
	ret = 0;

	isConfigMouse = false;

	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	if(argc > 1){
		if(!strcmp(argv[1], "mouse")){
			isConfigMouse = true;
		}
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	if(isConfigMouse){
		//config mouse for Ellysis T7.36 test case
		dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
		dr->bRequestType = 0x21;
		dr->bRequest = 0x0a;
		dr->wValue = cpu_to_le16(0);
		dr->wIndex = cpu_to_le16(0);
		dr->wLength = cpu_to_le16(0);
		urb = alloc_ctrl_urb(dr, NULL, udev);
		ret = f_ctrlrequest(urb,udev);
		kfree(dr);
		usb_free_urb(urb);
	}
	else{
		//set configuration
		dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
		dr->bRequestType = USB_DIR_OUT;
		dr->bRequest = USB_REQ_SET_CONFIGURATION;
		dr->wValue = cpu_to_le16(1);
		dr->wIndex = cpu_to_le16(0);
		dr->wLength = cpu_to_le16(0);
		urb = alloc_ctrl_urb(dr, NULL, udev);
		ret = f_ctrlrequest(urb,udev);
		kfree(dr);
		usb_free_urb(urb);
	}
//	kfree(urb);
	return ret;
}

static int t_slot_setu1u2(int argc, char** argv){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	//set feature - u1 enable
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_FEATURE;
	dr->wValue = cpu_to_le16(USB_U1_ENABLE);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);

//	kfree(urb);
	usb_free_urb(urb);
	kfree(dr);

	//set feature - u2 enable
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_FEATURE;
	dr->wValue = cpu_to_le16(USB_U2_ENABLE);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);

	kfree(dr);
	usb_free_urb(urb);
//	kfree(urb);
	return ret;
}

static int t_slot_getdevstatus(int argc, char** argv){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	//get status
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_STATUS;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_STATUS_SIZE);
	desc = kmalloc(USB_DT_STATUS_SIZE, GFP_KERNEL);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
//	kfree(urb);
	return ret;
}

static int t_slt_resetport(int argc, char** argv){
	u32 __iomem *addr;
	int temp;
	int port_id;
	char isWarmReset;
	struct xhci_hcd *xhci;

	port_id = g_port_id;
	isWarmReset = false;

	if(argc > 1){
		if(!strcmp(argv[1], "true")){
			mtk_test_dbg( "Do Warm Reset\n");
			isWarmReset = true;
		}
	}
	if(argc > 2){
		port_id = (int)simple_strtol(argv[2], &argv[2], 10);
	}

	xhci = hcd_to_xhci(my_hcd);

	if(isWarmReset){
		//do warm reset
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp | PORT_WR);
		xhci_writel(xhci, temp, addr);
		temp = xhci_readl(xhci, addr);
		xhci_dbg(xhci, "after reset port %d = 0x%x\n", port_id-1, temp);

	}
	else{
		//hot reset port
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp | PORT_RESET);
		xhci_writel(xhci, temp, addr);
	}
}

static int t_slt_ped(int argc, char** argv){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;
	int ret;

	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp |= PORT_PE;
	xhci_writel(xhci, temp, addr);
	ret = f_disconnect_port(0);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	ret = f_disable_slot();
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	return ret;
}

/* simply address a slot */
static int t_slot_address(int argc, char** argv){
	int ret;
	char isBSR;

	isBSR = false;
	if(argc > 1){
		if(!strcmp(argv[1], "true")){
			mtk_test_dbg( "test BSR=true\n");
			isBSR = true;
		}
	}

	return f_address_slot(isBSR, NULL);
}

static int t_slot_enable_slot(int argc, char** argv){
	char isEnablePort;
	isEnablePort = true;
	if(argc > 1){
		if(!strcmp(argv[1], "false")){
			mtk_test_dbg( "test BSR=true\n");
			isEnablePort = false;
		}
	}
	if(isEnablePort){
		if(f_enable_port(0) != RET_SUCCESS){
			return RET_FAIL;
		}
	}
	return f_enable_slot(NULL);
}

static int t_slot_reset_device(int argc, char** argv){
	char isWarmReset;
	isWarmReset = false;

	if(argc > 1){
		if(!strcmp(argv[1], "true")){
			mtk_test_dbg( "test WarmReset=true\n");
			isWarmReset = true;
		}
	}
	return f_slot_reset_device(0, isWarmReset);

}

static int t_u3auto_ctrl_loopback(int argc, char** argv){
	int ret,loop,length,num=0;
	char isFullSpeed, isReset;

	isFullSpeed = false;
	ret = 0;
	num = 1;
	length = 8;
	isReset = false;
	if(argc > 1){
		num = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	if(argc > 2){
		length = (int)simple_strtol(argv[2], &argv[2], 10);
	}
	if(argc > 3){
		if(!strcmp(argv[3], "ss")){
			mtk_test_dbg( "test super speed\n");
			isFullSpeed = false;
		}
		if(!strcmp(argv[3], "hs")){
			mtk_test_dbg( "test high speed\n");
			isFullSpeed = false;
		}
		if(!strcmp(argv[3], "fs")){
			mtk_test_dbg( "Test full speed\n");
			isFullSpeed = true;
		}
	}
	if(argc > 4){
		if(!strcmp(argv[4], "true")){
			mtk_test_dbg( "Reset device\n");
			isReset = true;
		}
	}
#if 0
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
#endif
	if(isFullSpeed && isReset){
		start_port_reenabled(0, DEV_SPEED_FULL);

		ret=dev_reset(DEV_SPEED_FULL,NULL);
		if(ret){
			mtk_test_dbg( "Set device to full speed failed!!\n");
			return RET_FAIL;
		}

		ret = f_disable_slot();
		if(ret){
			mtk_test_dbg( "disable slot failed!!\n");
			return RET_FAIL;
		}

		ret = f_reenable_port(0);
		if(ret){
			mtk_test_dbg( "reenable port failed!!\n");
			return RET_FAIL;
		}

		ret = f_enable_slot(NULL);
		if(ret){
			mtk_test_dbg( "enable slot failed!!\n");
			return RET_FAIL;
		}

		ret=f_address_slot(false, NULL);
		if(ret){
			mtk_test_dbg( "address device failed!!\n");
			return RET_FAIL;
		}
	}

	//num = 0, loop forever
	//num = 1, follow the length that user input
	//num != 1, loopback incremental length data
	int max_length = 2048;
	if(num == 0){
		for(length=1; length <= max_length;){
			mtk_test_dbg( "Do CTRL loopback, length %d\n", length);
			ret=dev_ctrl_loopback(length,NULL);


			if(ret)
			{
				mtk_test_dbg( "ctrl loop fail!!\n");
				mtk_test_dbg( "length : %d\n",length);
				break;
			}
			length = length + 1;
		}
	}
	else{
		for(loop=0;loop<num;loop++){
			if(num != 1){
				length=((((loop+1)*102400)+loop*40)%2048);
			}

			if(!length){
				length=2048;
			}
			mtk_test_dbg( "Do CTRL loopback, length %d\n", length);
			ret=dev_ctrl_loopback(length,NULL);

			if(ret)
			{
				mtk_test_dbg( "ctrl loop fail!!\n");
				mtk_test_dbg( "length : %d\n",length);
				break;
			}

		}
	}
	if(ret){
		mtk_test_dbg( "[FAIL] Ctrl loop back test failed\n");
	}
	else{
		mtk_test_dbg( "[PASS] ctrl loop back [%d] round\n",num);
	}
	return ret;
}

#define SG_SS_BULK_INTERVAL_SIZE	1
#define SG_SS_BULK_MAXP_SIZE		1
#define SG_SS_BULK_OUT_EP_SIZE		1
#define SG_SS_BULK_IN_EP_SIZE		1
#define SG_SS_BULK_SG_LEN_SIZE		4
#define SG_SS_BULK_BURST_SIZE		1
#define SG_SS_BULK_MULT_SIZE		1

#define SG_SS_INTR_INTERVAL_SIZE	1
#define SG_SS_INTR_MAXP_SIZE		1
#define SG_SS_INTR_OUT_EP_SIZE		1
#define SG_SS_INTR_IN_EP_SIZE		1
#define SG_SS_INTR_SG_LEN_SIZE		3
#define SG_SS_INTR_BURST_SIZE		1
#define SG_SS_INTR_MULT_SIZE		1

#define SG_HS_BULK_INTERVAL_SIZE	1
#define SG_HS_BULK_MAXP_SIZE		1
#define SG_HS_BULK_OUT_EP_SIZE		1
#define SG_HS_BULK_IN_EP_SIZE		1
#define SG_HS_BULK_SG_LEN_SIZE		4

#define SG_HS_INTR_INTERVAL_SIZE	1
#define SG_HS_INTR_MAXP_SIZE		1
#define SG_HS_INTR_OUT_EP_SIZE 		1
#define SG_HS_INTR_IN_EP_SIZE		1
#define SG_HS_INTR_SG_LEN_SIZE		3

#define SG_FS_BULK_INTERVAL_SIZE	1
#define SG_FS_BULK_MAXP_SIZE		1
#define SG_FS_BULK_OUT_EP_SIZE		1
#define SG_FS_BULK_IN_EP_SIZE		1
#define SG_FS_BULK_SG_LEN_SIZE		4

#define SG_FS_INTR_INTERVAL_SIZE	1
#define SG_FS_INTR_MAXP_SIZE		1
#define SG_FS_INTR_OUT_EP_SIZE		1
#define SG_FS_INTR_IN_EP_SIZE		1
#define SG_FS_INTR_SG_LEN_SIZE		3

#define DF_BURST_SIZE			1	//default for hs, fs
#define DF_MULT_SIZE			1	//default for hs, fs

static int t_u3auto_loopback_scan_sg(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int sg_len;
	int ep_out_num, ep_in_num;
	int speed;
	int start_add_index, ep_out_index, ep_in_index, maxp_index, interval_index, sg_len_index, mult_index, burst_index;
	int interval_arr_size, maxp_arr_size, out_ep_arr_size, in_ep_arr_size, sg_len_arr_size, mult_arr_size, burst_arr_size;
	int min_length, max_length, min_start_add, max_start_add;
	int *arr_interval, *arr_maxp, *arr_ep_out, *arr_ep_in, *arr_sg_len, *arr_mult, *arr_burst;
	int mult_dev, burst, mult;
	int dram_offset, extension;
	struct numsection *sec, *cur_sec;

	int sg_ss_bulk_interval[SG_SS_BULK_INTERVAL_SIZE] = {0};
	int sg_ss_bulk_maxp[SG_SS_BULK_MAXP_SIZE] = {1024};
	int sg_ss_bulk_out_ep[SG_SS_BULK_OUT_EP_SIZE] = {1};
	int sg_ss_bulk_in_ep[SG_SS_BULK_IN_EP_SIZE] = {1};
	int sg_ss_bulk_sg_len[SG_SS_BULK_SG_LEN_SIZE] = {512, 1024, 2048, 4096};
	int sg_ss_bulk_burst[SG_SS_BULK_BURST_SIZE] = {15};
	int sg_ss_bulk_mult[SG_SS_BULK_MULT_SIZE] = {0};

	int sg_ss_intr_interval[SG_SS_INTR_INTERVAL_SIZE] = {1};
	int sg_ss_intr_maxp[SG_SS_INTR_MAXP_SIZE] = {1024};
	int sg_ss_intr_out_ep[SG_SS_INTR_OUT_EP_SIZE] = {1};
	int sg_ss_intr_in_ep[SG_SS_INTR_IN_EP_SIZE] = {1};
	int sg_ss_intr_sg_len[SG_SS_INTR_SG_LEN_SIZE] = {64,512,1024};
	int sg_ss_intr_burst[SG_SS_INTR_BURST_SIZE] = {0/*,1,2*/};
	int sg_ss_intr_mult[SG_SS_INTR_MULT_SIZE] = {0};

	int sg_hs_bulk_interval[SG_HS_BULK_INTERVAL_SIZE] = {0};
	int sg_hs_bulk_maxp[SG_HS_BULK_MAXP_SIZE] = {512};
	int sg_hs_bulk_out_ep[SG_HS_BULK_OUT_EP_SIZE] = {1};
	int sg_hs_bulk_in_ep[SG_HS_BULK_IN_EP_SIZE] = {1};
	int sg_hs_bulk_sg_len[SG_HS_BULK_SG_LEN_SIZE] = {512, 1024, 2048, 4096};

	int sg_hs_intr_interval[SG_HS_INTR_INTERVAL_SIZE] = {1};
	int sg_hs_intr_maxp[SG_HS_INTR_MAXP_SIZE] = {1024};	//32, 512, 1024
	int sg_hs_intr_out_ep[SG_HS_INTR_OUT_EP_SIZE] = {2};
	int sg_hs_intr_in_ep[SG_HS_INTR_IN_EP_SIZE] = {2};
	int sg_hs_intr_sg_len[SG_HS_INTR_SG_LEN_SIZE] = {64, 512, 1024};

	int sg_fs_bulk_interval[SG_FS_BULK_INTERVAL_SIZE] = {0};
	int sg_fs_bulk_maxp[SG_FS_BULK_MAXP_SIZE] = {64};
	int sg_fs_bulk_out_ep[SG_FS_BULK_OUT_EP_SIZE] = {1};
	int sg_fs_bulk_in_ep[SG_FS_BULK_IN_EP_SIZE] = {1};
	int sg_fs_bulk_sg_len[SG_FS_BULK_SG_LEN_SIZE] = {64, 512, 1024, 2048};

	int sg_fs_intr_interval[SG_FS_INTR_INTERVAL_SIZE] = {1};// 1, 127,255
	int sg_fs_intr_maxp[SG_FS_INTR_MAXP_SIZE] = {32}; //8, 32, 64
	int sg_fs_intr_out_ep[SG_FS_INTR_OUT_EP_SIZE] = {2};
	int sg_fs_intr_in_ep[SG_FS_INTR_IN_EP_SIZE] = {2};
	int sg_fs_intr_sg_len[SG_FS_INTR_SG_LEN_SIZE] = {64,512,1024};

	int df_burst[DF_BURST_SIZE] = {0};
	int df_mult[DF_MULT_SIZE] = {0};

	ret = 0;
	speed = DEV_SPEED_HIGH;;
	transfer_type = EPATT_BULK;
	maxp = 512;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	mult_dev = 3;
	mult = 0;
	burst = 8;
	dram_offset = 0;
	extension = 0;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER; //TODO: superspeed
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(argc > 2){
		if(!strcmp(argv[2], "bulk")){
			mtk_test_dbg( "Test bulk transfer\n");
			transfer_type = EPATT_BULK;
		}
		else if(!strcmp(argv[2], "intr")){
			mtk_test_dbg( "Test intr transfer\n");
			transfer_type = EPATT_INT;
		}
		else if(!strcmp(argv[2], "isoc")){
			mtk_test_dbg( "Test isoc transfer\n");
			transfer_type = EPATT_ISO;
		}
	}

	if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_BULK){
		arr_maxp = sg_ss_bulk_maxp;
		maxp_arr_size = SG_SS_BULK_MAXP_SIZE;
		arr_interval = sg_ss_bulk_interval;
		interval_arr_size = SG_SS_BULK_INTERVAL_SIZE;
		//length 1~65535
		sec = init_num_sec(513, 1025);
		add_num_sec(10240-10, 10240+10,sec);
		add_num_sec(65535-10,65535,sec);
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		arr_ep_out = sg_ss_bulk_out_ep;
		out_ep_arr_size = SG_SS_BULK_OUT_EP_SIZE;
		arr_ep_in = sg_ss_bulk_in_ep;
		in_ep_arr_size = SG_SS_BULK_IN_EP_SIZE;

		//sg_len
		arr_sg_len = sg_ss_bulk_sg_len;
		sg_len_arr_size = SG_SS_BULK_SG_LEN_SIZE;

		arr_burst = sg_ss_bulk_burst;
		burst_arr_size = SG_SS_BULK_BURST_SIZE;
		arr_mult = sg_ss_bulk_mult;
		mult_arr_size = SG_SS_BULK_MULT_SIZE;
	}

	if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_INT){
		arr_maxp = sg_ss_intr_maxp;
		maxp_arr_size = SG_SS_INTR_MAXP_SIZE;
		arr_interval = sg_ss_intr_interval;
		interval_arr_size = SG_SS_INTR_INTERVAL_SIZE;
		//length 1~65535
		sec = init_num_sec(513, 1025);
		add_num_sec(10240-10, 10240+10,sec);
		add_num_sec(65535-10,65535,sec);
		//start_add 0
		min_start_add = 0;
		max_start_add = 0;
		arr_ep_out = sg_ss_intr_out_ep;
		out_ep_arr_size = SG_SS_INTR_OUT_EP_SIZE;
		arr_ep_in = sg_ss_intr_in_ep;
		in_ep_arr_size = SG_SS_INTR_IN_EP_SIZE;

		//sg_len
		arr_sg_len = sg_ss_intr_sg_len;
		sg_len_arr_size = SG_SS_INTR_SG_LEN_SIZE;

		arr_burst = sg_ss_intr_burst;
		burst_arr_size = SG_SS_INTR_BURST_SIZE;
		arr_mult = sg_ss_intr_mult;
		mult_arr_size = SG_SS_INTR_MULT_SIZE;
	}

	if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_BULK){
		arr_maxp = sg_hs_bulk_maxp;
		maxp_arr_size = SG_HS_BULK_MAXP_SIZE;
		arr_interval = sg_hs_bulk_interval;
		interval_arr_size = SG_HS_BULK_INTERVAL_SIZE;
		//length 4096~65535
		sec = init_num_sec(1,513);
		add_num_sec(10240-10,10240+10,sec);
		add_num_sec(65535-10,65535,sec);
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		//ep_out_num 1~15
		arr_ep_out = sg_hs_bulk_out_ep;
		out_ep_arr_size = SG_HS_BULK_OUT_EP_SIZE;
		//ep_in_num 1~15
		arr_ep_in = sg_hs_bulk_in_ep;
		in_ep_arr_size = SG_HS_BULK_IN_EP_SIZE;
		//sg_len
		arr_sg_len = sg_hs_bulk_sg_len;
		sg_len_arr_size = SG_HS_BULK_SG_LEN_SIZE;

		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
	if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_INT){
		arr_maxp = sg_hs_intr_maxp;
		maxp_arr_size = SG_HS_INTR_MAXP_SIZE;
		arr_interval = sg_hs_intr_interval;
		interval_arr_size = SG_HS_INTR_INTERVAL_SIZE;
		//length 1~2048
		sec = init_num_sec(1, 1025);
		add_num_sec(10240-10, 10240+10,sec);
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		//ep_out_num 1,8,15
		arr_ep_out = sg_hs_intr_out_ep;
		out_ep_arr_size = SG_HS_INTR_OUT_EP_SIZE;
		//ep_in_num 1,8,15
		arr_ep_in = sg_hs_intr_in_ep;
		in_ep_arr_size = SG_HS_INTR_IN_EP_SIZE;
		//sg_len
		arr_sg_len = sg_hs_intr_sg_len;
		sg_len_arr_size = SG_HS_INTR_SG_LEN_SIZE;

		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
	if(speed == DEV_SPEED_FULL && transfer_type == EPATT_BULK){
		arr_maxp = sg_fs_bulk_maxp;
		maxp_arr_size = SG_FS_BULK_MAXP_SIZE;
		arr_interval = sg_fs_bulk_interval;
		interval_arr_size = SG_FS_BULK_INTERVAL_SIZE;
		//length 4096~10241
		sec = init_num_sec(513, 1025);
		add_num_sec(10240-10, 10240+10,sec);
		add_num_sec(65535-10,65535,sec);
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		//ep_out_num 1~15
		arr_ep_out = sg_fs_bulk_out_ep;
		out_ep_arr_size = SG_FS_BULK_OUT_EP_SIZE;
		//ep_in_num 1~15
		arr_ep_in = sg_fs_bulk_in_ep;
		in_ep_arr_size = SG_FS_BULK_IN_EP_SIZE;
		//sg_len
		arr_sg_len = sg_fs_bulk_sg_len;
		sg_len_arr_size = SG_FS_BULK_SG_LEN_SIZE;

		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;

	}
	if(speed == DEV_SPEED_FULL && transfer_type == EPATT_INT){
		arr_maxp = sg_fs_intr_maxp;
		maxp_arr_size = SG_FS_INTR_MAXP_SIZE;
		arr_interval = sg_fs_intr_interval;
		interval_arr_size = SG_FS_INTR_INTERVAL_SIZE;
		//length 1024~4097
		sec = init_num_sec(1, 1025);
		add_num_sec(2048-10, 2048+10,sec);
		add_num_sec(3072-10, 3072+10,sec);
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		//ep_out_num 1,8,15
		arr_ep_out = sg_fs_intr_out_ep;
		out_ep_arr_size = SG_FS_INTR_OUT_EP_SIZE;
		//ep_in_num 1,8,15
		arr_ep_in = sg_fs_intr_in_ep;
		in_ep_arr_size = SG_FS_INTR_IN_EP_SIZE;
		//sg_len
		arr_sg_len = sg_fs_intr_sg_len;
		sg_len_arr_size = SG_FS_INTR_SG_LEN_SIZE;

		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
#if 0
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
#endif
	for(ep_in_index = 0; ep_in_index < in_ep_arr_size; ep_in_index++){
		ep_in_num = *(arr_ep_in + ep_in_index);
		for(ep_out_index = 0; ep_out_index < out_ep_arr_size; ep_out_index++){
			ep_out_num = *(arr_ep_out + ep_out_index);
			for(interval_index = 0; interval_index < interval_arr_size; interval_index++){
				bInterval = *(arr_interval + interval_index);
				for(maxp_index = 0; maxp_index < maxp_arr_size; maxp_index++){
					maxp = *(arr_maxp + maxp_index);
					for(mult_index = 0; mult_index < mult_arr_size; mult_index++){
						mult = *(arr_mult+mult_index);
						for(burst_index = 0; burst_index < burst_arr_size; burst_index++){
							burst = *(arr_burst+burst_index);
							mtk_test_dbg( "[TEST]*************************************\n");
							mtk_test_dbg( "      OUT_EP: %d***************************\n", ep_out_num);
							mtk_test_dbg( "      IN_EP: %d ***************************\n", ep_in_num);
							mtk_test_dbg( "      MAXP: %d  ***************************\n", maxp);
							mtk_test_dbg( "      INTERVAL: %d  ***************************\n", bInterval);
							mtk_test_dbg( "      MULT: %d **************************\n", mult);
							mtk_test_dbg( "      BURST: %d *************************\n", burst);
							/* ==phase 0 : device reset==*/
							start_port_reenabled(0, speed);
							ret=dev_reset(speed,NULL);
							if(ret){
								mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
								return ret;
							}

							ret = f_disable_slot();
							if(ret){
								mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
								return ret;
							}

							ret = f_reenable_port(0);
							if(ret != RET_SUCCESS){
								mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
								return ret;
							}

							ret = f_enable_slot(NULL);
							if(ret){
								mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
								return ret;
							}

							ret=f_address_slot(false, NULL);
							if(ret){
								mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
								return ret;
							}
							mtktest_mtk_xhci_scheduler_init();
							/* ==phase 1 : config EP==*/
							ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
							if(ret)
							{
								mtk_test_dbg( "config dev EP fail!!\n");
								return ret;
							}

							ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
							if(ret)
							{
								mtk_test_dbg( "config dev EP fail!!\n");
								return ret;
							}

							ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
							if(ret)
							{
								mtk_test_dbg( "config EP fail!!\n");
								return ret;
							}

							ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
							if(ret)
							{
								mtk_test_dbg( "config EP fail!!\n");
								return ret;
							}

							for(start_add = min_start_add; start_add <= max_start_add; start_add++){
								cur_sec = sec;
								while(cur_sec != NULL){
									length = cur_sec->current_value;
									for(sg_len_index = 0; sg_len_index < sg_len_arr_size; sg_len_index++){
										sg_len = *(arr_sg_len + sg_len_index);
										if((length/sg_len) > 61)
											continue;
										mtk_test_dbg( "      [ROUND]**************************\n");
										mtk_test_dbg( "             START_ADD: %d*************\n", start_add);
										mtk_test_dbg( "             LENGTH: %d   *************\n", length);
										mtk_test_dbg( "             SG_LEN: %d   *************\n", sg_len);
										/* ==phase 2 : loopback==*/
										bdp=1;
									    gpd_buf_size=length;
										bd_buf_size=4096;

										ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size, dram_offset, extension,NULL);
										if(ret)
										{
											mtk_test_dbg( "loopback request fail!!\n");
											f_power_suspend();
											return ret;
										}

		//								ret = f_loopback_sg_loop(
		//									ep_out_num, ep_in_num, length, start_add, sg_len, NULL);
										ret = f_loopback_sg_loop_gpd(
											ep_out_num,ep_in_num,length,start_add,sg_len, gpd_buf_size,NULL);
										if(ret)
										{
											mtk_test_dbg( "loopback fail!!\n");
											mtk_test_dbg( "length : %d\n",length);
											return ret;
										}

										/* ==phase 3: get device status==*/
										ret=dev_polling_status(NULL);
										if(ret)
										{
											f_power_suspend();
											mtk_test_dbg( "query request fail!!\n");
											return ret;
										}
									}  //sg_num
									cur_sec = find_next_num(cur_sec);
								} //length
							} //start_add
						} //burst
					} //mult
				} //maxp
			} //interval
		} //ep_out
	} //ep_in
	return ret;
}

#define SS_BULK_INTERVAL_SIZE	1
#define SS_BULK_MAXP_SIZE		1
#define SS_BULK_OUT_EP_SIZE		1
#define SS_BULK_IN_EP_SIZE		1
#define SS_BULK_BURST_SIZE		2
#define SS_BULK_MULT_SIZE		1

#define SS_INTR_INTERVAL_SIZE	2
#define SS_INTR_MAXP_SIZE		2
#define SS_INTR_OUT_EP_SIZE		1
#define SS_INTR_IN_EP_SIZE		1
#define SS_INTR_BURST_SIZE		1
#define SS_INTR_MULT_SIZE		1

#define SS_HB_INTR_INTERVAL_SIZE	1
#define SS_HB_INTR_MAXP_SIZE		1
#define SS_HB_INTR_OUT_EP_SIZE		1
#define SS_HB_INTR_IN_EP_SIZE		1
#define SS_HB_INTR_BURST_SIZE		2
#define SS_HB_INTR_MULT_SIZE		1

#define SS_ISOC_INTERVAL_SIZE	2
#define SS_ISOC_MAXP_SIZE		2
#define SS_ISOC_OUT_EP_SIZE		1
#define SS_ISOC_IN_EP_SIZE		1
#define SS_ISOC_BURST_SIZE		1
#define SS_ISOC_MULT_SIZE		1

#define SS_HB_ISOC_INTERVAL_SIZE	2
#define SS_HB_ISOC_MAXP_SIZE		1
#define SS_HB_ISOC_OUT_EP_SIZE		1
#define SS_HB_ISOC_IN_EP_SIZE		1
#define SS_HB_ISOC_BURST_SIZE		2
#define SS_HB_ISOC_MULT_SIZE		2

#define HS_BULK_INTERVAL_SIZE	1
#define HS_BULK_MAXP_SIZE		1
#define HS_BULK_OUT_EP_SIZE		1
#define HS_BULK_IN_EP_SIZE		1

#define HS_INTR_INTERVAL_SIZE	2
#define HS_INTR_MAXP_SIZE		4
#define HS_INTR_OUT_EP_SIZE 	1
#define HS_INTR_IN_EP_SIZE		1

#define HS_HB_INTR_INTERVAL_SIZE	2
#define HS_HB_INTR_MAXP_SIZE		1
#define HS_HB_INTR_OUT_EP_SIZE 	1
#define HS_HB_INTR_IN_EP_SIZE		1
#define HS_HB_INTR_MULT_SIZE		2

#define HS_ISOC_INTERVAL_SIZE	2
#define HS_ISOC_MAXP_SIZE		2
#define HS_ISOC_OUT_EP_SIZE 	1
#define HS_ISOC_IN_EP_SIZE		1

#define HS_HB_ISOC_INTERVAL_SIZE	2
#define HS_HB_ISOC_MAXP_SIZE		1
#define HS_HB_ISOC_OUT_EP_SIZE 	1
#define HS_HB_ISOC_IN_EP_SIZE		1
#define HS_HB_ISOC_MULT_SIZE		2

#define FS_BULK_INTERVAL_SIZE	1
#define FS_BULK_MAXP_SIZE		3
#define FS_BULK_OUT_EP_SIZE		1
#define FS_BULK_IN_EP_SIZE		1

#define FS_INTR_INTERVAL_SIZE	2
#define FS_INTR_MAXP_SIZE		4
#define FS_INTR_OUT_EP_SIZE		1
#define FS_INTR_IN_EP_SIZE		1

#define FS_ISOC_INTERVAL_SIZE	2
#define FS_ISOC_MAXP_SIZE		2
#define FS_ISOC_OUT_EP_SIZE		1
#define FS_ISOC_IN_EP_SIZE		1

#define DF_BURST_SIZE			1	//default for hs, fs
#define DF_MULT_SIZE			1	//default for hs, fs


static int t_u3auto_loopback_scan(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int start_add_index, ep_out_index, ep_in_index, maxp_index, interval_index, burst_index, mult_index;
	int interval_arr_size, maxp_arr_size, out_ep_arr_size, in_ep_arr_size, burst_arr_size, mult_arr_size;
	int min_length, max_length, min_start_add, max_start_add;
	int *arr_interval, *arr_maxp, *arr_ep_out, *arr_ep_in, *arr_burst, *arr_mult;
	int mult_dev, mult, burst;
	int dram_offset, extension;
	struct numsection *sec, *cur_sec;
	char isHB;
    #ifdef DMA_BURST_SIZE_TEST
    int dma_burst_sz = 0;
    int dma_tmp = 0;
    #endif

	int ss_bulk_interval[SS_BULK_INTERVAL_SIZE] = {0};
	int ss_bulk_maxp[SS_BULK_MAXP_SIZE] = {1024};
	int ss_bulk_out_ep[SS_BULK_OUT_EP_SIZE] = {1/*,2*/};
	int ss_bulk_in_ep[SS_BULK_IN_EP_SIZE] = {1/*,2*/};
	int ss_bulk_burst[SS_BULK_BURST_SIZE] = {0,15,/*0*/};
	int ss_bulk_mult[SS_BULK_MULT_SIZE] = {0};

	int ss_intr_interval[SS_INTR_INTERVAL_SIZE] = {1, 2/*, 16*/};
	int ss_intr_maxp[SS_INTR_MAXP_SIZE] = {8, /*32, 128, 512, */1024};
	int ss_intr_out_ep[SS_INTR_OUT_EP_SIZE] = {1};
	int ss_intr_in_ep[SS_INTR_IN_EP_SIZE] = {1};
	int ss_intr_burst[SS_INTR_BURST_SIZE] = {0};
	int ss_intr_mult[SS_INTR_MULT_SIZE] = {0};

	int ss_hb_intr_interval[SS_HB_INTR_INTERVAL_SIZE] = {1};
	int ss_hb_intr_maxp[SS_HB_INTR_MAXP_SIZE] = {1024};
	int ss_hb_intr_out_ep[SS_HB_INTR_OUT_EP_SIZE] = {1};
	int ss_hb_intr_in_ep[SS_HB_INTR_IN_EP_SIZE] = {1};
	int ss_hb_intr_burst[SS_HB_INTR_BURST_SIZE] = {1,2};
	int ss_hb_intr_mult[SS_HB_INTR_MULT_SIZE] = {0};

	int ss_isoc_interval[SS_ISOC_INTERVAL_SIZE] = {1, 2/*, 16*/};
	int ss_isoc_maxp[SS_ISOC_MAXP_SIZE] = {/*64, 512, */64,1024};
	int ss_isoc_out_ep[SS_ISOC_OUT_EP_SIZE] = {1};
	int ss_isoc_in_ep[SS_ISOC_IN_EP_SIZE] = {1};
	int ss_isoc_burst[SS_ISOC_BURST_SIZE] = {0};
	int ss_isoc_mult[SS_ISOC_MULT_SIZE] = {0};

	int ss_hb_isoc_interval[SS_HB_ISOC_INTERVAL_SIZE] = {1, 2};
	int ss_hb_isoc_maxp[SS_HB_ISOC_MAXP_SIZE] = {1024};
	int ss_hb_isoc_out_ep[SS_HB_ISOC_OUT_EP_SIZE] = {1};
	int ss_hb_isoc_in_ep[SS_HB_ISOC_IN_EP_SIZE] = {1};
	int ss_hb_isoc_burst[SS_HB_ISOC_BURST_SIZE] = {/*0,*/1,2};
	int ss_hb_isoc_mult[SS_HB_ISOC_MULT_SIZE] = {0,1/*,2*/};

	int hs_bulk_interval[HS_BULK_INTERVAL_SIZE] = {0};
	int hs_bulk_maxp[HS_BULK_MAXP_SIZE] = {512};
	int hs_bulk_out_ep[HS_BULK_OUT_EP_SIZE] = {1};
	int hs_bulk_in_ep[HS_BULK_IN_EP_SIZE] = {1};

	int hs_intr_interval[HS_INTR_INTERVAL_SIZE] = {1/*,7*/,2}; //0,8,15
	int hs_intr_maxp[HS_INTR_MAXP_SIZE] = {6, 8, 30/*,32,128,512*/,1024}; //8,16,128,512, 1024, 2048, 3072
	int hs_intr_out_ep[HS_INTR_OUT_EP_SIZE] = {1};
	int hs_intr_in_ep[HS_INTR_IN_EP_SIZE] = {1};

	int hs_hb_intr_interval[HS_HB_INTR_INTERVAL_SIZE] = {1/*,7*/,2}; //0,8,15
	int hs_hb_intr_maxp[HS_HB_INTR_MAXP_SIZE] = {1024}; //8,16,128,512, 1024, 2048, 3072
	int hs_hb_intr_out_ep[HS_HB_INTR_OUT_EP_SIZE] = {1};
	int hs_hb_intr_in_ep[HS_HB_INTR_IN_EP_SIZE] = {1};
	int hs_hb_intr_mult[HS_HB_INTR_MULT_SIZE] = {1,2};

	int hs_isoc_interval[HS_ISOC_INTERVAL_SIZE] = {1,2};
	int hs_isoc_maxp[HS_ISOC_MAXP_SIZE] = {/*8,32,*/128,/*512,*/1024};
	int hs_isoc_out_ep[HS_ISOC_OUT_EP_SIZE] = {1};
	int hs_isoc_in_ep[HS_ISOC_IN_EP_SIZE] = {1};

	int hs_hb_isoc_interval[HS_HB_ISOC_INTERVAL_SIZE] = {1,2};
	int hs_hb_isoc_maxp[HS_HB_ISOC_MAXP_SIZE] = {1024};
	int hs_hb_isoc_out_ep[HS_HB_ISOC_OUT_EP_SIZE] = {1};
	int hs_hb_isoc_in_ep[HS_HB_ISOC_IN_EP_SIZE] = {1};
	int hs_hb_isoc_mult[HS_HB_ISOC_MULT_SIZE] = {1,2};

	int fs_bulk_interval[FS_BULK_INTERVAL_SIZE] = {0};
	int fs_bulk_maxp[FS_BULK_MAXP_SIZE] = {8/*,16*/,32,64}; // 8,16,32,64
	int fs_bulk_out_ep[FS_BULK_OUT_EP_SIZE] = {1};
	int fs_bulk_in_ep[FS_BULK_IN_EP_SIZE] = {1};

	int fs_intr_interval[FS_INTR_INTERVAL_SIZE] = {1, 2}; //1, 127 ,255
	int fs_intr_maxp[FS_INTR_MAXP_SIZE] = {6, 8, 30,/*32,*/64}; // 8,32,64
	int fs_intr_out_ep[FS_INTR_OUT_EP_SIZE] = {1};
	int fs_intr_in_ep[FS_INTR_IN_EP_SIZE] = {1};

	int fs_isoc_interval[FS_ISOC_INTERVAL_SIZE] = {1, 2};
	int fs_isoc_maxp[FS_ISOC_MAXP_SIZE] = {/*8,32,*/128,1023};
	int fs_isoc_out_ep[FS_ISOC_OUT_EP_SIZE] = {1};
	int fs_isoc_in_ep[FS_ISOC_IN_EP_SIZE] = {1};

	int df_burst[DF_BURST_SIZE] = {0};
	int df_mult[DF_MULT_SIZE] = {0};

	ret = 0;
	speed = DEV_SPEED_HIGH;;
	transfer_type = EPATT_BULK;
	maxp = 512;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	isHB = false;
	length = 65535;
	mult_dev = 3;
	mult = 0;
	burst = 8;
	dram_offset = 0;
	extension = 0;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER;
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(argc > 2){
		if(!strcmp(argv[2], "bulk")){
			mtk_test_dbg( "Test bulk transfer\n");
			transfer_type = EPATT_BULK;
		}
		else if(!strcmp(argv[2], "intr")){
			mtk_test_dbg( "Test intr transfer\n");
			transfer_type = EPATT_INT;
		}
		else if(!strcmp(argv[2], "isoc")){
			mtk_test_dbg( "Test isoc transfer\n");
			transfer_type = EPATT_ISO;
		}
	}
	if(argc > 3){
		if(!strcmp(argv[3], "true")){
			mtk_test_dbg( "Test high bandwidth\n");
			isHB = true;
		}
		else{
			mtk_test_dbg( "Doesn't test high bandwidth\n");
			isHB = false;
		}
	}
	if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_BULK){
		arr_maxp = ss_bulk_maxp;
		maxp_arr_size = SS_BULK_MAXP_SIZE;
		arr_interval = ss_bulk_interval;
		interval_arr_size = SS_BULK_INTERVAL_SIZE;
		//1~1025, 10240+-512, 65535-1024~65535
#if 1
		sec = init_num_sec(1,1025);
		add_num_sec(10240-10,10240+10,sec);
		add_num_sec(65535-10,65535,sec);
#endif
//		sec = init_num_sec(1025,1027);
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		arr_ep_out = ss_bulk_out_ep;
		out_ep_arr_size = SS_BULK_OUT_EP_SIZE;
		arr_ep_in = ss_bulk_in_ep;
		in_ep_arr_size = SS_BULK_IN_EP_SIZE;
		arr_burst = ss_bulk_burst;
		burst_arr_size = SS_BULK_BURST_SIZE;
		arr_mult = ss_bulk_mult;
		mult_arr_size = SS_BULK_MULT_SIZE;
	}
	if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_INT && (!isHB)){
		arr_maxp = ss_intr_maxp;
		maxp_arr_size = SS_INTR_MAXP_SIZE;
		arr_interval = ss_intr_interval;
		interval_arr_size = SS_INTR_INTERVAL_SIZE;
		//length 1~1025,10241-1024~10241
#if 1
		sec = init_num_sec(1,1025);
		add_num_sec(2048-10,2048+10, sec);
		add_num_sec(3072-10,3072+10, sec);
		add_num_sec(10240-10,10240+10,sec);
#else
		sec = init_num_sec(1023,1027);
#endif
		//start_add 0
		min_start_add = 0;
		max_start_add = 0;
		arr_ep_out = ss_intr_out_ep;
		out_ep_arr_size = SS_INTR_OUT_EP_SIZE;
		arr_ep_in = ss_intr_in_ep;
		in_ep_arr_size = SS_INTR_IN_EP_SIZE;
		arr_burst = ss_intr_burst;
		burst_arr_size = SS_INTR_BURST_SIZE;
		arr_mult = ss_intr_mult;
		mult_arr_size = SS_INTR_MULT_SIZE;
	}
	if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_INT && isHB){
		arr_maxp = ss_hb_intr_maxp;
		maxp_arr_size = SS_HB_INTR_MAXP_SIZE;
		arr_interval = ss_hb_intr_interval;
		interval_arr_size = SS_HB_INTR_INTERVAL_SIZE;
		//length 1~1025,10241-1024~10241
#if 1
		sec = init_num_sec(1,1025);
		add_num_sec(2048-10,2048+10, sec);
		add_num_sec(3072-10,3072+10, sec);
		add_num_sec(10241-3,10241,sec);
#else
		sec = init_num_sec(1023,1027);
#endif
		//start_add 0
		min_start_add = 0;
		max_start_add = 0;
		arr_ep_out = ss_hb_intr_out_ep;
		out_ep_arr_size = SS_HB_INTR_OUT_EP_SIZE;
		arr_ep_in = ss_hb_intr_in_ep;
		in_ep_arr_size = SS_HB_INTR_IN_EP_SIZE;
		arr_burst = ss_hb_intr_burst;
		burst_arr_size = SS_HB_INTR_BURST_SIZE;
		arr_mult = ss_hb_intr_mult;
		mult_arr_size = SS_HB_INTR_MULT_SIZE;
	}
	if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_ISO && (!isHB)){
		arr_maxp = ss_isoc_maxp;
		maxp_arr_size = SS_ISOC_MAXP_SIZE;
		arr_interval = ss_isoc_interval;
		interval_arr_size = SS_ISOC_INTERVAL_SIZE;
		//length 1~1025, 10241-1024~10241, 65535-1024~65535
		//sec = init_num_sec(9217, 9225);
#if 1
		sec = init_num_sec(1,3);
		add_num_sec(1024-3,1024+3,sec);
		//sec = init_num_sec(1023,1025);
		add_num_sec(10241-3,10241,sec);
		//add_num_sec(65535-3,65535,sec);
#else
		sec = init_num_sec(1024*48-1,1024*48+1);
#endif
		//start_add 0
		min_start_add = 0;
		max_start_add = 0;
		arr_ep_out = ss_isoc_out_ep;
		out_ep_arr_size = SS_ISOC_OUT_EP_SIZE;
		arr_ep_in = ss_isoc_in_ep;
		in_ep_arr_size = SS_ISOC_IN_EP_SIZE;
		arr_burst = ss_isoc_burst;
		burst_arr_size = SS_ISOC_BURST_SIZE;
		arr_mult = ss_isoc_mult;
		mult_arr_size = SS_ISOC_MULT_SIZE;
	}
	if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_ISO && isHB){
		arr_maxp = ss_hb_isoc_maxp;
		maxp_arr_size = SS_HB_ISOC_MAXP_SIZE;
		arr_interval = ss_hb_isoc_interval;
		interval_arr_size = SS_HB_ISOC_INTERVAL_SIZE;
		//length 1~1025, 10241-1024~10241, 65535-1024~65535
		//sec = init_num_sec(9217, 9225);
#if 1
		sec = init_num_sec(1,3);
		add_num_sec(1024-3,1024+3,sec);
		//sec = init_num_sec(1023,1025);
		add_num_sec(10241-3,10240+3,sec);
		add_num_sec(65535-3,65535,sec);
#else
		sec = init_num_sec(1024*48-1,1024*48+1);
#endif
		//start_add 0
		min_start_add = 0;
		max_start_add = 0;
		arr_ep_out = ss_hb_isoc_out_ep;
		out_ep_arr_size = SS_HB_ISOC_OUT_EP_SIZE;
		arr_ep_in = ss_hb_isoc_in_ep;
		in_ep_arr_size = SS_HB_ISOC_IN_EP_SIZE;
		arr_burst = ss_hb_isoc_burst;
		burst_arr_size = SS_HB_ISOC_BURST_SIZE;
		arr_mult = ss_hb_isoc_mult;
		mult_arr_size = SS_HB_ISOC_MULT_SIZE;
	}
	if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_BULK){
		arr_maxp = hs_bulk_maxp;
		maxp_arr_size = HS_BULK_MAXP_SIZE;
		arr_interval = hs_bulk_interval;
		interval_arr_size = HS_BULK_INTERVAL_SIZE;
		//length 1~65535
		sec = init_num_sec(1,513);
		add_num_sec(10240-10, 10240+10,sec);
		add_num_sec(65535-10, 65535,sec);
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		//ep_out_num 1~15
		arr_ep_out = hs_bulk_out_ep;
		out_ep_arr_size = HS_BULK_OUT_EP_SIZE;
		//ep_in_num 1~15
		arr_ep_in = hs_bulk_in_ep;
		in_ep_arr_size = HS_BULK_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
	if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_INT && (!isHB)){
		arr_maxp = hs_intr_maxp;
		maxp_arr_size = HS_INTR_MAXP_SIZE;
		arr_interval = hs_intr_interval;
		interval_arr_size = HS_INTR_INTERVAL_SIZE;
		//length 1~2048
		sec = init_num_sec(1,1025);
		add_num_sec(2048-3,2048+3,sec);
		add_num_sec(3072-3,3072+3,sec);
		add_num_sec(10240-3,10240+3,sec);
//		min_length = 1;
//		max_length = 5120;
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		//ep_out_num 1,8,15
		arr_ep_out = hs_intr_out_ep;
		out_ep_arr_size = HS_INTR_OUT_EP_SIZE;
		//ep_in_num 1,8,15
		arr_ep_in = hs_intr_in_ep;
		in_ep_arr_size = HS_INTR_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
	if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_INT && (isHB)){
		arr_maxp = hs_hb_intr_maxp;
		maxp_arr_size = HS_HB_INTR_MAXP_SIZE;
		arr_interval = hs_hb_intr_interval;
		interval_arr_size = HS_HB_INTR_INTERVAL_SIZE;
		//length 1~2048
		sec = init_num_sec(1,1025);
		add_num_sec(2048-3,2048+3,sec);
		add_num_sec(3072-3,3072+3,sec);
		add_num_sec(10240-3,10240+3,sec);
//		min_length = 1;
//		max_length = 5120;
		//start_add 0~63
		min_start_add = 0;
		max_start_add = 0;
		//ep_out_num 1,8,15
		arr_ep_out = hs_hb_intr_out_ep;
		out_ep_arr_size = HS_HB_INTR_OUT_EP_SIZE;
		//ep_in_num 1,8,15
		arr_ep_in = hs_hb_intr_in_ep;
		in_ep_arr_size = HS_HB_INTR_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = hs_hb_intr_mult;
		mult_arr_size = HS_HB_INTR_MULT_SIZE;
	}
	if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_ISO && (!isHB)){
		arr_maxp = hs_isoc_maxp;
		maxp_arr_size = HS_ISOC_MAXP_SIZE;
		arr_interval = hs_isoc_interval;
		interval_arr_size = HS_ISOC_INTERVAL_SIZE;
		//length 1~10241
		sec = init_num_sec(1,3);
		add_num_sec(1024-3,1024+3,sec);
		add_num_sec(2048-3,2048+3,sec);
		add_num_sec(3072-3,3072+3,sec);
		add_num_sec(10240-3,10240+3,sec);
		//start_add 0
		min_start_add = 0;
		max_start_add = 0;

		arr_ep_out = hs_isoc_out_ep;
		out_ep_arr_size = HS_ISOC_OUT_EP_SIZE;
		arr_ep_in = hs_isoc_in_ep;
		in_ep_arr_size = HS_ISOC_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
	if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_ISO && (isHB)){
		arr_maxp = hs_hb_isoc_maxp;
		maxp_arr_size = HS_HB_ISOC_MAXP_SIZE;
		arr_interval = hs_hb_isoc_interval;
		interval_arr_size = HS_HB_ISOC_INTERVAL_SIZE;
		//length 1~10241
		sec = init_num_sec(1,3);
		add_num_sec(1024-3,1024+3,sec);
		add_num_sec(2048-3,2048+3,sec);
		add_num_sec(3072-3,3072+3,sec);
		add_num_sec(10240-3,10240+3,sec);
		//start_add 0
		min_start_add = 0;
		max_start_add = 0;

		arr_ep_out = hs_hb_isoc_out_ep;
		out_ep_arr_size = HS_HB_ISOC_OUT_EP_SIZE;
		arr_ep_in = hs_hb_isoc_in_ep;
		in_ep_arr_size = HS_HB_ISOC_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = hs_hb_isoc_mult;
		mult_arr_size = HS_HB_ISOC_MULT_SIZE;
	}
	if(speed == DEV_SPEED_FULL && transfer_type == EPATT_BULK){
		arr_maxp = fs_bulk_maxp;
		maxp_arr_size = FS_BULK_MAXP_SIZE;
		arr_interval = hs_bulk_interval;
		interval_arr_size = FS_BULK_INTERVAL_SIZE;
		//length 1~10241
		sec = init_num_sec(1,65);
		add_num_sec(10240-10,10240+10,sec);
		add_num_sec(65535-10,65535,sec);

		min_start_add = 0;
		max_start_add = 0;

		arr_ep_out = fs_bulk_out_ep;
		out_ep_arr_size = FS_BULK_OUT_EP_SIZE;
		arr_ep_in = fs_bulk_in_ep;
		in_ep_arr_size = FS_BULK_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
	if(speed == DEV_SPEED_FULL && transfer_type == EPATT_INT){
		arr_maxp = fs_intr_maxp;
		maxp_arr_size = FS_INTR_MAXP_SIZE;
		arr_interval = fs_intr_interval;
		interval_arr_size = FS_INTR_INTERVAL_SIZE;
		//length 1~10241
		sec = init_num_sec(1, 65);
		add_num_sec(3072-3,3072+3,sec);
		min_start_add = 0;
		max_start_add = 0;

		arr_ep_out = fs_intr_out_ep;
		out_ep_arr_size = FS_INTR_OUT_EP_SIZE;
		arr_ep_in = fs_intr_in_ep;
		in_ep_arr_size = FS_INTR_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
	if(speed == DEV_SPEED_FULL && transfer_type == EPATT_ISO){
		arr_maxp = fs_isoc_maxp;
		maxp_arr_size = FS_ISOC_MAXP_SIZE;
		arr_interval = fs_isoc_interval;
		interval_arr_size = FS_ISOC_INTERVAL_SIZE;
		sec = init_num_sec(1,3);
		add_num_sec(1023-3,1023+3,sec);
		add_num_sec(3072-3,3072+3,sec);
		min_start_add = 0;
		max_start_add = 0;

		arr_ep_out = fs_isoc_out_ep;
		out_ep_arr_size = FS_ISOC_OUT_EP_SIZE;
		arr_ep_in = fs_isoc_in_ep;
		in_ep_arr_size = FS_ISOC_IN_EP_SIZE;
		//burst & mult all default value
		arr_burst = df_burst;
		burst_arr_size = DF_BURST_SIZE;
		arr_mult = df_mult;
		mult_arr_size = DF_MULT_SIZE;
	}
#if 0
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
#endif
	for(ep_in_index = 0; ep_in_index < in_ep_arr_size; ep_in_index++){
		ep_in_num = *(arr_ep_in + ep_in_index);
		for(ep_out_index = 0; ep_out_index < out_ep_arr_size; ep_out_index++){
			ep_out_num = *(arr_ep_out + ep_out_index);
			for(maxp_index = 0; maxp_index < maxp_arr_size; maxp_index++){
				maxp = *(arr_maxp + maxp_index);
				for(interval_index = 0; interval_index < interval_arr_size; interval_index++){
					bInterval = *(arr_interval + interval_index);
					for(mult_index = 0; mult_index < mult_arr_size; mult_index++){
						mult = *(arr_mult+mult_index);
						for(burst_index = 0; burst_index < burst_arr_size; burst_index++){
							burst = *(arr_burst+burst_index);
							/*
							printk(KERN_ERR "[TEST]*************************************\n");
							printk(KERN_ERR "      OUT_EP: %d***************************\n", ep_out_num);
							printk(KERN_ERR "      IN_EP: %d ***************************\n", ep_in_num);
							printk(KERN_ERR "      MAXP: %d  ***************************\n", maxp);
							printk(KERN_ERR "      INTERVAL: %d  ***************************\n", bInterval);
							printk(KERN_ERR "      MULT: %d **************************\n", mult);
							printk(KERN_ERR "      BURST: %d *************************\n", burst);
							*/
							/* ==phase 0 : device reset==*/
#if 1
							if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_ISO && mult > 0 && burst > 1){
								mtk_test_dbg( "             SKIP!!\n");
								continue;
							}
							start_port_reenabled(0, speed);
							ret=dev_reset(speed, NULL);
							if(ret){
								mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
								return ret;
							}

							ret = f_disable_slot();
							if(ret){
								mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
								return ret;
							}

							ret = f_reenable_port(0);
							if(ret != RET_SUCCESS){
								mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
								return ret;
							}

							ret = f_enable_slot(NULL);
							if(ret){
								mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
								return ret;
							}

							ret=f_address_slot(false, NULL);
							if(ret){
								mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
								return ret;
							}
#endif
							//reset SW scheduler algorithm
							mtktest_mtk_xhci_scheduler_init();
							/* ==phase 1 : config EP==*/
							ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
							if(ret)
							{
								mtk_test_dbg( "config dev EP fail!!\n");
								return ret;
							}
							ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
							if(ret)
							{
								mtk_test_dbg( "config dev EP fail!!\n");
								return ret;
							}
							ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
							if(ret)
							{
								mtk_test_dbg( "config EP fail!!\n");
								return ret;
							}

							ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
							if(ret)
							{
								mtk_test_dbg( "config EP fail!!\n");
								return ret;
							}

							for(start_add = min_start_add; start_add <= max_start_add; start_add++){
								cur_sec = sec;
								while(cur_sec != NULL){
        							mtk_test_dbg( "[TEST]*************************************\n");
        							mtk_test_dbg( "      OUT_EP: %d***************************\n", ep_out_num);
        							mtk_test_dbg( "      IN_EP: %d ***************************\n", ep_in_num);
        							mtk_test_dbg( "      MAXP: %d  ***************************\n", maxp);
        							mtk_test_dbg( "      INTERVAL: %d  ***************************\n", bInterval);
        							mtk_test_dbg( "      MULT: %d **************************\n", mult);
        							mtk_test_dbg( "      BURST: %d *************************\n", burst);


									length = cur_sec->current_value;
									mtk_test_dbg( "      [ROUND]**************************\n");
									mtk_test_dbg( "             START_ADD: %d*************\n", start_add);
									mtk_test_dbg( "             LENGTH: %d   *************\n", length);
#if 1
									if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_ISO
										&& (maxp == 2048 || maxp == 3072) && (length%maxp != 0)
										&& (length % 1024 == 0)){
										mtk_test_dbg( "             SKIP!!\n");
										cur_sec = find_next_num(cur_sec);
										continue;
									}
#endif

									/* ==phase 2 : loopback==*/

									#ifdef DMA_BURST_SIZE_TEST
									dma_burst_sz = (dma_burst_sz + 1) % 4;
									dma_tmp = readl(SSUSB_XHCI_HDMA_CFG);
									/* clear dma r/w fields */
									dma_tmp &= ~0x0C0C;
									/* set r/w burst size */
									dma_tmp |= (dma_burst_sz << 2 | dma_burst_sz << 10);
									writel(dma_tmp, SSUSB_XHCI_HDMA_CFG);
									#endif
									mtk_test_dbg( "             DMA_BURST: %d   *************\n", dma_burst_sz);

#if 1
									if(speed == DEV_SPEED_SUPER && transfer_type == EPATT_ISO){
										if(length % maxp == 0){
											cur_sec = find_next_num(cur_sec);
											mtk_test_dbg( "             SKIP!!\n");
											continue;
										}
										else if((burst==0) && (length/maxp>250)){
											cur_sec = find_next_num(cur_sec);
											mtk_test_dbg( "             SKIP!!\n");
											continue;
										}
										else{
											bdp=1;
											gpd_buf_size = length;
											bd_buf_size = 4096;
										}
									}
									else if(speed == DEV_SPEED_HIGH && transfer_type == EPATT_ISO){
										if(length % maxp == 0){
											cur_sec = find_next_num(cur_sec);
											mtk_test_dbg( "             SKIP!!\n");
											continue;
										}
										else if((mult == 0) && (length/maxp>250)){
											cur_sec = find_next_num(cur_sec);
											mtk_test_dbg( "             SKIP!!\n");
											continue;
										}
										else{
											bdp=1;
											gpd_buf_size = length;
											bd_buf_size = 4096;
										}
									}
									else if(transfer_type == EPATT_ISO){
										if(length/maxp > 250){
											cur_sec = find_next_num(cur_sec);
											mtk_test_dbg( "             SKIP!!\n");
											continue;
										}
										else{
											bdp=1;
											gpd_buf_size = length;
											bd_buf_size = 4096;
										}
									}
									else{
										bdp=1;
										gpd_buf_size = length;
										bd_buf_size = 4096;
									}
#endif
									ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,NULL);
									if(ret)
									{
										mtk_test_dbg( "loopback request fail!!\n");
										f_power_suspend();
										return ret;
									}

		//							ret = f_loopback_loop(ep_out_num, ep_in_num, length, start_add,NULL);
									ret = f_loopback_loop_gpd(
										ep_out_num, ep_in_num, length, start_add, gpd_buf_size, NULL);
									if(ret)
									{
										mtk_test_dbg( "loopback fail!!\n");
										mtk_test_dbg( "length : %d\n",length);
										f_power_suspend();
										return ret;
									}

									/* ==phase 3: get device status==*/
									ret=dev_polling_status(NULL);
									if(ret)
									{
										f_power_suspend();
										mtk_test_dbg( "query request fail!!\n");
										return ret;
									}
									cur_sec = find_next_num(cur_sec);
								}
							}
						} //burst
					} //mult
				} //interval
			} //maxp
		} // ep_out
	} //ep_in
	clear_num_secs(sec);
	return ret;
}

static int t_u3auto_loopback(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int mult, burst, dev_mult;
	int dram_offset, extension;

	ret = 0;
	speed = DEV_SPEED_HIGH;;
	transfer_type = EPATT_BULK;
	maxp = 512;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	dev_mult = 1;
	burst = 8;
	mult = 0;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER; //TODO: superspeed
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(argc > 2){
		if(!strcmp(argv[2], "bulk")){
			mtk_test_dbg( "Test bulk transfer\n");
			transfer_type = EPATT_BULK;
		}
		else if(!strcmp(argv[2], "intr")){
			mtk_test_dbg( "Test intr transfer\n");
			transfer_type = EPATT_INT;
		}
		else if(!strcmp(argv[2], "isoc")){
			mtk_test_dbg( "Test isoc transfer\n");
			transfer_type = EPATT_ISO;
		}
	}
	if(argc > 3){
		maxp = (int)simple_strtol(argv[3], &argv[3], 10);
		mtk_test_dbg( "maxp set to %d\n", maxp);
	}
	if(argc > 4){
		bInterval = (int)simple_strtol(argv[4], &argv[4], 10);
		mtk_test_dbg( "interval set to %d\n", bInterval);
	}
	if(argc > 5){
		length = (int)simple_strtol(argv[5], &argv[5], 10);
		mtk_test_dbg( "length set to %d\n", length);
	}
	if(argc > 6){
		start_add = (int)simple_strtol(argv[6], &argv[6], 10);
		mtk_test_dbg( "start add offset set to %d\n", start_add);
	}
	if(argc > 7){
		ep_out_num = (int)simple_strtol(argv[7], &argv[7], 10);
		mtk_test_dbg( "ep out num set to %d\n", ep_out_num);
	}
	if(argc > 8){
		ep_in_num = (int)simple_strtol(argv[8], &argv[8], 10);
		mtk_test_dbg( "ep in num set to %d\n", ep_in_num);
	}
	if(argc > 9){
		burst = (int)simple_strtol(argv[9], &argv[9], 10);
		mtk_test_dbg( "burst set to %d\n", burst);
	}
	if(argc > 10){
		mult = (int)simple_strtol(argv[10], &argv[10], 10);
		mtk_test_dbg( "mult set to %d\n", mult);
	}
#if 0
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
#endif
	mtk_test_dbg( "/*=========loopback===========*/\n");

	/* ==phase 0 : device reset==*/
	start_port_reenabled(0, speed);
	ret=dev_reset(speed,NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}

	/* ==phase 1 : config EP==*/
	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	/* ==phase 2 : loopback==*/
	bdp=1;
    gpd_buf_size=length;
	bd_buf_size=8192;
    //TODO: device should turn off extension length feature
#if 0
	if(((length-10)%(bd_buf_size+6))<7){
		length+=12;
	}
#endif
	ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,NULL);
	if(ret)
	{
		mtk_test_dbg( "loopback request fail!!\n");
		return ret;
	}

	ret = f_loopback_loop_gpd(ep_out_num, ep_in_num, length, start_add, gpd_buf_size, NULL);
	if(ret)
	{
		mtk_test_dbg( "loopback fail!!\n");
		mtk_test_dbg( "length : %d\n",length);
		return ret;
	}

	/* ==phase 3: get device status==*/
	ret=dev_polling_status(NULL);
	if(ret)
	{
		mtk_test_dbg( "query request fail!!\n");
		return ret;
	}
#if 0
	ret=dev_reset(speed,NULL);
#endif
	return ret;
}

static int t_u3auto_loopback_sg(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int sg_len;
	int mult,burst, dev_mult;
	int dram_offset, extension;

	ret = 0;
	speed = DEV_SPEED_HIGH;
	transfer_type = EPATT_BULK;
	maxp = 512;
	ep_out_num = 1;
	ep_in_num = 1;
	bInterval = 0;
	length = 65535;
	mult=0;
	burst=8;
	dev_mult=1;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER;
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(argc > 2){
		if(!strcmp(argv[2], "bulk")){
			mtk_test_dbg( "Test bulk transfer\n");
			transfer_type = EPATT_BULK;
		}
		else if(!strcmp(argv[2], "intr")){
			mtk_test_dbg( "Test intr transfer\n");
			transfer_type = EPATT_INT;
		}
		else if(!strcmp(argv[2], "isoc")){
			mtk_test_dbg( "Test isoc transfer\n");
			transfer_type = EPATT_ISO;
		}
	}
	if(argc > 3){
		maxp = (int)simple_strtol(argv[3], &argv[3], 10);
		mtk_test_dbg( "maxp set to %d\n", maxp);
	}
	if(argc > 4){
		bInterval = (int)simple_strtol(argv[4], &argv[4], 10);
		mtk_test_dbg( "interval set to %d\n", bInterval);
	}
	if(argc > 5){
		length = (int)simple_strtol(argv[5], &argv[5], 10);
		mtk_test_dbg( "length set to %d\n", length);
	}
	if(argc > 6){
		start_add = (int)simple_strtol(argv[6], &argv[6], 10);
		mtk_test_dbg( "start add offset set to %d\n", start_add);
	}
	if(argc > 7){
		sg_len = (int)simple_strtol(argv[7], &argv[7], 10);
		mtk_test_dbg( "sg length set to %d\n", sg_len);
		if(sg_len == 0){
			mtk_test_dbg( "random sg length\n");
		}
	}
	if(argc > 8){
		ep_out_num = (int)simple_strtol(argv[8], &argv[8], 10);
		mtk_test_dbg( "ep out num set to %d\n", ep_out_num);
	}
	if(argc > 9){
		ep_in_num = (int)simple_strtol(argv[9], &argv[9], 10);
		mtk_test_dbg( "ep in num set to %d\n", ep_in_num);
	}
	if(argc > 10){
		burst = (int)simple_strtol(argv[10], &argv[10], 10);
		mtk_test_dbg( "burst set to %d\n", burst);
	}
	if(argc > 11){
		mult = (int)simple_strtol(argv[11], &argv[11], 10);
		mtk_test_dbg( "mult set to %d\n", mult);
	}
#if 0
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
#endif
	mtk_test_dbg( "/*=========loopback===========*/\n");

	/* ==phase 0 : device reset==*/
	start_port_reenabled(0, speed);
	ret=dev_reset(speed,NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}
	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}
	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}

	/* ==phase 1 : config EP==*/
	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult,NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	/* ==phase 2 : loopback==*/
	bdp=1;
    gpd_buf_size=length;
	bd_buf_size=4096;
    //TODO: device should turn off extension length feature
#if 0
	if(((length-10)%(bd_buf_size+6))<7){
		length+=12;
	}
#endif
	ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,NULL);
	if(ret)
	{
		mtk_test_dbg( "loopback request fail!!\n");
		return ret;
	}

	ret = f_loopback_sg_loop(ep_out_num, ep_in_num, length, start_add, sg_len, NULL);
	if(ret)
	{
		mtk_test_dbg( "loopback fail!!\n");
		mtk_test_dbg( "length : %d\n",length);
		return ret;
	}

	/* ==phase 3: get device status==*/
	ret=dev_polling_status(NULL);
	if(ret)
	{
		mtk_test_dbg( "query request fail!!\n");
		return ret;
	}
	return ret;
}

//always use bulk transfer
static int t_u3auto_random_suspend(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int mult, burst, dev_mult;
	int dram_offset, extension;
	int suspend_count, suspend_boundry;
	char isSuspend;

	ret = 0;
	speed = DEV_SPEED_HIGH;;
	transfer_type = EPATT_BULK;
	maxp = 512;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	dev_mult = 1;
	burst = 8;
	mult = 0;
	suspend_count = 0;
	suspend_boundry = 10;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER; //TODO: superspeed
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(argc > 2){
		suspend_boundry = (int)simple_strtol(argv[2], &argv[2], 10);
		mtk_test_dbg( "suspend_boundry set to %d\n", suspend_boundry);
	}
	if(speed == DEV_SPEED_SUPER){
		maxp = 1024;
	}
	else if(speed == DEV_SPEED_HIGH){
		maxp = 512;
	}
	else if(speed == DEV_SPEED_FULL){
		maxp = 64;
	}
	bInterval = 0;
	//random length
	start_add = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	burst = 8;
	mult = 0;
#if 0
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
#endif

	/* ==phase 0 : device reset==*/
	start_port_reenabled(0, speed);
	ret=dev_reset(speed,NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}
	mtktest_mtk_xhci_scheduler_init();
	/* ==phase 1 : config EP==*/
	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}
	suspend_count = 0;
	while(suspend_count < suspend_boundry){
		isSuspend = (char)(get_random_int() % 2);
		if(isSuspend){
			mtk_test_dbg( "Suspend/Resume\n");
			ret = f_power_suspend();
			if(ret){
				return ret;
			}
			mtk_test_dbg( "Enter U3, Suspend success\n");
			mdelay(1000);
			ret = f_power_resume();
			if(ret){
				return ret;
			}
			mtk_test_dbg( "Enter U0, Resume success\n");
			suspend_count++;
		}
		length = (get_random_int() % 65535)+1;
		mtk_test_dbg( "count(%d), isSuspend(%d), loopback %d length\n",
                                suspend_count, isSuspend, length);
		/* ==phase 2 : loopback==*/
		bdp=1;
	    gpd_buf_size=length;
		bd_buf_size=8192;
	    //TODO: device should turn off extension length feature
#if 0
		if(((length-10)%(bd_buf_size+6))<7){
			length+=12;
		}
#endif
		ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,NULL);
		if(ret)
		{
			mtk_test_dbg( "loopback request fail!!\n");
			return ret;
		}

		ret = f_loopback_loop(ep_out_num, ep_in_num, length, start_add,NULL);
		if(ret)
		{
			mtk_test_dbg( "loopback fail!!\n");
			mtk_test_dbg( "length : %d\n",length);
			return ret;
		}

		/* ==phase 3: get device status==*/
		ret=dev_polling_status(NULL);
		if(ret)
		{
			mtk_test_dbg( "query request fail!!\n");
			return ret;
		}

	}
#if 0
	ret=dev_reset(speed,NULL);
#endif
	return ret;
}

static int t_u3auto_random_wakeup(int argc, char**argv){
	int ret,length,start_add, random_delay;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int mult, burst, dev_mult;
	int dram_offset, extension;
	int suspend_count, suspend_boundry;
	char isSuspend;

	ret = 0;
	speed = DEV_SPEED_HIGH;;
	transfer_type = EPATT_BULK;
	maxp = 512;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	dev_mult = 1;
	burst = 8;
	mult = 0;
	suspend_count = 0;
	suspend_boundry = 10;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER;
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(argc > 2){
		suspend_boundry = (int)simple_strtol(argv[2], &argv[2], 10);
		mtk_test_dbg( "wakeup_boundary set to %d\n", suspend_boundry);
	}
	if(speed == DEV_SPEED_SUPER){
		maxp = 1024;
	}
	else if(speed == DEV_SPEED_HIGH){
		maxp = 512;
	}
	else if(speed == DEV_SPEED_FULL){
		maxp = 64;
	}
	bInterval = 0;
	//random length
	start_add = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	burst = 8;
	mult = 0;
#if 0
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
#endif

	/* ==phase 0 : device reset==*/
	start_port_reenabled(0, speed);
	ret=dev_reset(speed,NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}

	/* ==phase 1 : config EP==*/
	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,dev_mult,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}
	mtktest_mtk_xhci_scheduler_init();
	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}
	suspend_count = 0;
	while(suspend_count < suspend_boundry){
		length = (get_random_int() % 65535)+1;
		mtk_test_dbg( "loopback %d length\n", length);
		random_delay = (get_random_int() % 300000)+1;
		bdp=1;
		gpd_buf_size=length;
		bd_buf_size=8192;
		isSuspend = (char)(get_random_int() % 2);
		if(isSuspend){
			mtk_test_dbg( "random_delay(%d)\n", random_delay);
			ret = dev_remotewakeup(random_delay);
			ret = f_power_remotewakeup();
			if(ret){
				return ret;
			}
			suspend_count++;
		}

		/* ==phase 2 : loopback==*/

	    //TODO: device should turn off extension length feature
#if 0
		if(((length-10)%(bd_buf_size+6))<7){
			length+=12;
		}
#endif
		ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,NULL);
		if(ret)
		{
			mtk_test_dbg( "loopback request fail!!\n");
			return ret;
		}

		ret = f_loopback_loop(ep_out_num, ep_in_num, length, start_add,NULL);
		if(ret)
		{
			mtk_test_dbg( "loopback fail!!\n");
			mtk_test_dbg( "length : %d\n",length);
			return ret;
		}

		/* ==phase 3: get device status==*/
		ret=dev_polling_status(NULL);
		if(ret)
		{
			mtk_test_dbg( "query request fail!!\n");
			return ret;
		}

	}
#if 0
	ret=dev_reset(speed,NULL);
#endif
	return ret;
}

static int t_u3auto_reconfig(int argc, char** argv){
	int dev_speed;
	unsigned int rnd_transfer_type, rdn_maxp, rdn_interval;
	unsigned int length;
	int rdn_ep_num;
	int num_ep, round;
	int i;

	round = 2;
	num_ep = 1;
	rdn_ep_num = 1;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			dev_speed = DEV_SPEED_SUPER;
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			dev_speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			dev_speed = DEV_SPEED_FULL;
		}
		else if(!strcmp(argv[1], "stop")){
			mtk_test_dbg( "STOP!!\n");
			g_correct = false;
			return RET_SUCCESS;
		}
	}
	if(argc > 2){
		round = (int)simple_strtol(argv[2], &argv[2], 10);
	}
	if(argc > 3){
		num_ep = (int)simple_strtol(argv[3], &argv[3], 10);
	}

	for(i=0; i<round; i++){
		//random new endpoint
		rdn_ep_num = (get_random_int()%3)+1;
		rnd_transfer_type = (get_random_int()%3)+1;
		if(rnd_transfer_type == EPATT_ISO){
			rdn_interval = get_random_int()%3+1;
			rdn_maxp = (get_random_int()%8+1)*128;
		}
		else if(rnd_transfer_type == EPATT_BULK){
			rdn_interval = 0;
			if(dev_speed == DEV_SPEED_SUPER){
				rdn_maxp = 1024;
			}
			else if(dev_speed == DEV_SPEED_HIGH){
				rdn_maxp = 512;
			}
			else if(dev_speed == DEV_SPEED_FULL){
				rdn_maxp = 64;
			}
		}
		else if(rnd_transfer_type == EPATT_INT){
			rdn_interval = get_random_int()%4+1;
			rdn_maxp = (get_random_int()%8+1)*128;
		}
		//de-configure device endpoint

		//de-configure endpoint
		if(i>0){
			f_deconfig_ep(1, 0, 0, NULL, 0);
		}
		//configure endpoints

		//do loopback for 100 rounds

	}
}

static int t_u3auto_stress(int argc, char** argv){
	int ret, num_ep, i, speed;
	int transfer_type[5];
	int maxp[5],interval[5], burst[5],mult[5];
	char isCompare;
	char isEP0;
	int length;
	isCompare = true;
	isEP0 = false;
	int dev_num;
	int port_num;
	int dev_slot;
	struct usb_device *udev;
	int cur_index;

	num_ep = 2;
	dev_slot = 3;
	ret = 0;
	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER;
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
		else if(!strcmp(argv[1], "stop")){
			mtk_test_dbg( "STOP!!\n");
			g_correct = false;
			return RET_SUCCESS;
		}
	}
	if(argc > 2){
		dev_num = (int)simple_strtol(argv[2], &argv[2], 10);
	}
	if(argc > 3){
		//num of eps
		num_ep = (int)simple_strtol(argv[3], &argv[3], 10);
	}
	if(argc > 4){
		if(!strcmp(argv[4], "false")){
			isCompare = false;
		}
	}
	if(argc > 5){
		if(!strcmp(argv[5], "true")){
			isEP0 = true;
		}
	}
	//arg 6~9
	for(i=6; i<=9; i++){
		if(argc > i){
			cur_index = i-5;
			if(!strcmp(argv[i], "bulk")){
				mtk_test_dbg( "Test bulk transfer for ep %d\n", cur_index);
				transfer_type[cur_index] = EPATT_BULK;
				if(speed == DEV_SPEED_SUPER){
					maxp[cur_index] = 1024;
					burst[cur_index] = 8;
					mult[cur_index] = 0;
					interval[cur_index] = 1;

				}
				else if(speed == DEV_SPEED_HIGH){
					maxp[cur_index] = 512;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					interval[cur_index] = 1;

				}
				else if(speed == DEV_SPEED_FULL){
					maxp[cur_index] = 64;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					interval[cur_index] = 1;

				}
			}
			else if(!strcmp(argv[i], "intr")){
				mtk_test_dbg( "Test intr transfer for ep %d\n", (cur_index));
				transfer_type[cur_index] = EPATT_INT;
				if(speed == DEV_SPEED_SUPER){
					maxp[cur_index] = 1024;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					interval[cur_index] = 1;
				}
				else if(speed == DEV_SPEED_HIGH){
					maxp[cur_index] = 1024;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					if(dev_num >= 5){
						//for LPM test
						interval[cur_index] = 4;
					}
					else{
						interval[cur_index] = 1;
					}
				}
				else if(speed == DEV_SPEED_FULL){
					maxp[cur_index] = 64;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					if(dev_num >= 5){
						//for LPM test
						interval[cur_index] = 3;
					}
					else{
						interval[cur_index] = 1;
					}
				}
			}
			else if(!strcmp(argv[i], "intr_pm")){
				mtk_test_dbg( "Test intr transfer for ep %d\n", (cur_index));
				transfer_type[cur_index] = EPATT_INT;
				if(speed == DEV_SPEED_SUPER){
					maxp[cur_index] = 1024;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					interval[cur_index] = 4;
				}
			}
			else if(!strcmp(argv[i], "intr_pm_u2")){
				mtk_test_dbg( "Test intr transfer for ep %d\n", (cur_index));
				transfer_type[cur_index] = EPATT_INT;
				if(speed == DEV_SPEED_SUPER){
					maxp[cur_index] = 1024;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					interval[cur_index] = 5;
				}
			}
			else if(!strcmp(argv[i], "isoc")){
				mtk_test_dbg( "Test isoc transfer for ep %d\n", (cur_index));
				transfer_type[cur_index] = EPATT_ISO;
				if(speed == DEV_SPEED_SUPER){
					maxp[cur_index] = 1024;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					interval[cur_index] = 4;
				}
				else if(speed == DEV_SPEED_HIGH){
					maxp[cur_index] = 1024;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					if(dev_num >= 5){
						//for LPM test
						interval[cur_index] = 4;
					}
					else{
						interval[cur_index] = 1;
					}
				}
				else if(speed == DEV_SPEED_FULL){
					maxp[cur_index] = 1023;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					if(dev_num >= 5){
						//for LPM test
						interval[cur_index] = 3;
					}
					else{
						interval[cur_index] = 1;
					}

				}
			}
			else if(!strcmp(argv[i], "isoc_pm")){
				mtk_test_dbg( "Test isoc transfer for ep %d\n", (cur_index));
				transfer_type[cur_index] = EPATT_ISO;
				if(speed == DEV_SPEED_SUPER){
					maxp[cur_index] = 1024;
					burst[cur_index] = 0;
					mult[cur_index] = 0;
					interval[cur_index] = 5;
				}
			}
		}
	}
	udev = NULL;
	/* ==phase 0 : device reset==*/
	if(dev_num == 0){
#if 0 //doesn't reset now, should reset to tested speed before execute this test item
		start_port_reenabled(0, speed);
		ret=dev_reset(speed, NULL);
		if(ret){
			mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
			return ret;
		}

		ret = f_disable_slot();
		if(ret){
			mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
			return ret;
		}

		ret = f_reenable_port(0);
		if(ret != RET_SUCCESS){
			mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
			return ret;
		}

		ret = f_enable_slot(NULL);
		if(ret){
			mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
			return ret;
		}

		ret=f_address_slot(false, NULL);
		if(ret){
			mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
			return ret;
		}
		udev = NULL;
#endif
	}
	else if(dev_num < 5){
		udev = dev_list[dev_num-1];
		port_num = udev->portnum;
		f_hub_reset_dev(udev, dev_num, port_num, speed);
		udev = dev_list[dev_num-1];
	}
	/* ==phase 1 : config EP==*/
	for(i=1; i<=num_ep; i++){
		if(transfer_type[i] == EPATT_ISO){
			dev_slot = 3;
		}
		else{
			dev_slot = 1;
		}
		dev_config_ep(i,USB_RX, transfer_type[i], maxp[i], interval[i], dev_slot, burst[i],mult[i], udev);
		dev_config_ep(i,USB_TX, transfer_type[i], maxp[i], interval[i], dev_slot, burst[i],mult[i], udev);

		f_config_ep(i,EPADD_OUT,transfer_type[i],maxp[i],interval[i],burst[i],mult[i],udev,0);
		if(i == num_ep){
			f_config_ep(i, EPADD_IN, transfer_type[i], maxp[i], interval[i],burst[i],mult[i],udev,1);
		}
		else{
			f_config_ep(i, EPADD_IN, transfer_type[i], maxp[i], interval[i],burst[i],mult[i],udev,0);
		}
		if(transfer_type[i] == EPATT_ISO){
			f_ring_enlarge(EPADD_OUT, i, -1);
			f_ring_enlarge(EPADD_OUT, i, -1);
			f_ring_enlarge(EPADD_IN, i, -1);
			f_ring_enlarge(EPADD_IN, i, -1);
		}
	}


	g_correct = true;
	ret=dev_stress(0,GPD_LENGTH_RDN ,GPD_LENGTH_RDN,0,num_ep, udev);
	msleep(2000);
	for(i=1; i<=num_ep; i++){
		f_add_rdn_len_str_threads(dev_num, i, maxp[i], isCompare, udev, isEP0);
	}
	if(ret){
		mtk_test_dbg( "stress request failed!!!!!!!!!!\n");
		return ret;
	}
	return ret;
}

static int t_u3auto_isoc_frame_id(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int mult_dev, mult, burst;
	int dram_offset, extension;
	int i;

	ret = 0;
	speed = DEV_SPEED_HIGH;;
	transfer_type = EPATT_ISO;
	bInterval = 1;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 10241;
	start_add = 0;
	mult_dev = 3;
	mult = 0;
	burst = 0;
	dram_offset = 0;
	extension = 0;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER;
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(speed == DEV_SPEED_SUPER){
		maxp = 1024;
	}
	else if(speed == DEV_SPEED_HIGH){
		maxp = 1024;
	}
	else if(speed == DEV_SPEED_FULL){
		maxp = 1023;
	}


	//start test
	start_port_reenabled(0, speed);
	ret=dev_reset(speed, NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}
	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	//10 round
	g_iso_frame = true;
	bdp=1;
	gpd_buf_size = length;
	bd_buf_size = 4096;
	for(i=0; i<10; i++){
		ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,NULL);
		if(ret)
		{
			mtk_test_dbg( "loopback request fail!!\n");
			f_power_suspend();
			return ret;
		}
		ret = f_loopback_loop_gpd(
			ep_out_num, ep_in_num, length, start_add, gpd_buf_size, NULL);
		if(ret)
		{
			mtk_test_dbg( "loopback fail!!\n");
			mtk_test_dbg( "length : %d\n",length);
			f_power_suspend();
			return ret;
		}
		ret=dev_polling_status(NULL);
		if(ret)
		{
			f_power_suspend();
			mtk_test_dbg( "query request fail!!\n");
			return ret;
		}
	}

	g_iso_frame = false;
	return RET_SUCCESS;
}

static int t_ellysis_TD7_36(int argc, char** argv){
	int ret;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i,j;
	char *tmp;
	struct urb *urb_rx;
	struct usb_host_endpoint *ep_rx;
	int ep_index_rx;
	void *buffer_rx;
	char *buffer;
	ret = 0;

	if(u3auto_hcd_reset() != RET_SUCCESS)
			return RET_FAIL;
	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	xhci_dbg(xhci, "device speed %d\n", udev->speed);
	//get descriptor (device)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(8);
	desc = kmalloc(8, GFP_KERNEL);
	memset(desc, 0, 8);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//get descriptor (device)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//get descriptor (configure)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_CONFIG_SIZE);
	desc = kmalloc(USB_DT_CONFIG_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_CONFIG_SIZE);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//get descriptor (configure)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(40);
	desc = kmalloc(40, GFP_KERNEL);
	memset(desc, 0, 40);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//set configuration
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_CONFIGURATION;
	dr->wValue = cpu_to_le16(1);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);
	//set idle
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	dr->bRequest = 0x0A;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);
	//get descriptor (HID report)
	ret = f_config_ep(1, EPADD_IN, EPATT_INT, 4, 7,0,0, udev, 1);
	//interrupt input
	ep_rx = udev->ep_in[1];
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
	xhci_err(xhci, "[INPUT]\n");
	for(i=0; i<10; i++){
		urb_rx = usb_alloc_urb(0, GFP_KERNEL);
		ret = f_fill_urb(urb_rx,1,4,0,EPADD_IN, 0, 4, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill rx urb Error!!\n");
			return RET_FAIL;
		}
		urb_rx->transfer_flags &= ~URB_ZERO_PACKET;
		ret = f_queue_urb(urb_rx,1,udev);
		if(ret){
			xhci_err(xhci, "[ERROR]rx urb transfer failed!!\n");
			return RET_FAIL;
		}
		dma_sync_single_for_cpu(dev,urb_rx->transfer_dma, 4,DMA_BIDIRECTIONAL);
		for(j=0; j<urb_rx->transfer_buffer_length; j++){
			tmp = urb_rx->transfer_buffer+i;
			//xhci_err(xhci, "%x ", *tmp);
		}
		//xhci_err(xhci, "\n");
		usb_free_urb(urb_rx);
	}

	return ret;
}

static int t_class_keyboard(int argc, char** argv){
	int ret;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	int i;
	char *tmp;
	struct urb *urb_rx;
	struct usb_host_endpoint *ep_rx;
	int ep_index_rx;
	void *buffer_rx;
	char *buffer;
	ret = 0;

	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;

	//set configuration
	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	xhci_dbg(xhci, "device speed %d\n", udev->speed);
	//set configuration
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_CONFIGURATION;
	dr->wValue = cpu_to_le16(1);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);
	//set idle
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = 0x21;
	dr->bRequest = 0x0a;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);
	//set report
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = 0x21;
	dr->bRequest = 0x09;
	dr->wValue = cpu_to_le16(0x200);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(1);
	buffer = kmalloc(1, GFP_KERNEL);
	*buffer = 0x01;
	urb = alloc_ctrl_urb(dr, buffer, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);
	kfree(buffer);

	//config a interrupt IN endpiont, ep_num=1
	ret = f_config_ep(1, EPADD_IN, EPATT_INT, 8, 10,0,0, udev, 1);

	//continuous queue interrupt transfer for 10 times
	ep_rx = udev->ep_in[1];
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
	xhci_err(xhci, "[INPUT]\n");
	for(i=0; i<100; i++){
		urb_rx = usb_alloc_urb(0, GFP_KERNEL);
		ret = f_fill_urb(urb_rx,1,8,0,EPADD_IN, 0, 8, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill rx urb Error!!\n");
			return RET_FAIL;
		}
		urb_rx->transfer_flags &= ~URB_ZERO_PACKET;
		ret = f_queue_urb(urb_rx,1,udev);
		if(ret){
			xhci_err(xhci, "[ERROR]rx urb transfer failed!!\n");
			return RET_FAIL;
		}
		dma_sync_single_for_cpu(dev,urb_rx->transfer_dma, 8,DMA_BIDIRECTIONAL);
		for(i=0; i<urb_rx->transfer_buffer_length; i++){
			tmp = urb_rx->transfer_buffer+i;
			xhci_err(xhci, "%x ", *tmp);
		}
		xhci_err(xhci, "\n");
		usb_free_urb(urb_rx);
	}

	return ret;
}

static int t_u3auto_randomstop_dev(int argc, char** argv){
	int ret;
	int speed, transfer_type_1, transfer_type_2, maxp_1, maxp_2, gpd_buf_size, bdp
		, bd_buf_size, ep_1_num, ep_2_num, dir_1, dir_2, dev_dir_1, dev_dir_2
		, urb_dir_1, urb_dir_2, length;
	int stop_count_1, stop_count_2;

	//static parameters
	int bInterval = 1;
	int mult_dev = 1;
	int burst = 0;

	speed = DEV_SPEED_HIGH;
	transfer_type_1 = transfer_type_2 = EPATT_BULK;
	maxp_1 = maxp_2 = 512;
	gpd_buf_size = 16*1024;
	bd_buf_size = 0;
	ep_1_num = 1;
	dir_1 = EPADD_OUT;
	dev_dir_1 = USB_RX;
	urb_dir_1 = URB_DIR_OUT;

	ep_2_num = 2;
	dir_2 = EPADD_IN;
	dev_dir_2 = USB_TX;
	urb_dir_2 = URB_DIR_IN;

	bInterval =0;
	mult_dev =1;
	burst =4;

	stop_count_1 = stop_count_2 = 3;

	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "Test super speed\n");
			speed = DEV_SPEED_SUPER; //TODO: superspeed
		}
		else if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "Test high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "Test full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	if(argc > 2){
		if(!strcmp(argv[2], "bulk")){
			mtk_test_dbg( "Tx BULK transfer\n");
			transfer_type_1 = EPATT_BULK;
		}
		else if(!strcmp(argv[2], "intr")){
			mtk_test_dbg( "Tx INTERRUPT transfer\n");
			transfer_type_1 = EPATT_INT;
		}
		else if(!strcmp(argv[2], "isoc")){
			mtk_test_dbg( "Tx ISOC transfer\n");
			transfer_type_1 = EPATT_ISO;
		}
	}
	if(argc > 3){
		if(!strcmp(argv[3], "bulk")){
			mtk_test_dbg( "Rx BULK transfer\n");
			transfer_type_2 = EPATT_BULK;
		}
		else if(!strcmp(argv[3], "intr")){
			mtk_test_dbg( "Rx INTERRUPT transfer\n");
			transfer_type_2 = EPATT_INT;
		}
		else if(!strcmp(argv[3], "isoc")){
			mtk_test_dbg( "Rx ISOC transfer\n");
			transfer_type_2 = EPATT_ISO;
		}
	}
	if(argc > 4){
		ep_1_num= (int)simple_strtol(argv[4], &argv[4], 10);
	}
	if(argc > 5){
		ep_2_num= (int)simple_strtol(argv[5], &argv[5], 10);
	}
	if(argc > 6){
		if(!strcmp(argv[6], "OUT")){
			dir_1 = EPADD_OUT;
			dev_dir_1 = USB_RX;
			urb_dir_1 = URB_DIR_OUT;
		}
		else{
			dir_1 = EPADD_IN;
			dev_dir_1 = USB_TX;
			urb_dir_1 = URB_DIR_IN;
		}
	}
	if(argc > 7){
		if(!strcmp(argv[7], "OUT")){
			dir_2 = EPADD_OUT;
			dev_dir_2 = USB_RX;
			urb_dir_2 = URB_DIR_OUT;
		}
		else{
			dir_2 = EPADD_IN;
			dev_dir_2 = USB_TX;
			urb_dir_2 = URB_DIR_IN;
		}
	}
	if(argc > 8){
		maxp_1 = (int)simple_strtol(argv[8], &argv[8], 10);
	}
	if(argc > 9){
		maxp_2 = (int)simple_strtol(argv[9], &argv[9], 10);
	}
	if(argc > 10){
		gpd_buf_size = (int)simple_strtol(argv[10], &argv[10], 10);
	}
	if(argc > 11){
		bd_buf_size = (int)simple_strtol(argv[11], &argv[11], 10);
	}
	if(argc > 12){
		stop_count_1 = (int)simple_strtol(argv[12], &argv[12], 10);
	}
	if(argc > 13){
		stop_count_2 = (int)simple_strtol(argv[13], &argv[13], 10);
	}
	//config EP
	ret=dev_config_ep(ep_1_num,dev_dir_1,transfer_type_1,maxp_1, bInterval,mult_dev,burst,0, NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret=dev_config_ep(ep_2_num,dev_dir_2,transfer_type_2,maxp_2, bInterval,mult_dev,burst,0, NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_1_num,dir_1,transfer_type_1,maxp_1,bInterval,8,0, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_2_num,dir_2,transfer_type_2,maxp_2,bInterval,8,0, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}


	length = gpd_buf_size;

	ret=dev_random_stop(length, gpd_buf_size, bd_buf_size, dev_dir_1, dev_dir_2, stop_count_1, stop_count_2);
	if(ret){
		mtk_test_dbg( "random stop request failed!!!!!!!!!!\n");
		return ret;
	}
	ret = f_random_stop(ep_1_num, ep_2_num, stop_count_1, stop_count_2, urb_dir_1, urb_dir_2, length);


	return ret;
}


static int t_ring_random_ring_doorbell(int argc, char** argv){
	int ep_num, ep_dir, ep_index;
	struct xhci_hcd *xhci;

	ep_dir = EPADD_OUT;
	ep_num = 1;

	if(argc > 1){
		if(!strcmp(argv[1], "out")){
			mtk_test_dbg( "OUT EP\n");
			ep_dir = EPADD_OUT;
		}
		else if(!strcmp(argv[1], "in")){
			mtk_test_dbg( "IN EP\n");
			ep_dir = EPADD_IN;
		}
	}
	if(argc > 2){
		ep_num = (int)simple_strtol(argv[2], &argv[2], 10);
		mtk_test_dbg( "ep num set to %d\n", ep_num);
	}
	if(ep_num == 0){
		ep_index = 0;
	}
	else{
		if(ep_dir == EPADD_OUT){
			ep_index = ep_num * 2 - 1;
		}
		else{
			ep_index = ep_num * 2;
		}
	}
	xhci = hcd_to_xhci(my_hcd);
	f_add_random_ring_doorbell_thread(xhci, g_slot_id, ep_index);
	return RET_SUCCESS;
}

static int t_power_random_access_regs(int argc, char** argv){
	int port_id, port_rev, power_required;
	struct xhci_hcd *xhci;

	if(argc < 4){
		mtk_test_dbg( "arg: port_id port_rev is_power_required\n");
	}
	port_id = (int)simple_strtol(argv[1], &argv[1], 10);
	port_rev = (int)simple_strtol(argv[2], &argv[2], 10);
	if(!strcmp(argv[3], "true")){
		power_required = 1;
	}
	else{
		power_required = 0;
	}
	xhci = hcd_to_xhci(my_hcd);
	f_add_random_access_reg_thread(xhci, port_id, port_rev, power_required);
	return RET_SUCCESS;
}

static int t_ring_random_stop_ep(int argc, char** argv){
	int ep_num, ep_dir, ep_index;
	struct xhci_hcd *xhci;

	ep_dir = EPADD_OUT;
	ep_num = 1;

	if(argc > 1){
		if(!strcmp(argv[1], "out")){
			mtk_test_dbg( "OUT EP\n");
			ep_dir = EPADD_OUT;
		}
		else if(!strcmp(argv[1], "in")){
			mtk_test_dbg( "IN EP\n");
			ep_dir = EPADD_IN;
		}
	}
	if(argc > 2){
		ep_num = (int)simple_strtol(argv[2], &argv[2], 10);
		mtk_test_dbg( "ep num set to %d\n", ep_num);
	}
	if(ep_num == 0){
		ep_index = 0;
	}
	else{
		if(ep_dir == EPADD_OUT){
			ep_index = ep_num * 2 - 1;
		}
		else{
			ep_index = ep_num * 2;
		}
	}
	xhci = hcd_to_xhci(my_hcd);
	g_test_random_stop_ep = true;
	f_add_random_stop_ep_thread(xhci, g_slot_id, ep_index);
	return RET_SUCCESS;
}

static int t_loopback_configep(int argc, char** argv){
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_num, ep_dir;
	char is_config;
	int mult, burst;


	transfer_type = EPATT_BULK;
	ep_dir = EPADD_OUT;
	maxp = 512;
	ep_num = 1;
	bInterval = 0;
	is_config=0;


	if(argc > 1){
		if(!strcmp(argv[1], "bulk")){
			mtk_test_dbg( "Test bulk transfer\n");
			transfer_type = EPATT_BULK;
		}
		else if(!strcmp(argv[1], "intr")){
			mtk_test_dbg( "Test intr transfer\n");
			transfer_type = EPATT_INT;
		}
		else if(!strcmp(argv[1], "isoc")){
			mtk_test_dbg( "Test isoc transfer\n");
			transfer_type = EPATT_ISO;
		}
	}
	if(argc > 2){
		if(!strcmp(argv[2], "out")){
			mtk_test_dbg( "OUT EP\n");
			ep_dir = EPADD_OUT;
		}
		else if(!strcmp(argv[2], "in")){
			mtk_test_dbg( "IN EP\n");
			ep_dir = EPADD_IN;
		}
	}
	if(argc > 3){
		maxp = (int)simple_strtol(argv[3], &argv[3], 10);
		mtk_test_dbg( "maxp set to %d\n", maxp);
	}
	if(argc > 4){
		bInterval = (int)simple_strtol(argv[4], &argv[4], 10);
		mtk_test_dbg( "interval set to %d\n", bInterval);
	}
	if(argc > 5){
		ep_num = (int)simple_strtol(argv[5], &argv[5], 10);
		mtk_test_dbg( "ep num set to %d\n", ep_num);
	}
	if(argc > 6){
		is_config = (int)simple_strtol(argv[6], &argv[6], 10);
		mtk_test_dbg( "is_config set to %d\n", is_config);
	}
	if(argc > 7){
		burst = (int)simple_strtol(argv[7], &argv[7], 10);
		mtk_test_dbg( "burst set to %d\n", burst);
	}
	if(argc > 8){
		mult = (int)simple_strtol(argv[8], &argv[8], 10);
		mtk_test_dbg( "mult set to %d\n", mult);
	}
	return f_config_ep(ep_num, ep_dir, transfer_type, maxp, bInterval,burst,mult, NULL, is_config);
}

static int t_loopback_deconfigep(int argc, char** argv){
	//all or one-by-one
	int ep_num, ep_dir;
	char is_config;
	char is_all;


	ep_dir = EPADD_OUT;
	ep_num = 1;
	is_all = false;
	is_config=0;


	if(argc > 1){
		if(!strcmp(argv[1], "out")){
			mtk_test_dbg( "OUT EP\n");
			ep_dir = EPADD_OUT;
		}
		else if(!strcmp(argv[1], "in")){
			mtk_test_dbg( "IN EP\n");
			ep_dir = EPADD_IN;
		}
		else if(!strcmp(argv[1], "all")){
			mtk_test_dbg( "all EP\n");
			is_all = true;
		}

	}
	if(argc > 2){
		ep_num = (int)simple_strtol(argv[2], &argv[2], 10);
		mtk_test_dbg( "ep num set to %d\n", ep_num);
	}
	if(argc > 3){
		is_config = (int)simple_strtol(argv[3], &argv[3], 10);
		mtk_test_dbg( "is_config set to %d\n", is_config);
	}
	f_deconfig_ep(is_all, ep_num, ep_dir, NULL, is_config);
}

static int t_loopback_loop(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int ep_out_num, ep_in_num;
	int speed;
	int sg_len;
	int round, i;
	int dram_offset, extension;

	ret =0;
	speed = DEV_SPEED_HIGH;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	round=1;
	dram_offset = 0;
	extension = 0;
	sg_len = 0;

	if(argc > 1){
		length = (int)simple_strtol(argv[1], &argv[1], 10);
		mtk_test_dbg( "length set to %d\n", length);
	}
	if(argc > 2){
		start_add = (int)simple_strtol(argv[2], &argv[2], 10);
		mtk_test_dbg( "start add offset set to %d\n", start_add);
	}
	if(argc > 3){
		ep_out_num = (int)simple_strtol(argv[3], &argv[3], 10);
		mtk_test_dbg( "ep out num set to %d\n", ep_out_num);
	}
	if(argc > 4){
		ep_in_num = (int)simple_strtol(argv[4], &argv[4], 10);
		mtk_test_dbg( "ep in num set to %d\n", ep_in_num);
	}

	if(argc > 5){
		round = (int)simple_strtol(argv[5], &argv[5], 10);
		mtk_test_dbg( "Execute %d round\n", round);
	}

	if(argc > 6){
		sg_len = (int)simple_strtol(argv[6], &argv[6], 10);
		mtk_test_dbg( "sg_len set to %d\n", sg_len);
	}

	bdp=1;
    gpd_buf_size=length;
	bd_buf_size=4096;
    //TODO: device should turn off extension length feature
#if 0
	if(((length-10)%(bd_buf_size+6))<7){
		length+=12;
	}
#endif
	for(i=0; i<round; i++){
		if(round==1){}
		else{
			length = (get_random_int() % 65535) + 1;
			start_add = get_random_int() % 64;
			gpd_buf_size = length;
			mtk_test_dbg( "ROUND[%d] length[%d] start_add[%d]\n", i, length, start_add);
		}
		ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,NULL);
		if(ret)
		{
			mtk_test_dbg( "loopback request fail!!\n");
			return ret;
		}
		if(sg_len == 0){
			ret = f_loopback_loop(ep_out_num, ep_in_num, length, start_add,NULL);
		}
		else{
			ret = f_loopback_sg_loop(ep_out_num,ep_in_num,length,start_add,sg_len,NULL);
		}
		if(ret)
		{
			mtk_test_dbg( "loopback fail!!\n");
			mtk_test_dbg( "length : %d\n",length);
			return ret;
		}

		/* ==phase 3: get device status==*/
		ret=dev_polling_status(NULL);
		if(ret)
		{
			mtk_test_dbg( "query request fail!!\n");
			return ret;
		}
	}
	return ret;
}

static int t_power_suspend(int argc, char** argv){
	return f_power_suspend();
}

static int t_power_resume(int argc, char** argv){
	return f_power_resume();
}

static int t_power_suspendport(int argc, char** argv){
	u32 __iomem *addr;
	int temp;
	int port_id;
	struct xhci_hcd *xhci;

	port_id = g_port_id;

	if(argc > 1){
		port_id = (int)simple_strtol(argv[1], &argv[1], 10);
	}

	xhci = hcd_to_xhci(my_hcd);

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	mtk_xhci_handshake(xhci, addr, (15<<5), (3<<5), 30*1000);
	temp = xhci_readl(xhci, addr);
	if(PORT_PLS_VALUE(temp) != 3){
		xhci_err(xhci, "port not enter U3 state\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int t_power_resumeport(int argc, char** argv){
	u32 __iomem *addr;
	int temp;
	int port_id;
	int i;
	struct xhci_hcd *xhci;

	port_id = g_port_id;

	if(argc > 1){
		port_id = (int)simple_strtol(argv[1], &argv[1], 10);
	}

	xhci = hcd_to_xhci(my_hcd);

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	if(PORT_PLS(temp) != (3 << 5)){
		xhci_err(xhci, "port not in U3 state\n");
		return RET_FAIL;
	}

	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));

	if(DEV_SUPERSPEED(temp)){
		//superspeed direct set U0
		temp = (temp | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
	}
	else{
		//HS/FS, set resume for 20ms, then set U0
		temp = (temp | (15 << 5) | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
		mdelay(20);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
	}
	for(i=0; i<200; i++){
		temp = xhci_readl(xhci, addr);
		if(PORT_PLS_VALUE(temp) == 0){
			break;
		}
		msleep(1);

	}
	if(PORT_PLS_VALUE(temp) != 0){
		xhci_err(xhci, "port not return U0 state\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}


static int t_power_remotewakeup(int argc, char** argv){
	return f_power_remotewakeup();
}

static int t_power_u1u2(int argc, char** argv){
	int ret;
	int u_num;
	int value1, value2;

	u_num = 1;
	value1 = 1;
	value2 = 1;
	if(argc > 1){
		u_num = (int)simple_strtol(argv[1], &argv[1], 10);
//		xhci_dbg(xhci, "u_num set to %d\n", u_num);
	}
	if(argc > 2){
		value1 = (int)simple_strtol(argv[2], &argv[2], 10);
//		xhci_dbg(xhci, "value1 set to %d\n", value1);
	}
	if(argc > 3){
		value2 = (int)simple_strtol(argv[3], &argv[3], 10);
//		xhci_dbg(xhci, "value2 set to %d\n", value2);
	}
	if(u_num < 4){
		return f_power_set_u1u2(u_num, value1, value2);
	}
	else{
		if(u_num == 4){
			//reset counter
			f_power_reset_u1u2_counter(1);
			f_power_reset_u1u2_counter(2);
			return RET_SUCCESS;

		}
		else if (u_num == 5){
			//print counter
			mtk_test_dbg( "u1 counter = %d\n", f_power_get_u1u2_counter(1));
			mtk_test_dbg( "u2 counter = %d\n", f_power_get_u1u2_counter(2));
			return RET_SUCCESS;
		}
	}
}

static int t_power_u2_lpm(int argc, char** argv){
	int ret;
	int hle, rwe, hirdm, besl, besld, pdn, int_nak_active, bulk_nyet_active;
	int L1_timeout;
	if(argc > 1){
		hle = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	if(argc > 2){
		rwe = (int)simple_strtol(argv[2], &argv[2], 10);
	}
	if(argc > 3){
		hirdm = (int)simple_strtol(argv[3], &argv[3], 10);
	}
	if(argc > 4){
		L1_timeout = (int)simple_strtol(argv[4], &argv[4], 10);
	}
	if(argc > 5){
		besl = (int)simple_strtol(argv[5], &argv[5], 10);
	}
	if(argc > 6){
		besld = (int)simple_strtol(argv[6], &argv[6], 10);
	}
	if(argc > 7){
		pdn = (int)simple_strtol(argv[7], &argv[7], 10);
	}
	if(argc > 8){
		int_nak_active = (int)simple_strtol(argv[8], &argv[8], 10);
	}
	if(argc > 9){
		bulk_nyet_active = (int)simple_strtol(argv[9], &argv[9], 10);
	}

	//program hle, rwe
	f_power_config_lpm(g_slot_id, hirdm, L1_timeout, rwe, besl, besld, hle, int_nak_active, bulk_nyet_active);

	return RET_SUCCESS;
}

static int t_power_u2_swlpm(int argc, char** argv){
	int ret;
	int expected_L1S;
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;
	char is_wakeup;
	int num_u3_port;

	ret = RET_SUCCESS;
	is_wakeup = false;
	if(argc > 1){
		//0:resume 1:accept, 2:NYET, 3:STALL, 4:timeout, 5:remote_wakeup, 6:reset counter, 7:print counter
		expected_L1S = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	if(expected_L1S == 5){
		expected_L1S = 1;
		is_wakeup = true;

	}
	if(expected_L1S == 6){
		f_power_reset_L1_counter(1);
		f_power_reset_L1_counter(2);
		return RET_SUCCESS;
	}
	if(expected_L1S == 7){
		//print counter
		mtk_test_dbg( "L1 entr counter = %d\n", f_power_get_L1_counter(1));
		mtk_test_dbg( "L1 exit counter = %d\n", f_power_get_L1_counter(2));
		return RET_SUCCESS;
	}
	g_port_plc = 0;
	xhci = hcd_to_xhci(my_hcd);

	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	mtktest_enablePortClockPower((g_port_id-1-num_u3_port), 0x2);

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((g_port_id-1) & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	if(expected_L1S == 0){
		temp = (temp | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
		ret = mtk_xhci_handshake(xhci, addr, (0xf << 5), expected_L1S, ATTACH_TIMEOUT);
		if(ret != 0){
			xhci_err(xhci, "resume failed\n");
			return RET_FAIL;
		}
		else{
			return RET_SUCCESS;
		}
#if 0
		temp = (temp | (0xf << 5) | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
		msleep(20);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
		ret = mtk_xhci_handshake(xhci, addr, (0xf << 5), expected_L1S, ATTACH_TIMEOUT);
		if(ret != 0){
			xhci_err(xhci, "resume failed\n");
			return RET_FAIL;
		}
		else{
			return RET_SUCCESS;
		}
#endif
	}
	else{
        /* if expected_L1S != 1, go into L1 state */
		temp = (temp | (2 << 5) | PORT_LINK_STROBE);
	}
	if(is_wakeup){
		g_port_plc = 0;
	}
	xhci_writel(xhci, temp, addr);
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*((g_port_id-1) & 0xff);
	/* check L1 status of PORTPMSC */
	ret = mtk_xhci_handshake(xhci, addr, 0x7, expected_L1S, ATTACH_TIMEOUT);
	if(ret != 0){
		xhci_err(xhci, "L1S doesn't as expected, expected[%d], actual[%d]\n"
			, expected_L1S, xhci_readl(xhci, addr));
		return RET_FAIL;
	}
	if(expected_L1S == 2 || expected_L1S == 3 || expected_L1S == 4){
		if(g_port_plc == 0){
			xhci_err(xhci, "Doesn't get port PLC event\n");
			return RET_FAIL;
		}
	}
	if(expected_L1S == 1){
		mtktest_disablePortClockPower((g_port_id-1-num_u3_port), 0x2);
		
	}

	if(is_wakeup){
		wait_event_on_timeout(&g_port_plc, 1, 5000);
		if(g_port_plc == 0){
			xhci_err(xhci, "No port state change event\n");
			return RET_FAIL;
		}
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((g_port_id-1) & 0xff);
		ret = mtk_xhci_handshake(xhci, addr, (0xf << 5), 0, ATTACH_TIMEOUT);
		if(ret != 0){
			xhci_err(xhci, "Remote wakeup failed\n");
		}
#if 0
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((g_port_id-1) & 0xff);
		ret = mtk_xhci_handshake(xhci, addr, (0xf << 5), 0, ATTACH_TIMEOUT);
		if(ret != 0){
			xhci_err(xhci, "Remote wakeup failed\n");
		}
#endif
	}
	return ret;
}

static int t_power_fla(int argc, char** argv){
	int ret;
	int fla_value;

	if(argc > 1){
		fla_value = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "fla_value set to %d\n", fla_value);
	}
	return f_power_send_fla(fla_value);
}

static int t_power_occ(int argc, char** argv){
	int ret;
	u32 __iomem *addr;
	int temp;
	struct xhci_hcd *xhci;
	struct xhci_port *port;
	int i, port_id;

	xhci = hcd_to_xhci(my_hcd);
#if 1
	g_port_occ = false;
	g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	g_num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));
	//disable U2 port first
	for(i=1; i<=g_num_u2_port; i++){
		port_id=i+g_num_u3_port;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp &= ~PORT_POWER;
		xhci_writel(xhci, temp, addr);
	}
#endif
	addr = 0xf0008098;
	temp = readl(addr);
	temp |= (1<<16);
	writel(temp, addr);
	msleep(3);
	if(g_port_occ == true){
		ret = RET_SUCCESS;
	}
	else{
		mtk_test_dbg( "[ERROR] doesn't get over-current event\n");
		return RET_FAIL;
	}
	//check if PP=0
	//turn on PP, re-enable device
	//disable slot, port connection, enable...
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	if((temp & PORT_POWER)){
		mtk_test_dbg( "[ERROR] port_power bit is still 1\n");
		return RET_FAIL;
	}
	addr = 0xf0008098;
	temp = readl(addr);
	temp &= ~(1<<16);
	writel(temp, addr);
	f_disable_slot();
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	//start_port_reenabled(0, DEV_SPEED_SUPER);
	port = rh_port[0];
	port->port_status = DISCONNECTED;

	msleep(1000);

	temp = temp | PORT_POWER;
	xhci_writel(xhci, temp, addr);
	if(f_enable_port(0) != RET_SUCCESS){
		mtk_test_dbg( "[ERROR] port not reconnectted after set PP\n");
	}
	ret = f_enable_slot(NULL);
	ret = f_address_slot(false, NULL);

	return ret;
}

static int t_ring_enlarge(int argc, char** argv){
	int ep_dir, ep_num, ep_index, dev_num, slot_id;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep;
	struct xhci_virt_device *virt_dev;
	struct usb_device *udev, *rhdev;
	struct xhci_ring *ep_ring;
	struct xhci_segment	*next, *prev;
	u32 val, cycle_bit;
	int i, ret;

	ep_dir = EPADD_OUT;
	ep_num = 1;
	dev_num = -1;
	ret = 0;

	if(argc > 1){
		if(!strcmp(argv[1], "out")){
			mtk_test_dbg( "OUT EP\n");
			ep_dir = EPADD_OUT;
		}
		else if(!strcmp(argv[1], "in")){
			mtk_test_dbg( "IN EP\n");
			ep_dir = EPADD_IN;
		}
	}
	if(argc > 2){
		ep_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "ep_num set to %d\n", ep_num);
	}
	if(argc > 3){
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}

	xhci = hcd_to_xhci(my_hcd);
	if(dev_num == -1){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
		slot_id = udev->slot_id;
	}
	else{
		udev = dev_list[dev_num-1];
		slot_id = udev->slot_id;
	}
	virt_dev = xhci->devs[udev->slot_id];
	if(ep_dir == EPADD_OUT){
		ep = udev->ep_out[ep_num];
	}
	else{
		ep = udev->ep_in[ep_num];
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	ep_ring = (&(virt_dev->eps[ep_index]))->ring;

	prev = ep_ring->enq_seg;
	next = mtktest_xhci_segment_alloc(xhci, GFP_NOIO);
	next->next = prev->next;
	next->trbs[TRBS_PER_SEGMENT-1].link.segment_ptr = prev->next->dma;
	val = next->trbs[TRBS_PER_SEGMENT-1].link.control;
	val &= ~TRB_TYPE_BITMASK;
	val |= TRB_TYPE(TRB_LINK);
	val |= TRB_CHAIN;
	next->trbs[TRBS_PER_SEGMENT-1].link.control = val;
	xhci_dbg(xhci, "Linking segment 0x%llx to segment 0x%llx (DMA)\n",
			(unsigned long long)prev->dma,
			(unsigned long long)next->dma);
	//adjust cycle bit
	if(ep_ring->cycle_state == 1){
		cycle_bit = 0;
	}
	else{
		cycle_bit = 1;
	}
	for(i=0; i<TRBS_PER_SEGMENT; i++){
		val = next->trbs[i].generic.field[3];
		if(cycle_bit == 1){
			val |= cycle_bit;
		}
		else{
			val &= ~cycle_bit;
		}
		next->trbs[i].generic.field[3] = val;
		xhci_dbg(xhci, "Set new segment trb %d cycle bit 0x%x\n", i, val);
	}
	mtktest_xhci_link_segments(xhci, prev, next, true);
	if(prev->trbs[TRBS_PER_SEGMENT-1].link.control & LINK_TOGGLE){
		val = prev->trbs[TRBS_PER_SEGMENT-1].link.control;
		val &= ~LINK_TOGGLE;
		prev->trbs[TRBS_PER_SEGMENT-1].link.control = val;
		val = next->trbs[TRBS_PER_SEGMENT-1].link.control;
		val |= LINK_TOGGLE;
		next->trbs[TRBS_PER_SEGMENT-1].link.control = val;
	}
	return ret;
}

static int t_ring_shrink(int argc, char**argv){
	int ep_dir, ep_num, ep_index, dev_num, slot_id;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep;
	struct xhci_virt_device *virt_dev;
	struct usb_device *udev, *rhdev;
	struct xhci_ring *ep_ring;
	struct xhci_segment	*next, *prev;
	u32 val, cycle_bit;
	int i, ret;

	ep_dir = EPADD_OUT;
	ep_num = 1;
	dev_num = -1;
	ret = 0;

	if(argc > 1){
		if(!strcmp(argv[1], "out")){
			mtk_test_dbg( "OUT EP\n");
			ep_dir = EPADD_OUT;
		}
		else if(!strcmp(argv[1], "in")){
			mtk_test_dbg( "IN EP\n");
			ep_dir = EPADD_IN;
		}
	}
	if(argc > 2){
		ep_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "ep_num set to %d\n", ep_num);
	}
	if(argc > 3){
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}

	xhci = hcd_to_xhci(my_hcd);
	if(dev_num == -1){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
		slot_id = udev->slot_id;
	}
	else{
		udev = dev_list[dev_num-1];
		slot_id = udev->slot_id;
	}
	virt_dev = xhci->devs[udev->slot_id];
	if(ep_dir == EPADD_OUT){
		ep = udev->ep_out[ep_num];
	}
	else{
		ep = udev->ep_in[ep_num];
	}

	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	ep_ring = (&(virt_dev->eps[ep_index]))->ring;

	prev = ep_ring->enq_seg;
	next = prev->next;
	if(prev == next){
		mtk_test_dbg( "This is the last segment, can not be remove\n");
		return RET_FAIL;
	}
	if(prev->trbs[TRBS_PER_SEGMENT-1].link.control & LINK_TOGGLE){
		ep_ring->first_seg = next->next;
	}
	else if(next->trbs[TRBS_PER_SEGMENT-1].link.control & LINK_TOGGLE){
		val = prev->trbs[TRBS_PER_SEGMENT-1].link.control;
		val |= LINK_TOGGLE;
		prev->trbs[TRBS_PER_SEGMENT-1].link.control = val;
	}
	mtktest_xhci_link_segments(xhci, prev, next->next, true);
	mtktest_xhci_segment_free(xhci, next);
	return RET_SUCCESS;
}

static int t_ring_stop_ep(int argc, char** argv){
	int ep_dir, ep_num, ep_index, dev_num, slot_id;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep;
	struct xhci_virt_device *virt_dev;
	struct usb_device *udev, *rhdev;

	ep_dir = EPADD_OUT;
	ep_num = 1;
	dev_num = -1;

	if(argc > 1){
		if(!strcmp(argv[1], "out")){
			mtk_test_dbg( "OUT EP\n");
			ep_dir = EPADD_OUT;
		}
		else if(!strcmp(argv[1], "in")){
			mtk_test_dbg( "IN EP\n");
			ep_dir = EPADD_IN;
		}
	}
	if(argc > 2){
		ep_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "ep_num set to %d\n", ep_num);
	}
	if(argc > 3){
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}

	xhci = hcd_to_xhci(my_hcd);
	if(dev_num == -1){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
		slot_id = udev->slot_id;
	}
	else{
		udev = dev_list[dev_num-1];
		slot_id = udev->slot_id;
	}
	virt_dev = xhci->devs[udev->slot_id];
	if(ep_dir == EPADD_OUT){
		ep = udev->ep_out[ep_num];
	}
	else{
		ep = udev->ep_in[ep_num];
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	return f_ring_stop_ep(slot_id, ep_index);

}

static int t_ring_stop_cmd(int argc, char** argv){
	return f_ring_stop_cmd();
}

static int t_ring_abort_cmd_add(int argc, char** argv){
	int ret;
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;

	ret = f_enable_port(0);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	ret = f_enable_slot(NULL);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}

	//queue address slot command but not waiting for cmd complete
	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	mtktest_xhci_setup_addressable_virt_dev(xhci, udev);
	virt_dev = xhci->devs[udev->slot_id];
	ret = mtktest_xhci_queue_address_device(xhci, virt_dev->in_ctx->dma, udev->slot_id, false);

	//abort command ring right now
	return f_ring_abort_cmd();
}

static int t_ring_intr_moderation(int argc, char** argv){
	int i;
	int intr_mod_value;
	u32 temp;
	struct xhci_hcd *xhci;

	xhci = hcd_to_xhci(my_hcd);
	intr_mod_value = 0;

	if(argc > 1){
		intr_mod_value = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "intr_mod_value set to %d\n", intr_mod_value);
	}

	temp = xhci_readl(xhci, &xhci->ir_set->irq_control);
	temp &= ~0xFFFF;
	temp |=intr_mod_value;
	xhci_writel(xhci, temp, &xhci->ir_set->irq_control);
	g_intr_handled = 0;
	for(i=0; i<1000; i++){
		mtktest_mtk_xhci_setup_one_noop(xhci);
	}
	msleep(10);
	xhci_err(xhci, "interrupt handler executed %ld times\n", g_intr_handled);
	g_intr_handled = -1;
	return RET_SUCCESS;
}

static int t_ring_er_full(int argc, char** argv){
	int ret;
	struct xhci_hcd *xhci;
	int i;
	u32 temp;
	union xhci_trb *event;

	g_event_full = true;
	if(my_hcd == NULL){
		mtk_test_dbg( "[ERROR]host controller driver not initiated\n");
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);
	i = 1000;
	//turn off interrupt first
	temp = xhci_readl(xhci, &xhci->op_regs->command);
	temp &= (~CMD_EIE);
	xhci_dbg(xhci, "// Disable interrupts, cmd = 0x%x.\n", temp);
	xhci_writel(xhci, temp, &xhci->op_regs->command);
	//continuous queue no-op command
	while(i>0){
		event = xhci->event_ring->dequeue;
		struct xhci_generic_trb *event_trb = &event->generic;
		if((event->event_cmd.flags & TRB_CYCLE) == xhci->event_ring->cycle_state){
			xhci_dbg(xhci, "SW own current event\n");
			if(GET_COMP_CODE(event_trb->field[2]) == COMP_ER_FULL){
				xhci_dbg(xhci, "Got event ring full\n");
				return RET_SUCCESS;
			}
			else{
				xhci_dbg(xhci, "Increase command ring dequeue pointer\n");
				mtktest_inc_deq(xhci, xhci->cmd_ring, false);
			}
			mtktest_inc_deq(xhci, xhci->event_ring, true);
		}
		xhci_dbg(xhci, "Queue No-Op command\n");
		mdelay(50);
		mtktest_mtk_xhci_setup_one_noop(xhci);
		i--;
	}
	return RET_FAIL;
}

static int t_ring_idt(int argc, char** argv){
	//do loopback set IDT
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int mult_dev, mult, burst;

	struct xhci_hcd *xhci;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	u8 *tmp1, *tmp2;
	struct urb *urb_tx, *urb_rx;
	int iso_num_packets;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int max_esit_payload;
	void *buffer_tx, *buffer_rx;
	dma_addr_t mapping_tx, mapping_rx;

	ret = 0;
	speed = DEV_SPEED_SUPER;
	transfer_type = EPATT_BULK;
	maxp = 1024;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 7;
	mult_dev = 3;
	mult = 0;
	burst = 8;
	start_add = 0;
	bdp=1;
	gpd_buf_size = 65535;
	bd_buf_size = 4096;

	start_port_reenabled(0, speed);
	ret=dev_reset(speed, NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}

	/* ==phase 1 : config EP==*/
	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}
	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}
	g_idt_transfer = true;

	ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,0,0,NULL);
	if(ret)
	{
		mtk_test_dbg( "loopback request fail!!\n");
		return ret;
	}

	ret = f_loopback_loop_gpd(ep_out_num, ep_in_num, length, start_add, gpd_buf_size, NULL);
	if(ret){
		mtk_test_dbg( "loopback fail!!\n");
		return ret;
	}

	g_idt_transfer = false;

	return RET_SUCCESS;
}

static int t_ring_bei(int argc, char** argv){
	//do loopback set BEI
	//after tx round should not get URB complete status
	//queue no-op without set BEI
	//get URB complete status
	//do the same to Rx round
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int mult_dev, mult, burst;

	struct xhci_hcd *xhci;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	u8 *tmp1, *tmp2;
	struct urb *urb_tx, *urb_rx;
	int iso_num_packets;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int max_esit_payload;
	void *buffer_tx, *buffer_rx;
	dma_addr_t mapping_tx, mapping_rx;

	ret = 0;
	speed = DEV_SPEED_SUPER;
	transfer_type = EPATT_BULK;
	maxp = 1024;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	mult_dev = 3;
	mult = 0;
	burst = 8;
	start_add = 0;
	bdp=1;
	gpd_buf_size = 65535;
	bd_buf_size = 4096;


	start_port_reenabled(0, speed);
	ret=dev_reset(speed, NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}

	/* ==phase 1 : config EP==*/
	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}
	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}
	g_is_bei = true;

	ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,0,0,NULL);
	if(ret)
	{
		mtk_test_dbg( "loopback request fail!!\n");
		f_power_suspend();
		return ret;
	}
	//do Tx with BEI
	iso_num_packets = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	ep_tx = udev->ep_out[ep_out_num];
	ep_rx = udev->ep_in[ep_in_num];

	ret = 0;
	buffer_tx = kmalloc(length, GFP_KERNEL);
	mapping_tx = dma_map_single(dev, buffer_tx,length, DMA_BIDIRECTIONAL);
	urb_tx = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
	f_fill_urb_with_buffer(urb_tx, ep_out_num, length, buffer_tx
			, start_add, URB_DIR_OUT, iso_num_packets, max_esit_payload, mapping_tx, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb_tx,0,NULL);
	if(ret){
		xhci_err(xhci, "[ERROR]queue tx urb Error!!\n");
		return RET_FAIL;
	}
	msleep(100);
	if(urb_tx->status == 0){
		xhci_err(xhci, "URB_TX status become 0, BEI seems doesn't work\n");
		return RET_FAIL;
	}
	//queue a no-op
	mtktest_mtk_xhci_setup_one_noop(xhci);
	msleep(10);
	if(urb_tx->status != 0){
		xhci_err(xhci, "URB_TX status doesn't become 0 after interrupt\n");
		return RET_FAIL;
	}
	usb_free_urb(urb_tx);

	buffer_rx = kmalloc(length, GFP_KERNEL);
	memset(buffer_rx, 0, length);
	mapping_rx = dma_map_single(dev, buffer_rx,length, DMA_BIDIRECTIONAL);
	urb_rx = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
	f_fill_urb_with_buffer(urb_rx, ep_in_num, length, buffer_rx
			, start_add, URB_DIR_IN, iso_num_packets, max_esit_payload, mapping_rx,udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb_rx,0,NULL);
	if(ret){
		xhci_err(xhci, "[ERROR]queue rx urb Error!!\n");
		return RET_FAIL;
	}
	msleep(100);
	if(urb_rx->status == 0){
		xhci_err(xhci, "URB_RX status become 0, BEI seems doesn't work\n");
		return RET_FAIL;
	}
	//queue a no-op
	mtktest_mtk_xhci_setup_one_noop(xhci);
	msleep(10);
	if(urb_tx->status != 0){
		xhci_err(xhci, "URB_RX status doesn't become 0 after interrupt\n");
		return RET_FAIL;
	}
	urb_rx->transfer_buffer = NULL;
	urb_rx->transfer_dma = NULL;
	usb_free_urb(urb_rx);
	kfree(buffer_tx);
	kfree(buffer_rx);

	g_is_bei = false;

	return RET_SUCCESS;
}

static int t_ring_noop_transfer(int argc, char** argv){
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int transfer_type;
	int maxp;
	int bInterval;
	int ep_out_num, ep_in_num;
	int speed;
	int mult_dev, mult, burst;

	struct xhci_hcd *xhci;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	u8 *tmp1, *tmp2;
	struct urb *urb_tx1, *urb_tx2, *urb_rx1, *urb_rx2;
	int iso_num_packets;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int max_esit_payload;
	void *buffer_tx1, *buffer_tx2, *buffer_rx1, *buffer_rx2;
	dma_addr_t mapping_tx1, mapping_tx2, mapping_rx1, mapping_rx2;

	ret = 0;
	speed = DEV_SPEED_SUPER;
	transfer_type = EPATT_BULK;
	maxp = 1024;
	bInterval = 0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	mult_dev = 3;
	mult = 0;
	burst = 8;
	start_add = 0;
	bdp=1;
	gpd_buf_size = 65535;
	bd_buf_size = 4096;


	start_port_reenabled(0, speed);
	ret=dev_reset(speed, NULL);
	if(ret){
		mtk_test_dbg( "device reset failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_reenable_port(0);
	if(ret != RET_SUCCESS){
		mtk_test_dbg( "device reenable failed!!!!!!!!!!\n");
		return ret;
	}

	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}

	/* ==phase 1 : config EP==*/
	ret=dev_config_ep(ep_out_num,USB_RX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}
	ret=dev_config_ep(ep_in_num,USB_TX,transfer_type,maxp,bInterval,mult_dev,burst,mult,NULL);
	if(ret)
	{
		mtk_test_dbg( "config dev EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_out_num,EPADD_OUT,transfer_type,maxp,bInterval,burst,mult, NULL,0);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret = f_config_ep(ep_in_num,EPADD_IN,transfer_type,maxp,bInterval,burst,mult, NULL,1);
	if(ret)
	{
		mtk_test_dbg( "config EP fail!!\n");
		return ret;
	}

	ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,0,0,NULL);
	if(ret)
	{
		mtk_test_dbg( "loopback request fail!!\n");
		f_power_suspend();
		return ret;
	}

	//queue 2 URB, the first is false, and doesn't ring doorbell

	iso_num_packets = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	ep_tx = udev->ep_out[ep_out_num];
	ep_rx = udev->ep_in[ep_in_num];

	ret = 0;
	//first OUT urb
	buffer_tx1 = kmalloc(1000, GFP_KERNEL);
	mapping_tx1 = dma_map_single(dev, buffer_tx1,1000, DMA_BIDIRECTIONAL);
	urb_tx1 = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
	f_fill_urb_with_buffer(urb_tx1, ep_out_num, 1000, buffer_tx1
			, start_add, URB_DIR_OUT, iso_num_packets, max_esit_payload, mapping_tx1, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}

	g_td_to_noop = true;
	ret = f_queue_urb(urb_tx1,0,NULL);
	if(ret){
		xhci_err(xhci, "[ERROR]queue tx urb Error!!\n");
		return RET_FAIL;
	}
	g_td_to_noop = false;

	//second OUT urb
	buffer_tx2 = kmalloc(length, GFP_KERNEL);
	mapping_tx2 = dma_map_single(dev, buffer_tx2,length, DMA_BIDIRECTIONAL);
	urb_tx2 = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
	f_fill_urb_with_buffer(urb_tx2, ep_out_num, length, buffer_tx2
			, start_add, URB_DIR_OUT, iso_num_packets, max_esit_payload, mapping_tx2, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb_tx2,1,NULL);
	if(ret){
		xhci_err(xhci, "[ERROR]queue tx urb Error!!\n");
		return RET_FAIL;
	}
	urb_tx1->transfer_buffer = NULL;
	urb_tx2->transfer_buffer = NULL;
	urb_tx1->transfer_dma = NULL;
	urb_tx2->transfer_dma = NULL;
	usb_free_urb(urb_tx1);
	usb_free_urb(urb_tx2);

	//first IN urb
	buffer_rx1 = kmalloc(1000, GFP_KERNEL);
	memset(buffer_rx1, 0, 1000);
	mapping_rx1 = dma_map_single(dev, buffer_rx1,1000, DMA_BIDIRECTIONAL);
	urb_rx1 = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
	f_fill_urb_with_buffer(urb_rx1, ep_in_num, 1000, buffer_rx1
			, start_add, URB_DIR_IN, iso_num_packets, max_esit_payload, mapping_rx1,udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}
	g_td_to_noop = true;
	ret = f_queue_urb(urb_rx1,0,NULL);
	if(ret){
		xhci_err(xhci, "[ERROR]queue rx urb Error!!\n");
		return RET_FAIL;
	}
	g_td_to_noop = false;

	//second IN urb
	buffer_rx2 = kmalloc(length, GFP_KERNEL);
	memset(buffer_rx2, 0, length);
	mapping_rx2 = dma_map_single(dev, buffer_rx2,length, DMA_BIDIRECTIONAL);
	urb_rx2 = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
	f_fill_urb_with_buffer(urb_rx2, ep_in_num, length, buffer_rx2
			, start_add, URB_DIR_IN, iso_num_packets, max_esit_payload, mapping_rx2,udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}

	ret = f_queue_urb(urb_rx2,1,NULL);
	if(ret){
		xhci_err(xhci, "[ERROR]queue rx urb Error!!\n");
		return RET_FAIL;
	}

	urb_rx1->transfer_buffer = NULL;
	urb_rx1->transfer_dma = NULL;
	urb_rx2->transfer_buffer = NULL;
	urb_rx2->transfer_dma = NULL;
	usb_free_urb(urb_rx1);
	usb_free_urb(urb_rx2);
	kfree(buffer_tx1);
	kfree(buffer_tx2);
	kfree(buffer_rx1);
	kfree(buffer_rx2);

	return RET_SUCCESS;

}

static int t_hub_selsuspendss(int argc, char** argv){
	int hub_num, port_num;
	int ret;

	hub_num = 1;
	port_num = 1;

	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	return f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_LINK_STATE, port_num | (3<<8));
}

static int t_hub_selsuspend(int argc, char** argv){
	int hub_num, port_num;
	int ret;
	int speed;

	hub_num = 1;
	port_num = 1;
	speed = USB_SPEED_HIGH;

	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	if(argc > 3){
		if(!strcmp(argv[3], "ss")){
			mtk_test_dbg( "SUPERSPEED\n");
			speed = USB_SPEED_SUPER;
		}
	}
	if(speed == USB_SPEED_HIGH){
		return f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_SUSPEND, port_num);
	}
	else if(speed == USB_SPEED_SUPER){
		return f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_LINK_STATE, port_num | (3<<8));
	}
	else{
		mtk_test_dbg( "Error speed value\n");
	}
}


static int t_hub_selresume(int argc, char** argv){

	int hub_num, port_num;
	int ret;
	int speed;

	hub_num = 1;
	port_num = 1;
	speed = USB_SPEED_HIGH;

	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	if(argc > 3){
		if(!strcmp(argv[3], "ss")){
			mtk_test_dbg( "SUPERSPEED\n");
			speed = USB_SPEED_SUPER;
		}
	}
	if(speed == USB_SPEED_HIGH){
		return f_hub_clearportfeature(hub_num, HUB_FEATURE_PORT_SUSPEND, port_num);
	}
	else if(speed == USB_SPEED_SUPER){
		return f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_LINK_STATE, port_num);
	}
	else{
		mtk_test_dbg( "Error speed value\n");
	}
}

static int t_hub_configurehub(int argc, char** argv){
	int ret;
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_command *config_cmd;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_slot_ctx *slot_ctx;
	int i;
	int port_index;
	struct xhci_port *port;
	port_index = 0;

	if(my_hcd == NULL){
		mtk_test_dbg( "my_hcd is NULL\n");
		return RET_FAIL;
	}
	if(argc > 1){
		port_index = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "port_index set to %d\n", port_index);
	}
	xhci = hcd_to_xhci(my_hcd);
	ret = f_enable_port(port_index);
	if(ret != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] enable port failed\n");
		return RET_FAIL;
	}
	port = rh_port[port_index];
	xhci_err(xhci, "Port[%d] speed: %d\n", port_index, port->port_speed);
	ret = f_enable_slot(NULL);
	if(ret != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] enable slot failed\n");
		return RET_FAIL;
	}
	ret = f_address_slot(false, NULL);
	if(ret != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] address device failed\n");
		return RET_FAIL;
	}
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[port->port_id-1];
	virt_dev = xhci->devs[udev->slot_id];
	g_slot_id = 0;
	hdev_list[port_index] = udev;
	f_update_hub_device(udev, 4);
	if(f_hub_configep(port_index+1, port_index) != RET_SUCCESS){
		xhci_err(xhci, "config hub endpoint failed\n");
		return RET_FAIL;
	}
	mtktest_xhci_dbg_slot_ctx(xhci, virt_dev->out_ctx);
	//set port_power
	for(i=1; i<=4; i++){
		if(f_hub_setportfeature((port_index+1), HUB_FEATURE_PORT_POWER, i) != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] set port_power 1 failed\n");
			return RET_FAIL;
		}
	}
	//clear C_PORT_CONNECTION
	for(i=1; i<=4; i++){
		if(f_hub_clearportfeature((port_index+1), HUB_FEATURE_C_PORT_CONNECTION, i) != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] clear c_port_connection failed\n");
		}
	}

	return RET_SUCCESS;
}

static int t_hub_ixia_stress(int argc, char** argv){
	struct xhci_hcd *xhci;
	struct mutex *lock;
	int dev_count;
	int i;

	if(argc > 1){
		dev_count = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "dev_count set to %d\n", dev_count);
	}

	xhci = hcd_to_xhci(my_hcd);
	for(i=0; i<dev_count; i++){
		f_add_ixia_thread(xhci,i+1, ix_dev_list[i]);
	}
	return RET_SUCCESS;
}

static int t_hub_loop_stress(int argc, char** argv){
	struct xhci_hcd *xhci;
	struct mutex *lock;
	int dev_count, ep_count;
	int i,j;
	struct usb_device *udev;
	struct usb_host_endpoint *ep;
	char isEP0;
	int maxp;
	int ret;

	isEP0 = false;

	if(argc > 1){
		dev_count = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "dev_count set to %d\n", dev_count);
	}
	if(argc > 2){
		ep_count = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "ep_count set to %d\n", ep_count);
	}
	if(argc > 3){
		if(!strcmp(argv[3], "true")){
			isEP0 = true;
		}
	}
	g_correct = true;
	for(i=0; i<dev_count; i++){
		udev = dev_list[i];
		ret=dev_stress(0,GPD_LENGTH ,GPD_LENGTH,0,ep_count, udev);
		if(ret){
			mtk_test_dbg( "stress request failed!!!!!!!!!!\n");
			return ret;
		}
	}
	for(i=0; i<dev_count; i++){
		udev = dev_list[i];
		for(j=1; j<=ep_count; j++){
			ep = udev->ep_out[j];
			maxp = ep->desc.wMaxPacketSize & 0x7ff;
			f_add_str_threads(i,j,maxp,true, udev, isEP0);
		}
	}
	return RET_SUCCESS;
}

static int t_hub_loop(int argc, char** argv){
	struct usb_device *udev;
	int ret,length,start_add;
	char bdp;
	int gpd_buf_size,bd_buf_size;
	int ep_out_num, ep_in_num;
	int speed;
	int sg_len;
	int hub_num, port_num, dev_num, round, i;
	int dram_offset, extension;

	ret =0;
	ep_out_num = 1;
	ep_in_num = 1;
	length = 65535;
	round=1;
	dram_offset = 0;
	extension = 0;

	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	if(argc > 3){	//device number
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}
	if(argc > 4){
		length = (int)simple_strtol(argv[4], &argv[4], 10);
		mtk_test_dbg( "length set to %d\n", length);
	}
	if(argc > 5){
		ep_out_num = (int)simple_strtol(argv[5], &argv[5], 10);
		mtk_test_dbg( "ep out num set to %d\n", ep_out_num);
	}
	if(argc > 6){
		ep_in_num = (int)simple_strtol(argv[6], &argv[6], 10);
		mtk_test_dbg( "ep in num set to %d\n", ep_in_num);
	}
	if(argc > 7){
		round = (int)simple_strtol(argv[7], &argv[7], 10);
		mtk_test_dbg( "Execute %d round\n", round);
	}
	if(argc > 8){
		sg_len = (int)simple_strtol(argv[8], &argv[8], 10);
		mtk_test_dbg( "sg_len %d\n", sg_len);
	}

	bdp=0;
    gpd_buf_size=0xFC00;
	bd_buf_size=0x1FFF;
	udev = dev_list[dev_num-1];
    //TODO: device should turn off extension length feature
#if 0
	if(((length-10)%(bd_buf_size+6))<7){
		length+=12;
	}
#endif

	if(ep_out_num == 0 || ep_in_num == 0){
		if(round==1){
			ret = dev_ctrl_loopback(length, udev);
			if(ret){
				mtk_test_dbg( "Control loopback failed!!\n");
				return ret;
			}
			return RET_SUCCESS;
		}
		else{
			for(i=0; i<round; i++){
				length = get_random_int() % 2048;
				length = length - (length %4);
				if(length == 0)
					length = 2048;
				mtk_test_dbg( "Loopback control length[%d]\n", length);
				ret = dev_ctrl_loopback(length, udev);
				if(ret){
					mtk_test_dbg( "Control loopback failed!!\n");
					return ret;
				}
			}
			return RET_SUCCESS;
		}
	}
	if(round == 1){
		ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,udev);
		if(ret)
		{
			mtk_test_dbg( "loopback request fail!!\n");
			return ret;
		}
		if(sg_len == 0){
			ret = f_loopback_loop(ep_out_num, ep_in_num, length, start_add,udev);
		}
		else{
			ret = f_loopback_sg_loop(ep_out_num,ep_in_num, length, start_add
				, sg_len, udev);
		}
		ret=dev_polling_status(udev);
		if(ret)
		{
			mtk_test_dbg( "query request fail!!\n");
			return ret;
		}
		return RET_SUCCESS;
	}
	for(i=0; i<round; i++){
		length = (get_random_int() % 3072/*65535 for bulk, 3072 for intr*/) + 1;
		start_add = (get_random_int() % 63) + 1;
		sg_len = (get_random_int() % 9) * 8/*512 for bulk, 8 for intr*/;
		mtk_test_dbg( "Loopback length[%d] start_add[%d] sg_len[%d]\n", length, start_add, sg_len);
		if((sg_len != 0) && (length/sg_len) > 61){
			i--;
			mtk_test_dbg( "SKIP\n");
			continue;
		}
		ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size,dram_offset,extension,udev);
		if(ret)
		{
			mtk_test_dbg( "loopback request fail!!\n");
			return ret;
		}
		if(sg_len == 0){
			ret = f_loopback_loop(ep_out_num, ep_in_num, length, start_add,udev);
		}
		else{
			ret = f_loopback_sg_loop(ep_out_num,ep_in_num, length, start_add
				, sg_len, udev);
		}
		if(ret)
		{
			mtk_test_dbg( "loopback fail!!\n");
			mtk_test_dbg( "length : %d\n",length);
			return ret;
		}

		/* ==phase 3: get device status==*/
		ret=dev_polling_status(udev);
		if(ret)
		{
			mtk_test_dbg( "query request fail!!\n");
			return ret;
		}
	}
	return RET_SUCCESS;
}

static int t_hub_configuresubhub(int argc, char** argv){
	int parent_hub_num, hub_num, port_num;
	struct xhci_hcd *xhci;
	xhci = hcd_to_xhci(my_hcd);
	parent_hub_num = 1;
	hub_num = parent_hub_num+1;
//	hdev = hdev_list[parent_hub_num-1];
	if(argc > 1){
		parent_hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "parent_hub_num set to %d\n", parent_hub_num);
	}
	if(argc > 2){
		hub_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 3){
		port_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	return f_hub_config_subhub(parent_hub_num, hub_num, port_num);
}

static int t_hub_configure_eth_device(int argc, char** argv){
	int hub_num, port_num, dev_num;
	int ret;

	ret = 0;

	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	if(argc > 3){	//device number
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}

	ret = f_hub_configuredevice(hub_num, port_num, dev_num
		, 0, 0, 0, false, false, 0);
	if(ret)
	{
		mtk_test_dbg( "Config device failed\n");
		return ret;
	}
	ret = f_hub_configure_eth_device(hub_num, port_num, dev_num);
	if(ret)
	{
		mtk_test_dbg( "Config eth device failed\n");
		return ret;
	}
	return ret;
}

static int t_hub_configuredevice(int argc, char** argv){
	int hub_num, port_num, dev_num;
	int transfer_type, bInterval, maxp;
	char is_config_ep, is_stress;
	int stress_config;
	int ret;

	hub_num = 1;
	port_num = 1;
	dev_num = 1;
	transfer_type = EPATT_BULK;
	bInterval = 0;
	maxp = 512;
	is_config_ep = true;
	is_stress = false;

	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	if(argc > 3){	//device number
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}
	if(argc > 4){	//transfer type
		if(!strcmp(argv[4], "bulk")){
			transfer_type = EPATT_BULK;
			xhci_dbg(xhci, "transfer type set to BULK\n");
		}
		else if(!strcmp(argv[4], "intr")){
			xhci_dbg(xhci, "transfer type set to INTR\n");
			transfer_type = EPATT_INT;
		}
		else if(!strcmp(argv[4], "iso")){
			xhci_dbg(xhci, "transfer type set to ISO\n");
			transfer_type = EPATT_ISO;
		}
		else if(!strcmp(argv[4], "stress1")){
			xhci_dbg(xhci, "transfer type set to STRESS1 BULK+INT\n");
			is_stress = true;
			stress_config = 1;
			is_config_ep = false;
		}
		else if(!strcmp(argv[4], "stress2")){
			xhci_dbg(xhci, "transfer type set to STRESS2 BULK_ISO\n");
			is_stress = true;
			stress_config = 2;
			is_config_ep = false;
		}
	}
	else{
		is_config_ep = false;
	}
	if(argc > 5){	//maxp
		maxp = (int)simple_strtol(argv[5], &argv[5], 10);
		xhci_dbg(xhci, "maxp set to %d\n", maxp);
	}
	if(argc > 6){	//interval
		bInterval= (int)simple_strtol(argv[6], &argv[6], 10);
		xhci_dbg(xhci, "bInterval set to %d\n", bInterval);
	}

	return f_hub_configuredevice(hub_num, port_num, dev_num
		, transfer_type, maxp, bInterval, is_config_ep, is_stress, stress_config);
}

static int t_hub_reset_dev(int argc, char** argv){
	struct xhci_hcd *xhci;
	int hub_num, port_num, dev_num;
	int transfer_type, bInterval, maxp;
	struct usb_device *udev;
	USB_DEV_SPEED speed;
	int ret;

	hub_num = 1;
	port_num = 1;
	dev_num = 1;
	ret = 0;
	xhci = hcd_to_xhci(my_hcd);
	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	if(argc > 3){	//device number
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}
	if(argc > 4){
		if(!strcmp(argv[4], "hs")){
			mtk_test_dbg( "[DEV]Reset device to high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[4], "fs")){
			mtk_test_dbg( "[DEV]Reset device to full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}

	udev = dev_list[dev_num-1];
	g_slot_id = udev->slot_id;
	ret = dev_reset(speed,udev);
	if(f_hub_clearportfeature(hub_num, HUB_FEATURE_PORT_POWER, dev_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] clear port_power %d failed\n", dev_num);
		return RET_FAIL;
	}
	mdelay(500);
	if(f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_POWER, dev_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] set port_power %d failed\n", dev_num);
		return RET_FAIL;
	}
	if(f_hub_clearportfeature(hub_num, HUB_FEATURE_C_PORT_CONNECTION, dev_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] clear c_port_connection failed\n");
	}
	f_disable_slot();
	kfree(udev);
	dev_list[dev_num-1] = NULL;
	return RET_SUCCESS;
}

static int t_hub_queue_intr(int argc, char** argv){
	u32 status;
	int ret;
	struct usb_device *hdev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_host_endpoint *ep;
	int ep_index, data_length;

	xhci = hcd_to_xhci(my_hcd);
	hdev = hdev_list[0];
	ep = hdev->ep_in[1];
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	urb = usb_alloc_urb(0, GFP_KERNEL);
	data_length = 2;

	ret = f_fill_urb(urb,1,data_length,0,URB_DIR_IN, 0, 2, hdev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb,0,hdev);
	if(ret){
		xhci_err(xhci, "[ERROR]urb transfer failed!!\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int t_hub_remotewakeup_dev(int argc, char** argv){
	int hub_num, port_num, dev_num;
	int transfer_type, bInterval, maxp;
	struct usb_device *udev;
	USB_DEV_SPEED speed;
	int ret;
	int length;
	int bdp, gpd_buf_size, bd_buf_size;

	bdp=0;
    gpd_buf_size=0xFC00;
	bd_buf_size=0x1FFF;
	length=513;
	hub_num = 1;
	port_num = 1;
	dev_num = 1;
	ret = 0;

	if(argc > 1){	//hub number
		hub_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "hub_num set to %d\n", hub_num);
	}
	if(argc > 2){	//port number
		port_num = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}
	if(argc > 3){	//device number
		dev_num = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "dev_num set to %d\n", dev_num);
	}
	if(argc > 4){
		length = (int)simple_strtol(argv[4], &argv[4], 10);
		mtk_test_dbg( "length set to %d\n", length);
	}

	udev = dev_list[dev_num-1];
	g_slot_id = udev->slot_id;
	return dev_remotewakeup(0);
}

#define HUB_FEATURE_PORT_U1_TIMEOUT	23
#define HUB_FEATURE_PORT_U2_TIMEOUT 24

static int t_hub_set_u1u2(int argc, char** argv){
	int u_num, value1, value2, value;
	int feature_selector;
	int port_num;

	u_num = 1;
	value1 = 1;
	value2 = 1;
	value = 1;
	port_num = 1;


	if(argc > 1){
		u_num = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "u_num set to %d\n", u_num);
	}
	if(argc > 2){
		value1 = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "value1 set to %d\n", value1);
	}
	if(argc > 3){
		value2 = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "value2 set to %d\n", value2);
	}
	if(argc > 4){
		port_num = (int)simple_strtol(argv[4], &argv[4], 10);
		xhci_dbg(xhci, "port_num set to %d\n", port_num);
	}

	if(u_num == 1){
		feature_selector = HUB_FEATURE_PORT_U1_TIMEOUT;
		value = value1;
	}
	else if(u_num == 2){
		feature_selector = HUB_FEATURE_PORT_U2_TIMEOUT;
		value = value2;
	}

	f_hub_setportfeature(1, feature_selector, (port_num | (value<<8)));
	return RET_SUCCESS;
}

#define HUB_FEATURE_FORCE_LINKPM_ACCEPT	30
static int t_hub_force_pm(int argc, char** argv){
	f_hub_sethubfeature(1, HUB_FEATURE_FORCE_LINKPM_ACCEPT);
	return RET_SUCCESS;
}

static int t_dev_notification(int argc, char** argv){
	int ret;
	int type;
	int type_value;
	int value;

	ret = 0;
	type = 1;
	value = 1;

	if(!g_port_reset){
		mtk_test_dbg( "[ERROR] device not reset\n");
		return RET_FAIL;
	}
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot not enabled\n");
		return RET_FAIL;
	}
	if(argc > 1){
		//[DEV_NOTIF_FUNC_WAKE: 1]; [DEV_NOTIF_LTM: 2];
		//[DEV_NOTIF_BUS_INT_ADJ: 3];[VENDOR_DEV_TEST: 4]
		type = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	if(argv > 2){
		value = (int)simple_strtol(argv[2], &argv[2], 10);
	}
	f_enable_dev_note();
	g_dev_notification = 256;
	g_dev_not_value = 0;
//	type_value = type+1;
	//TODO: device notification interface has been modified
	ret = dev_notifiaction(type,value & 0xffffffff,(value >> 32) & 0xffffffff);
	if(ret)
	{
		mtk_test_dbg( "set device notification failed!!\n");
		return ret;
	}
	ret=dev_polling_status(NULL);
	if(ret)
	{
		mtk_test_dbg( "query request fail!!\n");
		return ret;
	}
	ret = wait_event_on_timeout(&g_dev_notification, type,CMD_TIMEOUT);
	if(ret){
		mtk_test_dbg( "device notification event not received!!\n");
		return ret;
	}
	if(g_dev_not_value != value){
		mtk_test_dbg( "device notification value error, expected %d, recived %ld!!\n", value, g_dev_not_value);
		return RET_FAIL;
	}
	return ret;
}

static int t_dev_init(int argc, char** argv){
	if(u3auto_hcd_reset() != RET_SUCCESS)
		return RET_FAIL;
}

static int t_dev_u1u2(int argc, char** argv){
	int ret;
	int value1, value2;
	char en_u1, en_u2;
	int dev_num;
	int mode;
	struct usb_device *udev;

	value1 = 0;
	value2 = 0;
	en_u1 = 1;
	en_u2 = 1;
	udev = NULL;
	mode = 0;

	if(argc > 1){
		value1 = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "u1 value set to %d\n", value1);
	}
	if(argc > 2){
		value2 = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "u2 value set to %d\n", value2);
	}
	if(argc > 3){
		en_u1 = (int)simple_strtol(argv[3], &argv[3], 10);
		xhci_dbg(xhci, "en_u1 set to %d\n", en_u1);
	}
	if(argc > 4){
		en_u2 = (int)simple_strtol(argv[4], &argv[4], 10);
		xhci_dbg(xhci, "en_u2 set to %d\n", en_u2);
	}
	if(argc > 5){
		dev_num = (int)simple_strtol(argv[5], &argv[5], 10);
		udev = dev_list[dev_num-1];
	}
	if(argc > 6){
		mode = (int)simple_strtol(argv[6], &argv[6], 10);
	}
	return dev_power(mode, value1, value2, en_u1, en_u2, udev);
}

static int t_dev_lpm(int argc, char** argv){
	int ret;
	int lpm_mode, wakeup, beslck, beslck_u3, beslckd, cond, cond_en;

	lpm_mode = 0;
	wakeup = 0;
	beslck = 0;
	beslck_u3 = 0;
	beslckd = 0;
	cond = 0;
	cond_en = 0;

	if(argc > 1){
		lpm_mode = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	if(argc > 2){
		wakeup = (int)simple_strtol(argv[2], &argv[2], 10);
	}
	if(argc > 3){
		beslck = (int)simple_strtol(argv[3], &argv[3], 10);
	}
	if(argc > 4){
		beslck_u3 = (int)simple_strtol(argv[4], &argv[4], 10);
	}
	if(argc > 5){
		beslckd = (int)simple_strtol(argv[5], &argv[5], 10);
	}
	if(argc > 6){
		cond = (int)simple_strtol(argv[6], &argv[6], 10);
	}
	if(argc > 7){
		cond_en = (int)simple_strtol(argv[7], &argv[7], 10);
	}

	dev_lpm_config_host(lpm_mode, wakeup, beslck, beslck_u3, beslckd, cond, cond_en);
}

static int t_dev_reset(int argc, char** argv){
	USB_DEV_SPEED speed;
	int ret;

	ret = 0;
	if(!g_port_reset){
		mtk_test_dbg( "[ERROR] device not reset\n");
		return RET_FAIL;
	}
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot not enabled\n");
		return RET_FAIL;
	}
	speed = DEV_SPEED_HIGH;
	if(argc > 1){
		if(!strcmp(argv[1], "ss")){
			mtk_test_dbg( "[DEV]Reset device to super speed\n");
			speed = DEV_SPEED_SUPER;
		}
		if(!strcmp(argv[1], "hs")){
			mtk_test_dbg( "[DEV]Reset device to high speed\n");
			speed = DEV_SPEED_HIGH;
		}
		else if(!strcmp(argv[1], "fs")){
			mtk_test_dbg( "[DEV]Reset device to full speed\n");
			speed = DEV_SPEED_FULL;
		}
	}
	start_port_reenabled(0, speed);
	ret = dev_reset(speed,NULL);
	if(ret){
		mtk_test_dbg( "[ERROR] reset device failed\n");
		return RET_FAIL;
	}
	ret = f_disable_slot();
	if(ret){
		mtk_test_dbg( "disable slot failed!!!!!!!!!!\n");
		return ret;
	}
	ret = f_reenable_port(0);
	if(ret){
		mtk_test_dbg( "[ERROR] port reenable failed\n");
		return RET_FAIL;
	}
	ret = f_enable_slot(NULL);
	if(ret){
		mtk_test_dbg( "enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		mtk_test_dbg( "address slot failed!!!!!!!!!!\n");
		return ret;
	}
	//reset SW scheduler algorithm
	mtktest_mtk_xhci_scheduler_init();
	return ret;
}

static int t_dev_query_status(int argc, char** argv){
	if(!g_port_reset){
		mtk_test_dbg( "[ERROR] device not reset\n");
		return RET_FAIL;
	}
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot not enabled\n");
		return RET_FAIL;
	}
	return dev_query_status(NULL);
}

static int t_dev_polling_status(int argc, char** argv){
	if(!g_port_reset){
		mtk_test_dbg( "[ERROR] device not reset\n");
		return RET_FAIL;
	}
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot not enabled\n");
		return RET_FAIL;
	}
	return dev_polling_status(NULL);
}

static int t_dev_config_ep(int argc, char** argv){
	char ep_num;
	char dir;
	char type;
	short int maxp;
	char bInterval;
	int mult_dev, burst, mult;

	if(!g_port_reset){
		mtk_test_dbg( "[ERROR] device not reset\n");
		return RET_FAIL;
	}
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot not enabled\n");
		return RET_FAIL;
	}
	//ep_out_num,USB_RX,transfer_type,maxp,bInterval
	ep_num = 1;
	dir= USB_TX;
	type=EPATT_BULK;
	maxp=512;
	bInterval=0;
	mult_dev = 3;
	burst = 8;
	mult = 0;

	if(argc > 1){
		ep_num = (int)simple_strtol(argv[1], &argv[1], 10);
		mtk_test_dbg( "ep_num set to %d\n", ep_num);
	}
	if(argc > 2){
		if(!strcmp(argv[2], "tx")){
			mtk_test_dbg( "TX endpoint\n");
			dir = USB_TX;
		}
		else if(!strcmp(argv[2], "rx")){
			mtk_test_dbg( "RX endpoint\n");
			dir = USB_RX;
		}
	}
	if(argc > 3){
		if(!strcmp(argv[3], "bulk")){
			mtk_test_dbg( "Test bulk transfer\n");
			type = EPATT_BULK;
		}
		else if(!strcmp(argv[3], "intr")){
			mtk_test_dbg( "Test intr transfer\n");
			type = EPATT_INT;
		}
		else if(!strcmp(argv[3], "isoc")){
			mtk_test_dbg( "Test isoc transfer\n");
			type = EPATT_ISO;
		}
	}
	if(argc > 4){
		maxp = (int)simple_strtol(argv[4], &argv[4], 10);
		mtk_test_dbg( "maxp set to %d\n", maxp);
	}
	if(argc > 5){
		bInterval = (int)simple_strtol(argv[5], &argv[5], 10);
		mtk_test_dbg( "bInterval set to %d\n", bInterval);
	}
	if(argc > 6){
		burst = (int)simple_strtol(argv[6], &argv[6], 10);
		mtk_test_dbg( "burst set to %d\n", burst);
	}
	if(argc > 7){
		mult = (int)simple_strtol(argv[7], &argv[7], 10);
		mtk_test_dbg( "mult set to %d\n", mult);
	}
	return dev_config_ep(ep_num, dir, type, maxp, bInterval,mult_dev,burst,mult,NULL);
}

static int t_dev_remotewakeup(int argc, char** argv){
	int delay_us;
	int bdp, gpd_buf_size, bd_buf_size;

	bdp=0;
    gpd_buf_size=0xFC00;
	bd_buf_size=0x1FFF;

	if(!g_port_reset){
		mtk_test_dbg( "[ERROR] device not reset\n");
		return RET_FAIL;
	}
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot not enabled\n");
		return RET_FAIL;
	}
	if(argc > 1){
		delay_us = (int)simple_strtol(argv[1], &argv[1], 10);
		mtk_test_dbg( "Device delay %d us\n", delay_us);
	}
	return dev_remotewakeup(delay_us);
}

//Test concurrently resume case
static int t_u3auto_concurrent_remotewakeup(int argc, char** argv){
        int ret,loop,length,j,i;
        char bdp;
        short gpd_buf_size,bd_buf_size;
        int speed,host_speed;
        int maxp, count,mult;
        uint rand_num;
        uint dev_delay_us, host_delay_ms;

        struct xhci_hcd *xhci;
        u32 __iomem *addr;
        int temp;
        int delay_time;

        xhci = hcd_to_xhci(my_hcd);
        addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);

        ret = 0;

        speed = DEV_SPEED_HIGH;
        count = 100;
		delay_time = 40;
        if(argc > 1){
                if(!strcmp(argv[1], "ss")){
                        mtk_test_dbg( "Test super speed\n");
                        speed = DEV_SPEED_SUPER;
                }
                else if(!strcmp(argv[1], "hs")){
                        mtk_test_dbg( "Test high speed\n");
                        speed = DEV_SPEED_HIGH;
                }
                else if(!strcmp(argv[1], "fs")){
                        mtk_test_dbg( "Test full speed\n");
                        speed = DEV_SPEED_FULL;
                }
        }

        if(argc > 2){
                count = (int)simple_strtol(argv[2], &argv[2], 10);
        }
		if(argc > 3){
                delay_time = (int)simple_strtol(argv[3], &argv[3], 10);
        }

		maxp = 1024;
		g_concurrent_resume = true;
        /* ==phase 1 : config EP==*/

#if 0
        ret=dev_config_ep(2,USB_RX,EPATT_BULK,maxp,0,3,0,0,NULL);
        if(ret)
        {
                mtk_test_dbg( "config dev EP fail!!\n");
                return ret;
        }

        ret=dev_config_ep(2,USB_TX,EPATT_BULK,maxp,0,3,0,0,NULL);
        if(ret)
        {
                mtk_test_dbg( "config dev EP fail!!\n");
                return ret;
        }

        ret = f_config_ep(2,EPADD_OUT,EPATT_BULK,maxp,0,0 ,0,NULL,0);
        if(ret)
        {
                mtk_test_dbg( "config EP fail!!\n");
                return ret;
        }

        ret = f_config_ep(2,EPADD_IN,EPATT_BULK,maxp,0,0 ,0,NULL,1);
        if(ret)
        {
                mtk_test_dbg( "config EP fail!!\n");
                return ret;
        }
#endif

        for(i=0;i<count;i++)
        {
                #if 0
                get_random_bytes(&rand_num , sizeof(rand_num));
                host_delay_ms = //suspend 30~60 ms to make sure the host side check timing saving
                get_random_bytes(&rand_num , sizeof(rand_num));
                dev_delay_ms = 30 + rand_num % 30; //suspend 30~60 ms to make sure the host side check timing saving
                #else
                dev_delay_us = 0;
                #endif

                mtk_test_dbg( "count: %d, host_delay: %d, dev_delay: %d\n", i, host_delay_ms, dev_delay_us);

                ret = dev_remotewakeup(dev_delay_us);
                if(ret)
                {
                        mtk_test_dbg( "remote wakeup request fail!!\n");
						g_concurrent_resume = false;
                        return ret;
                }

                ret = f_power_suspend();
                if(ret)
                {
                        mtk_test_dbg( "suspend fail!!!\n");
                }

                #if 0 //original remote wakeup test logic
                ret = f_power_remotewakeup();
                if(ret)
                {
                        mtk_test_dbg( "remote wakeup fail!!\n");
                        return ret;
                }
                #else //enhanced remote wakeup test logic
				addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
				mdelay(10);
				udelay(delay_time);
				//delay(i);

                ret=f_power_resume(0);
                if(ret)
                {
                        mtk_test_dbg( "wakeup fail!!\n");
						g_concurrent_resume = false;
                        return ret;
                }
                #endif

                mdelay(500);
#if 0
                /* ==phase 2 : loopback== */
                bdp=0;
                gpd_buf_size=0xFC00;
                bd_buf_size=0x1000;
                length=8*1024;


                ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size, 0, 0,NULL);
                if(ret)
                {
                        mtk_test_dbg( "loopback request fail!!\n");
                        mtk_test_dbg( "length : %d\n",length);
                        return ret;
                }

                ret = f_loopback_loop(2,2,length, 0,NULL);
                if(ret)
                {
                        mtk_test_dbg( "loopback fail!!\n");
                        mtk_test_dbg( "length : %d\n",length);
                        return ret;
                }

                ret=dev_polling_status(NULL);
                if(ret)
                {
                        mtk_test_dbg( "query request fail!!\n");
                        return ret;
                }

                mdelay(10);

                mtk_test_dbg( "length : %d\n",length);
#endif
        }
        g_concurrent_resume = false;
        return ret;
}

static int t_u3auto_concurrent_u1u2_exit(int argc, char** argv){
	int ret,loop,length,j,i;
    char bdp;
    short gpd_buf_size,bd_buf_size;
    int speed,host_speed;
    int maxp, count,mult;
    uint rand_num;
    struct xhci_hcd *xhci;
    u32 __iomem *addr;
    int temp;
    int dev_delay_time, host_delay_time, do_ux;
	int port_id;

	struct usb_ctrlrequest *dr;
	struct usb_device *udev, *rhdev;
	struct urb *urb;
	struct protocol_query *query;

    xhci = hcd_to_xhci(my_hcd);
    addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);

    ret = 0;

    speed = DEV_SPEED_SUPER;
    count = 100;
	port_id = 1;

    if(argc > 1){
        count = (int)simple_strtol(argv[1], &argv[1], 10);
    }
	if(argc > 2){
        dev_delay_time = (int)simple_strtol(argv[2], &argv[2], 10);
    }
	if(argc > 3){
		host_delay_time = (int)simple_strtol(argv[3], &argv[3], 10);
	}
	if(argc > 4){
		do_ux = (int)simple_strtol(argv[4], &argv[4], 10);
	}
	if(argc > 5){
		port_id = (int)simple_strtol(argv[5], &argv[5], 10);
	}
	for(i=0; i<count; i++){
		ret = dev_remotewakeup(dev_delay_time);
		if(ret)
		{
			mtk_test_dbg( "remote wakeup request fail!!\n");
			return ret;
		}
		mdelay(100);
		ret = f_port_set_pls(port_id, do_ux);
		if(ret){
			continue;
		}
		udelay(host_delay_time);
		ret = f_port_set_pls(1, 0);
	}
	//dev_power(0, 0, 0, 1, 1, NULL);
	//f_port_set_pls(1, 0);

	return RET_SUCCESS;
}

static int t_u3auto_concurrent_u1u2_enter(int argc, char** argv){
	int ret,loop,length,j,i;
    char bdp;
    short gpd_buf_size,bd_buf_size;
    int speed,host_speed;
    int maxp, count,mult;
    uint rand_num;
    uint dev_delay_ms, host_delay_ms;
    uint do_ux;
    struct xhci_hcd *xhci;
    u32 __iomem *addr;
    int temp;
    int delay_time, dev_u1_delay, dev_u2_delay;

	struct usb_ctrlrequest *dr;
	struct usb_device *udev, *rhdev;
	struct urb *urb;
	struct protocol_query *query;

    xhci = hcd_to_xhci(my_hcd);
    addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);

    ret = 0;

    speed = DEV_SPEED_SUPER;
    count = 100;
	delay_time = 40;

	dev_u1_delay = 0;
	dev_u2_delay = 0;

    if(argc > 1){
            count = (int)simple_strtol(argv[1], &argv[1], 10);
    }
	if(argc > 2){
            delay_time = (int)simple_strtol(argv[2], &argv[2], 10);
    }
	if(argc > 3){
            dev_u1_delay= (int)simple_strtol(argv[3], &argv[3], 10);
    }
	if(argc > 4){
            dev_u2_delay= (int)simple_strtol(argv[4], &argv[4], 10);
    }
	if(argc > 5){
		do_ux = (int)simple_strtol(argv[5], &argv[5], 10);
	}

	maxp = 1024;
#if 0
	/* ==phase 1 : config EP==*/
    ret=dev_config_ep(2,USB_RX,EPATT_BULK,maxp,0,3,0,0,NULL);
    if(ret)
    {
            mtk_test_dbg( "config dev EP fail!!\n");
            return ret;
    }

    ret=dev_config_ep(2,USB_TX,EPATT_BULK,maxp,0,3,0,0,NULL);
    if(ret)
    {
            mtk_test_dbg( "config dev EP fail!!\n");
            return ret;
    }

    ret = f_config_ep(2,EPADD_OUT,EPATT_BULK,maxp,0,0 ,0,NULL,0);
    if(ret)
    {
            mtk_test_dbg( "config EP fail!!\n");
            return ret;
    }

    ret = f_config_ep(2,EPADD_IN,EPATT_BULK,maxp,0,0 ,0,NULL,1);
    if(ret)
    {
            mtk_test_dbg( "config EP fail!!\n");
            return ret;
    }
#endif
#if 1
	//set device u1u2 timeout
	if(dev_u1_delay > 0){
		dev_power(1, dev_u1_delay, 0, 1, 1, NULL);
		//do_ux = 2;
	}
	else if(dev_u2_delay > 0){
		dev_power(2, 0, dev_u2_delay, 1, 1, NULL);
		//do_ux = 1;
	}
	else{
		mtk_test_dbg( "[ERROR] Doesn't set device u1 or u2 timeout value\n");
		return RET_FAIL;
	}
#endif
	//host side only set accept u1u2
	f_power_set_u1u2(3, 255, 255);
	msleep(5);
#if 0
	g_con_is_enter = true;
	g_con_delay_us = delay_time;
	g_con_enter_ux = do_ux;
#endif
	for(i=0; i<count; i++){
		//get_random_bytes(&rand_num , sizeof(rand_num));

		/* ==phase 1 : do some transfer to back to U0== */
		//dev_query_status(NULL);
#if 1
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];


		dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
		query= kmalloc(AT_CMD_ACK_DATA_LENGTH, GFP_NOIO);

		memset(query, 0, AT_CMD_ACK_DATA_LENGTH);

		dr->bRequestType = USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
		dr->bRequest = AT_CMD_ACK;
		dr->wValue = cpu_to_le16(0);
		dr->wIndex = cpu_to_le16(0);
		dr->wLength = cpu_to_le16(AT_CMD_ACK_DATA_LENGTH);
		urb = alloc_ctrl_urb(dr, query, udev);
		ret = f_ctrlrequest_nowait(urb, udev);

		//**************************************************/
		//wait_not_event_on_timeout(&(urb->status), -EINPROGRESS, TRANS_TIMEOUT);
#if 1
		mdelay(3);
		udelay(delay_time);
		/* ==phase 2 : transfer U1 or U2 packet== */
		//mtk_test_dbg( "[DBG] set port pls = %d\n", do_ux);
		f_port_set_pls(1, do_ux);
		udelay(100);
		if(do_ux == 3){
			f_port_set_pls(1, 0);
		}
		//mtk_test_dbg( "[DBG] set port pls = 0\n");
		//f_port_set_pls(1, 0);

		wait_not_event_on_timeout(&(urb->status), -EINPROGRESS, TRANS_TIMEOUT);
		mtktest_xhci_urb_free_priv(xhci, urb->hcpriv);
#endif
#endif
		//* clear some trash left						 */
		kfree(dr);
		kfree(query);
	//	kfree(urb);
		usb_free_urb(urb);
		//************************************************/
		dev_query_status(NULL);
	}
	dev_power(0, 0, 0, 1, 1, NULL);
	//f_port_set_pls(1, 0);

	return RET_SUCCESS;
}

static int t_start_host_test_mode(int argc, char** argv){
	int td_no;

	printk(KERN_ERR "[OTG_H] call t_start_host_test_mode\n");

	td_no = 0;  // td_no = 6 ~ 12 (0x6 ~ 0xc)
	if(argc > 1){
		td_no = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	f_host_test_mode(td_no) ;

	return RET_SUCCESS;
}	

static int t_stop_host_test_mode(int argc, char** argv){
	int td_no;

	td_no = 0;
	if(argc > 1){
		td_no = (int)simple_strtol(argv[1], &argv[1], 10);
	}

	f_host_test_mode_stop() ;
	
	return RET_SUCCESS;
}


static int t_drv_vbus(int argc, char** argv){
	 f_drv_vbus() ;
	return RET_SUCCESS;
}

static int t_stop_vbus(int argc, char** argv){
	 f_stop_vbus() ;
	return RET_SUCCESS;
}

//initial code attached to A-connector
static int t_u3h_otg_dev_A_host(int argc, char** argv){
	u32 temp;
	g_otg_test = true;
	g_otg_dev_B = false;
	//enable OTG interrupt
	temp = readl(SSUSB_OTG_INT_EN);
	temp = temp | SSUSB_ATTACH_A_ROLE_INT_EN | SSUSB_CHG_A_ROLE_A_INT_EN
		| SSUSB_CHG_B_ROLE_A_INT_EN | SSUSB_SRP_REQ_INTR_EN;
        writel(temp, SSUSB_OTG_INT_EN);
#if 0
	//disable SIFSLV CLK gating
	temp = readl(SSUSB_CSR_CK_CTRL);
	temp = temp & ~SSUSB_SIFSLV_MCU_BUS_CK_GATE_EN;
	writel(temp, SSUSB_CSR_CK_CTRL);
#endif
	kthread_run(otg_dev_A_host_thread, NULL, "otg_A_host");
	return RET_SUCCESS;
}

//initial code attached to B-connector
static int t_u3h_otg_dev_B_host(int argc, char** argv){
	u32 temp;
	g_otg_test = true;
	g_otg_dev_B = true;
	//enable OTG interrupt
	temp = readl(SSUSB_OTG_INT_EN);
	temp = temp | SSUSB_ATTACH_A_ROLE_INT_EN | SSUSB_CHG_A_ROLE_A_INT_EN
		| SSUSB_CHG_B_ROLE_A_INT_EN | SSUSB_SRP_REQ_INTR_EN;
        writel(temp, SSUSB_OTG_INT_EN);
#if 0
	//disable SIFSLV CLK gating
	temp = readl(SSUSB_CSR_CK_CTRL);
	temp = temp & ~SSUSB_SIFSLV_MCU_BUS_CK_GATE_EN;
	writel(temp, SSUSB_CSR_CK_CTRL);
#endif
	if(f_test_lib_init() != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H] controller driver init failed\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

//B device wait for hnp and become host
static int t_u3h_otg_dev_B_hnp(int argc, char** argv){
	g_otg_hnp_become_host = false;
	kthread_run(otg_dev_B_hnp, NULL, "otg_dev_B_hnp");
	return RET_SUCCESS;
}

//A device wait hnp and become device
static int t_u3h_otg_dev_A_hnp(int argc, char** argv){
	g_otg_hnp_become_dev = false;
	kthread_run(otg_dev_A_hnp, NULL, "otg_dev_A_hnp");
	return RET_SUCCESS;
}

//A device back to host after hnp
static int t_u3h_otg_dev_A_hnp_back(int argc, char** argv){
	g_otg_hnp_become_host = false;
	kthread_run(otg_dev_A_hnp_back, NULL, "otg_dev_A_hnp_back");
	return RET_SUCCESS;
}

//B device back to host after hnp
static int t_u3h_otg_dev_B_hnp_back(int argc, char** argv){
	g_otg_hnp_become_dev = false;
	kthread_run(otg_dev_B_hnp_back, NULL, "otg_dev_B_hnp_back");
	return RET_SUCCESS;
}

//A device srp
static int t_u3h_otg_dev_A_srp(int argc, char** argv){
	kthread_run(otg_dev_A_srp, NULL, "dev_A_srp");
	return RET_SUCCESS;
}

//OTG OPT
static int t_u3h_otg_uut_A(int argc, char** argv){
	kthread_run(otg_opt_uut_a, NULL, "[OTG_H]opt_uut_A");
	return RET_SUCCESS;
}

static int t_u3h_otg_uut_B(int argc, char** argv){
	kthread_run(otg_opt_uut_b, NULL, "[OTG_H]opt_uut_B");
	return RET_SUCCESS;
}

static int t_u3h_otg_pet_uutA(int argc, char** argv){
	kthread_run(otg_pet_uut_a, NULL, "[OTG_H]pet_uut_A");
	return RET_SUCCESS;
}

static int t_u3h_otg_pet_uutB(int argc, char** argv){
	kthread_run(otg_pet_uut_b, NULL, "[OTG_H]pet_uut_B");
	return RET_SUCCESS;
}

#define CTX_SIZE(_hcc) (HCC_64BYTE_CONTEXT(_hcc) ? 64 : 32)
static struct xhci_container_ctx *mtk_xhci_alloc_container_ctx(struct xhci_hcd *xhci,
						    int type, gfp_t flags)
{
	struct xhci_container_ctx *ctx = kzalloc(sizeof(*ctx), flags);
	if (!ctx)
		return NULL;

	BUG_ON((type != XHCI_CTX_TYPE_DEVICE) && (type != XHCI_CTX_TYPE_INPUT));
	ctx->type = type;
	ctx->size = HCC_64BYTE_CONTEXT(xhci->hcc_params) ? 2048 : 1024;
	if (type == XHCI_CTX_TYPE_INPUT)
		ctx->size += CTX_SIZE(xhci->hcc_params);

	ctx->bytes = dma_pool_alloc(xhci->device_pool, flags, &ctx->dma);
	memset(ctx->bytes, 0, ctx->size);
	return ctx;
}

static int dbg_reg_ewe(int argc, char** argv){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;
	int ret;

	xhci = hcd_to_xhci(my_hcd);
	g_mfindex_event = 0;
	temp = xhci_readl(xhci, &xhci->op_regs->command);
	temp |= CMD_EWE;
	xhci_writel(xhci, temp, &xhci->op_regs->command);
	msleep(3000);
	if(g_mfindex_event > 0){
		ret = RET_SUCCESS;
	}
	else{
		ret = RET_FAIL;
	}
	temp = xhci_readl(xhci, &xhci->op_regs->command);
	temp &= ~CMD_EWE;
	xhci_writel(xhci, temp, &xhci->op_regs->command);
	return ret;
}

static int ct_check_hcd(){
	if(!my_hcd){
		if(f_test_lib_init()){
			mtk_test_dbg( "init host controller FAILED\n");
			return RET_FAIL;
		}
	}
	return RET_SUCCESS;
}

static int u3ct_lecroy7061(){
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	mtk_test_dbg( "STEP 1: enable port and slot, address device(BSR=1)...\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u3ct_lecroy7062(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;

	mtk_test_dbg( "STEP 2: get_descriptor, whether success or failed...\n");
	ret = 0;
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);

	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	if(urb->status == -EINPROGRESS){
		//timeout, stop endpoint, set TR dequeue pointer
		f_ring_stop_ep(g_slot_id, 0);
		f_ring_set_tr_dequeue_pointer(g_slot_id, 0, urb);
	}
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);

	return ret;
}

static int u3ct_lecroy718(){
	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	mtk_test_dbg( "wait 1 sec\n");
	msleep(1000);

	mtk_test_dbg( "enable port, slot, address (BSR=1)\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}

	mtk_test_dbg( "set u1 timeout value=127\n");
	if(f_power_set_u1u2(3, 127, 0)){
		mtk_test_dbg( "set u1 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_lecroy719(){
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	mtk_test_dbg( "direct set u1 timeout value=0, u2 timeout value=127\n");
	if(f_power_set_u1u2(3, 0, 127)){
		mtk_test_dbg( "set u1/u2 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_lecroy720(){
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	mtk_test_dbg( "STEP 1: enable port and slot, address device(BSR=1)...\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}
	mtk_test_dbg( "set u1 timeout value=127\n");
	if(f_power_set_u1u2(3, 127, 0)){
		mtk_test_dbg( "set u1 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_lecroy721(){
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	mtk_test_dbg( "STEP 1: enable port and slot, address device(BSR=1)...\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}
	mtk_test_dbg( "set u1 timeout value=127\n");
	if(f_power_set_u1u2(3, 127, 0)){
		mtk_test_dbg( "set u1 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_lecroy729(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	char isWarmReset;
	struct xhci_hcd *xhci;

	mtk_test_dbg( "hot reset port...\n");
	port_id = g_port_id;

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_RESET);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int u3ct_lecroy731(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	char isWarmReset;
	struct xhci_hcd *xhci;

	mtk_test_dbg( "hot reset port...\n");
	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_RESET);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int u3ct_lecroy7341(){
	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u3ct_lecroy7342(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	char isWarmReset;
	struct xhci_hcd *xhci;

	mtk_test_dbg( "warm reset port\n");
	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_WR);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int u3ct_lecroy735(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	struct xhci_hcd *xhci;

	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	mtk_test_dbg( "suspend port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	mtk_xhci_handshake(xhci, addr, (15<<5), (3<<5), 30*1000);
	temp = xhci_readl(xhci, addr);
	if(PORT_PLS_VALUE(temp) != 3){
		xhci_err(xhci, "port not enter U3 state\n");
		return RET_FAIL;
	}

	mtk_test_dbg( "delay 1 sec\n");
	msleep(1000);
	mtk_test_dbg( "warm reset port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_WR);
	xhci_writel(xhci, temp, addr);
	return RET_FAIL;
}

static int u3ct_lecroy7361(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	struct xhci_hcd *xhci;

	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	mtk_test_dbg( "suspend port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	mtk_xhci_handshake(xhci, addr, (15<<5), (3<<5), 30*1000);
	temp = xhci_readl(xhci, addr);
	if(PORT_PLS_VALUE(temp) != 3){
		xhci_err(xhci, "port not enter U3 state\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u3ct_lecroy7362(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	struct xhci_hcd *xhci;
	int i;

	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	mtk_test_dbg( "resume port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	for(i=0; i<200; i++){
		temp = xhci_readl(xhci, addr);
		if(PORT_PLS_VALUE(temp) == 0){
			break;
		}
		msleep(1);

	}
	if(PORT_PLS_VALUE(temp) != 0){
		xhci_err(xhci, "port not return U0 state\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}


static int dbg_u3ct_lecroy(int argc, char** argv){
	int td_no;

	td_no = 0;
	if(argc > 1){
		td_no = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	switch(td_no){
		case 706:
			mtk_test_dbg( "(7061) STEP 1, (7062) STEP 2\n");
			break;
		case 7061:
			return u3ct_lecroy7061();
			break;
		case 7062:
			return u3ct_lecroy7062();
			break;
		case 718:
			return u3ct_lecroy718();
			break;
		case 719:
			return u3ct_lecroy719();
			break;
		case 720:
			return u3ct_lecroy720();
			break;
		case 721:
			return u3ct_lecroy721();
			break;
		case 729:
			return u3ct_lecroy729();
			break;
		case 731:
			return u3ct_lecroy731();
			break;
		case 734:
			mtk_test_dbg( "(7341) STEP 1, (7342) STEP 2\n");
			break;
		case 7341:
			return u3ct_lecroy7341();
			break;
		case 7342:
			return u3ct_lecroy7342();
			break;
		case 735:
			return u3ct_lecroy735();
			break;
		case 736:
			mtk_test_dbg( "(7361) STEP 1, (7362) STEP 2\n");
			break;
		case 7361:
			return u3ct_lecroy7361();
			break;
		case 7362:
			return u3ct_lecroy7362();
			break;
		default:
			mtk_test_dbg( "enter td number\n");
			break;
	}
}

static int u3ct_ellisys7061(){
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	mtk_test_dbg( "STEP 1: enable port and slot, address device(BSR=1)...\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u3ct_ellisys7062(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;

	mtk_test_dbg( "STEP 2: get_descriptor, whether success or failed...\n");
	ret = 0;
	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);

	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	if(urb->status == -EINPROGRESS){
		//timeout, stop endpoint, set TR dequeue pointer
		f_ring_stop_ep(g_slot_id, 0);
		f_ring_set_tr_dequeue_pointer(g_slot_id, 0, urb);
	}
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);

	return ret;
}

static int u3ct_ellisys718(){
	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	mtk_test_dbg( "wait 1 sec\n");
	msleep(1000);

	mtk_test_dbg( "enable port, slot, address (BSR=1)\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}

	mtk_test_dbg( "set u1 timeout value=127\n");
	if(f_power_set_u1u2(3, 127, 0)){
		mtk_test_dbg( "set u1 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_ellisys719(){
	mtk_test_dbg( "direct set u1 timeout value=0, u2 timeout value=127\n");
	if(f_power_set_u1u2(3, 0, 127)){
		mtk_test_dbg( "set u1/u2 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_ellisys720(){
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	mtk_test_dbg( "STEP 1: enable port and slot, address device(BSR=1)...\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}
	mtk_test_dbg( "set u1 timeout value=127\n");
	if(f_power_set_u1u2(3, 127, 0)){
		mtk_test_dbg( "set u1 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_ellisys721(){
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	mtk_test_dbg( "STEP 1: enable port and slot, address device(BSR=1)...\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(true, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}
	mtk_test_dbg( "set u1 timeout value=127\n");
	if(f_power_set_u1u2(3, 127, 0)){
		mtk_test_dbg( "set u1 timeout FAILED!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

static int u3ct_ellisys729(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	char isWarmReset;
	struct xhci_hcd *xhci;

	mtk_test_dbg( "hot reset port...\n");
	port_id = g_port_id;

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_RESET);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int u3ct_ellisys731(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	char isWarmReset;
	struct xhci_hcd *xhci;

	mtk_test_dbg( "hot reset port...\n");
	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_RESET);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int u3ct_ellisys7341(){
	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u3ct_ellisys7342(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	char isWarmReset;
	struct xhci_hcd *xhci;

	mtk_test_dbg( "warm reset port\n");
	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_WR);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int u3ct_ellisys735(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	struct xhci_hcd *xhci;

	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	mtk_test_dbg( "suspend port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	mtk_xhci_handshake(xhci, addr, (15<<5), (3<<5), 30*1000);
	temp = xhci_readl(xhci, addr);
	if(PORT_PLS_VALUE(temp) != 3){
		xhci_err(xhci, "port not enter U3 state\n");
		return RET_FAIL;
	}

	mtk_test_dbg( "delay 1 sec\n");
	msleep(1000);
	mtk_test_dbg( "warm reset port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp | PORT_WR);
	xhci_writel(xhci, temp, addr);
	return RET_FAIL;
}

static int u3ct_ellisys7361(){
	int ret;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i,j;
	char *tmp;
	struct urb *urb_rx;
	struct usb_host_endpoint *ep_rx;
	int ep_index_rx;
	void *buffer_rx;
	char *buffer;
	ret = 0;

	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(u3auto_hcd_reset() != RET_SUCCESS){
		mtk_test_dbg( "init host controller, enable slot, address dev FAILED\n");
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	xhci_dbg(xhci, "device speed %d\n", udev->speed);
	//get descriptor (device)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(8);
	desc = kmalloc(8, GFP_KERNEL);
	memset(desc, 0, 8);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//get descriptor (device)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//get descriptor (configure)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_CONFIG_SIZE);
	desc = kmalloc(USB_DT_CONFIG_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_CONFIG_SIZE);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//get descriptor (configure)
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(40);
	desc = kmalloc(40, GFP_KERNEL);
	memset(desc, 0, 40);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	//set configuration
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_CONFIGURATION;
	dr->wValue = cpu_to_le16(1);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);
	//set idle
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	dr->bRequest = 0x0A;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);
	//get descriptor (HID report)
	ret = f_config_ep(1, EPADD_IN, EPATT_INT, 4, 7,0,0, udev, 1);
	//interrupt input
	ep_rx = udev->ep_in[1];
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
	xhci_err(xhci, "[INPUT]\n");
	for(i=0; i<10; i++){
		urb_rx = usb_alloc_urb(0, GFP_KERNEL);
		ret = f_fill_urb(urb_rx,1,4,0,EPADD_IN, 0, 4, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill rx urb Error!!\n");
			return RET_FAIL;
		}
		urb_rx->transfer_flags &= ~URB_ZERO_PACKET;
		ret = f_queue_urb(urb_rx,1,udev);
		if(ret){
			xhci_err(xhci, "[ERROR]rx urb transfer failed!!\n");
			return RET_FAIL;
		}
		dma_sync_single_for_cpu(dev,urb_rx->transfer_dma, 4,DMA_BIDIRECTIONAL);
		for(j=0; j<urb_rx->transfer_buffer_length; j++){
			tmp = urb_rx->transfer_buffer+i;
			//xhci_err(xhci, "%x ", *tmp);
		}
		//xhci_err(xhci, "\n");
		usb_free_urb(urb_rx);
	}

	return ret;
}

static int u3ct_ellisys7362(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	struct xhci_hcd *xhci;

	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	mtk_test_dbg( "suspend port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	mtk_xhci_handshake(xhci, addr, (15<<5), (3<<5), 30*1000);
	temp = xhci_readl(xhci, addr);
	if(PORT_PLS_VALUE(temp) != 3){
		xhci_err(xhci, "port not enter U3 state\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u3ct_ellisys7363(){
	u32 __iomem *addr;
	int temp;
	int port_id;
	struct xhci_hcd *xhci;
	int i;

	port_id = g_port_id;
	xhci = hcd_to_xhci(my_hcd);

	mtk_test_dbg( "resume port\n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	for(i=0; i<200; i++){
		temp = xhci_readl(xhci, addr);
		if(PORT_PLS_VALUE(temp) == 0){
			break;
		}
		msleep(1);

	}
	if(PORT_PLS_VALUE(temp) != 0){
		xhci_err(xhci, "port not return U0 state\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}


static int dbg_u3ct_ellisys(int argc, char** argv){
	int td_no;

	td_no = 0;
	if(argc > 1){
		td_no = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	switch(td_no){
		case 706:
			mtk_test_dbg( "(7061) STEP 1, (7062) STEP 2\n");
			break;
		case 7061:
			return u3ct_ellisys7061();
			break;
		case 7062:
			return u3ct_ellisys7062();
			break;
		case 718:
			return u3ct_ellisys718();
			break;
		case 719:
			return u3ct_ellisys719();
			break;
		case 720:
			return u3ct_ellisys720();
			break;
		case 721:
			return u3ct_ellisys721();
			break;
		case 729:
			return u3ct_ellisys729();
			break;
		case 731:
			return u3ct_ellisys731();
			break;
		case 734:
			mtk_test_dbg( "(7341) STEP 1, (7342) STEP 2\n");
			break;
		case 7341:
			return u3ct_ellisys7341();
			break;
		case 7342:
			return u3ct_ellisys7342();
			break;
		case 735:
			return u3ct_ellisys735();
			break;
		case 736:
			mtk_test_dbg( "(7361) STEP 1, (7362) STEP 2, (7363) STEP 3\n");
			break;
		case 7361:
			return u3ct_ellisys7361();
			break;
		case 7362:
			return u3ct_ellisys7362();
			break;
		case 7363:
			return u3ct_ellisys7363();
			break;
		default:
			mtk_test_dbg( "enter td number\n");
			break;
	}
}

static int u2ct_signal_quality(){
	u32 __iomem *addr;
	struct xhci_hcd *xhci;
	u32 temp, port_id;
	int test_value;


	//initial host controller
	if(ct_check_hcd()){
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);
	//turn off port power
	g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_id = g_num_u3_port+1;
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	xhci_writel(xhci, temp, addr);

	mtk_test_dbg( "issue test packet\n");
	//test mode - test packet
	test_value = 4;
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(0xf<<28);
	temp |= (test_value<<28);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int u2ct_packet_parameter_init(){

	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	mtk_test_dbg( "STEP 1: enable port and slot, address device(BSR=1)...\n");
	if(f_enable_port(0)){
		mtk_test_dbg( "enable port FAILED!!\n");
		return RET_FAIL;
	}
	if(f_enable_slot(NULL)){
		mtk_test_dbg( "enable slot FAILED!!\n");
		return RET_FAIL;
	}
	if(f_address_slot(false, NULL)){
		mtk_test_dbg( "address device (BSR=1) FAILED!!\n");
		return RET_FAIL;
	}
}

static int u2ct_packet_parameter_getdesc(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	if(g_slot_id == 0){
		mtk_test_dbg( "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);

	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	if(urb->status == -EINPROGRESS){
		//timeout, stop endpoint, set TR dequeue pointer
		f_ring_stop_ep(g_slot_id, 0);
		f_ring_set_tr_dequeue_pointer(g_slot_id, 0, urb);
	}
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);

	return ret;
}

static int u2ct_disconnect_detect(){
	u32 __iomem *addr;
	struct xhci_hcd *xhci;
	u32 temp, port_id;
	int test_value;

	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	//turn off power
	xhci = hcd_to_xhci(my_hcd);
	//turn off port power
	g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_id = g_num_u3_port+1;
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	xhci_writel(xhci, temp, addr);
	//let SW doesn't do reset after get discon/conn events
	//g_hs_block_reset = true;
	g_port_connect = false;
	mb() ;
	printk(KERN_ERR "[OTG_H] u2ct_disconnect_detect, g_port_connect is %d\n", g_port_connect);
	
	//test mode - force enable
	test_value = 5;
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(0xf<<28);
	temp |= (test_value<<28);
	xhci_writel(xhci, temp, addr);
	mtk_test_dbg( "Please connect device in 30 secs\n");
	//waiting for conn event
	wait_event_on_timeout(&g_port_connect, true, TRANS_TIMEOUT);
	if(!g_port_connect){
		mtk_test_dbg( "Port not connected\n");
		g_hs_block_reset = false;
		return RET_FAIL;
	}
	mtk_test_dbg( "Device connected\n");
	mtk_test_dbg( "Please disconnect device in 30 secs\n");
	//waiting for discon event
	wait_event_on_timeout(&g_port_connect, false, TRANS_TIMEOUT);
	if(g_port_connect){
		mtk_test_dbg( "Port not disconnected\n");
		g_hs_block_reset = false;
		return RET_FAIL;
	}
	mtk_test_dbg( "Device disconnected\n");
	g_hs_block_reset = false;
	return RET_SUCCESS;
}

static int u2ct_chirp_timing(){
	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u2ct_suspend_resume_timing_init(){
	mtk_test_dbg( "Reset host controller\n");
	if(my_hcd){
		if(f_test_lib_cleanup()){
			mtk_test_dbg( "cleanup host controller FAILED\n");
			return RET_FAIL;
		}
	}
	if(f_test_lib_init()){
		mtk_test_dbg( "init host controller FAILED\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

static int u2ct_suspend_resume_timing_suspend(){
	return f_power_suspend();
}

static int u2ct_suspend_resume_timing_resume(){
	return f_power_resume();
}

static int u2ct_j(){
	u32 __iomem *addr;
	struct xhci_hcd *xhci;
	u32 temp, port_id;
	int test_value;

	if(ct_check_hcd()){
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);
	//turn off port power
	g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_id = g_num_u3_port+1;
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	xhci_writel(xhci, temp, addr);
	//test mode - test j
	test_value = 1;
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(0xf<<28);
	temp |= (test_value<<28);
	xhci_writel(xhci, temp, addr);

	return RET_SUCCESS;
}

static int u2ct_k(){
	u32 __iomem *addr;
	struct xhci_hcd *xhci;
	u32 temp, port_id;
	int test_value;

	if(ct_check_hcd()){
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	//turn off port power
	g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_id = g_num_u3_port+1;
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	xhci_writel(xhci, temp, addr);
	//test mode - test k
	test_value = 2;
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(0xf<<28);
	temp |= (test_value<<28);
	xhci_writel(xhci, temp, addr);

	return RET_SUCCESS;
}

static int u2ct_se0_nak(){
	u32 __iomem *addr;
	struct xhci_hcd *xhci;
	u32 temp, port_id;
	int test_value;

	if(ct_check_hcd()){
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	//turn off port power
	g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_id = g_num_u3_port+1;
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	xhci_writel(xhci, temp, addr);
	//test mode - test k
	test_value = 3;
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(0xf<<28);
	temp |= (test_value<<28);
	xhci_writel(xhci, temp, addr);

	return RET_SUCCESS;
}

static int dbg_u2ct(int argc, char** argv){
	int td_no;

	td_no = 0;
	if(argc > 1){
		td_no = (int)simple_strtol(argv[1], &argv[1], 10);
	}
	switch(td_no){
		case 6: case 3: case 7:
			return u2ct_signal_quality();
		break;
		case 21: case 25: case 23: case 22: case 55:
			mtk_test_dbg( "(211, 251, 231, 221, 551) STEP 1, (212, 252, 232, 222, 552) STEP 2, (213, 253, 233, 223, 553) STEP 3\n");
		break;
		case 211: case 251: case 231: case 221: case 551:
			return u2ct_packet_parameter_init();
		break;
		case 212: case 213: case 252: case 253: case 232: case 233: case 222: case 223: case 552: case 553:
			return u2ct_packet_parameter_getdesc();
		break;
		case 37: case 36:
			return u2ct_disconnect_detect();
		break;
		case 33: case 34: case 35:
			return u2ct_chirp_timing();
		break;
		case 39: case 41:
			mtk_test_dbg( "(391,411) STEP 1, (392, 412) STEP 2, (393, 413) STEP 3\n");
		break;
		case 391: case 411:
			return u2ct_suspend_resume_timing_init();
		break;
		case 392: case 412:
			return u2ct_suspend_resume_timing_suspend();
		break;
		case 393: case 413:
			return u2ct_suspend_resume_timing_resume();
		break;
		case 8:
			mtk_test_dbg( "(81) J test (82) K test\n");
		break;
		case 81:
			return u2ct_j();
		break;
		case 82:
			return u2ct_k();
		break;
		case 9:
			return u2ct_se0_nak();
		break;
		default:
			mtk_test_dbg( "please enter EL number\n");
		break;
	}
}

#define EP_SPEED_SS	1
#define EP_SPEED_HS	2
#define EP_SPEED_FS	3
#define EP_SPEED_LS	4
#define EP_SPEED_RANDOM	0
#define EP_OUT	1
#define EP_IN	0

#define MAX_EPNUM	100

static int dbg_sch_algorithm(int argc, char** argv){
	struct xhci_hcd *xhci;
	struct usb_device *udev_ss, *udev_hs, *udev_fs, *udev_ls, *rhdev, *udev;
	int test_speed, cur_speed;
	struct usb_host_endpoint *ep;
	struct xhci_ep_ctx *ep_ctx;
	char is_continue, ep_boundary, ep_num;
	int ep_dir, transfer_type, maxp, bInterval, mult, burst;
	int ret;
	int ep_index, i;
	struct usb_host_endpoint *ep_list[MAX_EPNUM];
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *slot_ctx;
	int interval;
	int isTT;
	struct sch_ep *sch_ep;
	test_speed = EP_SPEED_HS;
	ep_boundary = 30;

	if(argc > 1){
		//0: all(except for ss), 1: ss, 2: hs, 3: fs, 4: ls
		test_speed = (int)simple_strtol(argv[1], &argv[1], 10);
		mtk_test_dbg( "Test speed %d\n", test_speed);
	}
	if(argc > 2){
		ep_boundary = (int)simple_strtol(argv[2], &argv[2], 10);
		mtk_test_dbg( "Try %d EPs\n", ep_boundary);
		if(ep_boundary > MAX_EPNUM){
			mtk_test_dbg( "EP num too much!!\n");
			return RET_FAIL;
		}
	}

	xhci = hcd_to_xhci(my_hcd);
	//create 4 device, ss, hs, fs(tt), ls(tt)
	rhdev = my_hcd->self.root_hub;
	//ss
	udev_ss = mtk_usb_alloc_dev(rhdev, rhdev->bus, 1);
	udev_ss->speed = USB_SPEED_SUPER;
	udev_ss->level = rhdev->level + 1;
	udev_ss->slot_id = 1;
	mtktest_xhci_alloc_virt_device(xhci, 1, udev_ss, GFP_KERNEL);
	//hs
	udev_hs = mtk_usb_alloc_dev(rhdev, rhdev->bus, 2);
	udev_hs->speed = USB_SPEED_HIGH;
	udev_hs->level = rhdev->level + 1;
	udev_hs->slot_id = 2;
	mtktest_xhci_alloc_virt_device(xhci, 2, udev_hs, GFP_KERNEL);
	//fs
	udev_fs = mtk_usb_alloc_dev(rhdev, udev_hs->bus, 1);
	udev_fs->speed = USB_SPEED_FULL;
	udev_fs->level = udev_hs->level + 1;
	udev_fs->tt = kzalloc(sizeof(struct usb_tt), GFP_KERNEL);
	udev_fs->tt->hub = udev_hs;
	udev_fs->tt->multi = false;
	udev_fs->tt->think_time = 0;
	udev_fs->ttport = 1;
	udev_fs->slot_id = 3;
	mtktest_xhci_alloc_virt_device(xhci, 3, udev_fs, GFP_KERNEL);
	virt_dev = xhci->devs[3];
	slot_ctx = mtktest_xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	slot_ctx->tt_info = 0x1;
	//ls
	udev_ls = mtk_usb_alloc_dev(rhdev, udev_hs->bus, 2);
	udev_ls->speed = USB_SPEED_LOW;
	udev_ls->level = udev_hs->level + 1;
	udev_ls->tt = kzalloc(sizeof(struct usb_tt), GFP_KERNEL);
	udev_ls->tt->hub = udev_hs;
	udev_ls->tt->multi = false;
	udev_ls->tt->think_time = 0;
	udev_ls->ttport = 2;
	udev_ls->slot_id = 4;
	mtktest_xhci_alloc_virt_device(xhci, 4, udev_ls, GFP_KERNEL);
	virt_dev = xhci->devs[4];
	slot_ctx = mtktest_xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	slot_ctx->tt_info = 0x1;
	//1.random add ep (speed(fix or random), iso or intr, maxp, interrupt, mult, burst)
	//2.record this EP
	//3.directly call add/remove ep scheduler API
	//4.check if success, if failed over 10 times next, else goto 1
	ep_num = 0;
	ep_index = 0;
	while(ep_num < ep_boundary){
		ep_dir = get_random_int() % 2;
		burst = 0;
		mult = 0;
		isTT=0;
		if(test_speed == 0){
			cur_speed = (get_random_int()%3)+2;
		}
		else{
			cur_speed = test_speed;
		}
		switch(cur_speed){
			case EP_SPEED_SS:
				udev = udev_ss;
				transfer_type = get_random_int() % 2;
				if(transfer_type == 0){
					transfer_type = EPATT_INT;
					burst = get_random_int() % 3;
					if(burst > 0){
						maxp = 1024;
					}
					else{
						maxp = ((get_random_int() % 16)+1)*64;
					}
					bInterval = (get_random_int()%8)+1;
				}
				else{
					burst = get_random_int() % 16;
					if(burst > 0){
						maxp = 1024;
					}
					else{
						maxp = ((get_random_int() % 4)+1)*256;
					}
					mult = get_random_int() % 3;
					bInterval = (get_random_int()%8)+1;
				}
				break;
			case EP_SPEED_HS:
				udev = udev_hs;
				transfer_type = get_random_int() % 2;
				if(transfer_type == 0){
					transfer_type = EPATT_INT;
					maxp = ((get_random_int() % 16)+1)*64;
					mult = get_random_int() % 3;
					burst = 0;
					bInterval = (get_random_int()%8)+1;
				}
				else{
					maxp = ((get_random_int() % 16) + 1)*64;
					mult = get_random_int() % 3;
					burst = 0;
					bInterval = (get_random_int()%8)+1;
				}
				break;
			case EP_SPEED_FS:
				udev = udev_fs;
				transfer_type = get_random_int() % 2;
				if(transfer_type == 0){
					transfer_type = EPATT_INT;
					maxp = ((get_random_int() % 8)+1)*8;
					burst = 0;
					mult = 0;
					bInterval = (get_random_int()%256)+1;
					isTT=1;
				}
				else{
					maxp = ((get_random_int() % 16) + 1)*64;
					if(maxp==1024){
						maxp = 1023;
					}
					burst = 0;
					mult = 0;
					bInterval = (get_random_int()%6)+1;
					isTT=1;
				}
				break;
			case EP_SPEED_LS:
				udev = udev_ls;
				transfer_type = EPATT_INT;
				maxp = 8;
				bInterval = (get_random_int()%256)+1;
				isTT=1;
				break;
		}
		ep = kmalloc(sizeof(struct usb_host_endpoint), GFP_NOIO);
		ep->desc.bDescriptorType = USB_DT_ENDPOINT;
		if(ep_dir == EP_OUT){
			ep->desc.bEndpointAddress = EPADD_NUM(1) | EPADD_OUT;
		}
		else{
			ep->desc.bEndpointAddress = EPADD_NUM(1) | EPADD_IN;
		}
		ep->desc.bmAttributes = transfer_type;
		if(cur_speed == EP_SPEED_SS){
			ep->desc.wMaxPacketSize = maxp;
			ep->ss_ep_comp.bmAttributes = mult;
			ep->ss_ep_comp.bMaxBurst = burst;
		}
		else{
			ep->desc.wMaxPacketSize = maxp | (mult << 11);
			ep->ss_ep_comp.bmAttributes = 0;
			ep->ss_ep_comp.bMaxBurst = 0;
		}
		ep->desc.bInterval = bInterval;
		interval = 1<<(bInterval-1);
		ep_ctx = mtk_xhci_alloc_container_ctx(xhci, XHCI_CTX_TYPE_DEVICE, GFP_NOIO);
		if(transfer_type == EPATT_INT && (cur_speed == EP_SPEED_FS || cur_speed == EP_SPEED_LS)){
			//rounding interval
			interval = fls(8 * ep->desc.bInterval) - 1;
			interval = clamp_val(interval, 3, 10);
			mtk_test_dbg( "ROUNDING interval to %d\n", interval);
			ep_ctx->ep_info = EP_INTERVAL(interval);
		}
		mtk_test_dbg( "[EP]Speed[%d]dir[%d]transfer_type[%d]bInterval[%d]maxp[%d]burst[%d]mult[%d]"
			, cur_speed, ep_dir, transfer_type, bInterval, maxp, burst, mult);
#if MTK_SCH_NEW
		sch_ep = kmalloc(sizeof(struct sch_ep), GFP_KERNEL);
		ret = mtktest_mtk_xhci_scheduler_add_ep(udev->speed, usb_endpoint_dir_in(&ep->desc), isTT
			, transfer_type, maxp, interval, burst, mult, (u32 *)ep, (u32 *)ep_ctx, sch_ep);
#else
		ret = mtktest_mtk_xhci_scheduler_add_ep(xhci, udev, ep, ep_ctx);
#endif
		if(ret == SCH_SUCCESS){
			mtk_test_dbg( "......Success\n");
			ep_list[ep_index] = ep;
			ep = NULL;
			ep_index++;
		}
		else{
			mtk_test_dbg( "......Failed\n");
		}
		ep_num++;
	}
	//dump EP list and scheduler result
	//mtk_xhci_scheduler_dump_all(xhci);
	return RET_SUCCESS;
}

static int dbg_u3_calibration(int argc, char** argv){
	char u1TimeDelay;
	char u1TimingValue;
	char u1PhyDrvValue;
	char u1TxSel;
	char u1RxSel;
	char u1PhyDrv;
	int u4LatchValue;
	char *pu1Buf;
	char *pu2Buf;
	char u1ReadTimingValue;
	char u1ReadPhyClkDrvValue;
	int u4USBStsVal;
	int u4P1ScVal;
	char u1P1PlsVal;
	char u1TmpP1PlsVal;
	char u1Macltssm;
	char u1TmpMacltssm;
	int u4ErrorCnt;
	int u4RecoveryCnt;
	int u4CorrectCnt;
	int u4ResetValue;
	char u1IsPass;
	char i;
	int temp;
	int num_u3_port, num_u2_port;
	__u32 __iomem *ip_reset_addr;
	__u32 __iomem *latch_sel_addr;
	__u32 __iomem *u3_cmd_addr;
	__u32 __iomem *u3_sts_addr;
	__u32 __iomem *u3_p1_sc_addr;
	__u32 __iomem *u3_p1_ltssm_addr;
	__u32 __iomem *u3_link_err_count_addr;
	__u32 __iomem *u3_recovery_count_addr;

	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));

	latch_sel_addr = U3_PIPE_LATCH_SEL_ADD;
	u3_cmd_addr = U3_XHCI_CMD_ADDR;
	u3_sts_addr = U3_XHCI_STS_ADDR;
	u3_p1_sc_addr = U3_P1_SC;
	u3_p1_ltssm_addr = U3_P1_LTSSM;
	u3_link_err_count_addr = U3_LINK_ERR_COUNT;
	u3_recovery_count_addr = U3_RECOVERY_COUNT;
	u3phy_init();
	u1TxSel = 0;
	for(u1RxSel = 0; u1RxSel<2; u1RxSel++){
		u1PhyDrv = 2;
		for(u1TimeDelay=0; u1TimeDelay<32; u1TimeDelay++){
			mtk_test_dbg( "Test TxSel[0x%02x] RxSel[0x%02x] TimingDelay[0x%02x] Reg[0x%02x]...\n"
				, u1TxSel, u1RxSel, u1TimeDelay, u1TimeDelay<<3);
			//write phase, drv
			u3phy_ops->change_pipe_phase(u3phy, u1PhyDrv, u1TimeDelay);
			//reset U3H IP
			writel(readl(SSUSB_IP_PW_CTRL) | (SSUSB_IP_SW_RST), SSUSB_IP_PW_CTRL);
			writel(readl(SSUSB_IP_PW_CTRL) & (~SSUSB_IP_SW_RST), SSUSB_IP_PW_CTRL);
			writel(readl(SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN), SSUSB_IP_PW_CTRL_1);

			//turn on power, clock of each port
			for(i=0; i<num_u3_port; i++){
				temp = readl(SSUSB_U3_CTRL(i));
				temp = temp & (~SSUSB_U3_PORT_PDN) & (~SSUSB_U3_PORT_DIS);
				writel(temp, SSUSB_U3_CTRL(i));
			}
			for(i=0; i<num_u2_port; i++){
				temp = readl(SSUSB_U2_CTRL(i));
				temp & (~SSUSB_U2_PORT_PDN) & (~SSUSB_U2_PORT_DIS);
				writel(temp, SSUSB_U2_CTRL(i));
			}
			msleep(10);
			//set pipe latch
			u4LatchValue = (u1TxSel<<2) | u1RxSel;
			writel(u4LatchValue, latch_sel_addr);
			//set hcrst
			writel(2, u3_cmd_addr);
			//polling CNR
			for(i=0; i<500; i++){
				u4USBStsVal = readl(u3_sts_addr);
				if(!((u4USBStsVal&U3_CNR) == U3_CNR)){
					break;
				}
			}
			if((u4USBStsVal&U3_CNR) == U3_CNR){
				mtk_test_dbg( "CNR not 0 after HCRST \n");
				continue;
			}

			//for loopback mode, write mac3 t2r loopback
//			writel(0x1, 0xf00426dc);

			//enable port power of port 1 in xHC port reg
			temp = readl(u3_p1_sc_addr);
			temp = mtktest_xhci_port_state_to_neutral(temp);
			temp |= PORT_POWER;
			writel(temp, u3_p1_sc_addr);
			//Wait 100 ms
			msleep(100);
			//polling PLS
			u4CorrectCnt=0;
			u1IsPass=1;
			u1TmpMacltssm = 0;
			u1TmpP1PlsVal = 0;
			//polling 200 times
			//if PLS in compliance, set warm reset
			//if polling status in U0 more than 3 times, check recovery & link error count, report PASS
			//	print phase information
			//else report FAIL
			for(i=0; i<200; i++){
				u4P1ScVal = readl(u3_p1_sc_addr);
				u1P1PlsVal = (u4P1ScVal>>U3_PLS_OFFSET) & 0xf;
				u1Macltssm = readl(u3_p1_ltssm_addr);
				u1Macltssm = u1Macltssm & 0x1f;
				if(u1TmpMacltssm != u1Macltssm || u1TmpP1PlsVal != u1P1PlsVal){
					mtk_test_dbg( "Polling %d PLS=%d LTSSM=%d\n", i, u1P1PlsVal, u1Macltssm);
					u1TmpMacltssm = u1Macltssm;
					u1TmpP1PlsVal = u1P1PlsVal;
				}
				if(u1P1PlsVal == 10){
					u4P1ScVal = u4P1ScVal | (int)(1<<31);
					writel(u4P1ScVal, u3_p1_sc_addr);
				}
				if(u1P1PlsVal == 0){
					u4CorrectCnt++;
				}
				if(u4CorrectCnt==3){
					//clear recovery count
					writel((1<<16), u3_recovery_count_addr);
					u1IsPass = 0;
					//read error count
					u4ErrorCnt = readl(u3_link_err_count_addr);
					msleep(500);
					//read error count and recovery count in these period
					u4ErrorCnt = readl(u3_link_err_count_addr);
					u4RecoveryCnt = readl(u3_recovery_count_addr);
					u4CorrectCnt=4;
				}
				msleep(10);
			}
			if(u1IsPass == 0){
				mtk_test_dbg( "[PASS] PLS=0, Error=%d, Recovery=%d, TxSel[0x%02x] RxSel[0x%02x] PhyDrv[0x%02x] Reg[0x%02x] TimingDelay[0x%02x] Reg[0x%02x]\n"
					, u4ErrorCnt, u4RecoveryCnt, u1TxSel, u1RxSel, u1PhyDrv, u1PhyDrvValue, u1TimeDelay, u1TimingValue);
			}
			else{
				mtk_test_dbg( "[FAIL] PLS=%d\n", u1P1PlsVal);
			}
			msleep(3);
		}
	}
}

static int dbg_printhccparams(int argc, char** argv){
	struct xhci_hcd *xhci;
	xhci = hcd_to_xhci(my_hcd);
	u32 __iomem *addr;
	int temp, tmp_add, i;

//	mtktest_xhci_print_registers(xhci);
	xhci_dbg(xhci, "hcs_params1 add: 0x%p\n", &xhci->cap_regs->hcs_params1);
	xhci_dbg(xhci, "hcs_params2 add: 0x%p\n", &xhci->cap_regs->hcs_params2);
	xhci_dbg(xhci, "hcs_params3 add: 0x%p\n", &xhci->cap_regs->hcs_params3);
	xhci_dbg(xhci, "hc_capbase add: 0x%p\n", &xhci->cap_regs->hc_capbase);
	xhci_dbg(xhci, "hcc_params add: 0x%p\n", &xhci->cap_regs->hcc_params);
	temp = xhci_readl(xhci, &xhci->cap_regs->hcc_params);
	xhci_dbg(xhci, "hcc_params: 0x%x\n", temp);
	tmp_add = temp >> 16;
	addr = ((&xhci->cap_regs->hc_capbase) + (temp >>16));
	i = 1;
	while(tmp_add != 0){
		xhci_dbg(xhci, "cap pointer %d[0x%p: \n",i, addr);
		temp = xhci_readl(xhci, addr);
		xhci_dbg(xhci, "0x%x\n", temp);
		tmp_add = (temp >> 8) & 0xff;
		temp = xhci_readl(xhci, addr+1);
		xhci_dbg(xhci, "0x%x\n", temp);
		temp = xhci_readl(xhci, addr+2);
		xhci_dbg(xhci, "0x%x\n", temp);
		temp = xhci_readl(xhci, addr+3);
		xhci_dbg(xhci, "0x%x\n", temp);
		i++;
		addr = addr + tmp_add;
	}

	return RET_SUCCESS;
}

static dbg_port_set_pls(int argc, char** argv){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;
	int pls, port_id;

	if(argc > 1){
		port_id = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "port_id = %d\n", port_id);
	}

	if(argc > 2){
		pls = (int)simple_strtol(argv[2], &argv[2], 10);
		xhci_dbg(xhci, "pls set to %d\n", pls);
	}
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = temp & ~(PORT_PLS_MASK);
	temp = temp | (pls << 5) | PORT_LINK_STROBE;
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int dbg_port_set_ped(int argc, char** argv){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;
	int port_id;

	if(argc > 1){
		port_id = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "port_id = %d\n", port_id);
	}
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = temp | PORT_PE;
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

static int dbg_port_reset(int argc, char** argv){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp, port_id;
	char isWarmReset;

	port_id = g_port_id;
	isWarmReset = true;

	if(argc > 1){
		port_id = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "port_id = %d\n", port_id);
	}

	if(argc > 2){
		if(!strcmp(argv[2], "true")){
			mtk_test_dbg( "test WarmReset=true\n");
			isWarmReset = true;
		}
	}
	xhci = hcd_to_xhci(my_hcd);
	if(isWarmReset){
		//do warm reset
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp | PORT_WR);
		xhci_writel(xhci, temp, addr);
	}
	else{
		//hot reset port
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp | PORT_RESET);
		xhci_writel(xhci, temp, addr);
	}
	return RET_SUCCESS;
}

static int dbg_printportstatus(int argc, char** argv){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp, port_id;

	port_id = g_port_id;
	if(argc > 1){
		port_id = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "port_id = %d\n", port_id);
	}
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "port %d = 0x%x\n", port_id, temp);
	return RET_SUCCESS;
}

static int dbg_delayms(int argc, char** argv){
	int msecs;
	g_stopped = true;
#if 0
	msecs = 10;
	if(argc > 1){
		msecs = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "delay %d msecs\n", msecs);
	}

	msleep(msecs);
#endif
	return RET_SUCCESS;
}

extern PHY_INT32 _U3Read_Reg(PHY_INT32 address);
extern PHY_INT32 _U3Write_Reg(PHY_INT32 address, PHY_INT32 value);

static int dbg_u3w(int argc, char**argv){
	int u4TimingValue;
	char u1TimingValue;
	int u4TimingAddress;

	if (argc<3)
    {
        mtk_test_dbg( "Arg: address value\n");
        return RET_FAIL;
    }
	u4TimingAddress = (int)simple_strtol(argv[1], &argv[1], 16);
	u4TimingValue = (int)simple_strtol(argv[2], &argv[2], 16);
	u1TimingValue = u4TimingValue & 0xff;
	//_U3Write_Reg(u4TimingAddress, u1TimingValue);
	U3PhyWriteReg8(u4TimingAddress, u1TimingValue);
	mtk_test_dbg( "Write done\n");
	return RET_SUCCESS;
}

static int dbg_u3r(int argc, char**argv){
	char u1ReadTimingValue;
	int u4TimingAddress;
	if (argc<2)
    {
        mtk_test_dbg( "Arg: address\n");
        return 0;
    }
	mdelay(500);
	u4TimingAddress = (int)simple_strtol(argv[1], &argv[1], 16);
	//u1ReadTimingValue = _U3Read_Reg(u4TimingAddress);
	u1ReadTimingValue = U3PhyReadReg8(u4TimingAddress);
	mtk_test_dbg( "Value = 0x%x\n", u1ReadTimingValue);
	return 0;
}

#if 0
extern PHY_INT32 u2_slew_rate_calibration_d60802(struct u3phy_info *info);
static int dbg_u3PHY_init(int argc, char**argv){
	int ret;
	ret = u3phy_init();
	mtk_test_dbg( "phy registers and operations initial done\n");
	if(u3phy_ops->u2_slew_rate_calibration){
		u3phy_ops->u2_slew_rate_calibration(u3phy);
	}
	else{
		mtk_test_dbg( "WARN: PHY doesn't implement u2 slew rate calibration function\n");
	}
	if(u3phy_ops->init(u3phy) == PHY_TRUE)
		return RET_SUCCESS;
	return RET_FAIL;
}
#else

// USBIF , skip slew rate calibration ? 
extern void phy_probe_out_x();
extern void phy_dump_regs();
extern void phy_probe_reg();
extern void phy_reset();

static int dbg_u3PHY_init(int argc, char**argv){
	u3phy_init();
	mtk_test_dbg( "phy registers and operations initial done\n");
	u3phy_ops->init(u3phy);
	return RET_SUCCESS;
}

#endif

static int dbg_phy_eyescan(int argc, char** argv){
	_rEye1.bX_tl = (int)simple_strtol(argv[1], &argv[1], 10);
	_rEye1.bY_tl = (int)simple_strtol(argv[2], &argv[2], 10);
	_rEye1.bX_br = (int)simple_strtol(argv[3], &argv[3], 10);
	_rEye1.bY_br = (int)simple_strtol(argv[4], &argv[4], 10);
	_rEye1.bDeltaX = (int)simple_strtol(argv[5], &argv[5], 10);
	_rEye1.bDeltaY = (int)simple_strtol(argv[6], &argv[6], 10);

	_rEye2.bX_tl = (int)simple_strtol(argv[1], &argv[1], 10);
	_rEye2.bY_tl = (int)simple_strtol(argv[2], &argv[2], 10);
	_rEye2.bX_br = (int)simple_strtol(argv[3], &argv[3], 10);
	_rEye2.bY_br = (int)simple_strtol(argv[4], &argv[4], 10);
	_rEye2.bDeltaX = (int)simple_strtol(argv[5], &argv[5], 10);
	_rEye2.bDeltaY = (int)simple_strtol(argv[6], &argv[6], 10);

	_rTestCycle.wEyeCnt = (int)simple_strtol(argv[7], &argv[7], 10);
	_rTestCycle.bNumOfEyeCnt = (int)simple_strtol(argv[8], &argv[8], 10);
	_rTestCycle.bNumOfIgnoreCnt = (int)simple_strtol(argv[9], &argv[9], 10);
	_rTestCycle.bPICalEn = 1;

	u3phy_init();
	u3phy_ops->eyescan(u3phy, _rEye1.bX_tl, _rEye1.bY_tl, _rEye1.bX_br, _rEye1.bY_br, _rEye1.bDeltaX, _rEye1.bDeltaY
		, _rTestCycle.wEyeCnt, _rTestCycle.bNumOfEyeCnt, _rTestCycle.bPICalEn, _rTestCycle.bNumOfIgnoreCnt);
	return RET_SUCCESS;
}

static int dbg_u2_testmode(int argc, char** argv){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp, port_id;
	int test_value;

	if(argc<2){
		mtk_test_dbg( "Args: test mode value\n");
		mtk_test_dbg( "  (0)not enabled (1)J_STATE (2)K_STATE (3)SE0_NAK (4)Packet (5)FORCE_ENABLE (15)Port test control error\n");
		return RET_FAIL;
	}
	port_id = 2;

	test_value = (int)simple_strtol(argv[1], &argv[1], 10);
	xhci_dbg(xhci, "test_value = %d\n", test_value);

	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(0xf<<28);
	temp |= (test_value<<28);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;



}

static int dbg_memorywrite(int argc, char** argv){
	unsigned int addr, value;
	if (argc<3)
    {
        mtk_test_dbg( "Arg: address value\n");
        return RET_FAIL;
    }

	addr = (int)simple_strtol(argv[1], &argv[1], 16);
	value = (int)simple_strtol(argv[2], &argv[2], 16);

	writel(value, addr);
}

static int dbg_memoryread(int argc, char** argv){
	unsigned int addr, value;
	if (argc<2)
    {
        mtk_test_dbg( "Arg: address\n");
        return RET_FAIL;
    }
	addr = (int)simple_strtol(argv[1], &argv[1], 16);
	value = readl(addr);
	mtk_test_dbg( "Addr: 0x%x, Value: 0x%x\n", addr, value);
}

static int dbg_printslotcontext(int argc, char** argv){
	struct xhci_hcd *xhci;
	struct xhci_container_ctx *out_ctx, *in_ctx;
	int slot_id;

	slot_id = g_slot_id;

	if(argc > 1){
		slot_id = (int)simple_strtol(argv[1], &argv[1], 10);
		xhci_dbg(xhci, "slot id set to %d\n", slot_id);
	}

	xhci = hcd_to_xhci(my_hcd);
	in_ctx = xhci->devs[slot_id]->in_ctx;
	mtktest_xhci_dbg_ctx(xhci, in_ctx, 5);
	out_ctx = xhci->devs[slot_id]->out_ctx;
	mtktest_mtk_xhci_dbg_ctx(xhci, out_ctx, 5);
	return RET_SUCCESS;
}

static int dbg_read_xhci(int argc, char** argv)
{
    u32 u4SrcAddr;
    u32 u4Addr;
    u32 u4Len;
    u32 u4Idx;
    u32 temp1, temp2, temp3, temp4;

	struct xhci_hcd		*xhci = hcd_to_xhci(my_hcd);

    //4-bytes alignment
    u4SrcAddr = simple_strtoul(argv[1], NULL, 16) & 0xfffffffc;
    u4Len = simple_strtoul(argv[2], NULL, 16);

    //no operation is needed
    if (u4Len == 0)
    {
        return 0;
    }

    //the maximum number of bytes
    if (u4Len > 0x1000)
    {
        u4Len = 0x1000;
    }

    //sum together xhci register base address and offset
    u4SrcAddr += ((u32)(unsigned long)xhci->cap_regs);

    for (u4Idx = 0; u4Idx < u4Len; u4Idx += 16)
    {
    	temp1 = xhci_readl(xhci, (u4SrcAddr + u4Idx + 0));
    	temp2 = xhci_readl(xhci, (u4SrcAddr + u4Idx + 4));
    	temp3 = xhci_readl(xhci, (u4SrcAddr + u4Idx + 8));
    	temp4 = xhci_readl(xhci, (u4SrcAddr + u4Idx + 12));

		mtk_test_dbg("0x%08x | %08x %08x %08x %08x\r\n", (u4SrcAddr + u4Idx), temp1, temp2, temp3, temp4);
    }

    return 0;
}

static int dbg_dump_regs(int argc, char** argv)
{
	struct xhci_hcd		*xhci = hcd_to_xhci(my_hcd);

	mtktest_xhci_print_registers(xhci);

	return 0;
}

struct file_operations xhci_mtk_test_fops_host = {
    owner:   THIS_MODULE,
    read:    xhci_mtk_test_read,
    write:   xhci_mtk_test_write,
#if KERNEL_30
	unlocked_ioctl:   xhci_mtk_test_unlock_ioctl,
#else
    ioctl:   xhci_mtk_test_ioctl,
#endif
    open:    xhci_mtk_test_open,
    release: xhci_mtk_test_release,
};

// USBIF , to send IF uevent
int usbif_u3h_test_send_event(char* event)
{	
	char udev_event[128];
	char *envp[] = {udev_event, NULL };
	int ret ;

	snprintf(udev_event, 128, "USBIF_EVENT=%s",event);
	printk("usbif_u3h_test_send_event - sending event - %s in %s\n", udev_event, kobject_get_path(&mu3h_test_uevent_device.this_device->kobj, GFP_KERNEL));
	ret = kobject_uevent_env(&mu3h_test_uevent_device.this_device->kobj, KOBJ_CHANGE, envp);	
	if (ret < 0)
		printk("usbif_u3h_test_send_event sending failed with ret = %d, \n", ret);
		
	return ret;
}

static int __init mtk_test_init(void)
{
	int retval = 0;
	struct class *cli_class ;
	dev_t cli_dev_t = MKDEV(XHCI_MTK_TEST_MAJOR, 0) ;
	struct device *cli_dev ;
	
	mtk_test_dbg( "xchi_mtk_test Init\n");

	cli_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(cli_class))
		mtk_test_dbg("%s: failed to create cli class ", __func__);
		
	cli_dev = device_create(cli_class, NULL, cli_dev_t, NULL, DEVICE_NAME);
	if (IS_ERR(cli_dev))
		mtk_test_dbg("%s: failed to create cli dev ", __func__);
		
	retval = register_chrdev(XHCI_MTK_TEST_MAJOR, DEVICE_NAME, &xhci_mtk_test_fops_host);
	if(retval < 0)
	{
		mtk_test_dbg( "xchi_mtk_test Init failed, %d\n", retval);
		goto fail;
	}
	g_port_connect = false;
	g_port_reset = false;
	g_event_full = false;
	g_port_id = 0;
	g_slot_id = 0;
	g_speed = 0; // UNKNOWN_SPEED
	g_cmd_status = CMD_DONE;
//	g_trans_status = TRANS_DONE;
	g_exec_done = true ;
	g_stopped = true ;
	mb() ;

	printk(KERN_ERR "[OTG_H] mtk_test_init, g_port_connect is %d\n", g_port_connect);

	if (!misc_register(&mu3h_test_uevent_device)){
		printk("create the mu3h_test_uevent_device uevent device OK!\n") ;

	}else{
		printk("[ERROR] create the mu3h_test_uevent_device uevent device fail\n") ;
	}

	return 0;
	fail:
		return retval;
}
module_init(mtk_test_init);

static void __exit mtk_test_cleanup(void)
{
	mtk_test_dbg( "xchi_mtk_test End\n");
	misc_deregister(&mu3h_test_uevent_device);
	
	unregister_chrdev(XHCI_MTK_TEST_MAJOR, DEVICE_NAME);
	if(my_hcd != NULL){
		mtk_xhci_hcd_cleanup();
	}
}
module_exit(mtk_test_cleanup);

MODULE_LICENSE("GPL");
