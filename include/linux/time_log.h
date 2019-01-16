#include <linux/sched.h>
#include <linux/types.h>

extern bool printk_disable_uart;
#if 1
#define TIME_LOG_START() \
    { \
    	 unsigned long long _start_time = 0; \
    	 unsigned long long _end_time = 0; \
    	 unsigned long long _dur_time = 0; \
        if(printk_disable_uart == 0){ \
        _start_time = sched_clock(); \
        } \
        do { } while(0)

#define TIME_LOG_END(X...) \
				if(printk_disable_uart == 0){ \
        _end_time = sched_clock(); \
        _dur_time = _end_time - _start_time; \
        printk(KERN_ERR X); \
        printk(KERN_ERR"  s:%llu e:%llu d:%llu\n",  \
            _start_time, _end_time, _dur_time ); \
         if(_dur_time > 100000000){ \
        printk(KERN_ERR"warning init time too long!\n");} \
        } \
    } \
    do { } while(0)
#else
#define TIME_LOG_START()  do{} while(0)
#define TIME_LOG_END(X...) do{} while(0)
#endif