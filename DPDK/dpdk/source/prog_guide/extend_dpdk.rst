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

扩展 DPDK
============

本章描述了开发者如何通过扩展DPDK来提供一个新的库、目标文件或者支持新的开发板。

示例：添加新的库 libfoo
--------------------------

要添加新的库到DPDK，按照如下操作：

#.  添加新的配置选项：

    .. code-block:: bash

        for f in config/\*; do \
            echo CONFIG_RTE_LIBFOO=y >> $f; done

#.  创建新的源码目录：

    .. code-block:: console

        mkdir ${RTE_SDK}/lib/libfoo
        touch ${RTE_SDK}/lib/libfoo/foo.c
        touch ${RTE_SDK}/lib/libfoo/foo.h
        
#.  源码添加 foo() 函数。

    函数定义于 foo.c:

    .. code-block:: c

        void foo(void)
        {
        }

    函数声明于 foo.h:

    .. code-block:: c

        extern void foo(void);

#.  更新文件 lib/Makefile:

    .. code-block:: console

        vi ${RTE_SDK}/lib/Makefile
        # add:
        # DIRS-$(CONFIG_RTE_LIBFOO) += libfoo

#.  为新的库创建新的 Makefile，如派生自 mempool Makefile，进行修改：

    .. code-block:: console

        cp ${RTE_SDK}/lib/librte_mempool/Makefile ${RTE_SDK}/lib/libfoo/

        vi ${RTE_SDK}/lib/libfoo/Makefile
        # replace:
        # librte_mempool -> libfoo
        # rte_mempool -> foo

#.  更新文件 mk/DPDK.app.mk，添加 -lfoo 选项到 LDLIBS 变量中。
    链接DPDK应用程序时会自动添加此标志。
　　    　

#.  添加此新库之后，重新构建DPDK (此处仅显示这个特殊的部分)：

    .. code-block:: console

        cd ${RTE_SDK}
        make config T=x86_64-native-linuxapp-gcc
        make

#.  检测这个库被正确安装了：

    .. code-block:: console

        ls build/lib
        ls build/include

示例：在测试用例中使用新库 libfoo 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

测试应用程序用于验证DPDK的所有功能。
一旦添加了一个库，应该在测试用例程序中添加一个用例。

*   新的测试文件 test_foo.c 被添加，包含头文件 foo.h 并调用 foo() 函数。
    当测试通过时，test_foo() 函数需要返回0。

*   为了处理新的测试用例，Makefile, test.h 和 commands.c 必须同时更新。

*   测试报告生成：autotest.py 是一个脚本，用于生成文件 ${RTE_SDK}/doc/rst/test_report/autotests 目录中指定的测试用例报告。
    如果libfoo处于新的测试家族，链接 ${RTE_SDK}/doc/rst/test_report/test_report.rst 需要更新。

*   重新构建DPDK库，添加新的测试应用程序：


    .. code-block:: console

        cd ${RTE_SDK}
        make config T=x86_64-native-linuxapp-gcc
        make
