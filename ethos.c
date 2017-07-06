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

static void read_all_modules(void)
{
	int i, rssi[NUM_RX];

	for (i = 0; i < NUM_RX; i++)
		rssi[i] = read_module(i);

	printf("Got Module RSSIs:");
	for (i = 0; i < NUM_RX; i++)
		printf("%d, ", rssi[i]);
	printf("\n");
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

	for (i = 0; i < 10; i++)
		read_all_modules();

out_queue:
	queue_destroy(ctx);
out:
	free(ctx);
	return rc;
}
