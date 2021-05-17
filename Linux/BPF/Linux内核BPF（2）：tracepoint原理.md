<center><font size='5'>Linux内核 eBPF基础</font></center>
<center><font size='6'>Tracepoint原理源码分析</font></center>
<br/>
<br/>
<center><font size='3'>荣涛</font></center>
<center><font size='3'>2021年5月10日</font></center>
<br/>


# 1. 基本原理

需要注意的几点：

* 本文将从`sched_switch`相关的tracepoint展开；
* 关于`static_key`，请详见本博客jump-label相关文章[Linux Jump Label(x86)](https://rtoax.blog.csdn.net/article/details/115278450)、[Linux Jump Label/static-key机制详解](https://rtoax.blog.csdn.net/article/details/115279591)；
* 可参见内核注释版代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)


## 1.1. 源码头文件结构

首先看tracepoint源码文件结构：

* include/linux/tracepoint-defs.h
```c
    #include <linux/atomic.h>
    #include <linux/static_key.h>
```

* include/linux/tracepoint.h
```c
    #include <linux/tracepoint-defs.h>
    #include <linux/static_call.h>
```

* include/linux/trace_events.h
```c
    #include <linux/trace_seq.h>
    #include <linux/perf_event.h>
    #include <linux/tracepoint.h>
```

* include/trace/trace_events.h
```c
    #include <linux/trace_events.h>
```

* include/trace/define_trace.h
```c
    #ifdef TRACEPOINTS_ENABLED
    #include <trace/trace_events.h>
    #endif
```

* include/trace/events/sched.h
```c
    #include <linux/tracepoint.h>
    #include <trace/define_trace.h>
```

* kernel/trace/trace_sched_switch.c
```c
    #include <trace/events/sched.h>
```

* kernel/sched/core.c
```c
#define CREATE_TRACE_POINTS
#include <trace/events/sched.h>
#undef CREATE_TRACE_POINTS
```

### 1.1.1. include/linux/tracepoint-defs.h

该源文件定义了基本数据结构，如`struct tracepoint`。
```c
struct tracepoint {         /* 跟踪点 */
	const char *name;		/* Tracepoint name */
	struct static_key key;  /* static_key */
	struct static_call_key *static_call_key;
	void *static_call_tramp;/*  */
	void *iterator;         /*  */
	int (*regfunc)(void);   /* 注册函数 */
	void (*unregfunc)(void);/* 注销函数 */
	struct tracepoint_func __rcu *funcs;
};
```

### 1.1.2. include/linux/tracepoint.h

该头文件定义了相关的注册注销api，如下：

```c
extern int
tracepoint_probe_register(struct tracepoint *tp, void *probe, void *data);
extern int
tracepoint_probe_unregister(struct tracepoint *tp, void *probe, void *data);
```

并给出了关键的宏定义，如下：

* __DO_TRACE：运行实际的trace函数；
* __DECLARE_TRACE：声明tracepoint结构，定义静态的trace函数；
* DEFINE_TRACE_FN：定义tracepoint数据结构，static-key数据结构；
* DEFINE_TRACE：对`DEFINE_TRACE_FN`的封装；
* DECLARE_TRACE：对`__DECLARE_TRACE`的封装；
* DECLARE_TRACE_CONDITION：同上，多了条件变量；

#### 1.1.2.1. __DO_TRACE

```c
#define __DO_TRACE(name, proto, args, cond, rcuidle)			\
	do {								\
		struct tracepoint_func *it_func_ptr;			\
		int __maybe_unused __idx = 0;				\
		void *__data;						\
									\
		if (!(cond))						\
			return;						\
									\
		/* srcu can't be used from NMI */			\
		WARN_ON_ONCE(rcuidle && in_nmi());			\
									\
		/* keep srcu and sched-rcu usage consistent */		\
		preempt_disable_notrace();				\
									\
		/*							\
		 * For rcuidle callers, use srcu since sched-rcu	\
		 * doesn't work from the idle path.			\
		 */							\
		if (rcuidle) {						\
			__idx = srcu_read_lock_notrace(&tracepoint_srcu);\
			rcu_irq_enter_irqson();				\
		}							\
									\
		it_func_ptr =						\
			rcu_dereference_raw((&__tracepoint_##name)->funcs); \
		if (it_func_ptr) {					\
			__data = (it_func_ptr)->data;			\
			__DO_TRACE_CALL(name)(args);			\
		}							\
									\
		if (rcuidle) {						\
			rcu_irq_exit_irqson();				\
			srcu_read_unlock_notrace(&tracepoint_srcu, __idx);\
		}							\
									\
		preempt_enable_notrace();				\
	} while (0)
```
这里我们只关注`__DO_TRACE_CALL(name)(args);`，他的定义为：
```c
#ifdef CONFIG_HAVE_STATIC_CALL
#define __DO_TRACE_CALL(name)	static_call(tp_func_##name)
#else
#define __DO_TRACE_CALL(name)	__traceiter_##name
#endif /* CONFIG_HAVE_STATIC_CALL */
```
其中`__traceiter_##name`函数是在`DEFINE_TRACE_FN`中定义的，`static_call(tp_func_##name)`也是在`DEFINE_TRACE_FN`中定义的。

#### 1.1.2.2. __DECLARE_TRACE

```c
#define __DECLARE_TRACE(name, proto, args, cond, data_proto, data_args) \
	extern int __traceiter_##name(data_proto);			\
	DECLARE_STATIC_CALL(tp_func_##name, __traceiter_##name);	\
	extern struct tracepoint __tracepoint_##name;			\
	static inline void trace_##name(proto)				\
	{								\
		if (static_key_false(&__tracepoint_##name.key))		\
			__DO_TRACE(name,				\
				TP_PROTO(data_proto),			\
				TP_ARGS(data_args),			\
				TP_CONDITION(cond), 0);			\
		if (IS_ENABLED(CONFIG_LOCKDEP) && (cond)) {		\
			rcu_read_lock_sched_notrace();			\
			rcu_dereference_sched(__tracepoint_##name.funcs);\
			rcu_read_unlock_sched_notrace();		\
		}							\
	}								\
	__DECLARE_TRACE_RCU(name, PARAMS(proto), PARAMS(args),		\
		PARAMS(cond), PARAMS(data_proto), PARAMS(data_args))	\
	static inline int						\
	register_trace_##name(void (*probe)(data_proto), void *data)	\
	{								\
		return tracepoint_probe_register(&__tracepoint_##name,	\
						(void *)probe, data);	\
	}								\
	static inline int						\
	register_trace_prio_##name(void (*probe)(data_proto), void *data,\
				   int prio)				\
	{								\
		return tracepoint_probe_register_prio(&__tracepoint_##name, \
					      (void *)probe, data, prio); \
	}								\
	static inline int						\
	unregister_trace_##name(void (*probe)(data_proto), void *data)	\
	{								\
		return tracepoint_probe_unregister(&__tracepoint_##name,\
						(void *)probe, data);	\
	}								\
	static inline void						\
	check_trace_callback_type_##name(void (*cb)(data_proto))	\
	{								\
	}								\
	static inline bool						\
	trace_##name##_enabled(void)					\
	{								\
		return static_key_false(&__tracepoint_##name.key);	\
	}
```

其定义的函数名如下（以`sched_switch`为例）：

* trace_sched_switch；
* register_trace_sched_switch；
* register_trace_prio_sched_switch；
* unregister_trace_sched_switch；
* check_trace_callback_type_sched_switch；
* trace_sched_switch_enabled；

以上这些函数将在`kernel/trace/trace_sched_switch.c`中被调用。


#### 1.1.2.3. DEFINE_TRACE_FN

```c
#define DEFINE_TRACE_FN(_name, _reg, _unreg, proto, args)	/*  */	\
	static const char __tpstrtab_##_name[]				\
	__section("__tracepoints_strings") = #_name;			\
	extern struct static_call_key STATIC_CALL_KEY(tp_func_##_name);	\
	int __traceiter_##_name(void *__data, proto);			\
	struct tracepoint __tracepoint_##_name	__used			\
	__section("__tracepoints") = {					\
		.name = __tpstrtab_##_name,				\
		.key = STATIC_KEY_INIT_FALSE,				\
		.static_call_key = &STATIC_CALL_KEY(tp_func_##_name),	\
		.static_call_tramp = STATIC_CALL_TRAMP_ADDR(tp_func_##_name), \
		.iterator = &__traceiter_##_name,			\
		.regfunc = _reg,					\
		.unregfunc = _unreg,					\
		.funcs = NULL };					\
	__TRACEPOINT_ENTRY(_name);					\
	int __traceiter_##_name(void *__data, proto)			\
	{								\
		struct tracepoint_func *it_func_ptr;			\
		void *it_func;						\
									\
		it_func_ptr =						\
			rcu_dereference_raw((&__tracepoint_##_name)->funcs); \
		do {							\
			it_func = (it_func_ptr)->func;			\
			__data = (it_func_ptr)->data;			\
			((void(*)(void *, proto))(it_func))(__data, args); \
		} while ((++it_func_ptr)->func);			\
		return 0;						\
	}								\
	DEFINE_STATIC_CALL(tp_func_##_name, __traceiter_##_name);
```

其定义的结构如下（以`sched_switch`为例）：

* struct tracepoint __tracepoint_sched_switch
* 函数`__traceiter_sched_switch`；
* DEFINE_STATIC_CALL(`tp_func_sched_switch`, `__traceiter_sched_switch`)

这些函数是在`__DO_TRACE`中被调用的。


### 1.1.3. include/linux/trace_events.h

该文件中定义了以下数据结构

* `trace_iterator`

他的注释为：
```c
/*
 * Trace iterator - used by printout routines who present trace
 * results to users and which routines might sleep, etc:
 */
```
也就是说，它的出现是为了解决trace和user print的之间异步的问题。

* struct trace_event

```c
struct trace_event {    /* trace 时间 */
	struct hlist_node		node;
	struct list_head		list;
	int				type;
	struct trace_event_functions	*funcs;
};
```
声明了两个函数：

```c
extern int register_trace_event(struct trace_event *event);
extern int unregister_trace_event(struct trace_event *event);
```
注册函数`register_trace_event`是将trace_event添加至链表（哈希表）：
```c
    list_add_tail(&event->list, list);
    hlist_add_head(&event->node, &event_hash[key]);
```

注销函数`unregister_trace_event`是将trace_event从链表中删除：
```c
	hlist_del(&event->node);
	list_del(&event->list);
```

* struct trace_event_class
* struct trace_event_buffer
* struct trace_event_call
* ...

具体情况具体分析。

### 1.1.4. include/trace/trace_events.h

文件中通过`undef`、`define`和`TRACE_INCLUDE`将该文件分为几个`stage`，我分步说明。

首先看下trace文件目录结构：

```
include/trace
.
├── bpf_probe.h
├── define_trace.h
├── events
│   ├── 9p.h
│   ...
│   ├── xdp.h
│   └── xen.h
├── perf.h
├── syscall.h
└── trace_events.h -> 本文件include/trace/trace_events.h
```

#### 1.1.4.1. Stage 1 of the trace events.
```c
/*
 * Stage 1 of the trace events.
 *
 * Override the macros in the event tracepoint header <trace/events/XXX.h>
 * to include the following:
 *
 * struct trace_event_raw_<call> {
 *	struct trace_entry		ent;
 *	<type>				<item>;
 *	<type2>				<item2>[<len>];
 *	[...]
 * };
 *
 * The <type> <item> is created by the __field(type, item) macro or
 * the __array(type2, item2, len) macro.
 * We simply do "type item;", and that will create the fields
 * in the structure.
 */
```
后面的我们会在具体sched的trace中讲解。



### 1.1.5. include/trace/define_trace.h

该文件中为一些宏定义，以及包含以下头文件：
```c
#ifdef TRACEPOINTS_ENABLED
#include <trace/trace_events.h>
#include <trace/perf.h>
#include <trace/bpf_probe.h>
#endif
```

### 1.1.6. include/trace/events/sched.h

这是具体的函数和结构体的声明，如本文关注的：

```c
/*
 * Tracepoint for task switches, performed by the scheduler:
 */
TRACE_EVENT(sched_switch,

	TP_PROTO(bool preempt,
		 struct task_struct *prev,
		 struct task_struct *next),

	TP_ARGS(preempt, prev, next),

	TP_STRUCT__entry(
		__array(	char,	prev_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	prev_pid			)
		__field(	int,	prev_prio			)
		__field(	long,	prev_state			)
		__array(	char,	next_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	next_pid			)
		__field(	int,	next_prio			)
	),

	TP_fast_assign(
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->prev_pid	= prev->pid;
		__entry->prev_prio	= prev->prio;
		__entry->prev_state	= __trace_sched_switch_state(preempt, prev);
		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
		__entry->next_pid	= next->pid;
		__entry->next_prio	= next->prio;
		/* XXX SCHED_DEADLINE */
	),

	TP_printk("prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%d",
		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,

		(__entry->prev_state & (TASK_REPORT_MAX - 1)) ?
		  __print_flags(__entry->prev_state & (TASK_REPORT_MAX - 1), "|",
				{ TASK_INTERRUPTIBLE, "S" },
				{ TASK_UNINTERRUPTIBLE, "D" },
				{ __TASK_STOPPED, "T" },
				{ __TASK_TRACED, "t" },
				{ EXIT_DEAD, "X" },
				{ EXIT_ZOMBIE, "Z" },
				{ TASK_PARKED, "P" },
				{ TASK_DEAD, "I" }) :
		  "R",

		__entry->prev_state & TASK_REPORT_MAX ? "+" : "",
		__entry->next_comm, __entry->next_pid, __entry->next_prio)
);
```

### 1.1.7. kernel/sched/core.c

该文件包含了上节的头文件：
```c
#define CREATE_TRACE_POINTS
#include <trace/events/sched.h>
#undef CREATE_TRACE_POINTS
```
在`__schedule`有将调用静态跟踪点`trace_sched_switch`。
```c
static void __sched notrace __schedule(bool preempt)
{
    ...
		trace_sched_switch(preempt, prev, next);
    ...
}
```

## 1.2. 基本原理

下面从`kernel/sched/core.c`出发，从顶向下分析`trace_sched_switch`。

* `kernel/sched/core.c`中包含头文件`include/trace/events/sched.h`，并定义`CREATE_TRACE_POINTS`；
* `include/trace/events/sched.h`中包含了`include/linux/tracepoint.h`；
* 按照以上小节中的顺序依次进行了包含关系；

首先在`include/linux/tracepoint.h`中定义了`TRACE_EVENT`为`DECLARE_TRACE`；经过一系列追踪，`TRACE_EVENT(sched_switch`也将进行一系列的展开。

宏`TRACE_EVENT`几个关键的定义如下：

* include/trace/trace_events.h
* include/trace/define_trace.h
* include/linux/tracepoint.h


### 1.2.1. include/trace/define_trace.h
```c
#define TRACE_EVENT(name, proto, args, tstruct, assign, print)	\
	DEFINE_TRACE(name, PARAMS(proto), PARAMS(args))
#define DEFINE_TRACE(name, proto, args)		\
	DEFINE_TRACE_FN(name, NULL, NULL, PARAMS(proto), PARAMS(args));
```
展开后为：
```c
static const char __section("__tracepoints_strings") __tpstrtab_sched_switch[] = "sched_switch";            
extern struct static_call_key STATIC_CALL_KEY(tp_func_sched_switch); 
int __traceiter_sched_switch(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next);           
struct tracepoint __used __section("__tracepoints") __tracepoint_sched_switch = {                  
    .name = __tpstrtab_sched_switch,             
    .key = STATIC_KEY_INIT_FALSE,               
    .static_call_key = &STATIC_CALL_KEY(tp_func_sched_switch),   
    .static_call_tramp = STATIC_CALL_TRAMP_ADDR(tp_func_sched_switch), 
    .iterator = &__traceiter_sched_switch,
    .regfunc = NULL,                    
    .unregfunc = NULL,
    .funcs = NULL 
};                    
asm("   .section \"__tracepoints_ptrs\", \"a\"      \n" \
    "   .balign 4                   \n" \
    "   .long   __tracepoint_sched_switch - .      \n" \
    "   .previous                   \n");

int __traceiter_sched_switch(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next)            
{                               
    struct tracepoint_func *it_func_ptr;            
    void *it_func;                      

    it_func_ptr =                       
    rcu_dereference_raw((&__tracepoint_sched_switch)->funcs); 
    do {                            
        it_func = (it_func_ptr)->func;          
        __data = (it_func_ptr)->data;           
        ((void(*)(void *, bool preempt, struct task_struct *prev, struct task_struct *next))(it_func))(__data, preempt, prev, next); 
    } while ((++it_func_ptr)->func);            
    return 0;                       
}                               
DEFINE_STATIC_CALL(tp_func_sched_switch, __traceiter_sched_switch);
```


### 1.2.2. include/linux/tracepoint.h

```c
#define TRACE_EVENT(name, proto, args, struct, assign, print)	\
	DECLARE_TRACE(name, PARAMS(proto), PARAMS(args))
```

展开为：

```c
extern int __traceiter_sched_switch(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next);
DECLARE_STATIC_CALL(tp_func_sched_switch, __traceiter_sched_switch);    
extern struct tracepoint __tracepoint_sched_switch; 

static inline void trace_sched_switch(bool preempt, struct task_struct *prev, struct task_struct *next)
{
    if (static_key_false(&__tracepoint_sched_switch.key)) {
        struct tracepoint_func *it_func_ptr;
        int __maybe_unused __idx = 0;
        void *__data;

        if (!(cpu_online(raw_smp_processor_id())))
            return;

        /* srcu can't be used from NMI */
        WARN_ON_ONCE(0 && in_nmi());

        /* keep srcu and sched-rcu usage consistent */
        preempt_disable_notrace()

        /*
         * For rcuidle callers, use srcu since sched-rcu
         * doesn't work from the idle path.
         */
        if (0) {
            __idx = srcu_read_lock_notrace(&tracepoint_srcu);
            rcu_irq_enter_irqson();
        }
		it_func_ptr =						
			rcu_dereference_raw((&__tracepoint_sched_switch)->funcs); 
		if (it_func_ptr) {					
			__data = (it_func_ptr)->data;			
//			__DO_TRACE_CALL(sched_switch)(__data, preempt, prev, next);	
            __traceiter_sched_switch(__data, preempt, prev, next);	 /* 这里最终对应这个函数 */
		}

		if (0) {						
			rcu_irq_exit_irqson();				
			srcu_read_unlock_notrace(&tracepoint_srcu, __idx);
		}							
									
		preempt_enable_notrace();				
	} 
    if (IS_ENABLED(CONFIG_LOCKDEP) && (cpu_online(raw_smp_processor_id()))) {     
        rcu_read_lock_sched_notrace();          
        rcu_dereference_sched(__tracepoint_sched_switch.funcs);
        rcu_read_unlock_sched_notrace();        
    }                           
}                               

#ifndef MODULE
static inline void trace_sched_switch_rcuidle(proto)		
{								
	if (static_key_false(&__tracepoint_sched_switch.key))		
		/* 同上static_key_false 部分代码  */		
}
#endif

static inline int                       
register_trace_sched_switch(void (*probe)(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next), void *data)    
{                               
    return tracepoint_probe_register(&__tracepoint_sched_switch,  
                    (void *)probe, data);   
}                               
static inline int                       
register_trace_prio_sched_switch(void (*probe)(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next),
                void *data,
               int prio)                
{                               
    return tracepoint_probe_register_prio(&__tracepoint_sched_switch, 
                      (void *)probe, data, prio); 
}                               
static inline int                       
unregister_trace_sched_switch(void (*probe)(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next), 
        void *data)  
{                               
    return tracepoint_probe_unregister(&__tracepoint_sched_switch,
                    (void *)probe, data);   
}                               
static inline void                      
check_trace_callback_type_sched_switch(void (*cb)(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next))    
{                               
}                               
static inline bool                      
trace_sched_switch_enabled(void)                    
{                               
    return static_key_false(&__tracepoint_sched_switch.key);  
}
```



### 1.2.3. include/trace/trace_events.h
```c
#define TRACE_EVENT(name, proto, args, tstruct, assign, print) \
	DECLARE_EVENT_CLASS(name,			       \
			     PARAMS(proto),		       \
			     PARAMS(args),		       \
			     PARAMS(tstruct),		       \
			     PARAMS(assign),		       \
			     PARAMS(print));		       \
	DEFINE_EVENT(name, name, PARAMS(proto), PARAMS(args));
```

> TODO


至此，`trace_sched_switch`的结构就简单明亮了。

# 2. 实例分析`TRACE_EVENT(sched_switch, ...)`

在源代码`include/linux/tracepoint.h`中有这样的注释：
```c
/*
 * For use with the TRACE_EVENT macro:
 *
 * We define a tracepoint, its arguments, its printk format
 * and its 'fast binary record' layout.
 *
 * Firstly, name your tracepoint via TRACE_EVENT(name : the
 * 'subsystem_event' notation is fine.
 *
 * Think about this whole construct as the
 * 'trace_sched_switch() function' from now on.
 *
 *
 *  TRACE_EVENT(sched_switch,
 *
 *	*
 *	* A function has a regular function arguments
 *	* prototype, declare it via TP_PROTO():
 *	*
 *
 *	TP_PROTO(struct rq *rq, struct task_struct *prev,
 *		 struct task_struct *next),
 *
 *	*
 *	* Define the call signature of the 'function'.
 *	* (Design sidenote: we use this instead of a
 *	*  TP_PROTO1/TP_PROTO2/TP_PROTO3 ugliness.)
 *	*
 *
 *	TP_ARGS(rq, prev, next),
 *
 *	*
 *	* Fast binary tracing: define the trace record via
 *	* TP_STRUCT__entry(). You can think about it like a
 *	* regular C structure local variable definition.
 *	*
 *	* This is how the trace record is structured and will
 *	* be saved into the ring buffer. These are the fields
 *	* that will be exposed to user-space in
 *	* /sys/kernel/debug/tracing/events/<*>/format.
 *	*
 *	* The declared 'local variable' is called '__entry'
 *	*
 *	* __field(pid_t, prev_prid) is equivalent to a standard declariton:
 *	*
 *	*	pid_t	prev_pid;
 *	*
 *	* __array(char, prev_comm, TASK_COMM_LEN) is equivalent to:
 *	*
 *	*	char	prev_comm[TASK_COMM_LEN];
 *	*
 *
 *	TP_STRUCT__entry(
 *		__array(	char,	prev_comm,	TASK_COMM_LEN	)
 *		__field(	pid_t,	prev_pid			)
 *		__field(	int,	prev_prio			)
 *		__array(	char,	next_comm,	TASK_COMM_LEN	)
 *		__field(	pid_t,	next_pid			)
 *		__field(	int,	next_prio			)
 *	),
 *
 *	*
 *	* Assign the entry into the trace record, by embedding
 *	* a full C statement block into TP_fast_assign(). You
 *	* can refer to the trace record as '__entry' -
 *	* otherwise you can put arbitrary C code in here.
 *	*
 *	* Note: this C code will execute every time a trace event
 *	* happens, on an active tracepoint.
 *	*
 *
 *	TP_fast_assign(
 *		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
 *		__entry->prev_pid	= prev->pid;
 *		__entry->prev_prio	= prev->prio;
 *		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
 *		__entry->next_pid	= next->pid;
 *		__entry->next_prio	= next->prio;
 *	),
 *
 *	*
 *	* Formatted output of a trace record via TP_printk().
 *	* This is how the tracepoint will appear under ftrace
 *	* plugins that make use of this tracepoint.
 *	*
 *	* (raw-binary tracing wont actually perform this step.)
 *	*
 *
 *	TP_printk("task %s:%d [%d] ==> %s:%d [%d]",
 *		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,
 *		__entry->next_comm, __entry->next_pid, __entry->next_prio),
 *
 * );
 *
 * This macro construct is thus used for the regular printk format
 * tracing setup, it is used to construct a function pointer based
 * tracepoint callback (this is used by programmatic plugins and
 * can also by used by generic instrumentation like SystemTap), and
 * it is also used to expose a structured trace record in
 * /sys/kernel/debug/tracing/events/.
 *
 * A set of (un)registration functions can be passed to the variant
 * TRACE_EVENT_FN to perform any (un)registration work.
 */
```

## 2.1. trace_sched_switch调用

在`__schedule`中有对`trace_sched_switch`这样的调用：

```c
static void __sched notrace __schedule(bool preempt)
{
    ...
		trace_sched_switch(preempt, prev, next);
    ...
}
```

首先，将判断`if (static_key_false(&__tracepoint_sched_switch.key))`，当trace未开启是，`trace_sched_switch`的开销是微乎其微的。那么trace开关值怎么开启的呢？函数`trace_sched_switch_enabled`判断tracepoint是否被打开。

```c
static inline bool
trace_sched_switch_enabled(void)
{
    return static_key_false(&__tracepoint_sched_switch.key);  
}
```

## 2.2. 内核支持哪些追踪点

### 2.2.1. tplist命令

这里引入一个命令tplist，man如下：
```c
tplist(8)                                    System Manager's Manual                                   tplist(8)

NAME
       tplist - Display kernel tracepoints or USDT probes and their formats.

SYNOPSIS
       tplist [-p PID] [-l LIB] [-v] [filter]

DESCRIPTION
       tplist  lists  all  kernel  tracepoints,  and can optionally print out the tracepoint format; namely, the
       variables that you can trace when the tracepoint is hit.  tplist can also list USDT probes embedded in  a
       specific  library  or  executable,  and  can  list USDT probes for all the libraries loaded by a specific
       process.  These features are usually used in conjunction with the argdist and/or trace tools.

       On a typical system, accessing the tracepoint list and format requires  root.   However,  accessing  USDT
       probes does not require root.
```

首选用命令查看是否支持：
```
# tplist | grep sched_switch
sched:sched_switch
```
为了知道是如何查询的，用strace追踪一下：
```
stat("/sys/kernel/debug/tracing/events/sched/sched_switch", {st_mode=S_IFDIR|0755, st_size=0, ...}) = 0
```
可见tplist实际上是使用sys文件系统的`/sys/kernel/debug/tracing/events/sched/sched_switch`目录，看下目录下包含的文件：
```
/sys/kernel/debug/tracing/events/sched/sched_switch/
├── enable
├── filter
├── format
└── id
```
查看这个tracepoint的默认状态：
```
cat /sys/kernel/debug/tracing/events/sched/sched_switch/enable 
0
```

### 2.2.2. perf list

在perf list中同样可以找到这个追踪点
```
sched:sched_switch                                 [Tracepoint event]
```

### 2.2.3. bpf

同样，使用bcc和bpftrace等bpf前端工具也是非常容易找到这个追踪点的，这里不再详述。


## 2.3. 跟踪点如何开启

通过上面的讨论可见，这个追踪点默认是关闭的，那么如何开启的呢？很简单
```c
echo 1 > /sys/kernel/debug/tracing/events/sched/sched_switch/enable
```

## 2.4.  程序运行到跟踪点会执行什么

参见上面讲到的几个函数：

* register_trace_sched_switch
* register_trace_prio_sched_switch
* unregister_trace_sched_switch

### 2.4.1. ftrace

```c
ret = register_trace_sched_switch(ftrace_graph_probe_sched_switch, NULL);
if (ret)
	pr_info("ftrace_graph: Couldn't activate tracepoint"
		" probe to kernel_sched_switch\n");
```
程序运行到跟踪点时，函数`ftrace_graph_probe_sched_switch`将被执行，就是一些追踪
```c
static void
ftrace_graph_probe_sched_switch(void *ignore, bool preempt,
			struct task_struct *prev, struct task_struct *next)
{
	unsigned long long timestamp;
	int index;

	/*
	 * Does the user want to count the time a function was asleep.
	 * If so, do not update the time stamps.
	 */
	if (fgraph_sleep_time)
		return;

	timestamp = trace_clock_local();

	prev->ftrace_timestamp = timestamp;

	/* only process tasks that we timestamped */
	if (!next->ftrace_timestamp)
		return;

	/*
	 * Update all the counters in next to make up for the
	 * time next was sleeping.
	 */
	timestamp -= next->ftrace_timestamp;

	for (index = next->curr_ret_stack; index >= 0; index--)
		next->ret_stack[index].calltime += timestamp;
}
```
还有`ftrace_filter_pid_sched_switch_probe`。

### 2.4.2. probe
```c
	ret = register_trace_sched_switch(probe_sched_switch, NULL);
	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint"
			" probe to kernel_sched_switch\n");
		goto fail_deprobe_wake_new;
	}
```
还有另一个
```c
	ret = register_trace_sched_switch(probe_wakeup_sched_switch, NULL);
	if (ret) {
		pr_info("sched trace: Couldn't activate tracepoint"
			" probe to kernel_sched_switch\n");
		goto fail_deprobe_wake_new;
	}
```
其中`probe_sched_switch`如下：
```c
static void
probe_sched_switch(void *ignore, bool preempt,
		   struct task_struct *prev, struct task_struct *next)
{
	int flags;

	flags = (RECORD_TGID * !!sched_tgid_ref) +
		(RECORD_CMDLINE * !!sched_cmdline_ref);

	if (!flags)
		return;
	tracing_record_taskinfo_sched_switch(prev, next, flags);
}
```

具体的tracepoint调用的函数不在本文介绍和研究。


# 3. 结论

综上，tracepoint是基于jump-label的，关于jump-label详情参见文末链接。同时，将tracing功能和文件系统挂钩，简化用户使用流程，要查看系统支持哪些tracepoint，可以只用`perf list`和`tplist`指令查看，或者直接查看debugfs或者proc文件系统。


# 4. 相关阅读

* [Linux Jump Label(x86)](https://rtoax.blog.csdn.net/article/details/115278450)
* [Linux Jump Label/static-key机制详解](https://rtoax.blog.csdn.net/article/details/115279591)；