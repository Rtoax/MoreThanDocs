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

.. _External_Application/Library_Makefile_help:

外部应用程序/库的 Makefile
=============================

外部的应用程序或库必须包含RTE_SDK指定的位于mk目录中的Makefiles文件。
这些Makefiles包括：

*   ${RTE_SDK}/mk/rte.extapp.mk: 构建一个应用程序。

*   ${RTE_SDK}/mk/rte.extlib.mk: 构建一个静态库。

*   ${RTE_SDK}/mk/rte.extobj.mk: 购件一个目标文件。

前提
------

必须定义以下变量：

*   ${RTE_SDK}: 指向DPDK根目录。

*   ${RTE_TARGET}: 指向用于编译的目标编译器(如x86_64-native-linuxapp-gcc)。

构建 Targets
-------------

支持构建target时指定输出文件的目录，使用 O=mybuilddir 选项。
这是可选的，默认的输出目录是build。

*   all, "nothing" (仅make)

    编译应用程序或库到指定的输出目录中。

    例如：

    .. code-block:: console

        make O=mybuild

*   clean

    清除make操作产生的所有目标文件。

    例如：

    .. code-block:: console

        make clean O=mybuild

Help Targets
------------

*   help

    显示帮助信息。

其他有用的命令行变量
----------------------

以下变量可以在命令行中指定：

*   S=

    指定源文件的位置。默认情况下是当前目录。

*   M=

    指定需要被调用的Makefile。默认情况下使用 $(S)/Makefile。

*   V=

    使能详细编译（显示完全编译命令及一些中间命令过程）。

*   D=

    启用依赖关系调试。提供了一些有用的信息。

*   EXTRA_CFLAGS=, EXTRA_LDFLAGS=, EXTRA_ASFLAGS=, EXTRA_CPPFLAGS=

    添加的编译、连接或汇编标志。

*   CROSS=

    指定一个交叉工具链，该前缀将作为所有gcc/binutils应用程序的前缀。只有在gcc下才起作用。

从其他目录中编译
------------------

通过指定输出和源目录，可以从另一个目录运行Makefile。 例如：

.. code-block:: console

    export RTE_SDK=/path/to/DPDK
    export RTE_TARGET=x86_64-native-linuxapp-icc
    make -f /path/to/my_app/Makefile S=/path/to/my_app O=/path/to/build_dir
