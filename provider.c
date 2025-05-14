// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025 Intel Corporation
 * These contents may have been developed with support from one or more
 * Intel-operated generative artificial intelligence solutions.
 */
#include <crypto/acompress.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "compress.h"
#include <dpusm/provider_api.h>

enum handle_type {
	ZQH_REAL,
	ZQH_REF,
};

struct provider_handle {
	enum handle_type type;
	void *ptr;
	size_t size;
};

static void *ptr_start(struct provider_handle *zqh, const size_t offset)
{
	return (char *)zqh->ptr + offset;
}

static int zfs_acomp_algos(int *compress, int *decompress, int *checksum,
			   int *checksum_byteorder, int *raid)
{
	*compress = DPUSM_COMPRESS_GZIP_1 | DPUSM_COMPRESS_GZIP_2 |
		    DPUSM_COMPRESS_GZIP_3 | DPUSM_COMPRESS_GZIP_4 |
		    DPUSM_COMPRESS_GZIP_5 | DPUSM_COMPRESS_GZIP_6 |
		    DPUSM_COMPRESS_GZIP_7 | DPUSM_COMPRESS_GZIP_8 |
		    DPUSM_COMPRESS_GZIP_9;

	*decompress = *compress;

	*checksum = 0;
	*checksum_byteorder = 0;
	*raid = 0;

	return DPUSM_OK;
}

static void *zfs_acomp_alloc(size_t size)
{
	struct provider_handle *buf;

	buf = kmalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->type = ZQH_REAL;
	buf->ptr = kmalloc(size, GFP_KERNEL);
	if (!buf->ptr) {
		kfree(buf);
		return NULL;
	}

	buf->size = size;

	return buf;
}

static void *zfs_acomp_alloc_ref(void *handle, size_t offset, size_t size)
{
	struct provider_handle *src = handle;
	struct provider_handle *ref;

	ref = kmalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return NULL;

	ref->type = ZQH_REF;
	ref->ptr = ptr_start(src, offset);
	ref->size = size;

	return ref;
}

static int zfs_acomp_get_size(void *handle, size_t *size, size_t *actual)
{
	struct provider_handle *buf = handle;

	if (size)
		*size = buf->size;

	if (actual)
		*actual = buf->size;

	return DPUSM_OK;
}

static int zfs_acomp_free(void *handle)
{
	struct provider_handle *buf = handle;

	if (buf->type == ZQH_REAL)
		kfree(buf->ptr);

	kfree(buf);

	return DPUSM_OK;
}

static int zfs_acomp_copy_from_generic(dpusm_mv_t *mv, const void *buf, size_t size)
{
	struct provider_handle *dst = mv->handle;

	/*
	 * If the handle points to a real buffer, free it,
	 * and convert the handle to a reference to the
	 * user's pointer.
	 */
	if (dst->type == ZQH_REAL && mv->offset == 0) {
		kfree(dst->ptr);

		dst->type = ZQH_REF;
		dst->ptr = (void *)buf;
		dst->size = size;
	} else {
		memcpy(ptr_start(dst, mv->offset), buf, size);
	}

	return DPUSM_OK;
}

static int zfs_acomp_copy_to_generic(dpusm_mv_t *mv, void *buf, size_t size)
{
	struct provider_handle *src = mv->handle;
	void *start = ptr_start(src, mv->offset);

	/*
	 * if the handle is still a real buffer, it is not
	 * the same as buf, so the contents have to be copied
	 * into buf.
	 */
	if (start != buf)
		memcpy(buf, start, size);

	return DPUSM_OK;
}

static int zfs_acomp_compress(dpusm_compress_t alg, int level, void *src,
			      size_t s_len, void *dst, size_t *d_len)
{
	(void)alg;
	(void)level;

	struct provider_handle *s = src;
	struct provider_handle *d = dst;
	void *s_start;
	void *d_start;
	int ret;

	if (s_len > s->size || *d_len > d->size)
		return DPUSM_ERROR;

	s_start = ptr_start(s, 0);
	d_start = ptr_start(d, 0);

	ret = acomp_compress(s_start, s_len, d_start, *d_len, d_len);
	if (ret)
		return DPUSM_ERROR;

	/*
	 * TODO: understand what to do in case of overflow. In such case, ret
	 * is equals to -EOVERFLOW
	 */

	return DPUSM_OK;
}

static int zfs_acomp_decompress(dpusm_compress_t alg, int *level, void *src,
				size_t s_len, void *dst, size_t *d_len)
{
	(void)alg;
	(void)level;

	struct provider_handle *s = src;
	struct provider_handle *d = dst;
	void *s_start;
	void *d_start;
	int ret;

	if (s_len > s->size || *d_len > d->size)
		return DPUSM_ERROR;

	s_start = ptr_start(s, 0);
	d_start = ptr_start(d, 0);

	ret = acomp_decompress(s_start, s_len, d_start, *d_len, d_len);
	if (ret)
		return DPUSM_ERROR;

	return DPUSM_OK;
}

static const dpusm_pf_t zfs_acomp_provider_functions = {
	.algorithms = zfs_acomp_algos,
	.alloc = zfs_acomp_alloc,
	.alloc_ref = zfs_acomp_alloc_ref,
	.get_size = zfs_acomp_get_size,
	.free = zfs_acomp_free,
	.copy = {
			.from = {
					.generic = zfs_acomp_copy_from_generic,
					.ptr = NULL,
					.scatterlist = NULL,
				},
			.to =   {
					.generic = zfs_acomp_copy_to_generic,
					.ptr = NULL,
					.scatterlist = NULL,
				},
		},
	.compress = zfs_acomp_compress,
	.decompress = zfs_acomp_decompress,
	.at_connect = NULL,
	.at_disconnect = NULL,
	.mem_stats = NULL,
};

static int __init zfs_acomp_provider_init(void)
{
	int ret;

	ret = acomp_init();
	if (ret) {
		pr_err("%s: Error: Could not initialize provider\n",
		       module_name(THIS_MODULE));
		return ret;
	}

	return dpusm_register_gpl(THIS_MODULE, &zfs_acomp_provider_functions);
}

static void __exit zfs_acomp_provider_exit(void)
{
	dpusm_unregister_gpl(THIS_MODULE);

	acomp_exit();
}

module_init(zfs_acomp_provider_init);
module_exit(zfs_acomp_provider_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel");
MODULE_DESCRIPTION("DPUSM ACOMP Provider");
