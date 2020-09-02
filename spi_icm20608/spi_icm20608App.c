#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
	int result = 0;
	char value = 0;
	int fd = 0;
	char* filename = NULL;
	unsigned int readBuff[7];

	signed int gyro_x_adc, gyro_y_adc, gyro_z_adc;
	signed int accel_x_adc, accel_y_adc, accel_z_adc;
	signed int temp_adc;

	float gyro_x_act, gyro_y_act, gyro_z_act;
	float accel_x_act, accel_y_act, accel_z_act;
	float temp_act;

	if(argc < 2)
	{
		printf("**APP** : argc num error!!!\r\n");
		return -1;
	}

	filename = argv[1];
	
	fd = open(filename,O_RDWR);
	if(fd < 0)
	{
		printf("**APP** : open %s faild!!!\r\n",filename);
		return -1;
	}

	while(1)
	{
		read(fd, readBuff, sizeof(readBuff));

		gyro_x_adc = readBuff[0];
		gyro_y_adc = readBuff[1];
		gyro_z_adc = readBuff[2];
		accel_x_adc = readBuff[3];
		accel_y_adc = readBuff[4];
		accel_z_adc = readBuff[5];
		temp_adc = readBuff[6];

		/* 计算实际值 */
		gyro_x_act = (float)(gyro_x_adc)  / 16.4;
		gyro_y_act = (float)(gyro_y_adc)  / 16.4;
		gyro_z_act = (float)(gyro_z_adc)  / 16.4;
		accel_x_act = (float)(accel_x_adc) / 2048;
		accel_y_act = (float)(accel_y_adc) / 2048;
		accel_z_act = (float)(accel_z_adc) / 2048;
		temp_act = ((float)(temp_adc) - 25 ) / 326.8 + 25;


		printf("\r\n原始值:\r\n");
		printf("gx = %d, gy = %d, gz = %d\r\n", gyro_x_adc, gyro_y_adc, gyro_z_adc);
		printf("ax = %d, ay = %d, az = %d\r\n", accel_x_adc, accel_y_adc, accel_z_adc);
		printf("temp = %d\r\n", temp_adc);
		printf("实际值:");
		printf("act gx = %.2f°/S, act gy = %.2f°/S, act gz = %.2f°/S\r\n", gyro_x_act, gyro_y_act, gyro_z_act);
		printf("act ax = %.2fg, act ay = %.2fg, act az = %.2fg\r\n", accel_x_act, accel_y_act, accel_z_act);
		printf("act temp = %.2f°C\r\n", temp_act);
		usleep(200000);		//200ms
	}

	result = close(fd);
	if(result < 0)
	{
		printf("**APP** : close %s faild!!!\r\n",filename);
		return -1;
	}

	return 0;
}
















































