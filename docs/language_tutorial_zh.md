# Mote 语法教学

这份文档介绍当前仓库里已经实现、并且在 `test/` 中有覆盖的 Mote 语言语法。

Mote 的整体风格比较接近 Zig：

- 顶层就是一串语句
- `fn`、`struct`、`enum` 都是表达式
- “声明”本质上通常是赋值
- 类型本身也是值，泛型通过 `Type` 普通函数表达

## 1. 第一个例子

Mote 程序不强制写 `main` 函数；顶层语句会在程序启动时执行。

```mote
c = @import("c");

c.printf(@as(*char, "hello, mote\n"));
```

如果你要用内置 C FFI 包编译它：

```powershell
.\mote.exe -I lib hello.mote -o hello.exe
```

## 2. 注释

支持两种注释：

```mote
// 单行注释

/*
多行注释
*/
```

## 3. 语句与分号

普通语句后面要写分号：

```mote
x = 1;
mut y: i32 = 2;
call_something();
return;
return x;
```

控制流语句本身不需要额外分号：

```mote
if(flag) {
    x = 1;
}

while(x < 10) {
    x = x + 1;
}
```

因为函数、结构体、枚举都是表达式，所以把它们绑定到名字上时，整个赋值语句末尾仍然要有分号：

```mote
add = fn(a: i32, b: i32) i32 {
    return a + b;
};

Vec2 = struct {
    x: f32,
    y: f32,
};
```

## 4. 变量与赋值

Mote 用赋值语句来做“声明”。

```mote
x = 1;              // 不可变变量，类型自动推导
mut y = 2;          // 可变变量
z: i32 = 3;         // 显式类型
mut w: i32 = 4;     // 可变 + 显式类型
```

重新赋值时，左边必须是可写位置：

```mote
mut n: i32 = 0;
n = n + 1;
```

块会引入作用域：

```mote
mut x: i32 = 1;
{
    y: i32 = 2;
    x = x + y;
}
```

## 5. 基础类型

当前内建基础类型有：

- 有符号整数：`i8` `i16` `i32` `i64`
- 无符号整数：`u8` `u16` `u32` `u64`
- 浮点数：`f8` `f16` `f32` `f64`
- 其它：`char` `bool` `void` `Type`

示例：

```mote
a: i8 = 3;
b = 16;
c: char = 'a';
newline: char = '\n';
flag: bool = true;
pi: f32 = 3.14;
```

`Type` 是“类型的类型”。这也是 Mote 泛型写法的基础。

## 6. 字面量

当前常见字面量：

```mote
1
3.14
true
false
'a'
'\n'
"hello"
```

注意：

- 字符串字面量的默认类型是 `Array(char, N)`
- 目前不会自动转换成 `*char`
- 做 C FFI 时，通常写成 `@as(*char, "text")`

例如：

```mote
c.printf(@as(*char, "value=%d\n"), 42);
```

## 7. 表达式与运算符

当前支持的常见运算包括：

- 算术：`+ - * / %`
- 比较：`== != < <= > >=`
- 逻辑：`! && ||`
- 位运算：`~ & | ^ << >>`
- 一元正负号：`+x` `-x`
- 括号分组：`(expr)`

例如：

```mote
mut x = 2;
x = 2 - (3 + 2 * 5);

mut flag = true;
flag = false || true && !false;

x = +1 + -2 * ~3;
x = x & 3 | 4 ^ 5;
```

## 8. 函数

函数字面量语法：

```mote
add = fn(a: i32, b: i32) i32 {
    return a + b;
};
```

几点要注意：

- `fn(参数列表) 返回类型 { ... }`
- 返回类型可以省略；省略时会根据 `return` 语句推断，若没有返回值则默认为 `void`
- 函数本身是值，所以通常会被绑定到变量名
- `return;` 用于 `void` 返回

### 8.1 参数

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

### 8.2 匿名函数与闭包捕获

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

当前解析器还支持：

- `fn|&x|(...) { ... }`
- `fn|&mut x|(...) { ... }`

## 9. 控制流

### 9.1 `if / else if / else`

```mote
max = fn(a: i32, b: i32) i32 {
    if(a > b) {
        return a;
    } else {
        return b;
    }
};
```

单语句分支也可以不写花括号：

```mote
if(flag)
    x = 1;
else
    x = 2;
```

### 9.2 `while`

```mote
mut count: i32 = 0;
while(count < 3)
    count = count + 1;
```

### 9.3 `do ... while`

```mote
do {
    count = count - 1;
} while(count > 0);
```

### 9.4 `for`

语法和 C/Zig 风格接近：

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

### 9.5 `break` / `continue`

只允许出现在循环里：

```mote
break;
continue;
```

### 9.6 `defer`

`defer` 会在当前作用域退出时逆序执行：

```mote
cleanup = fn(value: &mut i32) void {
    defer value = value + 100;
    defer {
        value = value + 10;
    }

    value = value + 1;
};
```

## 10. 指针与引用

### 10.1 指针

```mote
mut x: i32 = 2;

x_ptr: *i32 = &x;
x_mut_ptr: *mut i32 = &mut x;

x_value = *x_ptr;
*x_mut_ptr = 5;
```

- `*T`：指向 `T` 的指针
- `*mut T`：可通过这个指针修改目标值
- `&x`：取不可变地址
- `&mut x`：取可变地址
- `*ptr`：解引用

### 10.2 引用

```mote
x_ref: &i32 = x;
mut x_mut_ref: &mut i32 = x;
x_mut_ref = 6;
```

- `&T`：引用类型
- `&mut T`：可变引用
- 给 `&mut T` 赋值时，表示修改它引用的目标值

## 11. 数组

定长数组类型写作 `Array(T, N)`：

```mote
xs: Array(i32, 4) = [1, 2, 3, 4];
mut ys = [1.0, 2.0, 3.0];
empty: Array(i32, 0) = [];
```

数组支持：

- 下标访问：`xs[0]`
- 嵌套数组：`Array(Array(i32, 3), 2)`
- 对元素取地址：`&xs[0]`
- 对整个数组取地址：`&xs`

当前限制：

- 数组长度必须是编译期整数字面量
- 空数组字面量 `[]` 需要显式的零长度数组类型配合使用

## 12. 结构体

结构体也是表达式：

```mote
Vec2 = struct {
    x: f32,
    y: f32,
};
```

### 12.1 结构体字面量

```mote
v: Vec2 = Vec2 {
    .x = 1,
    .y = 2,
};
```

字段初始化使用 `.字段 = 值`。

### 12.2 成员函数

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

说明：

- `Self` 只能在结构体方法内部作为当前类型使用
- 当第一个参数是 `Self`、`&Self`、`&mut Self`、`*Self`、`*mut Self` 时，方法调用会自动补第一个实参
- 指针访问成员时会自动解一层引用/指针

例如：

```mote
mut v = Vec2.new(1, 1);
v.add(v);

v_ptr: *mut Vec2 = &v;
v_ptr.x = 2;
```

## 13. 枚举

当前枚举只有标签，没有 payload：

```mote
State = enum {
    idle,
    running,
};

s: State = State.running;
```

## 14. 类型也是值：`Type`、`Function`、泛型

Mote 的泛型不是特殊语法，而是“接收 `Type`，返回 `Type` 或其它值”的普通函数。

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

函数类型写作：

```mote
Adder = Function([i32, i32], i32);
Printer = Function([*char, ...], i32);
```

其中：

- 参数列表放在 `[]` 里
- 变参函数用 `...`
- 返回类型写在逗号后面

## 15. 模块、`pub` 与 `@import`

### 15.1 `pub`

`pub` 只能用于顶层绑定导出：

```mote
pub base: i32 = 10;

pub scale = fn(value: i32) i32 {
    return value * 3;
};
```

### 15.2 导入

相对导入：

```mote
math = @import("./math");
```

包导入：

```mote
app = @import("app");
util = @import("app/util");
```

导入后的模块名可以通过 `.` 访问导出成员：

```mote
sum: i32 = math.add(1, 2);
```

包导入需要在命令行上传 `-I <dir>`，其中 `<dir>` 是包含对应模块目录的搜索根。

## 16. 内建语法：`@import`、`@extern`、`@zero`、`@as`

### 16.1 `@import`

```mote
c = @import("c");
math = @import("./math");
```

### 16.2 `@extern`

用于声明外部符号：

```mote
printf = @extern("printf", Function([*char, ...], i32));
```

第一个参数必须是字符串字面量，第二个参数必须是 `Function(...)` 类型。

### 16.3 `@zero`

生成某个类型的零值：

```mote
Vec2 = struct {
    x: i32,
    y: i32,
};

v: Vec2 = @zero(Vec2);
b: bool = @zero(bool);
```

### 16.4 `@as`

显式转换：

```mote
x: i32 = 7;
y: f32 = @as(f32, x);
z: i32 = @as(i32, y);
```

当前也支持字符串到 `*char` 的显式转换：

```mote
msg: *char = @as(*char, "hello\n");
```

## 17. FFI 例子

当前仓库已经内置了一批 C / libc 绑定，可以直接通过 `@import("c")` 或子模块使用：

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

## 18. 当前实现下的几个实用提醒

- 顶层不强制要求 `main`
- `pub` 只能出现在顶层
- 字符串字面量默认不是 `*char`
- `enum` 目前只有纯标签枚举
- 数组长度目前必须写成编译期整数字面量
- `if` / `while` / `for` 目前是语句，不是值表达式

## 19. 可以继续参考的示例

如果你想直接看仓库里的真实代码，推荐从这些文件开始：

- `test/basic/simple.mote`
- `test/basic/basic_types.mote`
- `test/basic/simple_functions.mote`
- `test/basic/control_if_else.mote`
- `test/basic/control_loops.mote`
- `test/basic/pointer_and_reference.mote`
- `test/basic/array.mote`
- `test/basic/structs.mote`
- `test/basic/type_and_lambda_function.mote`
- `test/ffi/ffi_main.mote`
- `test/multi/package_main.mote`
