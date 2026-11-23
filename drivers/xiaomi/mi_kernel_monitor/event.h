#ifndef __EVENT_H__
#define __EVENT_H__

#include <linux/sched.h>

#define ROOT_UID               0
#define SYSTEM_UID             1000
#define FIRST_APPLICATION_UID  10000
#define LAST_APPLICATION_UID   19999
#define PER_USER_RANGE         100000

#define LAT_CNT_MAX             (1024)
#define DROP_FRAME_CNT_MAX      (512)
#define PER_CPU_SCHED_LAT_COUNT (64)
#define TASK_IOWAIT             0x00000003
#define TASK_OTHER              0x00000004

struct mi_task_event_buffer_info {
	unsigned char *percpu_addr[NR_CPUS];
};

struct mi_event_info {
	u32 max_num;
	u32 field_len;
	u32 rear;
	u32 is_full;
	////u8 data[1];
};

enum KERN_LAT_TYPE
{
    MUTEX_WAIT_LAT=0,             //mutex等待时间长
    RWSEM_READ_WAIT_LAT,          //rwsem读锁等待时间长
    RWSEM_WRITE_WAIT_LAT,         //rwsem写锁等待时间长
    FUTEX_WAIT_LAT,               //futex等待时间长
    SCHED_LAT,                    //白名单线程runnable时间长，即调度延迟长
    IRQ_OFF_LAT,                  //中断关闭时间长, 表现为非RT线程运行时间长, kswapd/kcompactd
    RT_RUNNING_LAT,               //RT延迟长
    PREEMPT_OFF_LAT,              //抢占关闭时间长, 表现为非RT线程运行时间长, kswapd/kcompactd    
    IO_WAIT_LAT,
    DSTATE_LAT,
    BINDER_LAT,
    LAT_TYPE_MAX,
};

enum SCENE_TYPE
{
    APP_LAUNCH=0,
    APP_EXIT,
    SLIDE,
    CAMERA,
    ANIM,
    INPUT,
    VIDEO,
    GAME,
    SCENE_TYPE_MAX,
};

#define    LOCK_WAIT_LAT_THRESHOLD                (50*1000*1000)
#define    SCHED_LAT_THRESHOLD                    (50*1000*1000)
#define    RT_RUNNING_LAT_THRESHOLD               (50*1000*1000)
#define    PREEMPT_OFF_LAT_THRESHOLD              (50*1000*1000)
#define    IO_WAIT_LAT_THRESHOLD                  (50*1000*1000)
#define    DSTATE_LAT_THRESHOLD                   (50*1000*1000)
#define    IRQ_OFF_LAT_THRESHOLD                  (50*1000*1000)
#define    BINDER_LAT_THRESHOLD                   (50*1000*1000)
#define    ENQUEUE_INTERNAL_THRESHOLD             (16*1000*1000)

struct KERN_EVENT_INFO
{
	u8 lat_type;
    u8 grp;
    int delay;
    int  pid;
    ////int  prio;
    ////int cpu;
    enum SCENE_TYPE  scene_type;   
    char comm[TASK_COMM_LEN];
    char proc[TASK_COMM_LEN];
    u64 timestamp;
};

struct KERN_LAT_INFO
{
    unsigned int cnt;
	struct KERN_EVENT_INFO kern_lat_info[LAT_CNT_MAX];
};

struct FRAMEMISS_STAT_INFO
{
    int count;
    enum SCENE_TYPE scene_types[DROP_FRAME_CNT_MAX]; 
    u64 timestamp[DROP_FRAME_CNT_MAX];
    ////pid_t ui_pid[DROP_FRAME_CNT_MAX];
    ////pid_t render_pid[DROP_FRAME_CNT_MAX];
    u8 is_match[DROP_FRAME_CNT_MAX];
};

struct mi_task_switch_info {
    int cpu;
    int cpu_avg_load;    

    int pprev_time;
    int pprev_type; 

	int prev_pid;
    int prev_prio; 
    char prev_comm[TASK_COMM_LEN];
    char prev_proc[TASK_COMM_LEN];
    int prev_runtime;
    u8 prev_type;    //1:rt task, 2:vip task

    int cur_pid;
    int cur_prio;
    int cur_delay;
    u8 cur_type;
    char cur_comm[TASK_COMM_LEN];
    char cur_proc[TASK_COMM_LEN];
};

struct mi_sched_data {
    int count;
    struct mi_task_switch_info task_switch_info[PER_CPU_SCHED_LAT_COUNT];
};

int mi_task_event_init(void);
int mi_task_event_exit(void);
void mi_trace_event_write(int event_type, struct task_struct *curr_task, struct task_struct *task);
void mi_trace_sched_record(struct task_struct* curr_task, struct task_struct* task, int event_type);
bool is_critical_ux_task(struct task_struct* task);
void record_kern_lat_event(u8 type, int delay, struct task_struct *task);
bool is_screen_off(void);
int get_task_lock_cnt(struct task_struct* task);

#endif  /* endif */
