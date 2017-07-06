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

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "ethos.h"

int queue_init(struct ethos_ctx *ctx)
{
	ctx->rssi = calloc(Q_ELEMENTS, sizeof(struct rssi_raw));
	if (!ctx->rssi)
		return -ENOMEM;

	ctx->q_head = 0;
	pthread_mutex_init(&ctx->q_lock, NULL);

	return 0;
}

void queue_destroy(struct ethos_ctx *ctx)
{
	free(ctx->rssi);
	pthread_mutex_destroy(&ctx->q_lock);
}

struct rssi_raw *queue_get(struct ethos_ctx *ctx)
{
	if (pthread_mutex_lock(&ctx->q_lock) != 0)
		return NULL;
	if ((++ctx->q_head) == Q_ELEMENTS)
		ctx->q_head = 0;
	return &ctx->rssi[ctx->q_head];
}

int queue_put(struct ethos_ctx *ctx)
{
	return pthread_mutex_unlock(&ctx->q_lock);
}
