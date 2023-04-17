/*
 * SWIOTLB-based DMA API implementation
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/gfp.h>
#include <linux/acpi.h>
#include <linux/memblock.h>
#include <linux/cache.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/dma-direct.h>
#include <linux/dma-noncoherent.h>
#include <linux/dma-contiguous.h>
#include <linux/vmalloc.h>
#include <linux/swiotlb.h>
#include <linux/pci.h>

#include <asm/cacheflush.h>

static struct gen_pool *atomic_pool __ro_after_init;

#define DEFAULT_DMA_COHERENT_POOL_SIZE  SZ_256K
static size_t atomic_pool_size __initdata = DEFAULT_DMA_COHERENT_POOL_SIZE;

static int __init early_coherent_pool(char *p)
{
	atomic_pool_size = memparse(p, &p);
	return 0;
}
early_param("coherent_pool", early_coherent_pool);

static void *__alloc_from_pool(size_t size, struct page **ret_page, gfp_t flags)
{
	unsigned long val;
	void *ptr = NULL;

	if (!atomic_pool) {
		WARN(1, "coherent pool not initialised!\n");
		return NULL;
	}

	val = gen_pool_alloc(atomic_pool, size);
	if (val) {
		phys_addr_t phys = gen_pool_virt_to_phys(atomic_pool, val);

		*ret_page = phys_to_page(phys);
		ptr = (void *)val;
		memset(ptr, 0, size);
	}

	return ptr;
}

static bool __in_atomic_pool(void *start, size_t size)
{
	return addr_in_gen_pool(atomic_pool, (unsigned long)start, size);
}

static int __free_from_pool(void *start, size_t size)
{
	if (!__in_atomic_pool(start, size))
		return 0;

	gen_pool_free(atomic_pool, (unsigned long)start, size);

	return 1;
}

void *arch_dma_alloc(struct device *dev, size_t size, dma_addr_t *dma_handle,
		gfp_t flags, unsigned long attrs)
{
	struct page *page;
	void *ptr, *coherent_ptr;
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);

	size = PAGE_ALIGN(size);

	if (!gfpflags_allow_blocking(flags)) {
		struct page *page = NULL;
		void *addr = __alloc_from_pool(size, &page, flags);

		if (addr)
			*dma_handle = phys_to_dma(dev, page_to_phys(page));

		return addr;
	}

	ptr = dma_direct_alloc_pages(dev, size, dma_handle, flags, attrs);
	if (!ptr)
		goto no_mem;

	/* remove any dirty cache lines on the kernel alias */
	__dma_flush_area(ptr, size);

	/* create a coherent mapping */
	page = virt_to_page(ptr);
	coherent_ptr = dma_common_contiguous_remap(page, size, VM_USERMAP,
						   prot, __builtin_return_address(0));
	if (!coherent_ptr)
		goto no_map;

	return coherent_ptr;

no_map:
	dma_direct_free_pages(dev, size, ptr, *dma_handle, attrs);
no_mem:
	return NULL;
}

void arch_dma_free(struct device *dev, size_t size, void *vaddr,
		dma_addr_t dma_handle, unsigned long attrs)
{
	if (!__free_from_pool(vaddr, PAGE_ALIGN(size))) {
		void *kaddr = phys_to_virt(dma_to_phys(dev, dma_handle));

		vunmap(vaddr);
		dma_direct_free_pages(dev, size, kaddr, dma_handle, attrs);
	}
}

long arch_dma_coherent_to_pfn(struct device *dev, void *cpu_addr,
		dma_addr_t dma_addr)
{
	return __phys_to_pfn(dma_to_phys(dev, dma_addr));
}

pgprot_t arch_dma_mmap_pgprot(struct device *dev, pgprot_t prot,
		unsigned long attrs)
{
	if (!dev_is_dma_coherent(dev) || (attrs & DMA_ATTR_WRITE_COMBINE))
		return pgprot_writecombine(prot);
	return prot;
}

void arch_sync_dma_for_device(struct device *dev, phys_addr_t paddr,
		size_t size, enum dma_data_direction dir)
{
	__dma_map_area(phys_to_virt(paddr), size, dir);
}

void arch_sync_dma_for_cpu(struct device *dev, phys_addr_t paddr,
		size_t size, enum dma_data_direction dir)
{
	__dma_unmap_area(phys_to_virt(paddr), size, dir);
}

#ifdef CONFIG_IOMMU_DMA
static int __swiotlb_get_sgtable_page(struct sg_table *sgt,
				      struct page *page, size_t size)
{
	int ret = sg_alloc_table(sgt, 1, GFP_KERNEL);

	if (!ret)
		sg_set_page(sgt->sgl, page, PAGE_ALIGN(size), 0);

	return ret;
}

static int __swiotlb_mmap_pfn(struct vm_area_struct *vma,
			      unsigned long pfn, size_t size)
{
	int ret = -ENXIO;
	unsigned long nr_vma_pages = vma_pages(vma);
	unsigned long nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long off = vma->vm_pgoff;

	if (off < nr_pages && nr_vma_pages <= (nr_pages - off)) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      pfn + off,
				      vma->vm_end - vma->vm_start,
				      vma->vm_page_prot);
	}

	return ret;
}
#endif /* CONFIG_IOMMU_DMA */

static int __init atomic_pool_init(void)
{
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	unsigned long nr_pages = atomic_pool_size >> PAGE_SHIFT;
	struct page *page;
	void *addr;
	unsigned int pool_size_order = get_order(atomic_pool_size);

	if (dev_get_cma_area(NULL))
		page = dma_alloc_from_contiguous(NULL, nr_pages,
						 pool_size_order, false);
	else
		page = alloc_pages(GFP_DMA32, pool_size_order);

	if (page) {
		int ret;
		void *page_addr = page_address(page);

		memset(page_addr, 0, atomic_pool_size);
		__dma_flush_area(page_addr, atomic_pool_size);

		atomic_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!atomic_pool)
			goto free_page;

		addr = dma_common_contiguous_remap(page, atomic_pool_size,
					VM_USERMAP, prot, atomic_pool_init);

		if (!addr)
			goto destroy_genpool;

		ret = gen_pool_add_virt(atomic_pool, (unsigned long)addr,
					page_to_phys(page),
					atomic_pool_size, -1);
		if (ret)
			goto remove_mapping;

		gen_pool_set_algo(atomic_pool,
				  gen_pool_first_fit_order_align,
				  NULL);

		pr_info("DMA: preallocated %zu KiB pool for atomic allocations\n",
			atomic_pool_size / 1024);
		return 0;
	}
	goto out;

remove_mapping:
	dma_common_free_remap(addr, atomic_pool_size, VM_USERMAP);
destroy_genpool:
	gen_pool_destroy(atomic_pool);
	atomic_pool = NULL;
free_page:
	if (!dma_release_from_contiguous(NULL, page, nr_pages))
		__free_pages(page, pool_size_order);
out:
	pr_err("DMA: failed to allocate %zu KiB pool for atomic coherent allocation\n",
		atomic_pool_size / 1024);
	return -ENOMEM;
}

/********************************************
 * The following APIs are for dummy DMA ops *
 ********************************************/

static void *__dummy_alloc(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t flags,
			   unsigned long attrs)
{
	return NULL;
}

static void __dummy_free(struct device *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle,
			 unsigned long attrs)
{
}

static int __dummy_mmap(struct device *dev,
			struct vm_area_struct *vma,
			void *cpu_addr, dma_addr_t dma_addr, size_t size,
			unsigned long attrs)
{
	return -ENXIO;
}

static dma_addr_t __dummy_map_page(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   unsigned long attrs)
{
	return 0;
}

static void __dummy_unmap_page(struct device *dev, dma_addr_t dev_addr,
			       size_t size, enum dma_data_direction dir,
			       unsigned long attrs)
{
}

static int __dummy_map_sg(struct device *dev, struct scatterlist *sgl,
			  int nelems, enum dma_data_direction dir,
			  unsigned long attrs)
{
	return 0;
}

static void __dummy_unmap_sg(struct device *dev,
			     struct scatterlist *sgl, int nelems,
			     enum dma_data_direction dir,
			     unsigned long attrs)
{
}

static void __dummy_sync_single(struct device *dev,
				dma_addr_t dev_addr, size_t size,
				enum dma_data_direction dir)
{
}

static void __dummy_sync_sg(struct device *dev,
			    struct scatterlist *sgl, int nelems,
			    enum dma_data_direction dir)
{
}

static int __dummy_mapping_error(struct device *hwdev, dma_addr_t dma_addr)
{
	return 1;
}

static int __dummy_dma_supported(struct device *hwdev, u64 mask)
{
	return 0;
}

const struct dma_map_ops dummy_dma_ops = {
	.alloc                  = __dummy_alloc,
	.free                   = __dummy_free,
	.mmap                   = __dummy_mmap,
	.map_page               = __dummy_map_page,
	.unmap_page             = __dummy_unmap_page,
	.map_sg                 = __dummy_map_sg,
	.unmap_sg               = __dummy_unmap_sg,
	.sync_single_for_cpu    = __dummy_sync_single,
	.sync_single_for_device = __dummy_sync_single,
	.sync_sg_for_cpu        = __dummy_sync_sg,
	.sync_sg_for_device     = __dummy_sync_sg,
	.mapping_error          = __dummy_mapping_error,
	.dma_supported          = __dummy_dma_supported,
};
EXPORT_SYMBOL(dummy_dma_ops);

static int __init arm64_dma_init(void)
{
	WARN_TAINT(ARCH_DMA_MINALIGN < cache_line_size(),
		   TAINT_CPU_OUT_OF_SPEC,
		   "ARCH_DMA_MINALIGN smaller than CTR_EL0.CWG (%d < %d)",
		   ARCH_DMA_MINALIGN, cache_line_size());

	return atomic_pool_init();
}
arch_initcall(arm64_dma_init);

#ifdef CONFIG_IOMMU_DMA
#include <linux/dma-iommu.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>

/* Thankfully, all cache ops are by VA so we can ignore phys here */
static void flush_page(struct device *dev, const void *virt, phys_addr_t phys)
{
	__dma_flush_area(virt, PAGE_SIZE);
}

static void *__iommu_alloc_attrs(struct device *dev, size_t size,
				 dma_addr_t *handle, gfp_t gfp,
				 unsigned long attrs)
{
	bool coherent = dev_is_dma_coherent(dev);
	int ioprot = dma_info_to_prot(DMA_BIDIRECTIONAL, coherent, attrs);
	size_t iosize = size;
	void *addr;

	if (WARN(!dev, "cannot create IOMMU mapping for unknown device\n"))
		return NULL;

	size = PAGE_ALIGN(size);

	/*
	 * Some drivers rely on this, and we probably don't want the
	 * possibility of stale kernel data being read by devices anyway.
	 */
	gfp |= __GFP_ZERO;

	if (!gfpflags_allow_blocking(gfp)) {
		struct page *page;
		/*
		 * In atomic context we can't remap anything, so we'll only
		 * get the virtually contiguous buffer we need by way of a
		 * physically contiguous allocation.
		 */
		if (coherent) {
			page = alloc_pages(gfp, get_order(size));
			addr = page ? page_address(page) : NULL;
		} else {
			addr = __alloc_from_pool(size, &page, gfp);
		}
		if (!addr)
			return NULL;

		*handle = iommu_dma_map_page(dev, page, 0, iosize, ioprot);
		if (iommu_dma_mapping_error(dev, *handle)) {
			if (coherent)
				__free_pages(page, get_order(size));
			else
				__free_from_pool(addr, size);
			addr = NULL;
		}
	} else if (attrs & DMA_ATTR_FORCE_CONTIGUOUS) {
		pgprot_t prot = arch_dma_mmap_pgprot(dev, PAGE_KERNEL, attrs);
		struct page *page;

		page = dma_alloc_from_contiguous(dev, size >> PAGE_SHIFT,
					get_order(size), gfp & __GFP_NOWARN);
		if (!page)
			return NULL;

		*handle = iommu_dma_map_page(dev, page, 0, iosize, ioprot);
		if (iommu_dma_mapping_error(dev, *handle)) {
			dma_release_from_contiguous(dev, page,
						    size >> PAGE_SHIFT);
			return NULL;
		}
		addr = dma_common_contiguous_remap(page, size, VM_USERMAP,
						   prot,
						   __builtin_return_address(0));
		if (addr) {
			if (!coherent)
				__dma_flush_area(page_to_virt(page), iosize);
			memset(addr, 0, size);
		} else {
			iommu_dma_unmap_page(dev, *handle, iosize, 0, attrs);
			dma_release_from_contiguous(dev, page,
						    size >> PAGE_SHIFT);
		}
	} else {
		pgprot_t prot = arch_dma_mmap_pgprot(dev, PAGE_KERNEL, attrs);
		struct page **pages;

		pages = iommu_dma_alloc(dev, iosize, gfp, attrs, ioprot,
					handle, flush_page);
		if (!pages)
			return NULL;

		addr = dma_common_pages_remap(pages, size, VM_USERMAP, prot,
					      __builtin_return_address(0));
		if (!addr)
			iommu_dma_free(dev, pages, iosize, handle);
	}
	return addr;
}

static void __iommu_free_attrs(struct device *dev, size_t size, void *cpu_addr,
			       dma_addr_t handle, unsigned long attrs)
{
	size_t iosize = size;

	size = PAGE_ALIGN(size);
	/*
	 * @cpu_addr will be one of 4 things depending on how it was allocated:
	 * - A remapped array of pages for contiguous allocations.
	 * - A remapped array of pages from iommu_dma_alloc(), for all
	 *   non-atomic allocations.
	 * - A non-cacheable alias from the atomic pool, for atomic
	 *   allocations by non-coherent devices.
	 * - A normal lowmem address, for atomic allocations by
	 *   coherent devices.
	 * Hence how dodgy the below logic looks...
	 */
	if (__in_atomic_pool(cpu_addr, size)) {
		iommu_dma_unmap_page(dev, handle, iosize, 0, 0);
		__free_from_pool(cpu_addr, size);
	} else if (attrs & DMA_ATTR_FORCE_CONTIGUOUS) {
		struct page *page = vmalloc_to_page(cpu_addr);

		iommu_dma_unmap_page(dev, handle, iosize, 0, attrs);
		dma_release_from_contiguous(dev, page, size >> PAGE_SHIFT);
		dma_common_free_remap(cpu_addr, size, VM_USERMAP);
	} else if (is_vmalloc_addr(cpu_addr)){
		struct vm_struct *area = find_vm_area(cpu_addr);

		if (WARN_ON(!area || !area->pages))
			return;
		iommu_dma_free(dev, area->pages, iosize, &handle);
		dma_common_free_remap(cpu_addr, size, VM_USERMAP);
	} else {
		iommu_dma_unmap_page(dev, handle, iosize, 0, 0);
		__free_pages(virt_to_page(cpu_addr), get_order(size));
	}
}

static int __iommu_mmap_attrs(struct device *dev, struct vm_area_struct *vma,
			      void *cpu_addr, dma_addr_t dma_addr, size_t size,
			      unsigned long attrs)
{
	struct vm_struct *area;
	int ret;

	vma->vm_page_prot = arch_dma_mmap_pgprot(dev, vma->vm_page_prot, attrs);

	if (dma_mmap_from_dev_coherent(dev, vma, cpu_addr, size, &ret))
		return ret;

	if (attrs & DMA_ATTR_FORCE_CONTIGUOUS) {
		/*
		 * DMA_ATTR_FORCE_CONTIGUOUS allocations are always remapped,
		 * hence in the vmalloc space.
		 */
		unsigned long pfn = vmalloc_to_pfn(cpu_addr);
		return __swiotlb_mmap_pfn(vma, pfn, size);
	}

	area = find_vm_area(cpu_addr);
	if (WARN_ON(!area || !area->pages))
		return -ENXIO;

	return iommu_dma_mmap(area->pages, size, vma);
}

static int __iommu_get_sgtable(struct device *dev, struct sg_table *sgt,
			       void *cpu_addr, dma_addr_t dma_addr,
			       size_t size, unsigned long attrs)
{
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct vm_struct *area = find_vm_area(cpu_addr);

	if (attrs & DMA_ATTR_FORCE_CONTIGUOUS) {
		/*
		 * DMA_ATTR_FORCE_CONTIGUOUS allocations are always remapped,
		 * hence in the vmalloc space.
		 */
		struct page *page = vmalloc_to_page(cpu_addr);
		return __swiotlb_get_sgtable_page(sgt, page, size);
	}

	if (WARN_ON(!area || !area->pages))
		return -ENXIO;

	return sg_alloc_table_from_pages(sgt, area->pages, count, 0, size,
					 GFP_KERNEL);
}

static void __iommu_sync_single_for_cpu(struct device *dev,
					dma_addr_t dev_addr, size_t size,
					enum dma_data_direction dir)
{
	phys_addr_t phys;

	if (dev_is_dma_coherent(dev))
		return;

	phys = iommu_iova_to_phys(iommu_get_dma_domain(dev), dev_addr);
	arch_sync_dma_for_cpu(dev, phys, size, dir);
}

static void __iommu_sync_single_for_device(struct device *dev,
					   dma_addr_t dev_addr, size_t size,
					   enum dma_data_direction dir)
{
	phys_addr_t phys;

	if (dev_is_dma_coherent(dev))
		return;

	phys = iommu_iova_to_phys(iommu_get_dma_domain(dev), dev_addr);
	arch_sync_dma_for_device(dev, phys, size, dir);
}

static dma_addr_t __iommu_map_page(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   unsigned long attrs)
{
	bool coherent = dev_is_dma_coherent(dev);
	int prot = dma_info_to_prot(dir, coherent, attrs);
	dma_addr_t dev_addr = iommu_dma_map_page(dev, page, offset, size, prot);

	if (!coherent && !(attrs & DMA_ATTR_SKIP_CPU_SYNC) &&
	    !iommu_dma_mapping_error(dev, dev_addr))
		__dma_map_area(page_address(page) + offset, size, dir);

	return dev_addr;
}

static void __iommu_unmap_page(struct device *dev, dma_addr_t dev_addr,
			       size_t size, enum dma_data_direction dir,
			       unsigned long attrs)
{
	if ((attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0)
		__iommu_sync_single_for_cpu(dev, dev_addr, size, dir);

	iommu_dma_unmap_page(dev, dev_addr, size, dir, attrs);
}

static void __iommu_sync_sg_for_cpu(struct device *dev,
				    struct scatterlist *sgl, int nelems,
				    enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (dev_is_dma_coherent(dev))
		return;

	for_each_sg(sgl, sg, nelems, i)
		arch_sync_dma_for_cpu(dev, sg_phys(sg), sg->length, dir);
}

static void __iommu_sync_sg_for_device(struct device *dev,
				       struct scatterlist *sgl, int nelems,
				       enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	if (dev_is_dma_coherent(dev))
		return;

	for_each_sg(sgl, sg, nelems, i)
		arch_sync_dma_for_device(dev, sg_phys(sg), sg->length, dir);
}

static int __iommu_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
				int nelems, enum dma_data_direction dir,
				unsigned long attrs)
{
	bool coherent = dev_is_dma_coherent(dev);

	if ((attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0)
		__iommu_sync_sg_for_device(dev, sgl, nelems, dir);

	return iommu_dma_map_sg(dev, sgl, nelems,
				dma_info_to_prot(dir, coherent, attrs));
}

static void __iommu_unmap_sg_attrs(struct device *dev,
				   struct scatterlist *sgl, int nelems,
				   enum dma_data_direction dir,
				   unsigned long attrs)
{
	if ((attrs & DMA_ATTR_SKIP_CPU_SYNC) == 0)
		__iommu_sync_sg_for_cpu(dev, sgl, nelems, dir);

	iommu_dma_unmap_sg(dev, sgl, nelems, dir, attrs);
}

static const struct dma_map_ops iommu_dma_ops = {
	.alloc = __iommu_alloc_attrs,
	.free = __iommu_free_attrs,
	.mmap = __iommu_mmap_attrs,
	.get_sgtable = __iommu_get_sgtable,
	.map_page = __iommu_map_page,
	.unmap_page = __iommu_unmap_page,
	.map_sg = __iommu_map_sg_attrs,
	.unmap_sg = __iommu_unmap_sg_attrs,
	.sync_single_for_cpu = __iommu_sync_single_for_cpu,
	.sync_single_for_device = __iommu_sync_single_for_device,
	.sync_sg_for_cpu = __iommu_sync_sg_for_cpu,
	.sync_sg_for_device = __iommu_sync_sg_for_device,
	.map_resource = iommu_dma_map_resource,
	.unmap_resource = iommu_dma_unmap_resource,
	.mapping_error = iommu_dma_mapping_error,
};

static int __init __iommu_dma_init(void)
{
	return iommu_dma_init();
}
arch_initcall(__iommu_dma_init);

static void __iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				  const struct iommu_ops *ops)
{
	struct iommu_domain *domain;

	if (!ops)
		return;

	/*
	 * The IOMMU core code allocates the default DMA domain, which the
	 * underlying IOMMU driver needs to support via the dma-iommu layer.
	 */
	domain = iommu_get_domain_for_dev(dev);

	if (!domain)
		goto out_err;

	if (domain->type == IOMMU_DOMAIN_DMA) {
		if (iommu_dma_init_domain(domain, dma_base, size, dev))
			goto out_err;

		dev->dma_ops = &iommu_dma_ops;
	}

	return;

out_err:
	 pr_warn("Failed to set up IOMMU for device %s; retaining platform DMA ops\n",
		 dev_name(dev));
}

void arch_teardown_dma_ops(struct device *dev)
{
	dev->dma_ops = NULL;
}

#else

static void __iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				  const struct iommu_ops *iommu)
{ }

#endif  /* CONFIG_IOMMU_DMA */

void arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
			const struct iommu_ops *iommu, bool coherent)
{
	if (!dev->dma_ops)
		dev->dma_ops = &swiotlb_dma_ops;

	dev->dma_coherent = coherent;
	__iommu_setup_dma_ops(dev, dma_base, size, iommu);

#ifdef CONFIG_XEN
	if (xen_initial_domain()) {
		dev->archdata.dev_dma_ops = dev->dma_ops;
		dev->dma_ops = xen_dma_ops;
	}
#endif
}
