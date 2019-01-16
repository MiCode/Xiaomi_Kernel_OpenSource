#include <generated/autoconf.h>
#include <linux/bug.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/xlog.h>
#include "xlog_internal.h"


int __xlog_output(int level, const char *tag, const char *fmt, va_list args)
{
	int r;
	const char *level_str;
	char printk_string[1024];
	int plen;

	if (!tag)
		return -1;

#ifdef CONFIG_HAVE_XLOG_FEATURE
	if (!xLog_isOn(tag, level))
		return -1;
#endif
	switch (level) {
	case ANDROID_LOG_VERBOSE:
		level_str = KERN_DEBUG;
		break;

	case ANDROID_LOG_DEBUG:
		level_str = KERN_INFO;
		break;

	case ANDROID_LOG_INFO:
		level_str = KERN_NOTICE;
		break;

	case ANDROID_LOG_WARN:
		level_str = KERN_WARNING;
		break;

	case ANDROID_LOG_ERROR:
		level_str = KERN_ERR;
		break;

	case ANDROID_LOG_FATAL:
		level_str = KERN_EMERG;
		break;

	default:
		level_str = KERN_INFO;
		break;
	}

	/* printk("%s[%s] ", level_str, tag); */

	/* r = vprintk(fmt, args); */
	plen = snprintf(printk_string, sizeof(printk_string) - 1, "%s[%s] ", level_str, tag);
	vsnprintf(printk_string + plen, sizeof(printk_string) - plen - 1, fmt, args);

	r = printk("%s", printk_string);

	if (level == ANDROID_LOG_FATAL) {
		BUG();
	}
	return r;
}

asmlinkage int __xlog_printk(const struct xlog_record *rec, ...)
{
	va_list args;
	int r;
	va_start(args, rec);
	r = __xlog_output(rec->prio, rec->tag_str, rec->fmt_str, args);
	va_end(args);

	return r;
}

int __xlog_ale_printk(int prio, const struct ale_convert *convert, ...)
{
	if (convert->fmt_ptr != NULL) {
		va_list args;
		int r;
		va_start(args, convert);
		r = __xlog_output(prio, convert->tag_str, convert->fmt_ptr, args);
		va_end(args);
		return r;
	}
	return 0;
}


EXPORT_SYMBOL(__xlog_printk);
EXPORT_SYMBOL(__xlog_ale_printk);
