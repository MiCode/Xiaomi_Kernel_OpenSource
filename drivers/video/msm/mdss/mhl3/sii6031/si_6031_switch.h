#ifndef __MHL_8620_H
#define __MHL_8620_H


#define MHL_USB_0               0
#define MHL_USB_1               1
#define MHL_USB_VBUS            2

void sii_switch_to_mhl(bool switch_to_mhl);
/*#ifdef CONFIG_SII8061_MHL_SWITCH
void sii_switch_to_mhl(bool switch_to_mhl);
#else
void inline sii_switch_to_mhl(bool switch_to_mhl)
{
	        return;

}
#endif
*/
#endif
