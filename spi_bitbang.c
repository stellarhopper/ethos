/*
 * Copyright(c) 2017 Vishal Verma. All rights reserved.
 * SPI driver based on fs_skyrf_58g-main.c by Simon Chambers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <linux/types.h>

#include "ethos.h"
#include "spi_bitbang.h"

#define spi_bit_delay 1
#define spi_cs_delay 1

void spibb_setup(void)
{
	int i;

	/* NOTE: Assumes wiringPiSetup() has been done */
	for (i = 0; i < NUM_RX; i++) {
		pinMode(rx_cs[i], OUTPUT);
		digitalWrite(rx_cs[i], HIGH);
	}
	
	pinMode(rx_data, OUTPUT);
	pinMode(rx_clk, OUTPUT);
}

static void spibb_sendbit_1() {
	digitalWrite(rx_clk, LOW);
	delayMicroseconds(spi_bit_delay);

	digitalWrite(rx_data, HIGH);
	delayMicroseconds(spi_bit_delay);
	digitalWrite(rx_clk, HIGH);
	delayMicroseconds(spi_bit_delay);

	digitalWrite(rx_clk, LOW);
	delayMicroseconds(spi_bit_delay);
}

static void spibb_sendbit_0() {
	digitalWrite(rx_clk, LOW);
	delayMicroseconds(spi_bit_delay);

	digitalWrite(rx_data, LOW);
	delayMicroseconds(spi_bit_delay);
	digitalWrite(rx_clk, HIGH);
	delayMicroseconds(spi_bit_delay);

	digitalWrite(rx_clk, LOW);
	delayMicroseconds(spi_bit_delay);
}

static void spibb_cs_enable(int module) {
	delayMicroseconds(spi_cs_delay);
	digitalWrite(rx_cs[module], LOW); 
	delayMicroseconds(spi_cs_delay);
}

static void spibb_cs_disable(int module) {
	delayMicroseconds(spi_cs_delay); 
	digitalWrite(rx_cs[module], HIGH); 
	delayMicroseconds(spi_cs_delay);
}

static uint16_t freq_to_table(uint16_t freq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(freq_table); i++)
		if (freq_table[i] == freq)
			return channel_table[i];
	return 0;
}

int spibb_set_module_ch(uint8_t module, uint16_t freq)
{
	uint16_t channel_data = freq_to_table(freq);
	int i;

	if (module >= NUM_RX)
		return -EINVAL;
	if (channel_data == 0)
		return -EINVAL;

	printf("%s: setting rx[%d] to %d\n", __func__, module, freq);
	/*
	 * bitbang out 25 bits of data
	 * Order: A0-3, !R/W, D0-D19
	 * A0=0, A1=0, A2=0, A3=1, RW=0, D0-19=0
	 */
	spibb_cs_disable(module);
	delay(1);
	spibb_cs_enable(module);

	spibb_sendbit_0();
	spibb_sendbit_0();
	spibb_sendbit_0();
	spibb_sendbit_1();

	spibb_sendbit_0();

	/* remaining zeros */
	for (i = 20; i > 0; i--)
		spibb_sendbit_0();

	/* Clock the data in */
	spibb_cs_disable(module);
	delay(1);
	spibb_cs_enable(module);

	/*
	 * Second is the channel data from the lookup table
	 * 20 bytes of register data are sent, but the MSB 4 bits are zeros
	 * register address = 0x1, write, data0-15=channel_data data15-19=0x0
	 */
	spibb_cs_disable(module);
	spibb_cs_enable(module);

	/* Register 0x1 */
	spibb_sendbit_1();
	spibb_sendbit_0();
	spibb_sendbit_0();
	spibb_sendbit_0();

	/* Write to register */
	spibb_sendbit_1();

	/* D0-D15 */
	/*   note: loop runs backwards as more efficent on AVR */
	for (i = 16; i > 0; i--) {
		/* Is bit high or low? */
		if (channel_data & 0x1) {
			spibb_sendbit_1();
		}
		else {
			spibb_sendbit_0();
		}
		/* Shift bits along to check the next one */
		channel_data >>= 1;
	}

	/* Remaining D16-D19 */
	for (i = 4; i > 0; i--)
		spibb_sendbit_0();

	/* Finished clocking data in */
	spibb_cs_disable(module);
	delay(1);

	digitalWrite(rx_cs[module], LOW); 
	digitalWrite(rx_clk, LOW);
	digitalWrite(rx_data, LOW);

	return 0;
}
