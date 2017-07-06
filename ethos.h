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
#define NUM_THREADS 4
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define Q_ELEMENTS 64

enum threads {
	RSSI_READER = 0,
	PASS_DETECTOR,
	NET_HANDLER,
	CMD_HANDLER,
};

struct rssi_raw {
	int rx[NUM_RX];
	struct timespec ts[NUM_RX];
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
	pthread_t threads[NUM_THREADS];
	struct rssi_raw *rssi;
	pthread_mutex_t q_lock;
	int q_head;
};

#endif /* _ETHOS_H_ */
