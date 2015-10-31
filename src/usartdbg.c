/*
 * usartdbg.c
 *
 * Created: 10/30/2015 9:41:54 PM
 *  Author: James
 */ 

#include <asf.h>
#include "usartdbg.h"
#include "main.h"

static const gpio_map_t usart_gpio_map = {
	{AVR32_USART3_RXD_0_2_PIN, AVR32_USART3_RXD_0_2_FUNCTION},
	{AVR32_USART3_TXD_0_3_PIN, AVR32_USART3_TXD_0_3_FUNCTION}
};

static const usart_options_t usart_options = {
	.baudrate     = 115200,
	.charlength   = 8,
	.paritytype   = USART_NO_PARITY,
	.stopbits     = USART_1_STOPBIT,
	.channelmode  = USART_NORMAL_CHMODE
};

void usartdbg_init(void)
{
	sysclk_enable_peripheral_clock(USART_DEBUG);
	gpio_enable_module(usart_gpio_map, sizeof(usart_gpio_map) / sizeof(usart_gpio_map[0]));
	usart_init_rs232(USART_DEBUG, &usart_options, FPBA_HZ);
}
