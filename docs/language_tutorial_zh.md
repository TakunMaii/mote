# Mote 语言教学

这份文档介绍当前仓库中已经实现、并且建议用户依赖的 Mote 语言语法与语义。

本文档的目标不是罗列“理想中的设计”，而是尽量准确描述当前编译器的真实行为。若文档、测试和实现之间出现冲突，应优先以：

- `test/` 中的样例与诊断用例
- `src/` 中的当前实现

交叉确认。

## 1. 先建立整体心智模型

Mote 当前有几个非常重要的语言事实：

- 程序入口是“入口文件 + 它递归导入的模块”，而不是“用户自己写一个 `main` 函数”
- 顶层是一串会执行的语句，不只是声明区
- `fn`、`struct`、`enum` 都是表达式，可以出现在赋值右侧
- 类型本身也是值，`Type` 是一等公民
- 多文件程序会先构造一个统一的模块程序，再按确定顺序执行顶层语句

如果你熟悉 C/C++，这里最容易误判的一点是：

- Mote 当前不是“多个源文件分别编译后按链接规则组织初始化”
- Mote 更接近“从入口模块出发，把导入图整理成一个总程序，再执行总顶层”

## 2. 第一个例子

最小程序可以直接写顶层语句：

```mote
c = @import("c");

c.printf(@as(*char, "hello, mote\n"));
```

如果你要使用仓库内置的 C FFI 包：

```powershell
.\mote.exe -I lib hello.mote -o hello.exe
```

这里有几个点值得马上记住：

- `@import("c")` 会导入内置 `c` 包
- 字符串字面量默认不是 `*char`
- 传给 `printf` 时要显式写 `@as(*char, "...")`

## 3. 程序入口与执行模型

### 3.1 没有用户定义 `main`

当前 Mote 程序不要求、也不依赖用户显式定义 `main`。编译器会自己生成原生入口函数，再调用一段内部初始化逻辑来执行 Mote 顶层代码。

因此，下面这种形式就是合法程序：

```mote
x = 1;
y = 2;
z = x + y;
```

### 3.2 顶层会执行

顶层不是“只能放声明”的区域，而是正常语句序列：

```mote
mut x: i32 = 0;
x = x + 1;
```

这意味着：

- 顶层赋值、调用、控制流都可能产生副作用
- 模块导入不仅仅影响名字可见性，也会影响顶层执行顺序

### 3.3 多文件下的顺序

当前模块系统会从入口文件开始，递归解析 `@import`，然后按下面的顺序拼接顶层程序：

1. 先处理被导入模块
2. 再处理当前模块
3. 同一个模块内部保持源码顺序
4. 同一个模块只拼接一次

因此它的语义更接近“依赖优先、深度优先的顶层执行”。

例如：

`main.mote`

```mote
a = @import("./a");
b = @import("./b");
```

`a.mote`

```mote
c = @import("./c");
print_a = 1;
```

`b.mote`

```mote
print_b = 2;
```

`c.mote`

```mote
print_c = 3;
```

执行顺序会先经过 `c`，再 `a`，再 `b`，最后才是 `main` 自己的剩余顶层语句。

## 4. 注释

支持两种注释：

```mote
// 单行注释

/*
多行注释
*/
```

## 5. 语句、块与分号

### 5.1 普通语句需要分号

```mote
x = 1;
mut y: i32 = 2;
call_something();
return;
return x;
```

### 5.2 控制流语句不额外加分号

```mote
if(flag) {
    x = 1;
}

while(x < 10) {
    x = x + 1;
}
```

### 5.3 表达式形式的 `fn / struct / enum` 在赋值时仍属于语句

```mote
add = fn(a: i32, b: i32) i32 {
    return a + b;
};

Vec2 = struct {
    x: f32,
    y: f32,
};
```

原因很简单：真正的语句是外层赋值，右边只是表达式。

### 5.4 块会引入作用域

```mote
mut x: i32 = 1;
{
    y: i32 = 2;
    x = x + y;
}
```

## 6. 绑定、可变性与赋值

Mote 当前主要通过赋值语句来完成“声明绑定”。

```mote
x = 1;              // 不可变，自动推断类型
mut y = 2;          // 可变，自动推断类型
z: i32 = 3;         // 不可变，显式类型
mut w: i32 = 4;     // 可变，显式类型
```

需要区分两件事：

- “第一次把一个名字引入当前作用域”
- “对已有可写位置做重新赋值”

典型例子：

```mote
mut n: i32 = 0;
n = n + 1;
```

如果左边不是可写位置，或者目标不是 `mut`，语义检查会报错。

## 7. 内建基础类型

当前常见内建类型包括：

- 有符号整数：`i8` `i16` `i32` `i64`
- 无符号整数：`u8` `u16` `u32` `u64`
- 浮点数：`f8` `f16` `f32` `f64`
- 其它：`char` `bool` `void` `Type`

示例：

```mote
a: i8 = 3;
b: i32 = 16;
c: char = 'a';
newline: char = '\n';
flag: bool = true;
pi: f32 = 3.14;
```

`Type` 表示“类型本身的类型”。泛型能力就是建立在 `Type` 作为普通值这一点上的。

## 8. 字面量

当前常见字面量包括：

```mote
1
3.14
true
false
'a'
'\n'
"hello"
```

### 8.1 字符串字面量的关键语义

字符串字面量当前有两个非常重要的性质：

- 默认类型是 `Array(char, N)`
- `N` 表示字面量本身的字节数，不包含自动附加的 `\0`

因此：

- 它不是 `*char`
- 它不会自动退化为指针
- 做 C FFI 时通常需要显式写 `@as(*char, "...")`

例如：

```mote
c = @import("c");
c.printf(@as(*char, "value=%d\n"), 42);
```

### 8.2 `null`

Mote 现在支持 `null` 字面量，但它不是“任意类型的零值”。

- `null` 只能用于可选类型 `?T`
- 单独写 `x = null;` 不能做类型推断
- 需要显式上下文，例如 `x: ?i32 = null;`

例如：

```mote
maybe_num: ?i32 = null;
maybe_ptr: ?*char = null;
```

下面这种写法当前会报错：

```mote
x = null;
```

## 9. 表达式与运算

## 10. 可选类型

当前实现的是一个最小可用版本的可选类型。

### 10.1 类型语法

可选类型写作 `?T`，表示“要么是一个 `T`，要么是 `null`”。

```mote
maybe_num: ?i32 = null;
other_num: ?i32 = 42;
maybe_ptr: ?*char = null;
```

当前支持把 `T` 隐式装箱成 `?T`，也支持把 `null` 赋给 `?T`。

### 10.2 当前支持的判空方式

当前不提供 `@has`，也不会自动做流敏感缩窄。判空方式就是直接和 `null` 比较：

```mote
maybe_num: ?i32 = null;
other_num: ?i32 = 42;

is_null: bool = maybe_num == null;
is_not_null: bool = other_num != null;
```

当前只支持：

- `optional_value == null`
- `optional_value != null`

当前还不支持直接比较两个可选值：

```mote
a: ?i32 = 1;
b: ?i32 = 2;
// 当前会报错
same: bool = a == b;
```

### 10.3 取出值：`@unwrap`

当前通过 `@unwrap(optional_value)` 取出内部值：

```mote
other_num: ?i32 = 42;
value: i32 = @unwrap(other_num);
```

它要求参数必须是 `?T`，返回值类型是内部的 `T`。

如果传入的值实际上是 `null`，程序会在运行时直接失败：

- 会向标准错误输出 `runtime panic: @unwrap(null)`
- 然后调用 `abort()`

也就是说，`@unwrap` 当前是“带运行时检查的强制取值”，不是安全解包。

### 10.4 当前还不支持的能力

为了避免你按别的语言经验误用，这里明确一下当前没有实现的内容：

- 没有 `if x` 这种可选值真值判断
- 没有基于 `x != null` 的自动类型缩窄
- 没有 `?.`
- 没有 `??`
- 没有 `if let`

当前已实现的常见运算包括：

- 算术：`+ - * / %`
- 比较：`== != < <= > >=`
- 逻辑：`! && ||`
- 位运算：`~ & | ^ << >>`
- 一元正负号：`+x` `-x`
- 括号分组：`(expr)`

示例：

```mote
mut x = 2;
x = 2 - (3 + 2 * 5);

mut flag = true;
flag = false || true && !false;

x = +1 + -2 * ~3;
x = x & 3 | 4 ^ 5;
```

对于跨类型转换，当前应按下面的原则理解：

- 安全、可预期的上下文驱动转换通常允许隐式进行
- 可能丢信息或改变底层表示的转换，仍应显式写 `@as(...)`

目前已经可以依赖的隐式数值转换包括：

- 整数字面量到目标整数 / 浮点上下文
- 整数值到目标浮点上下文
- 同号整数变宽
- 浮点变宽

仍然建议显式写 `@as(...)` 的情况包括：

- `float -> int`
- 整数缩窄
- `signed <-> unsigned`
- 任意指针重解释

## 10. 函数

### 10.1 基本语法

函数字面量语法：

```mote
add = fn(a: i32, b: i32) i32 {
    return a + b;
};
```

函数本身是值，因此最常见写法就是把 `fn` 绑定到某个名字上。

### 10.2 参数

值参数：

```mote
square = fn(x: i32) i32 {
    return x * x;
};
```

引用参数：

```mote
addYToX = fn(x: &mut i32, y: i32) void {
    x = x + y;
};
```

### 10.3 返回

显式返回类型：

```mote
max = fn(a: i32, b: i32) i32 {
    if(a > b) {
        return a;
    }
    return b;
};
```

`void` 返回：

```mote
log = fn(x: i32) void {
    return;
};
```

### 10.4 返回类型自动推断

当前已经支持省略函数返回类型：

```mote
add = fn(a: i32, b: i32) {
    return a + b;
};
```

推断规则应理解为：

1. 如果函数体里没有 `return expr;`，则返回类型推断为 `void`
2. 如果存在多个 `return expr;`，它们的类型必须完全一致
3. `return;` 和 `return expr;` 不能混用
4. 如果你已经显式写了返回类型，则仍按显式类型做正常检查

下面这些是合法的：

```mote
f = fn() {
    return;
};

g = fn(x: i32) {
    if(x > 0) {
        return 1;
    }
    return 2;
};
```

下面这种会报错，因为返回值类型冲突：

```mote
bad = fn(flag: bool) {
    if(flag) {
        return 1;
    }
    return 2.0;
};
```

下面这种也会报错，因为混用了带值返回和空返回：

```mote
also_bad = fn(flag: bool) {
    if(flag) {
        return 1;
    }
    return;
};
```

### 10.5 匿名函数与闭包捕获

不捕获外部变量时：

```mote
f: Function([i32], i32) = fn(x: i32) i32 {
    return x + 1;
};
```

按值捕获：

```mote
make_adder = fn(x: i32) Function([i32], i32) {
    return fn|x|(y: i32) i32 {
        return x + y;
    };
};
```

当前稳定可依赖的是：

- 无捕获函数值
- `fn|x|...` 这种按值捕获闭包

按引用捕获 / 可变引用捕获的语法方向已经预留，但当前不应把
`fn|&x|...` 或 `fn|&mut x|...` 当成稳定能力写进正式代码约定里。

## 11. 控制流

### 11.1 `if / else if / else`

```mote
max = fn(a: i32, b: i32) i32 {
    if(a > b) {
        return a;
    } else {
        return b;
    }
};
```

单语句分支可以不写花括号：

```mote
if(flag)
    x = 1;
else
    x = 2;
```

### 11.2 `while`

```mote
mut count: i32 = 0;
while(count < 3)
    count = count + 1;
```

### 11.3 `do ... while`

```mote
do {
    count = count - 1;
} while(count > 0);
```

### 11.4 `for`

```mote
mut sum: i32 = 0;

for(mut i: i32 = 0; i < 10; i = i + 1) {
    if(i == 5) {
        continue;
    }
    if(i == 8) {
        break;
    }
    sum = sum + i;
}
```

### 11.5 `break` / `continue`

只允许出现在循环内部：

```mote
break;
continue;
```

把它们写在循环外会触发语义错误。

### 11.6 `defer`

`defer` 会在当前作用域退出时执行，并且是逆序执行：

```mote
cleanup = fn(value: &mut i32) void {
    defer value = value + 100;
    defer {
        value = value + 10;
    }

    value = value + 1;
};
```

## 12. 指针与引用

### 12.1 指针

```mote
mut x: i32 = 2;

x_ptr: *i32 = &x;
x_mut_ptr: *mut i32 = &mut x;

x_value = *x_ptr;
*x_mut_ptr = 5;
```

含义：

- `*T`：指向 `T` 的指针
- `*mut T`：可写指针
- `&x`：取不可变地址
- `&mut x`：取可变地址
- `*ptr`：解引用

### 12.2 引用

```mote
x_ref: &i32 = x;
x_mut_ref: &mut i32 = x;
x_mut_ref = 6;
```

引用和指针都能间接访问对象，但语言层上会保留它们的类型区分。

对 `&mut T` 赋值时，表示修改它引用的目标值，而不是把引用本身重新绑定到别处。

## 13. 数组

定长数组类型写作 `Array(T, N)`：

```mote
xs: Array(i32, 4) = [1, 2, 3, 4];
ys: Array(f32, 3) = [1.0, 2.0, 3.0];
empty: Array(i32, 0) = [];
```

当前已支持：

- 数组字面量
- 下标访问：`xs[0]`
- 嵌套数组
- 对元素取地址：`&xs[0]`
- 对整个数组取地址：`&xs`

当前限制：

- 数组长度必须是编译期整数字面量
- 空数组 `[]` 通常需要显式数组类型配合使用

## 14. 结构体

结构体本身也是表达式：

```mote
Vec2 = struct {
    x: f32,
    y: f32,
};
```

### 14.1 结构体字面量

```mote
v: Vec2 = Vec2 {
    .x = 1,
    .y = 2,
};
```

字段初始化使用 `.字段 = 值`。

### 14.2 方法

```mote
Vec2 = struct {
    x: f32,
    y: f32,

    new: fn(x: i32, y: i32) Vec2 {
        return Vec2 {
            .x = x,
            .y = y,
        };
    },

    add: fn(self: &mut Self, other: Vec2) void {
        self.x = self.x + other.x;
        self.y = self.y + other.y;
    },
};
```

当前规则：

- `Self` 只能在结构体方法内部表示当前结构体类型
- 当第一个参数是 `Self`、`&Self`、`&mut Self`、`*Self`、`*mut Self` 时，方法调用会自动补第一个实参
- 对指针或引用做成员访问时，当前实现支持自动解一层

例如：

```mote
mut v = Vec2.new(1, 1);
v.add(v);

v_ptr: *mut Vec2 = &v;
v_ptr.x = 2;
```

## 15. 枚举

当前枚举只有标签，没有 payload：

```mote
State = enum {
    idle,
    running,
};

s: State = State.running;
```

运行时表示目前就是一个整型标签值。

## 16. 类型也是值：`Type`、`Function` 与泛型

Mote 当前的泛型不是额外的语法系统，而是普通函数接收 `Type` 参数：

```mote
Pair = fn(T: Type) Type {
    return struct {
        left: T,
        right: T,
    };
};

a: Pair(i32) = Pair(i32) {
    .left = 1,
    .right = 2,
};
```

这意味着：

- 泛型本质上是“类型级普通函数”
- `Type` 参与求值与返回

当前比较稳的泛型能力包括：

- `fn(T: Type) Type` 这种类型工厂
- 显式传类型参数的泛型函数调用
- `Function([..], T)`、`*T`、`[]T` 这类与类型值组合的写法

当前仍应谨慎使用的边界包括：

- 依赖复杂自动推导的泛型调用
- 未特化泛型函数直接当运行时函数值传递
- 泛型和复杂闭包捕获叠加的花式写法

函数类型写作：

```mote
Adder = Function([i32, i32], i32);
Printer = Function([*char, ...], i32);
```

其中：

- 参数列表放在 `[]` 里
- 变参函数用 `...`
- 返回类型写在逗号后面

## 17. 模块系统：`pub` 与 `@import`

### 17.1 `pub`

`pub` 只能用于顶层绑定导出：

```mote
pub base: i32 = 10;

pub scale = fn(value: i32) i32 {
    return value * 3;
};
```

如果把 `pub` 放到局部作用域里，当前应视为非法用法。

### 17.2 相对导入

```mote
math = @import("./math");
```

相对导入是相对于“当前导入者文件所在目录”解析的。

### 17.3 包导入

```mote
app = @import("app");
util = @import("app/util");
```

这类导入需要命令行传 `-I <dir>` 作为搜索根。

### 17.4 导出成员访问

导入后的模块值可以用 `.` 访问导出成员：

```mote
sum: i32 = math.add(1, 2);
```

只有被 `pub` 导出的顶层绑定才允许被其他模块访问。

### 17.5 当前导入解析规则

`@import("x")` 当前要求：

- 必须是内建 `@import`
- 必须恰好有一个参数
- 这个参数必须是字符串字面量

路径查找大致遵循：

- 相对路径：从导入者目录解析
- 搜索根路径：从 `-I` 指定的目录解析
- 既可解析 `foo.mote`
- 也可解析 `foo/root.mote`

### 17.6 模块执行顺序的实际含义

由于顶层语句会执行，所以模块系统不仅是“命名空间系统”，也是“初始化顺序系统”。

你应该把当前行为理解为：

- 导入会把依赖模块提前放到执行序列里
- 导入本身不会作为顶层语句保留到最终程序里
- 被导入模块的顶层副作用会先于导入者发生

因此不建议把复杂隐式初始化逻辑堆在模块顶层。

## 18. 内建：`@import`、`@extern`、`@zero`、`@sizeof`、`@alignof`、`@as`

### 18.1 `@import`

```mote
c = @import("c");
math = @import("./math");
```

它用于加载模块，并返回可通过成员访问读取导出符号的模块别名。

### 18.2 `@extern`

用于声明外部符号：

```mote
printf = @extern("printf", Function([*char, ...], i32));
```

当前应满足：

- 第一个参数是字符串字面量
- 第二个参数是 `Function(...)` 类型

### 18.3 `@zero`

生成某个类型的零值：

```mote
Vec2 = struct {
    x: i32,
    y: i32,
};

v: Vec2 = @zero(Vec2);
b: bool = @zero(bool);
```

### 18.4 `@sizeof` 和 `@alignof`

查询类型的布局信息：

```mote
size: i64 = @sizeof(i32);
align: i64 = @alignof(?i32);
```

- `@sizeof(Type)` 返回该类型的字节大小
- `@alignof(Type)` 返回该类型的对齐
- 两者都在编译期求值，结果类型是 `i64`

### 18.5 `@as`

显式转换：

```mote
x: i32 = 7;
y: f32 = @as(f32, x);
z: i32 = @as(i32, y);
```

当前最常见用途之一是字符串转 `*char`：

```mote
msg: *char = @as(*char, "hello\n");
```

另一个常见用途是“明确要求有损或重解释转换”的地方，例如：

```mote
x: f32 = 3.5;
y: i32 = @as(i32, x);
```

如果只是把整数传给浮点参数、返回成浮点、或在 `f32` / `f64` 上下文里参与运算，
当前通常已经不需要再手写 `@as(f32, ...)`。

## 19. FFI 例子

仓库已经内置了一批 C / libc 绑定，可以直接导入：

```mote
c_mem = @import("c/memory");
c_io = @import("c/io");

heap: *mut char = c_mem.malloc(1);
*heap = 'A';
c_io.putchar(*heap);
c_mem.free(heap);
```

变参函数也已经支持：

```mote
c = @import("c");
c.printf(@as(*char, "val=%d %s\n"), 42, @as(*char, "ok"));
```

如果你需要理解更底层的 ABI 和聚合类型传参规则，继续看：

- [compiler_usage.md](compiler_usage.md)
- [runtime_abi.md](runtime_abi.md)

## 20. Slice 与内存管理

### 20.1 Slice

当前切片类型写作 `[]T`，它是语言内建类型，不需要自己定义结构体。

常见写法：

```mote
empty: []i32 = @slice(i32, @as(*mut i32, 0), 0);
```

当前可以稳定依赖：

- `[]T`
- `xs[i]`
- `@len(xs)`
- `@slice(T, ptr, len)`
- `@ptr_add(T, ptr, count)`
- `@ptr_diff(T, lhs, rhs)`
- `[]T -> *T / *mut T` 的隐式或显式上下文转换

不建议把切片当作公开暴露 `.ptr/.len` 成员的普通 struct 来理解；对用户代码来说，
长度请用 `@len(xs)`，从切片取指针请用目标类型上下文或显式 `@as(*T, xs)`。

其中指针位移专门使用显式内建，不和普通 `+ -` 混用：

```mote
base: *mut i32 = &mut values[0];
p2: *mut i32 = @ptr_add(i32, base, 2);
delta: i64 = @ptr_diff(i32, p2, base);
```

这里的偏移单位是“元素个数”，不是字节数。

### 20.2 `std/mem`

仓库当前已经提供一版 typed memory API：

```mote
mem = @import("std/mem");

ptr: *mut i32 = mem.new(i32);
xs: []i32 = mem.make(i32, 16);
```

常见接口包括：

- `new(T)` / `new_zeroed(T)`
- `make(T, len)` / `make_zeroed(T, len)`
- `try_new(T)` / `try_make(T, len)`
- `dup(T, ptr)` / `dup_slice(T, xs)`
- `free_ptr(T, ptr)`
- `free_slice(T, xs)`

释放规则按分配来源区分：

- `new/make/dup` 这类普通堆分配，用 `free_ptr` 或 `free_slice`
- `arena_*` 分配不能单独释放，只能统一 `arena_reset` 或 `arena_destroy`

### 20.3 Arena

当前也已经有显式 arena API：

```mote
mem = @import("std/mem");

mut arena: mem.Arena = mem.arena_init(1024);
defer mem.arena_destroy(&mut arena);

ptr = mem.arena_new(i32, arena);
xs = mem.arena_make(i32, arena, 32);
```

常见接口包括：

- `arena_init`
- `arena_new`
- `arena_make`
- `arena_dup`
- `arena_dup_slice`
- `arena_reset`
- `arena_destroy`

### 20.4 `std` 容器

当前标准库已经提供三类基础泛型容器：

- `std/list`
- `std/map`
- `std/set`

它们也都通过 `std` 根模块做了包装导出。

```mote
std = @import("std");
hash = @import("std/hash");

mut xs = std.list_init(i32);
std.list_append(i32, &mut xs, 10);
std.list_append(i32, &mut xs, 20);

mut kv = std.map_init(i32, i32, hash.hash_i32, hash.eq_i32);
std.map_put(i32, i32, &mut kv, 3, 30);

mut seen = std.set_init(i32, hash.hash_i32, hash.eq_i32);
std.set_insert(i32, &mut seen, 42);
```

`List(T)` 当前常见接口：

- `init` / `init_cap`
- `init_in_arena` / `init_cap_in_arena`
- `append` / `append_slice`
- `pop`
- `insert`
- `remove_at`
- `swap_remove`
- `as_slice`
- `deinit`

`Map(K, V)` 当前采用开放寻址哈希表，常见接口：

- `init` / `init_cap`
- `init_in_arena` / `init_cap_in_arena`
- `put`
- `get`
- `get_ptr`
- `contains`
- `remove`
- `clear`
- `deinit`

`Set(T)` 当前基于 `Map(T, bool)` 构建，常见接口：

- `init` / `init_cap`
- `init_in_arena` / `init_cap_in_arena`
- `insert`
- `contains`
- `remove`
- `clear`
- `deinit`

容器释放规则：

- 普通 `init/init_cap` 路径，最后调用对应的 `deinit`
- `init_in_arena/init_cap_in_arena` 路径，不单独释放底层块，只在 arena 上统一 `reset/destroy`

## 21. 当前实现限制与使用建议

下面这些不是“语法糖习惯”，而是当前应明确记住的实现边界：

- 顶层不要求 `main`
- 顶层语句会执行
- 多文件程序按依赖优先、深度优先顺序拼接顶层执行
- `pub` 只能出现在顶层
- 字符串字面量默认不是 `*char`
- `enum` 目前只有纯标签枚举，没有 payload
- 数组长度目前必须写成编译期整数字面量
- `if` / `while` / `for` 当前是语句，不是值表达式
- 函数省略返回类型时，所有带值 `return` 的类型必须完全一致
- `optional` 目前仍然只支持和 `null` 比较，不支持两个 `?T` 直接比较
- 泛型已经可用，但还不是“高级元编程系统”

最近这几轮已经额外修稳了一些底层边界：

- 递归类型参与报错时，不应该再把编译器本身打崩
- 跨模块的 `Type` 工厂调用路径比之前稳定一些
- import 扫描期的模块存储已经改成稳定地址，复杂模块图更不容易触发内部崩溃

如果你在写稍大一点的程序，建议遵循下面的工程习惯：

- 把模块顶层主要用于绑定定义，而不是复杂副作用
- 需要明确初始化顺序时，优先写显式函数调用链
- 做 FFI 时对字符串指针、原始指针重解释、函数指针这类地方保持显式
- 对数值转换，优先依赖当前已经稳定的上下文隐式转换；只有在可能丢信息时才显式写 `@as`

## 22. 建议继续阅读的样例

如果你想直接看仓库里的真实代码，推荐从这些文件开始：

- `test/basic/simple.mote`
- `test/basic/basic_types.mote`
- `test/basic/simple_functions.mote`
- `test/basic/function_return_infer.mote`
- `test/basic/control_if_else.mote`
- `test/basic/control_loops.mote`
- `test/basic/control_defer.mote`
- `test/basic/pointer_and_reference.mote`
- `test/basic/array.mote`
- `test/basic/contextual_numeric_conversions.mote`
- `test/basic/integer_literal_numeric_context.mote`
- `test/basic/int_to_float_implicit.mote`
- `test/basic/structs.mote`
- `test/basic/type_and_lambda_function.mote`
- `test/basic/std_mem_alloc_api.mote`
- `test/basic/std_mem_arena_api.mote`
- `test/ffi/ffi_main.mote`
- `test/ffi/string_as_ptr.mote`
- `test/multi/main.mote`
- `test/multi/package_main.mote`

如果你想看错误诊断用例，也可以读：

- `test/diagnostic/`
