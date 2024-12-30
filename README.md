# 基于 FUSE 的简单文件系统

一个使用 `C` 语言和 `FUSE` 编写的简单文件系统实现，可用于教学目的。

## 关于 FUSE

参见 `FUSE` [官方仓库](https://github.com/libfuse/libfuse)：

> FUSE（用户空间文件系统）是一个接口，允许用户空间程序将文件系统导出到 Linux 内核。FUSE 项目由两个组件组成：FUSE 内核模块和 libfuse 用户空间库。libfuse 提供了与 FUSE 内核模块通信的参考实现。

简单来说，`FUSE` 允许我们在系统调用时调用自己的函数，而不是使用默认的内核函数。内核的请求通过回调传递给主程序，我们可以在其中定义自己的函数来处理这些请求。

## 安装 FUSE
在 `Ubuntu` 上安装：

```bash
sudo apt-get install libfuse-dev
```

## 编译并运行 FUSE 文件系统
### 1. 克隆仓库

```bash
git clone git@github.com:ForceInjection/FUSE-Filesystem.git
```

### 2. 进入目录并创建挂载点

```bash
cd FS
mkdir mountpoint
```

### 3. 编译并运行 FS.c

```bash
gcc FS.c -o FS `pkg-config fuse --cflags --libs`
./FS -f mountpoint
```

### 4. 使用文件系统

将当前工作目录切换到 `mountpoint`，即可使用文件系统：

```bash
cd mountpoint
```

### 5. 创建文件
使用以下命令创建文件：

```bash
touch test.txt
echo "Hello, World!" > test.txt
```

### 6. 读取文件
使用以下命令读取文件内容：

```bash
cat test.txt
```

### 7. 卸载文件系统
完成操作后，卸载文件系统：

```bash
fusermount -u mountpoint
```

### 8. 查看 debug 输出
```bash
./FS -f /home/test/
LOADING
GETATTR /
GETATTR /
GETATTR /
READDIR
:grissom:
:test.txt:
GETATTR /grissom
GETATTR /test.txt
GETATTR /
READDIR
:grissom:
:test.txt:
GETATTR /
GETATTR /test.txt
OPEN
GETATTR /test2.txt
CREATEFILE
SAVING
1111000000000000000000000000000
GETATTR /test2.txt
WRITING
SAVING
1111000000000000000000000000000
...
```

## 支持的操作

以下操作已实现：

- 创建和删除目录。
- 创建、读取和写入文件。
- 删除现有文件。
- 追加和截断文件。
- 更新访问、修改和状态更改时间。
- 打开和关闭文件。