#ifndef MET_DRV
#define MET_DRV

#include <linux/version.h>
#include <linux/device.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>

extern int met_mode;

#define MET_MODE_TRACE_CMD_OFFSET	(1)
#define MET_MODE_TRACE_CMD			(1<<MET_MODE_TRACE_CMD_OFFSET)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define MET_PRINTK(FORMAT, args...)							\
	do { 													\
		if (met_mode & MET_MODE_TRACE_CMD)					\
			trace_printk("%s: " FORMAT, __func__, ##args);	\
		else												\
			trace_printk(FORMAT, ##args);					\
	} while (0)
#else
#define MET_STRBUF_SIZE		1024
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_nmi);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_irq);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_sirq);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf);

#ifdef CONFIG_FTRACE
#define TRACE_PUTS(p) \
	do { \
		trace_puts(pmet_strbuf); \
	} while (0)
#else
#define TRACE_PUTS(p) do {} while (0)
#endif

#define MET_PRINTK(FORMAT, args...) \
	do { \
		char *pmet_strbuf; \
		preempt_disable(); \
		if (in_nmi()) \
			pmet_strbuf = per_cpu(met_strbuf_nmi, smp_processor_id()); \
		else if (in_irq()) \
			pmet_strbuf = per_cpu(met_strbuf_irq, smp_processor_id()); \
		else if (in_softirq()) \
			pmet_strbuf = per_cpu(met_strbuf_sirq, smp_processor_id()); \
		else \
			pmet_strbuf = per_cpu(met_strbuf, smp_processor_id()); \
		if (met_mode & MET_MODE_TRACE_CMD) \
			snprintf(pmet_strbuf, MET_STRBUF_SIZE, "%s: " FORMAT, __func__, ##args); \
		else \
			snprintf(pmet_strbuf, MET_STRBUF_SIZE, FORMAT, ##args); \
		TRACE_PUTS(pmet_strbuf); \
		preempt_enable_no_resched(); \
	} while (0)
#endif

#define MET_FTRACE_PRINTK(TRACE_NAME, args...)			\
	do {							\
		trace_##TRACE_NAME(args);			\
	} while (0)


#define MET_TYPE_PMU	1
#define MET_TYPE_BUS	2
#define MET_TYPE_MISC	3

struct metdevice {

	struct list_head list;
	int type;
	const char *name;
	struct module *owner;
	struct kobject *kobj;

	int (*create_subfs) (struct kobject *parent);
	void (*delete_subfs) (void);
	int mode;
	int cpu_related;
	int polling_interval;
	int polling_count_reload;
	int __percpu *polling_count;
	void (*start) (void);
	void (*stop) (void);
	int (*reset) (void);
	void (*timed_polling) (unsigned long long stamp, int cpu);
	void (*tagged_polling) (unsigned long long stamp, int cpu);
	int (*print_help) (char *buf, int len);
	int (*print_header) (char *buf, int len);
	int (*process_argument) (const char *arg, int len);

	struct list_head exlist;	/* for linked list before register */
	void *reversed1;
};

int met_register(struct metdevice *met);
int met_deregister(struct metdevice *met);
int met_set_platform(const char *plf_name, int flag);
int met_set_topology(const char *topology_name, int flag);
int met_devlink_add(struct metdevice *met);
int met_devlink_del(struct metdevice *met);
int met_devlink_register_all(void);
int met_devlink_deregister_all(void);

int fs_reg(void);
void fs_unreg(void);


/******************************************************************************
 * Tracepoints
 ******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#	error Kernels prior to 2.6.32 not supported
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
#	define MET_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(PARAMS(proto))
#	define MET_REGISTER_TRACE(probe_name) \
		register_trace_##probe_name(probe_##probe_name)
#	define MET_UNREGISTER_TRACE(probe_name) \
		unregister_trace_##probe_name(probe_##probe_name)
#else
#	define MET_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(void *data, PARAMS(proto))
#	define MET_REGISTER_TRACE(probe_name) \
		register_trace_##probe_name(probe_##probe_name, NULL)
#	define MET_UNREGISTER_TRACE(probe_name) \
		unregister_trace_##probe_name(probe_##probe_name, NULL)
#endif



/* ====================== Tagging API ================================ */

#define MAX_EVENT_CLASS	31
#define MAX_TAGNAME_LEN	128
#define MET_CLASS_ALL	0x80000000

/* IOCTL commands of MET tagging */
typedef struct _mtag_cmd_t {
	unsigned int class_id;
	unsigned int value;
	unsigned int slen;
	char tname[MAX_TAGNAME_LEN];
	void *data;
	unsigned int size;
} mtag_cmd_t;

#define TYPE_START		1
#define TYPE_END		2
#define TYPE_ONESHOT	3
#define TYPE_ENABLE		4
#define TYPE_DISABLE	5
#define TYPE_REC_SET	6
#define TYPE_DUMP		7
#define TYPE_DUMP_SIZE	8
#define TYPE_DUMP_SAVE	9
#define TYPE_USRDATA	10

/* Use 'm' as magic number */
#define MTAG_IOC_MAGIC  'm'
/* Please use a different 8-bit number in your code */
#define MTAG_CMD_START		_IOW(MTAG_IOC_MAGIC, TYPE_START, mtag_cmd_t)
#define MTAG_CMD_END		_IOW(MTAG_IOC_MAGIC, TYPE_END, mtag_cmd_t)
#define MTAG_CMD_ONESHOT	_IOW(MTAG_IOC_MAGIC, TYPE_ONESHOT, mtag_cmd_t)
#define MTAG_CMD_ENABLE		_IOW(MTAG_IOC_MAGIC, TYPE_ENABLE, int)
#define MTAG_CMD_DISABLE	_IOW(MTAG_IOC_MAGIC, TYPE_DISABLE, int)
#define MTAG_CMD_REC_SET	_IOW(MTAG_IOC_MAGIC, TYPE_REC_SET, int)
#define MTAG_CMD_DUMP		_IOW(MTAG_IOC_MAGIC, TYPE_DUMP, mtag_cmd_t)
#define MTAG_CMD_DUMP_SIZE	_IOWR(MTAG_IOC_MAGIC, TYPE_DUMP_SIZE, int)
#define MTAG_CMD_DUMP_SAVE	_IOW(MTAG_IOC_MAGIC, TYPE_DUMP_SAVE, mtag_cmd_t)
#define MTAG_CMD_USRDATA         _IOW(MTAG_IOC_MAGIC, TYPE_USRDATA, mtag_cmd_t)

/* include file */
#ifndef MET_USER_EVENT_SUPPORT
#define met_tag_init() (0)

#define met_tag_uninit() (0)

#define met_tag_start(id, name) (0)

#define met_tag_end(id, name) (0)

#define met_tag_oneshot(id, name, value) (0)

#define met_tag_userdata(pData) (0)

#define met_tag_dump(id, name, data, length) (0)

#define met_tag_disable(id) (0)

#define met_tag_enable(id) (0)

#define met_set_dump_buffer(size) (0)

#define met_save_dump_buffer(pathname) (0)

#define met_save_log(pathname) (0)

#define met_record_on() (0)

#define met_record_off() (0)

#define met_show_bw_limiter() (0)
#define met_reg_bw_limiter() (0)

#else
#include <linux/kernel.h>
int __attribute__((weak)) met_tag_init(void);

int __attribute__((weak)) met_tag_uninit(void);

int __attribute__((weak)) met_tag_start(unsigned int class_id,
					const char *name);

int __attribute__((weak)) met_tag_end(unsigned int class_id,
					const char *name);

int __attribute__((weak)) met_tag_oneshot(unsigned int class_id,
					const char *name,
					unsigned int value);

int met_tag_userdata(char * pData);

int __attribute__((weak)) met_tag_dump(unsigned int class_id,
					const char *name,
					void *data,
					unsigned int length);

int __attribute__((weak)) met_tag_disable(unsigned int class_id);

int __attribute__((weak)) met_tag_enable(unsigned int class_id);

int __attribute__((weak)) met_set_dump_buffer(int size);

int __attribute__((weak)) met_save_dump_buffer(const char *pathname);

int __attribute__((weak)) met_save_log(const char *pathname);

int __attribute__((weak)) met_show_bw_limiter(void);
int __attribute__((weak)) met_reg_bw_limiter(void *fp);

#define met_record_on()		tracing_on()

#define met_record_off()	tracing_off()

#endif				/* MET_USER_EVENT_SUPPORT */

#endif				/* MET_DRV */
