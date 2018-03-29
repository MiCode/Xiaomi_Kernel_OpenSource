
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <mach/mt_clkmgr.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/compat.h>
#include "tpd.h"
#include <linux/delay.h>

#include <linux/cpu.h>

#include "teei_client.h"
#include "teei_common.h"
#include "teei_id.h"
#include "teei_debug.h"
#include "smc_id.h"

#include "nt_smc_call.h"
#include "teei_client_main.h"
#include "utos_version.h"

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/moduleparam.h>
#include "sched_status.h"
#include "teei_smc_struct.h"
#include "utdriver_macro.h"

#define GK_BUFF_SIZE		(4 * 1024)
#define GK_SYS_NO		(120)

extern unsigned long ut_get_free_pages(gfp_t gfp_mask, unsigned int order);
extern struct semaphore keymaster_api_lock;

struct semaphore boot_decryto_lock;

unsigned long cpu_notify_flag = 0;
static  int current_cpu_id = 0x00;

static int tz_driver_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu);
static struct notifier_block tz_driver_cpu_notifer = {
	.notifier_call = tz_driver_cpu_callback,
};

DEFINE_KTHREAD_WORKER(ut_fastcall_worker);

extern wait_queue_head_t __fp_open_wq;

/******************************
 * Message header
 ******************************/

struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};



struct smc_call_struct {
	unsigned long local_cmd;
	u32 teei_cmd_type;
	u32 dev_file_id;
	u32 svc_id;
	u32 cmd_id;
	u32 context;
	u32 enc_id;
	void *cmd_buf;
	size_t cmd_len;
	void *resp_buf;
	size_t resp_len;
	void *meta_data;
	void *info_data;
	size_t info_len;
	int *ret_resp_len;
	int *error_code;
	struct semaphore *psema;
	int retVal;
};

struct smc_call_struct smc_call_entry;

asmlinkage long sys_setpriority(int which, int who, int niceval);
asmlinkage long sys_getpriority(int which, int who);

struct teei_contexts_head {
	u32 dev_file_cnt;
	struct list_head context_list;
	struct rw_semaphore teei_contexts_sem;
} teei_contexts_head;

struct teei_shared_mem_head {
	int shared_mem_cnt;
	struct list_head shared_mem_list;
};

static struct task_struct *teei_fastcall_task;
struct task_struct *teei_switch_task;

static struct cpumask mask = { CPU_BITS_NONE };

int forward_call_flag = 0;
int irq_call_flag = 0;
int fp_call_flag = 0;
int keymaster_call_flag = 0;

unsigned long teei_config_flag = 0;
unsigned int soter_error_flag = 0;

DECLARE_COMPLETION(global_down_lock);
EXPORT_SYMBOL_GPL(global_down_lock);

struct semaphore smc_lock;

void *tz_malloc(size_t size, int flags)
{
	void *ptr = kmalloc(size, flags | GFP_ATOMIC);
	return ptr;
}
void *tz_malloc_shared_mem(size_t size, int flags)
{
#ifdef UT_DMA_ZONE
	return (void *) __get_free_pages(flags | GFP_DMA, get_order(ROUND_UP(size, SZ_4K)));
#else
	return (void *) __get_free_pages(flags, get_order(ROUND_UP(size, SZ_4K)));
#endif
}
void tz_free_shared_mem(void *addr, size_t size)
{
	free_pages((unsigned long)addr, get_order(ROUND_UP(size, SZ_4K)));
}

static struct class *driver_class;
static dev_t teei_client_device_no;
static struct cdev teei_client_cdev;

/* static u32 cacheline_size; */
unsigned long device_file_cnt = 0;

struct semaphore boot_sema;
struct semaphore fdrv_sema;
struct semaphore fdrv_lock;
unsigned long boot_vfs_addr;
unsigned long boot_soter_flag;

extern struct mutex pm_mutex;


int get_current_cpuid(void)
{
	return current_cpu_id;
}


static void secondary_boot_stage2(void *info)
{
	n_switch_to_t_os_stage2();
}

static void boot_stage2(void)
{
	int cpu_id = 0;

	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_boot_stage2, NULL, 1);
	put_online_cpus();
}

int switch_to_t_os_stages2(void)
{
	down(&(smc_lock));

	boot_stage2();

	if (forward_call_flag == GLSCH_NONE)
		forward_call_flag = GLSCH_LOW;
	else if (forward_call_flag == GLSCH_NEG)
		forward_call_flag = GLSCH_NONE;
	else
		return -1;

	down(&(boot_sema));

	return 0;
}

static void secondary_load_tee(void *info)
{
	n_invoke_t_load_tee(0, 0, 0);
}


static void load_tee(void)
{
	int cpu_id = 0;

	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_load_tee, NULL, 1);
	put_online_cpus();
}


void set_sch_load_img_cmd(void)
{
	struct message_head msg_head;

	memset(&msg_head, 0, sizeof(struct message_head));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = STANDARD_CALL_TYPE;
	msg_head.child_type = N_INVOKE_T_LOAD_TEE;

	memcpy((void *)message_buff, &msg_head, sizeof(struct message_head));
	Flush_Dcache_By_Area((unsigned long)message_buff, (unsigned long)message_buff + MESSAGE_SIZE);

	return;
}


int t_os_load_image(void)
{
	down(&smc_lock);

	set_sch_load_img_cmd();
	load_tee();

	/* start HIGH level glschedule. */
	if (forward_call_flag == GLSCH_NONE)
		forward_call_flag = GLSCH_LOW;
	else if (forward_call_flag == GLSCH_NEG)
		forward_call_flag = GLSCH_NONE;
	else
		return -1;

	/* block here until the TOS ack N_SWITCH_TO_T_OS_STAGE2 */
	down(&(boot_sema));

	return 0;
}

static void secondary_teei_invoke_drv(void)
{
	n_invoke_t_drv(0, 0, 0);
	return;
}

static void post_teei_invoke_drv(int cpu_id)
{
	smp_call_function_single(cpu_id,
			secondary_teei_invoke_drv,
			NULL,
			1);
	return;
}

static void teei_invoke_drv(void)
{
	int cpu_id = 0;
	get_online_cpus();
	cpu_id = get_current_cpuid();
	post_teei_invoke_drv(cpu_id);
	put_online_cpus();

	return;
}



struct boot_stage1_struct {
	unsigned long vfs_phy_addr;
	unsigned long tlog_phy_addr;
};

struct boot_stage1_struct boot_stage1_entry;


static void secondary_boot_stage1(void *info)
{
	struct boot_stage1_struct *cd = (struct boot_stage1_struct *)info;

	/* with a rmb() */
	rmb();

	n_init_t_boot_stage1(cd->vfs_phy_addr, cd->tlog_phy_addr, 0);

	/* with a wmb() */
	wmb();

}


static void boot_stage1(unsigned long vfs_addr, unsigned long tlog_addr)
{
	int cpu_id = 0;
	boot_stage1_entry.vfs_phy_addr = vfs_addr;
	boot_stage1_entry.tlog_phy_addr = tlog_addr;

	/* with a wmb() */
	wmb();

	get_online_cpus();
	cpu_id = get_current_cpuid();
	pr_debug("current cpu id [%d]\n", cpu_id);
	smp_call_function_single(cpu_id, secondary_boot_stage1, (void *)(&boot_stage1_entry), 1);
	put_online_cpus();

	/* with a rmb() */
	rmb();
}

static int teei_cpu_id[] = {0x0000, 0x0001, 0x0002, 0x0003, 0x0100, 0x0101, 0x0102, 0x0103, 0x0200, 0x0201, 0x0202, 0x0203};

static int __cpuinit tz_driver_cpu_callback(struct notifier_block *self,
		unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	unsigned int sched_cpu = get_current_cpuid();
	struct cpumask mtee_mask = { CPU_BITS_NONE };
	int retVal = 0;
	int i;
	int switch_to_cpu_id = 0;

	switch (action) {
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
			if (cpu == sched_cpu) {
				pr_debug("cpu down prepare ************************\n");
				retVal = down_trylock(&smc_lock);
				if (retVal == 1)
					return NOTIFY_BAD;
				else {
					cpu_notify_flag = 1;
					for_each_online_cpu(i) {
						/*pr_debug("current on line cpu [%d]\n", i);*/
						if (i == cpu) {
							continue;
						}
						switch_to_cpu_id = i;
					}
					/*pr_debug("current cpu id = [%d]\n", current_cpu_id);*/
					nt_sched_core(teei_cpu_id[switch_to_cpu_id],teei_cpu_id[cpu],0);

					/*pr_debug("[%s][%d]brefore cpumask set cpu\n",__func__,__LINE__);*/
#if 1
					cpumask_set_cpu(switch_to_cpu_id, &mtee_mask);
					set_cpus_allowed(teei_switch_task, mtee_mask);
					/*pr_debug("[%s][%d]after cpumask set cpu\n",__func__,__LINE__);*/
					current_cpu_id = switch_to_cpu_id;
					pr_debug("change cpu id from [%d] to [%d]\n", sched_cpu, switch_to_cpu_id);
#endif

				}
			}
			break;

	case CPU_DOWN_FAILED:
			if (cpu_notify_flag == 1) {
				pr_debug("cpu down failed *************************\n");
				up(&smc_lock);
				cpu_notify_flag = 0;
			}
			break;

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
			if (cpu_notify_flag == 1) {
				pr_debug("cpu down success ***********************\n");
				up(&smc_lock);
				cpu_notify_flag = 0;
			}
			break;
	}

	return NOTIFY_OK;
}

struct init_cmdbuf_struct {
	unsigned long phy_addr;
	unsigned long fdrv_phy_addr;
	unsigned long bdrv_phy_addr;
	unsigned long tlog_phy_addr;
};

struct init_cmdbuf_struct init_cmdbuf_entry;


static void secondary_init_cmdbuf(void *info)
{
	struct init_cmdbuf_struct *cd = (struct init_cmdbuf_struct *)info;

	/* with a rmb() */
	rmb();

	pr_debug("[%s][%d] message = %lx,  fdrv message = %lx, bdrv_message = %lx, tlog_message = %lx.\n", __func__, __LINE__,
		(unsigned long)cd->phy_addr, (unsigned long)cd->fdrv_phy_addr,
		(unsigned long)cd->bdrv_phy_addr, (unsigned long)cd->tlog_phy_addr);

	n_init_t_fc_buf(cd->phy_addr, cd->fdrv_phy_addr, 0);

	n_init_t_fc_buf(cd->bdrv_phy_addr, cd->tlog_phy_addr, 0);


	/* with a wmb() */
	wmb();
}


static void init_cmdbuf(unsigned long phy_address, unsigned long fdrv_phy_address,
			unsigned long bdrv_phy_address, unsigned long tlog_phy_address)
{
	int cpu_id = 0;

	init_cmdbuf_entry.phy_addr = phy_address;
	init_cmdbuf_entry.fdrv_phy_addr = fdrv_phy_address;
	init_cmdbuf_entry.bdrv_phy_addr = bdrv_phy_address;
	init_cmdbuf_entry.tlog_phy_addr = tlog_phy_address;

	/* with a wmb() */
	wmb();

	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_init_cmdbuf, (void *)(&init_cmdbuf_entry), 1);
	put_online_cpus();

	/* with a rmb() */
	rmb();
}


long create_cmd_buff(void)
{
	unsigned long irq_status = 0;
	long retVal = 0;

#ifdef UT_DMA_ZONE
	message_buff =  (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#else
	message_buff =  (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#endif
	if (message_buff == NULL) {
		pr_err("[%s][%d] Create message buffer failed!\n", __FILE__, __LINE__);
		return -ENOMEM;
	}
#ifdef UT_DMA_ZONE
	fdrv_message_buff =  (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#else
	fdrv_message_buff =  (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#endif
	if (fdrv_message_buff == NULL) {
		pr_err("[%s][%d] Create fdrv message buffer failed!\n", __FILE__, __LINE__);
		free_pages(message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		return -ENOMEM;
	}

#ifdef UT_DMA_ZONE
	bdrv_message_buff = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#else
	bdrv_message_buff = (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#endif
	if (bdrv_message_buff == NULL) {
		pr_err("[%s][%d] Create bdrv message buffer failed!\n", __FILE__, __LINE__);
		free_pages(message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		free_pages(fdrv_message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		return -ENOMEM;
	}

#ifdef UT_DMA_ZONE
	tlog_message_buff = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(MESSAGE_LENGTH * 64, SZ_4K)));
#else
	tlog_message_buff = (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(MESSAGE_LENGTH * 64, SZ_4K)));
#endif
	if (tlog_message_buff == NULL) {
		pr_err("[%s][%d] Create tlog message buffer failed!\n", __FILE__, __LINE__);
		free_pages(message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		free_pages(fdrv_message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		free_pages(bdrv_message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		return -ENOMEM;
	}

	retVal = create_utgate_log_thread(tlog_message_buff, MESSAGE_LENGTH * 64);
	if (retVal != 0) {
		pr_err("[%s][%d] failed to create utgate tlog thread!\n", __func__, __LINE__);
		free_pages(message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		free_pages(fdrv_message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		free_pages(bdrv_message_buff, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		free_pages(tlog_message_buff, get_order(ROUND_UP(MESSAGE_LENGTH * 64, SZ_4K)));
		return retVal;
	}

	/* smc_call to notify SOTER the share memory(message_buff) */

	/* n_init_t_fc_buf((unsigned long)virt_to_phys(message_buff), 0, 0); */
	pr_debug("[%s][%d] message = %lx,  fdrv message = %lx, bdrv_message = %lx, tlog_message = %lx\n", __func__, __LINE__,
			(unsigned long)virt_to_phys(message_buff),
			(unsigned long)virt_to_phys(fdrv_message_buff),
			(unsigned long)virt_to_phys(bdrv_message_buff),
			(unsigned long)virt_to_phys(tlog_message_buff));

	init_cmdbuf((unsigned long)virt_to_phys(message_buff), (unsigned long)virt_to_phys(fdrv_message_buff),
			(unsigned long)virt_to_phys(bdrv_message_buff), (unsigned long)virt_to_phys(tlog_message_buff));

	return 0;
}


long teei_service_init_first(void)
{
	/**
	 * register interrupt handler
	 */
	/* register_switch_irq_handler(); */
	long retVal = 0;

	pr_debug("[%s][%d] begin to create nq buffer!\n", __func__, __LINE__);

	retVal = create_nq_buffer();
	if (retVal < 0) {
		pr_err("[%s][%d] create nq buffer failed!\n", __func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

/*
	pr_debug("[%s][%d] begin to create fp buffer!\n", __func__, __LINE__);
	fp_buff_addr = create_fp_fdrv(FP_BUFF_SIZE);

	if (soter_error_flag == 1)
		return -1;
*/

	pr_debug("[%s][%d] begin to create keymaster buffer!\n", __func__, __LINE__);
	keymaster_buff_addr = create_keymaster_fdrv(KEYMASTER_BUFF_SIZE);
	if (keymaster_buff_addr == NULL) {
		pr_err("[%s][%d] create keymaster buffer failed!\n", __func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

	pr_debug("[%s][%d] begin to create gatekeeper buffer!\n", __func__, __LINE__);
	gatekeeper_buff_addr = create_gatekeeper_fdrv(GK_BUFF_SIZE);
	if (gatekeeper_buff_addr == NULL) {
		pr_err("[%s][%d] create gatekeeper buffer failed!\n", __func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

	/**
	 * init service handler
	 */
	retVal = init_all_service_handlers();
	if (retVal < 0) {
		pr_err("[%s][%d] init_all_service_handlers failed!\n", __func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;
	/**
	 * start service thread
	 */
	/* start_teei_service(); */

	/**
	 * Create Work Queue
	 */
	/* secure_wq = create_workqueue("Secure Call"); */

	return 0;
}

long teei_service_init_second(void)
{
	pr_debug("[%s][%d] begin to create fp buffer!\n", __func__, __LINE__);
	fp_buff_addr = create_fp_fdrv(FP_BUFF_SIZE);
	if (fp_buff_addr == NULL) {
		pr_err("[%s][%d] create fp buffer failed!\n", __func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

	return 0;
}


/**
 * @brief  init TEEI Framework
 * init Soter OS
 * init Global Schedule
 * init Forward Call Service
 * init CallBack Service
 * @return
 */

static int init_teei_framework(void)
{
	long retVal = 0;
	unsigned long tlog_buff = 0;

	boot_soter_flag = START_STATUS;

	sema_init(&(boot_sema), 0);
	sema_init(&(fdrv_sema), 0);
	sema_init(&(fdrv_lock), 1);
	sema_init(&(api_lock), 1);
	sema_init(&(boot_decryto_lock), 0);
#if 0
	register_boot_irq_handler();
	register_sched_irq_handler();
	register_switch_irq_handler();
	register_soter_irq_handler();
	register_fp_ack_handler();
	/* register_keymaster_ack_handler(); */
	register_bdrv_handler();
	register_tlog_handler();
	register_error_irq_handler();
#else
	register_ut_irq_handler();
	register_soter_irq_handler();
#endif
	tlog_buff = (unsigned long) __get_free_pages(GFP_KERNEL  | GFP_DMA , get_order(ROUND_UP(TLOG_SIZE, SZ_4K)));

	if (tlog_buff == NULL) {
		pr_err("[%s][%d]ERROR: There is no enough memory for TLOG!\n", __func__, __LINE__);
		return -1;
	}

	retVal = create_tlog_thread(tlog_buff, TLOG_SIZE);
	if (retVal != 0) {
		pr_err("[%s][%d]ERROR: Failed to create TLOG thread!\n", __func__, __LINE__);
		return -1;
	}

	secure_wq = create_workqueue("Secure Call");

#ifdef UT_DMA_ZONE
	boot_vfs_addr = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(VFS_SIZE, SZ_4K)));
#else
	boot_vfs_addr = (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(VFS_SIZE, SZ_4K)));
#endif
	if (boot_vfs_addr == NULL) {
		pr_err("[%s][%d]ERROR: There is no enough memory for booting Soter!\n", __func__, __LINE__);
		return -1;
	}

	down(&(smc_lock));

	boot_stage1((unsigned long)virt_to_phys(boot_vfs_addr), (unsigned long)virt_to_phys(tlog_buff));

	down(&(boot_sema));

	free_pages(boot_vfs_addr, get_order(ROUND_UP(VFS_SIZE, SZ_4K)));

	boot_soter_flag = END_STATUS;
	if (soter_error_flag == 1) {
		return -1;
	}

	down(&smc_lock);
	retVal = create_cmd_buff();
	up(&smc_lock);
	if (retVal < 0) {
		pr_err("[%s][%d] create_cmd_buff failed !\n", __func__, __LINE__);
		return retVal;
	}

	pr_debug("[%s][%d] begin to load Soter services.\n", __func__, __LINE__);
	switch_to_t_os_stages2();
	pr_debug("[%s][%d] load Soter services successfully.\n", __func__, __LINE__);

	if (soter_error_flag == 1) {
		return -1;
	}

	pr_debug("[%s][%d] begin to init daulOS services.\n", __func__, __LINE__);
	retVal = teei_service_init_first();
	if (retVal == -1)
		return -1;


	/* waiting for keymaster share memory ready and anable the keymaster IOCTL */
	up(&keymaster_api_lock);

	/* android notify the uTdriver that the TAs is ready !*/
	down(&boot_decryto_lock);
	up(&boot_decryto_lock);

	retVal = teei_service_init_second();
	if (retVal == -1)
		return -1;

	pr_debug("[%s][%d] begin to load TEEs.\n", __func__, __LINE__);
	t_os_load_image();
	if (soter_error_flag == 1)
		return -1;

	pr_debug("[%s][%d] load TEEs successfully.\n", __func__, __LINE__);

	teei_config_flag = 1;

	wake_up(&__fp_open_wq);

	return 0;
}

/**
 * @brief
 *
 * @param	file
 * @param	cmd
 * @param	arg
 *
 * @return
 */

#define TEEI_CONFIG_FULL_PATH_DEV_NAME "/dev/teei_config"
#define TEEI_CONFIG_DEV "teei_config"
#define TEEI_CONFIG_IOC_MAGIC 0x775B777E /* "TEEI Client" */
#define TEEI_CONFIG_IOCTL_INIT_TEEI _IOWR(TEEI_CONFIG_IOC_MAGIC, 3, int)

unsigned int teei_flags = 0;

int is_teei_ready(void)
{
	return teei_flags;
}
EXPORT_SYMBOL(is_teei_ready);

static long teei_config_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int retVal = 0;

	switch (cmd) {

	case TEEI_CONFIG_IOCTL_INIT_TEEI:
			if (teei_flags == 1) {
				break;
			} else {
				init_teei_framework();
				teei_flags = 1;
			}

			break;
	default:
			pr_err("[%s][%d] command not found!\n", __func__, __LINE__);
			retVal = -EINVAL;
	}

	return retVal;
}

/**
 * @brief		The open operation of /dev/teei_config device node.
 *
 * @param		inode
 * @param		file
 *
 * @return		ENOMEM: no enough memory in the linux kernel
 *			0: on success
 */

static int teei_config_open(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 * @brief	Map the vma with the free pages
 *
 * @param	filp
 * @param	vma
 *
 * @return	0: success
 *		EINVAL: Invalid parament
 *		ENOMEM: No enough memory
 */

/*
static int teei_config_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}
*/

/**
 * @brief		The release operation of /dev/teei_config device node.
 *
 * @param		inode: device inode structure
 * @param		file:  struct file
 *
 * @return		0: on success
 */
static int teei_config_release(struct inode *inode, struct file *file)
{
	return 0;
}

static dev_t teei_config_device_no;
static struct cdev teei_config_cdev;
static struct class *config_driver_class;

/**
 * @brief
 */
static const struct file_operations teei_config_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = teei_config_ioctl,
	.open = teei_config_open,
	/* .mmap = teei_config_mmap, */
	.release = teei_config_release
};

/**
 * @brief TEEI Agent Driver initialization
 * initialize Microtrust Tee environment
 * @return
 **/


static int teei_config_init(void)
{
	int retVal = 0;
	struct device *class_dev = NULL;

	retVal = alloc_chrdev_region(&teei_config_device_no, 0, 1, TEEI_CONFIG_DEV);
	if (retVal < 0) {
		pr_err("alloc_chrdev_region failed %x.\n", retVal);
		return retVal;
	}

	config_driver_class = class_create(THIS_MODULE, TEEI_CONFIG_DEV);
	if (IS_ERR(config_driver_class)) {
		retVal = -ENOMEM;
		pr_err("class_create failed %x\n", retVal);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(config_driver_class, NULL, teei_config_device_no, NULL, TEEI_CONFIG_DEV);
	if (NULL == class_dev) {
		pr_err("class_device_create failed %x\n", retVal);
		retVal = -ENOMEM;
		goto class_destroy;
	}

	cdev_init(&teei_config_cdev, &teei_config_fops);
	teei_config_cdev.owner = THIS_MODULE;

	retVal = cdev_add(&teei_config_cdev, MKDEV(MAJOR(teei_config_device_no), 0), 1);
	if (retVal < 0) {
		pr_err("cdev_add failed %x\n", retVal);
		goto class_device_destroy;
	}

	goto return_fn;

class_device_destroy:
	device_destroy(driver_class, teei_config_device_no);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(teei_config_device_no, 1);
return_fn:
	return retVal;
}


/* =========================================================================================== */

/**
 * @brief
 *
 * @param file
 * @param cmd
 * @param arg
 *
 * @return
 */
static long teei_client_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int retVal = 0;
	void *argp = (void __user *) arg;

	if (teei_config_flag == 0) {
		pr_err("Error: soter is NOT ready, Can not support IOCTL!\n");
		return -ECANCELED;
	}
	down(&api_lock);
	mutex_lock(&pm_mutex);
	switch (cmd) {

	case TEEI_CLIENT_IOCTL_INITCONTEXT_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_context_init(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed init context %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_CLOSECONTEXT_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_context_close(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed close context: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_SES_INIT_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_session_init(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed session init: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_SES_OPEN_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_session_open(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed session open: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN end .....\n", __func__, __LINE__);
#endif
			break;


	case TEEI_CLIENT_IOCTL_SES_CLOSE_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_session_close(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed session close: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_OPERATION_RELEASE:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_OPERATION_RELEASE beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_operation_release(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed operation release: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_OPERATION_RELEASE end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_SEND_CMD_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SEND_CMD beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_send_cmd(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed send cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SEND_CMD end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_GET_DECODE_TYPE:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_get_decode_type(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed decode cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_ENC_UINT32:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_encode_uint32(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_DEC_UINT32:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_decode_uint32(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_decode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_encode_array(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_decode_array_space(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_decode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_ENC_MEM_REF:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_encode_mem_ref(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_encode_mem_ref(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_shared_mem_alloc(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_shared_mem_alloc: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_FREE_REQ:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_client_shared_mem_free(file->private_data, argp);
			if (retVal != 0)
				pr_err("[%s][%d] failed teei_client_shared_mem_free: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE end .....\n", __func__, __LINE__);
#endif
			break;

	case TEEI_GET_TEEI_CONFIG_STAT:

#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT beginning .....\n", __func__, __LINE__);
#endif
			retVal = teei_config_flag;
#ifdef UT_DEBUG
			pr_debug("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT end .....\n", __func__, __LINE__);
#endif
			break;

	default:
			pr_err("[%s][%d] command not found!\n", __func__, __LINE__);
			retVal = -EINVAL;
	}
	mutex_unlock(&pm_mutex);
	up(&api_lock);
	return retVal;
}

/**
 * @brief
 * @fn teei_client_unioctl is used for 64bit system
 */
static long teei_client_unioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	/* pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT chengxin test unioctl11111.....\n", __func__, __LINE__); */
	int retVal = 0;
	void *argp = (void __user *) arg;

	if (teei_config_flag == 0) {
		pr_err("Error: soter is NOT ready, Can not support IOCTL!\n");
		return -ECANCELED;
	}
	down(&api_lock);
	mutex_lock(&pm_mutex);
	switch (cmd) {

	case TEEI_CLIENT_IOCTL_INITCONTEXT_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_context_init(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed init context %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_CLOSECONTEXT_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT beginning.....\n", __func__, __LINE__);
#endif
		retVal = teei_client_context_close(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed close context: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SES_INIT_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_session_init(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed session init: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SES_OPEN_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_session_open(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed session open: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN end .....\n", __func__, __LINE__);
#endif
		break;


	case TEEI_CLIENT_IOCTL_SES_CLOSE_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_session_close(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed session close: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_OPERATION_RELEASE:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_OPERATION_RELEASE beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_operation_release(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed operation release: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_OPERATION_RELEASE end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SEND_CMD_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SEND_CMD beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_send_cmd(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed send cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d]TEEI_CLIENT_IOCTL_SEND_CMD end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_GET_DECODE_TYPE:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_get_decode_type(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed decode cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_UINT32:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_encode_uint32_64bit(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_DEC_UINT32:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_decode_uint32(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_decode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_encode_array_64bit(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_decode_array_space(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_decode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_MEM_REF:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_encode_mem_ref_64bit(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF end  .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_encode_mem_ref_64bit(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_encode_cmd: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE beginning  .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_shared_mem_alloc(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_shared_mem_alloc: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_FREE_REQ:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE beginning .....\n", __func__, __LINE__);
#endif
		retVal = teei_client_shared_mem_free(file->private_data, argp);
		if (retVal != 0)
			pr_err("[%s][%d] failed teei_client_shared_mem_free: %x.\n", __func__, __LINE__, retVal);
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE end .....\n", __func__, __LINE__);
#endif
		break;

	case TEEI_GET_TEEI_CONFIG_STAT:

#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT beginning.....\n", __func__, __LINE__);
#endif
		retVal = teei_config_flag;
#ifdef UT_DEBUG
		pr_debug("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT end .....\n", __func__, __LINE__);
#endif
		break;

	default:
		pr_err("[%s][%d] command not found!\n", __func__, __LINE__);
		retVal = -EINVAL;
	}
	mutex_unlock(&pm_mutex);
	up(&api_lock);
	return retVal;
}


/**
 * @brief		The open operation of /dev/teei_client device node.
 *
 * @param inode
 * @param file
 *
 * @return		ENOMEM: no enough memory in the linux kernel
 *			0: on success
 */

static int teei_client_open(struct inode *inode, struct file *file)
{
	struct teei_context *new_context = NULL;

	device_file_cnt++;
	file->private_data = (void *)device_file_cnt;

	new_context = (struct teei_context *)tz_malloc(sizeof(struct teei_context), GFP_KERNEL);
	if (new_context == NULL) {
		pr_err("tz_malloc failed for new dev file allocation!\n");
		return -ENOMEM;
	}

	new_context->cont_id = device_file_cnt;
	INIT_LIST_HEAD(&(new_context->sess_link));
	INIT_LIST_HEAD(&(new_context->link));

	new_context->shared_mem_cnt = 0;
	INIT_LIST_HEAD(&(new_context->shared_mem_list));

	sema_init(&(new_context->cont_lock), 0);

	down_write(&(teei_contexts_head.teei_contexts_sem));
	list_add(&(new_context->link), &(teei_contexts_head.context_list));
	teei_contexts_head.dev_file_cnt++;
	up_write(&(teei_contexts_head.teei_contexts_sem));

	return 0;
}

/**
 * @brief	Map the vma with the free pages
 *
 * @param filp
 * @param vma
 *
 * @return	0: success
 *		EINVAL: Invalid parament
 *		ENOMEM: No enough memory
 */
static int teei_client_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int retVal = 0;
	struct teei_shared_mem *share_mem_entry = NULL;
	int context_found = 0;
	unsigned long alloc_addr = 0;
	struct teei_context *cont = NULL;
	long length = vma->vm_end - vma->vm_start;

	/* Reasch the context with ID equal filp->private_data */

	down_read(&(teei_contexts_head.teei_contexts_sem));

	list_for_each_entry(cont, &(teei_contexts_head.context_list), link) {
		if (cont->cont_id == (unsigned long)filp->private_data) {
			context_found = 1;
			break;
		}

	}

	if (context_found == 0) {
		up_read(&(teei_contexts_head.teei_contexts_sem));
		return -EINVAL;
	}

	/* Alloc one teei_share_mem structure */
	share_mem_entry = tz_malloc(sizeof(struct teei_shared_mem), GFP_KERNEL);
	if (share_mem_entry == NULL) {
		pr_err("[%s][%d] tz_malloc failed!\n", __func__, __LINE__);
		up_read(&(teei_contexts_head.teei_contexts_sem));
		return -ENOMEM;
	}

	/* Get free pages from Kernel. */
#ifdef UT_DMA_ZONE
	alloc_addr =  (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(length, SZ_4K)));
#else
	alloc_addr =  (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(length, SZ_4K)));
#endif
	if (alloc_addr == 0) {
		pr_err("[%s][%d] get free pages failed!\n", __func__, __LINE__);
		kfree(share_mem_entry);
		up_read(&(teei_contexts_head.teei_contexts_sem));
		return -ENOMEM;
	}

	vma->vm_flags = vma->vm_flags | VM_IO;

	/* Remap the free pages to the VMA */
	retVal = remap_pfn_range(vma, vma->vm_start, ((virt_to_phys((void *)alloc_addr)) >> PAGE_SHIFT),
			length, vma->vm_page_prot);

	if (retVal) {
		pr_err("[%s][%d] remap_pfn_range failed!\n", __func__, __LINE__);
		kfree(share_mem_entry);
		free_pages(alloc_addr, get_order(ROUND_UP(length, SZ_4K)));
		up_read(&(teei_contexts_head.teei_contexts_sem));
		return retVal;
	}

	/* Add the teei_share_mem into the teei_context struct */
	share_mem_entry->k_addr = (void *)alloc_addr;
	share_mem_entry->len = length;
	share_mem_entry->u_addr = (void *)vma->vm_start;
	share_mem_entry->index = share_mem_entry->u_addr;

	cont->shared_mem_cnt++;
	list_add_tail(&(share_mem_entry->head), &(cont->shared_mem_list));

	up_read(&(teei_contexts_head.teei_contexts_sem));

	return 0;
}

/**
 * @brief		The release operation of /dev/teei_client device node.
 *
 * @param		inode: device inode structure
 * @param		file:  struct file
 *
 * @return		0: on success
 */
static int teei_client_release(struct inode *inode, struct file *file)
{
	int retVal = 0;

	retVal = teei_client_service_exit(file->private_data);

	return retVal;
}

/**
 * @brief
 */
static const struct file_operations teei_client_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = teei_client_unioctl,
	.compat_ioctl = teei_client_ioctl,
	.open = teei_client_open,
	.mmap = teei_client_mmap,
	.release = teei_client_release
};

/**
 * @brief TEEI Agent Driver initialization
 * initialize service framework
 * @return
 */
static int teei_client_init(void)
{
	int ret_code = 0;
	long retVal = 0;
	struct device *class_dev = NULL;
	int i;
	long prior = 0;

	unsigned long irq_status = 0;

	unsigned long tmp_buff = 0;

	/* pr_debug("TEEI Agent Driver Module Init ...\n"); */

	pr_info("=============================================================\n\n");
	pr_info("~~~~~~~uTos version [%s]~~~~~~~\n", UTOS_VERSION);
	pr_info("=============================================================\n\n");

	ret_code = alloc_chrdev_region(&teei_client_device_no, 0, 1, TEEI_CLIENT_DEV);
	if (ret_code < 0) {
		pr_err("alloc_chrdev_region failed %x\n", ret_code);
		return ret_code;
	}

	driver_class = class_create(THIS_MODULE, TEEI_CLIENT_DEV);
	if (IS_ERR(driver_class)) {
		ret_code = -ENOMEM;
		pr_err("class_create failed %x\n", ret_code);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, teei_client_device_no, NULL, TEEI_CLIENT_DEV);
	if (NULL == class_dev) {
		pr_err("class_device_create failed %x\n", ret_code);
		ret_code = -ENOMEM;
		goto class_destroy;
	}

	cdev_init(&teei_client_cdev, &teei_client_fops);
	teei_client_cdev.owner = THIS_MODULE;

	ret_code = cdev_add(&teei_client_cdev, MKDEV(MAJOR(teei_client_device_no), 0), 1);
	if (ret_code < 0) {
		pr_err("cdev_add failed %x\n", ret_code);
		goto class_device_destroy;
	}

	memset(&teei_contexts_head, 0, sizeof(teei_contexts_head));

	teei_contexts_head.dev_file_cnt = 0;
	init_rwsem(&teei_contexts_head.teei_contexts_sem);

	INIT_LIST_HEAD(&teei_contexts_head.context_list);

	init_tlog_entry();

	sema_init(&(smc_lock), 1);

	for_each_online_cpu(i) {
		current_cpu_id = i;
		pr_debug("init stage : current_cpu_id = %d\n", current_cpu_id);
	}

	pr_debug("begin to create sub_thread.\n");

#if 0
	sub_pid = kernel_thread(global_fn, NULL, CLONE_KERNEL);
	retVal = sys_setpriority(PRIO_PROCESS, sub_pid, -3);
#endif

	/* struct sched_param param = {.sched_priority = -20 }; */
	teei_fastcall_task = kthread_create(global_fn, NULL, "teei_fastcall_thread");
	if (IS_ERR(teei_fastcall_task)) {
		pr_err("create fastcall thread failed: %ld\n", PTR_ERR(teei_fastcall_task));
		goto fastcall_thread_fail;
	}

	/* sched_setscheduler_nocheck(teei_fastcall_task, SCHED_NORMAL, &param); */
	/* get_task_struct(teei_fastcall_task); */
	wake_up_process(teei_fastcall_task);

	/* create the switch thread */
	teei_switch_task = kthread_create(kthread_worker_fn, &ut_fastcall_worker, "teei_switch_thread");
	if (IS_ERR(teei_switch_task)) {
		pr_err("create switch thread failed: %ld\n", PTR_ERR(teei_switch_task));
		teei_switch_task = NULL;
		goto fastcall_thread_fail;
	}

	/* sched_setscheduler_nocheck(teei_switch_task, SCHED_NORMAL, &param); */
	/* get_task_struct(teei_switch_task); */
	wake_up_process(teei_switch_task);

	cpumask_set_cpu(get_current_cpuid(), &mask);
	set_cpus_allowed(teei_switch_task, mask);

	pr_debug("create the sub_thread successfully!\n");

	register_cpu_notifier(&tz_driver_cpu_notifer);

	pr_debug("after  register cpu notify\n");

	teei_config_init();

	goto return_fn;

fastcall_thread_fail:
class_device_destroy:
	device_destroy(driver_class, teei_client_device_no);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(teei_client_device_no, 1);
return_fn:
	return ret_code;
}

/**
 * @brief
 */
static void teei_client_exit(void)
{
	pr_debug("teei_client exit\n");

	device_destroy(driver_class, teei_client_device_no);
	class_destroy(driver_class);
	unregister_chrdev_region(teei_client_device_no, 1);
}


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("TEEI <www.microtrust.com>");
MODULE_DESCRIPTION("TEEI Agent");
MODULE_VERSION("1.00");

module_init(teei_client_init);

module_exit(teei_client_exit);


