/*Add for fm is working properly by enable fm inside LAN when no headset is plugged in,
	add by wangfajie@longcheer.com at 20190501.
 */
#ifndef _FM_LAN_H_
#define _FM_LAN_H_

#define  g_fm_lan_gpio 97    /*LAN gpio enable pin*/
void headset_status_change(bool status);
void fm_lan_power_set(bool status);

#endif /* _FM_LAN_H_ */
