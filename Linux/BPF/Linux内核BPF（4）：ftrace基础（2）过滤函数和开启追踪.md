<center><font size='5'>Linux内核 eBPF基础</font></center>
<center><font size='6'>ftrace源码分析：过滤函数和开启追踪</font></center>
<br/>
<br/>
<center><font size='3'>荣涛</font></center>
<center><font size='3'>2021年5月12日</font></center>
<br/>

本文相关注释代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
上篇文章：[Linux内核 eBPF基础：ftrace基础](https://rtoax.blog.csdn.net/article/details/116718182)
推荐Presentation：[《Ftrace Kernel Hooks: More than just tracing》](https://rtoax.blog.csdn.net/article/details/116744253)

# 1. 从`set_ftrace_filter`讲起

`/sys/kernel/debug/tracing/tracing_on`

```bash
su
cd /sys/kernel/debug/tracing/
echo 1 > tracing_on
echo "schedule" > set_ftrace_filter
echo function_graph > current_tracer
cat trace
```

部分输出：

```
   1) ! 66730.47 us |  }
 ------------------------------------------
   1)  contain-2264  =>  contain-2147 
 ------------------------------------------

   1) ! 66717.12 us |  } /* schedule */
   1)   3.683 us    |  schedule();
   1)               |  schedule() {
 ------------------------------------------
   1)  contain-2147  =>  contain-2175 
 ------------------------------------------
```

那么以上流程在内核中都经历了什么呢？让我们细细道来。

# 2. `echo schedule > set_ftrace_filter`


在内核中`kernel\trace\ftrace.c`代码有：

```c
	trace_create_file("set_ftrace_filter", 0644, parent,
			  ops, &ftrace_filter_fops);
```
看一下文件操作符`ftrace_filter_fops`：
```c
static const struct file_operations ftrace_filter_fops = {
	.open = ftrace_filter_open,
	.read = seq_read,
	.write = ftrace_filter_write,
	.llseek = tracing_lseek,
	.release = ftrace_regex_release,
};
```
显然，这里对应`ftrace_filter_write`。

## 2.1. `ftrace_filter_write`函数

该函数很简单：
```c
ssize_t /* echo schedule > set_ftrace_filter */
ftrace_filter_write(struct file *file, const char __user *ubuf,
		    size_t cnt, loff_t *ppos)
{
	return ftrace_regex_write(file, ubuf, cnt, ppos, 1);
}
```
调用了`ftrace_regex_write`，首先从打开的文件私有数据中获取`struct ftrace_iterator`结构，

```c
	if (file->f_mode & FMODE_READ) {
		struct seq_file *m = file->private_data;
		iter = m->private;
	} else
		iter = file->private_data;
```

这个结构中包含了太多有用的信息：
```c
struct ftrace_iterator {
	loff_t				pos;
	loff_t				func_pos;
	loff_t				mod_pos;
	struct ftrace_page		*pg;
	struct dyn_ftrace		*func;
	struct ftrace_func_probe	*probe;
	struct ftrace_func_entry	*probe_entry;
	struct trace_parser		parser;
	struct ftrace_hash		*hash;
	struct ftrace_ops		*ops;
	struct trace_array		*tr;
	struct list_head		*mod_list;
	int				pidx;
	int				idx;
	unsigned			flags;
};
```

首先使用函数`trace_get_user`将用户字符串写入`struct trace_parser`结构中。接着执行下面代码：

```c
	if (read >= 0 && trace_parser_loaded(parser) &&
	    !trace_parser_cont(parser)) {
		ret = ftrace_process_regex(iter, parser->buffer,
					   parser->idx, enable);
		trace_parser_clear(parser);
		if (ret < 0)
			goto out;
	}
```
我们直接看`ftrace_process_regex`，此时`enable=1`。

## 2.2. `ftrace_process_regex`函数

函数首先将获取func，在我们的例子中，`func="schedul"`，然后会调用下面的代码：

```c
	func = strsep(&next, ":");

	if (!next) {
		ret = ftrace_match_records(hash, func, len);
		if (!ret)
			ret = -EINVAL;
		if (ret < 0)
			return ret;
		return 0;
	}
```
`ftrace_match_records`进一步会调用`match_records`，这里可以得知，`match_records`的入参为：
```c
match_records(hash, "schedul", len, NULL);
```

## 2.3. `match_records`函数

首选调用`filter_parse_regex`进行正则表达式匹配，匹配的类型包括：

```c
enum regex_type {
	MATCH_FULL = 0,
	MATCH_FRONT_ONLY,
	MATCH_MIDDLE_ONLY,
	MATCH_END_ONLY,
	MATCH_GLOB,
	MATCH_INDEX,
};
```
因为我们的例子是`func="schedul"`，所以类型为`MATCH_FULL`。接着运行下面的代码：

```c
	do_for_each_ftrace_rec(pg, rec) {

		if (rec->flags & FTRACE_FL_DISABLED)
			continue;

		if (ftrace_match_record(rec, &func_g, mod_match, exclude_mod)) {
			ret = enter_record(hash, rec, clear_filter);
			if (ret < 0) {
				found = ret;
				goto out_unlock;
			}
			found = 1;
		}
	} while_for_each_ftrace_rec();
```

首先是两个宏定义：

```c
#define do_for_each_ftrace_rec(pg, rec)					\
	for (pg = ftrace_pages_start; pg; pg = pg->next) {		\
		int _____i;						\
		for (_____i = 0; _____i < pg->index; _____i++) {	\
			rec = &pg->records[_____i];

#define while_for_each_ftrace_rec()		\
		}				\
	}
```
最重要的是全局变量`ftrace_pages_start`，我们需要细致讲解一下：

```c
static struct ftrace_page	*ftrace_pages_start;    /* ftrace 起始地址 */
                                                    /* 初始化位置 ftrace_process_locs() */
static struct ftrace_page	*ftrace_pages;          /* 同上，用于定位链表中最后一个 pg */
```

## 2.4. `ftrace_pages_start`和`ftrace_pages`

这两个全局变量是在`ftrace_process_locs`中初始化的，他是在`ftrace_init`中被调用，这在[《Linux内核 eBPF基础：ftrace基础-ftrace_init初始化》](https://rtoax.blog.csdn.net/article/details/116718182)有详细介绍，他的调用如下：
```c
	ret = ftrace_process_locs(NULL,
				  __start_mcount_loc,
				  __stop_mcount_loc);
```
在我的系统中为：
```
# cat /proc/kallsyms | grep mcount_loc
ffffffffaf0d75d0 T __start_mcount_loc
ffffffffaf1110e0 T __stop_mcount_loc
```
page大小可以计算得出。

在遍历过程中，每一项用`struct dyn_ftrace`标识，在循环中调用`ftrace_match_record`。

## 2.5. `ftrace_match_record`函数

他的入参为：

```c
ftrace_match_record(
    (struct dyn_ftrace*)rec,
    (struct ftrace_glob*) {
        .search = "schedule",
        .type = MATCH_FULL,
        .len = strlen("schedule"),
        },
    (struct ftrace_glob*)NULL,
    0
)
```
该函数首先调用：
```c
	kallsyms_lookup(rec->ip, NULL, NULL, &modname, str);
```
他的操作如下，即获取如下信息：
```
# cat /proc/kallsyms | grep " schedule"
[...]
ffffffffae97f1a0 T schedule
[...]
```
步骤如下：

* 使用`is_ksym_addr`判断是否在内核中；
* 使用`kallsyms_expand_symbol`获取ip对应的函数名（`str`）；
* 因为不在模块中，所以`if (mod_g)`分支不执行；
* 执行`ftrace_match`函数；

## 2.6. `ftrace_match`函数

他的入参为：
```c
ftrace_match(
    "schedule",
    (struct ftrace_glob*) {
        .search = "schedule",
        .type = MATCH_FULL,
        .len = strlen("schedule"),
        },
)
```

因为我没有使用正则表达式，所以我们的type为`MATCH_FULL`，也就是字符串完全匹配（`"schedule"`）。所以我匹配的代码是：

```c
	case MATCH_FULL:
		if (strcmp(str, g->search) == 0)
			matched = 1;
		break;
```
也就是说匹配上了。

这时，`ftrace_match_record`返回匹配结果`1`。然后调用`enter_record`函数。

## 2.7. `enter_record`函数

他的入参为：

```c
enter_record(
    (struct ftrace_hash *)hash,
    (struct dyn_ftrace *)rec,
    0
)
```
这里的hash对应`struct ftrace_iterator *iter`中的`struct ftrace_hash *hash`结构（iter为file文件的私有数据`iter = file->private_data;`）。

首先使用`ftrace_lookup_ip`从哈希结构中查找到这个ip对应的entry结构，他的结构为：

```c
struct ftrace_func_entry {
	struct hlist_node hlist;
	unsigned long ip;
	unsigned long direct; /* for direct lookup only */
};
```
如果存在，直接退出：
```c
	entry = ftrace_lookup_ip(hash, rec->ip);
	if (clear_filter) {
		/* Do nothing if it doesn't exist */
		if (!entry)
			return 0;

		free_hash_entry(hash, entry);
	} else {
		/* Do nothing if it exists */
		if (entry)
			return 0;

		ret = add_hash_entry(hash, rec->ip);
	}
```

至此，该函数返回，导致

```c
		if (ftrace_match_record(rec, &func_g, mod_match, exclude_mod)) {
			ret = enter_record(hash, rec, clear_filter);
			if (ret < 0) {
				found = ret;
				goto out_unlock;
			}
			found = 1;
		}
```
在遍历所有page之后，`match_records`返回`1`，所以`ftrace_match_records`返回`1`，在函数`ftrace_process_regex`中，
```c
		ret = ftrace_match_records(hash, func, len);
		if (!ret)
			ret = -EINVAL;
		if (ret < 0)
			return ret;
		return 0;
```
返回`0`。

至此，`ftrace_regex_write`函数返回，也就是说，我们在中断中执行的

```bash
echo schedule > set_ftrace_filter
```
返回了。


那么，关于`schedule`的ftrace是如何使用的呢？


# 3. `echo function_graph > current_tracer`

> 为什么是`function_graph`？请使用下面的命令查看
```bash
cat available_tracers 
hwlat blk function_graph wakeup_dl wakeup_rt wakeup function nop
```

在内核中`kernel\trace\trace.c`代码有：
```c
    /* /sys/kernel/debug/tracing/current_tracer */
	trace_create_file("current_tracer", 0644, d_tracer,
			tr, &set_tracer_fops);
```
也就是他对应的文件操作符为：

```c
static const struct file_operations set_tracer_fops = {
	.open		= tracing_open_generic,
	.read		= tracing_set_trace_read,
	.write		= tracing_set_trace_write,
	.llseek		= generic_file_llseek,
};
```
那我们就要看`tracing_set_trace_write`函数了。

## 3.1. `tracing_set_trace_write`函数

函数原型为：
```c
static ssize_t
tracing_set_trace_write(struct file *filp, const char __user *ubuf,
			size_t cnt, loff_t *ppos)
```


首先将用户字符串拷贝到内核`copy_from_user(buf, ubuf, cnt)`，然后取出空格

```c
	for (i = cnt - 1; i > 0 && isspace(buf[i]); i--)
		buf[i] = 0;
```
然后调用`tracing_set_tracer`函数。

## 3.2. `tracing_set_tracer`函数

`struct trace_array *tr = filp->private_data;`

他的入参为：

```c
tracing_set_tracer(
    (struct trace_array *)tr,
    "function_graph"
)
```

`struct trace_array`可以认为是一块缓冲区，当ftrace时间发生时需要对这段ring-buffer读写。

而对于`function_graph`，

```bash
cat available_tracers 
hwlat blk function_graph wakeup_dl wakeup_rt wakeup function nop
```
以上每个类型对应一个`struct tracer`结构，他的定义如下（部分）：

```c
struct tracer { /*  */
	const char		*name;
	int			    (*init)(struct trace_array *tr);
	void			(*reset)(struct trace_array *tr);
	void			(*start)(struct trace_array *tr);
```
在函数中或作这样的查找：

```c
	for (t = trace_types; t; t = t->next) {
		if (strcmp(t->name, buf) == 0)
			break;
	}
```

也就是找到`function_graph`对应的`struct tracer`结构，他是在源文件`kernel/trace/trace_functions_graph.c`中定义的：

```c
static struct tracer __tracer_data graph_trace  = {
	.name		= "function_graph",
	.update_thresh	= graph_trace_update_thresh,
	.open		= graph_trace_open,
	.pipe_open	= graph_trace_open,
	.close		= graph_trace_close,
	.pipe_close	= graph_trace_close,
	.init		= graph_trace_init,
	.reset		= graph_trace_reset,
	.print_line	= print_graph_function,
	.print_header	= print_graph_headers,
	.flags		= &tracer_flags,
	.set_flag	= func_graph_set_flag,
#ifdef CONFIG_FTRACE_SELFTEST
	.selftest	= trace_selftest_startup_function_graph,
#endif
};
```

这里我将跳过很多判断和鉴权，仅仅对函数调用感兴趣。

首先使用reset将之前的tracer重置
```c
	if (tr->current_trace->reset)
		tr->current_trace->reset(tr);   
```

加入上次使用`function_graph`，本次使用`function`，那么此处就会调用`function_graph`对应的reset函数，即`graph_trace_reset`，简单看下它做了什么：

```c
static void graph_trace_reset(struct trace_array *tr)
{
	tracing_stop_cmdline_record();
	if (tracing_thresh)
		unregister_ftrace_graph(&funcgraph_thresh_ops);
	else
		unregister_ftrace_graph(&funcgraph_ops);
}
```
简言之，就是注销一系列的东西（断点，进程调度相关内容等）。

然后就是使用初始化函数初始化`struct tracer`

```c
	if (t->init) {
		ret = tracer_init(t, tr);
		if (ret)
			goto out;
	}
```

## 3.3. `tracer_init`函数

入参：

```c
tracer_init(
    (struct tracer*)t,
    (struct trace_array *)tr
)
```
函数实现很简单：
```c
int tracer_init(struct tracer *t, struct trace_array *tr)
{
	tracing_reset_online_cpus(&tr->array_buffer);
	return t->init(tr);
}
```
首先使用`tracing_reset_online_cpus`重置buffer，然后调用tracer对应init函数，`function_graph`对应的init为`graph_trace_init`。

## 3.4. `graph_trace_init`函数

```c
static int graph_trace_init(struct trace_array *tr)
{
	int ret;

	set_graph_array(tr);
	if (tracing_thresh)
		ret = register_ftrace_graph(&funcgraph_thresh_ops);
	else
		ret = register_ftrace_graph(&funcgraph_ops);
	if (ret)
		return ret;
	tracing_start_cmdline_record();

	return 0;
}
```

首先初始化array
```c
void set_graph_array(struct trace_array *tr)
{
	graph_array = tr;

	/* Make graph_array visible before we start tracing */

	smp_mb();
}
```

> 内存屏障`smp_mb();`的作用：让`graph_array`在开始tracing之前可见。

然后调用`register_ftrace_graph`，函数中设置了对应的全局函数指针：

```c
ftrace_graph_return = trace_graph_return;
__ftrace_graph_entry = trace_graph_entry;
ftrace_graph_entry = ftrace_graph_entry_test;
```

使用`register_pm_notifier`注册电源管理同质量，这里不讨论。

调用`start_graph_tracing`函数。

## 3.5. `start_graph_tracing`函数

```c
	ftrace_graph_active++;
	ret = start_graph_tracing();
	if (ret) {
		ftrace_graph_active--;
		goto out;
	}
```

首先申请存放栈的内存空间：

```c
	ret_stack_list = kmalloc_array(FTRACE_RETSTACK_ALLOC_SIZE,
				       sizeof(struct ftrace_ret_stack *),
				       GFP_KERNEL);
```
默认大小为：
```c
#define FTRACE_RETFUNC_DEPTH 50
#define FTRACE_RETSTACK_ALLOC_SIZE 32
```
然后是对idle进程的设置：
```c
	/* The cpu_boot init_task->ret_stack will never be freed */
	for_each_online_cpu(cpu) {
		if (!idle_task(cpu)->ret_stack)
			ftrace_graph_init_idle_task(idle_task(cpu), cpu);
	}
```
然后尝试在FTRACE_RETSTACK_ALLOC_SIZE任务上分配一个返回堆栈数组。

```c
	do {
		ret = alloc_retstack_tasklist(ret_stack_list);
	} while (ret == -EAGAIN);
```
然后激活tracepoint
```c
		ret = register_trace_sched_switch(ftrace_graph_probe_sched_switch, NULL);
		if (ret)
			pr_info("ftrace_graph: Couldn't activate tracepoint"
				" probe to kernel_sched_switch\n");
```

这里可参见上面提到的`tracing_set_tracer`函数：
```c
	if (tr->current_trace->reset)
		tr->current_trace->reset(tr);
```

然后就进入了 `ftrace_startup`函数。

## 3.6. `ftrace_startup`函数

> `register_ftrace_function`函数内部会调用这个函数。

他的入参为：

```c
ftrace_startup(&graph_ops, FTRACE_START_FUNC_RET)
```
其中`graph_ops`为：

```c
static struct ftrace_ops graph_ops = {
	.func			= ftrace_stub,
	.flags			= FTRACE_OPS_FL_RECURSION_SAFE |
				   FTRACE_OPS_FL_INITIALIZED |
				   FTRACE_OPS_FL_PID |
				   FTRACE_OPS_FL_STUB,
#ifdef FTRACE_GRAPH_TRAMP_ADDR
	.trampoline		= FTRACE_GRAPH_TRAMP_ADDR,
	/* trampoline_size is only needed for dynamically allocated tramps */
#endif
	ASSIGN_OPS_HASH(graph_ops, &global_ops.local_hash)
};
```

该函数首先调用`__register_ftrace_function`函数。

### 3.6.1. `__register_ftrace_function`函数

函数入参为`graph_ops`。

首先调用函数`add_ftrace_ops(&ftrace_ops_list, ops)`，`ftrace_ops_list`为

```c
struct ftrace_ops __rcu __read_mostly *ftrace_ops_list  = &ftrace_list_end;
```

`ftrace_list_end`为：

```c
struct ftrace_ops __read_mostly ftrace_list_end  = {
	.func		= ftrace_stub,
	.flags		= FTRACE_OPS_FL_RECURSION_SAFE | FTRACE_OPS_FL_STUB,
	INIT_OPS_HASH(ftrace_list_end)
};
```

最终，生成的链表为(`graph_ops`和`ftrace_ops_list`是一个节点)：

```
ftrace_ops_list->graph_ops->ftrace_list_end
```
`add_ftrace_ops`函数刚刚运行结束后，func是这样的：

```
  链表头
    |
graph_ops->func = ftrace_stub
    |
ftrace_list_end->func = ftrace_stub
    |
  链表尾
```
也就是目前他们的func均为`ftrace_stub`。


### 3.6.2. `ftrace_update_trampoline`函数

大致流程为

* 调用`arch_ftrace_update_trampoline`架构相关的函数创建蹦床结构；
    * 调用`create_trampoline`创建蹦床并赋值；
    * 调用`calc_trampoline_call_offset`计算指令偏移；
    * 调用`ftrace_call_replace`生成并替换指令；
    * 调用`text_poke_bp`进行指令替换

下面对`arch_ftrace_update_trampoline`函数内部流程进行详述。

#### 3.6.2.1. `create_trampoline`函数

为了简化流程，我们默认用这段代码分支：

```c
	start_offset = (unsigned long)ftrace_caller;
	end_offset = (unsigned long)ftrace_caller_end;
	op_offset = (unsigned long)ftrace_caller_op_ptr;
	call_offset = (unsigned long)ftrace_call;
	jmp_offset = 0;
```
在我的系统中为：
```c
ffffffffae98f840 T ftrace_caller
ffffffffae98f89c T ftrace_call
```

然后申请蹦床需要的内存：

```c
trampoline = alloc_tramp(size + RET_SIZE + sizeof(void *));
```
这里的大小非常讲究`size + RET_SIZE + sizeof(void *)`，我将在下文中解释：

```
+--------+    start_offset=ftrace_caller
|        |
|        |
|  size  |
|        |
+--------+    end_offset=ftrace_caller_end
|RET_SIZE|
+--------+
| void * |
+--------+
```

然后将蹦床函数拷贝过去：
```c
ret = copy_from_kernel_nofault(trampoline, (void *)start_offset, size);
```
用ip指向end之后：
```c
ip = trampoline + size;
```
也就是：
```
+--------+    start_offset=ftrace_caller
|        |
|        |
|  size  |
|        |
+--------+    end_offset=ftrace_caller_end 
|RET_SIZE|    <--ip
+--------+
| void * |
+--------+
```
然后将`ftrace_stub`拷贝到`ip`

```c
	retq = (unsigned long)ftrace_stub;
	ret = copy_from_kernel_nofault(ip, (void *)retq, RET_SIZE);
```
```
+--------+    start_offset=ftrace_caller
|        |
|        |    
|  size  |
|        |
+--------+    end_offset=ftrace_caller_end 
|RET_SIZE|    ftrace_stub    <--ip
+--------+
| void * | ---> graph_ops
+--------+
```

接着，计算`void *`位置，并将ops赋值，
```c
	ptr = (unsigned long *)(trampoline + size + RET_SIZE);
	*ptr = (unsigned long)ops;
```

已知在源文件中：

```c
SYM_FUNC_START(ftrace_caller)
	/* save_mcount_regs fills in first two parameters */
	save_mcount_regs

SYM_INNER_LABEL(ftrace_caller_op_ptr, SYM_L_GLOBAL)
	/* Load the ftrace_ops into the 3rd parameter */
	movq function_trace_op(%rip), %rdx

	/* regs go into 4th parameter (but make it NULL) */
	movq $0, %rcx

SYM_INNER_LABEL(ftrace_call, SYM_L_GLOBAL)
	call ftrace_stub

	restore_mcount_regs

	/*
	 * The code up to this label is copied into trampolines so
	 * think twice before adding any new code or changing the
	 * layout here.
	 */
SYM_INNER_LABEL(ftrace_caller_end, SYM_L_GLOBAL)

	jmp ftrace_epilogue
SYM_FUNC_END(ftrace_caller);
```

也就是说现在是这样的：

```
+--------+    ftrace_caller(start_offset)
|        |
|        |    ftrace_caller_op_ptr(op_offset)
|        |
|  size  |    ftrace_call(call_offset)
|        |
+--------+    ftrace_caller_end(end_offset)
|RET_SIZE|    ftrace_stub    <--ip
+--------+
| void * | ---> graph_ops
+--------+
```

也就是说，对于本文的`schedule`是这样的：

```
<schedule>:
push %rbp
mov %rsp,%rbp
call ftrace_caller
pop %rbp
[…]
```

```
ftrace_caller:
    save regs
    load args
ftrace_call:
    call func
    restore regs
ftrace_stub:
    retq
```

关于函数`ftrace_update_trampoline`我们就讲到这，需要说明的是，如果要细讲，还有提多的东西需要说明。

### 3.6.3. `update_ftrace_function`函数

该函数会替换`func`函数，这里我们姑且认为它是`func = ftrace_ops_list_func;`。
同时，该函数中最终会调用追踪函数（链表）：
```c
op->func(ip, parent_ip, op, regs);
```

至此，`__register_ftrace_function`返回。

接下来就是一系列的使能：

* `ftrace_hash_ipmodify_enable`
* `ftrace_hash_rec_enable`
* `ftrace_startup_enable`

至此，开始返回到用户空间：

* `ftrace_startup`返回，
* `register_ftrace_graph`返回，
* `graph_trace_init`返回，
* `tracer_init`返回，
* `tracing_set_tracer`返回，
* `tracing_set_trace_write`返回，
* 指令`echo function_graph > current_tracer`返回。


# 4. `cat trace`命令
在内核中`kernel\trace\trace.c`代码有：
```c
	trace_create_file("trace", 0644, d_tracer,
			  tr, &tracing_fops);
```

文件操作符为：

```c
static const struct file_operations tracing_fops = {
	.open		= tracing_open,
	.read		= seq_read,
	.write		= tracing_write_stub,
	.llseek		= tracing_lseek,
	.release	= tracing_release,
};
```

2021年5月14日19:02:35，不要意思，要下班了，我要早点下班去，怕有长进着急。有时间在继续完成下面的部分，会在另一篇文章中继续讨论。

# 5. 蹦床`trampoline`函数

`TODO`

## 5.1. `ftrace_stub`函数

## 5.2. `ftrace_caller`函数
## 5.3. `ftrace_caller_end`函数
## 5.4. `ftrace_caller_op_ptr`函数
## 5.5. `ftrace_call`函数

## 5.6. `ftrace_ops_list_func`函数




# 6. 参考和相关链接

* 内核注释版代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* [《Linux内核 eBPF基础：kprobe原理源码分析：基本介绍与使用》](https://rtoax.blog.csdn.net/article/details/116643875)
* [Linux内核 eBPF基础：kprobe原理源码分析：源码分析](https://rtoax.blog.csdn.net/article/details/116643902)
* [《Linux内核：kprobe机制-探测点》](https://rtoax.blog.csdn.net/article/details/110835122)
* [《Linux eBPF：bcc 用法和原理初探之 kprobes 注入》](https://rtoax.blog.csdn.net/article/details/115603383)
* [《Linux内核调试技术——kprobe使用与实现》](https://blog.csdn.net/luckyapple1028/article/details/52972315)
* [《Linux kprobe调试技术使用》](https://www.cnblogs.com/arnoldlu/p/9752061.html)
* [linux-5.10.13/Documentation/trace/kprobes.rst](https://rtoax.blog.csdn.net/article/details/116664268)
* [https://github.com/tinyclub/markdown-lab/tree/clk-2016-ftrace/slides](https://github.com/tinyclub/markdown-lab/tree/clk-2016-ftrace/slides)
* [FTRACE: vmlinux __mcount_loc section](https://www.cnblogs.com/aspirs/p/14696965.html)
* [ftrace(一)原理简介](https://www.cnblogs.com/openix/p/4163995.html)
* [Linux内核 eBPF基础：ftrace基础](https://rtoax.blog.csdn.net/article/details/116718182)
* [GCC(-pg) profile mcount | ftrace基础原理](https://rtoax.blog.csdn.net/article/details/116718210)
* [《ftrace-kernel-hooks-2014-More than just tracing.pdf》](https://download.csdn.net/download/Rong_Toa/18627308)
* [《Ftrace Kernel Hooks: More than just tracing》](https://rtoax.blog.csdn.net/article/details/116744253)


附录
=========================================

参考路径
=================

* `kernel/include/asm-generic/vmlinux.lds.h`
* `/sys/kernel/debug/tracing/`

register_ftrace_function内核路径
=================

```c
//kernel/kprobes.c
enable_kprobe
    arm_kprobe
        arm_kprobe_ftrace
            __arm_kprobe_ftrace
                register_ftrace_function
```

```c
//kernel/trace/trace_events.c
// late_initcall(event_trace_self_tests_init);
event_trace_self_tests_init
    event_trace_self_test_with_function
        register_ftrace_function(&trace_ops)
```

```c
//kernel/trace/trace_event_perf.c
perf_ftrace_event_register
    perf_ftrace_function_register
        register_ftrace_function
```

```c
function_trace_init
    tracing_start_function_trace
        register_ftrace_function
```

```c
func_set_flag
    register_ftrace_function
```

```c
init_irqsoff_tracer() {
	register_tracer(&irqsoff_tracer);
}
core_initcall(init_irqsoff_tracer);
static struct tracer irqsoff_tracer  = {
	...
	.init		= irqsoff_tracer_init,
    ...
};
irqsoff_tracer_init
    __irqsoff_tracer_init
        start_irqsoff_tracer
            register_irqsoff_function
                register_ftrace_function
```


```c
stack_trace_sysctl
    register_ftrace_function
```

```c
stack_trace_init
    register_ftrace_function

device_initcall(stack_trace_init);
```