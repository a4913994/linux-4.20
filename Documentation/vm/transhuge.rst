.. _transhuge:

============================
Transparent Hugepage Support
透明巨页支持
============================

This document describes design principles Transparent Hugepage (THP)
Support and its interaction with other parts of the memory management.
这个文档描述了透明巨页（THP）支持的设计原则以及它与内存管理的其他部分的交互。

Design principles
设计原则
=================

- "graceful fallback": mm components which don't have transparent hugepage
  knowledge fall back to breaking huge pmd mapping into table of ptes and,
  if necessary, split a transparent hugepage. Therefore these components
  can continue working on the regular pages or regular pte mappings.
  "小心回退"：mm组件不具有透明巨页知识，回退到破坏巨型pmd映射到表的ptes，如果必要，分裂一个透明巨页。因此，这些组件可以继续在常规页面或常规pte映射上工作。

- if a hugepage allocation fails because of memory fragmentation,
  regular pages should be gracefully allocated instead and mixed in
  the same vma without any failure or significant delay and without
  userland noticing
  如果由于内存碎片导致巨页分配失败，则应该正常分配常规页面，而不是混合在同一个vma中，而不会出现任何失败或显着延迟，而且用户不会注意到

- if some task quits and more hugepages become available (either
  immediately in the buddy or through the VM), guest physical memory
  backed by regular pages should be relocated on hugepages
  automatically (with khugepaged)
  如果一些任务退出并且更多的巨页变得可用（无论是立即在伙伴中还是通过VM），则应该自动将由常规页面支持的客户机物理内存重新定位在巨页上（通过khugepaged）

- it doesn't require memory reservation and in turn it uses hugepages
  whenever possible (the only possible reservation here is kernelcore=
  to avoid unmovable pages to fragment all the memory but such a tweak
  is not specific to transparent hugepage support and it's a generic
  feature that applies to all dynamic high order allocations in the
  kernel)
  它不需要内存预留，反过来，它尽可能使用巨页（唯一可能的预留是kernelcore=以避免不可移动的页面碎片化所有内存，但这种调整不是透明巨页支持的特定内容，它是内核中所有动态高阶分配的通用功能）

get_user_pages and follow_page
==============================

get_user_pages and follow_page if run on a hugepage, will return the
head or tail pages as usual (exactly as they would do on
hugetlbfs). Most gup users will only care about the actual physical
address of the page and its temporary pinning to release after the I/O
is complete, so they won't ever notice the fact the page is huge. But
if any driver is going to mangle over the page structure of the tail
page (like for checking page->mapping or other bits that are relevant
for the head page and not the tail page), it should be updated to jump
to check head page instead. Taking reference on any head/tail page would
prevent page from being split by anyone.
get_user_pages和follow_page如果在巨页上运行，将返回头或尾页（就像在hugetlbfs上一样）。
大多数gup用户只关心页面的实际物理地址和其临时固定以在I/O完成后释放，因此他们永远不会注意到页面是巨大的。
但是，如果任何驱动程序要在尾页的页面结构上操纵（例如，检查page->mapping或其他与头页相关而不是尾页相关的位），则应该更新以跳转到检查头页。
对任何头/尾页的引用都将防止页面被任何人拆分。

.. note::
   these aren't new constraints to the GUP API, and they match the
   same constrains that applies to hugetlbfs too, so any driver capable
   of handling GUP on hugetlbfs will also work fine on transparent
   hugepage backed mappings.
   这些不是GUP API的新约束，它们与适用于hugetlbfs的相同约束相匹配，因此任何能够处理hugetlbfs上的GUP的驱动程序也可以在透明巨页支持的映射上正常工作。

In case you can't handle compound pages if they're returned by
follow_page, the FOLL_SPLIT bit can be specified as parameter to
follow_page, so that it will split the hugepages before returning
them. Migration for example passes FOLL_SPLIT as parameter to
follow_page because it's not hugepage aware and in fact it can't work
at all on hugetlbfs (but it instead works fine on transparent
hugepages thanks to FOLL_SPLIT). migration simply can't deal with
hugepages being returned (as it's not only checking the pfn of the
page and pinning it during the copy but it pretends to migrate the
memory in regular page sizes and with regular pte/pmd mappings).
在follow_page返回复合页时，如果无法处理复合页，则可以将FOLL_SPLIT位指定为follow_page的参数，以便在返回它们之前拆分巨页。
例如，迁移将FOLL_SPLIT作为参数传递给follow_page，因为它不是巨页感知的，实际上，它根本无法在hugetlbfs上工作（但是，它在透明巨页上工作得很好，感谢FOLL_SPLIT）。
迁移只能处理返回的巨页（因为它不仅检查页面的pfn并在复制期间对其进行固定，而且还假装以常规页面大小和常规pte/pmd映射迁移内存）。

Graceful fallback
小心回退
=================

Code walking pagetables but unaware about huge pmds can simply call
split_huge_pmd(vma, pmd, addr) where the pmd is the one returned by
pmd_offset. It's trivial to make the code transparent hugepage aware
by just grepping for "pmd_offset" and adding split_huge_pmd where
missing after pmd_offset returns the pmd. Thanks to the graceful
fallback design, with a one liner change, you can avoid to write
hundred if not thousand of lines of complex code to make your code
hugepage aware.
代码遍历页表，但不知道巨大的pmd，可以简单地调用split_huge_pmd（vma，pmd，addr），其中pmd是pmd_offset返回的pmd。
通过grep“pmd_offset”并在pmd_offset返回pmd后在缺少的地方添加split_huge_pmd，可以轻松地使代码透明巨页感知。
由于优雅的回退设计，只需一行代码，就可以避免编写数百行甚至数千行复杂的代码，以使代码巨页感知。

If you're not walking pagetables but you run into a physical hugepage
but you can't handle it natively in your code, you can split it by
calling split_huge_page(page). This is what the Linux VM does before
it tries to swapout the hugepage for example. split_huge_page() can fail
if the page is pinned and you must handle this correctly.
如果您没有遍历页表，但遇到了物理巨页，但是您无法在代码中本机处理它，则可以通过调用split_huge_page（page）将其拆分。
这就是Linux VM在尝试交换巨页之前所做的事情。如果页面被固定，则split_huge_page（）可能会失败，您必须正确处理这一点。

Example to make mremap.c transparent hugepage aware with a one liner
示例，使用一行代码使mremap.c透明巨页感知
change::

	diff --git a/mm/mremap.c b/mm/mremap.c
	--- a/mm/mremap.c
	+++ b/mm/mremap.c
	@@ -41,6 +41,7 @@ static pmd_t *get_old_pmd(struct mm_stru
			return NULL;

		pmd = pmd_offset(pud, addr);
	+	split_huge_pmd(vma, pmd, addr);
		if (pmd_none_or_clear_bad(pmd))
			return NULL;

Locking in hugepage aware code
巨页感知代码中的锁定
==============================

We want as much code as possible hugepage aware, as calling
split_huge_page() or split_huge_pmd() has a cost.
我们希望尽可能多的代码巨页感知，因为调用split_huge_page（）或split_huge_pmd（）是有成本的。

To make pagetable walks huge pmd aware, all you need to do is to call
pmd_trans_huge() on the pmd returned by pmd_offset. You must hold the
mmap_sem in read (or write) mode to be sure an huge pmd cannot be
created from under you by khugepaged (khugepaged collapse_huge_page
takes the mmap_sem in write mode in addition to the anon_vma lock). If
pmd_trans_huge returns false, you just fallback in the old code
paths. If instead pmd_trans_huge returns true, you have to take the
page table lock (pmd_lock()) and re-run pmd_trans_huge. Taking the
page table lock will prevent the huge pmd to be converted into a
regular pmd from under you (split_huge_pmd can run in parallel to the
pagetable walk). If the second pmd_trans_huge returns false, you
should just drop the page table lock and fallback to the old code as
before. Otherwise you can proceed to process the huge pmd and the
hugepage natively. Once finished you can drop the page table lock.
如果要使页表遍历支持超大的pmd，你只需要在由 pmd_offset 返回的 pmd 上调用 pmd_trans_huge()。
你必须以读（或写）模式持有 mmap_sem 信号量，以确保 khugepaged 无法在你的下方创建超大的 pmd（khugepaged 的 collapse_huge_page 还需要除 anon_vma lock 以外，
以写模式持有 mmap_sem）。如果 pmd_trans_huge() 返回 false，则回退到旧的代码路径。
如果 pmd_trans_huge() 返回 true，则必须获取页表锁（pmd_lock()）并重新执行 pmd_trans_huge()。
获取页表锁将防止超大的 pmd 在您的下面被转换为普通的 pmd（split_huge_pmd 可以与页表遍历并行执行）。
如果第二个 pmd_trans_huge() 返回 false，则应简单地放弃页表锁并回退到以前的代码路径。否则，您可以继续处理超大的 pmd 和超大页面。完成后，可以放弃页表锁。

Refcounts and transparent huge pages
透明巨页的引用计数
====================================

Refcounting on THP is mostly consistent with refcounting on other compound
引用计数在THP上与其他复合物上的引用计数几乎一致

pages:

  - get_page()/put_page() and GUP operate in head page's ->_refcount.
    get_page()/put_page()和GUP在头页的->_refcount中操作。

  - ->_refcount in tail pages is always zero: get_page_unless_zero() never
    succeed on tail pages.
    ->_refcount在尾页总是为零：get_page_unless_zero()从不在尾页上成功。

  - map/unmap of the pages with PTE entry increment/decrement ->_mapcount
    on relevant sub-page of the compound page.
    使用PTE条目映射/取消映射会增加/减少复合页的相关子页上的->_mapcount。

  - map/unmap of the whole compound page accounted in compound_mapcount
    (stored in first tail page). For file huge pages, we also increment
    ->_mapcount of all sub-pages in order to have race-free detection of
    last unmap of subpages.
    复合页的映射/取消映射在复合映射计数中计算（存储在第一个尾页中）。 对于文件巨页，我们还会增加所有子页的->_mapcount，以便在子页的最后一个取消映射中进行无竞争检测。

PageDoubleMap() indicates that the page is *possibly* mapped with PTEs.
PageDoubleMap()表示该页*可能*使用PTE映射。

For anonymous pages PageDoubleMap() also indicates ->_mapcount in all
subpages is offset up by one. This additional reference is required to
get race-free detection of unmap of subpages when we have them mapped with
both PMDs and PTEs.
对于匿名页，PageDoubleMap()还表示所有子页的->_mapcount都偏移了一个。
当我们使用PMD和PTE映射它们时，这个额外的引用是必需的，以便在子页的取消映射中进行无竞争检测。

This is optimization required to lower overhead of per-subpage mapcount
tracking. The alternative is alter ->_mapcount in all subpages on each
map/unmap of the whole compound page.
这是降低每个子页映射计数跟踪开销所需的优化。 另一种选择是在整个复合页的映射/取消映射时更改所有子页中的->_mapcount。

For anonymous pages, we set PG_double_map when a PMD of the page got split
for the first time, but still have PMD mapping. The additional references
go away with last compound_mapcount.
对于匿名页，当第一次为页的PMD拆分时，但仍然有PMD映射时，我们会设置PG_double_map。
额外的引用随着最后一个复合映射计数而消失。

File pages get PG_double_map set on first map of the page with PTE and
goes away when the page gets evicted from page cache.
文件页在第一次使用PTE映射页时设置PG_double_map，并在从页缓存中驱逐页时消失。

split_huge_page internally has to distribute the refcounts in the head
page to the tail pages before clearing all PG_head/tail bits from the page
structures. It can be done easily for refcounts taken by page table
entries. But we don't have enough information on how to distribute any
additional pins (i.e. from get_user_pages). split_huge_page() fails any
requests to split pinned huge page: it expects page count to be equal to
sum of mapcount of all sub-pages plus one (split_huge_page caller must
have reference for head page).
split_huge_page在清除页结构中的所有PG_head/tail位之前，内部必须将头页中的引用计数分配给尾页。
对于由页表条目获取的引用计数，这可以很容易地完成。
但是，我们没有足够的信息来确定如何分配任何其他引脚（即从get_user_pages获取）。
split_huge_page()拒绝拆分固定的巨大页面的任何请求：它希望页面计数等于所有子页的映射计数之和加一（split_huge_page调用者必须为头页保留引用）。

split_huge_page uses migration entries to stabilize page->_refcount and
page->_mapcount of anonymous pages. File pages just got unmapped.
split_huge_page使用迁移条目来稳定匿名页的页面->_refcount和页面->_mapcount。
文件页只是取消映射。

We safe against physical memory scanners too: the only legitimate way
scanner can get reference to a page is get_page_unless_zero().
我们也可以安全地扫描物理内存：扫描器可以获取页面引用的唯一合法方法是get_page_unless_zero()。

All tail pages have zero ->_refcount until atomic_add(). This prevents the
scanner from getting a reference to the tail page up to that point. After the
atomic_add() we don't care about the ->_refcount value. We already known how
many references should be uncharged from the head page.
直到atomic_add()之前，所有尾页的->_refcount都为零。 这可以防止扫描器在该点之前获取对尾页的引用。
在atomic_add()之后，我们不关心->_refcount值。 我们已经知道应该从头页取消多少引用。

For head page get_page_unless_zero() will succeed and we don't mind. It's
clear where reference should go after split: it will stay on head page.
对于头页，get_page_unless_zero()将成功，我们不介意。 拆分后清楚引用应该去哪里：它将保留在头页上。

Note that split_huge_pmd() doesn't have any limitation on refcounting:
pmd can be split at any point and never fails.
请注意，split_huge_pmd()对引用计数没有任何限制：pmd可以在任何时候拆分，而且永远不会失败。

Partial unmap and deferred_split_huge_page()
并发unmap和deferred_split_huge_page()
============================================

Unmapping part of THP (with munmap() or other way) is not going to free
memory immediately. Instead, we detect that a subpage of THP is not in use
in page_remove_rmap() and queue the THP for splitting if memory pressure
comes. Splitting will free up unused subpages.
取消映射THP的一部分（使用munmap()或其他方式）不会立即释放内存。 相反，我们在page_remove_rmap()中检测到THP的子页未使用，并在内存压力出现时将THP排队进行拆分。
拆分将释放未使用的子页。

Splitting the page right away is not an option due to locking context in
the place where we can detect partial unmap. It's also might be
counterproductive since in many cases partial unmap happens during exit(2) if
a THP crosses a VMA boundary.
由于我们可以检测到部分取消映射的位置中的锁定上下文，因此无法立即拆分页面。 由于在许多情况下，如果THP跨越VMA边界，则在exit(2)期间发生部分取消映射，因此这也可能是不利的。

Function deferred_split_huge_page() is used to queue page for splitting.
The splitting itself will happen when we get memory pressure via shrinker
interface.
函数deferred_split_huge_page()用于为拆分页面排队。 拆分本身将在通过shrinker接口获得内存压力时发生。
