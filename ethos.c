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
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mcp3004.h>
#include <pthread.h>
#include <wiringPi.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "ethos.h"
#include "queue.h"
#include "spi_bitbang.h"

#define BASE 100
#define SPI_CHAN 0
#define CFG_MAX_LINE 64
#define CFG_NUM_ELEMENTS 5

static void print_ts(struct timespec *ts)
{
	printf("%lld.%.6ld", (long long)ts->tv_sec, ts->tv_nsec/1000);
}

#if 0
static void ts_difference(struct timespec *start, struct timespec *stop,
		struct timespec *result)
{
	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}

	return;
}
#endif

static struct ethos_ctx *init_ctx(void)
{
	return calloc(1, sizeof(struct ethos_ctx));
}

static int read_config(struct ethos_ctx *ctx, char *path)
{
	int rc, i = 0, node, freq, low, hi, zc;
	uint32_t node_bitmap = 0;
	char buf[CFG_MAX_LINE];
	FILE *fp;
	char *p;

	fp = fopen(path, "r");
	if (fp == NULL)
		return -errno;

	while (!feof(fp)) {
		i++;
		fgets(buf, CFG_MAX_LINE, fp);
		p = buf;
		while (isblank(*p))
			p++;
		if (*p == '#' || *p == '\0')
			continue;
		rc = sscanf(buf, "%d %d %d %d %d",
			&node, &freq, &low, &hi, &zc);
		if (rc == EOF) {
			if (!errno)
				continue;
			rc = -errno;
			printf("%s: error while reading %s: %d\n",
				__func__, path, errno);
			return rc;
		}
		if (rc != CFG_NUM_ELEMENTS) {
			printf("%s: error: line %d only found %d element%s\n",
				__func__, i, rc, (rc == 1 ? "" : "s"));
			return -EINVAL;
		}

		ctx->settings.rx[node].freq = freq;
		ctx->settings.rx[node].thresh_low = low;
		ctx->settings.rx[node].thresh_hi = hi;
		ctx->settings.rx[node].zero_calibration = zc;
		node_bitmap |= 1 << node;
	}

	for (i = 0; i < NUM_RX; i++) {
		if (!(node_bitmap & (1 << i))) {
			printf("%s: config error: no setting for node %d\n",
				__func__, i);
			return -EINVAL;
		}
	}

	fclose(fp);

	return 0;
}

static void print_settings(struct ethos_ctx *ctx)
{
	int i;

	printf("global settings:\n");
	for (i = 0; i < NUM_RX; i++) {
		printf("node[%d]:\n", i);
		printf("  freq: %d\n", ctx->settings.rx[i].freq);
		printf("  thresh_low: %d\n", ctx->settings.rx[i].thresh_low);
		printf("  thresh_hi: %d\n", ctx->settings.rx[i].thresh_hi);
		printf("  zero_calibration: %d\n",
			ctx->settings.rx[i].zero_calibration);
	}
}

static int read_module(uint8_t module)
{
	return analogRead(BASE + module);
}

static int read_all_modules(struct rssi_raw *rssi)
{
	int i;

	for (i = 0; i < NUM_RX; i++) {
		rssi->rx[i] = read_module(i);
		if (clock_gettime(CLOCK_REALTIME, &rssi->ts[i]) != 0)
			return -errno;
	}
	return 0;
}

static void *rssi_thread(void *arg)
{
	struct ethos_ctx *ctx = arg;

	while(1) {
		struct rssi_raw *rssi;
		int rc;

		rssi = queue_get(ctx);
		if (rssi == NULL) {
			printf("Error: get_queue returned null (q_head = %d)\n",
				ctx->q_head);
			break;
		}
		rc = read_all_modules(rssi);
		queue_put(ctx);
		if (rc) {
			printf("Error: read_all_modules: %d (q_head = %d)\n",
				rc, ctx->q_head);
			break;
		}
	}
	pthread_exit(NULL);
}

static void *pass_thread(void *arg)
{
	struct ethos_ctx *ctx = arg;
	int i = 0;

	while(1) {
		struct rssi_raw *rssi = &ctx->rssi[i];
		int j;

		printf("cur_head: %d: q[%d]->\n", ctx->q_head, i);
		for (j = 0; j < NUM_RX; j++) {
			printf("    rssi[%d] = %d, ts = ", j, rssi->rx[j]);
			print_ts(&rssi->ts[j]);
			printf("\n");
		}

		if (++i == Q_ELEMENTS)
			i = 0;
	}
	pthread_exit(NULL);
}

static int init_hw_interfaces()
{
	int rc;

	rc = wiringPiSetup();
	if (rc != 0) {
		printf("Wiringpi setup failed: %d\n", rc);
		return rc;
	}
	rc = mcp3004Setup(BASE, SPI_CHAN);
	if (rc == FALSE) {
		printf("mcp3004 setup failed: %d\n", rc);
		return rc;
	}
	spibb_setup();

	return 0;
}

static int start_threads(struct ethos_ctx *ctx)
{
	int rc;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	rc = pthread_create(&ctx->threads[RSSI_READER], &attr, rssi_thread,
	        (void *)ctx);
	if (rc) {
		printf("%s: Failed to create RSSI_READER thread: %d\n",
			__func__, rc);
		return rc;
	}

	rc = pthread_create(&ctx->threads[PASS_DETECTOR], &attr, pass_thread,
	        (void *)ctx);
	if (rc) {
		printf("%s: Failed to create PASS_DETECTOR thread: %d\n",
			__func__, rc);
		return rc;
	}

	pthread_attr_destroy(&attr);

	return 0;
}

static void wait_threads(struct ethos_ctx *ctx)
{
	int i, rc;
	void *status;

	for (i = 0; i < NUM_THREADS; i++) {
		rc = pthread_join(ctx->threads[i], &status);
		if (rc) {
			printf("%s: pthread_join[%d] returned: %d\n",
				__func__, i, rc);
			return;
		}
		printf("%s: thread[%d]: joined with status %ld\n",
			__func__, i, (long)status);
	}
}

int main(int argc, char *argv[])
{
	struct ethos_ctx *ctx;
	int rc, i;

	if (argc < 2) {
		printf("Please provide a valid configuration file\n");
		return -EINVAL;
	}

	ctx = init_ctx();
	if (ctx == NULL)
		return -ENOMEM;

	rc = read_config(ctx, argv[1]);
	if (rc != 0) {
		printf("Initial config failed: %d\n", rc);
		goto out;
	}
	print_settings(ctx);

	rc = init_hw_interfaces();
	if (rc)
		goto out;

	rc = queue_init(ctx);
	if (rc)
		goto out;

	for (i = 0; i < NUM_RX; i++) {
		rc = spibb_set_module_ch(i, ctx->settings.rx[i].freq);
		if (rc != 0) {
			printf("  failed to set rx %d: %d\n", i, rc);
			goto out_queue;
		}
	}

	rc = start_threads(ctx);
	if (rc)
		goto out_queue;

	wait_threads(ctx);

out_queue:
	queue_destroy(ctx);
out:
	free(ctx);
	return rc;
}
