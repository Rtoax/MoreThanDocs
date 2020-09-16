<div align=center>
	<img src="_v_images/20200910110325796_584.png" width="600"> 
</div>
<br/>
<br/>

<center><font size='20'>DPDK笔记 DPDK 跟踪库</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>

# 1. 总览
跟踪是一种用于了解正在运行的软件系统中发生了什么的技术。用于跟踪的软件称为跟踪器，从概念上讲它类似于磁带记录器。记录时，放置在软件源代码中的特定检测点会生成事件，这些事件将保存在巨型磁带上：跟踪文件。然后，可以稍后在跟踪查看器中打开跟踪文件，以使用时间戳和多核视图来可视化和分析跟踪事件。这种机制对于解决诸如多核同步问题，等待时间测量，找出诸如CPU空闲时间之类的后分析信息之类的广泛问题非常有用，否则这些问题将极具挑战性。

通常将跟踪与日志记录进行比较。但是，跟踪器和记录器是两个不同的工具，可用于两个不同的目的。跟踪器旨在记录比日志消息更频繁发生的低级事件，通常在每秒数千个范围内，并且执行开销很小。日志记录更适合于对不太频繁的事件进行非常高级别的分析：用户访问，异常情况（例如错误和警告），数据库事务，即时消息传递通信等。简而言之，日志记录是可以满足跟踪需求的众多用例之一。

# 2. DPDK跟踪库功能
在控制和快速路径API中添加跟踪点的框架，对性能的影响最小。典型的跟踪开销约为20个周期，而仪器开销为1个周期。
在运行时启用和禁用跟踪点。
在任何时间点将跟踪缓冲区保存到文件系统。
支持overwrite和discard跟踪模式操作。
基于字符串的跟踪点对象查找。
根据正则表达式和/或通配启用和禁用一组跟踪点。
在`Common Trace Format (CTF)`中生成跟踪。CTF是一种开源跟踪格式，并且与LTTng兼容。有关详细信息，请参阅 [通用跟踪格式](https://diamon.org/ctf/)。

# 3. 如何添加跟踪点？
本节逐步介绍添加简单跟踪点的详细信息。

## 3.1. 创建跟踪点头文件

```c
#include <rte_trace_point.h>

RTE_TRACE_POINT(
       app_trace_string,
       RTE_TRACE_POINT_ARGS(const char *str),
       rte_trace_point_emit_string(str);
)
```

上面的宏创建app_trace_string跟踪点。用户可以为跟踪点选择任何名称。但是，在DPDK库中添加跟踪点时， rte_<library_name>_trace_[<domain>_]<name>必须遵循命名约定。的例子是rte_eal_trace_generic_str，rte_mempool_trace_create。

在RTE_TRACE_POINT从上述定义为以下函数模板宏展开：

```c
static __rte_always_inline void
app_trace_string(const char *str)
{
        /* Trace subsystem hooks */
        ...
        rte_trace_point_emit_string(str);
}
```

[TODO](https://doc.dpdk.org/guides/prog_guide/trace_lib.html)
[TODO](https://doc.dpdk.org/guides/prog_guide/trace_lib.html)
[TODO](https://doc.dpdk.org/guides/prog_guide/trace_lib.html)















<br/>
<div align=right>以上内容由RTOAX翻译整理。
	<img src="_v_images/20200910110657842_12395.jpg" width="40"> 
</div>