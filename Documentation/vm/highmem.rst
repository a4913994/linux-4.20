.. _highmem:

====================
High Memory Handling
====================

By: Peter Zijlstra <a.p.zijlstra@chello.nl>

.. contents:: :local:

What Is High Memory?
High Memory是什么？
====================

High memory (highmem) is used when the size of physical memory approaches or
exceeds the maximum size of virtual memory.  At that point it becomes
impossible for the kernel to keep all of the available physical memory mapped
at all times.  This means the kernel needs to start using temporary mappings of
the pieces of physical memory that it wants to access.
High Memory (highmem)
当物理内存的大小接近或超过虚拟内存的最大大小时,就会使用高内存（highmem）。
此时，内核不可能始终将所有可用的物理内存映射到内存中，因此内核需要开始使用临时映射来访问它想要访问的物理内存块。

The part of (physical) memory not covered by a permanent mapping is what we
refer to as 'highmem'.  There are various architecture dependent constraints on
where exactly that border lies.
未被永久映射覆盖的（物理）内存部分是我们所说的“高内存”。关于该边界实际位于何处，存在各种与体系结构相关的限制。

In the i386 arch, for example, we choose to map the kernel into every process's
VM space so that we don't have to pay the full TLB invalidation costs for
kernel entry/exit.  This means the available virtual memory space (4GiB on
i386) has to be divided between user and kernel space.
例如，在i386架构中，我们选择将内核映射到每个进程的虚拟内存空间中，这样我们就不必为内核的进入/退出付出全部的TLB无效成本。这意味着可用的虚拟内存空间（i386上为4GiB）必须在用户空间和内核空间之间分配。

The traditional split for architectures using this approach is 3:1, 3GiB for
userspace and the top 1GiB for kernel space::
采用此方法的体系结构的传统分割比是3:1，即3GiB用于用户空间，而顶部的1GiB用于内核空间。

		+--------+ 0xffffffff
		| Kernel |
		+--------+ 0xc0000000
		|        |
		| User   |
		|        |
		+--------+ 0x00000000

This means that the kernel can at most map 1GiB of physical memory at any one
time, but because we need virtual address space for other things - including
temporary maps to access the rest of the physical memory - the actual direct
map will typically be less (usually around ~896MiB).
这意味着内核一次最多只能映射1GiB的物理内存，但因为我们需要虚拟地址空间用于其他事情，包括访问剩余物理内存的临时映射，实际的直接映射通常会更少（通常约为~896MiB）。

Other architectures that have mm context tagged TLBs can have separate kernel
and user maps.  Some hardware (like some ARMs), however, have limited virtual
space when they use mm context tags.
其他具有 mm 上下文标记 TLB 的体系结构可以拥有独立的内核和用户映射。然而，一些硬件（例如某些 ARM 设备）在使用 mm 上下文标记时可能具有受限虚拟内存空间。


Temporary Virtual Mappings
临时虚拟映射
==========================

The kernel contains several ways of creating temporary mappings:
Kernel包含了几种创建临时映射的方法：

* vmap().  This can be used to make a long duration mapping of multiple
  physical pages into a contiguous virtual space.  It needs global
  synchronization to unmap.
  vmap().  这可用于将多个物理页面的长时间映射成连续的虚拟空间。解除映射时需要全局同步。

* kmap().  This permits a short duration mapping of a single page.  It needs
  global synchronization, but is amortized somewhat.  It is also prone to
  deadlocks when using in a nested fashion, and so it is not recommended for
  new code.
  kmap().  这允许对单个页面进行短时间的映射。它需要全局同步，但是有一定的摊销效果。当以嵌套方式使用时，它也容易发生死锁，因此不建议新代码使用。

* kmap_atomic().  This permits a very short duration mapping of a single
  page.  Since the mapping is restricted to the CPU that issued it, it
  performs well, but the issuing task is therefore required to stay on that
  CPU until it has finished, lest some other task displace its mappings.
  kmap_atomic().  这允许对单个页面进行非常短的映射。由于映射仅限于发出它的CPU，因此它的性能很好，但是发出任务必须保持在该CPU上，直到完成，否则其他任务将替换其映射。

  kmap_atomic() may also be used by interrupt contexts, since it is does not
  sleep and the caller may not sleep until after kunmap_atomic() is called.
  kmap_atomic()也可以由中断上下文使用，因为它不会睡眠，而调用者在调用kunmap_atomic()之前不会睡眠。

  It may be assumed that k[un]map_atomic() won't fail.
  可以假定k[un]map_atomic()不会失败。


Using kmap_atomic
使用kmap_atomic
=================

When and where to use kmap_atomic() is straightforward.  It is used when code
wants to access the contents of a page that might be allocated from high memory
(see __GFP_HIGHMEM), for example a page in the pagecache.  The API has two
functions, and they can be used in a manner similar to the following::
当和在何处使用kmap_atomic()是简单的。当代码想要访问可能从高内存分配的页面的内容时，它就会被使用（例如，页面缓存中的页面），
请参阅__GFP_HIGHMEM。API有两个函数，它们可以用类似于以下方式的方式使用：

	/* Find the page of interest. */
	struct page *page = find_get_page(mapping, offset);

	/* Gain access to the contents of that page. */
	void *vaddr = kmap_atomic(page);

	/* Do something to the contents of that page. */
	memset(vaddr, 0, PAGE_SIZE);

	/* Unmap that page. */
	kunmap_atomic(vaddr);

Note that the kunmap_atomic() call takes the result of the kmap_atomic() call
not the argument.
请注意，kunmap_atomic()调用采用kmap_atomic()调用的结果，而不是参数。

If you need to map two pages because you want to copy from one page to
another you need to keep the kmap_atomic calls strictly nested, like::
如果您需要映射两个页面，因为您想要从一个页面复制到另一个页面，您需要严格嵌套kmap_atomic调用，如下所示：

	vaddr1 = kmap_atomic(page1);
	vaddr2 = kmap_atomic(page2);

	memcpy(vaddr1, vaddr2, PAGE_SIZE);

	kunmap_atomic(vaddr2);
	kunmap_atomic(vaddr1);


Cost of Temporary Mappings
临时映射的成本
==========================

The cost of creating temporary mappings can be quite high.  The arch has to
manipulate the kernel's page tables, the data TLB and/or the MMU's registers.
创建临时映射的成本可能非常高。架构必须操作内核的页表、数据 TLB 和/或 MMU 寄存器。

If CONFIG_HIGHMEM is not set, then the kernel will try and create a mapping
simply with a bit of arithmetic that will convert the page struct address into
a pointer to the page contents rather than juggling mappings about.  In such a
case, the unmap operation may be a null operation.
如果未设置 CONFIG_HIGHMEM，那么内核将尝试仅通过一些算数计算来创建映射，将页面结构地址转换为页面内容的指针，而不是搬来搬去地映射。在这种情况下，解除映射操作可能是一个空操作。

If CONFIG_MMU is not set, then there can be no temporary mappings and no
highmem.  In such a case, the arithmetic approach will also be used.
如果未设置 CONFIG_MMU，那么就没有临时映射和高端内存。在这种情况下，也将使用算术方法。

i386 PAE
========

The i386 arch, under some circumstances, will permit you to stick up to 64GiB
of RAM into your 32-bit machine.  This has a number of consequences:
在某些情况下，i386架构可以允许你在32位机器上使用高达64GiB的RAM。这带来了一些后果：

* Linux needs a page-frame structure for each page in the system and the
  pageframes need to live in the permanent mapping, which means:
  Linux需要一个页面框结构来表示系统中的每个页面，并且页面框需要位于永久映射中，这意味着：

* you can have 896M/sizeof(struct page) page-frames at most; with struct
  page being 32-bytes that would end up being something in the order of 112G
  worth of pages; the kernel, however, needs to store more than just
  page-frames in that memory...
  你最多可以有896M/sizeof(struct page)个页面框；对于struct page是32字节的情况，这将最终达到112G的页面价值；但是，内核需要在该内存中存储不仅仅是页面框...

* PAE makes your page tables larger - which slows the system down as more
  data has to be accessed to traverse in TLB fills and the like.  One
  advantage is that PAE has more PTE bits and can provide advanced features
  like NX and PAT.
  PAE使你的页表变大了——这会使系统变慢，因为必须访问更多的数据才能在TLB填充等中遍历。一个优点是PAE有更多的PTE位，并且可以提供诸如NX和PAT之类的高级功能。

The general recommendation is that you don't use more than 8GiB on a 32-bit
machine - although more might work for you and your workload, you're pretty
much on your own - don't expect kernel developers to really care much if things
come apart.
一般建议是，你不要在32位机器上使用超过8GiB的内存——尽管更多的内存可能对你和你的工作负载有用，但你几乎是独自一人——不要期望内核开发人员真的关心事情是否会分开。
