#ifndef __SOC_REALTEK_SEMEM_H
#define __SOC_REALTEK_SEMEM_H

#include <linux/types.h>
void *semem_alloc(size_t size, dma_addr_t *phys);
void semem_free(void *virt);

#endif
