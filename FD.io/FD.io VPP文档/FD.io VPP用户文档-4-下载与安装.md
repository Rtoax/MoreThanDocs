<div align=center>
	<img src="_v_images/20200904171558212_22234.png" width="300"> 
</div>

<br/>
<br/>
<br/>

<center><font size='20'>FD.io VPP：用户文档 VPP的下载与安装</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2020年9月</font></center>
<br/>
<br/>
<br/>
<br/>



# 1. 引言

如果要使用VPP，可以方便地从现有软件包中安装二进制文件。本指南介绍了如何提取，安装和运行VPP软件包。

本节提供有关如何在Ubuntu，Centos和openSUSE平台上安装VPP二进制文件的说明。

使用Package Cloud安装FD.io VPP。有关如何使用程序包云安装VPP的完整说明，请参阅程序包云。

**这里需要说明的是，因为我们公司内部不能连接互联网，起初我采用本节方案进行安装（添加yum源，yum install vpp*的方案），但后续由于dpdk的plugin加载不上，遂采用了源代码安装。**

<br/>

# 2. 在Ubuntu上安装

**`略`**

<br/>

# 3. 在Centos上安装

这部分会单独出一份文档，这里简单介绍官网的安装方法，这种方法没有配置DPDK插件。
[CentOS7安装VPP](https://fd.io/docs/vpp/master/gettingstarted/installing/centos.html)

<br/>

# 4. 更新操作系统

在开始安装存储库之前，最好先更新和升级操作系统。运行以下命令以更新操作系统并获取一些软件包。

```bash
$ sudo yum update
$ sudo yum install pygpgme yum-utils
```

<br/>

# 5. 软件包云存储库

构建工件也将发布到`packagecloud.io`存储库中。这包括官方的发行点。要使用这些构建工件中的任何一个，请创建一个文件 “ `/etc/yum.repos.d/fdio-release.repo`”，其内容指向所需的版本。以下是所需内容的一些常见示例：

<br/>

# 6. VPP最新版本
要允许“ yum”访问官方VPP版本，请创建包含以下内容的文件 “ /etc/yum.repos.d/fdio-release.repo”。

```bash
$ cat /etc/yum.repos.d/fdio-release.repo
[fdio_release]
name=fdio_release
baseurl=https://packagecloud.io/fdio/release/el/7/$basearch
repo_gpgcheck=1
gpgcheck=0
enabled=1
gpgkey=https://packagecloud.io/fdio/release/gpgkey
sslverify=1
sslcacert=/etc/pki/tls/certs/ca-bundle.crt
metadata_expire=300

[fdio_release-source]
name=fdio_release-source
baseurl=https://packagecloud.io/fdio/release/el/7/SRPMS
repo_gpgcheck=1
gpgcheck=0
enabled=1
gpgkey=https://packagecloud.io/fdio/release/gpgkey
sslverify=1
sslcacert=/etc/pki/tls/certs/ca-bundle.crt
metadata_expire=300
```

更新您的本地yum缓存。

```bash
$ sudo yum clean all
$ sudo yum -q makecache -y --disablerepo='*' --enablerepo='fdio_release'
```

在“安装VPP”命令将安装最新的版本。要安装较早的发行版，请运行以下命令以获取所提供的发行版列表。

```bash
$ sudo yum --showduplicates list vpp* | expand
```

<br/>

# 7. VPP Master分支
要允许yum从VPP master分支访问夜间版本，请创建具有以下内容的文件“ /etc/yum.repos.d/fdio-release.repo”。

```bash
$ cat /etc/yum.repos.d/fdio-release.repo
[fdio_master]
name=fdio_master
baseurl=https://packagecloud.io/fdio/master/el/7/$basearch
repo_gpgcheck=1
gpgcheck=0
enabled=1
gpgkey=https://packagecloud.io/fdio/master/gpgkey
sslverify=1
sslcacert=/etc/pki/tls/certs/ca-bundle.crt
metadata_expire=300

[fdio_master-source]
name=fdio_master-source
baseurl=https://packagecloud.io/fdio/master/el/7/SRPMS
repo_gpgcheck=1
gpgcheck=0
enabled=1
gpgkey=https://packagecloud.io/fdio/master/gpgkey
sslverify=1
sslcacert=/etc/pki/tls/certs/ca-bundle.crt
metadata_expire=300
```

更新您的本地yum缓存。

```bash
$ sudo yum clean all
$ sudo yum -q makecache -y --disablerepo='*' --enablerepo='fdio_master'
```


在“安装VPP”命令将在树枝上安装最新版本。运行以下命令以获取分支产生的图像列表。

```bash
$ sudo yum clean all
$ sudo yum --showduplicates list vpp* | expand
```

<br/>

# 8. 安装VPP RPM

要安装VPP数据包引擎，请运行以下命令：

```bash
$ sudo yum install vpp
```

该VPP RPM依赖于VPP-LIB和VPP SELinux的政策 RPM的，所以它们将被安装好了。

*注意：该VPP SELinux的政策不会在系统上启用SELinux的。它将安装自定义VPP SELinux策略，如果随时启用SELinux，将使用该策略。*


还有其他可选软件包。这些软件包可以与上面的命令结合使用，一次安装，也可以根据需要安装：

```bash
sudo yum install vpp-plugins vpp-devel vpp-api-python vpp-api-lua vpp-api-java vpp-debuginfo vpp-devel libvpp0
```

<br/>

# 9. 启动VPP

在系统上安装VPP后，要在CentOS上将VPP作为systemd服务运行，请运行以下命令：

```bash
$ sudo systemctl start vpp
```
然后，要使VPP在系统重新引导时启动，请运行以下命令：

```bash
$ sudo systemctl enable vpp
```
除了将VPP作为系统服务运行之外，还可以手动启动VPP或使其在GDB中运行以进行调试。有关更多详细信息和针对特定系统定制VPP的方法，请参阅运行VPP。

<br/>

# 10. 卸载VPP RPM
要卸载VPP RPM，请运行以下命令：

```bash
$ sudo yum autoremove vpp*
```

<br/>

# 11. 在openSUSE上安装


**`略`**

<br/>

# 12. 安装包说明

**以下是与VPP一起安装的软件包的简要说明。**

* [**vpp**](https://fd.io/docs/vpp/master/gettingstarted/installing/packages.html#vpp)
    矢量包处理可执行文件。这是使用VPP必须安装的主要软件包。该软件包包含：
    vpp-矢量数据包引擎
    vpp_api_test-矢量数据包引擎API测试工具
    vpp_json_test-矢量数据包引擎JSON测试工具

* [**vpp-lib**](https://fd.io/docs/vpp/master/gettingstarted/installing/packages.html#vpp-lib)
    矢量包处理运行时库。该“VPP”的软件包依赖于这个包，所以它总是会被安装。该软件包包含VPP共享库，其中包括：
    vppinfra-支持矢量，哈希，位图，池和字符串格式的基础库。
    svm-虚拟机库
    vlib-矢量处理库
    vlib-api-二进制API库
    vnet-网络堆栈库

* [**vpp-plugins**](https://fd.io/docs/vpp/master/gettingstarted/installing/packages.html#vpp-plugins)
    矢量数据包处理插件模块。
    `acl`
    `dpdk`
    flowprobe
    gtpu
    ixge
    kubeproxy
    l2e
    lb
    memif
    nat
    pppoe
    sixrd
    stn

* [**vpp-dbg**](https://fd.io/docs/vpp/master/gettingstarted/installing/packages.html#vpp-dbg)
    矢量包处理调试符号。

* [**vpp-dev**](https://fd.io/docs/vpp/master/gettingstarted/installing/packages.html#vpp-dev)
    矢量包处理`开发支持`。该软件包包含VPP库的开发支持文件。

* **vpp-api-python**
    VPP Binary API的Python绑定。

* **vpp-api-lua**
    Lua绑定VPP Binary API。

* [**vpp-selinux-policy**](https://fd.io/docs/vpp/master/gettingstarted/installing/packages.html#vpp-selinux-policy)
    该软件包包含VPP定制SELinux策略。它仅为Fedora和CentOS发行版生成。对于那些发行版，“ vpp”软件包取决于此软件包，因此它将始终被安装。它不会在系统上启用SELinux。它将安装自定义VPP SELinux策略，如果随时启用SELinux，将使用该策略。


<br/>

<br/>
<div align=right>	以上内容由荣涛翻译整理自网络。</div>