/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# "Insert system clock initialization code here" comment
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

/*
TXS0108:
B1	-scanmedia
B2	write data
B3	motor on
B4	-writable
B5	read data
B6	-media set
B7	-ready
B8	-stop motor

Wires from UC3:
1- PA25 - purple	-scan media
2- PA26 - blue		write data
3- PX57 - green		motor on/battery
4- PX58 - yellow	-writable media
5- PB09 - orange	read data
6- PB10 - red		-media set
7- PB08 - brown		-ready
8- PB07 - black		-stop motor

5- PA07 - green
7- PA11 - orange
*/

#include <asf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "flash.h"
#include "fds.h"
#include "usartdbg.h"

//#include "tapedump-disk.c"
//#include "allnight-disk.c"
//#include "smb2-disk.c"
//#include "metroid1-disk.c"
//#include "metroid2-disk.c"

//uint8_t *diskdata;
//int disklen;

#define PIN_SCANMEDIA	AVR32_PIN_PA31
#define PIN_WRITEDATA	AVR32_PIN_PA30
#define PIN_MOTORON		AVR32_PIN_PA29
#define PIN_WRITABLE	AVR32_PIN_PA28
#define PIN_READ		AVR32_PIN_PA27
#define PIN_MEDIASET	AVR32_PIN_PB00
#define PIN_READY		AVR32_PIN_PB11
#define PIN_STOPMOTOR	AVR32_PIN_PX19

#define PIN_DATA		AVR32_PIN_PA25
#define PIN_RATE		AVR32_PIN_PA26

#define PIN_WRITE		AVR32_PIN_PA10

#define SET_READY()			gpio_set_pin_low(PIN_READY)
#define SET_MEDIASET()		gpio_set_pin_low(PIN_MEDIASET)
#define SET_WRITABLE()		gpio_set_pin_low(PIN_WRITABLE)
#define SET_MOTORON()		gpio_set_pin_high(PIN_MOTORON)


#define CLEAR_READY()		gpio_set_pin_high(PIN_READY)
#define CLEAR_MEDIASET()	gpio_set_pin_high(PIN_MEDIASET)
#define CLEAR_WRITABLE()	gpio_set_pin_high(PIN_WRITABLE)
#define CLEAR_MOTORON()		gpio_set_pin_low(PIN_MOTORON)

#define IS_SCANMEDIA()		(gpio_get_pin_value(PIN_SCANMEDIA) == false)
#define IS_STOPMOTOR()		(gpio_get_pin_value(PIN_STOPMOTOR) == false)
#define IS_DONT_STOPMOTOR()	(gpio_get_pin_value(PIN_STOPMOTOR) == true)


//#define TRANSFER_RATE	96400
#define TRANSFER_RATE	100000
#define FDS_KHZ		(FPBA_HZ / 2 / TRANSFER_RATE)

volatile int count = 0;

volatile uint8_t data, data2;
volatile int outbit = 0;
volatile int needbyte;
volatile int bytes;
int diskblock = 0;

__attribute__((__interrupt__)) static void tc1_irq(void)
{
	int sr = tc_read_sr(&AVR32_TC1, 1);
	
	//counter overflowed, it has been one cycle, rate is high
	if(sr & AVR32_TC_SR1_CPCS_MASK) {
		count++;
		if(count == 8) {
			count = 0;
			data = data2;
			needbyte++;
		}
		outbit = data & 1;
		data >>= 1;
		if(outbit == 0) {
			gpio_set_pin_low(PIN_DATA);
		}
		else {
			gpio_set_pin_high(PIN_DATA);
		}
		gpio_set_pin_high(PIN_RATE);
	}
	
	//half of one read cycle, rate is low
	else if(sr & AVR32_TC_SR1_CPAS_MASK) {
		gpio_set_pin_low(PIN_RATE);
	}
}

static void set_cpu_hz(void)
{
	// note that fpba_hz will be adjusted during runtime
	pcl_freq_param_t pcl_freq_param =
	{
		.cpu_f = FCPU_HZ,
		.pba_f = FPBA_HZ,
		.osc0_f = FOSC0,
		.osc0_startup = OSC0_STARTUP
	};

	pcl_configure_clocks(&pcl_freq_param);
}

static void init_timers(void)
{
	volatile avr32_tc_t *tc1 = &AVR32_TC1;

	//tc1-a2
	tc_waveform_opt_t waveform_opt = {
		.channel  = 1,
		.bswtrg   = TC_EVT_EFFECT_NOOP,
		.beevt    = TC_EVT_EFFECT_NOOP,
		.bcpc     = TC_EVT_EFFECT_NOOP,
		.bcpb     = TC_EVT_EFFECT_NOOP,
		.aswtrg   = TC_EVT_EFFECT_NOOP,
		.aeevt    = TC_EVT_EFFECT_NOOP,
		.acpc     = TC_EVT_EFFECT_NOOP,
		.acpa     = TC_EVT_EFFECT_NOOP,
		.wavsel   = TC_WAVEFORM_SEL_UP_MODE_RC_TRIGGER,
		.enetrg   = false,
		.eevt     = 0,
		.eevtedg  = TC_SEL_NO_EDGE,
		.cpcdis   = false,
		.cpcstop  = false,
		.burst    = false,
		.clki     = false,
		.tcclks   = TC_CLOCK_SOURCE_TC2
	};

	static const tc_interrupt_t TC_INTERRUPT = {
		.etrgs = 0,
		.ldrbs = 0,
		.ldras = 0,
		.cpcs  = 1,
		.cpbs  = 0,
		.cpas  = 1,
		.lovrs = 0,
		.covfs = 0
	};

	cpu_irq_disable();

	INTC_init_interrupts();
	INTC_register_interrupt(&tc1_irq, AVR32_TC1_IRQ1, AVR32_INTC_INT0);
	
	tc_init_waveform(tc1, &waveform_opt);
	tc_write_rc(tc1, 1, FDS_KHZ);
	tc_write_ra(tc1, 1, FDS_KHZ / 2);
	tc_write_rb(tc1, 1, 0);
	tc_configure_interrupts(tc1, 1, &TC_INTERRUPT);
	tc_start(tc1, 1);
}

static void begin_transfer_data(void)
{
	char tmp[64];
	
	usart_write_line(USART_DEBUG,"beginning transfer...\r\n");
	count = 7;


	flash_read_disk_start(diskblock);
	needbyte = 0;
	bytes = 1;
	flash_read_disk((uint8_t)&data2,1);
	
	cpu_irq_enable();
	while(IS_SCANMEDIA() && IS_DONT_STOPMOTOR()) {
		if(needbyte) {
			needbyte = 0;
			bytes++;
			flash_read_disk((uint8_t)&data2,1);
		}
		if(bytes >= 0xFF00) {
			usart_write_line(USART_DEBUG,"reached end of data block, something went wrong...\r\n");
			break;
		}
	}
	cpu_irq_disable();

	flash_read_disk_stop();

	sprintf(tmp,"transferred %d bytes\r\n",bytes);
	usart_write_line(USART_DEBUG,tmp);
}

void write_to_flash(int block, char *name, int next, uint8_t *data, int length)
{
	flash_header_t header,header2;
	char poo[512];
			
	memset((void*)&header,0,256);
	strcpy(header.name,name);
	header.lead_in = 0;
	header.next_disk = next;
	flash_write_disk(block,&header,data,length);
	flash_read_disk_header(block,&header2);
	sprintf(poo,"wrote disk '%s' to flash.\r\n",header2.name);
	usart_write_line(USART_DEBUG,poo);
}

static void print_block_info(int block)
{
	char poo[512];
	flash_header_t header2;

	flash_read_disk_header(block,&header2);
	if(header2.name[0] == 0xFF) {
		sprintf(poo,"block %d: empty\r\n",block);
	}
	else {
		sprintf(poo,"block %d: '%s'\r\n",block,header2.name);
	}
	usart_write_line(USART_DEBUG,poo);
}

int main (void)
{
//	diskdata = (const uint8_t*)tapedump;
//	disklen = tapedump_length;
//	diskdata = (const uint8_t*)allnight;
//	disklen = allnight_length;

	/* Insert system clock initialization code here (sysclk_init()). */

	board_init();
	
	set_cpu_hz();
	
	sysclk_enable_peripheral_clock(&AVR32_TC1);

	usartdbg_init();
	flash_init();

	init_timers();

	cpu_irq_disable();

	usart_write_line(USART_DEBUG,"\r\n\r\nuc3-fdsemu started.\r\n");

	gpio_configure_pin(PIN_WRITE,		GPIO_DIR_INPUT);
	gpio_configure_pin(PIN_SCANMEDIA,	GPIO_DIR_INPUT);
	gpio_configure_pin(PIN_WRITEDATA,	GPIO_DIR_INPUT);
	gpio_configure_pin(PIN_STOPMOTOR,	GPIO_DIR_INPUT);

	gpio_configure_pin(PIN_MOTORON,		GPIO_DIR_OUTPUT);
	gpio_configure_pin(PIN_WRITABLE,	GPIO_DIR_OUTPUT);
	gpio_configure_pin(PIN_READ,		GPIO_DIR_OUTPUT);
	gpio_configure_pin(PIN_READY,		GPIO_DIR_OUTPUT);
	gpio_configure_pin(PIN_MEDIASET,	GPIO_DIR_OUTPUT);

	gpio_configure_pin(PIN_RATE,		GPIO_DIR_OUTPUT);
	gpio_configure_pin(PIN_DATA,		GPIO_DIR_OUTPUT);

	CLEAR_WRITABLE();
	CLEAR_READY();
	CLEAR_MEDIASET();
	CLEAR_MOTORON();

//	cpu_delay_ms(2000,FCPU_HZ);
//	write_to_flash(0,"All Night Nippon",0xFFFF,allnight,allnight_length);

/*	cpu_delay_ms(2000,FCPU_HZ);
	write_to_flash(0,"All Night Nippon",0xFFFF,allnight,allnight_length);
	write_to_flash(1,"Metroid",3,metroid1,metroid1_length);
	//write_to_flash(2,"Super Mario Brothers 2",0xFFFF,smb2,smb2_length);
	write_to_flash(3,"Metroid",1,metroid2,metroid2_length);
	write_to_flash(4,"Tapedump",0xFFFF,tapedump,tapedump_length);
	write_to_flash(5,"All Night Nippon",0xFFFF,allnight,allnight_length);

	{
		int n;

		for(n=0;n<8;n++) {
			print_block_info(n);
		}
	}*/


	while(1) {
		int ch;
		
		if(usart_read_char(USART_DEBUG,&ch) == USART_SUCCESS) {
			char tmp[64];
			char help[] = 
				"help:\r\n"
				"  0-7 : select block to read disk data from\r\n"
				"  i   : insert disk\r\n"
				"  r   : remove disk\r\n"
				"  f   : flip disk to next side/disk\r\n"
				"  p   : print disks stored in flash\r\n"
				"\r\n";
			int n;

			switch((char)ch) {
				case '?':
					usart_write_line(USART_DEBUG,help);
					sprintf(tmp,"currently selected disk in block is %d.\r\n\r\n",diskblock);
					usart_write_line(USART_DEBUG,tmp);
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					diskblock = ch - '0';
					sprintf(tmp,"selected disk in block %d.\r\n",diskblock);
					usart_write_line(USART_DEBUG,tmp);
					break;
				case 'i':
					usart_write_line(USART_DEBUG,"inserting disk\r\n");
					SET_MEDIASET();
					break;
				case 'r':
					usart_write_line(USART_DEBUG,"removing disk\r\n");
					CLEAR_MEDIASET();
					break;
				case 'p':
					for(n=0;n<8;n++) {
						print_block_info(n);
					}
				break;									
			}
		}
		gpio_toggle_pin(LED0_GPIO);

		//disk inserted
		if(gpio_get_pin_value(GPIO_PUSH_BUTTON_0) == 0) {
			usart_write_line(USART_DEBUG,"inserting disk\r\n");
			SET_MEDIASET();
		}

		//check if ram adaptor wants to stop the motor
		if(IS_STOPMOTOR()) {
//			usart_write_line(USART_DEBUG,"ram adaptor stopping motor.\r\n");
			CLEAR_MOTORON();
		}

		//if ram adaptor wants to scan the media
		if(IS_SCANMEDIA()) {
//			usart_write_line(USART_DEBUG,"ram adaptor wants to scan the media.\r\n");

			//if ram adaptor doesnt want to stop the motor
			if(IS_DONT_STOPMOTOR()) {
				usart_write_line(USART_DEBUG,"ram adaptor wants the motor to run, we will send data...\r\n");
				//prepare for transfer
				
				//turn motor on
				SET_MOTORON();
				
				//delay for something like 14354 bit transfers (0.15 seconds)
				cpu_delay_ms(150,FCPU_HZ);
				
				//tell ram adaptor data is coming out of the disk drive
				SET_READY();

				//begin transferring data
				begin_transfer_data();

				CLEAR_READY();
			}
		}
	}
}
