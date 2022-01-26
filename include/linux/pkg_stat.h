#ifdef CONFIG_MIGT
enum MIGT_TASK_TYPE {
	MIGT_NORMAL_TASK,
	MI_VIP_TASK,
	GAME_TASK_LEVELS
};
#define MASK_MI_VTASK	(1 << MI_VIP_TASK)

struct package_runtime_info {
	struct {
		enum MIGT_TASK_TYPE flag;
		unsigned long boost_end;
	} migt;
};

int game_vip_task(struct task_struct *tsk);
void migt_monitor_init(struct task_struct *p);

#else
static inline int game_vip_task(struct task_struct *tsk) {return 0;}
static inline void migt_monitor_init(struct task_struct *p) {}
#endif
