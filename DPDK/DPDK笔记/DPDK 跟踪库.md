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

[跟踪库](https://doc.dpdk.org/guides/prog_guide/trace_lib.html)

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

此跟踪点的使用者可以调用`app_trace_string(const char *str)` 以将跟踪事件发送到跟踪缓冲区。

## 3.2. 注册跟踪点

```
#include <rte_trace_point_register.h>
#include <my_tracepoint.h>
RTE_TRACE_POINT_REGISTER(app_trace_string, app.trace.string)
```

上面的代码片段将app_trace_string跟踪点注册到跟踪库。在此，my_tracepoint.h是用户在第一步中创建的头文件。创建跟踪点头文件。

的第二个参数RTE_TRACE_POINT_REGISTER是跟踪点的名称。该字符串将用于跟踪点查找或基于正则表达式和/或基于glob的跟踪点操作。不需要跟踪点函数及其名称相似。但是，建议使用类似的名称以更好地命名。

>该rte_trace_point_register.h头必须包含任何列入前rte_trace_point.h头。

>在RTE_TRACE_POINT_REGISTER定义了占位符 rte_trace_point_t跟踪点的对象。用户必须__<trace_function_name>在库.map文件中导出 符号，此跟踪点才能在共享版本中从库中使用。例如，__app_trace_string在上面的示例中将是导出的符号。

# 4. 快速路径跟踪点
为了避免对快速路径代码的性能影响，引入了库 RTE_TRACE_POINT_FP。在快速路径代码中添加跟踪点时，用户必须使用RTE_TRACE_POINT_FP代替RTE_TRACE_POINT。

RTE_TRACE_POINT_FP默认情况下已被编译，并且可以使用CONFIG_RTE_ENABLE_TRACE_FP配置参数启用它 。该enable_trace_fp选项应与介子构建相同。

# 5. 事件记录模式
事件记录模式是跟踪缓冲区的属性。跟踪库公开以下模式：

覆写Overwrite：跟踪缓冲区已满时，新的跟踪事件将覆盖跟踪缓冲区中现有的捕获事件。
丢弃Discard：当跟踪缓冲区已满时，新的跟踪事件将被丢弃。
可以在应用程序启动时使用EAL命令行参数--trace-mode来配置该模式，也可以 在运行时使用API rte_trace_mode_set()进行配置。

# 6. 跟踪文件位置
在启动rte_trace_save()或rte_eal_cleanup()调用时，该库会将跟踪缓冲区保存到文件系统中。默认情况下，跟踪文件存储在 `$HOME/dpdk-traces/rte-yyyy-mm-dd-[AP]M-hh-mm-ss/`中。可以使用EAL命令行--trace-dir=\<directory path\>选项覆盖它。

有关更多信息，请参阅EAL参数以获取跟踪EAL命令行选项。

# 7. 查看和分析记录的事件
跟踪目录可用后，用户可以查看/检查记录的事件。

您可以使用许多工具来读取DPDK跟踪：

1. `babeltrace`是用于转换跟踪格式的命令行实用程序；它支持DPDK跟踪库生成的格式，CTF以及可以grep的基本文本输出。babeltrace命令是开源Babeltrace项目的一部分。

2. `Trace Compass`是一个图形用户界面，用于查看和分析任何类型的日志或跟踪，包括DPDK跟踪。

## 7.1. 使用babeltrace命令行工具

列出跟踪的所有记录事件的最简单方法是不带任何选项将其路径传递给babeltrace：

```
babeltrace </path-to-trace-events/rte-yyyy-mm-dd-[AP]M-hh-mm-ss/>
```

babeltrace 在给定路径中递归查找所有跟踪，并打印所有事件，并按时间顺序合并它们。
您可以将babeltrace的输出通过管道传输到grep（1）之类的工具中，以进行进一步过滤。下面的示例ethdev仅对事件进行grep ：

```
babeltrace /tmp/my-dpdk-trace | grep ethdev
```
您可以将babeltrace的输出通过管道传送到类似wc（1）的工具中，以对记录的事件进行计数。下面的示例计算ethdev事件数：

```
babeltrace /tmp/my-dpdk-trace | grep ethdev | wc --lines
```

## 7.2. 使用tracecompass GUI工具
略




<br/>
<div align=right>以上内容由RTOAX翻译整理。
	<img src="_v_images/20200910110657842_12395.jpg" width="40"> 
</div>