#ifndef _AIC3XXX_VERBOSE_H_
#define _AIC3XXX_VERBOSE_H_

enum aic3xxx_verbose_enum {
	AIC3XXX_NO_PRINT = 0,	/* No debug prints */
	AIC3XXX_ALERT,		/* Init functions can use this level */
	AIC3XXX_CRIT,		/* vebosity levels for warning messages */
	AIC3XXX_ERR,		/* if/else/switch case decisions */
	AIC3XXX_WARNING,	/* function entry/exit */
	AIC3XXX_NOTICE,		/* upper level codec read/write */
	AIC3XXX_INFO,		/* low level i2c read/write */
	AIC3XXX_DEBUG,		/* verbose level for bottom halves */
	AIC3XXX_ENUM_MAX
};

/* Defined in sound/soc/codecs/tlv320aic326x.c */
extern unsigned long debug_level;

#define __aic3xxx_debug(PRIORITY, FORMAT, ARGV...) \
do { \
	if (debug_level > 0) { \
		if ((debug_level >= PRIORITY) && \
				(PRIORITY > AIC3XXX_NO_PRINT)) \
			printk(FORMAT, ##ARGV); \
	} \
} while (0)

#ifdef CONFIG_AIC3XXX_VERBOSE
#define aic3xxx_print(PRIORITY, FORMAT, ARGV...) \
	__aic3xxx_debug(PRIORITY, FORMAT, ##ARGV)
#else
#define aic3xxx_print(PRIORITY, FORMAT, ARGV...)
#endif

#define INIT_PRINT(FORMAT, ARGV...) \
	aic3xxx_print(AIC3XXX_ALERT, FORMAT, ##ARGV)

#define WARN_PRINT(FORMAT, ARGV...) \
	aic3xxx_print(AIC3XXX_CRIT, FORMAT, ##ARGV)

#define DECISION_PRINT(FORMAT, ARGV...) \
	aic3xxx_print(AIC3XXX_ERR, FORMAT, ##ARGV)

#define FUNCTION_ENTRY_PRINT() \
	aic3xxx_print(AIC3XXX_WARNING, "%s Entered\n", __func__)

#define FUNCTION_EXIT_PRINT() \
	aic3xxx_print(AIC3XXX_WARNING, "Line=%d, %s Exit\n", __LINE__, __func__)

#define CODEC_PRINT(FORMAT, ARGV...) \
	aic3xxx_print(AIC3XXX_NOTICE, FORMAT, ##ARGV)

#define LVL_MFD_PRINT(FORMAT, ARGV...) \
	aic3xxx_print(AIC3XXX_INFO, FORMAT, ##ARGV)

#define BTM_HALF_PRINT(FORMAT, ARGV...) \
	aic3xxx_print(AIC3XXX_DEBUG, FORMAT, ##ARGV)

#endif /* _AIC3XXX_VERBOSE_H_ */
