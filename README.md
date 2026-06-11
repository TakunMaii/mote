# Mote

Mote 是一个用纯 C 实现的实验性编程语言与编译器项目。当前仓库已经具备：

- 词法、语法、语义分析
- 类型系统、模块系统、多文件编译
- MIR 中间表示
- LLVM IR 后端与默认可执行文件输出
- 基础 C FFI
- typed memory API、slice、defer、显式 arena
- `std` 泛型容器 `List/Map/Set`
- 一个较完整的 raylib 示例游戏 `notgate`

## 核心思想

Mote 目前的语言设计大致围绕这些原则：

- 表达式优先：`fn`、`struct`、`enum` 都是表达式，而不是特殊声明语法
- 顶层直接执行：程序不强制要求用户定义 `main`，顶层语句会在启动时执行
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

- `src/`：编译器实现
- `runtime/`：链接生成可执行文件时使用的运行时支持代码
- `lib/`：内置标准库与基础包，目前主要是 `std` 和 `c`
- `vendor/`：第三方绑定包，目前包括 `raylib`、`glfw`、`opengl`
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
- 分发 `mote.exe` 时，需要连同 `runtime/mote_runtime.c` 一起保留相对目录结构

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
- `-I <dir>`：添加模块搜索根，供 `@import("c")`、`@import("std")`、`@import("vendor/raylib")` 这类导入使用
- `-L <dir>`：添加链接库搜索目录
- `-l<name>`：链接库
- `-Wl,<args>`：把参数直接转发给 linker
- `--dump-ast` / `--dump-mir`：显式打印 AST / MIR

说明：

- 编译器会相对自身可执行文件自动加入官方模块搜索根
- 默认会自动查找同级目录与同级 `lib/`
- 也就是说正常分发时，`std` / `c` 不需要用户手工再写 `-I`

更完整的 CLI 说明见：

- [docs/compiler_usage.md](docs/compiler_usage.md)

## 快速开始

### 1. 先编一个最小例子

```powershell
.\mote.exe test\ffi\string_as_ptr.mote -o test\artifacts\string_as_ptr.exe
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
.\mote.exe test\game\notgate_main.mote -I . -I lib -I test\game -o test\artifacts\notgate.exe
```

运行：

```powershell
.\test\artifacts\notgate.exe
```

说明：

- 这个命令默认会把可执行文件输出到 `test\artifacts\notgate.exe`
- `-I .` 让 `@import("vendor/...")` 能从仓库根目录解析第三方 vendor 包
- 从仓库根目录运行最稳妥，因为 `notgate` 的资源路径优先按仓库内布局查找
- `vendor/raylib/lib` 和 `vendor/glfw/lib` 下的官方预编译库会由编译器自动按平台查找并链接

## 语言速览

Mote 当前已经稳定可用的一批语法包括：

- 基础类型：`i8/i16/i32/i64`、`u8/u16/u32/u64`、`f8/f16/f32/f64`、`char`、`bool`
- 顶层绑定与局部变量：`x = 1;`、`mut y: i32 = 2;`
- 函数字面量：`fn(a: i32) i32 { ... }`，返回类型也可以省略为 `fn(a: i32) { ... }`
- 控制流：`if`、`while`、`do while`、`for`、`break`、`continue`、`defer`
- 指针 / 引用：`*T`、`*mut T`、`&T`、`&mut T`
- 定长数组：`Array(T, N)`
- 结构体 / 枚举：`struct { ... }`、`enum { ... }`
- 泛型：通过 `Type` 普通函数表达
- 模块系统：`pub`、`@import`
- vendor 绑定：`@import("vendor/raylib")`、`@import("vendor/glfw")`、`@import("vendor/opengl")`
- 内建：`@extern`、`@zero`、`@as`、`@slice`、`@len`、`@ptr_add`、`@ptr_diff`
- FFI 句柄类型：`Name = opaque;`，通常配合 `*Name` 使用
- 受限运算符重载：`@operator(+)`、`@operator(*)`、`@operator(==)`

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

### `@operator` 当前规则

顶层或局部独立函数声明使用：

```mote
@operator(+)
add = fn(lhs: Vec2, rhs: Vec2) Vec2 {
    return Vec2{
        .x = lhs.x + rhs.x,
        .y = lhs.y + rhs.y,
    };
};
```

结构体成员函数声明使用：

```mote
Vec2 = struct {
    @operator(+)
    add: fn(lhs: Vec2, rhs: Vec2) Vec2 {
        return lhs;
    },
};
```

当前限制：

- 第一版只支持 `+ - * / ==`，以及一元 `-`
- `!=` 自动复用 `==`
- 只做左操作数分派
- 内建数值类型仍然优先走原生运算
- 暂不支持泛型 `@operator`

完整语法教程见：

- [docs/language_tutorial_zh.md](docs/language_tutorial_zh.md)

## Vendor 说明

- 标准库与基础包放在 `lib/`，当前主要是 `std` 和 `c`
- 第三方绑定统一放在 `vendor/`
- `raylib`、`glfw`、`opengl` 现在都应通过显式 vendor 路径导入

例如：

```mote
glfw = @import("vendor/glfw");
gl = @import("vendor/opengl");
```

其中 `vendor/opengl` 当前采用显式加载路线，需要在创建 OpenGL 上下文之后调用：

```mote
gl.LoadWith(glfw.GetProcAddress);
```

## 标准库进展

当前 `std` 里比较值得直接使用的部分：

- `std/mem`
  - `new/make/dup`
  - `free_ptr/free_slice`
  - `arena_init/arena_make/arena_destroy`
- `std/list`
  - 动态数组
- `std/map`
  - 开放寻址哈希表
- `std/set`
  - 基于 `Map(T, bool)` 的集合
- `std/hash`
  - 若干基础 hash/eq helper
- `std/math`
  - 当前以显式浮点 API 为主
  - 命名采用 `_f32` / `_f64` 后缀，例如 `sqrt_f32`、`pow_f64`
  - 另有纯语言级 `min/max/clamp` 泛型 helper
- `std/linalg`
  - 面向游戏 / 图形的向量与矩阵
  - 当前提供 `Vec2/Vec3/Vec4/IVec2/Mat4`
  - 支持常用 operator、dot/cross、normalize、`Mat4` 变换、`look_at`、`perspective`

## 当前执行模型

这部分值得单独强调，因为它会直接影响你如何组织模块和初始化代码。

- 当前程序入口不是“查找用户定义的 `main` 函数”
- 编译器会生成原生入口，再执行 Mote 顶层语句
- 多文件程序会从入口文件出发，递归解析 `@import`
- 顶层最终按“依赖优先、深度优先、模块内保持源码顺序”的方式拼接执行

因此当前的模块系统不只是命名空间机制，也会影响程序启动时的副作用顺序。

## 文档索引

- [docs/language_tutorial_zh.md](docs/language_tutorial_zh.md)
  - 当前 Mote 语法、语义、模块行为与实现限制的中文教程
- [docs/compiler_usage.md](docs/compiler_usage.md)
  - 编译器 CLI、输出行为、导入路径解析与链接参数
- [docs/runtime_abi.md](docs/runtime_abi.md)
  - MIR / LLVM 后端当前采用的运行时 ABI 约定

## 当前状态

这个项目目前仍然是快速演进中的实验实现，文档和实现会尽量保持同步，但仍然建议优先以：

- `test/` 中的样例
- `docs/` 中的说明
- `src/` 中的实际实现

三者交叉确认。

## 最小测试 Harness

仓库现在提供一个最小回归入口，用来验证基础成功样例和关键失败诊断：

```powershell
pwsh -File scripts/test.ps1 -Build
```

或者：

```powershell
make test
```

当前 manifest 位于 `test/harness/manifest.json`，当前最小回归覆盖：

- 基础 LLVM 生成
- `std` 包 smoke / API
- `vendor/raylib`、`vendor/glfw`、`vendor/opengl` 的基础导入 smoke
- 多文件模块 LLVM 生成
- 类型错误诊断
- `break` 越界语义诊断
