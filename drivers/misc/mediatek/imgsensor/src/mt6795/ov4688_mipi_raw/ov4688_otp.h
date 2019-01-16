/*file discription : otp*/
//#define OTP_DRV_LSC_SIZE 186
//#define OV4688_USE_WB_OTP

struct otp_struct 
{
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int user_data[5];
	int VCM_start;
	int VCM_end;
};

#define RG_Ratio_Typical 0x125
#define BG_Ratio_Typical 0x131

int read_otp(struct otp_struct *otp_ptr);
int apply_otp(struct otp_struct *otp_ptr);
void ov4688_otp_cali(unsigned char writeid);
#if 0
int Decode_13850R2A(unsigned char*pInBuf, unsigned char* pOutBuf);
void otp_cali(unsigned char writeid);
void LumaDecoder(uint8_t *pData, uint8_t *pPara);
void ColorDecoder(uint8_t *pData, uint8_t *pPara);
//extern int read_otp_info(int index, struct otp_struct *otp_ptr);
//extern int update_otp_wb(void);
//extern int update_otp_lenc(void);
#endif










