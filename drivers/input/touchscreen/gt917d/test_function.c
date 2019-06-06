#ifdef __cplusplus

#endif

#include <linux/string.h>




#include "test_function.h"





#define GT_Config_Len  186
int test_error_code;
 int current_data_index=0;
extern u8 parse_config[250];



char itosavepath[50]={'\0'};

extern u8 gt9xx_sen_num;
extern u8 gt9xx_drv_num;


struct system_variable sys;
static mm_segment_t old_fs;
static loff_t file_pos = 0;
void TP_init(void)
{
  GTP_INFO("GXT:TP-INIT");

        sys.chip_type =_GT9P;
        sys.chip_name="GT9110";
        sys.config_length=186;
        sys.driver_num=42;
        sys.sensor_num=30;
        sys.max_driver_num=42;
        sys.max_sensor_num=30;
        sys.key_number=0;



}
FILE *fopen(const char *path, const char *mode)
{

	FILE *filp = NULL;

	if (!strcmp(mode, "a+")) {
		if(file_pos == 0) {

			filp = filp_open(path, O_RDWR | O_CREAT, 0666);
		}else {
			filp = filp_open(path, O_RDWR | O_CREAT, 0666);

		}
		if (!IS_ERR(filp)) {
			filp->f_op->llseek(filp, 0, SEEK_END);


		}

		if(filp == NULL){
			pr_err("open file as a+ mode filp == NULL 1\n");
		}

	} else if (!strcmp(mode, "r")) {

		filp = filp_open(path, O_RDONLY, 0666);

	}



	old_fs = get_fs();

	set_fs(KERNEL_DS);
	if(filp == NULL){
		pr_err("open file as a+ mode filp == NULL 2\n");
	}
	return filp;
}

int fclose(FILE * filp)
{
	filp_close(filp, NULL);

	filp = NULL;

	set_fs(old_fs);

	return 0;
}
 size_t fread(void *buffer, size_t size, size_t count, FILE * filp)
{
	return filp->f_op->read(filp, (char *)buffer, count, &filp->f_pos);
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE * filp)
{
	ssize_t  writeCount = -1;
	writeCount = vfs_write(filp, (char *)buffer, size, &file_pos);

	return writeCount;
}

 s32 Save_testing_data(u16 *current_diffdata_temp, int test_types, u16 *current_rawdata_temp)
{
	FILE *fp = NULL;
	s32 ret;
	s32 tmp = 0;
	u8 *data = NULL;
	s32 i = 0, j=0;
	s32 bytes = 0;
	int max, min;
	int average;
	int ITO_Sensor_ID;
	data = (u8 *) malloc(_GT9_MAX_BUFFER_SIZE);

	ITO_Sensor_ID=0;
	  GTP_INFO("Save_testing_data---in");
	if (NULL == data) {

		GTP_ERROR(" memory error!");
				GTP_INFO(" memory error!");

		return MEMORY_ERR;
	}
	for(i=0;i<_GT9_MAX_BUFFER_SIZE;i++)
	{
		data[i]=0;
	}

	sprintf((char *)itosavepath, "/sdcard/Gooidx_Rawdata_%02d.csv", 0);
	fp = fopen(itosavepath, "a+");
	if (NULL == fp) {
		GTP_ERROR("open %s failed!", itosavepath);
		free(data);
		return FILE_OPEN_CREATE_ERR;
	}


	if (current_data_index == 0) {
		bytes = (s32) sprintf((char *)data, "Device Type:%s\n", "GT9110");
		bytes += (s32) sprintf((char *)&data[bytes], "Config:\n");
		if(ITO_Sensor_ID==0)
		{
			for (i = 0; i < GT_Config_Len; i++)
				{
					bytes += (s32) sprintf((char *)&data[bytes], "0x%02X,", parse_config[i+2]);
				}
		}
		bytes += (s32) sprintf((char *)&data[bytes], "\n");
		ret = fwrite(data, bytes, 1, fp);
		bytes = 0;
		if (ret < 0) {
			GTP_ERROR("write to file fail.");
			goto exit_save_testing_data;
		}

		if ((test_types & _MAX_CHECK) != 0) {
			bytes = (s32) sprintf((char *)data, "Channel maximum:\n");
			for (i = 0; i < gt9xx_sen_num; i++) {
				for (j = 0; j < gt9xx_drv_num; j++) {
					if(ITO_Sensor_ID==0)
						{
						bytes += (s32) sprintf((char *)&data[bytes], "%d,", 10000);
						}
				}

				bytes += (s32) sprintf((char *)&data[bytes], "\n");
				ret = fwrite(data, bytes, 1, fp);
				bytes = 0;
				if (ret < 0) {
					GTP_ERROR("write to file fail.");
					goto exit_save_testing_data;
				}
			}


		}

		if ((test_types & _MIN_CHECK) != 0) {

			bytes = (s32) sprintf((char *)data, "\nChannel minimum:\n");
			for (i = 0; i < gt9xx_sen_num; i++) {
				for (j = 0; j < gt9xx_drv_num; j++) {
					if(ITO_Sensor_ID==0)
						{
						bytes += (s32) sprintf((char *)&data[bytes], "%d,", 100);
						}

				}

				bytes += (s32) sprintf((char *)&data[bytes], "\n");
				ret = fwrite(data, bytes, 1, fp);
				bytes = 0;
				if (ret < 0) {
					GTP_ERROR("write to file fail.");
					goto exit_save_testing_data;
				}
			}


		}

		if ((test_types & _ACCORD_CHECK) != 0) {

			bytes = (s32) sprintf((char *)data, "\nChannel average:(%d)\n", FLOAT_AMPLIFIER);
			for (i = 0; i < gt9xx_sen_num; i++) {
				for (j = 0; j < gt9xx_drv_num; j++) {
					if(ITO_Sensor_ID==0)
						{
						bytes += (s32) sprintf((char *)&data[bytes], "%d,", 0);
						}
				}

				bytes += (s32) sprintf((char *)&data[bytes], "\n");
				ret = fwrite(data, bytes, 1, fp);
				bytes = 0;
				if (ret < 0) {
					GTP_ERROR("write to file fail.");
					goto exit_save_testing_data;
				}
			}

			bytes = (s32) sprintf((char *)data, "\n");
			ret = fwrite(data, bytes, 1, fp);
			if (ret < 0) {
				GTP_ERROR("write to file fail.");
				goto exit_save_testing_data;
			}

		}

		bytes = (s32) sprintf((char *)data, " Rawdata\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			GTP_ERROR("write to file fail.");
			goto exit_save_testing_data;
		}
	}


	bytes = (s32) sprintf((char *)data, "No.%d\n", current_data_index);
	ret = fwrite(data, bytes, 1, fp);
	if (ret < 0) {
		GTP_ERROR("write to file fail.");
		goto exit_save_testing_data;
	}

	max=1000;
	min=5000;
	average = 0;

	for (i = 0; i < gt9xx_sen_num; i++) {
		bytes = 0;
		for (j = 0; j <gt9xx_drv_num; j++) {
			tmp = current_rawdata_temp[i + j * gt9xx_sen_num];
			bytes += (s32) sprintf((char *)&data[bytes], "%d,", tmp);
			if (tmp > max) {
				max = tmp;
			}
			if (tmp < min) {
				min = tmp;
			}
			average += tmp;
		}
		bytes += (s32) sprintf((char *)&data[bytes], "\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			GTP_ERROR("write to file fail.");
			goto exit_save_testing_data;
		}
	}
	average = average / (gt9xx_drv_num* gt9xx_sen_num);

	bytes = (s32) sprintf((char *)data, "  Maximum:%d  Minimum:%d  Average:%d\n\n", max, min, average);
	ret = fwrite(data, bytes, 1, fp);
	if (ret < 0) {
		GTP_ERROR("write to file fail.");
		goto exit_save_testing_data;
	}

	for (i = 0; i < gt9xx_sen_num; i++) {
		bytes = 0;
		for (j = 0; j <gt9xx_drv_num; j++) {
			tmp = current_diffdata_temp[i + j * gt9xx_sen_num];
			bytes += (s32) sprintf((char *)&data[bytes], "%d,", tmp);

		}
		bytes += (s32) sprintf((char *)&data[bytes], "\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			GTP_ERROR("write to file fail.");
			goto exit_save_testing_data;
		}
	}

	if ((test_types & _ACCORD_CHECK) != 0) {
		bytes = (s32) sprintf((char *)data, "Channel_Accord :(%d)\n", FLOAT_AMPLIFIER);
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			GTP_ERROR("write to file fail.");
			goto exit_save_testing_data;
		}
		for (i = 0; i < gt9xx_sen_num; i++) {
			bytes = 0;
			for (j = 0; j < gt9xx_drv_num; j++) {

				bytes += (s32) sprintf((char *)&data[bytes], "%d,", 0);

			}
			bytes += (s32) sprintf((char *)&data[bytes], "\n");
			ret = fwrite(data, bytes, 1, fp);
			if (ret < 0) {
				GTP_ERROR("write to file fail.");
				goto exit_save_testing_data;
			}
		}

		bytes = (s32) sprintf((char *)data, "\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			GTP_ERROR("write to file fail.");
			goto exit_save_testing_data;
		}
	}


exit_save_testing_data:
	/* pr_err("step4"); */
	free(data);
	/* pr_err("step3"); */
	fclose(fp);
	/* pr_err("step2"); */
	return ret;
}

 s32 Save_test_result_data(char *save_test_data_dir, int test_types)
{
	FILE *fp = NULL;
	s32 ret;

	u8 *data = NULL;
	s32 bytes = 0;

	data = (u8 *) malloc(_GT9_MAX_BUFFER_SIZE);
	if (NULL == data) {
		GTP_ERROR("memory error!");
		return MEMORY_ERR;
	}
	GTP_ERROR("before fopen patch = %s\n", save_test_data_dir);

	sprintf((char *)itosavepath, "/sdcard/ITO_Test_Data_%02d.csv", 0);
	fp = fopen(itosavepath, "a+");

	if (NULL == fp) {
		GTP_ERROR("open %s failed!", save_test_data_dir);
		free(data);
		return FILE_OPEN_CREATE_ERR;
	}

	bytes = (s32) sprintf((char *)data, "Test Result:");
	if (test_error_code == _CHANNEL_PASS) {
		bytes += (s32) sprintf((char *)&data[bytes], "Pass\n\n");
	} else {
		bytes += (s32) sprintf((char *)&data[bytes], "Fail\n\n");
	}
	bytes += (s32) sprintf((char *)&data[bytes], "Test items:\n");
	if ((test_types & _MAX_CHECK) != 0) {
		bytes += (s32) sprintf((char *)&data[bytes], "Max Rawdata:  ");
		if (test_error_code & _BEYOND_MAX_LIMIT) {
			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");
		} else {
			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");
		}
	}

	if ((test_types & _MIN_CHECK) != 0) {
		bytes += (s32) sprintf((char *)&data[bytes], "Min Rawdata:  ");
		if (test_error_code & _BEYOND_MIN_LIMIT) {
			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");
		} else {
			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");
		}
	}

	if ((test_types & _ACCORD_CHECK) != 0) {
		bytes += (s32) sprintf((char *)&data[bytes], "Area Accord:  ");

		if (test_error_code & _BETWEEN_ACCORD_AND_LINE) {
			bytes += (s32) sprintf((char *)&data[bytes], "Fuzzy !\n");
		} else {
			if (test_error_code & _BEYOND_ACCORD_LIMIT) {

				bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

			} else {

				bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

			}
		}
	}

	if ((test_types & _SHORT_CHECK) != 0) {
		bytes += (s32) sprintf((char *)&data[bytes], "Moudle Short Test:  ");
		if (test_error_code & _SENSOR_SHORT) {
			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");
		} else {
			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");
		}
	}


	if ((test_types & _OFFSET_CHECK) != 0) {

		bytes += (s32) sprintf((char *)&data[bytes], "Max Offest:  ");

		if (test_error_code & _BEYOND_OFFSET_LIMIT) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
	}

	if ((test_types & _JITTER_CHECK) != 0) {

		bytes += (s32) sprintf((char *)&data[bytes], "Max Jitier:  ");

		if (test_error_code & _BEYOND_JITTER_LIMIT) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
	}

	if (test_types & _UNIFORMITY_CHECK) {

		bytes += (s32) sprintf((char *)&data[bytes], "Uniformity:  ");

		if (test_error_code & _BEYOND_UNIFORMITY_LIMIT) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
	}

	if ((test_types & _KEY_MAX_CHECK) != 0) {

		bytes += (s32) sprintf((char *)&data[bytes], "Key Max Rawdata:  ");

		if (test_error_code & _KEY_BEYOND_MAX_LIMIT) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
	}

	if ((test_types & _KEY_MIN_CHECK) != 0) {

		bytes += (s32) sprintf((char *)&data[bytes], "Key Min Rawdata:  ");

		if (test_error_code & _KEY_BEYOND_MIN_LIMIT) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
	}

	if (test_types & (_VER_EQU_CHECK | _VER_GREATER_CHECK | _VER_BETWEEN_CHECK)) {

		bytes += (s32) sprintf((char *)&data[bytes], "Device Version:  ");

		if (test_error_code & _VERSION_ERR) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
	}

	if (test_types & _MODULE_TYPE_CHECK) {

		bytes += (s32) sprintf((char *)&data[bytes], "Module Type:  ");

		if (test_error_code & _MODULE_TYPE_ERR) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n");

		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
	}

	ret = fwrite(data, bytes, 1, fp);

	if (ret < 0) {

		GTP_ERROR("write to file fail.");

		free(data);

		fclose(fp);

		return ret;

	}


/*	if ((test_types & _MODULE_SHORT_CHECK) != 0) {

		bytes = (s32) sprintf((char *)data, "Module short test:  ");

		if (test_error_code & _GT_SHORT) {

			bytes += (s32) sprintf((char *)&data[bytes], "NG !\n\n\nError items:\nShort:\n");

			if (shortresult[0] > _GT9_UPLOAD_SHORT_TOTAL) {

				WARNING("short total over limit, data error!");

				shortresult[0] = 0;

			}

			for (index = 0; index < shortresult[0]; index++) {

				//pr_err("bytes=%d shortresult[0]=%d",bytes,shortresult[0]);
				if (shortresult[1 + index * 4] & 0x80) {

					bytes += (s32) sprintf((char *)&data[bytes], "Drv%d - ", shortresult[1 + index * 4] & 0x7F);

				}

				else {

					if (shortresult[1 + index * 4] == (sys.max_driver_num + 1)) {

						bytes += (s32) sprintf((char *)&data[bytes], "GND\\VDD%d - ", shortresult[1 + index * 4] & 0x7F);

					}

					else {

						bytes += (s32) sprintf((char *)&data[bytes], "Sen%d - ", shortresult[1 + index * 4] & 0x7F);

					}
				}
				if (shortresult[2 + index * 4] & 0x80) {

					bytes += (s32) sprintf((char *)&data[bytes], "Drv%d 之间短路", shortresult[2 + index * 4] & 0x7F);

				}

				else {

					if (shortresult[2 + index * 4] == (sys.max_driver_num + 1)) {

						bytes += (s32) sprintf((char *)&data[bytes], "GND\\VDD 之间短路");

					}

					else {

						bytes += (s32) sprintf((char *)&data[bytes], "Sen%d 之间短路", shortresult[2 + index * 4] & 0x7F);

					}
				}
				bytes += (s32) sprintf((char *)&data[bytes], "(R=%d Kohm)\n", (((shortresult[3 + index * 4] << 8) + shortresult[4 + index * 4]) & 0xffff) / 10);

				pr_err("%d&%d:", shortresult[1 + index * 4], shortresult[2 + index * 4]);

				pr_err("%dK", (((shortresult[3 + index * 4] << 8) + shortresult[4 + index * 4]) & 0xffff) / 10);

			}
		}

		else {

			bytes += (s32) sprintf((char *)&data[bytes], "pass\n");

		}
		ret = fwrite(data, bytes, 1, fp);

		if (ret < 0) {

			WARNING("write to file fail.");

			free(data);

			fclose(fp);

			return ret;

		}

	}
*/
	free(data);

	fclose(fp);

	return 1;

}


