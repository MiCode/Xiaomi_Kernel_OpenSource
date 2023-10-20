#ifndef __AW882XX_LOG_H__
#define __AW882XX_LOG_H__

/********************************************
 * print information control
 *******************************************/
#define aw_dev_err(dev, format, ...) \
	do { \
		pr_err("[Awinic][%s]%s: " format "\n", dev_name(dev), __func__, ##__VA_ARGS__); \
	} while (0)
#define aw_dev_info(dev, format, ...) \
	do { \
		pr_info("[Awinic][%s]%s: " format "\n", dev_name(dev), __func__, ##__VA_ARGS__); \
	} while (0)
#define aw_dev_dbg(dev, format, ...) \
	do { \
		pr_debug("[Awinic][%s]%s: " format "\n", dev_name(dev), __func__, ##__VA_ARGS__); \
	} while (0)

#define aw_pr_err(format, ...) \
		do { \
			pr_err("[Awinic]%s: " format "\n", __func__, ##__VA_ARGS__); \
		} while (0)
#define aw_pr_info(format, ...) \
		do { \
			pr_info("[Awinic]%s: " format "\n", __func__, ##__VA_ARGS__); \
		} while (0)
#define aw_pr_dbg(format, ...) \
		do { \
			pr_debug("[Awinic]%s: " format "\n", __func__, ##__VA_ARGS__); \
		} while (0)

#endif

