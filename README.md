# Mote

> 注意：这个语言正在开发中，存在不可预知的风险，请不要将其运用在你的生产级项目中。

Mote是一个力求在简洁和简单之间找到平衡，减少编程者心智负担的通用编程语言。

## 从源代码构建

在仓库根目录执行：

```bash
gcc src/main.c -o mote
```

就能得到编译器的可执行程序。

## 编译器用法

当前 CLI 形式如下：

```text
mote [options] <dir>
```

最常见的三种用法：

默认生成可执行文件：

```bash
./mote test/basic/simple
```

输出 LLVM IR：

```bash
./mote -S test/basic/simple
```

打印 AST / MIR：

```bash
./mote --dump-ast --dump-mir -S test/basic/simple
```

几个关键选项：

- `-o <file>`：指定输出路径
- `-S`：输出 `.ll`，不链接
- `-I <dir>`：添加包搜索根，供 `@import("pkg")` 和 `@import("collection:path")` 这类导入使用
- `-L <dir>`：添加链接库搜索目录
- `-l<name>`：链接库
- `-Wl,<args>`：把参数直接转发给 linker
- `--dump-ast` / `--dump-mir`：显式打印 AST / MIR

## 快速开始

`notgate` 是一个用mote语言写成的推箱子游戏。

编译运行：

```bash
cd ./test/game/notgate
../../../mote .. -o notgate
./notgate
```