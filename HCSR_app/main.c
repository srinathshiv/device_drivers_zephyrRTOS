#include<zephyr.h>
#include<board.h>
#include<shell/shell.h>
#include<misc/util.h>
#include<gpio.h>
#include<i2c.h>
#include<pinmux.h>
#include<stdlib.h>
#include<ctype.h>
#include<sensor.h>
#include "board.h"

#define STACKSIZE 256
K_THREAD_STACK_DEFINE(stack_area0, STACKSIZE);
K_SEM_DEFINE(sem0,0,1);
K_SEM_DEFINE(sem1,0,1);

struct device *sensor;
struct device *memory;
struct sensor_value *sensor_val;

struct k_thread thread0;

static int shell_cmd_enable(int argc, char *argv[]){
int en= atoi(argv[1]);

switch(en){
case 0:	printk("No Sensor loaded\n");
		break;

case 1: sensor = device_get_binding(CONFIG_HCSR04_DEV0_NAME);
		(!sensor)? printk("\nHCSR0 sensor binding failed"): printk("\nHCSR0 Sensor loaded\n");

		memory = device_get_binding(CONFIG_EEPROM_24FC256_DEV_NAME);
		(!memory)? printk("\nEEPROM binding failed"): printk("\nEEPROM loaded");
		break;

case 2: sensor = device_get_binding(CONFIG_HCSR04_DEV1_NAME);
		(!sensor)? printk("\nHCSR1 sensor binding failed"): printk("\nHCSR1 Sensor loaded\n");
		
		memory = device_get_binding(CONFIG_EEPROM_24FC256_DEV_NAME);
		(!memory)? printk("\nEEPROM binding failed"): printk("\nEEPROM loaded");
		break;

default:printk("\nInvalid input. Valid inputs are 0,1 and 2");
}

return 0;
}

static int shell_cmd_start(int argc, char *argv[]){
int writep = atoi(argv[1]);
int ret, i_sensor;
uint32_t buf0[16];
uint32_t buf1[16];


eeprom_erase(memory, 0, 0);

for(int i=0; i< writep; i++){
	printk("\nPage %d write operation:\n",i);
	
	for(i_sensor=0; i_sensor< 16; i_sensor++){	
		ret = hcsr_channel_get(sensor, NULL, sensor_val);
		k_sleep(100);
		buf0[i_sensor]   = sensor_val->val1;
		printk("%d ",buf0[i_sensor]);
	}
	
	eeprom_write(memory, 0, buf0, 64); 
	}
	

return 0;
}

static int shell_cmd_dump(int argc, char *argv[]){
int readp1 = atoi(argv[1]);
int readp2 = atoi(argv[2]);
int i,k;
uint8_t read_buffer[64]; //4*16 bytes = 64bytes

while(readp1 <= readp2){
	printk("\nPage %d :\n", readp1);
	eeprom_read(memory, readp1, read_buffer, 64);
	uint32_t* integers = (uint32_t *) read_buffer;
	for(i=0; i<16; i++){
		printk("%d cms  ", (integers[i]/58000) );
		if(i%8 == 0) printk("\n");
	}
	readp1 += 1;
}

return 0;
}

static struct shell_cmd commands[]={
	{"enable", shell_cmd_enable, "Enable no sensor or sensoe0 or sensor1"},
	{"start" , shell_cmd_start,  "Start storing data in EEPROM"},
	{"dump"  , shell_cmd_dump,   "Dump data from EEPROM to console"},
	{NULL,NULL,NULL}
};

void main(){

printk("\n");
SHELL_REGISTER("SHELL", commands);
printk("\n");
}
