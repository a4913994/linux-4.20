.. hmm:

=====================================
Heterogeneous Memory Management (HMM)
异构内存管理
=====================================

Provide infrastructure and helpers to integrate non-conventional memory (device
memory like GPU on board memory) into regular kernel path, with the cornerstone
of this being specialized struct page for such memory (see sections 5 to 7 of
this document).
提供基础设施和辅助工具, 将非传统内存(例如设备内存,如GPU板载内存)整合到常规内核路径中,其中的关键是为这些内存提供专用的struct page结构体(详见本文档的第5到7节)。

HMM also provides optional helpers for SVM (Share Virtual Memory), i.e.,
allowing a device to transparently access program address coherently with
the CPU meaning that any valid pointer on the CPU is also a valid pointer
for the device. This is becoming mandatory to simplify the use of advanced
heterogeneous computing where GPU, DSP, or FPGA are used to perform various
computations on behalf of a process.
HMM的 SVM (Share Virtual Memory) 提供了可选的辅助工具,允许设备与CPU通过共享虚拟内存,可以透明地访问同一个内存地址,即任何在CPU上的有效指针也是设备上的有效指针。 
这在高级异构计算中变得越来越必需,例如在GPU、DSP或FPGA上代表进程执行各种计算操作,以简化使用流程。

This document is divided as follows: in the first section I expose the problems
related to using device specific memory allocators. In the second section, I
expose the hardware limitations that are inherent to many platforms. The third
section gives an overview of the HMM design. The fourth section explains how
CPU page-table mirroring works and the purpose of HMM in this context. The
fifth section deals with how device memory is represented inside the kernel.
Finally, the last section presents a new migration helper that allows lever-
aging the device DMA engine.
本文档分为以下几个部分: 
第一部分阐述使用设备特定内存分配器所面临的问题;
第二部分说明了许多平台天生固有的硬件限制;
第三部分概述了HMM的设计
第四部分解释了CPU页表镜像的工作原理以及HMM在这一背景下的目的
第五部分介绍了内核内部设备内存的表示方式
最后一部分介绍了一种新的可以利用DMA引擎的迁移助手,

.. contents:: :local:

Problems of using a device specific memory allocator
使用设备特定内存分配器所面临的问题
====================================================

Devices with a large amount of on board memory (several gigabytes) like GPUs
have historically managed their memory through dedicated driver specific APIs.
This creates a disconnect between memory allocated and managed by a device
driver and regular application memory (private anonymous, shared memory, or
regular file backed memory). From here on I will refer to this aspect as split
address space. I use shared address space to refer to the opposite situation:
i.e., one in which any application memory region can be used by a device
transparently.
过去, 拥有大量内存(几个G bytes)的设备(例如GPU)通过专用的驱动程序API来管理它们的内存。
这导致由设备驱动程序分配和管理的内存与常规应用程序内存（私有匿名、共享内存或常规文件支持的内存）之间存在分离。从现在开始，我将将这一方面称为分离地址空间。
而当一个应用程序内存区域可以被设备透明地使用时，则称之为共享地址空间。

Split address space happens because device can only access memory allocated
through device specific API. This implies that all memory objects in a program
are not equal from the device point of view which complicates large programs
that rely on a wide set of libraries.
分离地址空间的发生是因为设备只能通过设备特定的API访问分配的存储器。这意味着从设备的角度来看,程序中的所有内存对象都不是平等的,这会使依赖广泛的库的大型程序变得更加复杂。

Concretely this means that code that wants to leverage devices like GPUs needs
to copy object between generically allocated memory (malloc, mmap private, mmap
share) and memory allocated through the device driver API (this still ends up
with an mmap but of the device file).
具体来说, 这意味着希望利用GPU等设备的代码需要在通用分配的内存对象(malloc、mmap私有、mmap共享)和通过设备驱动程序API分配的内存之间进行复制(这最终仍然需要使用设备文件的mmap)。

For flat data sets (array, grid, image, ...) this isn't too hard to achieve but
complex data sets (list, tree, ...) are hard to get right. Duplicating a
complex data set needs to re-map all the pointer relations between each of its
elements. This is error prone and program gets harder to debug because of the
duplicate data set and addresses.
对于平坦数据集（数组、网格、图像等），这并不太难实现，但是对于复杂的数据集（列表、树等）确实很难。
复制复杂数据集需要重新映射其每个元素之间的所有指针关系。这很容易出错，并且由于数据集和地址的重复，程序变得更加难以调试。

Split address space also means that libraries cannot transparently use data
they are getting from the core program or another library and thus each library
might have to duplicate its input data set using the device specific memory
allocator. Large projects suffer from this and waste resources because of the
various memory copies.
分离地址空间也意味着库无法透明地使用它们从核心程序或另一个库获取的数据，因此每个库可能都需要使用特定于设备的内存分配器来复制其输入数据集。大型项目受此影响，并因各种内存复制而浪费资源。

Duplicating each library API to accept as input or output memory allocated by
each device specific allocator is not a viable option. It would lead to a
combinatorial explosion in the library entry points.
为每个库API重复接受由每个特定于设备的分配器分配的内存作为输入或输出也不是一种可行的选项。这将导致库入口点的组合爆炸。

Finally, with the advance of high level language constructs (in C++ but in
other languages too) it is now possible for the compiler to leverage GPUs and
other devices without programmer knowledge. Some compiler identified patterns
are only do-able with a shared address space. It is also more reasonable to use
a shared address space for all other patterns.
最后,随着高级语言构造的进步(无论是在C++中还是在其他语言中),现在编译器可以在程序员不知情的情况下利用GPU和其他设备。
一些编译器识别的模式只能使用共享地址空间实现。对于所有其他模式，使用共享地址空间也更加合理。


I/O bus, device memory characteristics
I/O总线、设备内存特性
======================================

I/O buses cripple shared address spaces due to a few limitations. Most I/O
buses only allow basic memory access from device to main memory; even cache
coherency is often optional. Access to device memory from CPU is even more
limited. More often than not, it is not cache coherent.
输入/输出总线由于一些限制使共享地址空间受到了影响。大多数I/O总线只允许设备到主存储器的基本内存访问, 即使缓存一致性通常也是可选的。
从CPU访问设备内存的访问限制更大。往往并不是缓存一致性的。

If we only consider the PCIE bus, then a device can access main memory (often
through an IOMMU) and be cache coherent with the CPUs. However, it only allows
a limited set of atomic operations from device on main memory. This is worse
in the other direction: the CPU can only access a limited range of the device
memory and cannot perform atomic operations on it. Thus device memory cannot
be considered the same as regular memory from the kernel point of view.
如果我们只考虑PCIE总线, 那么设备可以通过IOMMU访问主存储器, 并与CPU之间具有缓存一致性。
然而, 它只允许设备在主存储器上进行有限的原子操作。从另一个方向来看, 这种情况更糟糕: CPU只能访问设备内存的有限范围,并且无法对其执行原子操作。
因此，从内核的角度来看，设备内存不能被视为与普通内存相同。

Another crippling factor is the limited bandwidth (~32GBytes/s with PCIE 4.0
and 16 lanes). This is 33 times less than the fastest GPU memory (1 TBytes/s).
The final limitation is latency. Access to main memory from the device has an
order of magnitude higher latency than when the device accesses its own memory.
另一个影响因素是带宽有限(使用PCIE 4.0和16个通道通常约为32GBytes/s)。这比最快的GPU内存(1 TBytes/s)低了33倍。
最后一个限制是延迟。从设备访问主内存的延迟比从设备访问其自己的内存的延迟高一个数量级。

Some platforms are developing new I/O buses or additions/modifications to PCIE
to address some of these limitations (OpenCAPI, CCIX). They mainly allow two-
way cache coherency between CPU and device and allow all atomic operations the
architecture supports. Sadly, not all platforms are following this trend and
some major architectures are left without hardware solutions to these problems.
一些平台正在开发新的I/O总线或对PCIE进行添加/修改, 以解决其中一些限制(如OpenCAPI、CCIX)。
它们主要允许CPU和设备之间的双向缓存一致性, 并允许架构所支持的所有原子操作。可悲的是，并非所有平台都遵循这种趋势，一些主要的架构在这些问题上没有硬件解决方案。

So for shared address space to make sense, not only must we allow devices to
access any memory but we must also permit any memory to be migrated to device
memory while device is using it (blocking CPU access while it happens).
因此, 为了使共享地址空间有意义, 不仅必须允许设备访问任何内存, 还必须允许在设备使用内存时将任何内存迁移到设备内存中(在此过程中阻塞CPU访问)。

Shared address space and migration
共享地址空间和迁移
==================================

HMM intends to provide two main features. First one is to share the address
space by duplicating the CPU page table in the device page table so the same
address points to the same physical memory for any valid main memory address in
the process address space.
HMM旨在提供两个主要功能。第一个功能是通过在设备页表中复制CPU页表, 从而共享地址空间，因此对于进程地址空间中的任何有效主内存地址，相同的地址都指向相同的物理内存。

To achieve this, HMM offers a set of helpers to populate the device page table
while keeping track of CPU page table updates. Device page table updates are
not as easy as CPU page table updates. To update the device page table, you must
allocate a buffer (or use a pool of pre-allocated buffers) and write GPU
specific commands in it to perform the update (unmap, cache invalidations, and
flush, ...). This cannot be done through common code for all devices. Hence
why HMM provides helpers to factor out everything that can be while leaving the
hardware specific details to the device driver.
为了实现这一点, HMM提供了一组函数助手程序, 可以在跟踪CPU页表更新的同时填充设备页表。设备页表的更新并不像CPU页表的更新那样容易。
要更新设备页表, 您必须分配一个缓冲区(或使用预先分配的缓冲区池), 并编写专门针对GPU的命令以执行更新(取消映射、缓存失效和刷新等)。
这无法通过所有设备的公共代码完成。因此, HMM提供了助手来分离出所有可以分离的内容, 同时将硬件特定的细节留给设备驱动程序。

The second mechanism HMM provides is a new kind of ZONE_DEVICE memory that
allows allocating a struct page for each page of the device memory. Those pages
are special because the CPU cannot map them. However, they allow migrating
main memory to device memory using existing migration mechanisms and everything
looks like a page is swapped out to disk from the CPU point of view. Using a
struct page gives the easiest and cleanest integration with existing mm mech-
anisms. Here again, HMM only provides helpers, first to hotplug new ZONE_DEVICE
memory for the device memory and second to perform migration. Policy decisions
of what and when to migrate things is left to the device driver.
HMM提供的第二种机制是一种新类型的ZONE_DEVICE存储器, 允许为设备存储器的每个页面分配一个struct page。这些页面是特殊的, 因为CPU无法对它们进行映射。
但是, 它们允许使用现有的迁移机制将主内存迁移到设备内存, 从CPU的角度来看, 所有东西都像是将一个页面交换到磁盘上。使用struct page可以使现有的mm机制最简单、最清晰地集成。
同样, HMM只提供帮助程序, 第一个是为设备内存热插入新的ZONE_DEVICE内存, 第二个是执行迁移。什么时候迁移什么东西的策略决定权留给设备驱动程序。

Note that any CPU access to a device page triggers a page fault and a migration
back to main memory. For example, when a page backing a given CPU address A is
migrated from a main memory page to a device page, then any CPU access to
address A triggers a page fault and initiates a migration back to main memory.
需要注意的是, 对设备页面的任何CPU访问都会触发页面错误和迁移回主内存。例如, 当支持给定CPU地址A的页面从主内存页面迁移到设备页面时, 对地址A的任何CPU访问都会触发页面错误并启动迁移回主内存。

With these two features, HMM not only allows a device to mirror process address
space and keeping both CPU and device page table synchronized, but also lever-
ages device memory by migrating the part of the data set that is actively being
used by the device.
通过这两个特性, HMM不仅允许设备镜像进程地址空间并保持CPU和设备页表同步, 还通过迁移设备正在积极使用的数据集的部分来利用设备存储器。


Address space mirroring implementation and API
地址空间镜像的实现和API
==============================================

Address space mirroring's main objective is to allow duplication of a range of
CPU page table into a device page table; HMM helps keep both synchronized. A
device driver that wants to mirror a process address space must start with the
registration of an hmm_mirror struct::
地址空间镜像的主要目标是允许将一定范围的CPU页表复制到设备页表中; HMM有助于保持两者同步。想要镜像进程地址空间的设备驱动程序必须从注册hmm_mirror结构开始:

 int hmm_mirror_register(struct hmm_mirror *mirror,
                         struct mm_struct *mm);
 int hmm_mirror_register_locked(struct hmm_mirror *mirror,
                                struct mm_struct *mm);


The locked variant is to be used when the driver is already holding mmap_sem
of the mm in write mode. The mirror struct has a set of callbacks that are used
to propagate CPU page tables::
当驱动程序已经以写模式占用了mm的mmap_sem时, 应使用锁定变体。镜像结构具有一组回调函数, 用于传播CPU页表:

 struct hmm_mirror_ops {
     /* sync_cpu_device_pagetables() - synchronize page tables
      *
      * @mirror: pointer to struct hmm_mirror
      * @update_type: type of update that occurred to the CPU page table
      * @start: virtual start address of the range to update
      * @end: virtual end address of the range to update
      *
      * This callback ultimately originates from mmu_notifiers when the CPU
      * page table is updated. The device driver must update its page table
      * in response to this callback. The update argument tells what action
      * to perform.
      *
      * The device driver must not return from this callback until the device
      * page tables are completely updated (TLBs flushed, etc); this is a
      * synchronous call.
      */
      void (*update)(struct hmm_mirror *mirror,
                     enum hmm_update action,
                     unsigned long start,
                     unsigned long end);
 };

The device driver must perform the update action to the range (mark range
read only, or fully unmap, ...). The device must be done with the update before
the driver callback returns.

When the device driver wants to populate a range of virtual addresses, it can
use either::

  int hmm_vma_get_pfns(struct vm_area_struct *vma,
                      struct hmm_range *range,
                      unsigned long start,
                      unsigned long end,
                      hmm_pfn_t *pfns);
  int hmm_vma_fault(struct vm_area_struct *vma,
                    struct hmm_range *range,
                    unsigned long start,
                    unsigned long end,
                    hmm_pfn_t *pfns,
                    bool write,
                    bool block);

The first one (hmm_vma_get_pfns()) will only fetch present CPU page table
entries and will not trigger a page fault on missing or non-present entries.
The second one does trigger a page fault on missing or read-only entry if the
write parameter is true. Page faults use the generic mm page fault code path
just like a CPU page fault.

Both functions copy CPU page table entries into their pfns array argument. Each
entry in that array corresponds to an address in the virtual range. HMM
provides a set of flags to help the driver identify special CPU page table
entries.

Locking with the update() callback is the most important aspect the driver must
respect in order to keep things properly synchronized. The usage pattern is::

 int driver_populate_range(...)
 {
      struct hmm_range range;
      ...
 again:
      ret = hmm_vma_get_pfns(vma, &range, start, end, pfns);
      if (ret)
          return ret;
      take_lock(driver->update);
      if (!hmm_vma_range_done(vma, &range)) {
          release_lock(driver->update);
          goto again;
      }

      // Use pfns array content to update device page table

      release_lock(driver->update);
      return 0;
 }

The driver->update lock is the same lock that the driver takes inside its
update() callback. That lock must be held before hmm_vma_range_done() to avoid
any race with a concurrent CPU page table update.

HMM implements all this on top of the mmu_notifier API because we wanted a
simpler API and also to be able to perform optimizations latter on like doing
concurrent device updates in multi-devices scenario.

HMM also serves as an impedance mismatch between how CPU page table updates
are done (by CPU write to the page table and TLB flushes) and how devices
update their own page table. Device updates are a multi-step process. First,
appropriate commands are written to a buffer, then this buffer is scheduled for
execution on the device. It is only once the device has executed commands in
the buffer that the update is done. Creating and scheduling the update command
buffer can happen concurrently for multiple devices. Waiting for each device to
report commands as executed is serialized (there is no point in doing this
concurrently).
HMM还可以充当CPU页表更新方式（通过CPU对页表的写操作和TLB刷新）与设备更新其自身页表之间的阻抗失配。
设备更新是一个多步骤的过程。首先，将适当的命令写入缓冲区，然后将该缓冲区安排在设备上执行。只有当设备在缓冲区中执行了命令之后才完成更新。
为多个设备创建和安排更新命令缓冲区可以并发完成。等待每个设备报告已执行的命令是串行化的（并发执行此操作没有意义）。


Represent and manage device memory from core kernel point of view
从内核的核心角度来表示和管理设备内存。
=================================================================

Several different designs were tried to support device memory. First one used
a device specific data structure to keep information about migrated memory and
HMM hooked itself in various places of mm code to handle any access to
addresses that were backed by device memory. It turns out that this ended up
replicating most of the fields of struct page and also needed many kernel code
paths to be updated to understand this new kind of memory.
试了几种不同的设计来支持设备内存。第一种方法是使用特定于设备的数据结构来保存有关迁移内存的信息，并且HMM将自己钩入mm代码的各个位置，以处理由设备内存支持的任何地址的访问。
结果发现，这实际上复制了大部分struct page的字段，还需要更新许多内核代码路径以理解这种新型内存。

Most kernel code paths never try to access the memory behind a page
but only care about struct page contents. Because of this, HMM switched to
directly using struct page for device memory which left most kernel code paths
unaware of the difference. We only need to make sure that no one ever tries to
map those pages from the CPU side.
大多数内核代码路径都不尝试访问页面背后的内存，只关心struct page的内容。因此，HMM切换到直接使用struct page来表示设备内存，使得大多数内核代码路径意识不到差异。
我们只需要确保没有人从CPU端尝试映射这些页面即可。

HMM provides a set of helpers to register and hotplug device memory as a new
region needing a struct page. This is offered through a very simple API::
HMM提供了一组辅助函数，以注册和热插拔设备内存作为需要struct page的新区域。这是通过一个非常简单的API提供的：

 struct hmm_devmem *hmm_devmem_add(const struct hmm_devmem_ops *ops,
                                   struct device *device,
                                   unsigned long size);
 void hmm_devmem_remove(struct hmm_devmem *devmem);

The hmm_devmem_ops is where most of the important things are::

 struct hmm_devmem_ops {
     void (*free)(struct hmm_devmem *devmem, struct page *page);
     int (*fault)(struct hmm_devmem *devmem,
                  struct vm_area_struct *vma,
                  unsigned long addr,
                  struct page *page,
                  unsigned flags,
                  pmd_t *pmdp);
 };

The first callback (free()) happens when the last reference on a device page is
dropped. This means the device page is now free and no longer used by anyone.
The second callback happens whenever the CPU tries to access a device page
which it cannot do. This second callback must trigger a migration back to
system memory.
第一个回调函数(free())会在设备页面上的最后一个引用被删除时触发。这意味着设备页面现在是可用的，且不再被任何人使用。
第二个回调函数会在CPU尝试访问无法访问的设备页面时触发。这个第二个回调函数必须触发将页面迁移回系统内存。

Migration to and from device memory
迁移到设备内存和从设备内存迁移
===================================

Because the CPU cannot access device memory, migration must use the device DMA
engine to perform copy from and to device memory. For this we need a new
migration helper::
因为CPU无法访问设备内存，迁移必须使用设备DMA引擎来执行从设备内存到系统内存的复制。为此，我们需要一个新的迁移助手：

 int migrate_vma(const struct migrate_vma_ops *ops,
                 struct vm_area_struct *vma,
                 unsigned long mentries,
                 unsigned long start,
                 unsigned long end,
                 unsigned long *src,
                 unsigned long *dst,
                 void *private);

Unlike other migration functions it works on a range of virtual address, there
are two reasons for that. First, device DMA copy has a high setup overhead cost
and thus batching multiple pages is needed as otherwise the migration overhead
makes the whole exercise pointless. The second reason is because the
migration might be for a range of addresses the device is actively accessing.
与其他迁移函数不同的是，它适用于一系列的虚拟地址，有两个原因。首先，设备DMA复制具有高延迟成本，因此需要批量处理多个页面，否则迁移成本会使整个过程变得毫无意义。
第二个原因是，迁移可能是因为设备正在主动访问一系列地址。

The migrate_vma_ops struct defines two callbacks. First one (alloc_and_copy())
controls destination memory allocation and copy operation. Second one is there
to allow the device driver to perform cleanup operations after migration::
migrate_vma_ops结构体定义了两个回调函数。第一个回调函数（alloc_and_copy()）控制目标内存的分配和复制操作。第二个回调函数允许设备驱动程序在迁移之后执行清理操作：

 struct migrate_vma_ops {
     void (*alloc_and_copy)(struct vm_area_struct *vma,
                            const unsigned long *src,
                            unsigned long *dst,
                            unsigned long start,
                            unsigned long end,
                            void *private);
     void (*finalize_and_map)(struct vm_area_struct *vma,
                              const unsigned long *src,
                              const unsigned long *dst,
                              unsigned long start,
                              unsigned long end,
                              void *private);
 };

It is important to stress that these migration helpers allow for holes in the
virtual address range. Some pages in the range might not be migrated for all
the usual reasons (page is pinned, page is locked, ...). This helper does not
fail but just skips over those pages.
需要强调的是，这些迁移助手程序允许虚拟地址范围中存在空洞。由于各种原因（页面被固定，页面被锁定等），该范围内的某些页面可能无法迁移。此助手程序不会失败，而只是跳过这些页面。

The alloc_and_copy() might decide to not migrate all pages in the
range (for reasons under the callback control). For those, the callback just
has to leave the corresponding dst entry empty.
alloc_and_copy()函数可能决定不迁移范围内的所有页面（由回调函数控制的原因）。对于这些页面，回调函数只需将相应的目标地址条目保留为空即可。

Finally, the migration of the struct page might fail (for file backed page) for
various reasons (failure to freeze reference, or update page cache, ...). If
that happens, then the finalize_and_map() can catch any pages that were not
migrated. Note those pages were still copied to a new page and thus we wasted
bandwidth but this is considered as a rare event and a price that we are
willing to pay to keep all the code simpler.
最后，对于基于文件的页面，结构体页面的迁移可能由于各种原因而失败（如无法冻结引用或更新页面缓存等）。
如果发生这种情况，最终的finalize_and_map()函数可以捕捉到未被迁移的任何页面。请注意，这些页面仍被复制到了新页面中，
因此我们浪费了带宽，但这被看作是一种罕见事件和我们愿意为保持所有代码更简单而付出的代价。

Memory cgroup (memcg) and rss accounting
内存控制组（memcg）和RSS记账
========================================

For now device memory is accounted as any regular page in rss counters (either
anonymous if device page is used for anonymous, file if device page is used for
file backed page or shmem if device page is used for shared memory). This is a
deliberate choice to keep existing applications, that might start using device
memory without knowing about it, running unimpacted.
目前，设备内存在RSS计数器中被当作普通页面进行计账（如果设备页面用于匿名页，则被视为匿名页；如果设备页面用于基于文件的页面，则被视为文件页；
如果设备页面用于共享内存，则被视为共享内存）。这是出于故意考虑做出的选择，以确保现有的应用程序仍能正常运行，即使这些应用程序可能未意识到已经开始使用设备内存。

A drawback is that the OOM killer might kill an application using a lot of
device memory and not a lot of regular system memory and thus not freeing much
system memory. We want to gather more real world experience on how applications
and system react under memory pressure in the presence of device memory before
deciding to account device memory differently.
不足之处在于，OOM杀手可能会终止使用大量设备内存但使用较少常规系统内存的应用程序，从而没有释放太多系统内存。
在决定如何区分计算设备内存之前，我们希望在设备内存的存在下收集更多关于应用程序和系统在内存压力下的真实世界经验，并据此进行决策。


Same decision was made for memory cgroup. Device memory pages are accounted
against same memory cgroup a regular page would be accounted to. This does
simplify migration to and from device memory. This also means that migration
back from device memory to regular memory cannot fail because it would
go above memory cgroup limit. We might revisit this choice latter on once we
get more experience in how device memory is used and its impact on memory
resource control.
在内存cgroup方面也做出了相同的决策。设备内存页面被计入常规页面所计入的同一内存cgroup中。这样可以简化设备内存的迁移操作。
同时，这意味着从设备内存返回常规内存的迁移是不会失败的，因为它将在内存cgroup限制之内。在我们获取更多关于设备内存使用以及其对内存资源控制的影响方面的经验后，我们可能会重新审视这项选择。


Note that device memory can never be pinned by device driver nor through GUP
and thus such memory is always free upon process exit. Or when last reference
is dropped in case of shared memory or file backed memory.
需要注意的是，设备内存永远不会被设备驱动程序或通过GUP钉住，因此这种内存在进程退出时总是被释放。或在共享内存或文件支持的内存中，当最后一个引用被删除时释放。