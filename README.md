# Mote

Mote 是一个用纯 C 实现的实验性编程语言与编译器项目。当前仓库已经具备：

- 词法、语法、语义分析
- 类型系统、模块系统、多文件编译
- MIR 中间表示
- LLVM IR 后端与默认可执行文件输出
- 基础 C FFI
- 一个较完整的 raylib 示例游戏 `notgate`

## 核心思想

Mote 目前的语言设计大致围绕这些原则：

- 表达式优先：`fn`、`struct`、`enum` 都是表达式，而不是特殊声明语法
- 顶层直接执行：程序不强制要求 `main`，顶层语句会在启动时执行
- 类型也是值：`Type` 是一等公民，泛型通过“接收 `Type`、返回 `Type` 或其它值”的普通函数表达
- 显式而简单：数组、指针、引用、转换、FFI 都尽量走直接语法，不隐藏太多机制
- 面向编译实现：前端语义、MIR 和 LLVM lowering 的关系尽量清晰，便于继续演进后端和 ABI

一个很小的例子：

```mote
c = @import("c");

add = fn(a: i32, b: i32) i32 {
    return a + b;
};

c.printf(@as(*char, "1 + 2 = %d\n"), add(1, 2));
```

## 仓库结构

- `src/`：编译器实现与运行时
- `lib/`：内置包，目前主要是 `c` 和 `raylib`
- `test/`：语言样例、错误样例、FFI、多文件、raylib、`notgate`
- `docs/`：编译器用法、语法教程、运行时 ABI

## 构建编译器

在仓库根目录执行：

```powershell
gcc src\main.c -o mote.exe
```

说明：

- `gcc` 用来构建 `mote.exe`
- `clang` 需要在 `PATH` 中，默认生成可执行文件时会用到

## 编译器用法

当前 CLI 形式如下：

```text
mote [options] <input.mote>
```

最常见的三种用法：

默认生成可执行文件：

```powershell
.\mote.exe test\basic\simple.mote
```

输出 LLVM IR：

```powershell
.\mote.exe -S test\basic\simple.mote
```

打印 AST / MIR：

```powershell
.\mote.exe --dump-ast --dump-mir -S test\basic\simple.mote
```

几个关键选项：

- `-o <file>`：指定输出路径
- `-S`：输出 `.ll`，不链接
- `-I <dir>`：添加模块搜索根，供 `@import("c")`、`@import("raylib")` 这类导入使用
- `-L <dir>`：添加链接库搜索目录
- `-l<name>`：链接库
- `-Wl,<args>`：把参数直接转发给 linker
- `--dump-ast` / `--dump-mir`：显式打印 AST / MIR

更完整的 CLI 说明见：

- [docs/compiler_usage.md](docs/compiler_usage.md)

## 快速开始

### 1. 先编一个最小例子

```powershell
.\mote.exe test\ffi\string_as_ptr.mote -I lib -o test\artifacts\string_as_ptr.exe
.\test\artifacts\string_as_ptr.exe
```

### 2. 编译并运行 notgate

`notgate` 是当前仓库里最完整的 Mote 示例，覆盖了：

- 多文件模块
- `pub` / `@import`
- raylib FFI
- 贴图、音效、shader
- 游戏循环与较复杂的状态逻辑

推荐始终在仓库根目录执行下面的命令。

编译：

```powershell
.\mote.exe test\game\notgate_main.mote -I lib -I test\game -L test\game\notgate\build -lraylib -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -o test\artifacts\notgate.exe
```

运行：

```powershell
.\test\artifacts\notgate.exe
```

说明：

- 这个命令默认会把可执行文件输出到 `test\artifacts\notgate.exe`
- 从仓库根目录运行最稳妥，因为 `notgate` 的资源路径优先按仓库内布局查找
- `raylib.lib` 已经放在 `test\game\notgate\build\raylib.lib`

## 语言速览

Mote 当前已经稳定可用的一批语法包括：

- 基础类型：`i8/i16/i32/i64`、`u8/u16/u32/u64`、`f8/f16/f32/f64`、`char`、`bool`
- 顶层绑定与局部变量：`x = 1;`、`mut y: i32 = 2;`
- 函数字面量：`fn(a: i32) i32 { ... }`
- 控制流：`if`、`while`、`do while`、`for`、`break`、`continue`、`defer`
- 指针 / 引用：`*T`、`*mut T`、`&T`、`&mut T`
- 定长数组：`Array(T, N)`
- 结构体 / 枚举：`struct { ... }`、`enum { ... }`
- 泛型：通过 `Type` 普通函数表达
- 模块系统：`pub`、`@import`
- 内建：`@extern`、`@zero`、`@as`

例如：

```mote
Vec2 = struct {
    x: f32,
    y: f32,

    add: fn(self: &mut Self, other: Vec2) void {
        self.x = self.x + other.x;
        self.y = self.y + other.y;
    },
};

mut a: Vec2 = Vec2 { .x = 1, .y = 2 };
b: Vec2 = Vec2 { .x = 3, .y = 4 };
a.add(b);
```

完整语法教程见：

- [docs/language_tutorial_zh.md](docs/language_tutorial_zh.md)

## 文档索引

- [docs/language_tutorial_zh.md](docs/language_tutorial_zh.md)
  - 当前 Mote 语法教学与示例
- [docs/compiler_usage.md](docs/compiler_usage.md)
  - 编译器 CLI、输出行为、包导入与链接参数
- [docs/runtime_abi.md](docs/runtime_abi.md)
  - MIR / LLVM 后端当前采用的运行时 ABI 约定

## 当前状态

这个项目目前仍然是快速演进中的实验实现，文档和实现会尽量保持同步，但仍然建议优先以：

- `test/` 中的样例
- `docs/` 中的说明
- `src/` 中的实际实现

三者交叉确认。
