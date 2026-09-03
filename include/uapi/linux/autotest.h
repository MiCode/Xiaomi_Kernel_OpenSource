/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _UAPI_LINUX_AUTOTEST_H_
#define _UAPI_LINUX_AUTOTEST_H_

#define AUTOTEST_MAGIC  'A'

#define AT_PINCTRL	_IOWR(AUTOTEST_MAGIC, 0, int)/* Set at_pinctrl cmd*/
#define AT_GPIO		_IOWR(AUTOTEST_MAGIC, 1, int)/* Set at_gpio cmd */
#define AT_OTG		_IOWR(AUTOTEST_MAGIC, 2, int)/* Set at_otg cmd */

#endif /* _UAPI_LINUX_AUTOTEST_H_ */
