/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2025 Intel Corporation
 * These contents may have been developed with support from one or more
 * Intel-operated generative artificial intelligence solutions.
 */
#ifndef _ACOMP_COMPRESS_H
#define _ACOMP_COMPRESS_H

#include <linux/types.h>

int acomp_init(void);
void acomp_exit(void);
int acomp_compress(void *src, int src_len, void *dst, int dst_len, size_t *c_len);
int acomp_decompress(void *src, int src_len, void *dst, int dst_len, size_t *c_len);

#endif /* _ACOMP_COMPRESS_H */
