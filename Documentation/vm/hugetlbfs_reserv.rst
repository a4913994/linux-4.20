.. _hugetlbfs_reserve:

=====================
Hugetlbfs Reservation
Hugetlbfs（Huge pages file system）预留空间
=====================

Overview
概览
========

Huge pages as described at :ref:`hugetlbpage` are typically
preallocated for application use.  These huge pages are instantiated in a
task's address space at page fault time if the VMA indicates huge pages are
to be used.  If no huge page exists at page fault time, the task is sent
a SIGBUS and often dies an unhappy death.  Shortly after huge page support
was added, it was determined that it would be better to detect a shortage
of huge pages at mmap() time.  The idea is that if there were not enough
huge pages to cover the mapping, the mmap() would fail.  This was first
done with a simple check in the code at mmap() time to determine if there
were enough free huge pages to cover the mapping.  Like most things in the
kernel, the code has evolved over time.  However, the basic idea was to
'reserve' huge pages at mmap() time to ensure that huge pages would be
available for page faults in that mapping.  The description below attempts to
describe how huge page reserve processing is done in the v4.10 kernel.

如 :ref:hugetlbpage 所述，Huge Pages通常是为应用程序预先分配的。如果VMA指示使用Huge Pages，
则在任务出现页面错误时大页会在任务的地址空间中实例化。如果页面错误时没有Huge Pages存在，则任务会收到SIGBUS信号，并且通常会遇到不愉快的死亡。
在支持Huge Pages不久后，人们发现最好在mmap()时检测Huge Pages不足。这个想法是，如果没有足够的Huge Pages来覆盖映射，则mmap()将失败。
最初，通过在mmap()代码中进行简单的检查来确定是否有足够的Huge Pages可用来覆盖映射来实现此目标。就像内核中的大多数内容一样，该代码随着时间的推移而发展。
但是，基本思想是在mmap()中“预留”Huge Pages，以确保在该映射的页面错误中可用Huge Pages。
下面的描述试图说明在v4.10内核中如何处理Huge Pages预留。

Audience
阅读对象
========
This description is primarily targeted at kernel developers who are modifying
hugetlbfs code.
这篇描述主要是针对正在修改hugetlbfs代码的内核开发人员。


The Data Structures
数据结构
===================

resv_huge_pages
	This is a global (per-hstate) count of reserved huge pages.  Reserved
	huge pages are only available to the task which reserved them.
	Therefore, the number of huge pages generally available is computed
	as (``free_huge_pages - resv_huge_pages``).
	这是保留的巨大页面的全局(每个hstate)计数。保留的巨大页面仅对预留它们的任务可用。因此，通常可用的巨大页面数量计算为(free_huge_pages - resv_huge_pages)。

Reserve Map
	A reserve map is described by the structure::

		struct resv_map {
			struct kref refs;
			spinlock_t lock;
			struct list_head regions;
			long adds_in_progress;
			struct list_head region_cache;
			long region_cache_count;
		};

	There is one reserve map for each huge page mapping in the system.
	The regions list within the resv_map describes the regions within
	the mapping.  A region is described as::
	在系统中每个巨大页面映射都有一个保留映射。resv_map内的regions列表描述了映射内的区域。一个区域被描述为:

		struct file_region {
			struct list_head link;
			long from;
			long to;
		};

	The 'from' and 'to' fields of the file region structure are huge page
	indices into the mapping.  Depending on the type of mapping, a
	region in the reserv_map may indicate reservations exist for the
	range, or reservations do not exist.
	文件区域结构的“from”和“to”字段是映射中的巨大页面索引。根据映射类型，reserv_map中的一个区域可能指示该范围存在保留，也可能不存在保留。

Flags for MAP_PRIVATE Reservations
	These are stored in the bottom bits of the reservation map pointer.
	这些位存储在保留映射指针的最低位中。	

	``#define HPAGE_RESV_OWNER    (1UL << 0)``
		Indicates this task is the owner of the reservations
		associated with the mapping.
		定义此任务是与映射相关的保留的所有者。
	``#define HPAGE_RESV_UNMAPPED (1UL << 1)``
		Indicates task originally mapping this range (and creating
		reserves) has unmapped a page from this task (the child)
		due to a failed COW.
		定义任务最初映射此范围(并创建保留)的任务已经因为COW失败而从此任务(子任务)中取消了页面映射。
Page Flags
	The PagePrivate page flag is used to indicate that a huge page
	reservation must be restored when the huge page is freed.  More
	details will be discussed in the "Freeing huge pages" section.
	巨大页面标志PagePrivate用于指示在释放巨大页面时必须恢复巨大页面保留。更多细节将在“释放巨大页面”部分中讨论。


Reservation Map Location (Private or Shared)
保留映射的位置（私有或共享）
============================================

A huge page mapping or segment is either private or shared.  If private,
it is typically only available to a single address space (task).  If shared,
it can be mapped into multiple address spaces (tasks).  The location and
semantics of the reservation map is significantly different for two types
of mappings.  Location differences are:
一个巨大页面映射或段是私有的还是共享的。
如果是私有的，它通常只对单个地址空间(任务)可用。
如果是共享的，它可以映射到多个地址空间(任务)。保留映射的位置和语义对于两种类型的映射有很大的不同。位置差异是:

- For private mappings, the reservation map hangs off the the VMA structure.
  Specifically, vma->vm_private_data.  This reserve map is created at the
  time the mapping (mmap(MAP_PRIVATE)) is created.
  对于私有映射，保留映射挂在VMA结构上。具体来说，vma->vm_private_data。在创建映射(映射私有)时创建此保留映射。
- For shared mappings, the reservation map hangs off the inode.  Specifically,
  inode->i_mapping->private_data.  Since shared mappings are always backed
  by files in the hugetlbfs filesystem, the hugetlbfs code ensures each inode
  contains a reservation map.  As a result, the reservation map is allocated
  when the inode is created.
  对于共享映射，保留映射挂在inode上。具体来说，inode->i_mapping->private_data。
  由于共享映射总是由hugetlbfs文件系统中的文件支持的，hugetlbfs代码确保每个inode包含一个保留映射。因此，当创建inode时分配保留映射。


Creating Reservations
创建保留
=====================
Reservations are created when a huge page backed shared memory segment is
created (shmget(SHM_HUGETLB)) or a mapping is created via mmap(MAP_HUGETLB).
These operations result in a call to the routine hugetlb_reserve_pages()::
当创建一个巨大页面支持的共享内存段时(Shmget(SHM_HUGETLB))或通过mmap(MAP_HUGETLB)创建映射时，将创建保留。
这些操作导致对hugetlb_reserve_pages()的调用:

	int hugetlb_reserve_pages(struct inode *inode,
				  long from, long to,
				  struct vm_area_struct *vma,
				  vm_flags_t vm_flags)

The first thing hugetlb_reserve_pages() does is check for the NORESERVE
flag was specified in either the shmget() or mmap() call.  If NORESERVE
was specified, then this routine returns immediately as no reservation
are desired.
hugetlb_reserve_pages()首先做的是检查shmget()或mmap()调用中是否指定了NORESERVE标志。 如果指定了NORESERVE，则此例程立即返回，因为不需要保留。

The arguments 'from' and 'to' are huge page indices into the mapping or
underlying file.  For shmget(), 'from' is always 0 and 'to' corresponds to
the length of the segment/mapping.  For mmap(), the offset argument could
be used to specify the offset into the underlying file.  In such a case
the 'from' and 'to' arguments have been adjusted by this offset.
参数“from”和“to”是映射或底层文件中的巨大页面索引。对于shmget()，“from”始终为0，“to”对应于段/映射的长度。
对于mmap()，偏移参数可用于指定底层文件中的偏移量。在这种情况下，“from”和“to”参数已经被此偏移量调整。

One of the big differences between PRIVATE and SHARED mappings is the way
in which reservations are represented in the reservation map.
私有和共享映射之间的一个重要区别是保留映射中保留的表示方式。

- For shared mappings, an entry in the reservation map indicates a reservation
  exists or did exist for the corresponding page.  As reservations are
  consumed, the reservation map is not modified.
  对于共享映射，保留映射中的条目表示对应页面的保留存在或曾经存在。当消耗保留时，保留映射不会被修改。
- For private mappings, the lack of an entry in the reservation map indicates
  a reservation exists for the corresponding page.  As reservations are
  consumed, entries are added to the reservation map.  Therefore, the
  reservation map can also be used to determine which reservations have
  been consumed.
  对于私有映射，保留映射中没有条目表示对应页面的保留存在。当消耗保留时，条目被添加到保留映射中。
  因此，保留映射也可用于确定已消耗的保留。

For private mappings, hugetlb_reserve_pages() creates the reservation map and
hangs it off the VMA structure.  In addition, the HPAGE_RESV_OWNER flag is set
to indicate this VMA owns the reservations.
对于私有映射，hugetlb_reserve_pages()创建保留映射并将其挂在VMA结构上。此外，设置HPAGE_RESV_OWNER标志以指示此VMA拥有保留。

The reservation map is consulted to determine how many huge page reservations
are needed for the current mapping/segment.  For private mappings, this is
always the value (to - from).  However, for shared mappings it is possible that some reservations may already exist within the range (to - from).  See the
section :ref:`Reservation Map Modifications <resv_map_modifications>`
for details on how this is accomplished.
保留映射被查询以确定当前映射/段需要多少巨大页面保留。对于私有映射，这始终是值(to-from)。但是，对于共享映射，可能在范围(to-from)内已经存在一些保留。
有关如何实现此操作的详细信息，请参阅:ref:`Reservation Map Modifications <resv_map_modifications>`部分。

The mapping may be associated with a subpool.  If so, the subpool is consulted
to ensure there is sufficient space for the mapping.  It is possible that the
subpool has set aside reservations that can be used for the mapping.  See the
section :ref:`Subpool Reservations <sub_pool_resv>` for more details.
映射可能与子池相关联。如果是这样，将查询子池以确保映射有足够的空间。可能子池已经为映射保留了一些保留。有关更多详细信息，请参阅:ref:`Subpool Reservations <sub_pool_resv>`部分。

After consulting the reservation map and subpool, the number of needed new
reservations is known.  The routine hugetlb_acct_memory() is called to check
for and take the requested number of reservations.  hugetlb_acct_memory()
calls into routines that potentially allocate and adjust surplus page counts.
However, within those routines the code is simply checking to ensure there
are enough free huge pages to accommodate the reservation.  If there are,
the global reservation count resv_huge_pages is adjusted something like the
following::
在查询保留映射和子池之后，已知所需的新保留数。调用hugetlb_acct_memory()以检查并获取所需的保留数。
hugetlb_acct_memory()调用可能分配和调整剩余页面计数的例程。但是，在这些例程中，代码只是检查是否有足够的空闲巨大页面来容纳保留。
如果有，全局保留计数resv_huge_pages会像下面这样调整::

	if (resv_needed <= (resv_huge_pages - free_huge_pages))
		resv_huge_pages += resv_needed;

Note that the global lock hugetlb_lock is held when checking and adjusting
these counters.
注意，在检查和调整这些计数器时，会持有全局锁hugetlb_lock。

If there were enough free huge pages and the global count resv_huge_pages
was adjusted, then the reservation map associated with the mapping is
modified to reflect the reservations.  In the case of a shared mapping, a
file_region will exist that includes the range 'from' 'to'.  For private
mappings, no modifications are made to the reservation map as lack of an
entry indicates a reservation exists.
如果有足够的空闲巨大页面，并且调整了全局计数resv_huge_pages，则与映射相关联的保留映射将被修改以反映保留。
在共享映射的情况下，将存在包含范围'from'to'的file_region。对于私有映射，不会对保留映射进行任何修改，因为缺少条目表示保留存在。


If hugetlb_reserve_pages() was successful, the global reservation count and
reservation map associated with the mapping will be modified as required to
ensure reservations exist for the range 'from' - 'to'.
如果hugetlb_reserve_pages()成功，则将根据需要修改与映射相关联的全局保留计数和保留映射，以确保范围'from'to'的保留存在。


.. _consume_resv:

Consuming Reservations/Allocating a Huge Page
消耗保留内存/分配一个巨大页面
=============================================

Reservations are consumed when huge pages associated with the reservations
are allocated and instantiated in the corresponding mapping.  The allocation
is performed within the routine alloc_huge_page()::
当与保留相关的巨大页面被分配并实例化到相应的映射中时，将消耗保留。分配是在alloc_huge_page()例程中执行的::

	struct page *alloc_huge_page(struct vm_area_struct *vma,
				     unsigned long addr, int avoid_reserve)

alloc_huge_page is passed a VMA pointer and a virtual address, so it can
consult the reservation map to determine if a reservation exists.  In addition,
alloc_huge_page takes the argument avoid_reserve which indicates reserves
should not be used even if it appears they have been set aside for the
specified address.  The avoid_reserve argument is most often used in the case
of Copy on Write and Page Migration where additional copies of an existing
page are being allocated.
alloc_huge_page函数接收VMA指针和虚拟地址作为参数，因此它可以通过查询保留映射来确定是否存在保留空间。
此外，alloc_huge_page还接收参数avoid_reserve，它表示即使看起来已经为指定的地址保留了空间，也不应使用这些保留空间。
在存在写时复制和页面迁移的情况下，avoid_reserve参数最常被使用，用于分配现有页面的附加副本。

The helper routine vma_needs_reservation() is called to determine if a
reservation exists for the address within the mapping(vma).  See the section
:ref:`Reservation Map Helper Routines <resv_map_helpers>` for detailed
information on what this routine does.
调用辅助例程vma_needs_reservation()来确定映射(vma)中的地址是否存在保留。
有关此例程执行的操作的详细信息，请参阅:ref:`Reservation Map Helper Routines <resv_map_helpers>`部分。

The value returned from vma_needs_reservation() is generally
0 or 1.  0 if a reservation exists for the address, 1 if no reservation exists.
If a reservation does not exist, and there is a subpool associated with the
mapping the subpool is consulted to determine if it contains reservations.
If the subpool contains reservations, one can be used for this allocation.
However, in every case the avoid_reserve argument overrides the use of
a reservation for the allocation.  After determining whether a reservation
exists and can be used for the allocation, the routine dequeue_huge_page_vma()
is called.  This routine takes two arguments related to reservations:
vma_needs_reservation()函数的返回值通常是0或1。如果指定地址存在保留空间，则返回0；如果不存在保留空间，则返回1。
如果映射关联了子池，且子池包含保留空间，则可以使用其中一个用于此次分配。但无论何种情况，avoid_reserve参数都会优先覆盖保留空间的使用。
在确定是否存在可用的保留空间后，调用dequeue_huge_page_vma()函数。这个函数接收两个与保留空间相关的参数：


- avoid_reserve, this is the same value/argument passed to alloc_huge_page()
  avoid_reserve是传递给alloc_huge_page()函数的相同参数值。

- chg, even though this argument is of type long only the values 0 or 1 are
  passed to dequeue_huge_page_vma.  If the value is 0, it indicates a
  reservation exists (see the section "Memory Policy and Reservations" for
  possible issues).  If the value is 1, it indicates a reservation does not
  exist and the page must be taken from the global free pool if possible.
  chg参数虽然是long类型，但是传递给dequeue_huge_page_vma()函数的值只有0和1。如果该值为0，则表示存在保留空间（可能会出现“内存策略和保留空间”部分所述的问题）。
  如果该值为1，则表示不存在保留空间，必须尽可能从全局空闲池中获取页面。

The free lists associated with the memory policy of the VMA are searched for
a free page.  If a page is found, the value free_huge_pages is decremented
when the page is removed from the free list.  If there was a reservation
associated with the page, the following adjustments are made::
针对VMA的内存策略，会搜索与其关联的空闲列表以寻找空闲页面。如果找到了一个页面，则当该页面从空闲列表中移除时，free_huge_pages的值将减少。
如果该页面与保留空间有关联，则将进行以下调整：

	SetPagePrivate(page);	/* Indicates allocating this page consumed
				 * a reservation, and if an error is
				 * encountered such that the page must be
				 * freed, the reservation will be restored. */
	resv_huge_pages--;	/* Decrement the global reservation count */

Note, if no huge page can be found that satisfies the VMA's memory policy
an attempt will be made to allocate one using the buddy allocator.  This
brings up the issue of surplus huge pages and overcommit which is beyond
the scope reservations.  Even if a surplus page is allocated, the same
reservation based adjustments as above will be made: SetPagePrivate(page) and
resv_huge_pages--.
需要注意的是，如果没有找到满足VMA内存策略的大页面，则尝试使用伙伴分配器进行分配。这引发了大页面过剩和超额提交的问题，这超出了保留空间的范围。
即使分配了多余的页面，也会进行与上述相同的基于保留空间的调整：设置page为私有页面（SetPagePrivate(page)）并且 resv_huge_pages--。

After obtaining a new huge page, (page)->private is set to the value of
the subpool associated with the page if it exists.  This will be used for
subpool accounting when the page is freed.
获取新的大页面后，将（page）-> private设置为与页面关联的子池的值（如果存在）。当页面被释放时，将使用此值进行子池核算。

The routine vma_commit_reservation() is then called to adjust the reserve
map based on the consumption of the reservation.  In general, this involves
ensuring the page is represented within a file_region structure of the region
map.  For shared mappings where the the reservation was present, an entry
in the reserve map already existed so no change is made.  However, if there
was no reservation in a shared mapping or this was a private mapping a new
entry must be created.
然后调用vma_commit_reservation()函数来根据保留空间的消耗来调整保留映射。一般来说，这涉及确保页面在区域映射的file_region结构中表示。
对于存在保留空间的共享映射，保留映射中已经存在一个条目，因此不做任何更改。但是，如果共享映射中没有保留空间或者这是一个私有映射，则必须创建一个新的条目。

It is possible that the reserve map could have been changed between the call
to vma_needs_reservation() at the beginning of alloc_huge_page() and the
call to vma_commit_reservation() after the page was allocated.  This would
be possible if hugetlb_reserve_pages was called for the same page in a shared
mapping.  In such cases, the reservation count and subpool free page count
will be off by one.  This rare condition can be identified by comparing the
return value from vma_needs_reservation and vma_commit_reservation.  If such
a race is detected, the subpool and global reserve counts are adjusted to
compensate.  See the section
在调用alloc_huge_page()函数时，开始进行保留空间需求检查的vma_needs_reservation()函数和在分配了页面后调用vma_commit_reservation()函数之间，保留空间映射可能已经发生变化。
如果在共享映射中为同一页面调用了hugetlb_reserve_pages()函数，则会出现这种情况。在这种情况下，保留计数和子池空闲页面计数将相差一。
检测到这种罕见情况时，子池和全局的保留计数将进行调整以进行补偿。请见相关章节。
:ref:`Reservation Map Helper Routines <resv_map_helpers>` for more
information on these routines.


Instantiate Huge Pages
实例化大页面（注：这里的“实例化”指的是将大页面映射到用户空间）
======================

After huge page allocation, the page is typically added to the page tables
of the allocating task.  Before this, pages in a shared mapping are added
to the page cache and pages in private mappings are added to an anonymous
reverse mapping.  In both cases, the PagePrivate flag is cleared.  Therefore,
when a huge page that has been instantiated is freed no adjustment is made
to the global reservation count (resv_huge_pages).
在大页面分配之后，通常会将该页面添加到分配任务的页表中。在此之前，共享映射中的页面将添加到页面缓存中，而私有映射中的页面将添加到匿名反向映射中。
在这两种情况下，PagePrivate标志都会被清除。因此，当实例化的大页面被释放时，不会对全局保留计数（resv_huge_pages）进行调整。

Freeing Huge Pages
释放大页面
==================

Huge page freeing is performed by the routine free_huge_page().  This routine
is the destructor for hugetlbfs compound pages.  As a result, it is only
passed a pointer to the page struct.  When a huge page is freed, reservation
accounting may need to be performed.  This would be the case if the page was
associated with a subpool that contained reserves, or the page is being freed
on an error path where a global reserve count must be restored.
free_huge_page()函数用于释放大页面。这个函数是hugetlbfs复合页面的析构函数。因此，它只能传递一个指向页面结构的指针。
当释放大页面时，可能需要进行保留空间核算。如果页面与包含保留空间的子池相关联，或者页面正在被释放，这将是一种情况，而且全局保留计数必须被恢复。

The page->private field points to any subpool associated with the page.
If the PagePrivate flag is set, it indicates the global reserve count should
be adjusted (see the section
:ref:`Consuming Reservations/Allocating a Huge Page <consume_resv>`
for information on how these are set).
page->private字段指向与页面相关联的任何子池。如果设置了PagePrivate标志，则表示应该调整全局保留计数（有关如何设置这些内容的信息，请参见“消耗保留空间/分配大页面”部分）。

The routine first calls hugepage_subpool_put_pages() for the page.  If this
routine returns a value of 0 (which does not equal the value passed 1) it
indicates reserves are associated with the subpool, and this newly free page
must be used to keep the number of subpool reserves above the minimum size.
Therefore, the global resv_huge_pages counter is incremented in this case.
该过程首先为页面调用hugepage_subpool_put_pages()函数。如果此函数返回值为0（即不等于传递的值1），则表示该子池与保留相关，
并且必须使用这个新释放的页面来保持子池保留数量超过最小大小。在这种情况下，全局的resv_huge_pages计数器将被递增。

If the PagePrivate flag was set in the page, the global resv_huge_pages counter
will always be incremented.
如果在页面中设置了PagePrivate标志，则全局的resv_huge_pages计数器将始终被递增。

.. _sub_pool_resv:

Subpool Reservations
子池保留
====================

There is a struct hstate associated with each huge page size.  The hstate
tracks all huge pages of the specified size.  A subpool represents a subset
of pages within a hstate that is associated with a mounted hugetlbfs
filesystem.
每个大页面大小都有一个与之相关联的struct hstate。hstate跟踪指定大小的所有大页面。子池表示与已挂载的hugetlbfs文件系统相关联的hstate中页面的子集。

When a hugetlbfs filesystem is mounted a min_size option can be specified
which indicates the minimum number of huge pages required by the filesystem.
If this option is specified, the number of huge pages corresponding to
min_size are reserved for use by the filesystem.  This number is tracked in
the min_hpages field of a struct hugepage_subpool.  At mount time,
hugetlb_acct_memory(min_hpages) is called to reserve the specified number of
huge pages.  If they can not be reserved, the mount fails.
当挂载hugetlbfs文件系统时，可以指定一个min_size选项，该选项指示文件系统所需的最小大页面数。如果指定了此选项，
则对应于min_size的大页面数将被保留供文件系统使用。这个数字在struct hugepage_subpool的min_hpages字段中跟踪。
在挂载时，调用hugetlb_acct_memory(min_hpages)函数来保留指定数量的大页面。如果不能保留这些页面，则挂载失败。

The routines hugepage_subpool_get/put_pages() are called when pages are
obtained from or released back to a subpool.  They perform all subpool
accounting, and track any reservations associated with the subpool.
hugepage_subpool_get/put_pages are passed the number of huge pages by which
to adjust the subpool 'used page' count (down for get, up for put).  Normally,
they return the same value that was passed or an error if not enough pages
exist in the subpool.
当从子池中获取或释放页面时，会调用hugepage_subpool_get/put_pages()函数。它们执行所有子池核算，并跟踪与子池相关的任何保留。
hugepage_subpool_get/put_pages函数通过调整子池“已使用页面”计数来传递大页面的数量（下降用于get，上升用于put）。
通常，它们返回传递的相同值，如果子池中不存在足够的页面，则返回错误。

However, if reserves are associated with the subpool a return value less
than the passed value may be returned.  This return value indicates the
number of additional global pool adjustments which must be made.  For example,
suppose a subpool contains 3 reserved huge pages and someone asks for 5.
The 3 reserved pages associated with the subpool can be used to satisfy part
of the request.  But, 2 pages must be obtained from the global pools.  To
relay this information to the caller, the value 2 is returned.  The caller
is then responsible for attempting to obtain the additional two pages from
the global pools.
但是，如果子池与保留相关，则可能返回小于传递值的返回值。此返回值表示必须进行的其他全局池调整的数量。
例如，假设一个子池包含3个保留的大页面，而有人要求5个。与子池相关的3个保留页面可用于满足部分请求。
但是，必须从全局池中获取2个页面。为了将此信息传递给调用者，返回值为2。然后，调用者负责尝试从全局池中获取额外的两个页面。


COW and Reservations
COW和保留
====================

Since shared mappings all point to and use the same underlying pages, the
biggest reservation concern for COW is private mappings.  In this case,
two tasks can be pointing at the same previously allocated page.  One task
attempts to write to the page, so a new page must be allocated so that each
task points to its own page.
由于共享映射都指向并使用相同的底层页面，因此COW的最大保留问题是私有映射。在这种情况下，两个任务可以指向同一个先前分配的页面。
一个任务尝试写入页面，因此必须分配一个新页面，以便每个任务都指向自己的页面。

When the page was originally allocated, the reservation for that page was
consumed.  When an attempt to allocate a new page is made as a result of
COW, it is possible that no free huge pages are free and the allocation
will fail.
当最初分配页面时，该页面的保留就被消耗了。当尝试分配一个新页面作为COW的结果时，可能没有空闲的大页面，并且分配将失败。

When the private mapping was originally created, the owner of the mapping
was noted by setting the HPAGE_RESV_OWNER bit in the pointer to the reservation
map of the owner.  Since the owner created the mapping, the owner owns all
the reservations associated with the mapping.  Therefore, when a write fault
occurs and there is no page available, different action is taken for the owner
and non-owner of the reservation.
当最初创建私有映射时，通过在指向所有者的保留映射的指针中设置HPAGE_RESV_OWNER位来记录映射的所有者。
由于所有者创建了映射，因此所有者拥有与映射相关的所有保留。因此，当发生写入故障并且没有可用页面时，

In the case where the faulting task is not the owner, the fault will fail and
the task will typically receive a SIGBUS.
对于故障任务不是所有者的情况，故障将失败，并且任务通常会收到SIGBUS。

If the owner is the faulting task, we want it to succeed since it owned the
original reservation.  To accomplish this, the page is unmapped from the
non-owning task.  In this way, the only reference is from the owning task.
In addition, the HPAGE_RESV_UNMAPPED bit is set in the reservation map pointer
of the non-owning task.  The non-owning task may receive a SIGBUS if it later
faults on a non-present page.  But, the original owner of the
mapping/reservation will behave as expected.
如果所有者是故障任务，我们希望它成功，因为它拥有原始保留。为此，将页面从非所有者任务中取消映射。
这样，唯一的引用来自所有者任务。此外，将HPAGE_RESV_UNMAPPED位设置在非所有者任务的保留映射指针中。
如果非所有者任务稍后在非存在页面上发生故障，则可能会收到SIGBUS。但是，映射/保留的原始所有者将按预期的方式运行。


.. _resv_map_modifications:

Reservation Map Modifications
保留映射修改
=============================

The following low level routines are used to make modifications to a
reservation map.  Typically, these routines are not called directly.  Rather,
a reservation map helper routine is called which calls one of these low level
routines.  These low level routines are fairly well documented in the source
code (mm/hugetlb.c).  These routines are::
接下来的低级例程用于对保留映射进行修改。通常，这些例程不会直接调用。相反，调用保留映射助手例程，该例程调用这些低级例程之一。
这些低级例程在源代码（mm/hugetlb.c）中有很好的文档。这些例程是：

	long region_chg(struct resv_map *resv, long f, long t);
	long region_add(struct resv_map *resv, long f, long t);
	void region_abort(struct resv_map *resv, long f, long t);
	long region_count(struct resv_map *resv, long f, long t);

Operations on the reservation map typically involve two operations:
操作保留映射通常涉及两个操作：
1) region_chg() is called to examine the reserve map and determine how
   many pages in the specified range [f, t) are NOT currently represented.
   region_chg()是调用来检查保留映射并确定指定范围[f，t)中的多少页当前未表示。

   The calling code performs global checks and allocations to determine if
   there are enough huge pages for the operation to succeed.
   调用代码执行全局检查和分配，以确定是否有足够的大页面使操作成功。

2)
  a) If the operation can succeed, region_add() is called to actually modify
     the reservation map for the same range [f, t) previously passed to
     region_chg().
	 如果操作可以成功，region_add()被调用以实际修改相同的范围[f，t)的保留映射，该范围先前传递给region_chg()。
  b) If the operation can not succeed, region_abort is called for the same
     range [f, t) to abort the operation.
	 如果操作无法成功，region_abort被调用以中止相同的范围[f，t)的操作。

Note that this is a two step process where region_add() and region_abort()
are guaranteed to succeed after a prior call to region_chg() for the same
range.  region_chg() is responsible for pre-allocating any data structures
necessary to ensure the subsequent operations (specifically region_add()))
will succeed.
注意，这是一个两步过程，其中region_add()和region_abort()在先前对相同范围的region_chg()调用之后保证会成功。
region_chg()负责预先分配任何必要的数据结构，以确保后续操作（特别是region_add()）将成功。

As mentioned above, region_chg() determines the number of pages in the range
which are NOT currently represented in the map.  This number is returned to
the caller.  region_add() returns the number of pages in the range added to
the map.  In most cases, the return value of region_add() is the same as the
return value of region_chg().  However, in the case of shared mappings it is
possible for changes to the reservation map to be made between the calls to
region_chg() and region_add().  In this case, the return value of region_add()
will not match the return value of region_chg().  It is likely that in such
cases global counts and subpool accounting will be incorrect and in need of
adjustment.  It is the responsibility of the caller to check for this condition
and make the appropriate adjustments.
如上所述，region_chg()确定范围中不在映射中表示的页数。该数字返回给调用者。region_add()返回范围中添加到映射中的页数。
在大多数情况下，region_add()的返回值与region_chg()的返回值相同。但是，在共享映射的情况下，可能会在调用region_chg()和region_add()之间对保留映射进行更改。
在这种情况下，region_add()的返回值将不匹配region_chg()的返回值。在这种情况下，全局计数和子池会计可能不正确并需要调整。
调用者有责任检查此条件并进行适当的调整。

The routine region_del() is called to remove regions from a reservation map.
It is typically called in the following situations:
region_del()例程用于从保留映射中删除区域。它通常在以下情况下被调用：

- When a file in the hugetlbfs filesystem is being removed, the inode will
  be released and the reservation map freed.  Before freeing the reservation
  map, all the individual file_region structures must be freed.  In this case
  region_del is passed the range [0, LONG_MAX).
  当在hugetlbfs文件系统中删除文件时，将释放inode并释放保留映射。在释放保留映射之前，必须释放所有单个file_region结构。
  在这种情况下，region_del传递范围[0，LONG_MAX)。
- When a hugetlbfs file is being truncated.  In this case, all allocated pages
  after the new file size must be freed.  In addition, any file_region entries
  in the reservation map past the new end of file must be deleted.  In this
  case, region_del is passed the range [new_end_of_file, LONG_MAX).
  当hugetlbfs文件被截断时。在这种情况下，必须释放新文件大小之后的所有分配的页面。此外，保留映射中的任何file_region条目都必须在文件的新结尾之后删除。
  在这种情况下，region_del传递范围[new_end_of_file，LONG_MAX)。
- When a hole is being punched in a hugetlbfs file.  In this case, huge pages
  are removed from the middle of the file one at a time.  As the pages are
  removed, region_del() is called to remove the corresponding entry from the
  reservation map.  In this case, region_del is passed the range
  [page_idx, page_idx + 1).
  当在hugetlbfs文件中打孔时。在这种情况下，从文件的中间一次删除一个巨大页面。删除页面时，调用region_del()以从保留映射中删除相应的条目。
  在这种情况下，region_del传递范围[page_idx，page_idx + 1)。

In every case, region_del() will return the number of pages removed from the
reservation map.  In VERY rare cases, region_del() can fail.  This can only
happen in the hole punch case where it has to split an existing file_region
entry and can not allocate a new structure.  In this error case, region_del()
will return -ENOMEM.  The problem here is that the reservation map will
indicate that there is a reservation for the page.  However, the subpool and
global reservation counts will not reflect the reservation.  To handle this
situation, the routine hugetlb_fix_reserve_counts() is called to adjust the
counters so that they correspond with the reservation map entry that could
not be deleted.
在每种情况下，region_del()都会返回从保留映射中删除的页数。在非常罕见的情况下，region_del()可能会失败。
这只能发生在孔打孔的情况下，它必须拆分现有的file_region条目并且无法分配新结构。在这种错误情况下，region_del()将返回-ENOMEM。
这里的问题是，保留映射将指示该页有保留。但是，子池和全局保留计数不会反映保留。为了处理这种情况，调用hugetlb_fix_reserve_counts()例程来调整计数器，以便它们与无法删除的保留映射条目相对应。

region_count() is called when unmapping a private huge page mapping.  In
private mappings, the lack of a entry in the reservation map indicates that
a reservation exists.  Therefore, by counting the number of entries in the
reservation map we know how many reservations were consumed and how many are
outstanding (outstanding = (end - start) - region_count(resv, start, end)).
Since the mapping is going away, the subpool and global reservation counts
are decremented by the number of outstanding reservations.
当取消映射私有巨大页面映射时，调用region_count()。在私有映射中，保留映射中没有条目的缺乏表示存在保留。
因此，通过计算保留映射中的条目数，我们知道消耗了多少保留以及有多少保留（outstanding =（end - start）- region_count（resv，start，end））。
由于映射即将消失，因此通过未决保留的数量减少子池和全局保留计数。

.. _resv_map_helpers:

Reservation Map Helper Routines
预留映射助手例程
===============================

Several helper routines exist to query and modify the reservation maps.
These routines are only interested with reservations for a specific huge
page, so they just pass in an address instead of a range.  In addition,
they pass in the associated VMA.  From the VMA, the type of mapping (private
or shared) and the location of the reservation map (inode or VMA) can be
determined.  These routines simply call the underlying routines described
in the section "Reservation Map Modifications".  However, they do take into
account the 'opposite' meaning of reservation map entries for private and
shared mappings and hide this detail from the caller::
存在几个助手例程来查询和修改预留映射。这些例程只对特定巨大页面的保留感兴趣，因此它们只需传递地址而不是范围。
此外，它们传递相关的VMA。从VMA，可以确定映射的类型（私有或共享）以及保留映射的位置（inode或VMA）。
这些例程只是调用“预留映射修改”部分中描述的底层例程。但是，它们确实考虑了私有和共享映射的预留映射条目的“相反”含义，并且从调用者隐藏了这个细节：

	long vma_needs_reservation(struct hstate *h,
				   struct vm_area_struct *vma,
				   unsigned long addr)

This routine calls region_chg() for the specified page.  If no reservation
exists, 1 is returned.  If a reservation exists, 0 is returned::

	long vma_commit_reservation(struct hstate *h,
				    struct vm_area_struct *vma,
				    unsigned long addr)

This calls region_add() for the specified page.  As in the case of region_chg
and region_add, this routine is to be called after a previous call to
vma_needs_reservation.  It will add a reservation entry for the page.  It
returns 1 if the reservation was added and 0 if not.  The return value should
be compared with the return value of the previous call to
vma_needs_reservation.  An unexpected difference indicates the reservation
map was modified between calls::

	void vma_end_reservation(struct hstate *h,
				 struct vm_area_struct *vma,
				 unsigned long addr)

This calls region_abort() for the specified page.  As in the case of region_chg
and region_abort, this routine is to be called after a previous call to
vma_needs_reservation.  It will abort/end the in progress reservation add
operation::

	long vma_add_reservation(struct hstate *h,
				 struct vm_area_struct *vma,
				 unsigned long addr)

This is a special wrapper routine to help facilitate reservation cleanup
on error paths.  It is only called from the routine restore_reserve_on_error().
This routine is used in conjunction with vma_needs_reservation in an attempt
to add a reservation to the reservation map.  It takes into account the
different reservation map semantics for private and shared mappings.  Hence,
region_add is called for shared mappings (as an entry present in the map
indicates a reservation), and region_del is called for private mappings (as
the absence of an entry in the map indicates a reservation).  See the section
"Reservation cleanup in error paths" for more information on what needs to
be done on error paths.


Reservation Cleanup in Error Paths
错误路径中的保留清理
==================================

As mentioned in the section
:ref:`Reservation Map Helper Routines <resv_map_helpers>`, reservation
map modifications are performed in two steps.  First vma_needs_reservation
is called before a page is allocated.  If the allocation is successful,
then vma_commit_reservation is called.  If not, vma_end_reservation is called.
Global and subpool reservation counts are adjusted based on success or failure
of the operation and all is well.
如 :ref:保留映射帮助例程 <resv_map_helpers> 部分所述，保留映射的修改分为两个步骤。
首先，在分配页面之前调用 vma_needs_reservation。如果分配成功，则调用 vma_commit_reservation。如果未成功，就会调用 vma_end_reservation。根据操作的成功或失败，
全局和子池保留计数会得到相应调整，一切都会井然有序。

Additionally, after a huge page is instantiated the PagePrivate flag is
cleared so that accounting when the page is ultimately freed is correct.
另外，在巨大页面被实例化之后，PagePrivate 标志被清除，以便在最终释放页面时进行正确的计算。

However, there are several instances where errors are encountered after a huge
page is allocated but before it is instantiated.  In this case, the page
allocation has consumed the reservation and made the appropriate subpool,
reservation map and global count adjustments.  If the page is freed at this
time (before instantiation and clearing of PagePrivate), then free_huge_page
will increment the global reservation count.  However, the reservation map
indicates the reservation was consumed.  This resulting inconsistent state
will cause the 'leak' of a reserved huge page.  The global reserve count will
be  higher than it should and prevent allocation of a pre-allocated page.
但是，在分配巨大页面之后，但在实例化之前遇到错误的情况下，会出现几个实例。在这种情况下，
页面分配已经消耗了预留，并进行了适当的子池，预留映射和全局计数调整。如果在此时释放页面（在实例化和清除 PagePrivate 之前），
则 free_huge_page 将增加全局预留计数。但是，保留映射表明预留已经消耗。这种导致不一致状态的结果将导致保留的巨大页面“泄漏”。

The routine restore_reserve_on_error() attempts to handle this situation.  It
is fairly well documented.  The intention of this routine is to restore
the reservation map to the way it was before the page allocation.   In this
way, the state of the reservation map will correspond to the global reservation
count after the page is freed.
restore_reserve_on_error() 试图处理这种情况。它的文档相当详细。这个例程的意图是在页面分配之前将保留映射恢复到原来的状态。
这样，页面释放后，保留映射的状态将与全局保留计数相对应。

The routine restore_reserve_on_error itself may encounter errors while
attempting to restore the reservation map entry.  In this case, it will
simply clear the PagePrivate flag of the page.  In this way, the global
reserve count will not be incremented when the page is freed.  However, the
reservation map will continue to look as though the reservation was consumed.
A page can still be allocated for the address, but it will not use a reserved
page as originally intended.
restore_reserve_on_error 本身在尝试恢复保留映射条目时可能会遇到错误。在这种情况下，它将简单地清除页面的 PagePrivate 标志。
这样，当页面被释放时，全局保留计数将不会增加。但是，保留映射将继续看起来好像预留已经消耗。仍然可以为地址分配页面，
但它不会使用预留的页面，如最初所预期的那样。

There is some code (most notably userfaultfd) which can not call
restore_reserve_on_error.  In this case, it simply modifies the PagePrivate
so that a reservation will not be leaked when the huge page is freed.
有一些代码（最重要的是 userfaultfd）不能调用 restore_reserve_on_error。在这种情况下，
它只是修改 PagePrivate，以便在释放巨大页面时不会泄漏保留。


Reservations and Memory Policy
保留和内存策略
==============================
Per-node huge page lists existed in struct hstate when git was first used
to manage Linux code.  The concept of reservations was added some time later.
When reservations were added, no attempt was made to take memory policy
into account.  While cpusets are not exactly the same as memory policy, this
comment in hugetlb_acct_memory sums up the interaction between reservations
and cpusets/memory policy::
每个节点的巨大页面列表在 git 第一次用于管理 Linux 代码时存在于 struct hstate 中。
预留的概念在稍后添加。当添加预留时，没有尝试考虑内存策略。虽然 cpusets 并不完全与内存策略相同，
但 hugetlb_acct_memory 中的注释总结了预留和 cpusets/内存策略之间的交互::

	/*
	 * When cpuset is configured, it breaks the strict hugetlb page
	 * reservation as the accounting is done on a global variable. Such
	 * reservation is completely rubbish in the presence of cpuset because
	 * the reservation is not checked against page availability for the
	 * current cpuset. Application can still potentially OOM'ed by kernel
	 * with lack of free htlb page in cpuset that the task is in.
	 * Attempt to enforce strict accounting with cpuset is almost
	 * impossible (or too ugly) because cpuset is too fluid that
	 * task or memory node can be dynamically moved between cpusets.
	 * 当 cpuset 被配置时，它会打破严格的巨大页面预留，因为会在全局变量上进行计算。
	 * 在 cpuset 存在的情况下，这种预留完全是废话，因为预留不会针对当前 cpuset 的页面可用性进行检查。
	 * 应用程序仍然可能由于缺乏 cpuset 中的空闲 htlb 页面而被内核 OOM。
	 * 尝试在 cpuset 中强制执行严格的会计几乎是不可能的（或者太丑陋），因为 cpuset 太流动了，
	 * 任务或内存节点可以在 cpusets 之间动态移动。

	 * The change of semantics for shared hugetlb mapping with cpuset is
	 * undesirable. However, in order to preserve some of the semantics,
	 * we fall back to check against current free page availability as
	 * a best attempt and hopefully to minimize the impact of changing
	 * semantics that cpuset has.
	 * 与 cpuset 共享的共享 hugetlb 映射的语义的变化是不可取的。但是，为了保留一些语义，
	 * 我们回退到检查当前空闲页面可用性，以最大限度地减少 cpuset 的语义变化对影响。
	 */

Huge page reservations were added to prevent unexpected page allocation
failures (OOM) at page fault time.  However, if an application makes use
of cpusets or memory policy there is no guarantee that huge pages will be
available on the required nodes.  This is true even if there are a sufficient
number of global reservations.
巨大页面预留是为了防止在页面故障时出现意外的页面分配失败（OOM）。但是，如果应用程序使用 cpusets 或内存策略，
则不能保证巨大页面将在所需节点上可用。即使有足够的全局预留，这也是正确的。

Hugetlbfs regression testing
巨大页面文件系统回归测试
============================

The most complete set of hugetlb tests are in the libhugetlbfs repository.
If you modify any hugetlb related code, use the libhugetlbfs test suite
to check for regressions.  In addition, if you add any new hugetlb
functionality, please add appropriate tests to libhugetlbfs.
最完整的 hugetlb 测试集在 libhugetlbfs 仓库中。如果您修改任何 hugetlb 相关代码，请使用 libhugetlbfs 测试套件来检查回归。
此外，如果您添加任何新的 hugetlb 功能，请将适当的测试添加到 libhugetlbfs 中。

--
Mike Kravetz, 7 April 2017
