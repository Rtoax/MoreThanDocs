<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档 构建VPP</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>


要开始使用VPP进行开发，您需要获取所需的VPP源代码，然后构建软件包。

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

```# 5. make build
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
   2:xorg-x11-proto-devel-2018.4-1.el7################################# [  1%]
   3:libX11-common-1.6.7-2.el7        ################################# [  1%]
   4:centos-release-7-8.2003.0.el7.cen################################# [  1%]
   5:adobe-mappings-cmap-20171205-3.el################################# [  2%]
   6:adobe-mappings-cmap-deprecated-20################################# [  2%]
   7:setup-2.8.71-11.el7              警告：/etc/group 已建立为 /etc/group.rpmnew 
警告：/etc/profile 已建立为 /etc/profile.rpmnew 
################################# [  3%]
警告：/etc/shadow 已建立为 /etc/shadow.rpmnew 
警告：/etc/shells 已建立为 /etc/shells.rpmnew 
/usr/bin/newaliases: error while loading shared libraries: libmysqlclient.so.18: cannot open shared object file: No 
such file or directory   8:filesystem-3.2-25.el7            ################################# [  3%]
   9:urw-base35-fonts-common-20170801-################################# [  3%]
  10:yelp-xsl-3.28.0-1.el7            ################################# [  4%]
  11:tzdata-2020a-1.el7               ################################# [  4%]
  12:glibc-2.17-307.el7.1             警告：/etc/nsswitch.conf 已建立为 /etc/nsswitch.conf.rpmnew 
################################# [  4%]
  13:bash-4.2.46-34.el7               ################################# [  5%]
  14:glibc-common-2.17-307.el7.1      ################################# [  5%]
  15:info-5.1-5.el7                   ################################# [  6%]
  16:libstdc++-4.8.5-39.el7           ################################# [  6%]
  17:xorg-x11-font-utils-1:7.5-21.el7 ################################# [  6%]
  18:expat-2.1.0-11.el7               ################################# [  7%]
  19:libjpeg-turbo-1.2.90-8.el7       ################################# [  7%]
  20:libX11-1.6.7-2.el7               ################################# [  7%]
  21:libquadmath-4.8.5-39.el7         ################################# [  8%]
  22:libpfm-4.7.0-10.el7              ################################# [  8%]
  23:numactl-libs-2.0.12-5.el7        ################################# [  8%]
  24:papi-5.2.0-26.el7                ################################# [  9%]
  25:libgfortran-4.8.5-39.el7         ################################# [  9%]
  26:freeglut-3.0.0-8.el7             ################################# [ 10%]
  27:exiv2-0.27.0-2.el7_6             ################################# [ 10%]
  28:exiv2-libs-0.27.0-2.el7_6        ################################# [ 10%]
  29:urw-base35-bookman-fonts-20170801################################# [ 11%]
  30:urw-base35-c059-fonts-20170801-10################################# [ 11%]
  31:urw-base35-d050000l-fonts-2017080################################# [ 11%]
  32:urw-base35-gothic-fonts-20170801-################################# [ 12%]
  33:urw-base35-nimbus-mono-ps-fonts-2################################# [ 12%]
  34:urw-base35-nimbus-roman-fonts-201################################# [ 13%]
  35:urw-base35-nimbus-sans-fonts-2017################################# [ 13%]
  36:urw-base35-p052-fonts-20170801-10################################# [ 13%]
  37:urw-base35-standard-symbols-ps-fo################################# [ 14%]
  38:urw-base35-z003-fonts-20170801-10################################# [ 14%]
  39:urw-base35-fonts-20170801-10.el7 ################################# [ 14%]
  40:libproxy-0.4.11-11.el7           ################################# [ 15%]
  41:mozjs17-17.0.0-20.el7            ################################# [ 15%]
  42:cpp-4.8.5-39.el7                 ################################# [ 15%]
  43:diffutils-3.3-5.el7              ################################# [ 16%]
  44:libatomic-4.8.5-39.el7           ################################# [ 16%]
  45:libgomp-4.8.5-39.el7             ################################# [ 17%]
  46:libitm-4.8.5-39.el7              ################################# [ 17%]
  47:iptables-1.4.21-34.el7           ################################# [ 17%]
  48:libpaper-1.1.24-8.el7            ################################# [ 18%]
  49:fuse-libs-2.9.2-11.el7           ################################# [ 18%]
  50:kernel-tools-libs-3.10.0-1127.19.################################# [ 18%]
  51:libtirpc-0.2.4-0.16.el7          ################################# [ 19%]
  52:python3-pip-9.0.3-7.el7_7        ################################# [ 19%]
  53:python3-setuptools-39.2.0-10.el7 ################################# [ 20%]
  54:python3-3.6.8-13.el7             ################################# [ 20%]
  55:python3-libs-3.6.8-13.el7        ################################# [ 20%]
  56:libvorbis-1:1.3.3-8.el7.1        ################################# [ 21%]
  57:openjpeg2-2.3.1-3.el7_7          ################################# [ 21%]
  58:unixODBC-2.3.1-14.el7            ################################# [ 21%]
  59:kernel-headers-3.10.0-1127.19.1.e################################# [ 22%]
  60:glibc-headers-2.17-307.el7.1     ################################# [ 22%]
  61:gnome-user-docs-3.28.2-1.el7     ################################# [ 23%]
  62:adwaita-cursor-theme-3.28.0-1.el7################################# [ 23%]
  63:adobe-mappings-pdf-20180407-1.el7################################# [ 23%]
  64:libgs-9.25-2.el7_7.3             ################################# [ 24%]
  65:adwaita-icon-theme-3.28.0-1.el7  ################################# [ 24%]
  66:gnome-getting-started-docs-3.28.2################################# [ 24%]
  67:glibc-devel-2.17-307.el7.1       ################################# [ 25%]
  68:unixODBC-devel-2.3.1-14.el7      ################################# [ 25%]
  69:libsndfile-1.0.25-11.el7         ################################# [ 25%]
  70:python3-html2text-2019.8.11-1.el7################################# [ 26%]
  71:kernel-tools-3.10.0-1127.19.1.el7################################# [ 26%]
  72:fuse-devel-2.9.2-11.el7          ################################# [ 27%]
  73:iptables-devel-1.4.21-34.el7     ################################# [ 27%]
  74:libatomic-static-4.8.5-39.el7    ################################# [ 27%]
  75:rcs-5.9.0-7.el7                  ################################# [ 28%]
  76:libproxy-mozjs-0.4.11-11.el7     ################################# [ 28%]
  77:freeglut-devel-3.0.0-8.el7       ################################# [ 28%]
  78:papi-devel-5.2.0-26.el7          ################################# [ 29%]
  79:numactl-2.0.12-5.el7             ################################# [ 29%]
  80:numactl-devel-2.0.12-5.el7       ################################# [ 30%]
  81:libpfm-devel-4.7.0-10.el7        ################################# [ 30%]
  82:xorg-x11-xkb-utils-7.7-14.el7    ################################# [ 30%]
  83:compat-libtiff3-3.9.4-12.el7     ################################# [ 31%]
  84:jasper-libs-1.900.1-33.el7       ################################# [ 31%]
  85:libjpeg-turbo-devel-1.2.90-8.el7 ################################# [ 31%]
  86:libvncserver-0.9.9-14.el7_8.1    ################################# [ 32%]
  87:compat-exiv2-023-0.23-2.el7      ################################# [ 32%]
  88:expat-devel-2.1.0-11.el7         ################################# [ 32%]
  89:doxygen-1:1.8.5-4.el7            ################################# [ 33%]
  90:libstdc++-devel-4.8.5-39.el7     ################################# [ 33%]
  91:libwvstreams-4.6.1-12.el7_8      ################################# [ 34%]
  92:ltrace-0.7.91-16.el7             ################################# [ 34%]
  93:bison-3.0.4-2.el7                ################################# [ 34%]
  94:cpio-2.11-27.el7                 ################################# [ 35%]
  95:findutils-1:4.5.11-6.el7         ################################# [ 35%]
  96:flex-2.5.37-6.el7                ################################# [ 35%]
  97:sed-4.2.2-6.el7                  ################################# [ 36%]
  98:wget-1.14-18.el7_6.1             ################################# [ 36%]
  99:ca-certificates-2020.2.41-70.0.el################################# [ 37%]
 100:iprutils-2.4.17.1-3.el7_7        ################################# [ 37%]
 101:scl-utils-20130529-19.el7        ################################# [ 37%]
 102:strace-4.24-4.el7                ################################# [ 38%]
 103:yelp-tools-3.28.0-1.el7          ################################# [ 38%]
 104:compat-libgfortran-41-4.1.2-45.el################################# [ 38%]
 105:dialog-1.2-5.20130523.el7        ################################# [ 39%]
 106:foomatic-filters-4.0.9-9.el7     ################################# [ 39%]
 107:libexif-0.6.21-7.el7_8           ################################# [ 39%]
 108:libpcap-14:1.5.3-12.el7          ################################# [ 40%]
 109:libtdb-1.3.18-1.el7              ################################# [ 40%]
 110:libtevent-0.9.39-1.el7           ################################# [ 41%]
 111:libXfont-1.5.4-1.el7             ################################# [ 41%]
 112:rfkill-0.4-10.el7                ################################# [ 41%]
 113:epel-release-7-12                ################################# [ 42%]
 114:xkeyboard-config-2.24-1.el7      ################################# [ 42%]
 115:qt5-qttranslations-5.9.7-1.el7   ################################# [ 42%]
 116:qt5-qtdoc-5.9.7-1.el7            ################################# [ 43%]
 117:man-pages-overrides-7.8.1-1.el7  ################################# [ 43%]
 118:m17n-db-1.6.4-4.el7              ################################# [ 44%]
 119:iwl7260-firmware-25.30.13.0-76.el################################# [ 44%]
 120:iwl6050-firmware-41.28.5.1-76.el7################################# [ 44%]
 121:iwl6000g2b-firmware-18.168.6.1-76################################# [ 45%]
 122:iwl6000g2a-firmware-18.168.6.1-76################################# [ 45%]
 123:iwl6000-firmware-9.221.4.1-76.el7################################# [ 45%]
 124:iwl5150-firmware-8.24.2.2-76.el7 ################################# [ 46%]
 125:iwl5000-firmware-8.83.5.1_1-76.el################################# [ 46%]
 126:iwl4965-firmware-228.61.2.24-76.e################################# [ 46%]
 127:iwl3945-firmware-15.32.2.9-76.el7################################# [ 47%]
 128:iwl3160-firmware-25.30.13.0-76.el################################# [ 47%]
 129:iwl2030-firmware-18.168.6.1-76.el################################# [ 48%]
 130:iwl2000-firmware-18.168.6.1-76.el################################# [ 48%]
 131:iwl135-firmware-18.168.6.1-76.el7################################# [ 48%]
 132:iwl105-firmware-18.168.6.1-76.el7################################# [ 49%]
 133:iwl100-firmware-39.31.5.1-76.el7 ################################# [ 49%]
 134:iwl1000-firmware-1:39.31.5.1-76.e################################# [ 49%]
 135:glibc-2.17-307.el7.1             ################################# [ 50%]
 136:glibc-devel-2.17-307.el7.1       ################################# [ 50%]
 137:gcc-4.8.5-39.el7                 ################################# [ 51%]
 138:libquadmath-devel-4.8.5-39.el7   ################################# [ 51%]
 139:libX11-1.6.7-2.el7               ################################# [ 51%]
 140:libX11-devel-1.6.7-2.el7         ################################# [ 52%]
 141:libgcc-4.8.5-39.el7              ################################# [ 52%]
 142:libX11-devel-1.6.7-2.el7         ################################# [ 52%]
 143:gcc-gfortran-4.8.5-39.el7        ################################# [ 53%]
 144:libitm-devel-4.8.5-39.el7        ################################# [ 53%]
 145:libstdc++-4.8.5-39.el7           ################################# [ 54%]
 146:libstdc++-devel-4.8.5-39.el7     ################################# [ 54%]
 147:gcc-c++-4.8.5-39.el7             ################################# [ 54%]
正在清理/删除...
 148:glibc-devel-2.17-260.el7_6.3     ################################# [ 55%]
 149:libX11-devel-1.6.5-2.el7         ################################# [ 55%]
 150:libX11-devel-1.6.5-2.el7         ################################# [ 55%]
 151:libX11-1.6.5-2.el7               ################################# [ 56%]
 152:yelp-tools-3.18.0-1.el7          ################################# [ 56%]
 153:urw-fonts-2.4-16.el7             ################################# [ 56%]
 154:libitm-devel-4.8.5-36.el7_6.1    ################################# [ 57%]
 155:adwaita-icon-theme-3.22.0-1.el7  ################################# [ 57%]
 156:gcc-gfortran-4.8.5-36.el7_6.1    ################################# [ 58%]
 157:libproxy-mozjs-0.4.11-10.el7     ################################# [ 58%]
 158:gcc-c++-4.8.5-36.el7_6.1         ################################# [ 58%]
 159:exiv2-libs-0.23-6.el7            ################################# [ 59%]
 160:compat-libtiff3-3.9.4-11.el7     ################################# [ 59%]
 161:mozjs17-17.0.0-19.el7            ################################# [ 59%]
 162:libproxy-0.4.11-10.el7           ################################# [ 60%]
 163:libgfortran-4.8.5-36.el7_6.1     ################################# [ 60%]
 164:libwvstreams-4.6.1-11.el7        ################################# [ 61%]
 165:kernel-tools-3.10.0-693.el7      ################################# [ 61%]
 166:doxygen-1:1.8.5-3.el7            ################################# [ 61%]
 167:strace-4.12-4.el7                ################################# [ 62%]
 168:rcs-5.9.0-5.el7                  ################################# [ 62%]
 169:ltrace-0.7.91-14.el7             ################################# [ 62%]
 170:diffutils-3.3-4.el7              ################################# [ 63%]
 171:libitm-4.8.5-36.el7_6.1          ################################# [ 63%]
 172:xorg-x11-font-utils-1:7.5-20.el7 ################################# [ 63%]
 173:wget-1.14-15.el7                 ################################# [ 64%]
 174:sed-4.2.2-5.el7                  ################################# [ 64%]
 175:numactl-2.0.9-6.el7_2            ################################# [ 65%]
 176:libvncserver-0.9.9-9.el7_0.1     ################################# [ 65%]
 177:libsndfile-1.0.25-10.el7         ################################# [ 65%]
 178:jasper-libs-1.900.1-31.el7       ################################# [ 66%]
 179:flex-2.5.37-3.el7                ################################# [ 66%]
 180:findutils-1:4.5.11-5.el7         ################################# [ 66%]
 181:cpio-2.11-24.el7                 ################################# [ 67%]
 182:bison-3.0.4-1.el7                ################################# [ 67%]
 183:libquadmath-devel-4.8.5-36.el7_6.################################# [ 68%]
 184:libstdc++-devel-4.8.5-36.el7_6.1 ################################# [ 68%]
 185:unixODBC-devel-2.3.1-11.el7      ################################# [ 68%]
 186:papi-devel-5.2.0-23.el7          ################################# [ 69%]
 187:numactl-devel-2.0.9-6.el7_2      ################################# [ 69%]
 188:libstdc++-devel-4.8.5-36.el7_6.1 ################################# [ 69%]
 189:libstdc++-4.8.5-36.el7_6.1       ################################# [ 70%]
 190:libpfm-devel-4.7.0-4.el7         ################################# [ 70%]
 191:libjpeg-turbo-devel-1.2.90-6.el7 ################################# [ 70%]
 192:libatomic-static-4.8.5-16.el7    ################################# [ 71%]
 193:iptables-devel-1.4.21-18.0.1.el7.################################# [ 71%]
 194:gnome-getting-started-docs-3.22.0################################# [ 72%]
 195:fuse-devel-2.9.2-8.el7           ################################# [ 72%]
 196:freeglut-devel-2.8.1-3.el7       ################################# [ 72%]
 197:expat-devel-2.1.0-10.el7_3       ################################# [ 73%]
 198:epel-release-7-11                ################################# [ 73%]
 199:ca-certificates-2017.2.14-71.el7 ################################# [ 73%]
 200:gcc-4.8.5-36.el7_6.1             ################################# [ 74%]
 201:glibc-devel-2.17-260.el7_6.3     ################################# [ 74%]
 202:cpp-4.8.5-36.el7_6.1             ################################# [ 75%]
 203:glibc-headers-2.17-260.el7_6.3   ################################# [ 75%]
 204:glibc-2.17-260.el7_6.3           ################################# [ 75%]
 205:libgomp-4.8.5-36.el7_6.1         ################################# [ 76%]
 206:freeglut-2.8.1-3.el7             ################################# [ 76%]
 207:numactl-libs-2.0.9-6.el7_2       ################################# [ 76%]
 208:papi-5.2.0-23.el7                ################################# [ 77%]
 209:libstdc++-4.8.5-36.el7_6.1       ################################# [ 77%]
 210:libquadmath-4.8.5-36.el7_6.1     ################################# [ 77%]
 211:iptables-1.4.21-18.0.1.el7.centos################################# [ 78%]
 212:libatomic-4.8.5-16.el7           ################################# [ 78%]
 213:info-5.1-4.el7                   ################################# [ 79%]
 214:xorg-x11-xkb-utils-7.7-12.el7    ################################# [ 79%]
 215:libX11-1.6.5-2.el7               ################################# [ 79%]
 216:scl-utils-20130529-17.el7_1      ################################# [ 80%]
 217:iprutils-2.4.14.1-1.el7          ################################# [ 80%]
 218:libpfm-4.7.0-4.el7               ################################# [ 80%]
 219:filesystem-3.2-21.el7            ################################# [ 81%]
 220:setup-2.8.71-7.el7               ################################# [ 81%]
 221:expat-2.1.0-10.el7_3             ################################# [ 82%]
 222:fuse-libs-2.9.2-8.el7            ################################# [ 82%]
 223:libjpeg-turbo-1.2.90-6.el7       ################################# [ 82%]
 224:unixODBC-2.3.1-11.el7            ################################# [ 83%]
 225:libvorbis-1:1.3.3-8.el7          ################################# [ 83%]
 226:libXfont-1.5.2-1.el7             ################################# [ 83%]
 227:kernel-tools-libs-3.10.0-693.el7 ################################# [ 84%]
 228:rfkill-0.4-9.el7                 ################################# [ 84%]
 229:libtirpc-0.2.4-0.10.el7          ################################# [ 85%]
 230:libtevent-0.9.31-1.el7           ################################# [ 85%]
 231:libtdb-1.3.12-2.el7              ################################# [ 85%]
 232:libpcap-14:1.5.3-9.el7           ################################# [ 86%]
 233:libexif-0.6.21-6.el7             ################################# [ 86%]
 234:foomatic-filters-4.0.9-8.el7     ################################# [ 86%]
 235:dialog-1.2-4.20130523.el7        ################################# [ 87%]
 236:compat-libgfortran-41-4.1.2-44.el################################# [ 87%]
 237:centos-release-7-4.1708.el7.cento################################# [ 87%]
 238:libX11-common-1.6.5-2.el7        ################################# [ 88%]
 239:kernel-headers-3.10.0-693.el7    ################################# [ 88%]
 240:gnome-user-docs-3.22.0-1.el7     ################################# [ 89%]
 241:libgcc-4.8.5-36.el7_6.1          ################################# [ 89%]
 242:adwaita-cursor-theme-3.22.0-1.el7################################# [ 89%]
 243:yelp-xsl-3.20.1-1.el7            ################################# [ 90%]
 244:xorg-x11-proto-devel-7.7-20.el7  ################################# [ 90%]
 245:xkeyboard-config-2.20-1.el7      ################################# [ 90%]
 246:qt5-qttranslations-5.6.2-1.el7   ################################# [ 91%]
 247:qt5-qtdoc-5.6.2-1.el7            ################################# [ 91%]
 248:man-pages-overrides-7.4.3-1.el7  ################################# [ 92%]
 249:m17n-db-1.6.4-3.el7              ################################# [ 92%]
 250:iwl7265-firmware-22.0.7.0-56.el7 ################################# [ 92%]
 251:iwl7260-firmware-22.0.7.0-56.el7 ################################# [ 93%]
 252:iwl6050-firmware-41.28.5.1-56.el7################################# [ 93%]
 253:iwl6000g2b-firmware-17.168.5.2-56################################# [ 93%]
 254:iwl6000g2a-firmware-17.168.5.3-56################################# [ 94%]
 255:iwl6000-firmware-9.221.4.1-56.el7################################# [ 94%]
 256:iwl5150-firmware-8.24.2.2-56.el7 ################################# [ 94%]
 257:iwl5000-firmware-8.83.5.1_1-56.el################################# [ 95%]
 258:iwl4965-firmware-228.61.2.24-56.e################################# [ 95%]
 259:iwl3945-firmware-15.32.2.9-56.el7################################# [ 96%]
 260:iwl3160-firmware-22.0.7.0-56.el7 ################################# [ 96%]
 261:iwl2030-firmware-18.168.6.1-56.el################################# [ 96%]
 262:iwl2000-firmware-18.168.6.1-56.el################################# [ 97%]
 263:iwl135-firmware-18.168.6.1-56.el7################################# [ 97%]
 264:iwl105-firmware-18.168.6.1-56.el7################################# [ 97%]
 265:iwl100-firmware-39.31.5.1-56.el7 ################################# [ 98%]
 266:iwl1000-firmware-1:39.31.5.1-56.e################################# [ 98%]
 267:glibc-common-2.17-260.el7_6.3    ################################# [ 99%]
 268:bash-4.2.46-28.el7               ################################# [ 99%]
 269:glibc-2.17-260.el7_6.3           ################################# [ 99%]
 270:tzdata-2017b-1.el7               ################################# [100%]
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
   3:pkgconfig-1:0.27.1-4.el7         ################################# [  4%]
   4:pkcs11-helper-1.11-3.el7         ################################# [  6%]
   5:ustr-1.0.4-16.el7                ################################# [  7%]
   6:libsemanage-2.5-14.el7           ################################# [  9%]
   7:policycoreutils-2.5-34.el7       ################################# [ 10%]
   8:mbedtls-2.7.16-2.el7             ################################# [ 12%]
   9:checkpolicy-2.5-8.el7            ################################# [ 13%]
  10:PyQt4-4.10.1-13.el7              ################################# [ 15%]
  11:setools-libs-3.3.8-4.el7         ################################# [ 16%]
  12:python-ipython-console-3.2.1-1.el################################# [ 18%]
  13:python-ipython-gui-3.2.1-1.el7   ################################# [ 19%]
  14:mbedtls-devel-2.7.16-2.el7       ################################# [ 21%]
  15:selinux-policy-3.13.1-266.el7    ################################# [ 22%]
  16:libsemanage-python-2.5-14.el7    ################################# [ 24%]
  17:ustr-devel-1.0.4-16.el7          ################################# [ 25%]
  18:libsemanage-devel-2.5-14.el7     ################################# [ 26%]
  19:audit-libs-python-2.8.5-4.el7    ################################# [ 28%]
  20:policycoreutils-python-2.5-34.el7################################# [ 29%]
  21:policycoreutils-devel-2.5-34.el7 ################################# [ 31%]
  22:selinux-policy-devel-3.13.1-266.e################################# [ 32%]
  23:boost-system-1.53.0-28.el7       ################################# [ 34%]
  24:libecap-1.0.0-1.el7              ################################# [ 35%]
  25:libffi-3.0.13-19.el7             ################################# [ 37%]
  26:libmatchbox-1.9-15.el7           ################################# [ 38%]
  27:matchbox-window-manager-1.2-16.1.################################# [ 40%]
  28:libXdmcp-1.1.2-6.el7             ################################# [ 41%]
  29:libXfont2-2.0.3-1.el7            ################################# [ 43%]
  30:xcb-util-image-0.4.0-2.el7       ################################# [ 44%]
  31:xcb-util-keysyms-0.4.0-1.el7     ################################# [ 46%]
  32:xcb-util-renderutil-0.3.9-3.el7  ################################# [ 47%]
  33:xorg-x11-server-common-1.20.4-10.################################# [ 49%]
  34:xorg-x11-server-Xephyr-1.20.4-10.################################# [ 50%]
  35:policycoreutils-sandbox-2.5-34.el################################# [ 51%]
  36:libffi-devel-3.0.13-19.el7       ################################# [ 53%]
  37:libecap-devel-1.0.0-1.el7        ################################# [ 54%]
  38:boost-filesystem-1.53.0-28.el7   ################################# [ 56%]
  39:libsemanage-static-2.5-14.el7    ################################# [ 57%]
  40:mbedtls-static-2.7.16-2.el7      ################################# [ 59%]
  41:python-ipython-3.2.1-1.el7       ################################# [ 60%]
  42:setools-libs-tcl-3.3.8-4.el7     ################################# [ 62%]
  43:PyQt4-devel-4.10.1-13.el7        ################################# [ 63%]
  44:mbedtls-utils-2.7.16-2.el7       ################################# [ 65%]
  45:policycoreutils-newrole-2.5-34.el################################# [ 66%]
  46:pkcs11-helper-devel-1.11-3.el7   ################################# [ 68%]
  47:audit-libs-devel-2.8.5-4.el7     ################################# [ 69%]
  48:audit-2.8.5-4.el7                ################################# [ 71%]
  49:policycoreutils-restorecond-2.5-3################################# [ 72%]
  50:squid-sysvinit-7:3.5.20-15.el7   ################################# [ 74%]
  51:squid-migration-script-7:3.5.20-1################################# [ 75%]
  52:mbedtls-doc-2.7.16-2.el7         ################################# [ 76%]
  53:epel-release-7-12                ################################# [ 78%]
  54:audit-libs-static-2.8.5-4.el7    ################################# [ 79%]
正在清理/删除...
  55:selinux-policy-3.13.1-229.el7_6.9################################# [ 81%]
  56:libffi-devel-3.0.13-18.el7       ################################# [ 82%]
  57:audit-libs-devel-2.7.6-3.el7     ################################# [ 84%]
  58:policycoreutils-python-2.5-29.el7################################# [ 85%]
  59:audit-libs-python-2.7.6-3.el7    ################################# [ 87%]
  60:policycoreutils-2.5-29.el7_6.1   ################################# [ 88%]
  61:boost-filesystem-1.53.0-27.el7   ################################# [ 90%]
  62:audit-2.7.6-3.el7                ################################# [ 91%]
  63:xorg-x11-server-common-1.20.1-5.3################################# [ 93%]
  64:audit-libs-2.7.6-3.el7           ################################# [ 94%]
  65:boost-system-1.53.0-27.el7       ################################# [ 96%]
  66:checkpolicy-2.5-4.el7            ################################# [ 97%]
  67:libffi-3.0.13-18.el7             ################################# [ 99%]
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
<div align=right>	以上内容由荣涛翻译整理自网络。</div>