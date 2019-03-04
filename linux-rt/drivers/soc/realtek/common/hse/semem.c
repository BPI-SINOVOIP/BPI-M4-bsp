#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/slab.h>

static void *reg = NULL;
static dma_addr_t dma = 0;
static dma_addr_t dma_tail = 0;
static LIST_HEAD(alloc_list);

struct semem_obj {
	struct list_head   list;
	void              *virt;
	dma_addr_t         dma;
	size_t             size;
};

void *semem_alloc(size_t size, dma_addr_t *phys)
{
	struct semem_obj *obj;

	if (!reg) {
		pr_err("%s: semem not ready\n", __func__);
		return NULL;
	}

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		pr_err("%s: failed to alloc semem_obj\n", __func__);
		return NULL;
	}

	obj->size = size;
	if ((size & (PAGE_SIZE - 1)) != 0) {
		size = (size & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
	}
	pr_debug("%s: size=%zu, aligned size=%zu\n", __func__, obj->size, size);

	if (dma + size > dma_tail) {
		pr_err("%s: not enough space: dma=%pad+%zu > dma_tail=%pad\n",
			__func__, &dma, size, &dma_tail);
		goto free_obj;
	}

	obj->virt = ioremap(dma, size);
	if (!obj->virt) {
		pr_err("%s: failed to ioremap()\n", __func__);
		goto free_obj;
	}

	obj->dma = dma;
	list_add(&obj->list, &alloc_list);
	*phys = obj->dma;
	dma += size;
	pr_debug("%s: phys:%pad, virt=%p, size=%zu\n", __func__,
		phys, obj->virt, size);
	return obj->virt;
free_obj:
	kfree(obj);
	return NULL;
}

static void semem_init_dma_range(void)
{
	u32 val;

	val = readl(reg + 0x408);
	dma = val << 2;
	val = readl(reg + 0x40C);
	dma_tail = val << 2;

	pr_debug("%s: ranges %pad-%pad\n", __func__, &dma, &dma_tail);
}

void semem_free(void *virt)
{
	struct semem_obj *pos;
	struct semem_obj *obj = NULL;

	if (!reg)
		return;

	if (!virt)
		return;

	list_for_each_entry(pos, &alloc_list, list)
		if (pos->virt == virt) {
			obj = pos;
			break;
		}

	if (!obj) {
		pr_err("%s: no match %p in semem\n", __func__, virt);
		return;
	}

	iounmap(obj->virt);
	list_del(&obj->list);
	if (list_empty(&alloc_list))
		semem_init_dma_range();
}

static __init int semem_alloc_init(void)
{
	reg = ioremap(0x98008000, 0x1000);
	if (!reg) {
		pr_err("failed to map dc_sys register\n");
		return -EINVAL;
	}
	semem_init_dma_range();
	return 0;
}
early_initcall(semem_alloc_init);
