/*
  boot logger: drivers/misc/mtprof/bootprof
  interface: /proc/bootprof
*/
#ifdef CONFIG_SCHEDSTATS
extern void log_boot(char *str);
#else
static inline void log_boot(char *str)
{
}
#endif
