<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档 构建与安装</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>


要开始使用VPP进行开发，您需要获取所需的VPP源代码，然后构建软件包。
>注意，安装的rpm有些需要强制安装，请先确认环境中运行的软件没有对这些rpm包的依赖。

<br/>

# 1. 设置代理

如果你的服务器本地可以连接互联网，则跳过设置代理的步骤。

根据您所使用的环境，可能需要设置代理。运行以下代理命令以指定代理服务器名称和相应的端口号：

## 1.1. 设置HTTP代理

```bash
$ export http_proxy=http://<proxy-server-name>.com:<port-number>
$ export https_proxy=https://<proxy-server-name>.com:<port-number>
```
## 1.2. 设置YUM代理

编辑文件`/etc/yum.conf`，添加下面一行

```bash
proxy=http://10.175.0.13:808
```

## 1.3. 设置GIT代理

如果获取源代码采用git方式（github），在文件`~/.gitconfig`添加如下内容，该修改只对当前用户有效。

```
[http]
	proxy=http://10.175.0.13:808
[https]
	proxy=http://10.175.0.13:808
```



<br/>

# 2. 获取VPP
要获取用于创建内部版本的VPP源，请运行以下命令：

## 2.1. 从gerrit.fd.io

```
$ git clone https://gerrit.fd.io/r/vpp
$ cd vpp
```
## 2.2. 从GitHub

如果使用github

```
$ git clone https://github.com/FDio/vpp
```

选择分支，我安装的是`20.05`版本

![VPP分支](_v_images/20200908095947580_11055.png)

<br/>

# 3. 安装VPP依赖
在构建VPP映像之前，通过输入以下命令，确保没有安装FD.io VPP或DPDK软件包：

```
$ dpkg -l | grep vpp
$ dpkg -l | grep DPDK
```
运行上述命令后，应该没有输出或没有显示任何程序包。


运行以下make命令以安装FD.io VPP的依赖项。

如果下载在任何时候都挂起，则可能需要 设置代理才能使下载正常工作。

```
$ make install-dep
```

最终指令运行成功的日志如下：

```
Hit:1 http://us.archive.ubuntu.com/ubuntu xenial InRelease
Get:2 http://us.archive.ubuntu.com/ubuntu xenial-updates InRelease [109 kB]
Get:3 http://security.ubuntu.com/ubuntu xenial-security InRelease [107 kB]
Get:4 http://us.archive.ubuntu.com/ubuntu xenial-backports InRelease [107 kB]
Get:5 http://us.archive.ubuntu.com/ubuntu xenial-updates/main amd64 Packages [803 kB]
Get:6 http://us.archive.ubuntu.com/ubuntu xenial-updates/main i386 Packages [732 kB]
...
...
Update-alternatives: using /usr/lib/jvm/java-8-openjdk-amd64/bin/jmap to provide /usr/bin/jmap (jmap) in auto mode
Setting up default-jdk-headless (2:1.8-56ubuntu2) ...
Processing triggers for libc-bin (2.23-0ubuntu3) ...
Processing triggers for systemd (229-4ubuntu6) ...
Processing triggers for ureadahead (0.100.0-19) ...
Processing triggers for ca-certificates (20160104ubuntu1) ...
Updating certificates in /etc/ssl/certs...
0 added, 0 removed; done.
Running hooks in /etc/ca-certificates/update.d...

done.
done.
```

<br/>


## 3.1. 在我的虚拟机上的完整的log如下：

```
# make install-dep
已加载插件：fastestmirror, langpacks
fdio_release-source/signature                                                               |  819 B  00:00:00     
fdio_release-source/signature                                                               |  951 B  00:00:00 !!! 
Loading mirror speeds from cached hostfile
 * base: ftp.sjtu.edu.cn
 * centos-sclo-rh: mirror.bit.edu.cn
 * centos-sclo-sclo: ftp.sjtu.edu.cn
 * elrepo: dfw.mirror.rackspace.com
 * extras: mirror.bit.edu.cn
 * updates: ftp.sjtu.edu.cn
软件包 centos-release-scl-rh-2-3.el7.centos.noarch 已安装并且是最新版本
软件包 epel-release-7-11.noarch 已安装并且是最新版本
无须任何处理
已加载插件：fastestmirror, langpacks
没有安装组信息文件
Maybe run: yum groups mark convert (see man yum)
Loading mirror speeds from cached hostfile
 * base: ftp.sjtu.edu.cn
 * centos-sclo-rh: mirror.bit.edu.cn
 * centos-sclo-sclo: ftp.sjtu.edu.cn
 * elrepo: dfw.mirror.rackspace.com
 * extras: mirror.bit.edu.cn
 * updates: ftp.sjtu.edu.cn
警告：分组 development 不包含任何可安装软件包。
Maybe run: yum groups mark install (see man yum)
指定组中没有可安装或升级的软件包
已加载插件：fastestmirror, langpacks
Loading mirror speeds from cached hostfile
 * base: ftp.sjtu.edu.cn
 * centos-sclo-rh: mirror.bit.edu.cn
 * centos-sclo-sclo: ftp.sjtu.edu.cn
 * elrepo: dfw.mirror.rackspace.com
 * extras: mirror.bit.edu.cn
 * updates: ftp.sjtu.edu.cn
软件包 redhat-lsb-4.1-27.el7.centos.1.x86_64 已安装并且是最新版本
软件包 check-0.9.9-5.el7.x86_64 已安装并且是最新版本
软件包 check-devel-0.9.9-5.el7.x86_64 已安装并且是最新版本
没有可用软件包 mbedtls-devel。
软件包 1:openssl-devel-1.0.2k-19.el7.x86_64 已安装并且是最新版本
没有可用软件包 python36-ply。
没有可用软件包 python36-jsonschema。
没有可用软件包 cmake3。
正在解决依赖关系
--> 正在检查事务
---> 软件包 apr-devel.x86_64.0.1.4.8-3.el7 将被 升级
---> 软件包 apr-devel.x86_64.0.1.4.8-5.el7 将被 更新
--> 正在处理依赖关系 apr = 1.4.8-5.el7，它被软件包 apr-devel-1.4.8-5.el7.x86_64 需要
---> 软件包 boost.x86_64.0.1.53.0-27.el7 将被 升级
---> 软件包 boost.x86_64.0.1.53.0-28.el7 将被 更新

此处省略一万行....

---> 软件包 rpm-sign.x86_64.0.4.11.3-43.el7 将被 更新
--> 解决依赖关系完成

依赖关系解决

===================================================================================================================
 Package                                 架构            版本                        源                       大小
===================================================================================================================
正在安装:
 chrpath                                 x86_64          0.16-0.el7                  base                     26 k
 devtoolset-9                            x86_64          9.1-0.el7                   centos-sclo-rh          5.4 k
正在更新:
 apr-devel                               x86_64          1.4.8-5.el7                 base                    188 k
 boost                                   x86_64          1.53.0-28.el7               base                     33 k
 boost-devel                             x86_64          1.53.0-28.el7               base                    7.0 M
 glibc-static                            x86_64          2.17-307.el7.1              base                    1.6 M
 libffi-devel                            x86_64          3.0.13-19.el7               base                     23 k
 
此处省略一万行....

 selinux-policy-targeted                 noarch          3.13.1-266.el7_8.1          updates                 7.0 M
 util-linux                              x86_64          2.23.2-63.el7               base                    2.0 M

事务概要
===================================================================================================================
安装   2 软件包 (+26 依赖软件包)
升级  14 软件包 (+63 依赖软件包)

总计：174 M
总下载量：159 M
Is this ok [y/d/N]: y
Downloading packages:
No Presto metadata available for base
No Presto metadata available for centos-sclo-rh
No Presto metadata available for updates
(1/65): boost-devel-1.53.0-28.el7.x86_64.rpm                                                | 7.0 MB  00:00:02     
(2/65): devtoolset-9-binutils-2.32-16.el7.x86_64.rpm                                        | 5.9 MB  00:00:37     
(3/65): devtoolset-9-libquadmath-devel-9.3.1-2.el7.x86_64.rpm                               | 171 kB  00:00:01     
此处省略一万行....
(64/65): python3-test-3.6.8-13.el7.x86_64.rpm                                               | 7.2 MB  00:02:18     
(65/65): python3-libs-3.6.8-13.el7.x86_64.rpm                                               | 7.0 MB  00:04:44     
-------------------------------------------------------------------------------------------------------------------
总计                                                                               289 kB/s | 159 MB  00:09:21     
Running transaction check
Running transaction test
Transaction test succeeded
Running transaction
警告：RPM 数据库已被非 yum 程序修改。
  正在安装    : devtoolset-9-runtime-9.1-0.el7.x86_64                                                        1/182 
  正在更新    : glibc-2.17-307.el7.1.x86_64                                                                  2/182 
此处省略一万行....
  验证中      : boost-python-1.53.0-27.el7.x86_64                                                          182/182 

已安装:
  chrpath.x86_64 0:0.16-0.el7                            devtoolset-9.x86_64 0:9.1-0.el7                           

作为依赖被安装:
  devtoolset-9-binutils.x86_64 0:2.32-16.el7                devtoolset-9-dwz.x86_64 0:0.12-1.1.el7                 
此处省略一万行....
  devtoolset-9-valgrind.x86_64 1:3.15.0-9.el7               source-highlight.x86_64 0:3.1.6-6.el7                  

更新完毕:
  apr-devel.x86_64 0:1.4.8-5.el7                              boost.x86_64 0:1.53.0-28.el7                         
此处省略一万行....
  selinux-policy-devel.noarch 0:3.13.1-266.el7_8.1            yum-utils.noarch 0:1.1.31-54.el7_8                   

作为依赖被升级:
  apr.x86_64 0:1.4.8-5.el7                             boost-atomic.x86_64 0:1.53.0-28.el7                         

此处省略一万行....
  rpm-sign.x86_64 0:4.11.3-43.el7                      selinux-policy-targeted.noarch 0:3.13.1-266.el7_8.1         
  util-linux.x86_64 0:2.23.2-63.el7                   

完毕！
已加载插件：fastestmirror, langpacks


Error getting repository data for base-debuginfo, repository not found
make: *** [install-dep] 错误 1

```



<br/>


# 4. 生成VPP（调试）
此构建版本包含调试符号，这些调试符号对于修改VPP非常有用。下面的 make命令可构建VPP的调试版本。生成调试映像时，二进制文件可以在`/ build-root / vpp_debug-native`中找到。

```
$ make build
make[1]: Entering directory '/home/vagrant/vpp-master/build-root'
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for dpdk @@@@
@@@@ Makefile fragment found in /home/vagrant/vpp-master/build-data/packages/dpdk.mk @@@@
@@@@ Source found in /home/vagrant/vpp-master/dpdk @@@@
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for vpp @@@@
@@@@ Makefile fragment found in /home/vagrant/vpp-master/build-data/packages/vpp.mk @@@@
@@@@ Source found in /home/vagrant/vpp-master/src @@@@
...
...
make[5]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp/vpp-api/java'
make[4]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp/vpp-api/java'
make[3]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp'
make[2]: Leaving directory '/home/vagrant/vpp-master/build-root/build-vpp_debug-native/vpp'
@@@@ Installing vpp: nothing to do @@@@
make[1]: Leaving directory '/home/vagrant/vpp-master/build-root'
```

我这里已经运行过这条命令，输出如下：

```
# 5. make build
make[1]: 进入目录“/opt/vpp-stable-2005/build-root”
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for external @@@@
@@@@ Makefile fragment found in /opt/vpp-stable-2005/build-data/packages/external.mk @@@@
@@@@ Source found in /opt/vpp-stable-2005/build @@@@
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for vpp @@@@
@@@@ Makefile fragment found in /opt/vpp-stable-2005/build-data/packages/vpp.mk @@@@
@@@@ Source found in /opt/vpp-stable-2005/src @@@@
@@@@ Configuring external: nothing to do @@@@
@@@@ Building external: nothing to do @@@@
@@@@ Installing external: nothing to do @@@@
@@@@ Configuring vpp: nothing to do @@@@
@@@@ Building vpp in /opt/vpp-stable-2005/build-root/build-vpp_debug-native/vpp @@@@
[0/1] Re-running CMake...
-- Looking for ccache
-- Looking for ccache - found
-- Looking for libuuid
-- Found uuid in /usr/include
-- Intel IPSecMB found: /opt/vpp/external/x86_64/include
-- dpdk plugin needs libdpdk.a library - found at /opt/vpp/external/x86_64/lib/libdpdk.a
-- Found DPDK 20.2.0 in /opt/vpp/external/x86_64/include/dpdk
-- dpdk plugin needs numa library - found at /usr/lib64/libnuma.so
-- dpdk plugin needs libIPSec_MB.a library - found at /opt/vpp/external/x86_64/lib/libIPSec_MB.a
-- Found quicly 0.1.0-vpp in /opt/vpp/external/x86_64/include
-- rdma plugin needs libibverbs.a library - found at /opt/vpp/external/x86_64/lib/libibverbs.a
-- rdma plugin needs librdma_util.a library - found at /opt/vpp/external/x86_64/lib/librdma_util.a
-- rdma plugin needs libmlx5.a library - found at /opt/vpp/external/x86_64/lib/libmlx5.a
-- tlsmbedtls plugin needs mbedtls library - found at /usr/lib64/libmbedtls.so
-- tlsmbedtls plugin needs mbedx509 library - found at /usr/lib64/libmbedx509.so
-- tlsmbedtls plugin needs mbedcrypto library - found at /usr/lib64/libmbedcrypto.so
-- Looking for picotls
-- Found picotls in /opt/vpp/external/x86_64/include and /opt/vpp/external/x86_64/lib/libpicotls-core.a
-- Configuration:
VPP version         : 20.05.1-5~gc53cb5c
VPP library version : 20.05.1
GIT toplevel dir    : /opt/vpp-stable-2005
Build type          : debug
C flags             : -Wno-address-of-packed-member -g -fPIC -Werror -Wall -march=corei7 -mtune=corei7-avx -O0 -DCLIB_DEBUG -fst
ack-protector -DFORTIFY_SOURCE=2 -fno-common Linker flags (apps) : 
Linker flags (libs) : 
Host processor      : x86_64
Target processor    : x86_64
Prefix path         : /opt/vpp/external/x86_64;/opt/vpp-stable-2005/build-root/install-vpp_debug-native/external
Install prefix      : /opt/vpp-stable-2005/build-root/install-vpp_debug-native/vpp
-- Configuring done
-- Generating done
-- Build files have been written to: /opt/vpp-stable-2005/build-root/build-vpp_debug-native/vpp
[2/2] Linking C shared library lib/vpp_plugins/test1_plugin.so
@@@@ Installing vpp @@@@
[0/1] Install the project...
-- Install configuration: "debug"
make[1]: 离开目录“/opt/vpp-stable-2005/build-root”
```

<br/>

# 5. 生成VPP（发行版）
本节介绍如何构建FD.io VPP的常规发行版。该发行版本经过优化，不会创建任何调试符号。可在/ build-root / vpp-native中找到用于构建发行映像的二进制文件。

在下面使用以下make命令来构建FD.io VPP的发行版本。

```
$ make build-release
```

我已经执行过该指令，输出如下：

```
# 6. make build-release
make[1]: 进入目录“/opt/vpp-stable-2005/build-root”
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for external @@@@
@@@@ Makefile fragment found in /opt/vpp-stable-2005/build-data/packages/external.mk @@@@
@@@@ Source found in /opt/vpp-stable-2005/build @@@@
@@@@ Arch for platform 'vpp' is native @@@@
@@@@ Finding source for vpp @@@@
@@@@ Makefile fragment found in /opt/vpp-stable-2005/build-data/packages/vpp.mk @@@@
@@@@ Source found in /opt/vpp-stable-2005/src @@@@
@@@@ Configuring external: nothing to do @@@@
@@@@ Building external: nothing to do @@@@
@@@@ Installing external: nothing to do @@@@
@@@@ Configuring vpp: nothing to do @@@@
@@@@ Building vpp: nothing to do @@@@
@@@@ Installing vpp: nothing to do @@@@
make[1]: 离开目录“/opt/vpp-stable-2005/build-root”

```

<br/>

# 6. 构建必要的软件包
需要构建的软件包取决于VPP将在其上运行的系统类型：

* 在Debian软件包如果VPP是要在Ubuntu上运行是建立
* 该RPM软件包内置如果VPP是要在CentOS或红帽运行

<br/>

# 7. 构建Debian软件包

因为我们的系统为CentOS7，不使用deb包管理工具，此章节略过。

要构建debian软件包，请使用以下命令：

```
$ make pkg-deb
```

一旦构建了软件包，就可以在build-root目录中找到它们。

```
$ ls *.deb

If the packages are built correctly, then this should be the corresponding output:

vpp_18.07-rc0~456-gb361076_amd64.deb             vpp-dbg_18.07-rc0~456-gb361076_amd64.deb
vpp-dev_18.07-rc0~456-gb361076_amd64.deb         vpp-api-lua_18.07-rc0~456-gb361076_amd64.deb
vpp-lib_18.07-rc0~456-gb361076_amd64.deb         vpp-api-python_18.07-rc0~456-gb361076_amd64.deb
vpp-plugins_18.07-rc0~456-gb361076_amd64.deb
```

最后，可以使用以下命令安装创建的软件包。安装与VPP将在其上运行的操作系统相对应的软件包：

对于Ubuntu：

```
$ sudo bash
# dpkg -i *.deb
```


<br/>

# 8. 构建RPM软件包
为了生成自己的VPP安装rpm包，执行下面命令，可以生成一系列的软件包
要生成rpm软件包，请根据系统使用以下命令之一：

```
$ make pkg-rpm

----------此处省略一万行------
Requires(rpmlib): rpmlib(CompressedFileNames) <= 3.0.4-1 rpmlib(FileDigests) <= 4.6.0-1 rpmlib(PayloadFilesHavePrefix) <= 4.0-1
Requires(post): /bin/sh libselinux-utils policycoreutils policycoreutils-python selinux-policy-base >= 3.13.1-128.6.fc22 selinux
-policy-targeted >= 3.13.1-128.6.fc22Requires(postun): /bin/sh
处理文件：vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64
Provides: vpp-debuginfo = 20.05.1-5~gc53cb5c vpp-debuginfo(x86-64) = 20.05.1-5~gc53cb5c
Requires(rpmlib): rpmlib(FileDigests) <= 4.6.0-1 rpmlib(PayloadFilesHavePrefix) <= 4.0-1 rpmlib(CompressedFileNames) <= 3.0.4-1
检查未打包文件：/usr/lib/rpm/check-files /opt/vpp-stable-2005/build-root/rpmbuild/BUILDROOT/vpp-20.05.1-5~gc53cb5c.x86_64
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-api-lua-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-api-python-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-api-python3-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm
写道:/opt/vpp-stable-2005/build-root/rpmbuild/RPMS/x86_64/vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm
执行(%clean): /bin/sh -e /var/tmp/rpm-tmp.NyscIa
+ umask 022
+ cd /opt/vpp-stable-2005/build-root/rpmbuild
+ cd vpp-20.05.1
+ /usr/bin/rm -rf /opt/vpp-stable-2005/build-root/rpmbuild/BUILDROOT/vpp-20.05.1-5~gc53cb5c.x86_64
+ exit 0
mv $(find /opt/vpp-stable-2005/build-root/rpmbuild/RPMS -name \*.rpm -type f) /opt/vpp-stable-2005/build-root
make[1]: 离开目录“/opt/vpp-stable-2005/extras/rpm”

```

我构建RPM包后，就可以在build-root目录中找到它们：

```
# cd /opt/vpp-stable-2005/build-root
[root@localhost build-root]# ls
autowank                                 vpp-20.05.1-5~gc53cb5c.x86_64.rpm
build-config.mk                          vpp-api-lua-20.05.1-4~g202978f_dirty.x86_64.rpm
build-config.mk.README                   vpp-api-lua-20.05.1-5~gc53cb5c.x86_64.rpm
build-test                               vpp-api-python-20.05.1-4~g202978f_dirty.x86_64.rpm
build-vpp_debug-native                   vpp-api-python-20.05.1-5~gc53cb5c.x86_64.rpm
build-vpp-native                         vpp-api-python3-20.05.1-4~g202978f_dirty.x86_64.rpm
config.site                              vpp-api-python3-20.05.1-5~gc53cb5c.x86_64.rpm
copyimg                                  vpp-debuginfo-20.05.1-4~g202978f_dirty.x86_64.rpm
install-vpp_debug-native                 vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm
install-vpp-native                       vpp-devel-20.05.1-4~g202978f_dirty.x86_64.rpm
Makefile                                 vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm
platforms.mk                             vpp-latest.tar.xz
rpmbuild                                 vpp-lib-20.05.1-4~g202978f_dirty.x86_64.rpm
scripts                                  vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
vagrant                                  vpp-plugins-20.05.1-4~g202978f_dirty.x86_64.rpm
vpp-20.05.1-4~g202978f-dirty.tar.xz      vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-20.05.1-4~g202978f_dirty.x86_64.rpm  vpp-selinux-policy-20.05.1-4~g202978f_dirty.x86_64.rpm
vpp-20.05.1-5~gc53cb5c.tar.xz            vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm

```

对于`Centos或Redhat`，可以使用下面的命令安装我们构建的软件包：

```
$ sudo bash
# rpm -ivh *.rpm
```

# 9. 离线安装VPP

我们的环境大多数为内网，无法连接互联网，之前在`10.170.7.166`上安装是因为配置了网络代理，所以这里对于离线安装方法的介绍是必不可少的。

## 9.1. 系统RPM包更新

在有网络环境的环境下使用yum下载更新的rpm包

```bash
yum update --downloadonly --downloaddir=./ --skip-broken
```
在我的虚拟机（10.170.6.59）下，下载的rpm包如下：
```
adobe-mappings-cmap-20171205-3.el7.noarch.rpm
adobe-mappings-cmap-deprecated-20171205-3.el7.noarch.rpm
adobe-mappings-pdf-20180407-1.el7.noarch.rpm
adwaita-cursor-theme-3.28.0-1.el7.noarch.rpm
adwaita-icon-theme-3.28.0-1.el7.noarch.rpm
bash-4.2.46-34.el7.x86_64.rpm
bison-3.0.4-2.el7.x86_64.rpm
ca-certificates-2020.2.41-70.0.el7_8.noarch.rpm
centos-release-7-8.2003.0.el7.centos.x86_64.rpm
compat-exiv2-023-0.23-2.el7.x86_64.rpm
compat-libgfortran-41-4.1.2-45.el7.x86_64.rpm
compat-libtiff3-3.9.4-12.el7.x86_64.rpm
cpio-2.11-27.el7.x86_64.rpm
cpp-4.8.5-39.el7.x86_64.rpm
dialog-1.2-5.20130523.el7.x86_64.rpm
diffutils-3.3-5.el7.x86_64.rpm
doxygen-1.8.5-4.el7.x86_64.rpm
epel-release-7-12.noarch.rpm
exiv2-0.27.0-2.el7_6.x86_64.rpm
exiv2-libs-0.27.0-2.el7_6.x86_64.rpm
expat-2.1.0-11.el7.x86_64.rpm
expat-devel-2.1.0-11.el7.x86_64.rpm
filesystem-3.2-25.el7.x86_64.rpm
findutils-4.5.11-6.el7.x86_64.rpm
flex-2.5.37-6.el7.x86_64.rpm
foomatic-filters-4.0.9-9.el7.x86_64.rpm
freeglut-3.0.0-8.el7.x86_64.rpm
freeglut-devel-3.0.0-8.el7.x86_64.rpm
fuse-devel-2.9.2-11.el7.x86_64.rpm
fuse-libs-2.9.2-11.el7.x86_64.rpm
gcc-4.8.5-39.el7.x86_64.rpm
gcc-c++-4.8.5-39.el7.x86_64.rpm
gcc-gfortran-4.8.5-39.el7.x86_64.rpm
glibc-2.17-307.el7.1.i686.rpm
glibc-2.17-307.el7.1.x86_64.rpm
glibc-common-2.17-307.el7.1.x86_64.rpm
glibc-devel-2.17-307.el7.1.i686.rpm
glibc-devel-2.17-307.el7.1.x86_64.rpm
glibc-headers-2.17-307.el7.1.x86_64.rpm
gnome-getting-started-docs-3.28.2-1.el7.noarch.rpm
gnome-user-docs-3.28.2-1.el7.noarch.rpm
info-5.1-5.el7.x86_64.rpm
iprutils-2.4.17.1-3.el7_7.x86_64.rpm
iptables-1.4.21-34.el7.x86_64.rpm
iptables-devel-1.4.21-34.el7.x86_64.rpm
iwl1000-firmware-39.31.5.1-76.el7.noarch.rpm
iwl100-firmware-39.31.5.1-76.el7.noarch.rpm
iwl105-firmware-18.168.6.1-76.el7.noarch.rpm
iwl135-firmware-18.168.6.1-76.el7.noarch.rpm
iwl2000-firmware-18.168.6.1-76.el7.noarch.rpm
iwl2030-firmware-18.168.6.1-76.el7.noarch.rpm
iwl3160-firmware-25.30.13.0-76.el7.noarch.rpm
iwl3945-firmware-15.32.2.9-76.el7.noarch.rpm
iwl4965-firmware-228.61.2.24-76.el7.noarch.rpm
iwl5000-firmware-8.83.5.1_1-76.el7.noarch.rpm
iwl5150-firmware-8.24.2.2-76.el7.noarch.rpm
iwl6000-firmware-9.221.4.1-76.el7.noarch.rpm
iwl6000g2a-firmware-18.168.6.1-76.el7.noarch.rpm
iwl6000g2b-firmware-18.168.6.1-76.el7.noarch.rpm
iwl6050-firmware-41.28.5.1-76.el7.noarch.rpm
iwl7260-firmware-25.30.13.0-76.el7.noarch.rpm
jasper-libs-1.900.1-33.el7.x86_64.rpm
kernel-headers-3.10.0-1127.19.1.el7.x86_64.rpm
kernel-tools-3.10.0-1127.19.1.el7.x86_64.rpm
kernel-tools-libs-3.10.0-1127.19.1.el7.x86_64.rpm
libatomic-4.8.5-39.el7.x86_64.rpm
libatomic-static-4.8.5-39.el7.x86_64.rpm
libexif-0.6.21-7.el7_8.x86_64.rpm
libgcc-4.8.5-39.el7.i686.rpm
libgcc-4.8.5-39.el7.x86_64.rpm
libgfortran-4.8.5-39.el7.x86_64.rpm
libgomp-4.8.5-39.el7.x86_64.rpm
libgs-9.25-2.el7_7.3.x86_64.rpm
libitm-4.8.5-39.el7.x86_64.rpm
libitm-devel-4.8.5-39.el7.x86_64.rpm
libjpeg-turbo-1.2.90-8.el7.x86_64.rpm
libjpeg-turbo-devel-1.2.90-8.el7.x86_64.rpm
libpaper-1.1.24-8.el7.x86_64.rpm
libpcap-1.5.3-12.el7.x86_64.rpm
libpfm-4.7.0-10.el7.x86_64.rpm
libpfm-devel-4.7.0-10.el7.x86_64.rpm
libproxy-0.4.11-11.el7.x86_64.rpm
libproxy-mozjs-0.4.11-11.el7.x86_64.rpm
libquadmath-4.8.5-39.el7.x86_64.rpm
libquadmath-devel-4.8.5-39.el7.x86_64.rpm
libsndfile-1.0.25-11.el7.x86_64.rpm
libstdc++-4.8.5-39.el7.i686.rpm
libstdc++-4.8.5-39.el7.x86_64.rpm
libstdc++-devel-4.8.5-39.el7.i686.rpm
libstdc++-devel-4.8.5-39.el7.x86_64.rpm
libtdb-1.3.18-1.el7.x86_64.rpm
libtevent-0.9.39-1.el7.x86_64.rpm
libtirpc-0.2.4-0.16.el7.x86_64.rpm
libvncserver-0.9.9-14.el7_8.1.x86_64.rpm
libvorbis-1.3.3-8.el7.1.x86_64.rpm
libwvstreams-4.6.1-12.el7_8.x86_64.rpm
libX11-1.6.7-2.el7.i686.rpm
libX11-1.6.7-2.el7.x86_64.rpm
libX11-common-1.6.7-2.el7.noarch.rpm
libX11-devel-1.6.7-2.el7.i686.rpm
libX11-devel-1.6.7-2.el7.x86_64.rpm
libXfont-1.5.4-1.el7.x86_64.rpm
ltrace-0.7.91-16.el7.x86_64.rpm
m17n-db-1.6.4-4.el7.noarch.rpm
man-pages-overrides-7.8.1-1.el7.x86_64.rpm
mozjs17-17.0.0-20.el7.x86_64.rpm
numactl-2.0.12-5.el7.x86_64.rpm
numactl-devel-2.0.12-5.el7.x86_64.rpm
numactl-libs-2.0.12-5.el7.x86_64.rpm
openjpeg2-2.3.1-3.el7_7.x86_64.rpm
papi-5.2.0-26.el7.x86_64.rpm
papi-devel-5.2.0-26.el7.x86_64.rpm
python3-3.6.8-13.el7.x86_64.rpm
python3-html2text-2019.8.11-1.el7.noarch.rpm
python3-libs-3.6.8-13.el7.x86_64.rpm
python3-pip-9.0.3-7.el7_7.noarch.rpm
python3-setuptools-39.2.0-10.el7.noarch.rpm
qt5-qtdoc-5.9.7-1.el7.noarch.rpm
qt5-qttranslations-5.9.7-1.el7.noarch.rpm
rcs-5.9.0-7.el7.x86_64.rpm
rfkill-0.4-10.el7.x86_64.rpm
scl-utils-20130529-19.el7.x86_64.rpm
sed-4.2.2-6.el7.x86_64.rpm
setup-2.8.71-11.el7.noarch.rpm
strace-4.24-4.el7.x86_64.rpm
tzdata-2020a-1.el7.noarch.rpm
unixODBC-2.3.1-14.el7.x86_64.rpm
unixODBC-devel-2.3.1-14.el7.x86_64.rpm
urw-base35-bookman-fonts-20170801-10.el7.noarch.rpm
urw-base35-c059-fonts-20170801-10.el7.noarch.rpm
urw-base35-d050000l-fonts-20170801-10.el7.noarch.rpm
urw-base35-fonts-20170801-10.el7.noarch.rpm
urw-base35-fonts-common-20170801-10.el7.noarch.rpm
urw-base35-gothic-fonts-20170801-10.el7.noarch.rpm
urw-base35-nimbus-mono-ps-fonts-20170801-10.el7.noarch.rpm
urw-base35-nimbus-roman-fonts-20170801-10.el7.noarch.rpm
urw-base35-nimbus-sans-fonts-20170801-10.el7.noarch.rpm
urw-base35-p052-fonts-20170801-10.el7.noarch.rpm
urw-base35-standard-symbols-ps-fonts-20170801-10.el7.noarch.rpm
urw-base35-z003-fonts-20170801-10.el7.noarch.rpm
wget-1.14-18.el7_6.1.x86_64.rpm
xkeyboard-config-2.24-1.el7.noarch.rpm
xorg-x11-font-utils-7.5-21.el7.x86_64.rpm
xorg-x11-proto-devel-2018.4-1.el7.noarch.rpm
xorg-x11-xkb-utils-7.7-14.el7.x86_64.rpm
yelp-tools-3.28.0-1.el7.noarch.rpm
yelp-xsl-3.28.0-1.el7.noarch.rpm
```

将上述rpm包拷贝至需要安装vpp的环境下（这个环境是没网的吧！），然后进行安装：

```bash
rpm -Uvh *rpm
```
输出如下：

```
[root@localhost yum-update]# rpm -Uvh *rpm
准备中...                          ################################# [100%]
	file /usr/lib64/libgs.so.9 from install of libgs-9.25-2.el7_7.3.x86_64 conflicts with file from package ghos
tscript-9.07-28.el7.x86_64	file /usr/lib64/libijs-0.35.so from install of libgs-9.25-2.el7_7.3.x86_64 conflicts with file from package 
ghostscript-9.07-28.el7.x86_64	file /usr/bin/html2text from install of python3-html2text-2019.8.11-1.el7.noarch conflicts with file from pa
ckage python2-html2text-2016.9.19-1.el7.noarch	file /usr/bin/python-html2text from install of python3-html2text-2019.8.11-1.el7.noarch conflicts with file 
from package python2-html2text-2016.9.19-1.el7.noarch	file /usr/share/man/man1/html2text.1.gz from install of python3-html2text-2019.8.11-1.el7.noarch conflicts w
ith file from package python2-html2text-2016.9.19-1.el7.noarch	file /usr/share/man/man1/python-html2text.1.gz from install of python3-html2text-2019.8.11-1.el7.noarch conf
licts with file from package python2-html2text-2016.9.19-1.el7.noarch[root@localhost yum-update]# rpm -Uvh *rpm  --force --nodeps 
准备中...                          ################################# [100%]
正在升级/安装...
   1:libgcc-4.8.5-39.el7              ################################# [  0%]

此处省略一万行....
 271:libgcc-4.8.5-36.el7_6.1          ################################# [100%]
```

## 9.2. VPP的依赖安装

这里，我下载了VPP的一些依赖rpm包，这对应VPP安装过程中的`make install-dep`指令。依赖包如下，可能不全：
```
[root@localhost vpp]# cd vpp-install-deps/
[root@localhost vpp-install-deps]# ls
audit-2.8.5-4.el7.x86_64.rpm                                 pkcs11-helper-devel-1.11-3.el7.x86_64.rpm
audit-libs-2.8.5-4.el7.x86_64.rpm                            pkgconfig-0.27.1-4.el7.x86_64.rpm
audit-libs-devel-2.8.5-4.el7.x86_64.rpm                      policycoreutils-2.5-34.el7.x86_64.rpm
audit-libs-python-2.8.5-4.el7.x86_64.rpm                     policycoreutils-devel-2.5-34.el7.x86_64.rpm
audit-libs-static-2.8.5-4.el7.x86_64.rpm                     policycoreutils-newrole-2.5-34.el7.x86_64.rpm
boost-filesystem-1.53.0-28.el7.x86_64.rpm                    policycoreutils-python-2.5-34.el7.x86_64.rpm
boost-system-1.53.0-28.el7.x86_64.rpm                        policycoreutils-restorecond-2.5-34.el7.x86_64.rpm
checkpolicy-2.5-8.el7.x86_64.rpm                             policycoreutils-sandbox-2.5-34.el7.x86_64.rpm
epel-release-7-12.noarch.rpm                                 PyQt4-4.10.1-13.el7.x86_64.rpm
glibc-2.17-307.el7.1.x86_64.rpm                              PyQt4-devel-4.10.1-13.el7.x86_64.rpm
libecap-1.0.0-1.el7.x86_64.rpm                               python-ipython-3.2.1-1.el7.noarch.rpm
libecap-devel-1.0.0-1.el7.x86_64.rpm                         python-ipython-console-3.2.1-1.el7.noarch.rpm
libffi-3.0.13-19.el7.x86_64.rpm                              python-ipython-gui-3.2.1-1.el7.noarch.rpm
libffi-devel-3.0.13-19.el7.x86_64.rpm                        README.txt
libmatchbox-1.9-15.el7.x86_64.rpm                            selinux-policy-3.13.1-266.el7.noarch.rpm
libsemanage-2.5-14.el7.x86_64.rpm                            selinux-policy-devel-3.13.1-266.el7.noarch.rpm
libsemanage-devel-2.5-14.el7.x86_64.rpm                      setools-libs-3.3.8-4.el7.x86_64.rpm
libsemanage-python-2.5-14.el7.x86_64.rpm                     setools-libs-tcl-3.3.8-4.el7.x86_64.rpm
libsemanage-static-2.5-14.el7.x86_64.rpm                     squid-migration-script-3.5.20-15.el7.x86_64.rpm
libXdmcp-1.1.2-6.el7.x86_64.rpm                              squid-sysvinit-3.5.20-15.el7.x86_64.rpm
libXfont2-2.0.3-1.el7.x86_64.rpm                             ustr-1.0.4-16.el7.x86_64.rpm
matchbox-window-manager-1.2-16.1.20070628svn.el7.x86_64.rpm  ustr-devel-1.0.4-16.el7.x86_64.rpm
mbedtls-2.7.16-2.el7.x86_64.rpm                              xcb-util-image-0.4.0-2.el7.x86_64.rpm
mbedtls-devel-2.7.16-2.el7.x86_64.rpm                        xcb-util-keysyms-0.4.0-1.el7.x86_64.rpm
mbedtls-doc-2.7.16-2.el7.noarch.rpm                          xcb-util-renderutil-0.3.9-3.el7.x86_64.rpm
mbedtls-static-2.7.16-2.el7.x86_64.rpm                       xorg-x11-server-common-1.20.4-10.el7.x86_64.rpm
mbedtls-utils-2.7.16-2.el7.x86_64.rpm                        xorg-x11-server-Xephyr-1.20.4-10.el7.x86_64.rpm
pkcs11-helper-1.11-3.el7.x86_64.rpm
```

由于版本的影响，安装过程中会提示冲突导致安装不成功，这里使用强制安装

```bash
rpm -Uvh *rpm  --force --nodeps 
```
输出如下：
```
[root@localhost vpp-install-deps]# rpm -Uvh *rpm  --force --nodeps 
准备中...                          ################################# [100%]
正在升级/安装...
   1:glibc-2.17-307.el7.1             ################################# [  1%]
   2:audit-libs-2.8.5-4.el7           ################################# [  3%]
此处省略一万行....
  68:libXfont2-2.0.1-2.el7            ################################# [100%]
```


## 9.3. VPP RPM包的安装

我再10.170.7.166服务器上成功的联网安装了VPP，利用`make pkg-rpm`生成VPP rpm包，将其拷贝至需要安装VPP的环境
我生成的VPP安装包如下：
```
vpp-20.05.1-5~gc53cb5c.tar.xz                  vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-20.05.1-5~gc53cb5c.x86_64.rpm              vpp-latest.tar.xz
vpp-api-lua-20.05.1-5~gc53cb5c.x86_64.rpm      vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python-20.05.1-5~gc53cb5c.x86_64.rpm   vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python3-20.05.1-5~gc53cb5c.x86_64.rpm  vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm
```
安装vpp-selinux-policy
```
rpm -i vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm 
```
安装vpp-debuginfo
```
rpm -i vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm 
```
安装vpp-lib
```
rpm -i vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
```
安装vpp
```
rpm -i vpp-20.05.1-5~gc53cb5c.x86_64.rpm 
```
安装vpp-plugins
```
rpm -i vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm 
```
安装vpp-devel
```
rpm -i vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm 
```

安装VPP依赖
```
rpm -Uvh vpp-ext-deps-20.05-11.x86_64.rpm
```


安装过程中的一些log如下：
```

[root@localhost vpp-pkg-rpm]# rpm -i vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm 
错误：依赖检测失败：
	libnat.so.20.05.1()(64bit) 被 vpp-plugins-20.05.1-5~gc53cb5c.x86_64 需要
	libvatclient.so.20.05.1()(64bit) 被 vpp-plugins-20.05.1-5~gc53cb5c.x86_64 需要
	vpp = 20.05.1-5~gc53cb5c 被 vpp-plugins-20.05.1-5~gc53cb5c.x86_64 需要
[root@localhost vpp-pkg-rpm]# rpm -i vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm 
[root@localhost vpp-pkg-rpm]# rpm -i vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm 
[root@localhost vpp-pkg-rpm]# rpm -i *rpm
	软件包 vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64 已经安装
[root@localhost vpp-pkg-rpm]# rpm -Uvh *rpm
准备中...                          ################################# [100%]
	软件包 vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64 已经安装
[root@localhost vpp-pkg-rpm]# rpm -i vpp-20.05.1-5~gc53cb5c.x86_64.rpm 
错误：依赖检测失败：
	libsvm.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libsvmdb.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libvatplugin.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libvlib.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libvlibmemory.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libvlibmemoryclient.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libvnet.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libvppapiclient.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	libvppinfra.so.20.05.1()(64bit) 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
	vpp-lib = 20.05.1-5~gc53cb5c 被 vpp-20.05.1-5~gc53cb5c.x86_64 需要
[root@localhost vpp-pkg-rpm]# rpm -i vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm 
[root@localhost vpp-pkg-rpm]# rpm -i vpp-
vpp-20.05.1-5~gc53cb5c.x86_64.rpm                 vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-lua-20.05.1-5~gc53cb5c.x86_64.rpm         vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python-20.05.1-5~gc53cb5c.x86_64.rpm      vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python3-20.05.1-5~gc53cb5c.x86_64.rpm     vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm       
[root@localhost vpp-pkg-rpm]# rpm -i vpp-api-*
错误：依赖检测失败：
	vpp = 20.05.1-5~gc53cb5c 被 vpp-api-lua-20.05.1-5~gc53cb5c.x86_64 需要
	vpp = 20.05.1-5~gc53cb5c 被 vpp-api-python-20.05.1-5~gc53cb5c.x86_64 需要
	vpp = 20.05.1-5~gc53cb5c 被 vpp-api-python3-20.05.1-5~gc53cb5c.x86_64 需要
[root@localhost vpp-pkg-rpm]# rpm -i vpp-20.05.1-5~gc53cb5c.x86_64.rpm 
* Applying /usr/lib/sysctl.d/00-system.conf ...
* Applying /usr/lib/sysctl.d/10-default-yama-scope.conf ...
kernel.yama.ptrace_scope = 0
* Applying /usr/lib/sysctl.d/50-default.conf ...
kernel.sysrq = 16
kernel.core_uses_pid = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.promote_secondaries = 1
net.ipv4.conf.all.promote_secondaries = 1
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
* Applying /usr/lib/sysctl.d/60-libvirtd.conf ...
fs.aio-max-nr = 1048576
* Applying /etc/sysctl.d/80-vpp.conf ...
vm.nr_hugepages = 1024
vm.max_map_count = 3096
vm.hugetlb_shm_group = 0
kernel.shmmax = 2147483648
* Applying /etc/sysctl.d/99-sysctl.conf ...
* Applying /etc/sysctl.conf ...
[root@localhost vpp-pkg-rpm]# rpm -i vpp-
vpp-20.05.1-5~gc53cb5c.x86_64.rpm                 vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-lua-20.05.1-5~gc53cb5c.x86_64.rpm         vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python-20.05.1-5~gc53cb5c.x86_64.rpm      vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python3-20.05.1-5~gc53cb5c.x86_64.rpm     vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm       
[root@localhost vpp-pkg-rpm]# rpm -i vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm 
[root@localhost vpp-pkg-rpm]# rpm -i vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm 
[root@localhost vpp-pkg-rpm]# rpm -i vpp-api-*
(reverse-i-search)`fi': ldcon^Cg 
[root@localhost vpp-pkg-rpm]# ^C
[root@localhost vpp-pkg-rpm]# ^C
[root@localhost vpp-pkg-rpm]# files=`ls *.rpm`;for file in $files; do rpm -i $file; done
	软件包 vpp-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-api-lua-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-api-python-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-api-python3-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-devel-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-lib-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-plugins-20.05.1-5~gc53cb5c.x86_64 已经安装
	软件包 vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64 已经安装
[root@localhost vpp-pkg-rpm]# 
[root@localhost vpp-pkg-rpm]# vpp
vpp                    vpp_api_test           vpp_echo               vpp_get_stats          vpp_restart
vppapigen              vppctl                 vpp_get_metrics        vpp_prometheus_export  
[root@localhost vpp-pkg-rpm]# vpp
vpp                    vpp_api_test           vpp_echo               vpp_get_stats          vpp_restart
vppapigen              vppctl                 vpp_get_metrics        vpp_prometheus_export  
[root@localhost vpp-pkg-rpm]# ls
vpp-20.05.1-5~gc53cb5c.tar.xz                  vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-20.05.1-5~gc53cb5c.x86_64.rpm              vpp-latest.tar.xz
vpp-api-lua-20.05.1-5~gc53cb5c.x86_64.rpm      vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python-20.05.1-5~gc53cb5c.x86_64.rpm   vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python3-20.05.1-5~gc53cb5c.x86_64.rpm  vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm
[root@localhost vpp-pkg-rpm]# 
[root@localhost vpp-pkg-rpm]# ls
vpp-20.05.1-5~gc53cb5c.tar.xz                  vpp-devel-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-20.05.1-5~gc53cb5c.x86_64.rpm              vpp-latest.tar.xz
vpp-api-lua-20.05.1-5~gc53cb5c.x86_64.rpm      vpp-lib-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python-20.05.1-5~gc53cb5c.x86_64.rpm   vpp-plugins-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-api-python3-20.05.1-5~gc53cb5c.x86_64.rpm  vpp-selinux-policy-20.05.1-5~gc53cb5c.x86_64.rpm
vpp-debuginfo-20.05.1-5~gc53cb5c.x86_64.rpm
[root@localhost vpp-pkg-rpm]# cd ..
[root@localhost vpp]# ls
vpp-install-deps  vpp-pkg-rpm  yum-update
[root@localhost vpp]# 
[root@localhost vpp]# ls
```

<br/>
<div align=right>	以上内容由荣涛编写，或翻译、整理自网络。</div>