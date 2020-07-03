..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

.. _Building_Your_Own_Application:

构建你自己的应用程序
======================

在DPDK中编译一个示例程序
--------------------------

当编译示例应用程序(如 hello world)时，需要导出变量：RTE_SDK 和 RTE_TARGET。

.. code-block:: console

    ~/DPDK$ cd examples/helloworld/
    ~/DPDK/examples/helloworld$ export RTE_SDK=/home/user/DPDK
    ~/DPDK/examples/helloworld$ export RTE_TARGET=x86_64-native-linuxapp-gcc
    ~/DPDK/examples/helloworld$ make
        CC main.o
        LD helloworld
        INSTALL-APP helloworld
        INSTALL-MAP helloworld.map

生成的二进制文件默认放在build目录下：

.. code-block:: console

    ~/DPDK/examples/helloworld$ ls build/app
    helloworld helloworld.map

在DPDK外构建自己的应用程序
----------------------------

示例应用程序(Hello World)可以复制到一个新的目录中作为开发目录：

.. code-block:: console

    ~$ cp -r DPDK/examples/helloworld my_rte_app
    ~$ cd my_rte_app/
    ~/my_rte_app$ export RTE_SDK=/home/user/DPDK
    ~/my_rte_app$ export RTE_TARGET=x86_64-native-linuxapp-gcc
    ~/my_rte_app$ make
        CC main.o
        LD helloworld
        INSTALL-APP helloworld
        INSTALL-MAP helloworld.map

定制 Makefiles
----------------

应用程序 Makefile
~~~~~~~~~~~~~~~~~~~~

示例应用程序默认的makefile可以作为一个很好的起点，我们可以直接修订使用。它包括：

*   起始处包含 $(RTE_SDK)/mk/rte.vars.mk 

*   终止处包含 $(RTE_SDK)/mk/rte.extapp.mk

用户必须配置几个变量：

*   APP: 应用程序的名称

*   SRCS-y: 源文件列表(\*.c, \*.S)。

库 Makefile
~~~~~~~~~~~~~

同样的方法也可以用于构建库：

*   起始处包含 $(RTE_SDK)/mk/rte.vars.mk

*   终止处包含 $(RTE_SDK)/mk/rte.extlib.mk

唯一的不同之处就是用LIB名称替换APP的名称，例如：libfoo.a。

定制 Makefile 动作
~~~~~~~~~~~~~~~~~~~~

可以通过定制一些变量来制定 Makefile 动作。常用的动作列表可以参考文档 :ref:`Development Kit Build System <Development_Kit_Build_System>` 章节  :ref:`Makefile Description <Makefile_Description>` 。


*   VPATH: 构建系统将搜索的源文件目录，默认情况下 RTE_SRCDIR 将被包含在 VPATH 中。

*   CFLAGS_my_file.o: 编译c文件时指定的编译flag标志。

*   CFLAGS: C编译标志。

*   LDFLAGS: 链接标志。

*   CPPFLAGS: 预处理器标志（只是用于汇编.s文件）。

*   LDLIBS: 链接库列表(如 -L /path/to/libfoo - lfoo)。
