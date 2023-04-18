.. _numa:

Started Nov 1999 by Kanoj Sarcar <kanoj@sgi.com>

=============
What is NUMA?
什么是NUMA？
=============

This question can be answered from a couple of perspectives:  the
hardware view and the Linux software view.
这个问题可以从几个角度来回答：硬件视图和Linux软件视图。

From the hardware perspective, a NUMA system is a computer platform that
comprises multiple components or assemblies each of which may contain 0
or more CPUs, local memory, and/or IO buses.  For brevity and to
disambiguate the hardware view of these physical components/assemblies
from the software abstraction thereof, we'll call the components/assemblies
'cells' in this document.
从硬件的角度来看，NUMA系统是一个计算机平台，它包含多个组件或装配体，每个组件或装配体都可以包含0个或多个CPU、本地内存和/或IO总线。
为了简洁性和区分硬件视图中的这些物理组件/装配体与软件抽象，我们在本文档中将这些组件/装配体称为“单元”。

Each of the 'cells' may be viewed as an SMP [symmetric multi-processor] subset
of the system--although some components necessary for a stand-alone SMP system
may not be populated on any given cell.   The cells of the NUMA system are
connected together with some sort of system interconnect--e.g., a crossbar or
point-to-point link are common types of NUMA system interconnects.  Both of
these types of interconnects can be aggregated to create NUMA platforms with
cells at multiple distances from other cells.
每个“单元”都可以被视为系统的SMP [对称多处理器]子集，尽管某些对于独立的SMP系统必需的组件可能不会被填充到任何给定的单元上。
NUMA系统的单元通过某种系统互联连接在一起——例如，交叉栏或点对点链接是NUMA系统互联的常见类型。
这两种类型的互联可以聚合在一起，以创建具有多个距离的单元的NUMA平台。

For Linux, the NUMA platforms of interest are primarily what is known as Cache
Coherent NUMA or ccNUMA systems.   With ccNUMA systems, all memory is visible
to and accessible from any CPU attached to any cell and cache coherency
is handled in hardware by the processor caches and/or the system interconnect.
对于Linux，感兴趣的NUMA平台主要是所谓的高速缓存一致NUMA或ccNUMA系统。
在ccNUMA系统中，所有内存对于任何附加到任何单元的CPU都是可见的和可访问的，并且缓存一致性由处理器缓存和/或系统互联处理器硬件处理。

Memory access time and effective memory bandwidth varies depending on how far
away the cell containing the CPU or IO bus making the memory access is from the
cell containing the target memory.  For example, access to memory by CPUs
attached to the same cell will experience faster access times and higher
bandwidths than accesses to memory on other, remote cells.  NUMA platforms
can have cells at multiple remote distances from any given cell.
内存访问时间和有效内存带宽取决于CPU或IO总线进行内存访问的单元与包含目标内存的单元之间的距离。
例如，附加到同一单元的CPU对内存的访问将比访问其他远程单元上的内存具有更快的访问时间和更高的带宽。
NUMA平台可以在任何给定单元的多个远程距离上具有单元。

Platform vendors don't build NUMA systems just to make software developers'
lives interesting.  Rather, this architecture is a means to provide scalable
memory bandwidth.  However, to achieve scalable memory bandwidth, system and
application software must arrange for a large majority of the memory references
[cache misses] to be to "local" memory--memory on the same cell, if any--or
to the closest cell with memory.
平台供应商不仅仅是为了使软件开发人员的生活更有趣而构建NUMA系统。
相反，这种体系结构是提供可扩展内存带宽的一种手段。
但是，为了实现可扩展的内存带宽，系统和应用程序软件必须安排大多数内存引用[缓存未命中]为“本地”内存——如果有的话，同一单元上的内存或最近的带内存的单元。

This leads to the Linux software view of a NUMA system:
这就引出了Linux软件对NUMA系统的看法：

Linux divides the system's hardware resources into multiple software
abstractions called "nodes".  Linux maps the nodes onto the physical cells
of the hardware platform, abstracting away some of the details for some
architectures.  As with physical cells, software nodes may contain 0 or more
CPUs, memory and/or IO buses.  And, again, memory accesses to memory on
"closer" nodes--nodes that map to closer cells--will generally experience
faster access times and higher effective bandwidth than accesses to more
remote cells.
Linux将系统的硬件资源分成多个称为“节点”的软件抽象。 Linux将节点映射到硬件平台的物理单元上，为某些体系结构抽象了一些细节。
与物理单元一样，软件节点可以包含0个或多个CPU、内存和/或IO总线。
而且，同样，对“更近”节点上的内存的内存访问——映射到更近单元的节点——通常会比访问更远的单元的内存访问具有更快的访问时间和更高的有效带宽。

For some architectures, such as x86, Linux will "hide" any node representing a
physical cell that has no memory attached, and reassign any CPUs attached to
that cell to a node representing a cell that does have memory.  Thus, on
these architectures, one cannot assume that all CPUs that Linux associates with
a given node will see the same local memory access times and bandwidth.
对于某些体系结构，例如x86，Linux将“隐藏”任何没有附加内存的物理单元表示的节点，并重新分配任何附加到该单元的CPU到表示具有内存的单元的节点。
因此，在这些体系结构上，不能假定Linux与给定节点关联的所有CPU都会看到相同的本地内存访问时间和带宽。

In addition, for some architectures, again x86 is an example, Linux supports
the emulation of additional nodes.  For NUMA emulation, linux will carve up
the existing nodes--or the system memory for non-NUMA platforms--into multiple
nodes.  Each emulated node will manage a fraction of the underlying cells'
physical memory.  NUMA emluation is useful for testing NUMA kernel and
application features on non-NUMA platforms, and as a sort of memory resource
management mechanism when used together with cpusets.
[see Documentation/cgroup-v1/cpusets.txt]
此外，对于某些体系结构，例如x86，Linux还支持额外节点的模拟。 对于NUMA模拟，linux将现有节点——或非NUMA平台的系统内存——划分为多个节点。
每个模拟节点将管理底层单元的物理内存的一部分。 NUMA emluation对于在非NUMA平台上测试NUMA内核和应用程序功能很有用，
并且当与cpusets一起使用时，它可以作为一种内存资源管理机制。 [参见Documentation/cgroup-v1/cpusets.txt]

For each node with memory, Linux constructs an independent memory management
subsystem, complete with its own free page lists, in-use page lists, usage
statistics and locks to mediate access.  In addition, Linux constructs for
each memory zone [one or more of DMA, DMA32, NORMAL, HIGH_MEMORY, MOVABLE],
an ordered "zonelist".  A zonelist specifies the zones/nodes to visit when a
selected zone/node cannot satisfy the allocation request.  This situation,
when a zone has no available memory to satisfy a request, is called
"overflow" or "fallback".
对于每个具有内存的节点，Linux构造一个独立的内存管理子系统，包括自己的空闲页面列表、使用中的页面列表、使用统计信息和锁来协调访问。
此外，Linux为每个内存区[DMA、DMA32、NORMAL、HIGH_MEMORY、MOVABLE之一或多个]构造一个有序的“zonelist”。
zonelist指定在无法满足分配请求时访问的区域/节点。 当区域没有可用内存来满足请求时，这种情况称为“溢出”或“回退”。

Because some nodes contain multiple zones containing different types of
memory, Linux must decide whether to order the zonelists such that allocations
fall back to the same zone type on a different node, or to a different zone
type on the same node.  This is an important consideration because some zones,
such as DMA or DMA32, represent relatively scarce resources.  Linux chooses
a default Node ordered zonelist. This means it tries to fallback to other zones
from the same node before using remote nodes which are ordered by NUMA distance.
因为一些节点包含多个包含不同类型内存的区域，Linux必须决定是否对zonelists进行排序，以便分配回退到不同节点上的相同区域类型，
或者在同一节点上的不同区域类型。 这是一个重要的考虑因素，因为一些区域，例如DMA或DMA32，代表相对稀缺的资源。
Linux选择默认的Node有序zonelist。 这意味着它尝试在使用按NUMA距离排序的远程节点之前从同一节点回退到其他区域。

By default, Linux will attempt to satisfy memory allocation requests from the
node to which the CPU that executes the request is assigned.  Specifically,
Linux will attempt to allocate from the first node in the appropriate zonelist
for the node where the request originates.  This is called "local allocation."
If the "local" node cannot satisfy the request, the kernel will examine other
nodes' zones in the selected zonelist looking for the first zone in the list
that can satisfy the request.
默认情况下，Linux将尝试从执行请求的CPU分配的节点满足内存分配请求。 具体来说，Linux将尝试从请求源节点的适当zonelist中的第一个节点分配。
这称为“本地分配”。 如果“本地”节点无法满足请求，内核将检查所选zonelist中的其他节点的区域，寻找可以满足请求的列表中的第一个区域。

Local allocation will tend to keep subsequent access to the allocated memory
"local" to the underlying physical resources and off the system interconnect--
as long as the task on whose behalf the kernel allocated some memory does not
later migrate away from that memory.  The Linux scheduler is aware of the
NUMA topology of the platform--embodied in the "scheduling domains" data
structures [see Documentation/scheduler/sched-domains.txt]--and the scheduler
attempts to minimize task migration to distant scheduling domains.  However,
the scheduler does not take a task's NUMA footprint into account directly.
Thus, under sufficient imbalance, tasks can migrate between nodes, remote
from their initial node and kernel data structures.
本地分配将倾向于保持后续对分配的内存的访问“本地”到底层物理资源，并且不会影响系统互连——只要内核代表内核分配了一些内存的任务不会后来迁移到该内存。
Linux调度程序知道平台的NUMA拓扑结构——体现在“调度域”数据结构中[参见Documentation/scheduler/sched-domains.txt]——调度程序尝试最小化任务迁移到远程调度域。
但是，调度程序不直接考虑任务的NUMA足迹。 因此，在足够的不平衡下，任务可以在节点之间迁移，远离它们的初始节点和内核数据结构。

System administrators and application designers can restrict a task's migration
to improve NUMA locality using various CPU affinity command line interfaces,
such as taskset(1) and numactl(1), and program interfaces such as
sched_setaffinity(2).  Further, one can modify the kernel's default local
allocation behavior using Linux NUMA memory policy.
[see Documentation/admin-guide/mm/numa_memory_policy.rst.]
系统管理员和应用程序设计人员可以使用各种CPU亲和性命令行界面（如taskset(1)和numactl(1)）和程序接口（如sched_setaffinity(2)）来限制任务的迁移，
以改善NUMA局部性。 此外，可以使用Linux NUMA内存策略修改内核的默认本地分配行为。 [参见Documentation/admin-guide/mm/numa_memory_policy.rst。]

System administrators can restrict the CPUs and nodes' memories that a non-
privileged user can specify in the scheduling or NUMA commands and functions
using control groups and CPUsets.  [see Documentation/cgroup-v1/cpusets.txt]
系统管理员可以使用控制组和CPUsets在调度或NUMA命令和功能中限制非特权用户可以指定的CPU和节点的内存。
[参见Documentation/cgroup-v1/cpusets.txt]

On architectures that do not hide memoryless nodes, Linux will include only
zones [nodes] with memory in the zonelists.  This means that for a memoryless
node the "local memory node"--the node of the first zone in CPU's node's
zonelist--will not be the node itself.  Rather, it will be the node that the
kernel selected as the nearest node with memory when it built the zonelists.
So, default, local allocations will succeed with the kernel supplying the
closest available memory.  This is a consequence of the same mechanism that
allows such allocations to fallback to other nearby nodes when a node that
does contain memory overflows.
在不隐藏内存的节点的架构上，Linux将仅包含zonelists中的区域[节点]。 这意味着对于一个没有内存的节点，“本地内存节点”——CPU节点的zonelist中第一个区域的节点——不会是节点本身。
相反，它将是内核在构建zonelists时选择的最近的具有内存的节点。 因此，默认的本地分配将成功，内核将提供最近可用的内存。
这是允许这种分配回退到其他附近节点的同一机制的结果，当包含内存的节点溢出时。

Some kernel allocations do not want or cannot tolerate this allocation fallback
behavior.  Rather they want to be sure they get memory from the specified node
or get notified that the node has no free memory.  This is usually the case when
a subsystem allocates per CPU memory resources, for example.
一些内核分配不想或不能容忍这种分配回退行为。 相反，他们想确保他们从指定的节点获取内存，或者得到通知该节点没有可用内存。
当子系统分配每个CPU的内存资源时，通常就是这种情况。 例如。

A typical model for making such an allocation is to obtain the node id of the
node to which the "current CPU" is attached using one of the kernel's
numa_node_id() or CPU_to_node() functions and then request memory from only
the node id returned.  When such an allocation fails, the requesting subsystem
may revert to its own fallback path.  The slab kernel memory allocator is an
example of this.  Or, the subsystem may choose to disable or not to enable
itself on allocation failure.  The kernel profiling subsystem is an example of
this.
用于进行此类分配的典型模型是使用内核的numa_node_id()或CPU_to_node()函数之一获取“当前CPU”附加的节点的节点id，
然后仅从返回的节点id请求内存。 当这样的分配失败时，请求子系统可能会恢复到自己的回退路径。
slab内核内存分配器就是这样的例子。 或者，子系统可以选择在分配失败时禁用或不启用自身。
内核分析子系统就是这样的例子。

If the architecture supports--does not hide--memoryless nodes, then CPUs
attached to memoryless nodes would always incur the fallback path overhead
or some subsystems would fail to initialize if they attempted to allocated
memory exclusively from a node without memory.  To support such
architectures transparently, kernel subsystems can use the numa_mem_id()
or cpu_to_mem() function to locate the "local memory node" for the calling or
specified CPU.  Again, this is the same node from which default, local page
allocations will be attempted.
如果架构支持——不隐藏——没有内存的节点，那么附加到没有内存的节点的CPU将始终承担回退路径开销，
或者如果它们尝试仅从没有内存的节点分配内存，则某些子系统将无法初始化。
为了透明地支持这些架构，内核子系统可以使用numa_mem_id()或cpu_to_mem()函数来定位调用或指定CPU的“本地内存节点”。
