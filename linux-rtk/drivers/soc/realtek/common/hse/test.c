#define pr_fmt(fmt)  "hse-test: " fmt
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <soc/realtek/rtk_chip.h>
#include "hse.h"
#include "semem.h"

#define MEM_SIZE              (4*PAGE_SIZE)
#define MEM_NUM               (6)

struct hse_test_buf {
	int type;
	dma_addr_t phys;
	void *virt;
	int size;
};

static void hse_memset(struct hse_test_buf *buf, int val, size_t size)
{
	if (buf->type == 0) {
		memset(buf->virt, val, size);
	} else {
		// TODO
		WARN_ON_ONCE(1);
	}
}

static void hse_test_buf_free(struct device *dev,
			      struct hse_test_buf *bufs,
			      int mem_num)
{
	int i;

	for (i = 0; i < mem_num; i++) {
		if (!bufs[i].virt)
			continue;
		if (bufs[i].type) {
			semem_free(bufs[i].virt);
		} else {
			dma_free_coherent(dev, bufs[i].size, bufs[i].virt,
				bufs[i].phys);
		}
	}
	kfree(bufs);
}

static __must_check struct hse_test_buf *hse_test_buf_alloc(struct device *dev,
							    size_t mem_num,
							    int size,
							    bool secure_en)
{
	struct hse_test_buf *bufs;
	int i;

	bufs = kcalloc(mem_num, sizeof(*bufs), GFP_KERNEL);
	if (!bufs)
		return NULL;


	for (i = 0; i < mem_num; i++) {
		if (secure_en) {
			bufs[i].type = 1;
			bufs[i].size = size;
			bufs[i].virt = semem_alloc(size, &bufs[i].phys);
		} else {
			bufs[i].type = 0;
			bufs[i].size = size;
			bufs[i].virt = dma_alloc_coherent(dev, size, &bufs[i].phys,
				GFP_KERNEL | GFP_DMA);
		}
		dev_err(dev, "mem%d: type:%d, phys:%pad, virt=%p\n", i,
			bufs[i].type, &bufs[i].phys, bufs[i].virt);
		if (!bufs[i].virt)
			goto error;
	}

	return bufs;
error:
	hse_test_buf_free(dev, bufs, mem_num);
	return NULL;
}

struct hse_test_obj {
	int pass;
	int fail;
	int total;
	struct hse_test_buf *bufs;
	struct hse_engine *eng;
};

static __must_check struct hse_test_obj *hse_test_obj_alloc(struct hse_engine *eng)
{
	struct hse_test_obj *hobj;
	struct hse_device *hdev = eng->hdev;
	struct hse_test_buf *bufs;
	int secure_buf = eng->base_offset < 0x200 ? 1 : 0;

	hobj = kzalloc(sizeof(*hobj), GFP_KERNEL);
	if (!hobj) {
		pr_err("fail to alloc test_obj\n");
		return NULL;
	}

	bufs = hse_test_buf_alloc(hdev->dev, MEM_NUM, MEM_SIZE, secure_buf);
	if (!bufs) {
		pr_err("failed to alloc buf\n");
		kfree(hobj);
		return NULL;
	}

	hobj->bufs = bufs;
	hobj->eng = eng;
	return hobj;
}

static void hse_test_obj_free(struct hse_test_obj *hobj)
{
	hse_test_buf_free(hobj->eng->hdev->dev, hobj->bufs, MEM_NUM);
	kfree(hobj);
}

static int hse_test_run_cmd(struct hse_engine *eng, u32 *cmds, u32 size)
{
	struct hse_command_queue *cq;

	cq = hse_cq_alloc(eng->hdev);
	if (!cq)
		return -ENOMEM;

	hse_cq_add_data(cq, cmds, size);
	hse_cq_pad(cq);
	hse_engine_execute_cq(eng, cq);
	hse_cq_free(cq);
	return 0;
}

#define HSE_ROTATE_90         0
#define HSE_ROTATE_180        1
#define HSE_ROTATE_270        2

static int hse_rotate_verify_args(u32 mode,
				  u32 width,
				  u32 height,
				  u32 src_pitch,
				  u32 dst_pitch)
{
	if (mode != 0 && mode != 1 && mode != 2) {
		pr_err("%s: invalid mode(%u)\n", __func__, mode);
		return -EINVAL;
	}
	if (width & 0xf) {
		pr_err("%s: width(%u) is not aligned\n", __func__, width);
		return -EINVAL;
	}
	if (height & 0xf) {
		pr_err("%s: height(%u) is not aligned\n", __func__, height);
		return -EINVAL;
	}
	if (src_pitch & 0xf) {
		pr_err("%s: src_pitch(%u) is not aligned\n", __func__, src_pitch);
		return -EINVAL;
	}
	if (dst_pitch & 0xf) {
		pr_err("%s: dst_pitch(%u) is not aligned\n", __func__, dst_pitch);
		return -EINVAL;
	}
	if (src_pitch < width) {
		pr_err("%s: src_pitch(%u) is less than width(%u)\n", __func__, src_pitch, width);
		return -EINVAL;
	}
	if ((mode == 0 || mode == 2) && dst_pitch < height) {
		 pr_err("%s: dst_pitch(%u) is less than height(%u) when rotate 90/270 degrees\n",
			__func__, dst_pitch, height);
		 return -EINVAL;
	}
	if (mode == 1 && dst_pitch < width) {
		 pr_err("%s: dst_pitch(%u) is less than width(%u) when rotate 180 degrees\n",
			__func__, dst_pitch, width);
		 return -EINVAL;
	}
	return 0;
}

static int hse_rotate_verify_result(struct hse_test_buf *dst,
			 struct hse_test_buf *src,
			 u32 mode,
			 u32 width,
			 u32 height,
			 u32 src_pitch,
			 u32 dst_pitch)
{
	u32 x_size;
	u32 y_size;
	u32 x_bound;
	u8 *s, *d;
	u8 *p;
	int i, j;
	int err;
	u32 idx;

	switch (mode) {
	case HSE_ROTATE_90:
	case HSE_ROTATE_270:
		x_size = dst_pitch;
		y_size = width;
		x_bound = height;
		break;
	case HSE_ROTATE_180:
		x_size = dst_pitch;
		y_size = height;
		x_bound = width;
		break;
	default:
		pr_err("%s: invalid mode\n", __func__);
		return -EINVAL;
	}

	p = kzalloc(x_size * y_size, GFP_KERNEL);
	if (!p) {
		pr_err("%s: failed to alloc memory\n", __func__);
		return -ENOMEM;
	}

	/* do sw rotate */
	s = src->virt;
	if (mode == HSE_ROTATE_90) {
		/*
		i j -> x y
		0 0 -> 3 0
		0 1 -> 2 0
		...
		1 0 -> 3 1
		1 1 -> 2 1
		...
		*/
		for (i = 0; i < height; i++)
			for (j = 0; j < width; j++)
				p[(y_size - 1 - j) * x_size + i] = s[i * src_pitch + j];
	}
	else if (mode == HSE_ROTATE_270) {
		/*
		i j -> x y
		0 0 -> 0 3
		0 1 -> 1 3
		...
		1 0 -> 0 2
		1 1 -> 1 2
		...
		*/
		for (i = 0; i < height; i++)
			for (j = 0; j < width; j++)
				p[(j) * x_size + width - 1 - i] = s[i * src_pitch + j];
	}
	else if (mode == HSE_ROTATE_180) {
		/*
		i j -> x y
		0 0 -> 3 3
		0 1 -> 3 2
		...
		1 0 -> 2 3
		1 1 -> 2 2
		...
		*/
		for (i = 0; i < height; i++)
			for (j = 0; j < width; j++)
				p[(y_size - 1 - i) * x_size + width - 1 - j] = s[i * src_pitch + j];
	}

	/* compare */
	d = dst->virt;
	err = 0;
	for (i = 0; i < y_size; i++)
		for (j = 0; j < x_bound; j++) {
			idx = x_size * i + j;

			if (d[idx] != p[idx]) {
				err = 1;
				break;
			}
		}
	if (err) {
		pr_err("%s: error => hw[%u], sw[%u]\n", __func__, idx, idx);
		idx &= ~0x1f;
		pr_err("%s: dump hw=%p+%04x\n", __func__, d, idx);
		print_hex_dump(KERN_ERR, "hw ", DUMP_PREFIX_OFFSET, 16, 1, d+idx, 0x20, false);
		pr_err("%s: dump sw=%p+%04x\n", __func__, p, idx);
		print_hex_dump(KERN_ERR, "sw ", DUMP_PREFIX_OFFSET, 16, 1, p+idx, 0x20, false);
	}
	kfree(p);

	return err;

}

static int hse_rotate(struct hse_engine *eng,
		      struct hse_test_buf *dst,
		      struct hse_test_buf *src,
		      u32 mode,
		      u32 width,
		      u32 height,
		      u32 src_pitch,
		      u32 dst_pitch)
{
	u32 cmds[32];

	if (hse_rotate_verify_args(mode, width, height, src_pitch, dst_pitch))
		return -EINVAL;

	cmds[0] = 0x05 | (mode << 29);
	cmds[1] = height | (width << 16);
	cmds[2] = dst_pitch | (src_pitch << 16);
	cmds[3] = dst->phys;
	cmds[4] = src->phys;
	return hse_test_run_cmd(eng, cmds, 5);
}

static int hse_yuy2_to_nv12_verify_args(u32 width,
					u32 height,
					u32 src_pitch,
					u32 dst_pitch)
{
	if (width & 0xf) {
		pr_err("%s: width(%u) is not aligned\n", __func__, width);
		return -EINVAL;
	}
	if (height & 0xf) {
		pr_err("%s: height(%u) is not aligned\n", __func__, height);
		return -EINVAL;
	}
	if (src_pitch & 0xf) {
		pr_err("%s: src_pitch(%u) is not aligned\n", __func__, src_pitch);
		return -EINVAL;
	}
	if (dst_pitch & 0xf) {
		pr_err("%s: dst_pitch(%u) is not aligned\n", __func__, dst_pitch);
		return -EINVAL;
	}
	if (src_pitch < width) {
		pr_err("%s: src_pitch(%u) is less than width(%u)\n", __func__, src_pitch, width);
		return -EINVAL;
	}
	if (dst_pitch < width/2) {
		pr_err("%s: dst_pitch(%u) is less than half of width(%u)\n", __func__, src_pitch, width);
		return -EINVAL;
	}
	return 0;
}

static int hse_yuy2_to_nv12_verify_result(struct hse_test_buf *dst0,
					  struct hse_test_buf *dst1,
					  struct hse_test_buf *src,
					  u32 width,
					  u32 height,
					  u32 src_pitch,
					  u32 dst_pitch)
{
	int i, j, k;
	int w, h;
	u8 *s0 = src->virt;
	u8 *d0 = dst0->virt;
	u8 *d1 = dst1->virt;
	int err = 0;

	/* compare */
	i = j = k = 0;
	for (h = 0; h < height; h++) {
		for (w = 0; w < width; w+=4) {
			if (s0[i] != d0[j]) {
				err = 1;
				break;
			}
			i++; j++;
			if (s0[i] != d1[k]) {
				err = 1;
				break;
			}
			i++; k++;
			if (s0[i] != d0[j]) {
				err = 1;
				break;
			}
			i++; j++;
			if (s0[i] != d1[k]) {
				err = 1;
				break;
			}
			i++; k++;
		}
		i += (src_pitch - width);
		j += (dst_pitch - width / 2);
		k += (dst_pitch - width / 2);
	}

	if (err){
		pr_err("%s: error => src[%d],y[%d],uv[%d]\n", __func__, i, j, k);
		i &= ~0x1f;
		j &= ~0x1f;
		k &= ~0x1f;
		pr_err("%s: dump src=%p+%04x\n", __func__, s0, i);
		print_hex_dump(KERN_ERR, "src ", DUMP_PREFIX_OFFSET, 16, 1, s0+i, 0x20, false);
		pr_err("%s: dump y=%p+%04x\n", __func__, d0, j);
		print_hex_dump(KERN_ERR, "y   ", DUMP_PREFIX_OFFSET, 16, 1, d0+j, 0x20, false);
		pr_err("%s: dump uv=%p+%04x\n", __func__, d1, k);
		print_hex_dump(KERN_ERR, "uv  ", DUMP_PREFIX_OFFSET, 16, 1, d1+k, 0x20, false);
	}

	return err;
}

static int hse_yuy2_to_nv12(struct hse_engine *eng,
			    struct hse_test_buf *dst0,
			    struct hse_test_buf *dst1,
			    struct hse_test_buf *src,
			    u32 width,
			    u32 height,
			    u32 src_pitch,
			    u32 dst_pitch)
{
	u32 cmds[32];

	if (hse_yuy2_to_nv12_verify_args(width, height, src_pitch, dst_pitch))
		return -EINVAL;

	cmds[0] = 0x2;
	cmds[1] = height | (width << 16);
	cmds[2] = dst_pitch | (src_pitch << 16);
	cmds[3] = dst0->phys;
	cmds[4] = dst1->phys;
	cmds[5] = src->phys;
	return hse_test_run_cmd(eng, cmds, 6);
}

static int hse_copy_verify_result(struct hse_test_buf *dst,
				  struct hse_test_buf *src,
				  u32 size)
{
	int ret;

	ret = memcmp(dst->virt, src->virt, size);
	if (ret) {
		print_hex_dump(KERN_ERR, "src ", DUMP_PREFIX_OFFSET, 16, 1, src->virt, 0x20, false);
		if (size >= 0x40) {
			printk(KERN_ERR "src ...\n");
			print_hex_dump(KERN_ERR, "src ", DUMP_PREFIX_OFFSET, 16, 1, src->virt+size-0x20, 0x20, false);
		}
		print_hex_dump(KERN_ERR, "dst ", DUMP_PREFIX_OFFSET, 16, 1, dst->virt, 0x20, false);
		if (size >= 0x40) {
			printk(KERN_ERR "dst ...\n");
			print_hex_dump(KERN_ERR, "dst ", DUMP_PREFIX_OFFSET, 16, 1, dst->virt+size-0x20, 0x20, false);
		}
	}
	return ret;
}


static int hse_copy(struct hse_engine *eng,
		    struct hse_test_buf *dst,
		    struct hse_test_buf *src,
		    u32 size)
{
	u32 cmds[32];

	cmds[0] = 0x1;
	cmds[1] = size;
	cmds[2] = dst->phys;
	cmds[3] = src->phys;
	return hse_test_run_cmd(eng, cmds, 4);
}

static int hse_xor_verify_args(u32 num_srcs,
			       u32 size)
{
	if (num_srcs < 1 || num_srcs > 5) {
		pr_err("%s: invalid num_srcs(%u)\n", __func__, num_srcs);
		return -EINVAL;
	}

	return 0;
}

static int hse_xor_verify_result(struct hse_test_buf *dst,
				 struct hse_test_buf *srcs,
				 u32 num_srcs,
				 u32 size)
{
	u8 *p;
	int i, j;
	int ret;

	p = kzalloc(size, GFP_KERNEL);
	if (!p) {
		pr_err("%s: failed to alloc memory\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < size; i++) {
		p[i] = ((u8 *)srcs[0].virt)[i];
		for (j = 1; j < num_srcs; j++)
			p[i] ^= ((u8 *)srcs[j].virt)[i];
	}
	ret = memcmp(dst->virt, p, size);
	if (ret) {
		print_hex_dump(KERN_ERR, "hw ", DUMP_PREFIX_OFFSET, 16, 1, dst->virt, 0x20, false);
		print_hex_dump(KERN_ERR, "sw ", DUMP_PREFIX_OFFSET, 16, 1, p, 0x20, false);
	}
	kfree(p);
	return ret;
}

static int hse_xor(struct hse_engine *eng,
		   struct hse_test_buf *dst,
		   struct hse_test_buf *srcs,
		   u32 num_srcs,
		   u32 size)
{
	u32 cmds[32];
	u32 i;

	if (hse_xor_verify_args(num_srcs, size))
		return -EINVAL;

	i = 0;
	cmds[i++] = 0x01 | ((num_srcs-1) << 16);
	cmds[i++] = size;
	cmds[i++] = dst->phys;
	cmds[i++] = srcs[0].phys;
	if (num_srcs > 1)
		cmds[i++] = srcs[1].phys;
	if (num_srcs > 2)
		cmds[i++] = srcs[2].phys;
	if (num_srcs > 3)
		cmds[i++] = srcs[3].phys;
	if (num_srcs > 4)
		cmds[i++] = srcs[4].phys;
	return hse_test_run_cmd(eng, cmds, i);
}

struct hse_rotate_args {
	u32 m;
	u32 w;
	u32 h;
	u32 sp;
	u32 dp;
	u32 inv;
};

#define TEST_BEGIN(_hobj, _fmt, ...) \
do { \
	pr_err("\n"); \
	pr_err("### test: %d => " _fmt, ++(_hobj)->total, __VA_ARGS__); \
} while (0);

#define TEST_END(_hobj, _ret) \
do { \
	pr_err("\n"); \
	pr_err("  result: %s\n", (_ret) ? "failed" : "passed"); \
	if (_ret) \
		(_hobj)->fail++; \
	else  \
		(_hobj)->pass++; \
} while (0);


__maybe_unused
static void hse_test_engine_rotate(struct hse_test_obj *hobj)
{
	struct hse_engine *eng = hobj->eng;
	struct hse_test_buf *bufs = hobj->bufs;
	int ret;
	int i;
	struct hse_rotate_args cases[] = {
		{ HSE_ROTATE_90,  16,  16,  16,  16, 0, },
		{ HSE_ROTATE_90,  64,  32,  64,  32, 0, },
		{ HSE_ROTATE_90, 128,  32, 128,  64, 0, },
		{ HSE_ROTATE_90,  64,  64,  64,  64, 0, },
		{ HSE_ROTATE_90,  64,  64,  64, 128, 0, },
		{ HSE_ROTATE_90,  64,  64, 128,  64, 0, },
		{ HSE_ROTATE_90,  64,  64, 128,  64, 0, },
		{ HSE_ROTATE_90,  64,  64, 128, 128, 0, },
		{ HSE_ROTATE_180, 64,  64,  64,  64, 0, },
		{ HSE_ROTATE_180, 64,  64,  64, 128, 0, },
		{ HSE_ROTATE_180, 64,  64, 128,  64, 0, },
		{ HSE_ROTATE_180, 64,  64, 128, 128, 0, },
		{ HSE_ROTATE_270, 64,  64,  64,  64, 0, },
		{ HSE_ROTATE_270, 64,  64,  64, 128, 0, },
		{ HSE_ROTATE_270, 64,  64, 128,  64, 0, },
		{ HSE_ROTATE_270, 64,  64, 128, 128, 0, },
		{ HSE_ROTATE_90,  32, 128,  32,  64, 1, },
		{ HSE_ROTATE_90,  33, 128,  32,  64, 1, },
		{ 4,  33, 128,  32,  64, 1, },
	};

	/* prepare data for rotate */
	for (i = 0; i < MEM_SIZE; i++)
		((u8 *)bufs[0].virt)[i] = i;
	hse_flush_dcache_area(bufs[0].virt, MEM_SIZE);

	/* rotate */
	for (i = 0 ; i < ARRAY_SIZE(cases); i++) {
		u32 m = cases[i].m;
		u32 w = cases[i].w;
		u32 h = cases[i].h;
		u32 sp = cases[i].sp;
		u32 dp = cases[i].dp;

		hse_memset(&bufs[1], 0x00, MEM_SIZE);
		hse_flush_dcache_area(bufs[1].virt, MEM_SIZE);

		TEST_BEGIN(hobj, "hse_rotate (m=%u, w=%u h=%u sp=%u dp=%u)\n", m, w, h, sp, dp);
		ret = hse_rotate(eng, &bufs[1], &bufs[0], m, w, h, sp, dp);
		if (!ret)
			ret = hse_rotate_verify_result(&bufs[1], &bufs[0], m, w, h, sp, dp);
		if (cases[i].inv) {
			if (ret)
				ret = 0;
			else
				ret = -EINVAL;
		}
		TEST_END(hobj, ret);
	}
}

struct hse_yuy2_to_nv12_args {
	u32 w;
	u32 h;
	u32 sp;
	u32 dp;
};

__maybe_unused
static void hse_test_engine_yuy2(struct hse_test_obj *hobj)
{
	struct hse_engine *eng = hobj->eng;
	struct hse_test_buf *bufs = hobj->bufs;
	struct hse_yuy2_to_nv12_args cases[] = {
		{ 32, 32, 32, 16, },
		{ 32, 32, 32, 32, },
		{ 32, 32, 64, 16, },
		{ 32, 32, 64, 32, },
	};
	int ret;
	int i;


	/* prepare data for yuy2_to_nv12 */
	for (i = 0; i < MEM_SIZE; i++)
		((u8 *)bufs[0].virt)[i] = i;
	hse_flush_dcache_area(bufs[0].virt, MEM_SIZE);

	/* yuy2 to nv */
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		u32 w = cases[i].w;
		u32 h = cases[i].h;
		u32 sp = cases[i].sp;
		u32 dp = cases[i].dp;

		hse_memset(&bufs[1], 0x00, MEM_SIZE);
		hse_flush_dcache_area(bufs[1].virt, MEM_SIZE);
		hse_memset(&bufs[2], 0x00, MEM_SIZE);
		hse_flush_dcache_area(bufs[2].virt, MEM_SIZE);

		TEST_BEGIN(hobj, "hse_yuy2_to_nv12 (w=%u h=%u sp=%u dp=%u)\n", w, h, sp, dp);
		ret = hse_yuy2_to_nv12(eng, &bufs[1], &bufs[2], &bufs[0], w, h, sp, dp);
		if (!ret)
			ret = hse_yuy2_to_nv12_verify_result(&bufs[1], &bufs[2], &bufs[0], w, h, sp, dp);
		TEST_END(hobj, ret);

	}
}

static void hse_test_engine_xor(struct hse_test_obj *hobj)
{
	struct hse_engine *eng = hobj->eng;
	struct hse_test_buf *bufs = hobj->bufs;
	int ret;
	int i;

	/* prepare data for copy and xor */
	hse_memset(&bufs[1], 0x01, MEM_SIZE);
	hse_flush_dcache_area(bufs[1].virt, MEM_SIZE);
	hse_memset(&bufs[2], 0x02, MEM_SIZE);
	hse_flush_dcache_area(bufs[2].virt, MEM_SIZE);
	hse_memset(&bufs[3], 0x06, MEM_SIZE);
	hse_flush_dcache_area(bufs[3].virt, MEM_SIZE);
	hse_memset(&bufs[4], 0x18, MEM_SIZE);
	hse_flush_dcache_area(bufs[4].virt, MEM_SIZE);
	hse_memset(&bufs[5], 0x30, MEM_SIZE);
	hse_flush_dcache_area(bufs[5].virt, MEM_SIZE);

	/* copy */
	hse_memset(&bufs[0], 0x00, MEM_SIZE);
	hse_flush_dcache_area(bufs[1].virt, MEM_SIZE);

	TEST_BEGIN(hobj, "hse_copy (size=%u)\n", (u32)MEM_SIZE);
	ret = hse_copy(eng, &bufs[0], &bufs[1], MEM_SIZE);
	if (!ret)
		ret = hse_copy_verify_result(&bufs[0], &bufs[1], MEM_SIZE);
	TEST_END(hobj, ret);

	/* xor */
	for (i = 2; i <= 5; i++) {
		u32 xor_num;

		hse_memset(&bufs[0], 0x00, MEM_SIZE);
		hse_flush_dcache_area(bufs[1].virt, MEM_SIZE);

		xor_num = i;
		TEST_BEGIN(hobj, "hse_xor (size=%u, num=%u)\n", (u32)MEM_SIZE, xor_num);
		ret = hse_xor(eng, &bufs[0], &bufs[1], xor_num, MEM_SIZE);
		if (!ret)
			ret = hse_xor_verify_result(&bufs[0], &bufs[1], xor_num, MEM_SIZE);
		TEST_END(hobj, ret);
	}
}

static int hse_test_engine(struct hse_engine *eng)
{
	int ret = 0;
	struct hse_test_obj *hobj;
	bool rotate_workaround = get_rtd_chip_revision() == RTD_CHIP_A00;

	pr_err("test engine@%03x\n", eng->base_offset);

	hobj = hse_test_obj_alloc(eng);
	if (!hobj)
		return -ENOMEM;

	hse_test_engine_xor(hobj);

#ifdef CONFIG_RTK_HSE_HAS_YUY2
	hse_test_engine_yuy2(hobj);
#endif

#ifdef CONFIG_RTK_HSE_HAS_ROTATE

	if (rotate_workaround) {
		pr_err("rotate_workaround=%d\n", rotate_workaround);
		reset_control_reset(eng->hdev->rstc);
	}
	hse_test_engine_rotate(hobj);
	if (rotate_workaround)
		reset_control_reset(eng->hdev->rstc);
	hse_test_engine_xor(hobj);
#endif

	pr_err("\n");
	pr_err("### result ###\n");
	pr_err("### passed: %d, failed: %d, total: %d ###\n", hobj->pass, hobj->fail, hobj->total);
	pr_err("\n");

	hse_test_obj_free(hobj);
	return ret;
}

int hse_self_test(struct hse_device *hdev)
{
	struct hse_engine *engs[HSE_MAX_ENGINES] = {0};
	int i;

	pm_runtime_get_sync(hdev->dev);
	for (i = 0; i < HSE_MAX_ENGINES; i++) {
		engs[i] = hse_engine_get_any(hdev);
		if (!engs[i])
			break;
	}

	for (i = 0; i < HSE_MAX_ENGINES; i++)
		if (engs[i]) {
			hse_test_engine(engs[i]);
		}

	for (i = 0; i < HSE_MAX_ENGINES; i++)
		if (engs[i])
			hse_engine_put(engs[i]);
        pm_runtime_put_sync(hdev->dev);
	return 0;
}
