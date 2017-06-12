/*
 * Copyright(c) 2017 Vishal Verma. All rights reserved.
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

#ifndef _ETHOS_H_
#define _ETHOS_H_

#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <linux/types.h>

#define NUM_RX 8
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct rssi_raw {
	int rx[NUM_RX];
	struct timespec ts;
	pthread_mutex_t lock;
};

struct rx_settings {
	int freq;
	int thresh_hi;
	int thresh_low;
	int zero_calibration;
};

struct global_settings {
	struct rx_settings rx[NUM_RX];
};

struct ethos_ctx {
	struct global_settings settings;
};

#endif /* _ETHOS_H_ */
