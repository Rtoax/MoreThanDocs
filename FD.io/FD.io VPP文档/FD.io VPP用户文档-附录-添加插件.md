<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>



# 1. 添加插件

## 1.1. 总览
本节说明VPP开发人员如何创建新插件并将其添加到VPP。我们假设我们从VPP <工作区顶部>开始。

作为一个例子，我们将使用make-plugin.sh中发现的工具 ./extras/emacs。make-plugin.sh是由一组emacs-lisp框架构造的综合插件生成器的简单包装。
## 1.2. 将目录更改为./src/plugins，然后运行插件生成器：

```
$ cd ./src/plugins
$ ../../extras/emacs/make-plugin.sh
<snip>
Loading /scratch/vpp-docs/extras/emacs/tunnel-c-skel.el (source)...
Loading /scratch/vpp-docs/extras/emacs/tunnel-decap-skel.el (source)...
Loading /scratch/vpp-docs/extras/emacs/tunnel-encap-skel.el (source)...
Loading /scratch/vpp-docs/extras/emacs/tunnel-h-skel.el (source)...
Loading /scratch/vpp-docs/extras/emacs/elog-4-int-skel.el (source)...
Loading /scratch/vpp-docs/extras/emacs/elog-4-int-track-skel.el (source)...
Loading /scratch/vpp-docs/extras/emacs/elog-enum-skel.el (source)...
Loading /scratch/vpp-docs/extras/emacs/elog-one-datum-skel.el (source)...
Plugin name: myplugin
Dispatch type [dual or qs]: dual
(Shell command succeeded with no output)

OK...
```
插件生成器脚本会问两个问题：插件的名称以及要使用的两种调度类型中的哪一种。由于插件名称进入了很多地方-文件名，typedef名称，图形弧名称-值得思考一下。

调度类型是指用于构造node.c（即形式数据平面节点）的编码模式 。对偶选项构造具有推测性排队的双单环对。这是用于负载存储密集图节点的传统编码模式。

所述适量选项生成其使用`vlib_get_buffers`一个四单环路对（...）和`vlib_buffer_enqueue_to_next（...）`。这些运算符充分利用了可用的SIMD向量单元运算。如果您决定稍后将四单循环对更改为双单循环对，则非常简单。

## 1.3. 生成的文件
这是生成的文件。我们待会儿会通过它们。

```
$ cd ./myplugin
$ ls
CMakeLists.txt        myplugin.c           myplugin_periodic.c  setup.pg
myplugin_all_api_h.h  myplugin.h           myplugin_test.c
myplugin.api          myplugin_msg_enum.h  node.c
```
由于最近构建系统的改进，你并不需要接触任何其他文件到您的新插件集成到VPP构建。只需从头开始重建工作空间，就会出现新的插件。

## 1.4. 重建工作区
这是重新配置和重建工作空间的直接方法：
```
$ cd <top-of-workspace>
$ make rebuild [or rebuild-release]
```
多亏了ccache，该操作不会花费很多时间。

## 1.5. 健全性检查：运行vpp

为了快速进行完整性检查，请运行vpp并确保已加载“ myplugin_plugin.so”和“ myplugin_test_plugin.so”：

```
$ cd <top-of-workspace>
$ make run
<snip>
load_one_plugin:189: Loaded plugin: myplugin_plugin.so (myplugin description goes here)
<snip>
load_one_vat_plugin:67: Loaded plugin: myplugin_test_plugin.so
<snip>
DBGvpp#
```
如果此简单测试失败，请寻求帮助。

## 1.6. 详细生成的文件

本节将详细讨论生成的文件。可以略过本节，然后再返回以获取更多详细信息。

### 1.6.1. CMakeLists.txt
这是用于构建插件的构建系统配方。请修正版权声明：

```
# 4. Copyright (c) <current-year> <your-organization>
```
其余的构建方法非常简单：
```
add_vpp_plugin (myplugin
SOURCES
myplugin.c
node.c
myplugin_periodic.c
myplugin.h

MULTIARCH_SOURCES
node.c

API_FILES
myplugin.api

INSTALL_HEADERS
myplugin_all_api_h.h
myplugin_msg_enum.h

API_TEST_SOURCES
myplugin_test.c
)
```
如您所见，构建配方由几个文件列表组成。SOURCES是C源文件的列表。API_FILES是插件的二进制API定义文件的列表（此文件通常很多），依此类推。

MULTIARCH_SOURCES列出了认为对性能至关重要的数据平面图节点调度功能源文件。这些文件中的特定功能会被多次编译，以便它们可以利用特定于CPU的功能。稍后对此进行更多讨论。

如果添加源文件，只需将它们添加到指定的列表中即可。

### 1.6.2. myplugin.h
这是新插件的主要#include文件。除其他外，它定义了插件的main_t数据结构。这是添加特定于问题的数据结构的正确位置。请抵制在插件中创建一组静态或[更糟]全局变量的诱惑。裁判插件之间的名称冲突并不是任何人的好时机。

### 1.6.3. myplugin.c
为了缺少更好的描述方式，myplugin.c是vpp插件的“ main.c”等效项。它的工作是将插件挂接到vpp二进制API消息调度程序中，并将其消息添加到vpp的全局“ message-name_crc”哈希表中。请参见“ myplugin_init（…”）”

Vpp本身使用dlsym（…）来跟踪由`VLIB_PLUGIN_REGISTER`宏生成的vlib_plugin_registration_t：

```
VLIB_PLUGIN_REGISTER () =
  {
    .version = VPP_BUILD_VER,
    .description = "myplugin plugin description goes here",
  };
```
Vpp仅从插件目录中加载包含该数据结构实例的.so文件。

您可以从命令行启用或禁用特定的vpp插件。默认情况下，插件已加载。若要更改该行为，请在宏VLIB_PLUGIN_REGISTER中设置default_disabled：
```
VLIB_PLUGIN_REGISTER () =
  {
    .version = VPP_BUILD_VER,
    .default_disabled = 1
    .description = "myplugin plugin description goes here",
  };
```
样板生成器将图形节点分配功能放置到“设备输入”特征弧上。这可能有用也可能没有用。
```
VNET_FEATURE_INIT (myplugin, static) =
{
  .arc_name = "device-input",
  .node_name = "myplugin",
  .runs_before = VNET_FEATURES ("ethernet-input"),
};
```

如插件生成器所给出的那样，myplugin.c包含二进制API消息处理程序，用于通用的“请在这样的接口上启用我的功能”二进制API消息。如您所见，设置vpp消息API表很简单。大警告：该方案不能容忍小错误。示例：忘记添加mainp-> msg_id_base可能导致非常混乱的失败。

如果您坚持谨慎地修改生成的样板-而不是尝试根据首要原理来构建代码-您将节省很多时间和麻烦

### 1.6.4. myplugin_test.c
该文件包含二进制API消息生成代码，该代码被编译成单独的.so文件。“ vpp_api_test”程序将加载这些插件，从而立即访问您的插件API，以进行外部客户端二进制API测试。

vpp本身会加载测试插件，并通过“ binary-api”调试CLI使代码可用。这是在集成测试之前对二进制API进行单元测试的一种常用方法。

### 1.6.5. node.c
`这是生成的图节点分派函数`。您需要重写它来解决当前的问题。保留节点分发功能的结构将节省大量时间和麻烦。

即使是专家，也要浪费时间来重新构造循环结构，排队模式等等。只需撕下并用与您要解决的问题相关的代码替换样本1x，2x，4x数据包处理代码即可。

## 1.7. 插件是“有好处的朋友”

在vpp VLIB_INIT_FUNCTION函数中，通常可以看到特定的init函数调用其他init函数：

```c
if ((error = vlib_call_init_function (vm, some_other_init_function))
   return error;
```
如果一个插件需要在另一个插件中调用init函数，请使用vlib_call_plugin_init_function宏：
```c
if ((error = vlib_call_plugin_init_function (vm, "otherpluginname", some_init_function))
   return error;
```
这允许在插件初始化函数之间进行排序。

如果您希望获得指向另一个插件中的符号的指针，请使用`vlib_plugin_get_symbol（…）`API：

```c
void *p = vlib_get_plugin_symbol ("plugin_name", "symbol");
```
更多例子
有关更多信息，您可以阅读目录“ ./src/plugins”中的许多示例插件。

<br/>
<div align=right>	以上内容由荣涛翻译整理自网络。</div>