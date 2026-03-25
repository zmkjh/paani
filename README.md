# Paani 编程语言

Paani（印地语"水"的意思）是一个专为 ECS（Entity-Component-System）架构设计的领域特定语言。

## 核心设计理念

1. **严格的包隔离** - 符号必须显式导出才能被其他包使用，普通函数完全隔离
2. **World 严格隔离** - 每个包有自己的 ECS world，entity 只能拥有其所在 world 的组件
3. **World 自动推断** - 编译器自动推断 ECS 操作应该使用哪个 world
4. **零成本抽象** - 生成的 C 代码直接调用运行时，无额外开销

---

## 目录

- [快速开始](#快速开始)
- [包管理](#包管理)
- [World 自动推断](#world-自动推断)
- [类型系统](#类型系统)
- [ECS 核心](#ecs-核心)
- [控制流](#控制流)
- [CLI 命令](#cli-命令)
- [项目结构](#项目结构)
- [注意事项](#注意事项)

---

## 快速开始

```bash
# 创建新项目
paani new mygame
cd mygame

# 编译所有库包
paani build

# 编译并运行
paani run
```

---

## 包管理

### use 声明

```C
use engine;              // 导入 engine 包
use engine as e;         // 导入并设置别名
```

**包隔离规则：**

| 符号类型 | 跨包访问 | 说明 |
|---------|---------|------|
| `fn` | ❌ 不能 | 普通函数完全隔离 |
| `export fn` | ✅ 能 | 通过 `package.funcName()` 访问 |
| `data/trait/template` | ✅ 能（main 包） | main 包可通过 `package.Name` 访问 |
| `extern fn/type` | ❌ 不能 | 只在声明的包内部可见 |

**不同包的 use 行为：**

```C
// main 包（入口点）- use 加载整个包
use engine;  // 加载所有导出符号

// 非 main 包 - use 只加载 utils/ 目录下的函数
use engine;  // 只加载 engine.utils/ 下的函数
```

### export 规则

```C
// 可导出的符号（带 export 关键字）
export data Position { x: f32, y: f32 }
export trait Movable [Position, Velocity]
export template Player { Position, Velocity }
export handle File with fclose
export fn public_api() -> i32 { return 42; }

// 不可导出的符号（包内私有）
fn internal_func() {}           // 普通函数
system mySystem() {}            // system
data LocalData { x: i32 }       // data
```

---

## World 自动推断

### World 推断规则

| 操作 | World 推断规则 | 示例 |
|------|---------------|------|
| `spawn` | 空 spawn 使用当前包 world | `let e = spawn` → `__paani_gen_this_w` |
| `spawn Template` | 使用模板所属包的 world | `spawn engine.Player` → `__paani_gen_engine_w` |
| `give e Component` | 使用组件所属包的 world | `give e engine.Health` → `__paani_gen_engine_w` |
| `tag e as Trait` | 使用 trait 所属包的 world | `tag e as engine.Movable` → `__paani_gen_engine_w` |
| `e has Component` | 使用组件所属包的 world | `e has engine.Health` → `__paani_gen_engine_w` |
| `destroy e` | 使用 entity 所属的 world | `destroy player` → `__paani_gen_engine_w` |

### 跨包 ECS 操作示例

**❌ 错误示例：**

```paani
// main world 的 entity 不能拥有 engine world 的组件
let player = spawn;  // main world 的 entity
give player engine.Health: {hp: 100};  // 编译错误！World 不匹配
```

**✅ 正确示例：**

```C
use engine;

// 通过 engine 的 export fn 创建 engine world 的 entity
let player = engine.create_player();

// 遍历 engine world 的实体
for engine.Movable in updateEngineEntities(dt: f32) as obj {
    obj.Position.x += 1.0;
}

// 检查 engine 包的组件
if (player has engine.Health) {
    // ...
}
```

### 导出函数的 World 参数

所有 `export fn` 在生成的 C 代码中会自动添加 world 参数：

```C
export fn create_player() -> entity {
    let player = spawn;
    give player Health: {hp: 100};
    return player;
}
```

生成的 C 代码：

```c
paani_entity_t main__create_player(paani_world_t* __paani_gen_world) {
    paani_entity_t player = paani_entity_create(__paani_gen_world);
    paani_component_add(__paani_gen_world, player, s_ctype_main__Health, &__comp_data);
    return player;
}
```

---

## 类型系统

### 基本类型

| Paani 类型 | C 类型 | 说明 |
|-----------|--------|------|
| `void` | void | 空类型（仅用于函数返回） |
| `bool` | uint8_t | 布尔值 |
| `i8/i16/i32/i64` | int8/16/32/64_t | 有符号整数 |
| `u8/u16/u32/u64` | uint8/16/32/64_t | 无符号整数 |
| `f32` | float | 单精度浮点 |
| `f64` | double | 双精度浮点 |
| `string` | const char* | 字符串（语言内置，不可修改） |
| `entity` | uint64_t | 实体句柄 |

**注意：**
- Paani **不支持**指针类型（没有 `*T` 语法）
- `void` **只能**用于函数返回类型，**不能**用于变量声明

### 数组类型

```C
// 数组字面量（仅用于初始化）
let arr = [1, 2, 3, 4, 5]
let empty = []

// 数组索引
let first = arr[0]
arr[1] = 10
```

### 外部类型

```C
// 不透明类型，指定大小
extern type Window[64]
extern type Texture[32]
```

### Handle 类型（自动资源管理）

```C
// 声明带析构函数的资源类型
handle File with fclose

// 在组件中使用
data Asset {
    file: File,
}
// 当实体被销毁时，自动调用 fclose
```

**Handle 析构时机：**

| 使用场景 | 析构时机 |
|---------|---------|
| 组件字段 | 实体销毁时 |
| 局部变量 | 作用域结束时 |
| main 包全局语句 | 程序退出时 |
| 普通包全局语句 | `module_init` 结束时 |

### 全局语句详解

全局语句是包级别直接执行的代码（不在函数内），用于初始化：

**执行顺序：**

```
1. 创建所有包的 World
2. 执行各包的 module_init（包括普通包的全局语句）
3. 进入 main() 函数
4. 执行 main 包的全局语句
5. 进入主游戏循环
```

**main 包（入口点）：**
```C
// main/main.paani
use engine;

// 全局语句在所有包初始化完成后执行
// 此时 engine 包已完全初始化
let player = engine.create_player();  // 使用 __paani_gen_this_w
tag player as engine.Player;

fn main() {
    // player 已创建，可直接使用
    exit;
}
```

**普通包（库）：**
```C
// engine/core.paani

// 全局语句在 module_init() 中执行
// 此时该包的 ECS 对象已注册完成
let config = load_config();  // 使用 __paani_gen_world

export fn get_config() -> Config {
    return config;  // 错误！全局语句变量在函数中不可见
}
```

**关键差异：**

| 特性 | main 包 | 普通包 |
|-----|---------|--------|
| 执行时机 | 所有包 `module_init` 完成后，`main()` 中 | `module_init()` 中，ECS 注册后 |
| World 变量 | `__paani_gen_this_w` | `__paani_gen_world` |
| 可用资源 | 所有包已初始化完成 | 仅本包已注册的资源 |
| 变量作用域 | 仅该语句内 | 仅该语句内 |
| 跨语句访问 | ❌ 不能 | ❌ 不能 |
| 函数内访问 | ❌ 不能 | ❌ 不能 |

**设计理念：**
- 强制 ECS：长期状态必须存储在 entity-component 中
- 无全局状态：禁止随意定义全局可访问变量
- 明确生命周期：全局语句是独立执行单元

---

## ECS 核心

### Data（组件）

```C
data Position {
    x: f32,
    y: f32
}

data Velocity {
    vx: f32,
    vy: f32
}
```

### Trait（特征标签）

```C
// 纯标签
trait Enemy
trait Player

// 带依赖的 trait
trait Movable [Position, Velocity]
trait Living [Health]
```

**注意：** Trait 只是标签！不会自动添加组件：

```C
let e = spawn
give e Position: {x: 0, y: 0}
give e Velocity: {vx: 1, vy: 0}
tag e as Movable   // OK：现在可以标记为 Movable
```

### Template（实体模板）

```C
template Player {
    Position: { x: 100.0, y: 200.0 },
    Velocity: { vx: 0.0, vy: 0.0 },
    tags: [Movable, Living, Player]
}
```

### 实体操作

```C
// 创建实体
let e1 = spawn                    // 空实体
let player = spawn Player         // 从模板创建

// 添加组件
give player Position: { x: 10.0, y: 20.0 }
give player Weapon              // 添加空组件

// Tag/Untag
tag player as Elite
untag player as Movable

// 检查组件/Tag
if (player has Position) { }
if (player !has Boss) { }        // 检查没有

// 销毁（延迟销毁）
destroy player
destroy                          // 销毁当前实体（在 system 中）
```

### System 定义

```C
// 普通 System
system initSystem() {
    // ...
}

// Trait System（遍历）
for Movable in moveSystem(dt: f32) as obj {
    obj.Position.x += obj.Velocity.vx * dt * 0.001
    obj.Position.y += obj.Velocity.vy * dt * 0.001
}

// #batch System（SIMD 优化）
#batch
for Movable in physicsSystem(dt: f32) as obj {
    obj.Position.x += obj.Velocity.vx * dt * 0.001
    obj.Position.y += obj.Velocity.vy * dt * 0.001
}
```

**#batch 限制：**
- ❌ `if` / `while` / `for`
- ❌ `spawn` / `give` / `tag` / `untag` / `destroy` / `has`
- ❌ `query`
- ❌ `break` / `continue` / `return`
- ❌ `lock` / `unlock` / `exit`
- ❌ 函数调用

### System 依赖

```C
// 语法：depend A -> B -> C;
depend inputSystem -> updatePosition -> collisionSystem -> renderSystem
```

### Query（查询）

```C
// 语法：query Trait with Comp as var { ... }
query Living with Health as patient {
    if (patient.Health.hp < patient.Health.max) {
        patient.Health.hp += 5.0 * dt * 0.001
    }
}

// 多 trait 查询
query (Movable, Living) as entity {
    // ...
}
```

---

## 控制流

### 变量声明

```C
let x: i32 = 10       // 不可变
let y = 20            // 类型推断
var z: f32 = 1.5      // 可变
```

**注意：** `let` 变量不可重新赋值，需要可变时请使用 `var`。

### 条件与循环

```C
// if-else
if (x > 0) {
    return x
} else if (x < 0) {
    return -x
} else {
    return 0
}

// while
while (running) {
    update()
}

// 循环控制
break
continue
```

### 函数

```C
// 普通函数
fn add(x: i32, y: i32) -> i32 {
    return x + y
}

// 导出函数
export fn create_player() -> entity {
    let player = spawn Player
    return player
}

// 外部 C 函数
extern fn sin(x: f64) -> f64
extern fn cos(x: f64) -> f64
```

### 其他

```C
// 返回
return x
return                  // void 函数

// 退出游戏（只能在 main 包中使用）
exit
```

---

## CLI 命令

```bash
paani new <name>          # 创建新项目
paani build [pkg]         # 编译所有包或指定包
paani link                # 编译 main 包并生成可执行文件
paani run                 # 编译并运行可执行文件
paani clean               # 清理生成的文件
```

---

## 项目结构

```
mygame/
├── engine/               # engine 包
│   ├── core.paani        # ECS 定义、system
│   ├── utils/            # 纯函数工具
│   │   └── math.paani
│   └── libs/             # 外部库文件
│       ├── libfoo.a      # 静态库
│       └── bar.dll       # 动态库
├── game/                 # game 包
│   ├── logic.paani
│   └── utils/
│       └── helpers.paani
├── main/                 # main 包（入口点）
│   └── main.paani        # 全局初始化代码
├── output/               # 生成的 C 代码
│   ├── engine.c/h
│   ├── game.c/h
│   ├── main.c/h
│   └── Makefile
└── build/                # 编译产物
    ├── main.exe
    ├── *.o
    └── *.a
```

**关键规则：**
- 包 = 目录，所有 `.paani` 文件合并编译
- `utils/` 子目录**只能**包含纯函数，不能包含 ECS 相关代码
- `libs/` 子目录用于存放外部库文件（静态库、动态库、对象文件）
- `main/` 包是入口点，其全局语句在 `main()` 函数中执行
- 普通包的全局语句在 `module_init()` 中执行，执行完毕后立即释放临时资源
- `main/` 包是入口点，包含全局初始化代码
- 每个包有自己的 ECS world
- 普通包的 system 默认 **lock**，需要 `unlock` 才能运行

### libs/ 子目录

`libs/` 子目录用于存放外部库文件，支持以下格式：

| 文件类型 | 说明 |
|---------|------|
| `.a` / `.lib` | 静态库 |
| `.dll` / `.so` / `.dylib` | 动态库 |
| `.o` / `.obj` | 对象文件 |

**使用示例：**

```
engine/
├── core.paani
└── libs/
    ├── libphysics.a      # 物理引擎静态库
    ├── librender.so      # 渲染引擎动态库
    └── custom.o          # 自定义对象文件
```

在编译时，这些库文件会自动链接到最终的可执行文件中。

---

## 注意事项

1. **entity 类型不能用于组件字段** - 使用 `u64` 存储实体 ID
2. **#batch 有限制** - 不能使用 if/spawn/destroy/函数调用等
3. **trait 只是标签** - `tag` 不会自动添加组件，必须先用 `give`
4. **dt 单位是毫秒** - 计算时通常需要 `* 0.001` 转换为秒
5. **destroy 是延迟的** - 当前帧结束时才真正销毁
6. **void 只能用于函数返回** - 不能声明 void 变量
7. **不支持指针类型** - 没有 `*T` 语法
8. **extern 函数只在当前包可见** - 不能被其他包通过 `use` 访问
9. **World 严格隔离** - entity 只能拥有其所在 world 的组件
10. **包前缀语法** - 使用其他包的 ECS 对象时必须加包前缀
11. **main 包调用权限** - 可通过 `export fn` 调用其他包的功能
12. **exit 只能在 main 包使用** - 普通包中使用会导致编译错误
13. **let 变量不可变** - 使用 `var` 声明可变变量

---

## 命名空间规则

| Paani 符号 | C 符号 |
|-----------|--------|
| `data Position` | `package__Position` |
| `trait Movable` | `s_trait_package__Movable` |
| `fn func()` | `package__func` |
| `export fn func()` | `package__func` |
| `extern fn func()` | `func`（无前缀）|
| `system sys()` | `package__system_sys` |
| `template T` | `package__spawn_T` |

---

## 示例程序

### 基本示例

```C
// main.paani
use engine

data Player { x: f32, y: f32 }
trait Living [Player]

for Living in updateSystem(dt: f32) as player {
    player.Player.x += 1.0 * dt * 0.001
}

export fn create_player() -> entity {
    let e = spawn
    give e Player: {x: 0, y: 0}
    tag e as Living
    return e
}
```

### 跨包操作完整示例

```C
// engine/core.paani
export data Position { x: f32, y: f32 }
export data Health { hp: f32, max_hp: f32 }

export trait Movable [Position]
export trait Living [Health]

export template PlayerTemplate {
    Position: { x: 0.0, y: 0.0 },
    Health: { hp: 100.0, max_hp: 100.0 },
    tags: [Movable, Living]
}

for Movable in moveSystem(dt: f32) as obj {
    obj.Position.x += 10.0 * dt * 0.001;
}

export fn create_player() -> entity {
    return spawn PlayerTemplate;
}
```

```C
// main/main.paani
use engine;

let player = engine.create_player();

for engine.Living in healSystem(dt: f32) as e {
    if (e.Health.hp < e.Health.max_hp) {
        e.Health.hp += 1.0 * dt * 0.001;
    }
}

export fn get_player_health() -> f32 {
    query engine.Living as e {
        return e.Health.hp;
    }
    return 0.0;
}
```
