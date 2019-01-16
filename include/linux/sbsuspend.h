/* include/linux/sbsuspend.h
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
 
#ifndef _LINUX_SBSUSPEND_H
#define _LINUX_SBSUSPEND_H

#ifdef CONFIG_HAS_SBSUSPEND
#include <linux/list.h>
#endif

//level definition
enum {
    SB_LEVEL_DISABLE_TOUCH = 50,
    SB_LEVEL_DISABLE_KEYPAD = 100,
    SB_LEVEL_DISABLE_FB = 150,
};

//smartbook handler structure
struct sb_handler {
#ifdef CONFIG_HAS_SBSUSPEND
    struct list_head link;
    int level;
    char name[32];
    void (*plug_in)(struct sb_handler *h);
    void (*plug_out)(struct sb_handler *h);
    void (*enable)(struct sb_handler *h);
    void (*disable)(struct sb_handler *h);
    void (*suspend)(struct sb_handler *h);
    void (*resume)(struct sb_handler *h);
#endif
};


typedef int __bitwise sb_state_t;
#define SB_STATE_DISABLE    ((__force sb_state_t) 0)
#define SB_STATE_ENABLE     ((__force sb_state_t) 1)
#define SB_STATE_SUSPEND    ((__force sb_state_t) 2)
#define SB_STATE_RESUME     ((__force sb_state_t) 3)
#define SB_STATE_MAX        ((__force sb_state_t) 4)

/*
typedef int __bitwise sb_event_t;
#define SB_EVENT_ENABLE     ((__force sb_state_t) 0)
#define SB_EVENT_DISABLE    ((__force sb_state_t) 1)
#define SB_EVENT_SUSPEND    ((__force sb_state_t) 2)
#define SB_EVENT_RESUME     ((__force sb_state_t) 3)
*/

#ifdef CONFIG_HAS_SBSUSPEND

//for peripheral drivers who is smartbook aware to register handler (process context)
extern void register_sb_handler(struct sb_handler *handler);
extern void unregister_sb_handler(struct sb_handler *handler);

//for tester to execute callbacks directly (process context)
extern void sb_enable(void);
extern void sb_disable(void);
extern void sb_suspend(void);
extern void sb_resume(void);

//for hdmi driver to notify plug in/out event
extern void sb_plug_in(void);
extern void sb_plug_out(void);

#else

#define register_sb_handler(handler) do { } while (0)
#define unregister_sb_handler(handler) do { } while (0)
#define sb_enable() do { } while (0)
#define sb_disable() do { } while (0)
#define sb_suspend() do { } while (0)
#define sb_resume() do { } while (0)

#endif

#endif //#ifndef _LINUX_SBSUSPEND_H

