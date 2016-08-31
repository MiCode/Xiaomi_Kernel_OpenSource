/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */

#ifndef _OZKOBJECT_H
#define _OZKOBJECT_H

#define OZ_MAX_NW_IF	6

void oz_create_sys_entry(void);
void oz_destroy_sys_entry(void);
void oz_set_serial_mode(u8 mode);
u8 oz_get_serial_mode(void);

#endif /* _OZKOBJECT_H */
