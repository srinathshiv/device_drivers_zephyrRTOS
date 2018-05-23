#include<zephyr.h>
#include<kernel.h>
#include<device.h>
#include<init.h>
#include<misc/util.h>
#include<gpio.h>
#include<sensor.h>
#include<sys_clock.h>
#include<clock_control.h>
#include<pinmux.h>

#include "board.h"

#define HCSR01_PRIORITY 10
#define HCSR02_PRIORITY 11
#define SLEEPT 4000

#define IO0 0
#define IO1 1
#define IO2 2
#define IO3 3
#define TRIG_PINZERO 3
#define ECHO_PINZERO 4	
#define TRIG_PINONE 5
#define ECHO_PINONE 6
 
uint64_t start_time,end_time,total_time;
struct gpio_callback gpio_cb;
//int flag=0;
K_SEM_DEFINE(semaphore, 0, 1);

/*----- config -----*/
struct hcsr_dev_config{
	uint32_t *base;

	uint32_t int_pin;
	uint32_t trigger_pin;
	uint32_t echo_pin;

	struct device *int_dev;
	struct device *dev;
};
/*------------------*/

/*----- runtime_data -----*/
struct hcsr_dev_runtime{
	uint32_t trigger_pin;
	uint32_t echo_pin;
	struct gpio_callback gpio_cb;
};
/*------------------ -----*/


void callback_func(struct device *dev, struct gpio_callback *cb, uint32_t pin)
{

	int read_val, ret;
	struct device *irq_device=NULL;
	
	if( ( cb -> pin_mask = BIT(ECHO_PINZERO)) ){
		irq_device = device_get_binding(CONFIG_HCSR04_DEV0_NAME);
	}
	if( (cb -> pin_mask = BIT(ECHO_PINONE)) ){
		irq_device = device_get_binding(CONFIG_HCSR04_DEV1_NAME);
	}
	
	
	struct hcsr_dev_config *config = irq_device -> config -> config_info;
	struct hcsr_dev_runtime *device = irq_device -> driver_data;
	
	gpio_pin_read(dev, config->int_pin, &read_val);
//	printk("\nRead Value on interrupt pin: %d", read_val);
	if(read_val == 1){
	
	start_time = _tsc_read();

		ret = gpio_pin_configure(dev, config-> int_pin, GPIO_DIR_IN | GPIO_INT |  GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW | GPIO_INT_DEBOUNCE | GPIO_PUD_PULL_DOWN ); 
//		ret<0 ? printk("\nError in ISR: GPIO config") : printk("\nISR: GPIO config success");
	}
	else if(read_val == 0){
		ret = gpio_pin_configure(dev, config-> int_pin, GPIO_DIR_IN | GPIO_INT |  GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE | GPIO_PUD_PULL_DOWN); 
//		ret<0 ? printk("\nError in ISR: GPIO config") : printk("\nISR: GPIO config success");
		
		end_time = _tsc_read();
		total_time = end_time-start_time;
	}

	k_sem_give(&semaphore);
}


int hcsr_channel_get(struct device *devD, enum sensor_channel chan,
					 struct sensor_value *val)
{
	int i=0;
	int ret, read_val;
	struct hcsr_dev_config *config = devD -> config -> config_info;
	struct hcsr_dev_runtime * dev = devD -> driver_data;

	gpio_pin_write(config->dev , dev->trigger_pin  ,1);
	k_busy_wait(12);	
	gpio_pin_write(config->dev , dev->trigger_pin ,0);

	if( k_sem_take(&semaphore,24) != 0){
//		printk("\nTotal time:%ld",total_time);
		printk("\nSem not taken");
		return -1;
	}

val->val1 = total_time;

return 0;
}

int hcsr_sample_fetch(struct device *devD )
{

return 0;
}

static struct sensor_driver_api hcsr_api_func = {
	.sample_fetch = hcsr_sample_fetch,
	.channel_get  = hcsr_channel_get,

};

/*struct device * int_conf(struct device *devD, struct device *pinmux)
{
	int ret;
	struct device *interrupt;
	struct hcsr_dev_config * config = devD -> config -> config_info;
	struct hcsr_dev_runtime * const dev = devD -> driver_data;
	int ipin = config -> int_pin;
	
	interrupt = device_get_binding(PINMUX_GALILEO_GPIO_DW_NAME);

	(!interrupt)? printk("\nError in Interrupt pin config: GPIO device binding"): printk("\nInterrupt pin config:GPIO Device binding for interrupt pin success") ; 

	pinmux_pin_set(pinmux, IO1, PINMUX_FUNC_A);
	gpio_pin_configure(interrupt, ipin, GPIO_DIR_OUT );
	gpio_pin_write(interrupt , ipin, 0);

	pinmux_pin_set(pinmux, IO1, PINMUX_FUNC_B);
	ret= gpio_pin_configure(interrupt, ipin, GPIO_DIR_IN | GPIO_INT |  GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE |GPIO_PUD_PULL_DOWN);
	(ret<0) ? printk("\nError in Interrupt pin config: GPIO pin configure"): printk("\nInterrupt pin config:GPIO pin configure success");
	if (ret < 0) printk("\nInterrupt pin config failed");
	else printk("\n\nPin %d configured as interrupt\n\n", ipin);

	gpio_init_callback(&dev->gpio_cb, callback_func, BIT(ipin) );
	gpio_add_callback(interrupt, &dev->gpio_cb);
	gpio_pin_enable_callback(interrupt, ipin);

return interrupt;
}*/

int hcsr_init(struct device *devD){

	int ret0,ret1;

	struct hcsr_dev_config *config = devD -> config -> config_info;
	struct hcsr_dev_runtime * const dev = devD -> driver_data;

	struct device *gpio;
	struct device *pinmux;
	pinmux = device_get_binding(CONFIG_PINMUX_NAME);

//	config->int_dev = int_conf(devD, pinmux);


	dev->trigger_pin   = config-> trigger_pin;
	dev->echo_pin      = config-> echo_pin;

	if(dev->echo_pin == ECHO_PINZERO){
		config->dev = device_get_binding(PINMUX_GALILEO_GPIO_DW_NAME);	
		pinmux_pin_set(pinmux, IO0, PINMUX_FUNC_A);
		
		gpio_pin_configure(config->dev , TRIG_PINZERO, GPIO_DIR_OUT );
		gpio_pin_write(config->dev , TRIG_PINZERO, 0);

		
		pinmux_pin_set(pinmux, IO1, PINMUX_FUNC_A);	
		gpio_pin_configure(config->dev, ECHO_PINZERO, GPIO_DIR_OUT);
		gpio_pin_write(config->dev, ECHO_PINZERO, 0);
		
		pinmux_pin_set(pinmux, IO1, PINMUX_FUNC_B);
		gpio_pin_configure(config->dev, ECHO_PINZERO , GPIO_DIR_IN | GPIO_INT |  GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE |GPIO_PUD_PULL_DOWN);

		gpio_init_callback( &gpio_cb ,callback_func, BIT(ECHO_PINZERO));
		ret0=gpio_add_callback(config->dev, &gpio_cb );
		ret1= gpio_pin_enable_callback(config->dev, ECHO_PINZERO);
		
		ret0<0 ? printk("\nadd callback failed for echo0"): printk("\nadd callback success for echo0");
		ret1<0 ? printk("\nenable callbcak failed for echo0"): printk("\nenable callbcak success for echo0");
	}
	
	else if(dev->echo_pin == ECHO_PINONE){
		config->dev = device_get_binding(PINMUX_GALILEO_GPIO_DW_NAME );
		pinmux_pin_set(pinmux, IO2, PINMUX_FUNC_A);
		pinmux_pin_set(pinmux, IO3, PINMUX_FUNC_B);

		gpio_pin_configure(config->dev, TRIG_PINONE, GPIO_DIR_OUT );
		gpio_pin_write(config->dev , TRIG_PINONE, 0);

		pinmux_pin_set(pinmux, IO3, PINMUX_FUNC_A);	
		gpio_pin_configure(config->dev, ECHO_PINONE, GPIO_DIR_OUT);
		gpio_pin_write(config->dev, ECHO_PINONE, 0);
		
		pinmux_pin_set(pinmux, IO3, PINMUX_FUNC_B);
		gpio_pin_configure(config->dev, ECHO_PINONE , GPIO_DIR_IN | GPIO_INT |  GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE |GPIO_PUD_PULL_DOWN);

		gpio_init_callback( &gpio_cb ,callback_func, BIT(ECHO_PINONE));
		ret0 = gpio_add_callback(config->dev, &gpio_cb );
		ret1 = gpio_pin_enable_callback(config->dev, ECHO_PINONE);

		ret0<0 ? printk("\nadd callback failed for echo1"): printk("\nadd callback success for echo1");
		ret1<0 ? printk("\nenable callbcak failed for echo1"): printk("\nenable callbcak success for echo1");

	}
	
return 0;
}

struct hcsr_dev_config hcsr_config0={
	.int_pin	   = ECHO_PINZERO,
	.trigger_pin   = TRIG_PINZERO,
	.echo_pin      = ECHO_PINZERO
};

struct hcsr_dev_config hcsr_config1={
	.int_pin       = ECHO_PINONE, 
	.trigger_pin   = TRIG_PINONE,
	.echo_pin      = ECHO_PINONE
};

struct hcsr_dev_runtime hcsr_runtime_data0;
struct hcsr_dev_runtime hcsr_runtime_data1;

//DEVICE_AND_API_INIT(HCSR01, CONFIG_HCSR04_DEV0_NAME, hcsr_init,
//					&hcsr_runtime_data0, &hcsr_config0, APPLICATION,
//					HCSR01_PRIORITY, &hcsr_api_func);

DEVICE_AND_API_INIT(HCSR02, CONFIG_HCSR04_DEV1_NAME, hcsr_init,
					&hcsr_runtime_data1, &hcsr_config1, APPLICATION,
					HCSR02_PRIORITY, &hcsr_api_func);
