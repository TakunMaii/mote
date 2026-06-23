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

## 测试脚本

仓库内置了一套 Python 测试脚本，入口是 `tests/runner.py`。

先构建编译器，再在仓库根目录执行：

```bash
python tests/runner.py --compiler ./mote --all
```

常用用法：

```bash
python tests/runner.py --compiler ./mote --list
python tests/runner.py --compiler ./mote --group parser
python tests/runner.py --compiler ./mote --group builtin
python tests/runner.py --compiler ./mote --case parser/invalid_top_level_statement
python tests/runner.py --compiler ./mote --all --verbose
```

参数说明：

- `--compiler <path>`：指定 `mote` 编译器可执行文件路径
- `--all`：运行全部测试
- `--group <name>`：只运行某个测试分组
- `--case <group/name>`：只运行单个测试用例
- `--list`：列出所有可用测试用例
- `--verbose`：打印实际执行的编译/运行命令

测试分组当前主要包括：

- `parser`
- `module`
- `builtin`
- `semantics`
- `types`
- `entry`

测试脚本支持三类检查：

- 仅检查是否成功编译 / 是否按预期报错
- 检查 LLVM IR 输出
- 在 `clang` 可用时检查原生可执行文件的输入输出与退出码

如果系统里没有 `clang`，需要原生执行的 case 会被自动跳过。更详细的测试目录结构和约定见 [tests/README.md](tests/README.md)。