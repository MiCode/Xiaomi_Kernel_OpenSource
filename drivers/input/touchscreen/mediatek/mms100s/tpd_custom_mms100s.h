
#ifndef __TPD_CUSTOM_MMS100S_H__

#define key_1 250,1950 
#define key_2 900,1950
#define TPD_KEY_COUNT           3
#define TPD_KEYS {KEY_MENU,KEY_HOMEPAGE, KEY_BACK}
//#define TPD_KEYS_DIM1 {{key_1,50,20},{key_2,50,20}}

#define CUSTOM_MAX_WIDTH (1080)
#define CUSTOM_MAX_HEIGHT (1920)

#define MMS_MAX_WIDTH  (1080)
#define MMS_MAX_HEIGHT (1920)


#define TPD_YMAX        (2075)
#define TPD_Y_OFFSET		30

#define TPD_B1_FP	80		//Button 1 pad space
#define TPD_B1_W	240		//Button 1 Width
#define TPD_B2_FP	90		//Button 2 pad space
#define TPD_B2_W	240		//Button 2 Width
#define TPD_B3_FP	120		//Button 3 pad space
#define TPD_B3_W	240		//Button 3 Width

#define TPD_BUTTON1_X_CENTER	(TPD_B1_FP + TPD_B1_W/2)
#define TPD_BUTTON2_X_CENTER	(TPD_B1_FP + TPD_B1_W + TPD_B2_FP + TPD_B2_W/2)
#define TPD_BUTTON3_X_CENTER	(TPD_B1_FP + TPD_B1_W + TPD_B2_FP + TPD_B2_W + TPD_B3_FP + TPD_B3_W/2)

#define TPD_BUTTON_SIZE_HEIGHT  (TPD_YMAX - CUSTOM_MAX_HEIGHT - TPD_Y_OFFSET)
#define TPD_BUTTON_Y_CENTER   	(CUSTOM_MAX_HEIGHT + TPD_Y_OFFSET + (TPD_YMAX - CUSTOM_MAX_HEIGHT - TPD_Y_OFFSET)/2)

#define TPD_KEYS_DIM		{{TPD_BUTTON1_X_CENTER, TPD_BUTTON_Y_CENTER, TPD_B1_W, TPD_BUTTON_SIZE_HEIGHT},	\
				 			{TPD_BUTTON2_X_CENTER, TPD_BUTTON_Y_CENTER, TPD_B2_W, TPD_BUTTON_SIZE_HEIGHT},	\
							{TPD_BUTTON3_X_CENTER, TPD_BUTTON_Y_CENTER, TPD_B3_W, TPD_BUTTON_SIZE_HEIGHT}}



#define TPD_WARP_X
#define TPD_WARP_Y

#if 0 //TPD_WARP_X
#undef TPD_WARP_X
#define TPD_WARP_X(x_max, x) ( x_max - 1 - x )
#else
#define TPD_WARP_X(x_max, x) x
#endif

#if 0 //TPD_WARP_Y
#undef TPD_WARP_Y
#define TPD_WARP_Y(y_max, y) ( y_max - 1 - y )
#else
#define TPD_WARP_Y(y_max, y) y
#endif

#endif //__TPD_CUSTOM_MMS100S_H__
