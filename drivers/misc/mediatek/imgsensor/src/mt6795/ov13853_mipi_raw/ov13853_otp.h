/*file discription : otp*/
#define OTP_DRV_LSC_SIZE 186

struct otp_struct {
int flag;//bit[7]:info,bit[6]:wb,bit[5]:vcm,bit[4]:lenc
int module_integrator_id;
int lens_id;
int production_year;
int production_month;
int production_day;
int rg_ratio;
int bg_ratio;
//int light_rg;
//int light_bg;
//int typical_rg_ratio;
//int typical_bg_ratio;
unsigned char lenc[OTP_DRV_LSC_SIZE];
int checksumLSC;
int checksumOTP;
int checksumTotal;
int VCM_start;
int VCM_end;
int VCM_dir;
};

#define RG_Ratio_Typical 0x125
#define BG_Ratio_Typical 0x131

int read_otp(struct otp_struct *otp_ptr);
int apply_otp(struct otp_struct *otp_ptr);
int Decode_13853R2A(unsigned char*pInBuf, unsigned char* pOutBuf);
void otp_cali(unsigned char writeid);
void LumaDecoder(uint8_t *pData, uint8_t *pPara);
void ColorDecoder(uint8_t *pData, uint8_t *pPara);
//extern int read_otp_info(int index, struct otp_struct *otp_ptr);
//extern int update_otp_wb(void);
//extern int update_otp_lenc(void);







