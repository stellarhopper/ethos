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

#ifndef _QUEUE_H_
#define _QUEUE_H_

int queue_init(struct ethos_ctx *ctx);
void queue_destroy(struct ethos_ctx *ctx);
struct rssi_raw *queue_get(struct ethos_ctx *ctx);
int queue_put(struct ethos_ctx *ctx);

#endif /* _ETHOS_H_ */
