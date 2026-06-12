# Mote

> 注意：这个语言正在开发中，存在不可预知的风险，请不要将其运用在你的生产级项目中。

Mote是一个力求在简洁和简单之间找到平衡，减少编程者心智负担的通用编程语言。

## 从源代码构建

在仓库根目录执行：

```bash
gcc src\main.c -o mote
```

就能得到编译器的可执行程序。

## 编译器用法

当前 CLI 形式如下：

```text
mote [options] <dir>
```

最常见的三种用法：

默认生成可执行文件：

```powershell
.\mote.exe test\basic\simple
```

输出 LLVM IR：

```powershell
.\mote.exe -S test\basic\simple
```

打印 AST / MIR：

```powershell
.\mote.exe --dump-ast --dump-mir -S test\basic\simple
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

`notgate` 是一个用mote语言写成的推箱子游戏，覆盖了：

- 多文件包
- `@package` / `pub` / `@import`
- raylib FFI
- 贴图、音效、shader
- 游戏循环与较复杂的状态逻辑

等功能。

编译：

```powershell
.\mote.exe test\game\notgate -I . -o test\artifacts\notgate.exe
```

运行：

```powershell
.\test\artifacts\notgate.exe
```

## 文档索引

- [docs/language_tutorial_zh.md](docs/language_tutorial_zh.md)
  - 当前 Mote 语法、语义、包系统行为与实现限制的中文教程
- [docs/compiler_usage.md](docs/compiler_usage.md)
  - 编译器 CLI、包导入解析与链接参数
- [docs/runtime_abi.md](docs/runtime_abi.md)
  - MIR / LLVM 后端当前采用的运行时 ABI 约定
