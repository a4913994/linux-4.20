.. _page_frags:

==============
Page fragments
页面片段
==============

A page fragment is an arbitrary-length arbitrary-offset area of memory
which resides within a 0 or higher order compound page.  Multiple
fragments within that page are individually refcounted, in the page's
reference counter.
页面片段是指在一个0或更高阶的复合页面中，占据任意长度和任意偏移量的内存区域。页面中的多个片段会在页面的引用计数器中各自被引用计数。

The page_frag functions, page_frag_alloc and page_frag_free, provide a
simple allocation framework for page fragments.  This is used by the
network stack and network device drivers to provide a backing region of
memory for use as either an sk_buff->head, or to be used in the "frags"
portion of skb_shared_info.
page_frag函数，page_frag_alloc和page_frag_free，提供了一个简单的页面片段分配框架。
这个框架被网络栈和网络设备驱动程序用来提供一个内存区域，用于作为sk_buff->head或者skb_shared_info的“frags”部分。

In order to make use of the page fragment APIs a backing page fragment
cache is needed.  This provides a central point for the fragment allocation
and tracks allows multiple calls to make use of a cached page.  The
advantage to doing this is that multiple calls to get_page can be avoided
which can be expensive at allocation time.  However due to the nature of
this caching it is required that any calls to the cache be protected by
either a per-cpu limitation, or a per-cpu limitation and forcing interrupts
to be disabled when executing the fragment allocation.
为了使用页面片段API，需要一个页面片段缓存。这个缓存提供了一个中心点，用于分配片段并允许多次调用使用缓存页面。
这样做的好处是可以避免多次调用get_page，这在分配时可能会很昂贵。但是由于这种缓存的性质，需要保护任何对缓存的调用，
要么通过每个CPU的限制，要么通过每个CPU的限制并在执行片段分配时强制禁用中断。


The network stack uses two separate caches per CPU to handle fragment
allocation.  The netdev_alloc_cache is used by callers making use of the
__netdev_alloc_frag and __netdev_alloc_skb calls.  The napi_alloc_cache is
used by callers of the __napi_alloc_frag and __napi_alloc_skb calls.  The
main difference between these two calls is the context in which they may be
called.  The "netdev" prefixed functions are usable in any context as these
functions will disable interrupts, while the "napi" prefixed functions are
only usable within the softirq context.
网络栈在每个CPU上使用两个单独的缓存来处理片段分配。netdev_alloc_cache由使用__netdev_alloc_frag和__netdev_alloc_skb调用的调用者使用。
napi_alloc_cache由使用__napi_alloc_frag和__napi_alloc_skb调用的调用者使用。这两个调用之间的主要区别在于它们可能被调用的上下文。
以“netdev”为前缀的函数可在任何上下文中使用，因为这些函数将禁用中断，而以“napi”为前缀的函数仅可在软中断上下文中使用。

Many network device drivers use a similar methodology for allocating page
fragments, but the page fragments are cached at the ring or descriptor
level.  In order to enable these cases it is necessary to provide a generic
way of tearing down a page cache.  For this reason __page_frag_cache_drain
was implemented.  It allows for freeing multiple references from a single
page via a single call.  The advantage to doing this is that it allows for
cleaning up the multiple references that were added to a page in order to
avoid calling get_page per allocation.
许多网络设备驱动程序使用类似的方法来分配页面片段，但是页面片段是在环或描述符级别缓存的。
为了启用这些情况，需要提供一种通用的方法来清理页面缓存。因此，实现了__page_frag_cache_drain。
它允许通过单个调用从单个页面释放多个引用。这样做的好处是它允许清理添加到页面中的多个引用，
以避免每次分配调用get_page。

Alexander Duyck, Nov 29, 2016.
