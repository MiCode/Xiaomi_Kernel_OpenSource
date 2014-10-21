#include <linux/ioctl.h>
struct entry_pid_viewid{
unsigned long long int pid;
unsigned long long int viewid;
};

#define VIDT_MAGIC_NUMBER   255
#define VIDT_PID_VIEW_MAP       _IOW(VIDT_MAGIC_NUMBER,1,struct entry_pid_viewid)
#define VIDT_PID_VIEW_UNMAP     _IOW(VIDT_MAGIC_NUMBER,2,struct entry_pid_viewid)
#define VIDT_REGISTER           _IO(VIDT_MAGIC_NUMBER,3)
#define VIDT_REGISTER_HP_PID    _IOW(VIDT_MAGIC_NUMBER,4,pid_t)
