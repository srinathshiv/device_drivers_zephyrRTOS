#include<zephyr.h>
#include<kernel.h>
#include<device.h>
#include<flash.h>
#include<init.h>
#include<i2c.h>
#include<misc/util.h>
#include<string.h>
#include<pinmux.h>
#include<clock_control.h>
#include"board.h"

#define PAGES 512
#define WORDS_PER_PAGE 16
#define PAGE_SIZE_IN_BYTES 64
#define SLAVE_ADDRESS 85 //0X55
#define EEPROM_PRIORITY 20

#define SDAPIN 0 
#define SCLPIN 2
/*----- config ------*/
struct eeprom_dev_config{
//	uint32_t *base;	
	
	uint32_t sda_conf;
	uint32_t scl_conf;
	struct device *i2c_dev;
	struct device *i2c;	
};
/*------------------*/


struct eeprom_dev_runtime{
	uint32_t sda;
	uint32_t scl;

};

int buffer[WORDS_PER_PAGE];
int current_page = 0;

uint8_t control_byte = 160; //0XA0
uint16_t full_address;	// 16bit address
struct mem_addr{
	uint8_t highbyte;		// upper 8bit
	uint8_t lowbyte ;		// lower 8bit
};
typedef struct mem_addr mem_addr;
mem_addr address;

void calculate_address(int pagenum){
	full_address = 0;

	for(int i=0; i<pagenum; i++){
		full_address += 64;
	}

	address.lowbyte  = full_address & 255; //0XFF
	address.highbyte = full_address >> 8 ;
}

int eeprom_read(struct device *devD, off_t offset, void *data, size_t len)
{
	int ret_i2c_r;
	int offset_pagenum = (int)offset;
	uint8_t local_buffer[2];

	struct eeprom_dev_config *config = devD -> config ->config_info;
	struct eeprom_dev_runtime *dev = devD -> driver_data;


	calculate_address(offset_pagenum);
	local_buffer[0] = address.highbyte;
	local_buffer[1] = address.lowbyte;
	printk("\n I2C write addr low high full : %d %d %d",address.lowbyte, address.highbyte, full_address);
	
//	ret_i2c_r = i2c_burst_read16(config->i2c_dev,SLAVE_ADDRESS, full_address, (uint8_t *)data , 64);

	ret_i2c_r = i2c_write( config->i2c_dev, local_buffer, 2, SLAVE_ADDRESS);
	ret_i2c_r = i2c_read( config->i2c_dev,(uint32_t *) data ,64,SLAVE_ADDRESS);
	if(ret_i2c_r==0)
		printk("\nRead on page %d success",offset_pagenum); 
	else{
		printk("\nRead failed on page %d",offset_pagenum);
		return -1;
	}

return 0;
}

int eeprom_write(struct device *devD, off_t offset, const void *data, size_t len)
{
	int i, j, ret_i2c_w;
	uint32_t num_bytes = (uint32_t) len;
	uint8_t local_buffer[ PAGE_SIZE_IN_BYTES+2 ]; // high_byte + low_byte + 64byte data
	uint8_t local_data[PAGE_SIZE_IN_BYTES]; 
	
	struct eeprom_dev_config *config = devD -> config ->config_info;
	struct eeprom_dev_runtime *dev = devD -> driver_data;

	memcpy(local_data, (uint8_t *)data, 64);
	
	calculate_address(current_page);
	printk("\n I2C write addr low high: %d %d",address.lowbyte, address.highbyte);	
	local_buffer[0]=address.highbyte;
	local_buffer[1]=address.lowbyte;
	for(i=2, j=0; i< num_bytes+2 && j<num_bytes ; i++, j++)
		local_buffer[i] = local_data[j];

	printk("\nI2c Write info:");
	for(i=0; i<num_bytes+2;i++)
		printk("%d ",local_buffer[i]);
	
	ret_i2c_w = i2c_write( config->i2c_dev, local_buffer, num_bytes+2, SLAVE_ADDRESS);
//	(ret_i2c_w ==0)?printk("\n------------------------------\nI2C Write Success\n"): printk("\nI2C Write failed");
	
	if(current_page == PAGES){
		printk("\nError: Beyond maximum page limit");	
		return -1;
	}
	else
		current_page += 1;

return 0;
}

int eeprom_erase(struct device *devD, off_t offset, size_t size)
{
	uint16_t local_full_addr=0;
	int len=(int)size;
	int i, j, i2c_write_w0, ret_i2c_w0;
	int erase_count=0;
	uint8_t local_buffer[ PAGE_SIZE_IN_BYTES+2 ];
	uint8_t local_data[PAGE_SIZE_IN_BYTES];

	struct eeprom_dev_config *config = devD -> config ->config_info;
	struct eeprom_dev_runtime *dev = devD -> driver_data;


	memcpy(local_data, 0, 64);

	for(i=2, j=0; i< len+2 && j<len ; i++, j++)
		local_buffer[i] = local_data[j];

	for(i=0; i< PAGES; i++){
		local_buffer[0]= local_full_addr & 255; //0XFF
		local_buffer[1]= local_full_addr >> 8;
		ret_i2c_w0 = i2c_write(config->i2c_dev, local_buffer, len+2, SLAVE_ADDRESS);

	if(ret_i2c_w0 == 0) 
		erase_count++;

	local_full_addr += 64;
	}

	if(erase_count == PAGES) 
		printk("\nAll pages in EEPROM are erased");
	else
		return -1;

return 0;
}

int eeprom_wp(struct device *dev, bool enable)
{
printk("\n ");
return 0;
}

static struct flash_driver_api api_func={
	.read				= eeprom_read,
	.write				= eeprom_write,
	.erase				= eeprom_erase,
	.write_protection	= eeprom_wp 
};

int eeprom_init(struct device *devD){
	int ret;
	
	struct eeprom_dev_config *config = devD -> config -> config_info;
	struct eeprom_dev_runtime * const dev = devD -> driver_data;
	
	struct device *pinmux;
	pinmux = device_get_binding(CONFIG_PINMUX_NAME);

	dev->sda = config->sda_conf;
	dev->scl = config->scl_conf;

	config->i2c = device_get_binding("I2C_0");
	config->i2c_dev = device_get_binding(CONFIG_GPIO_PCAL9535A_1_I2C_MASTER_DEV_NAME);
	i2c_configure(config->i2c_dev, (I2C_SPEED_FAST << 1) | I2C_MODE_MASTER);

	pinmux_pin_set(pinmux, 18, PINMUX_FUNC_C); //sda
	pinmux_pin_set(pinmux, 19, PINMUX_FUNC_C); //scl


return 0;
}


struct eeprom_dev_config eeprom_config={
	//	.base =
	.sda_conf = SDAPIN, 
	.scl_conf =	SCLPIN
};


struct eeprom_dev_runtime eeprom_runtime_data;

DEVICE_AND_API_INIT(EEPROM, CONFIG_EEPROM_24FC256_DEV_NAME, eeprom_init, 
					&eeprom_runtime_data, &eeprom_config , APPLICATION,
					EEPROM_PRIORITY, &api_func);

