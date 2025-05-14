// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025 Intel Corporation
 * These contents may have been developed with support from one or more
 * Intel-operated generative artificial intelligence solutions.
 */
#include "compress.h"

#include <crypto/acompress.h>
#include <linux/completion.h>
#include <linux/pagemap.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

static struct crypto_acomp *acomp_tfm;
static bool acomp_init_done;

int acomp_init(void)
{
	acomp_tfm = crypto_alloc_acomp("zlib-deflate", 0, 0);
	if (IS_ERR(acomp_tfm)) {
		pr_err("Failed to allocate zlib-deflate acomp transform\n");
		return PTR_ERR(acomp_tfm);
	}

	acomp_init_done = true;

	return 0;
}

void acomp_exit(void)
{
	if (!acomp_init_done)
		return;

	if (acomp_tfm) {
		crypto_free_acomp(acomp_tfm);
		acomp_tfm = NULL;
	}
	acomp_init_done = false;
}

enum comp_dir {
	DECOMPRESS = 0,
	COMPRESS = 1,
};

static int acomp_comp_decomp_sg(enum comp_dir dir, struct scatterlist *src, int src_len,
				struct scatterlist *dst, int dst_len, size_t *c_len)
{
	struct crypto_wait wait;
	struct acomp_req *req;
	int ret;

	pr_debug("[%s] dir: %s, src_len: %d, dst_len: %d\n", __func__,
		 dir == COMPRESS ? "COMPRESS" : "DECOMPRESS", src_len, dst_len);

	req = acomp_request_alloc(acomp_tfm);
	if (!req) {
		pr_err("Failed to allocate acomp request\n");
		return -ENOMEM;
	}

	crypto_init_wait(&wait);
	acomp_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   crypto_req_done, &wait);

	acomp_request_set_params(req, src, dst, src_len, dst_len);

	if (dir == COMPRESS)
		ret = crypto_acomp_compress(req);
	else
		ret = crypto_acomp_decompress(req);

	ret = crypto_wait_req(ret, &wait);
	if (ret)
		goto out;

	*c_len = req->dlen;

out:
	acomp_request_free(req);
	pr_debug("[%s] ret: %d, dlen=%zu\n", __func__, ret, *c_len);

	return ret;
}

static int vmalloc_to_sg(const void *vmem, size_t size, struct scatterlist *sgl, int nents)
{
	unsigned long vaddr = (unsigned long)vmem;
	unsigned long offset;
	size_t remaining = size;
	struct page *page;
	int sg_count = 0;

	sg_init_table(sgl, nents);

	while (remaining > 0) {
		page = vmalloc_to_page((void *)vaddr);
		if (!page) {
			pr_err("Failed to retrieve page for vaddr: 0x%lx\n", vaddr);
			return -EINVAL;
		}

		offset = vaddr & (PAGE_SIZE - 1);
		size_t len = min((size_t)(PAGE_SIZE - offset), remaining);

		sg_set_page(&sgl[sg_count], page, len, offset);

		vaddr += len;
		remaining -= len;
		sg_count++;

		if (sg_count >= nents && remaining > 0) {
			pr_err("Not enough scatterlist entries for memory size: %zu\n", size);
			return -ENOMEM;
		}
	}

	sg_mark_end(&sgl[sg_count - 1]);

	return 0;
}

static int count_pages(const void *vmem, size_t size)
{
	unsigned long vaddr = (unsigned long)vmem;
	size_t remaining = size;
	int page_count = 0;

	while (remaining > 0) {
		size_t len = min((size_t)(PAGE_SIZE - (vaddr & (PAGE_SIZE - 1))), remaining);

		vaddr += len;
		remaining -= len;
		page_count++;
	}

	return page_count;
}

static int alloc_scatterlist(const void *vmem, size_t size, struct scatterlist **sgl)
{
	struct scatterlist *sg = NULL;
	int ret = 0;
	int pages;

	if (!is_vmalloc_addr(vmem)) {
		sg = kmalloc(sizeof(*sg), GFP_KERNEL);
		if (!sg) {
			*sgl = NULL;
			return -ENOMEM;
		}

		sg_init_one(sg, vmem, size);

		*sgl = sg;
		return 0;
	}

	pages = count_pages(vmem, size);
	if (pages <= 0) {
		pr_err("Failed to count pages for memory\n");
		return -EINVAL;
	}

	sg = kmalloc_array(pages, sizeof(struct scatterlist), GFP_KERNEL);
	if (!sg)
		return -ENOMEM;

	memset(sg, 0, sizeof(struct scatterlist) * pages);

	ret = vmalloc_to_sg(vmem, size, sg, pages);
	if (ret) {
		pr_err("Failed to map vmalloc memory to scatterlist\n");
		kfree(sg);
		*sgl = NULL;
		return ret;
	}

	*sgl = sg;
	return ret;
}

static int acomp_comp_decomp(enum comp_dir dir, void *src, int src_len,
			     void *dst, int dst_len, size_t *c_len)
{
	struct scatterlist *src_sg, *dst_sg;
	int ret;

	ret = alloc_scatterlist(src, src_len, &src_sg);
	if (ret) {
		pr_err("Failed to allocate scatterlist for SRC memory\n");
		return ret;
	}

	ret = alloc_scatterlist(dst, dst_len, &dst_sg);
	if (ret) {
		pr_err("Failed to allocate scatterlist for DST memory\n");
		kfree(src_sg);
		return ret;
	}

	ret = acomp_comp_decomp_sg(dir, src_sg, src_len, dst_sg, dst_len, c_len);

	kfree(src_sg);
	kfree(dst_sg);

	return ret;
}

int acomp_compress(void *src, int src_len, void *dst, int dst_len, size_t *c_len)
{
	return acomp_comp_decomp(COMPRESS, src, src_len, dst, dst_len, c_len);
}

int acomp_decompress(void *src, int src_len, void *dst, int dst_len, size_t *c_len)
{
	return acomp_comp_decomp(DECOMPRESS, src, src_len, dst, dst_len, c_len);
}
