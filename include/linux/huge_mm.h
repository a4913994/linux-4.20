/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_HUGE_MM_H
#define _LINUX_HUGE_MM_H

#include <linux/sched/coredump.h>
#include <linux/mm_types.h>

#include <linux/fs.h> /* only for vma_is_dax() */

// 用于处理一个大页面映射中的匿名页故障。函数名中的 "pmd" 指的是页中间目录 (Page Middle Directory)，表示该函数是在页表项级别上处理大页面映射。
extern vm_fault_t do_huge_pmd_anonymous_page(struct vm_fault *vmf);
// 函数copy_huge_pmd用于在进程的地址空间之间复制一个大页面的PMD项。这个函数需要两个进程的内存描述符作为参数，以及源和目标PMD指针，以及大页面地址和对应的虚拟内存区域结构。该函数可以被用于内存页共享的操作中，比如共享库、匿名页面和映射到文件的页面。
extern int copy_huge_pmd(struct mm_struct *dst_mm, struct mm_struct *src_mm,
			 pmd_t *dst_pmd, pmd_t *src_pmd, unsigned long addr,
			 struct vm_area_struct *vma);
// huge_pmd_set_accessed函数用于设置大页面中的pmd项为已访问状态。
// 参数vmf包含了有关页面访问的信息，
// 而orig_pmd参数则是要设置为已访问状态的原始pmd项。
extern void huge_pmd_set_accessed(struct vm_fault *vmf, pmd_t orig_pmd);

// copy_huge_pud函数用于将一个源进程的一个 pud 页表项（对应地址 addr）复制到目标进程的页表中，从而实现进程之间的地址空间共享
// 这个函数与 copy_huge_pmd 函数类似，不同的是，它是针对大页(pud)的，而不是大页表(pmd)的
// 具体而言，这个函数会在目标进程的页表中找到 pud 所在的页表项，然后将源进程的 pud 所对应的物理页的引用计数加 1，
// 同时将目标进程的页表项设置为和源进程的页表项一样的值。如果源进程的 pud 所对应的物理页是共享的，那么多个进程就可以共享这个物理页。
extern int copy_huge_pud(struct mm_struct *dst_mm, struct mm_struct *src_mm,
			 pud_t *dst_pud, pud_t *src_pud, unsigned long addr,
			 struct vm_area_struct *vma);

#ifdef CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD
extern void huge_pud_set_accessed(struct vm_fault *vmf, pud_t orig_pud);
#else
static inline void huge_pud_set_accessed(struct vm_fault *vmf, pud_t orig_pud)
{
}
#endif

/*
do_huge_pmd_wp_page() 是 Linux 内核中处理 hugetlb page 的函数之一，它的作用是在访问一个写保护的 hugetlb page 时，为它分配一个新的 page 并进行写操作。
函数的参数是一个 vm_fault 结构体指针和一个 pmd_t 类型的参数 orig_pmd。vm_fault 结构体用来描述缺页中断，orig_pmd 参数是原始的 hugetlb page 所在的 pmd 项。
该函数会返回一个 vm_fault_t 类型的值，表示缺页中断的结果。如果成功，返回 VM_FAULT_NOPAGE；如果出错，返回 VM_FAULT_SIGBUS 或 VM_FAULT_OOM。

HugeTLB page（大页面）是一种超大的物理页面大小，它通常是2 MB或1 GB，并且比常规4 KB页面大得多。由于大页面大小比常规页面大小大得多，因此它们对于某些应用程序，例如需要大量内存映射的数据库应用程序或科学计算应用程序非常有用。使用大页面可以提高内存管理效率，并且在某些情况下可以提高性能。
*/
extern vm_fault_t do_huge_pmd_wp_page(struct vm_fault *vmf, pmd_t orig_pmd);

// 该函数用于查找一个巨大页面的物理地址，并返回对应的struct page指针。它接收虚拟地址addr，虚拟内存区域对象vma，以及页表项指针pmd。flags是一些控制标志位，如FAULT_FLAG_WRITE表示这是一个写操作。
extern struct page *follow_trans_huge_pmd(struct vm_area_struct *vma,
					  unsigned long addr,
					  pmd_t *pmd,
					  unsigned int flags);

// 作用是释放一个巨大页面(PMD)。该函数接收5个参数：
// tlb：一个mmu_gather结构，它在函数执行期间用于TLB刷新操作，以确保所有CPU的TLB都被更新。
// vma：与给定地址相关的虚拟内存区域的描述符
// pmd：指向给定地址处PMD的指针
// addr：要释放的页面的起始地址
// next：要释放的页面的下一个页面的起始地址
// 该函数的返回值为布尔值，表示释放页面是否成功。
extern bool madvise_free_huge_pmd(struct mmu_gather *tlb,
			struct vm_area_struct *vma,
			pmd_t *pmd, unsigned long addr, unsigned long next);

/*
tlb：表示处理器TLB（快表）刷新相关的数据结构
vma：表示虚拟内存区域，即进程的内存映射区域
pmd：表示待删除的PMD指针
addr：表示待删除的虚拟地址
删除一个指定的PMD指针，同时更新相应的TLB（快表）等数据结构。PMD是指Page Middle Directory，表示一级页面目录项，是一种管理物理内存的数据结构。
在Linux内核中，PMD主要用于管理HugePage，即大页面，因此删除PMD通常意味着释放大页面。
*/
extern int zap_huge_pmd(struct mmu_gather *tlb,
			struct vm_area_struct *vma,
			pmd_t *pmd, unsigned long addr);
// 用于清理一个已经映射的huge page，并将相关的PUD条目（pud）或PMD条目（pmd）清空
extern int zap_huge_pud(struct mmu_gather *tlb,
			struct vm_area_struct *vma,
			pud_t *pud, unsigned long addr);

// mincore_huge_pmd 函数的作用是查询一个进程地址空间中的一个 huge page 的指定范围是否被映射到物理内存中。该函数接收进程的地址空间描述符 vma，指向该 huge page 的 pmd 指针 pmd，要查询的地址范围 [addr, end)，以及用于存储结果的位向量 vec。
// 返回值为 0 表示所有页都已经被映射到物理内存中，返回值大于 0 则表示只有部分页面被映射到物理内存中。如果返回值小于 0，则表示出现了错误。
extern int mincore_huge_pmd(struct vm_area_struct *vma, pmd_t *pmd,
			unsigned long addr, unsigned long end,
			unsigned char *vec);
/*
move_huge_pmd函数是用来移动一个大页(pmd)的地址范围的。函数原型如下：
vma：虚拟内存区域结构体，表示要操作的虚拟内存区域。
old_addr：旧地址，即需要移动的大页的原始地址。
new_addr：新地址，即需要移动的大页移动后的地址。
old_end：旧地址范围的末尾地址。
old_pmd：指向旧地址的大页中间页表项的指针。
new_pmd：指向新地址的大页中间页表项的指针。
*/
extern bool move_huge_pmd(struct vm_area_struct *vma, unsigned long old_addr,
			 unsigned long new_addr, unsigned long old_end,
			 pmd_t *old_pmd, pmd_t *new_pmd);
/*
该函数用于修改huge page所映射的虚拟内存区域的保护属性。它接受如下参数：

vma：指向虚拟内存区域的vm_area_struct结构体；
pmd：指向要修改的PMD结构体的指针；
addr：要修改的虚拟内存区域的起始地址；
newprot：新的保护属性；
prot_numa：用于NUMA保护属性的掩码。
该函数会将newprot应用于pmd中的huge page所映射的物理内存页面，并且更新页表项以反映新的保护属性。如果操作成功，则返回0；否则返回一个负数错误码。
*/
extern int change_huge_pmd(struct vm_area_struct *vma, pmd_t *pmd,
			unsigned long addr, pgprot_t newprot,
			int prot_numa);
/*
将PFN映射为一个大页面，建立pmd与PFN之间的映射关系
vma：待映射的虚拟地址所在的VM区域
addr：待映射的虚拟地址
pmd：建立映射关系的pmd
pfn：待映射的PFN
write：是否可写
在Linux中，PFN是一个表示页面帧编号的数据类型，其全称是“Page Frame Number”。
一个PFN代表了一个页面在物理内存中的位置，PFN与物理地址之间可以通过简单的位移关系相互转换。
在内核中，PFN通常作为struct page的成员，用于表示该页面在系统中的位置。
*/
vm_fault_t vmf_insert_pfn_pmd(struct vm_area_struct *vma, unsigned long addr,
			pmd_t *pmd, pfn_t pfn, bool write);
vm_fault_t vmf_insert_pfn_pud(struct vm_area_struct *vma, unsigned long addr,
			pud_t *pud, pfn_t pfn, bool write);

/*
TRANSPARENT_HUGEPAGE_FLAG：使用透明大页面
TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG：在应用程序要求使用大页面时使用
TRANSPARENT_HUGEPAGE_DEFRAG_DIRECT_FLAG：在页面被使用前立即进行碎片整理
TRANSPARENT_HUGEPAGE_DEFRAG_KSWAPD_FLAG：在交换进程（kswapd）运行时进行碎片整理
TRANSPARENT_HUGEPAGE_DEFRAG_KSWAPD_OR_MADV_FLAG：在交换进程运行时或应用程序要求使用大页面时进行碎片整理
TRANSPARENT_HUGEPAGE_DEFRAG_REQ_MADV_FLAG：在应用程序要求使用大页面时进行碎片整理
TRANSPARENT_HUGEPAGE_DEFRAG_KHUGEPAGED_FLAG：在大页面守护进程（khugepaged）运行时进行碎片整理
TRANSPARENT_HUGEPAGE_USE_ZERO_PAGE_FLAG：使用空页面进行初始化
TRANSPARENT_HUGEPAGE_DEBUG_COW_FLAG：用于在调试中标识内核是否采用了写时复制机制
 */
enum transparent_hugepage_flag {
	TRANSPARENT_HUGEPAGE_FLAG,
	TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_DIRECT_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_KSWAPD_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_KSWAPD_OR_MADV_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_REQ_MADV_FLAG,
	TRANSPARENT_HUGEPAGE_DEFRAG_KHUGEPAGED_FLAG,
	TRANSPARENT_HUGEPAGE_USE_ZERO_PAGE_FLAG,
#ifdef CONFIG_DEBUG_VM
	TRANSPARENT_HUGEPAGE_DEBUG_COW_FLAG,
#endif
};

struct kobject;
struct kobj_attribute;

/*
该函数会在设置透明大页面（Transparent Huge Pages）相关的单个标志位时被调用
kobj: kobject对象，指向/sys/kernel/mm/transparent_hugepage/目录下的透明大页面相关文件所对应的kobject对象。
attr: kobj_attribute对象，指向/sys/kernel/mm/transparent_hugepage/目录下的透明大页面相关文件所对应的kobj_attribute对象。
buf: 字符串指针，指向用户传入的缓冲区，存储着用户设置的标志位的值。
count: 字节数，表示缓冲区的长度。
flag: enum类型的参数，表示要设置的标志位。
该函数的作用是将透明大页面相关的单个标志位的值设置为用户传入的值，具体实现可以查看函数的定义。
*/
extern ssize_t single_hugepage_flag_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count,
				 enum transparent_hugepage_flag flag);
extern ssize_t single_hugepage_flag_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf,
				enum transparent_hugepage_flag flag);
extern struct kobj_attribute shmem_enabled_attr;

// 这两行代码定义了巨大页面的页面中的页面(pmd page)大小及数量，其中HPAGE_PMD_SHIFT定义为21，即2的21次方（2MB），
// PAGE_SHIFT为12，即2的12次方（4KB）。因此，HPAGE_PMD_ORDER为21-12=9，即HPAGE_PMD_NR为1左移9位（512）。
#define HPAGE_PMD_ORDER (HPAGE_PMD_SHIFT-PAGE_SHIFT)
#define HPAGE_PMD_NR (1<<HPAGE_PMD_ORDER)

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
// HPAGE_PMD_SHIFT 定义为 PMD_SHIFT，表示PMD级别的Huge Page的大小为2MB（1<<21）。
#define HPAGE_PMD_SHIFT PMD_SHIFT
// HPAGE_PMD_SIZE 定义为 1UL << HPAGE_PMD_SHIFT，表示PMD级别的Huge Page的大小为2MB（1<<21）。
#define HPAGE_PMD_SIZE	((1UL) << HPAGE_PMD_SHIFT)
// HPAGE_PMD_MASK 定义为 ~(HPAGE_PMD_SIZE - 1)，表示PMD级别的Huge Page的掩码。
#define HPAGE_PMD_MASK	(~(HPAGE_PMD_SIZE - 1))
// HPAGE_PUD_SHIFT 定义为 PUD_SHIFT，表示PUD级别的Huge Page的大小为1GB（1<<30）。
#define HPAGE_PUD_SHIFT PUD_SHIFT
// HPAGE_PUD_SIZE 定义为 1UL << HPAGE_PUD_SHIFT，表示PUD级别的Huge Page的大小为1GB（1<<30）。
#define HPAGE_PUD_SIZE	((1UL) << HPAGE_PUD_SHIFT)
// HPAGE_PUD_MASK 定义为 ~(HPAGE_PUD_SIZE - 1)，表示PUD级别的Huge Page的掩码。
#define HPAGE_PUD_MASK	(~(HPAGE_PUD_SIZE - 1))

/*
该函数用于判断给定的虚拟内存区域是否为临时栈。
临时栈是一种特殊的栈，用于在内核中临时执行一些操作，如中断处理程序等。这些栈通常被放置在内核堆栈之外，以便在发生栈溢出时不会破坏内核堆栈。
该函数在给定的虚拟内存区域是栈，并且该栈是由内核创建的临时栈时返回 true，否则返回 false。
*/
extern bool is_vma_temporary_stack(struct vm_area_struct *vma);

extern unsigned long transparent_hugepage_flags;

// 用于判断是否启用透明大页面
static inline bool transparent_hugepage_enabled(struct vm_area_struct *vma)
{
	// 如果vma的vm_flags中包含VM_NOHUGEPAGE标志，则不启用透明大页面，返回false。
	if (vma->vm_flags & VM_NOHUGEPAGE)
		return false;
	// 如果vma是一个临时的栈区域（通过函数is_vma_temporary_stack来判断），则不启用透明大页面，返回false。
	if (is_vma_temporary_stack(vma))
		return false;
	// 如果vma所属的进程中设置了MMF_DISABLE_THP标志，则不启用透明大页面，返回false。
	if (test_bit(MMF_DISABLE_THP, &vma->vm_mm->flags))
		return false;
	// 如果transparent_hugepage_flags中包含TRANSPARENT_HUGEPAGE_FLAG标志，则启用透明大页面，返回true。
	if (transparent_hugepage_flags & (1 << TRANSPARENT_HUGEPAGE_FLAG))
		return true;
	// 如果vma是一个DAX类型的区域，则启用透明大页面，返回true。
	if (vma_is_dax(vma))
		return true;
	// 如果transparent_hugepage_flags中包含TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG标志，且vma的vm_flags中包含VM_HUGEPAGE标志，则启用透明大页面，返回true。
	if (transparent_hugepage_flags &
				(1 << TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG))
		return !!(vma->vm_flags & VM_HUGEPAGE);

	return false;
}
// 该宏用于判断是否开启使用全0页功能，即transparent_hugepage_flags中TRANSPARENT_HUGEPAGE_USE_ZERO_PAGE_FLAG位是否为1，如果为1则返回true，否则返回false。
#define transparent_hugepage_use_zero_page()				\
	(transparent_hugepage_flags &					\
	 (1<<TRANSPARENT_HUGEPAGE_USE_ZERO_PAGE_FLAG))
#ifdef CONFIG_DEBUG_VM
#define transparent_hugepage_debug_cow()				\
	(transparent_hugepage_flags &					\
	 (1<<TRANSPARENT_HUGEPAGE_DEBUG_COW_FLAG))
#else /* CONFIG_DEBUG_VM */
#define transparent_hugepage_debug_cow() 0
#endif /* CONFIG_DEBUG_VM */

/*
thp_get_unmapped_area 函数用于获取适合存储大页面的虚拟地址区域
filp：指向文件结构的指针，表示需要映射到用户空间的文件。
addr：表示希望映射到用户空间的虚拟地址起始地址。
len：表示需要映射的字节数。
pgoff：表示从文件起始位置的页偏移。
flags：表示映射标志。
*/
extern unsigned long thp_get_unmapped_area(struct file *filp,
		unsigned long addr, unsigned long len, unsigned long pgoff,
		unsigned long flags);
// prep_transhuge_page函数是在准备进行大页面映射时，对新申请的页进行初始化的函数
// 该函数会在页表映射前被调用，用于确保页的属性和状态正确，并对内存进行清零处理。
// 在实现上，该函数会对新申请的页的 PG_head 和 PG_head_tail 属性进行初始化，确保页在链表中的正确性，
// 并调用 zero_user_largepages 来清零页的内容。在 THP（Transparent Huge Pages，透明大页面）场景下，这个函数是在调用 handle_mm_fault 函数的过程中被调用，用于准备将大页面映射到用户空间。
extern void prep_transhuge_page(struct page *page);
// 该函数的作用是释放一个大页（Transparent Huge Page）。它接收一个指向大页所在物理页框的页结构体的指针，并将其释放回操作系统的页框池中。释放大页的内存空间可以重新用于其他用途。
extern void free_transhuge_page(struct page *page);

bool can_split_huge_page(struct page *page, int *pextra_pins);
int split_huge_page_to_list(struct page *page, struct list_head *list);
static inline int split_huge_page(struct page *page)
{
	return split_huge_page_to_list(page, NULL);
}
// deferred_split_huge_page函数的作用是将大页面进行拆分。
// 在Linux内核中，大页面可以通过将多个物理页面合并到一个虚拟页面的方式来实现。
// 这样做可以减少页面表项的数量，提高系统的性能。但是，在某些情况下，需要对大页面进行拆分，比如当应用程序只使用了一部分大页面时，为了避免浪费内存，就需要将其余的页面拆分出来。deferred_split_huge_page函数就是用来实现这个功能的。
void deferred_split_huge_page(struct page *page);

void __split_huge_pmd(struct vm_area_struct *vma, pmd_t *pmd,
		unsigned long address, bool freeze, struct page *page);

#define split_huge_pmd(__vma, __pmd, __address)				\
	do {								\
		pmd_t *____pmd = (__pmd);				\
		if (is_swap_pmd(*____pmd) || pmd_trans_huge(*____pmd)	\
					|| pmd_devmap(*____pmd))	\
			__split_huge_pmd(__vma, __pmd, __address,	\
						false, NULL);		\
	}  while (0)


void split_huge_pmd_address(struct vm_area_struct *vma, unsigned long address,
		bool freeze, struct page *page);

void __split_huge_pud(struct vm_area_struct *vma, pud_t *pud,
		unsigned long address);

#define split_huge_pud(__vma, __pud, __address)				\
	do {								\
		pud_t *____pud = (__pud);				\
		if (pud_trans_huge(*____pud)				\
					|| pud_devmap(*____pud))	\
			__split_huge_pud(__vma, __pud, __address);	\
	}  while (0)
// 该函数用于实现对巨大页面（hugepage）的madvice（MADV_HUGEPAGE）操作。它接收一个指向虚拟内存区域的指针，一个指向虚拟内存区域标志位的指针，以及advice参数。advice参数可以是MADV_HUGEPAGE或MADV_NOHUGEPAGE。如果advice参数是MADV_HUGEPAGE，则会将虚拟内存区域标志位中的VM_HUGEPAGE位置位。如果advice参数是MADV_NOHUGEPAGE，则会将VM_HUGEPAGE位清零。
// madvise是Linux系统提供的一组系统调用函数，用于向内核通知应用程序有关它管理的虚拟内存区域的一些信息，如内存映射、匿名内存、
// 共享内存和动态链接库等。madvise函数的主要作用是告诉内核对这些区域进行相应的优化和处理，以提高内存管理的效率。
// 常用的madvise选项包括MADV_NORMAL、MADV_SEQUENTIAL、MADV_RANDOM、MADV_WILLNEED、MADV_DONTNEED等。
extern int hugepage_madvise(struct vm_area_struct *vma,
			    unsigned long *vm_flags, int advice);
/* 
vma_adjust_trans_huge 函数用于调整指定的虚拟内存区域的 transparent huge page 机制。它的参数如下：
struct vm_area_struct *vma：指定的虚拟内存区域；
unsigned long start：起始地址；
unsigned long end：结束地址；
long adjust_next：调整下一个区域的大小。
该函数的作用是将指定的虚拟内存区域调整为适合 transparent huge page 的大小。如果 adjust_next 参数不为0，则会将下一个区域的大小也调整为适合的大小。
*/
extern void vma_adjust_trans_huge(struct vm_area_struct *vma,
				    unsigned long start,
				    unsigned long end,
				    long adjust_next);
extern spinlock_t *__pmd_trans_huge_lock(pmd_t *pmd,
		struct vm_area_struct *vma);
extern spinlock_t *__pud_trans_huge_lock(pud_t *pud,
		struct vm_area_struct *vma);

// 这段代码判断传入的pmd是否为交换类型页表项，如果是则返回1，否则返回0。
// pmd是页表项的一种类型，在x86-64中是一个64位的数据类型。函数中的pmd_none和pmd_present都是宏定义，分别用来判断一个pmd是否为none和present类型。pmd_present(pmd)表示判断pmd是否指向了一个物理页框，如果pmd_present(pmd)返回0，则说明该pmd不指向物理页框，即该pmd为交换类型页表项。
static inline int is_swap_pmd(pmd_t pmd)
{
	return !pmd_none(pmd) && !pmd_present(pmd);
}

/* mmap_sem must be held on entry */
static inline spinlock_t *pmd_trans_huge_lock(pmd_t *pmd,
		struct vm_area_struct *vma)
{
	VM_BUG_ON_VMA(!rwsem_is_locked(&vma->vm_mm->mmap_sem), vma);
	if (is_swap_pmd(*pmd) || pmd_trans_huge(*pmd) || pmd_devmap(*pmd))
		return __pmd_trans_huge_lock(pmd, vma);
	else
		return NULL;
}
static inline spinlock_t *pud_trans_huge_lock(pud_t *pud,
		struct vm_area_struct *vma)
{
	VM_BUG_ON_VMA(!rwsem_is_locked(&vma->vm_mm->mmap_sem), vma);
	if (pud_trans_huge(*pud) || pud_devmap(*pud))
		return __pud_trans_huge_lock(pud, vma);
	else
		return NULL;
}
// 这个函数是用来计算一个页面所占的物理页框数的。如果传入的page是一个大页面（hugepage）的第一个页面，那么返回这个大页面所占的页框数；否则，返回1。在内核中，大页面的大小通常是2MB或者1GB，与普通页面（4KB或者2MB）不同。由于大页面需要映射更多的物理地址，因此在处理大页面时需要特别小心。
static inline int hpage_nr_pages(struct page *page)
{
	if (unlikely(PageTransHuge(page)))
		return HPAGE_PMD_NR;
	return 1;
}

// 这个函数的作用是在vma中查找addr对应的pmd项，并返回该pmd对应的物理页面。如果该页面是通过设备映射（即devmap）而来，则返回该页面所在的dev_pagemap结构体，以便后续调用对应的解除映射函数。flags参数指定了对该页面的操作权限。
struct page *follow_devmap_pmd(struct vm_area_struct *vma, unsigned long addr,
		pmd_t *pmd, int flags, struct dev_pagemap **pgmap);
struct page *follow_devmap_pud(struct vm_area_struct *vma, unsigned long addr,
		pud_t *pud, int flags, struct dev_pagemap **pgmap);

// do_huge_pmd_numa_page()是一个函数原型声明，用于在 NUMA 系统中为 vmf 参数中描述的进程页面处理虚拟内存异常。
//该函数处理巨大的页面，使用传递给函数的 orig_pmd 页中间数据来标识页面和地址。函数返回一个 vm_fault_t 类型的错误代码，表示页面处理是否成功。
extern vm_fault_t do_huge_pmd_numa_page(struct vm_fault *vmf, pmd_t orig_pmd);

extern struct page *huge_zero_page;

static inline bool is_huge_zero_page(struct page *page)
{
	return READ_ONCE(huge_zero_page) == page;
}

static inline bool is_huge_zero_pmd(pmd_t pmd)
{
	return is_huge_zero_page(pmd_page(pmd));
}

static inline bool is_huge_zero_pud(pud_t pud)
{
	return false;
}

struct page *mm_get_huge_zero_page(struct mm_struct *mm);
void mm_put_huge_zero_page(struct mm_struct *mm);

#define mk_huge_pmd(page, prot) pmd_mkhuge(mk_pmd(page, prot))

static inline bool thp_migration_supported(void)
{
	return IS_ENABLED(CONFIG_ARCH_ENABLE_THP_MIGRATION);
}

#else /* CONFIG_TRANSPARENT_HUGEPAGE */
#define HPAGE_PMD_SHIFT ({ BUILD_BUG(); 0; })
#define HPAGE_PMD_MASK ({ BUILD_BUG(); 0; })
#define HPAGE_PMD_SIZE ({ BUILD_BUG(); 0; })

#define HPAGE_PUD_SHIFT ({ BUILD_BUG(); 0; })
#define HPAGE_PUD_MASK ({ BUILD_BUG(); 0; })
#define HPAGE_PUD_SIZE ({ BUILD_BUG(); 0; })

#define hpage_nr_pages(x) 1

static inline bool transparent_hugepage_enabled(struct vm_area_struct *vma)
{
	return false;
}

static inline void prep_transhuge_page(struct page *page) {}

#define transparent_hugepage_flags 0UL

#define thp_get_unmapped_area	NULL

static inline bool
can_split_huge_page(struct page *page, int *pextra_pins)
{
	BUILD_BUG();
	return false;
}
static inline int
split_huge_page_to_list(struct page *page, struct list_head *list)
{
	return 0;
}
static inline int split_huge_page(struct page *page)
{
	return 0;
}
static inline void deferred_split_huge_page(struct page *page) {}
#define split_huge_pmd(__vma, __pmd, __address)	\
	do { } while (0)

static inline void __split_huge_pmd(struct vm_area_struct *vma, pmd_t *pmd,
		unsigned long address, bool freeze, struct page *page) {}
static inline void split_huge_pmd_address(struct vm_area_struct *vma,
		unsigned long address, bool freeze, struct page *page) {}

#define split_huge_pud(__vma, __pmd, __address)	\
	do { } while (0)

static inline int hugepage_madvise(struct vm_area_struct *vma,
				   unsigned long *vm_flags, int advice)
{
	BUG();
	return 0;
}
static inline void vma_adjust_trans_huge(struct vm_area_struct *vma,
					 unsigned long start,
					 unsigned long end,
					 long adjust_next)
{
}
static inline int is_swap_pmd(pmd_t pmd)
{
	return 0;
}
static inline spinlock_t *pmd_trans_huge_lock(pmd_t *pmd,
		struct vm_area_struct *vma)
{
	return NULL;
}
static inline spinlock_t *pud_trans_huge_lock(pud_t *pud,
		struct vm_area_struct *vma)
{
	return NULL;
}

static inline vm_fault_t do_huge_pmd_numa_page(struct vm_fault *vmf,
		pmd_t orig_pmd)
{
	return 0;
}

static inline bool is_huge_zero_page(struct page *page)
{
	return false;
}

static inline bool is_huge_zero_pud(pud_t pud)
{
	return false;
}

static inline void mm_put_huge_zero_page(struct mm_struct *mm)
{
	return;
}

static inline struct page *follow_devmap_pmd(struct vm_area_struct *vma,
	unsigned long addr, pmd_t *pmd, int flags, struct dev_pagemap **pgmap)
{
	return NULL;
}

static inline struct page *follow_devmap_pud(struct vm_area_struct *vma,
	unsigned long addr, pud_t *pud, int flags, struct dev_pagemap **pgmap)
{
	return NULL;
}

static inline bool thp_migration_supported(void)
{
	return false;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

#endif /* _LINUX_HUGE_MM_H */
