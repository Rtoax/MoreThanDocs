<div align=center>
	<img src="_v_images/20200910110325796_584.png" width="600"> 
</div>
<br/>
<br/>

<center><font size='6'>DPDK笔记 DPDK 跟踪库tracepoint源码实例分析</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2021年4月</font></center>
<br/>
<br/>
<br/>
<br/>

> 注意：
> 1. [跟踪库](https://doc.dpdk.org/guides/prog_guide/trace_lib.html)
> 2. 基于DPDK 20.05
> 3. [DPDK跟踪库：trace library](https://rtoax.blog.csdn.net/article/details/108716237)

# 1. trace流程源码分析

## 1.1. rte_eal_trace_thread_remote_launch

在源代码中有`rte_eal_remote_launch`函数如下：

```c
int
rte_eal_remote_launch(int (*f)(void *), void *arg, unsigned slave_id)
{
	...
	rte_eal_trace_thread_remote_launch(f, arg, slave_id, rc);
	...
}
```

在文件`lib\librte_eal\common\eal_common_trace_points.c`中有相关的初始化流程：

```c
...
RTE_TRACE_POINT_DEFINE(rte_eal_trace_thread_remote_launch);
...

RTE_INIT(eal_trace_init)
{
    ...
	RTE_TRACE_POINT_REGISTER(rte_eal_trace_thread_remote_launch,
		lib.eal.thread.remote.launch);
	...
}
```
函数`eal_trace_init`将在初始化eal过程中被执行，以挂载tracepoint。对于`RTE_TRACE_POINT_DEFINE`的定义，如下：

```c
/** Macro to define the tracepoint. */
#define RTE_TRACE_POINT_DEFINE(tp) \
rte_trace_point_t __attribute__((section("__rte_trace_point"))) __##tp
```
那么`RTE_TRACE_POINT_DEFINE(rte_eal_trace_thread_remote_launch);`将被展开为：

```c
rte_trace_point_t __attribute__((section("__rte_trace_point"))) __rte_eal_trace_thread_remote_launch;
```
其中`rte_trace_point_t`为：
```c
/** The tracepoint object. */
typedef uint64_t rte_trace_point_t;
```
进而：
```c
uint64_t __attribute__((section("__rte_trace_point"))) __rte_eal_trace_thread_remote_launch;
```
然后，在`eal_trace_init`中的`RTE_TRACE_POINT_REGISTER`定义如下：

```c
#define RTE_TRACE_POINT_REGISTER(trace, name) \
	__rte_trace_point_register(&__##trace, RTE_STR(name), \
		(void (*)(void)) trace)
```
将其展开为：

```c
uint64_t __attribute__((section("__rte_trace_point"))) __rte_eal_trace_thread_remote_launch;

RTE_INIT(eal_trace_init)
{
    ...
    __rte_trace_point_register(&__rte_eal_trace_thread_remote_launch, \
                    "lib.eal.thread.remote.launch", \
		            (void (*)(void)) rte_eal_trace_thread_remote_launch);
    ...
}
```
显然，在`rte_eal_remote_launch`被调用的`rte_eal_trace_thread_remote_launch`是一个函数，那么，它是在哪里定义的呢？在文件`lib\librte_eal\include\rte_eal_trace.h`中有如下定义：
```c
/* Thread */
RTE_TRACE_POINT(
	rte_eal_trace_thread_remote_launch,
	RTE_TRACE_POINT_ARGS(int (*f)(void *), void *arg,
		unsigned int slave_id, int rc),
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_ptr(arg);
	rte_trace_point_emit_u32(slave_id);
	rte_trace_point_emit_int(rc);
)
```
这里的`RTE_TRACE_POINT`为`__RTE_TRACE_POINT`的封装，给出它们：

```c
#define RTE_TRACE_POINT(tp, args, ...) \
	__RTE_TRACE_POINT(generic, tp, args, __VA_ARGS__)
	
#define RTE_TRACE_POINT_ARGS

/** @internal Helper macro to support RTE_TRACE_POINT and RTE_TRACE_POINT_FP */
#define __RTE_TRACE_POINT(_mode, _tp, _args, ...) \
extern rte_trace_point_t __##_tp; \
static __rte_always_inline void \
_tp _args \
{ \
	__rte_trace_point_emit_header_##_mode(&__##_tp); \
	__VA_ARGS__ \
}
```

## 1.2. __rte_trace_point_emit_header_generic

所以`lib\librte_eal\include\rte_eal_trace.h`中的`RTE_TRACE_POINT`可以展开为：

```c
extern uint64_t __rte_eal_trace_thread_remote_launch;

static __rte_always_inline void rte_eal_trace_thread_remote_launch(int (*f)(void *), void *arg, unsigned int slave_id, int rc) {
    __rte_trace_point_emit_header_generic(&__rte_eal_trace_thread_remote_launch);
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_ptr(arg);
	rte_trace_point_emit_u32(slave_id);
	rte_trace_point_emit_int(rc);
}
```
在tracepoint创建过程中，会区分快速路径和慢速路径，这里先不做出解释说明。下面给出`generic`的宏定义已经相关的位：

```c
#define __RTE_TRACE_FIELD_SIZE_SHIFT 0
#define __RTE_TRACE_FIELD_SIZE_MASK (0xffffULL << __RTE_TRACE_FIELD_SIZE_SHIFT)
#define __RTE_TRACE_FIELD_ID_SHIFT (16)
#define __RTE_TRACE_FIELD_ID_MASK (0xffffULL << __RTE_TRACE_FIELD_ID_SHIFT)
#define __RTE_TRACE_FIELD_ENABLE_MASK (1ULL << 63)
#define __RTE_TRACE_FIELD_ENABLE_DISCARD (1ULL << 62)

#define __rte_trace_point_emit_header_generic(t) \
void *mem; \
do { \
	const uint64_t val = __atomic_load_n(t, __ATOMIC_ACQUIRE); \
	if (likely(!(val & __RTE_TRACE_FIELD_ENABLE_MASK))) \
		return; \
	mem = __rte_trace_mem_get(val); \
	if (unlikely(mem == NULL)) \
		return; \
	mem = __rte_trace_point_emit_ev_header(mem, val); \
} while (0)
```

这里对原子操作给出解释：

* `__ATOMIC_RELAXED`    //编译器和处理器可以对memory access做任何的reorder
* `__ATOMIC_ACQUIRE`    //保证本线程中,所有后续的读操作必须在本条原子操作完成后执行。
* `__ATOMIC_RELEASE`    //保证本线程中,所有之前的写操作完成后才能执行本条原子操作。
* `__ATOMIC_ACQ_REL`    //同时包含 memory_order_acquire 和 memory_order_release
* `__ATOMIC_CONSUME`    //释放消费顺序（release/consume）的规范正在修订中，而且暂时不鼓励使用 
* `__ATOMIC_SEQ_CST`    //最强的同步模式，同时也是默认的模型，具有强烈的happens-before语义

`__rte_trace_point_emit_header_generic`分为以下几步：

1. 原子load获取变量，改变变量是`uint64_t`类型；
2. 判断变量的`__RTE_TRACE_FIELD_ENABLE_MASK`(63)使能标志位，如果没有置位，直接返回；
3. 调用`__rte_trace_mem_get`获取trace mem，如果mem为空，返回；如果不为空，继续执行；
4. 执行`__rte_trace_point_emit_ev_header`；

### 1.2.1. __rte_trace_mem_get

首先，分析`__rte_trace_mem_get`函数，首先直接给出函数的定义：

```c
static __rte_always_inline void *
__rte_trace_mem_get(uint64_t in)
{
	struct __rte_trace_header *trace = RTE_PER_LCORE(trace_mem);
	const uint16_t sz = in & __RTE_TRACE_FIELD_SIZE_MASK;

	/* Trace memory is not initialized for this thread */
	if (unlikely(trace == NULL)) {
		__rte_trace_mem_per_thread_alloc();
		trace = RTE_PER_LCORE(trace_mem);
		if (unlikely(trace == NULL))
			return NULL;
	}
	/* Check the wrap around case */
	uint32_t offset = trace->offset;
	if (unlikely((offset + sz) >= trace->len)) {
		/* Disable the trace event if it in DISCARD mode */
		if (unlikely(in & __RTE_TRACE_FIELD_ENABLE_DISCARD))
			return NULL;

		offset = 0;
	}
	/* Align to event header size */
	offset = RTE_ALIGN_CEIL(offset, __RTE_TRACE_EVENT_HEADER_SZ);
	void *mem = RTE_PTR_ADD(&trace->mem[0], offset);
	offset += sz;
	trace->offset = offset;

	return mem;
}
```


> `PRE_LCORE` 变量：
> 为每个线程提供单独的变量的副本，消除同步锁对性能的影响，参见 `gcc __thread`;


首先，在文件`rte_trace_point.h`中有per-lcore变量`RTE_DECLARE_PER_LCORE(void *, trace_mem);`，这个宏定义是这样的：
```c
#define RTE_DECLARE_PER_LCORE(type, name)			\
	extern __thread __typeof__(type) per_lcore_##name
```
也就是说它是这样：

```c
extern __thread void * per_lcore_trace_mem;
```
请注意，初始阶段`per_lcore_trace_mem`为`NULL`.

在`__rte_trace_mem_get`第一次执行时，`trace`为空，这时候会走下面的unlikely分支：

```c
static __rte_always_inline void *
__rte_trace_mem_get(uint64_t in)
{
	struct __rte_trace_header *trace = RTE_PER_LCORE(trace_mem);
	const uint16_t sz = in & __RTE_TRACE_FIELD_SIZE_MASK;

	/* Trace memory is not initialized for this thread */
	if (unlikely(trace == NULL)) {
		__rte_trace_mem_per_thread_alloc();
		trace = RTE_PER_LCORE(trace_mem);
		if (unlikely(trace == NULL))
			return NULL;
	}
	...
}
```
函数`__rte_trace_mem_per_thread_alloc`将为trace点分配响应需要的内存。

#### 1.2.1.1. __rte_trace_mem_per_thread_alloc

首先，看下数据结构`struct trace`：
```c
struct trace {
	char dir[PATH_MAX];
	int dir_offset;
	int register_errno;
	bool status;
	enum rte_trace_mode mode;
	rte_uuid_t uuid;
	uint32_t buff_len;
	STAILQ_HEAD(, trace_arg) args;
	uint32_t nb_trace_points;
	uint32_t nb_trace_mem_list;
	struct thread_mem_meta *lcore_meta;
	uint64_t epoch_sec;
	uint64_t epoch_nsec;
	uint64_t uptime_ticks;
	char *ctf_meta;
	uint32_t ctf_meta_offset_freq;
	uint32_t ctf_meta_offset_freq_off_s;
	uint32_t ctf_meta_offset_freq_off;
	uint16_t ctf_fixup_done;
	rte_spinlock_t lock;
};
```
有个`trace`静态变量在文件`eal_common_trace.c`中，并提供了相应的获取接口`trace_obj_get`：

```c
static struct trace trace = { .args = STAILQ_HEAD_INITIALIZER(trace.args), };

struct trace *
trace_obj_get(void)
{
	return &trace;
}
```
函数`__rte_trace_mem_per_thread_alloc`在起始处获取这个全局变量的地址，然后检测trace使能开关，接着，如果`RTE_PER_LCORE(trace_mem)`不为空，直接返回。然后就要进入内存分配阶段了，首先使用自旋锁进行临界区保护，从trace获取`count = trace->nb_trace_mem_list`的大小，并`realloc`多一个`lcore_meta`，这个结构很简单：
```c
enum trace_area_e {
	TRACE_AREA_HEAP,
	TRACE_AREA_HUGEPAGE,
};

struct thread_mem_meta {
	void *mem;
	enum trace_area_e area;
};
```
`area`表明`mem`指向的内存，要么是堆上的内存，要么是大页内存上的，它将指向`struct __rte_trace_header`结构，这在接下来的代码可以看出。首先尝试从大页内存中获取（具体大页内存的操作这里不做讨论）：

```c
	/* First attempt from huge page */
	header = eal_malloc_no_trace(NULL, trace_mem_sz(trace->buff_len), 8);
	if (header) {
		trace->lcore_meta[count].area = TRACE_AREA_HUGEPAGE;
		goto found;
	}
```
如果失败，尝试从堆上申请内存：

```c
	/* Second attempt from heap */
	header = malloc(trace_mem_sz(trace->buff_len));
	if (header == NULL) {
		trace_crit("trace mem malloc attempt failed");
		header = NULL;
		goto fail;
	}
```
其中header结构如下：

```c
struct __rte_trace_stream_header {
	uint32_t magic;
	rte_uuid_t uuid;
	uint32_t lcore_id;
	char thread_name[__RTE_TRACE_EMIT_STRING_LEN_MAX];
} __rte_packed;

struct __rte_trace_header {
	uint32_t offset;
	uint32_t len;
	struct __rte_trace_stream_header stream_header;
	uint8_t mem[];
};
```
默认的申请长度为`1MB`：

```c
void
trace_bufsz_args_apply(void)
{
	struct trace *trace = trace_obj_get();

	if (trace->buff_len == 0)
		trace->buff_len = 1024 * 1024; /* 1MB */
}
```
接下来就是保存相关的信息了：
```c
	/* Initialize the trace header */
found:
	header->offset = 0;
	header->len = trace->buff_len;
	header->stream_header.magic = TRACE_CTF_MAGIC;
	rte_uuid_copy(header->stream_header.uuid, trace->uuid);
	header->stream_header.lcore_id = rte_lcore_id();

	/* Store the thread name */
	char *name = header->stream_header.thread_name;
	memset(name, 0, __RTE_TRACE_EMIT_STRING_LEN_MAX);
	rte_thread_getname(pthread_self(), name,
		__RTE_TRACE_EMIT_STRING_LEN_MAX);

	trace->lcore_meta[count].mem = header;
```

最后，对计数器`trace->nb_trace_mem_list++`递增，然后`RTE_PER_LCORE(trace_mem) = header`，至此`__rte_trace_mem_per_thread_alloc`运行结束并返回。


接下来我们回到`__rte_trace_mem_get`函数，接下来这块的代码说明，如果trace数量过多，将会覆盖之前的trace：

```c
	/* Check the wrap around case */
	uint32_t offset = trace->offset;
	if (unlikely((offset + sz) >= trace->len)) {
		/* Disable the trace event if it in DISCARD mode */
		if (unlikely(in & __RTE_TRACE_FIELD_ENABLE_DISCARD))
			return NULL;

		offset = 0;
	}
```
最终，`__rte_trace_mem_get`函数将返回这块指定的内存区域。

### 1.2.2. __rte_trace_point_emit_ev_header

接下来将回到`__rte_trace_point_emit_header_generic`宏，并执行`__rte_trace_point_emit_ev_header`函数，这个函数比较短小精悍：

```c
static __rte_always_inline void *
__rte_trace_point_emit_ev_header(void *mem, uint64_t in)
{
	uint64_t val;

	/* Event header [63:0] = id [63:48] | timestamp [47:0] */
	val = rte_get_tsc_cycles() &
		~(0xffffULL << __RTE_TRACE_EVENT_HEADER_ID_SHIFT);
	val |= ((in & __RTE_TRACE_FIELD_ID_MASK) <<
		(__RTE_TRACE_EVENT_HEADER_ID_SHIFT -
		 __RTE_TRACE_FIELD_ID_SHIFT));

	*(uint64_t *)mem = val;
	return RTE_PTR_ADD(mem, __RTE_TRACE_EVENT_HEADER_SZ);
}
```
首先先直接用内嵌汇编获取时间戳：
```c
static inline uint64_t
rte_rdtsc(void)
{
	union {
		uint64_t tsc_64;
		RTE_STD_C11
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;

#ifdef RTE_LIBRTE_EAL_VMWARE_TSC_MAP_SUPPORT
	if (unlikely(rte_cycles_vmware_tsc_map)) {
		/* ecx = 0x10000 corresponds to the physical TSC for VMware */
		asm volatile("rdpmc" :
		             "=a" (tsc.lo_32),
		             "=d" (tsc.hi_32) :
		             "c"(0x10000));
		return tsc.tsc_64;
	}
#endif

	asm volatile("rdtsc" :
		     "=a" (tsc.lo_32),
		     "=d" (tsc.hi_32));
	return tsc.tsc_64;
}
static inline uint64_t
rte_get_tsc_cycles(void) { return rte_rdtsc(); }
```

> **TSC简介**
> 一个用于时间戳计数器的64位的寄存器，它在每个时钟信号(CLK, CLK是微处理器中一条用于接收外部振荡器的时钟信号输入引线）到来时加一。
> 通过它可以计算CPU的主频，比如：如果微处理器的主频是1MHZ的话，那么TSC就会在1秒内增加1000000。除了计算CPU的主频外，还可以通过TSC来测试微处理器其他处理单元的运算速度。

这里`__RTE_TRACE_EVENT_HEADER_ID_SHIFT`为48，经过推算，
```c
	/* Event header [63:0] = id [63:48] | timestamp [47:0] */
	val = rte_get_tsc_cycles() &
		~(0xffffULL << __RTE_TRACE_EVENT_HEADER_ID_SHIFT);
```
将只保留tsc寄存器的低48bits作为时间戳，高12位干嘛用呢，接下来的代码做了说明：

```c
	val |= ((in & __RTE_TRACE_FIELD_ID_MASK) <<
		(__RTE_TRACE_EVENT_HEADER_ID_SHIFT -
		 __RTE_TRACE_FIELD_ID_SHIFT));
```
这将变量`in`的`[16:32]`拷贝到`val`的`[48:63]`位上，这些位置存放了一些标志位，比如上文中提到的**使能信息标志位**。

接着，将这个无符号长整型保存，并返回他的下一个地址：

```c
	*(uint64_t *)mem = val;
	return RTE_PTR_ADD(mem, __RTE_TRACE_EVENT_HEADER_SZ);
```

就这样`__rte_trace_point_emit_header_generic`执行结束。接下来会执行下面的系列函数：

```c
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_ptr(arg);
	rte_trace_point_emit_u32(slave_id);
	rte_trace_point_emit_int(rc);
```
```c
RTE_TRACE_POINT(
	rte_eal_trace_thread_remote_launch,
	RTE_TRACE_POINT_ARGS(int (*f)(void *), void *arg,
		unsigned int slave_id, int rc),
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_ptr(arg);
	rte_trace_point_emit_u32(slave_id);
	rte_trace_point_emit_int(rc);
)
```

## 1.3. rte_trace_point_emit_ptr

这几个函数一起说，首先他们的定义如下：
```c

#define rte_trace_point_emit_u64(in) __rte_trace_point_emit(in, uint64_t)
#define rte_trace_point_emit_i64(in) __rte_trace_point_emit(in, int64_t)
#define rte_trace_point_emit_u32(in) __rte_trace_point_emit(in, uint32_t)
#define rte_trace_point_emit_i32(in) __rte_trace_point_emit(in, int32_t)
#define rte_trace_point_emit_u16(in) __rte_trace_point_emit(in, uint16_t)
#define rte_trace_point_emit_i16(in) __rte_trace_point_emit(in, int16_t)
#define rte_trace_point_emit_u8(in) __rte_trace_point_emit(in, uint8_t)
#define rte_trace_point_emit_i8(in) __rte_trace_point_emit(in, int8_t)
#define rte_trace_point_emit_int(in) __rte_trace_point_emit(in, int32_t)
#define rte_trace_point_emit_long(in) __rte_trace_point_emit(in, long)
#define rte_trace_point_emit_float(in) __rte_trace_point_emit(in, float)
#define rte_trace_point_emit_double(in) __rte_trace_point_emit(in, double)
#define rte_trace_point_emit_ptr(in) __rte_trace_point_emit(in, uintptr_t)
```
继续：
```c
#define __rte_trace_point_emit(in, type) \
do { \
	memcpy(mem, &(in), sizeof(in)); \
	mem = RTE_PTR_ADD(mem, sizeof(in)); \
} while (0)
```
可见`__rte_trace_point_emit`和`__rte_trace_point_emit_header_generic`是遥相呼应的，因为都会对`mem`这个变量操作，接下来看看发生什么。

上面我们已经说过，函数`__rte_trace_point_emit_ev_header`将返回填充了时间戳和其他标志信息(如使能标志位)的下一个地址（64bist大小），那这里就很显然了，就是保存由`rte_trace_point_emit_ptr`等宏填充的数据类型，如下代码：

```c
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_ptr(arg);
	rte_trace_point_emit_u32(slave_id);
	rte_trace_point_emit_int(rc);
```
就会在mem后面陆续填入一个函数指针，一个arg指针，一个32bits的slave_id，和一个int型rc变量，这些变量是在`rte_eal_remote_launch`中给出的。

```c
int
rte_eal_remote_launch(int (*f)(void *), void *arg, unsigned slave_id)
{
	...
	rte_eal_trace_thread_remote_launch(f, arg, slave_id, rc);
	...
}
```
然后`rte_eal_trace_thread_remote_launch`将分别会执行：

```c
extern uint64_t __rte_eal_trace_thread_remote_launch;

static __rte_always_inline void rte_eal_trace_thread_remote_launch(int (*f)(void *), void *arg, unsigned int slave_id, int rc) {
    __rte_trace_point_emit_header_generic(&__rte_eal_trace_thread_remote_launch);
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_ptr(arg);
	rte_trace_point_emit_u32(slave_id);
	rte_trace_point_emit_int(rc);
}
```

至此，DPDK的一个tracepoint就结束了。















<br/>
<div align=right>作者 RTOAX</div>