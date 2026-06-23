# Mote 中文教程

如果一句话概括 Mote 的思路，可以说是：

- `fn`、`struct`、`enum` 都是表达式
- 类型本身也是值，`Type` 是“类型的类型”
- 泛型不是单独的一套语法，而是“接收 `Type` 参数的普通函数”

## 源码组织规则

1. 编译器输入必须是“包目录”，不是单个 `.mote` 文件。
2. 包目录里的每个 `.mote` 文件都必须以 `@package("包名");` 开头。
3. 同一目录下所有 `.mote` 文件的包名必须一致。
4. 顶层只能放声明，不能直接写可执行语句。
5. 入口是顶层 `main` 绑定，并且必须是无参数函数。

最小可编译包可以长这样：

```text
hello/
  main.mote
```

```mote
@package("hello");

main :: fn() i32 {
    return 0;
};
```

编译：

```bash
./mote hello
```

只输出 LLVM IR：

```bash
./mote -S hello
```

几个常用选项：

- `-o <file>`：指定输出路径
- `-S`：只生成 `.ll`
- `-I <dir>`：增加包搜索根
- `-C <name=dir>`：增加或覆盖 collection
- `-L <dir>`、`-l<name>`、`-Wl,<args>`：透传给链接阶段
- `--dump-ast`、`--dump-mir`：打印中间结果

## 声明、赋值和语句

Mote 顶层和块内都用同一种“声明/赋值”外观：

```mote
x := 1;
y: i32 = 2;
x = x + y;
```

当前规则可以直接理解成：

- `name :: expr;` 表示把一个值绑定到名字，常用于顶层定义、导入、函数、类型
- `name := expr;` 表示局部类型推断声明
- `name: T = expr;` 显式标注类型
-  `name = expr;` 是赋值
- 语句分号 `;` 是可选的

例子：

```mote
main :: fn() i32 {
    sum: i32 = 0;

    for(i: i32 = 0; i < 10; i = i + 1) {
        if(i == 5) {
            continue;
        }
        if(i == 8) {
            break;
        }
        sum = sum + i;
    }

    defer sum = sum + 100;
    return sum;
};
```

## 基本类型

- 标量：`bool` `char` `i8` `i16` `i32` `i64` `u8` `u16` `u32` `u64` `f8` `f16` `f32` `f64`
- `void`
- `Type`
- 指针：`*T`
- 引用：`&T`
- 可选：`?T`
- `string`
- 定长数组：`Array(T, N)`
- 切片：`[]T`
- 函数类型：`Function([A, B, ...], Ret)`
- 命名类型、应用类型（泛型实例化）
- `opaque`

几个直观例子：

```mote
flag: bool = true;
count: i32 = 3;
title: string = "hello";
name: Array(char, 5) = ['h', 'e', 'l', 'l', 'o'];
ptr: *i32 = 0;
maybe: ?i32 = null;
```

### 指针和引用

```mote
main :: fn() i32 {
    x: i32 = 2;

    x_ptr: *i32 = &x;
    x_ref: &i32 = x;

    *x_ptr = 5;
    x_ref = 6;
    return x;
};
```

当前实现里有两点值得注意：

- `&expr` 可以得到可写位置的引用
- 切片构造 `@slice(T, ptr, len)` 的第二个参数必须是 `*T`

### 可选类型 `?T`

```mote
maybe_num: ?i32 = null;
other_num: ?i32 = 42;

is_null: bool = maybe_num == null;
is_not_null: bool = other_num != null;
value: i32 = @unwrap(other_num);
same_value: i32 = other_num?;
```

当前实现边界：

- `?T` 现在只能和 `null` 比较
- 不能直接比较两个可选值
- `@unwrap(null)` 会在运行时 panic
- `value?` 是 `@unwrap(value)` 的语法糖

### 字符串、数组和切片

Mote 里现在有一个官方字符串类型：`string`。

- `string`：只读字符串视图，运行时布局是 `{ ptr, len }`
- `[]char`：可写字符切片/缓冲区
- `*char`：原始字符指针，常用于 FFI 或 C 接口边界

最常见的例子：

```mote
str :: @import("std:str");

main :: fn() i32 {
    name: string = "mote";
    chars: []char = str.to_chars(name);
    again: string = str.from_chars(chars);

    if(str.equal(name, again)) {
        return 0;
    }
    return 1;
};
```

当前约定：

- 字符串字面量默认类型是 `string`
- `string` 不保证 `\0` 结尾
- `string -> *char` 只表示“取底层内容指针”，不是 C-string 转换
- 真正的 C-string 桥接在 `std:internal/cstr`

### 数组和切片

数组是内联定长值，切片是单独的切片类型：

```mote
main :: fn() i32 {
    values: Array(i32, 3) = [1, 2, 3];
    xs: []i32 = @slice(i32, &values[0], 3);

    if(@len(xs) == 3 && xs[1] == 2) {
        return 0;
    }
    return 1;
};
```

`docs/runtime_abi.md` 里还能看到更底层的事实：

- `Array(T, N)` 是内联连续存储
- 不带隐式长度字段
- 字符串字面量的默认类型是 `string`，不是 `*char`

## 函数、闭包和函数类型

函数字面量就是 `fn(...) ... { ... }`。

```mote
add :: fn(x: i32, y: i32) i32 {
    return x + y;
};
```

返回类型可以省略，当前编译器会做推断：

```mote
add :: fn(a: i32, b: i32) {
    return a + b;
};

noop :: fn() {
};
```

但入口 `main` 不能省略返回类型，必须显式写成 `void` 或 `i32`。

### 函数是一等值

函数类型写成：

```mote
Adder :: Function([i32, i32], i32);
```

函数可以赋值、返回、作为字段保存：

```mote
make_adder :: fn(x: i32) Function([i32], i32) {
    return fn|x|(y: i32) i32 {
        return x + y;
    };
};
```

### 闭包捕获

当前 parser 支持三种捕获写法：

- `|x|`
- `|&x|`

一个引用捕获例子：

```mote
@package("closure_demo");

main :: fn() i32 {
    value: i32 = 41;
    add_ref: Function([], i32) = fn|&value|() i32 {
        return value + 1;
    };
    return add_ref();
};
```

## `struct`、`enum` 和 `Self`

Mote 的一个核心设计是：`struct` 和 `enum` 都是表达式。

```mote
Vec2 :: struct {
    x: f32,
    y: f32,
};

State :: enum {
    idle,
    running,
};
```

这意味着它们和函数声明在表面上很统一：本质都是“把某个值绑定到名字上”。

### 6.1 结构体成员函数

```mote
Vec2 :: struct {
    x: f32,
    y: f32,

    new: fn(x: f32, y: f32) Self {
        return Self {
            .x = x,
            .y = y,
        };
    },

    add: fn(self: &Self, other: Vec2) void {
        self.x = self.x + other.x;
        self.y = self.y + other.y;
    },
};
```

当前实现里，若成员函数第一个参数是下面几种形式：

- `Self`
- `&Self`
- `*Self`

那么调用 `obj.method(...)` 时会自动补 receiver。

也就是说，这样可以直接写：

```mote
main :: fn() i32 {
    v: Vec2 = Vec2.new(1.0, 2.0);
    v.add(Vec2.new(3.0, 4.0));
    if(v.x == 4.0 && v.y == 6.0) {
        return 0;
    }
    return 1;
};
```

### 6.2 枚举

当前 `enum` 是纯 tag 枚举，没有 payload：

```mote
Option :: enum {
    ok,
    not_ok,
};

value = Option.ok;
```

## 泛型

泛型不是特殊声明，而是“接收 `Type` 的普通函数”。

```mote
Vec2 :: fn(T: Type) Type {
    return struct {
        x: T,
        y: T,
    };
};

make_vec2 :: fn(T: Type, x: T, y: T) Vec2(T) {
    return Vec2(T) {
        .x = x,
        .y = y,
    };
};
```

使用时像普通调用：

```mote
v: Vec2(i32) = make_vec2(i32, 1, 2);
```

这套思路贯穿整个项目，包括标准库容器：

```mote
list :: @import("std:list");

main :: fn() i32 {
    xs: list.List(i32) = list.init(i32);
    xs.append(10);
    xs.append(20);
    if(xs.len == 2) {
        return 0;
    }
    return 1;
};
```

这也是 Mote 最值得理解的一点：

- `Type` 是值
- `struct` / `enum` 可以返回类型值
- 泛型实例化就是普通函数调用

## 内建函数

- `@import("...")`
- `@extern("symbol", Function([...], Ret))`
- `@as(T, expr)`
- `@zero(T)`
- `@len(slice_or_string)`
- `@slice(T, ptr, len)`
- `@unwrap(optional)`
- `optional?`
- `@panic(message)`
- `@assert(condition)`
- `@sizeof(T)` 或 `@sizeof(value_type)`
- `@alignof(T)` 或 `@alignof(value_type)`
- `@ptr_add(T, ptr, count)`
- `@ptr_diff(T, lhs, rhs)`
- `@debug(...)`

### `@import`

当前正式导入方式分三类：

1. plain package import：`@import("pkg")`
2. collection import：`@import("std:mem")`
3. collection import：`@import("c:file")`、`@import("vendor:raylib")`

例子：

```mote
mem :: @import("std:mem");
c :: @import("c");
rl :: @import("vendor:raylib");
```

### `@extern` 和 FFI

最直接的写法：

```mote
printf :: @extern("printf", Function([*char, ...], i32));
```

不过项目已经在 `lib/c` 里封了一层，更常见的写法是：

```mote
c :: @import("c");

c.printf("val=%d %s\n", 42, @as(*char, "ok"));
```

当前实现可确认：

- variadic extern 已支持
- 小聚合参数/返回值在 native ABI 边界有专门处理
- 字符串字面量可以在指针目标上下文里转成 `*char`
- 面向 C API 的安全桥接建议使用 `std:internal/cstr`

### `@slice`、`@ptr_add`、`@ptr_diff`

- `@slice(T, ptr, len)`：第一个参数必须是类型，第二个参数必须是 `*T`
- `@ptr_add(T, ptr, count)`：第二个参数必须是 `*T`
- `@ptr_diff(T, lhs, rhs)`：后两个参数都必须是同元素类型的指针

例子：

```mote
main :: fn() i32 {
    values: Array(i32, 4) = [10, 20, 30, 40];
    base: *i32 = &values[0];
    p2: *i32 = @ptr_add(i32, base, 2);
    diff: i64 = @ptr_diff(i32, p2, base);

    if(*p2 == 30 && diff == 2) {
        return 0;
    }
    return 1;
};
```

### `@unwrap`、`?`、`@panic`、`@assert`

- `@unwrap(optional)`：参数必须是 `?T`，结果是 `T`
- `optional?`：等价于 `@unwrap(optional)`
- `@panic(message)`：参数必须是 `string`，运行时打印消息并终止
- `@assert(condition)`：参数必须是 `bool`；为 `false` 时运行时 panic

## 操作符重载

当前实现已经支持一部分操作符重载，但不是“全套都支持”。

从 parser 和语义检查能确认，当前 `@operator(...)` 支持：

- `+`
- `-`
- `*`
- `/`
- `==`

例子：

```mote
Vec2 :: struct {
    x: f32,
    y: f32,
};

@operator(+)
vec2_add :: fn(lhs: Vec2, rhs: Vec2) Vec2 {
    return Vec2{
        .x = lhs.x + rhs.x,
        .y = lhs.y + rhs.y,
    };
};
```

当前限制：

- `@operator` 只能标注命名函数声明
- 当前不支持泛型操作符函数

## 包、模块和可见性

当前模块系统的工作方式是：

1. 读取包目录内所有 `.mote` 文件
2. 解析 `@import`
3. 按依赖关系递归加载包
4. 收集顶层绑定
5. 对导入和顶层名字做重写与符号改名
6. 把所有模块拼成一个统一 AST 再走语义检查

对使用者最重要的表面规则是：

- `pub` 控制导出
- 导入模块后要先绑定到名字，再通过成员访问使用

例如：

```mote
mem :: @import("std:mem");
```

然后：

```mote
mem.copy(...);
```

不能把 `@import("x").y` 当成正式写法直接依赖。模块重写阶段会显式拦这种用法。

## 标准库和官方 collection

当前编译器会自动尝试把这些 collection 加到搜索路径里：

- `std`
- `c`
- `vendor`

仓库里现在已经有这些 `std` 包：

- `std:bytes`
- `std:ctype`
- `std:filesystem`
- `std:fmt`
- `std:fs`
- `std:hash`
- `std:io`
- `std:linalg`
- `std:list`
- `std:map`
- `std:math`
- `std:mem`
- `std:path`
- `std:rand`
- `std:set`
- `std:slice`
- `std:str`
- `std:string`（兼容 shim）
- `std:thread`
- `std:time`
- `std:types`

`c` collection 里当前也有一层常用封装：

- `c`
- `c:ctype`
- `c:file`
- `c:io`
- `c:math`
- `c:memory`
- `c:string`
- `c:time`

如果你想快速看真实用法，最值得先翻的是这些测试：

- `test/basic/std_containers_smoke.mote`
- `test/basic/std_fmt_api.mote`
- `test/basic/std_fs_api.mote`
- `test/basic/std_path_api.mote`
- `test/ffi/ffi_main.mote`
