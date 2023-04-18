.. _overcommit_accounting:

=====================
Overcommit Accounting
过度承诺计算（Overcommit Accounting）
=====================

The Linux kernel supports the following overcommit handling modes
Linux Kernel支持以下过度承诺处理模式
0
	Heuristic overcommit handling. Obvious overcommits of address
	space are refused. Used for a typical system. It ensures a
	seriously wild allocation fails while allowing overcommit to
	reduce swap usage.  root is allowed to allocate slightly more
	memory in this mode. This is the default.
	启发式的过度承诺处理。显然超额承诺的地址空间是被拒绝的，这适用于典型的系统。它可以确保对于非常大的内存分配请求会失败，
	同时允许过度承诺来减少交换空间的使用。在这种模式下，root用户被允许稍微多分配一些内存。这是系统的默认设置。	

1
	Always overcommit. Appropriate for some scientific
	applications. Classic example is code using sparse arrays and
	just relying on the virtual memory consisting almost entirely
	of zero pages.
	总是过度承诺。适用于一些科学应用。经典的例子是使用稀疏数组的代码，只依赖于虚拟内存几乎全部由零页组成。

2
	Don't overcommit. The total address space commit for the
	system is not permitted to exceed swap + a configurable amount
	(default is 50%) of physical RAM.  Depending on the amount you
	use, in most situations this means a process will not be
	killed while accessing pages but will receive errors on memory
	allocation as appropriate.

	Useful for applications that want to guarantee their memory
	allocations will be available in the future without having to
	initialize every page.
	不要过度承诺。系统的总地址空间承诺不允许超过交换空间加物理内存的可配置数量（默认为50%）。
	根据你所使用的数量，在大多数情况下，这意味着访问页面时进程不会被杀死，但在内存分配时会适当地收到错误。
	这对于希望保证未来的内存分配可用性而无需初始化每个页面的应用程序非常有用。

The overcommit policy is set via the sysctl ``vm.overcommit_memory``.

The overcommit amount can be set via ``vm.overcommit_ratio`` (percentage)
or ``vm.overcommit_kbytes`` (absolute value).

The current overcommit limit and amount committed are viewable in
``/proc/meminfo`` as CommitLimit and Committed_AS respectively.

Gotchas
陷阱
=======

The C language stack growth does an implicit mremap. If you want absolute
guarantees and run close to the edge you MUST mmap your stack for the
largest size you think you will need. For typical stack usage this does
not matter much but it's a corner case if you really really care
C语言的堆栈增长执行了隐式的mremap。如果你想要绝对的保证并且运行在边缘上，你必须为你认为你需要的最大大小来mmap你的堆栈。
对于典型的堆栈使用，这并不重要，但如果你真的真的关心，这是一个角落的情况。 

In mode 2 the MAP_NORESERVE flag is ignored.
在模式2中，MAP_NORESERVE标志被忽略。


How It Works
============

The overcommit is based on the following rules

For a file backed map
	| SHARED or READ-only	-	0 cost (the file is the map not swap)
	| PRIVATE WRITABLE	-	size of mapping per instance

For an anonymous or ``/dev/zero`` map
	| SHARED			-	size of mapping
	| PRIVATE READ-only	-	0 cost (but of little use)
	| PRIVATE WRITABLE	-	size of mapping per instance

Additional accounting
	| Pages made writable copies by mmap
	| shmfs memory drawn from the same pool

Status
======

*	We account mmap memory mappings
*	We account mprotect changes in commit
*	We account mremap changes in size
*	We account brk
*	We account munmap
*	We report the commit status in /proc
*	Account and check on fork
*	Review stack handling/building on exec
*	SHMfs accounting
*	Implement actual limit enforcement

To Do
=====
*	Account ptrace pages (this is hard)
