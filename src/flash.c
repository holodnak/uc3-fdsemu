/*
 * flash.c
 *
 * Created: 10/29/2015 8:02:54 PM
 *  Author: James
 */ 

#include <asf.h>
#include <stdio.h>
#include <string.h>
#include "flash.h"
#include "usartdbg.h"

static const gpio_map_t spi_gpio_map = {
	{AVR32_SPI1_MISO_0_0_PIN, AVR32_SPI1_MISO_0_0_FUNCTION},
	{AVR32_SPI1_MOSI_0_0_PIN, AVR32_SPI1_MOSI_0_0_FUNCTION},
	{AVR32_SPI1_NPCS_0_0_PIN, AVR32_SPI1_NPCS_0_0_FUNCTION},
	{AVR32_SPI1_SCK_0_0_PIN, AVR32_SPI1_SCK_0_0_FUNCTION}
};

static struct spi_device spi_flash = {
	.id = SPI_SLAVECHIP_NUMBER
};

static void get_flash_info(void)
{
	uint8_t data_out[4] = {0x90,0,0,0};
	uint8_t data_in[4] = {0x90};
	char tmp[64];

	// Select the DF memory to check.
	spi_select_device(SPI_FLASH, &spi_flash);

	// Send the Status Register Read command following by a dummy data.
	spi_write_packet(SPI_FLASH, data_out, 4);

	// Receive status.
	spi_read_packet(SPI_FLASH, data_in, 2);

	// Deselect the checked DF memory.
	spi_deselect_device(SPI_FLASH, &spi_flash);

	sprintf(tmp,"flash manufacturer id: $%02X, device id: $%02X\r\n",data_in[0],data_in[1]);
	usart_write_line(USART_DEBUG,tmp);

}

//keep waiting for chip to become not busy
static void flash_busy_wait(void)
{
	uint8_t data = 0x05;

	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, &data, 1);
	do {
		spi_read_packet(SPI_FLASH, &data, 1);
	} while(data & 1);
	spi_deselect_device(SPI_FLASH, &spi_flash);
}

static void block_erase(int block)
{
	uint8_t data[4];

	//enable writes
	data[0] = 0x06;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, &spi_flash);

	//erase block
	data[0] = 0xD8;
	data[1] = block;
	data[2] = 0x00;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_deselect_device(SPI_FLASH, &spi_flash);
	
	//wait for it to finish
	flash_busy_wait();
}

int flash_init(void)
{
	uint8_t data[4];

	sysclk_enable_peripheral_clock(SPI_FLASH);
	gpio_enable_module(spi_gpio_map, sizeof(spi_gpio_map) / sizeof(spi_gpio_map[0]));
	spi_master_init(SPI_FLASH);
	spi_master_setup_device(SPI_FLASH, &spi_flash, SPI_MODE_0, SPI_FLASH_BAUDRATE, 0);
	spi_enable(SPI_FLASH);
	get_flash_info();

	//enable reset
	data[0] = 0x66;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, &spi_flash);

	//reset
	data[0] = 0x99;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, &spi_flash);
	return(0);
}

static void flash_write_page(int block, int page, uint8_t *d,int len)
{
	uint8_t data[4];

	//enable writes
	data[0] = 0x06;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 1);
	spi_deselect_device(SPI_FLASH, &spi_flash);

	//write the data
	data[0] = 0x02;
	data[1] = block;
	data[2] = page;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_write_packet(SPI_FLASH, d, len);
	spi_deselect_device(SPI_FLASH, &spi_flash);
	
	//wait for write to end
	flash_busy_wait();
}

//write a disk to flash
void flash_write_disk(int block,flash_header_t *header,uint8_t *disk,int disklen)
{
	char tmp[64];
	int i,j,k;

	sprintf(tmp,"writing '%s' to flash block %d...\r\n",header->name,block);
	usart_write_line(USART_DEBUG,tmp);
	
	block_erase(block);

	//write the data
	flash_write_page(block,0,(uint8_t*)header,256);
	for(i=0,j=0;j<disklen;i++,j+=256) {
		if((disklen - j) < 256) {
			k = disklen - j;
		}
		else {
			k = 256;
		}
		flash_write_page(block,i + 1,disk + j,k);
	}
	usart_write_line(USART_DEBUG,"write completed.\r\n");
}

void flash_read_disk_header(int block,flash_header_t *header)
{
	uint8_t data[4];

	//write the data
	data[0] = 0x03;
	data[1] = block;
	data[2] = 0x00;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 4);
	spi_read_packet(SPI_FLASH, (uint8_t*)header, 256);
	spi_deselect_device(SPI_FLASH, &spi_flash);
}

void flash_read_disk_start(int block)
{
	uint8_t data[4];

	//read data
	data[0] = 0x03;
	data[1] = block;
	data[2] = 0x01;
	data[3] = 0x00;
	spi_select_device(SPI_FLASH, &spi_flash);
	spi_write_packet(SPI_FLASH, data, 4);
}

void flash_read_disk_stop(void)
{
	spi_deselect_device(SPI_FLASH, &spi_flash);
}

void flash_read_disk(uint8_t *data,int len)
{
	spi_read_packet(SPI_FLASH, data, len);
}
