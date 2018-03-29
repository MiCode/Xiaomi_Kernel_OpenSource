#ifndef __TEEI_DEBUG_H_
#define __TEEI_DEBUG_H_

/* #define TEEI_DEBUG */
/* #define TEEI_INFO */

#undef TDEBUG
#undef TINFO
#undef TERR

#define TZDebug(fmt, args...) pr_info("\033[;34m[TZDriver]"fmt"\033[0m\n", ##args)
#ifdef TEEI_DEBUG
/*
#define TDEBUG(fmt, args...) pr_info("%s(%i, %s): " fmt "\n", \
	__func__, current->pid, current->comm, ##args)
*/
#define TDEBUG(fmt, args...) pr_info("tz driver"fmt"\n", ##args)
#else
#define TDEBUG(fmt, args...)
#endif

#ifdef TEEI_INFO
#define TINFO(fmt, args...) pr_info("%s(%i, %s): " fmt "\n", \
	__func__, current->pid, current->comm, ##args)
#else
#define TINFO(fmt, args...)
#endif

#define TERR(fmt, args...) pr_err("%s(%i, %s): " fmt "\n", \
	__func__, current->pid, current->comm, ##args)


#endif
