# Paani 编程语言

Paani（印地语"水"的意思）是一个专为 ECS（Entity-Component-System）架构设计的领域特定语言。

## 核心设计理念

1. **严格的包隔离** - 符号必须显式导出才能被其他包使用，普通函数完全隔离
2. **World 严格隔离** - 每个包有自己的 ECS world，entity 只能拥有其所在 world 的组件
3. **World 自动推断** - 编译器自动推断 ECS 操作应该使用哪个 world
4. **零成本抽象** - 生成的 C 代码直接调用运行时，无额外开销

## 语言特性总览

### 1. 包管理

#### use 声明
```paani
use engine;              // 导入 engine 包
use engine as e;         // 导入并设置别名
```

**包隔离规则：**
- 普通函数 (`fn`)：**不能**跨包访问，即使 `use` 了包也不行
- 导出函数 (`export fn`)：可以通过 `package.funcName()` 访问
- ECS 对象 (data/trait/template)：main 包可以通过 `package.Name` 访问
- extern 函数/类型：只在声明的包内部可见

**不同包的 use 行为：**
```paani
// main 包（入口点）- use 加载整个包
use engine;  // 加载所有导出符号（包括 ECS 对象、函数等）

// 非 main 包 - use 只加载 utils/ 目录下的函数
use engine;  // 只加载 engine.utils/ 下的函数
// 不能访问 engine 的 ECS 对象（data/trait/template）
```

#### export 规则
```paani
// 可导出的符号（带 export 关键字）
export data Position { x: f32, y: f32 }
export trait Movable [Position, Velocity]
export template Player { Position, Velocity }
export handle File with fclose
export fn public_api() -> i32 { return 42; }

// 不可导出的符号（包内私有）
fn internal_func() {}           // 普通函数 - 本包内可用
system mySystem() {}            // system - 本包内可用
extern fn my_func() {}          // extern 函数 - 本包内可用
data LocalData { x: i32 }       // data - 本包内可用
```

### 2. World 自动推断

Paani 支持多 World 架构，每个包有自己的 ECS world。编译器会自动推断 ECS 操作应该使用哪个 world。

#### World 推断规则

| 操作 | World 推断规则 | 示例 |
|------|---------------|------|
| `spawn` | 空 spawn 使用当前包 world | `let e = spawn` → `__paani_gen_this_w` |
| `spawn Template` | 使用模板所属包的 world | `spawn engine.PlayerTemplate` → `__paani_gen_engine_w` |
| `give e Component` | 使用组件所属包的 world | `give e engine.Health` → `__paani_gen_engine_w` |
| `tag e as Trait` | 使用 trait 所属包的 world | `tag e as engine.Movable` → `__paani_gen_engine_w` |
| `e has Component` | 使用组件所属包的 world | `e has engine.Health` → `__paani_gen_engine_w` |
| `destroy e` | 使用当前包 world | `destroy e` → `__paani_gen_this_w` |

#### 跨包 ECS 操作示例

**错误示例（编译失败）：**

```paani
// ❌ 错误：main world 的 entity 不能拥有 engine world 的组件
let player = spawn;  // main world 的 entity
give player engine.Health: {hp: 100};  // 编译错误！World 不匹配
```

**正确示例：**

```paani
// main.paani
use engine;

// 通过 engine 的 export fn 创建 engine world 的 entity
let player = engine.create_player();
// player 自动拥有 engine.Health 和 engine.Movable

// 在 main 包中遍历 engine world 的实体
for engine.Movable in updateEngineEntities(dt: f32) as obj {
    // 可以读取/修改 engine 的组件
    obj.Position.x += 1.0;
}

// 检查 engine 包的组件
if (player has engine.Health) {
    // ...
}
```

生成的 C 代码：
```c
// 调用 engine.create_player() - 在 engine world 创建 entity
paani_entity_t player = engine__create_player(__paani_gen_engine_w);
// player 自动拥有 engine.Health（在 engine world 中）

// 遍历 engine.Movable - 使用 engine 的 world
paani_query_t* __query = paani_query_trait(__paani_gen_engine_w, s_trait_engine__Movable);

// 检查组件 - 使用 engine 的 world
if (paani_component_has(__paani_gen_engine_w, player, s_ctype_engine__Health))
```

#### World 一致性检查

编译器会在语义分析阶段检查 world 一致性：
- **Entity 只能拥有其所在 world 的组件和 trait**
- main 包可以操作其他包的 ECS 对象，但不能跨 world 给 entity 添加组件
- 非 main 包只能操作本包的 ECS 对象

```paani
// ❌ 错误：给 main world 的 entity 添加 engine world 的组件
let player = spawn;  // main world 的 entity
give player engine.Health: {hp: 100};  // 编译错误！World 不匹配

// ✅ 正确：通过 export fn 让 engine 包创建 entity
let engine_player = engine.create_player();  // engine world 的 entity
// engine_player 自动拥有 engine.Health，无需手动添加
```

#### 导出函数的 World 参数

所有 `export fn` 在生成的 C 代码中会自动添加 world 参数：

```paani
// Paani 代码
export fn create_player() -> entity {
    let player = spawn;
    give player Health: {hp: 100};
    return player;
}
```

```c
// 生成的 C 代码
paani_entity_t main__create_player(paani_world_t* __paani_gen_world) {
    paani_entity_t player = paani_entity_create(__paani_gen_world);
    paani_component_add(__paani_gen_world, player, s_ctype_main__Health, &__comp_data);
    return player;
}
```

### 3. 编译器架构

Paani 编译器采用经典的前端-后端架构：

```
Paani 源码 → Lexer → Parser → AST → Sema → CodeGen → C 代码 → GCC → 可执行文件
```

#### 编译流程

1. **词法分析 (Lexer)** - 将源码转换为 Token 流
2. **语法分析 (Parser)** - 构建抽象语法树 (AST)
3. **语义分析 (Sema)** - 类型检查、符号解析、World 推断
4. **代码生成 (CodeGen)** - 生成 C 代码
5. **编译链接** - 调用 GCC 生成可执行文件

#### 编译命令

```bash
# 创建新项目
paani new mygame

# 编译所有库包（生成 .c/.h 文件到 output/）
paani build

# 编译 main 包并链接生成可执行文件
paani link

# 清理生成文件
paani clean

# 运行
paani run
```

#### 生成的文件结构

```
output/
├── engine.c          # engine 包实现
├── engine.h          # engine 包头文件
├── game.c            # game 包实现
├── game.h            # game 包头文件
├── main.c            # main 包实现（入口点）
├── main.h            # main 包头文件
└── Makefile          # 构建脚本
```

#### 全局语句执行时机

**main 包**（入口点）和 **普通包** 的全局语句执行时机不同：

**main 包** - 在 `main()` 函数中执行：
```c
int main() {
    // 1. 创建所有 world
    __paani_gen_engine_w = paani_world_create();
    __paani_gen_game_w = paani_world_create();
    __paani_gen_this_w = paani_world_create();
    
    // 2. 初始化所有包（执行各包的 module_init，包括普通包的全局语句）
    engine__paani_module_init(__paani_gen_engine_w);  // 执行 engine 包的全局语句
    game__paani_module_init(__paani_gen_game_w);      // 执行 game 包的全局语句
    main__paani_module_init(__paani_gen_this_w);      // 不执行全局语句（在下一步）
    
    // 3. 执行 main 包的全局初始化代码
    // 这里执行 main.paani 中的所有全局语句
    paani_entity_t timer_entity = paani_entity_create(__paani_gen_this_w);
    ...
    
    // 4. 进入主游戏循环
    while (running) {
        // ...
    }
}
```

**普通包** - 在 `package__paani_module_init()` 最后执行：
```c
void engine__paani_module_init(paani_world_t* __paani_gen_world) {
    // 1. 注册组件、trait、system
    // ...
    
    // 2. 设置 system 依赖
    // ...
    
    // 3. 执行包的全局初始化代码（最后执行）
    // 这里执行 engine/*.paani 中的所有全局语句
    paani_entity_t entity = paani_entity_create(__paani_gen_world);
    ...
}
```

**注意：**
- 全局语句只在程序启动时执行一次，用于初始化实体、组件等
- 游戏逻辑应该放在 system 中
- 普通包的全局语句在 module_init 最后执行，此时该包的 ECS 对象已注册完成

**全局语句变量的作用域限制（重要设计）：**

全局语句中定义的变量**仅在该语句内部可见**，不能跨语句或跨函数访问：

```paani
// main/main.paani
let counter = 0;
let next = counter + 1;  // ✅ 可以访问同文件中之前定义的变量

fn get_count() -> i32 {
    // return counter;  // ❌ 编译错误！全局语句变量在函数中不可见
    return 0;
}
```

**设计理念：**
- **强制 ECS 架构**：所有需要长期存在的状态必须通过 ECS（entity-component）管理
- **避免全局状态**：禁止随意定义全局可访问的变量，减少耦合和副作用
- **明确生命周期**：全局语句是独立的执行单元，其变量生命周期仅限于该语句

如果需要跨函数共享状态，应该：
1. 使用 ECS：将数据存储在 entity 的 component 中
2. 使用 export fn：通过函数参数传递数据
3. 使用 query：在 system 中查询需要的数据

#### 全局语句中的 Handle

在全局语句中定义的 handle，其析构时机因包类型不同而异：

**main 包全局语句中的 handle：**
- **生命周期**：整个程序运行期间
- **析构时机**：程序退出时（cleanup 阶段）
- 适合用于需要长期存在的资源（如全局配置、日志文件等）

```paani
// main/main.paani
extern fn fopen(path: string, mode: string) -> File;
extern fn fclose(file: File) -> void;
handle File with fclose;

let global_log = fopen("game.log", "w");  // 程序退出时才 fclose
```

**普通包全局语句中的 handle：**
- **生命周期**：`module_init` 执行期间
- **析构时机**：`module_init` 结束时立即调用
- 适合用于临时初始化操作（如加载配置、初始化资源等）

```paani
// engine/core.paani
fn load_config() -> i32 {
    let config_file = fopen("engine.cfg", "r");  // 临时文件
    // 读取配置...
    return 0;  // config_file 在这里 fclose
}
```

**设计理念：**
- **main 包**拥有程序生命周期控制权，其全局资源可存活到程序结束
- **普通包**是库/模块，其全局初始化代码应该快速完成并释放临时资源
- 所有 handle 都有**确定的生命周期**，无需手动管理，避免内存泄漏

### 4. 类型系统

#### 基本类型
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
- `string` 和 `entity` 是语言内置的特殊类型，由运行时管理
- `void` **只能**用于函数返回类型，**不能**用于变量声明

#### 数组类型
```paani
// 数组字面量（仅用于初始化）
let arr = [1, 2, 3, 4, 5]
let empty = []

// 数组索引
let first = arr[0]
arr[1] = 10
```

#### 外部类型
```paani
// 不透明类型，指定大小
extern type Window[64]
extern type Texture[32]
```

**代码生成规则：**
```c
// 生成的C代码
// Extern type: Window (64 bytes)
typedef struct { uint8_t _[64]; } package__Window;

// Extern type: Texture (32 bytes)
typedef struct { uint8_t _[32]; } package__Texture;
```

- 生成不透明结构体，使用字节数组占位
- 大小由声明时的`[size]`指定
- 通过`package__TypeName`访问（带包名前缀）

#### Handle 类型（自动资源管理）



Paani 的 handle 类型实现了**确定性的资源生命周期管理**，无需手动调用释放函数。



```paani

// 声明带析构函数的资源类型

handle File with fclose

handle Buffer with free_buffer



// 在组件中使用

data Asset {

    file: File,

    data: Buffer

}

// 当实体被销毁时，自动调用 fclose 和 free_buffer

```



**设计理念：**







- **RAII（资源获取即初始化）**：资源与生命周期绑定



- **确定性析构**：编译期确定析构时机，非垃圾回收



- **零开销**：生成的 C 代码直接插入析构调用，无运行时开销



- **包隔离**：handle 类型只在声明的包内可见



- **无悬空指针**：通过作用域控制，确保不会访问已释放的资源







**代码生成规则：**







| 使用场景 | 析构时机 | 生成的 C 代码 |



|---------|---------|--------------|



| 组件字段 | 实体销毁时 | 组件析构函数中调用 |



| 局部变量 | 作用域结束时 | 代码块末尾插入 |



| main 包全局语句 | 程序退出时 | `main()` 的 cleanup 阶段 |



| 普通包全局语句 | `module_init` 结束时 | 立即调用（临时资源） |







**重要限制：**







由于全局语句变量对外不可见，全局语句中的 handle 只能用于：



- **临时初始化操作**（如读取配置、加载数据）



- **main 包的长期资源**（日志文件、全局配置等）







不能将全局语句中的 handle 传递给外部使用，因为：



1. 变量本身对外不可见



2. handle 在语句结束时可能已被析构（普通包）



3. 违反 ECS 设计理念（状态应该保存在 component 中）

| 普通包全局语句 | `module_init` 结束时 | `module_init()` 末尾插入 |



**1. 组件中的 Handle**

```c

// 组件析构函数（自动注册到ECS运行时）

static void Asset_dtor(void* ptr) {

    Asset* self = (Asset*)ptr;

    if (self->data != NULL) { free_buffer(self->data); self->data = NULL; }

    if (self->file != NULL) { fclose(self->file); self->file = NULL; }

}

```



**2. 局部变量 Handle**

```paani

fn test() {

    let f: File = fopen("test.txt");

    // ...

} // 自动调用: if (f != NULL) { fclose(f); f = NULL; }

```



**3. 全局语句中的 Handle**

```paani

// main 包 - 程序退出时才析构

let log = fopen("game.log", "w");  // 长期存活



// 普通包 - module_init 结束时立即析构

let config = fopen("temp.cfg", "r");  // 临时使用

```



**规则：**

- Handle 本质是 `void*` 指针

- 析构函数按声明顺序**逆序**调用（LIFO）

- 被 `move` 的 handle 不会调用析构函数（所有权转移）

- 析构前检查 `!= NULL`，析构后设为 `NULL`（防重复释放）



**注意：** `with` 后的析构函数**必须**先通过 `extern fn` 声明，编译器**会检查签名**：
- 返回类型必须是 `void`
- 必须有且仅有 **1 个参数**

```paani
// 正确
extern fn fclose(file: File) -> void;
handle File with fclose;

// 错误：返回类型不是 void
extern fn bad_return(x: File) -> i32;
handle Handle1 with bad_return;  // 编译错误！

// 错误：参数数量不对
extern fn bad_params(x: File, y: i32) -> void;
handle Handle2 with bad_params;  // 编译错误！

// 错误：没有参数
extern fn no_params() -> void;
handle Handle3 with no_params;  // 编译错误！
```

### 5. ECS 核心

#### Data（组件）
```paani
data Position {
    x: f32,
    y: f32
}

data Velocity {
    vx: f32,
    vy: f32
}
```

**跨包组件访问：**
```paani
use engine;

// 使用其他包的组件（必须使用 package.Component 语法）
let player = spawn;
give player engine.Position: {x: 0, y: 0};

// 检查其他包的组件
if (player has engine.Position) {
    // ...
}
```

#### Trait（特征标签）
```paani
// 纯标签
trait Enemy
trait Player

// 带依赖的 trait（语法要求方括号）
trait Movable [Position, Velocity]
trait Living [Health]
```

**重要：** Trait 只是标签！不会自动添加组件：
```paani
let e = spawn
give e Position: {x: 0, y: 0}
give e Velocity: {vx: 1, vy: 0}
tag e as Movable   // OK：现在可以标记为 Movable
// e 必须有 Position 和 Velocity 才能 tag 为 Movable
```

**跨包 trait 访问：**
```paani
use engine;

// 使用其他包的 trait
tag player as engine.Movable;

// 在其他包的 trait system 中操作
for engine.Movable in mySystem(dt: f32) as obj {
    // obj 可以访问 engine.Position 和 engine.Velocity
}
```

#### Template（实体模板）
```paani
template Player {
    Position: { x: 100.0, y: 200.0 },
    Velocity: { vx: 0.0, vy: 0.0 },
    tags: [Movable, Living, Player]
}

template EnemyGrunt {
    Position,                           // 使用字段默认值
    Velocity: { vx: -50.0, vy: 0.0 },
    tags: [Movable, Enemy]
}
```

### 4. 实体操作

```paani
// 创建实体
let e1 = spawn                    // 空实体
let player = spawn Player         // 从模板创建

// 添加组件
give player Position: { x: 10.0, y: 20.0 }
give player Weapon              // 添加空组件

// Tag/Untag（语法要求 "as"）
tag player as Elite
untag player as Movable

// 检查组件/Tag（后缀语法）
if (player has Position) { }
if (player !has Boss) { }        // 检查没有

// 销毁（延迟销毁）
destroy player
destroy                        // 销毁当前实体（在 system 中）
```

### 6. System 定义

#### 普通 System
```paani
system initSystem() {

}
```

#### Trait System（遍历）
```paani
// 语法：for Trait in Name(params) as var { ... }
// 必须指定 "as" 实体变量名
for Movable in moveSystem(dt: f32) as obj {
    obj.Position.x += obj.Velocity.vx * dt * 0.001
    obj.Position.y += obj.Velocity.vy * dt * 0.001
}
```

**跨包 trait system：**
```paani
use engine;

// 使用其他包的 trait 定义 system
// 这会遍历 engine 包 world 中所有带有 Movable trait 的实体
for engine.Movable in updateEngineEntities(dt: f32) as obj {
    obj.Position.x += 1.0;
}
```

**注意：** 普通包的 trait system 默认是被锁定的，需要 `unlock` 才能运行。main 包的 trait system 自动运行。

#### #batch System（SIMD 优化）
```paani
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

编译器会检查并警告。

#### System 依赖
```paani
// 语法：depend A -> B -> C;
depend inputSystem -> updatePosition -> collisionSystem -> renderSystem
```

### 6. Query（查询）

```paani
// 语法：query Trait with Comp as var { ... }
query Living with Health as patient {
    if (patient.Health.hp < patient.Health.max) {
        patient.Health.hp += 5.0 * dt * 0.001
    }
}

// 多 trait 查询（逗号分隔）
query (Movable, Living) as entity {
    // ...
}
```

### 7. 函数

#### 普通函数
```paani
fn add(x: i32, y: i32) -> i32 {
    return x + y
}
```

#### 导出函数（带隐式 world 参数）
```paani
// C 调用：paani_entity_t game__create_player(paani_world_t* world)
export fn create_player() -> entity {
    let player = spawn Player
    return player
}

export fn create_enemy_at(x: f32, y: f32) -> entity {
    let e = spawn
    give e Position: {x: x, y: y}
    return e
}
```

#### 外部 C 函数
```paani
// 声明（注意：不支持可变参数 ...）
extern fn sin(x: f64) -> f64
extern fn cos(x: f64) -> f64
extern fn printf(fmt: string) -> i32  // 编译器特殊处理
```

**重要规则：**
- extern 函数**只在声明它的包内部可见**
- extern 函数**不能**被其他包通过 `use` 访问
- extern 函数**不需要**也不能被 `export`

**函数调用优先级（同名函数）：**

当 `extern fn` 和普通 `fn` 同名时，调用优先级如下：

```paani
// 声明外部 C 函数
extern fn foo() -> void;

// 定义同名的 Paani 函数
fn foo() {
    // Paani 实现
}

fn test() {
    foo();  // 调用的是 extern fn（外部 C 函数）！
}
```

**优先级顺序：**
1. `extern fn` - 最高优先级（直接调用外部 C 函数）
2. `fn` / `export fn` - 次优先级（调用 `package__func`）

**实用场景：无缝封装外部库**

```paani
// 声明外部 C 库函数（默认使用）
extern fn sin(x: f64) -> f64;
extern fn cos(x: f64) -> f64;

// 使用
fn test() {
    let a = sin(1.0);      // 调用 extern fn（C 库版本）
    let c = cos(3.14159);  // 调用 extern fn（C 库版本）
}
```

**注意：** Paani 是强类型语言，**不支持**语言层面的类型转换（如 `x as f64`）。
如果需要类型转换，需要在 C 层处理或定义包装函数：

```c
// 在 C 代码中提供类型转换包装
float sin_f32(float x) {
    return (float)sin((double)x);
}
```

```paani
// 然后声明并使用
extern fn sin_f32(x: f32) -> f32;

fn test() {
    let b = sin_f32(1.0f);  // 使用 f32 版本
}
```

这种设计允许你：
- 默认使用高性能的 C 库函数
- 通过不同函数名提供多类型版本
- 无需改名，无缝切换

### 8. 控制流

```paani
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

// 变量声明
let x: i32 = 10       // 不可变
let y = 20            // 类型推断
var z: f32 = 1.5      // 可变

// 返回
return x
return                  // void 函数

// 循环控制
break
continue

// 退出游戏（只能在 main 包中使用）
exit
```

### 9. 运算符

```paani
// 算术
+ - * / %

// 比较
== != < > <= >=

// 逻辑
&& || !

// 位运算
& | ^ << >> ~

// 赋值
= += -= *= /=
```

### 10. 模板字符串（f-string）

```paani
let name = "World"
let score = 95.5

let msg = f"Hello {name}!"           // "Hello World!"
let info = f"Score: {score}"         // "Score: 95.50"
let total = f"Total: {1 + 2 + 3}"   // "Total: 6"
```

### 12. Lock/Unlock

**System 锁定规则：**
- **普通包**（库）：system **默认锁定**（需要 `unlock` 才能运行）
- **main 包**（入口点）：system **默认解锁**（自动运行，无需手动 unlock）

```paani
// main 包的 system 自动运行，无需 unlock
for Player in playerSystem(dt: f32) as e {
    // 自动执行
}

// 使用其他包的 system（需要 unlock）
use engine;

// engine 包的 system 默认锁定
unlock engine.physicsSystem     // 解锁后才能运行

// 锁定 system（暂停执行）
lock engine.physicsSystem
```

**应用场景：**
- 游戏暂停：lock 所有 system
- 场景切换：lock 当前场景 system，unlock 新场景 system
- 条件执行：根据游戏状态动态控制 system 运行

**设计理念：**
- 普通包作为库提供功能，但默认不自动运行（避免副作用）
- main 包控制整个程序流程，其 system 自动运行
- 跨包使用 system 需要显式 unlock，明确控制权限

## CLI 命令

```bash
paani new <name>          # 创建新项目
paani build [pkg]         # 编译所有包或指定包
paani link                # 编译 main 包并生成可执行文件
paani clean               # 清理生成的文件
```

## 项目结构

```
mygame/
├── engine/               # engine 包
│   ├── core.paani        # ECS 定义、system
│   └── utils/
│       └── math.paani    # utils 目录：只能放函数
│   └── libs/
│       └── libcore.o     # libs 目录：放依赖的库，编译时自动链接
├── game/                 # game 包
│   ├── logic.paani
│   └── utils/
│       └── helpers.paani
├── main/                 # main 包（入口点，特殊）
│   └── main.paani        # 全局初始化代码、跨包操作
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
- `utils/` 子目录**只能**包含普通函数，不能包含 ECS 对象（data/trait/template/system）
- `main/` 包是入口点，包含全局初始化代码和跨包 ECS 操作
- 每个包有自己的 ECS world，main 包可以操作所有 world
- 普通包的 system 默认 **lock**，需要 `unlock` 才能运行；main 包的 system 自动运行

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

## 编译器生成变量

所有以 `__paani_gen_` 开头的标识符都是编译器保留的，用户代码禁止使用此前缀。

### World 变量

| 变量 | 说明 | 使用场景 |
|------|------|---------|
| `__paani_gen_world` | 函数 world 参数 | export fn 参数 |
| `__paani_gen_this_w` | 当前包 world | main 包全局代码 |
| `__paani_gen_engine_w` | engine 包 world | engine 包 ECS 操作 |
| `__paani_gen_game_w` | game 包 world | game 包 ECS 操作 |

### 类型 ID 变量

| 变量 | 说明 |
|------|------|
| `s_ctype_package__DataName` | 组件类型 ID |
| `s_trait_package__TraitName` | trait 类型 ID |

### 生成的函数

| 函数 | 说明 |
|------|------|
| `package__spawn_TemplateName(world)` | 模板 spawn 函数 |
| `package__paani_module_init(world)` | 包初始化函数 |

### 示例

```c
// main.c 中的 world 变量
static paani_world_t* __paani_gen_this_w = NULL;     // main 包 world
static paani_world_t* __paani_gen_engine_w = NULL;   // engine 包 world
static paani_world_t* __paani_gen_game_w = NULL;     // game 包 world

// 组件类型 ID
paani_ctype_t s_ctype_main__Timer = (paani_ctype_t)-1;
paani_ctype_t s_ctype_engine__Health = (paani_ctype_t)-1;

// trait 类型 ID
paani_trait_t s_trait_main__AutoExit = (paani_trait_t)-1;
paani_trait_t s_trait_engine__Movable = (paani_trait_t)-1;
```

## 示例程序

### 基本示例

```paani
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

```paani
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

```paani
// main/main.paani
use engine;

// 正确的跨包使用方式：通过 export fn 创建 engine world 的 entity
let player = engine.create_player();
// player 自动拥有 Position, Health 组件和 Movable, Living trait

// 主循环 system - 遍历 engine world 中带有 Living trait 的实体
for engine.Living in healSystem(dt: f32) as e {
    // 恢复生命值
    if (e.Health.hp < e.Health.max_hp) {
        e.Health.hp += 1.0 * dt * 0.001;
    }
}

// 导出函数
export fn get_player_health() -> f32 {
    // 查询 engine.Living trait
    query engine.Living as e {
        return e.Health.hp;
    }
    return 0.0;
}
```

## 注意事项

1. **entity 类型不能用于组件字段** - 使用 `u64` 存储实体 ID
2. **#batch 有限制** - 不能使用 if/spawn/destroy/函数调用等
3. **trait 只是标签** - `tag` 不会自动添加组件，必须先用 `give`
4. **dt 单位是毫秒** - 计算时通常需要 `* 0.001` 转换为秒
5. **destroy 是延迟的** - 当前帧结束时才真正销毁
6. **void 只能用于函数返回** - 不能声明 void 变量（如 `let a: void;` 会报错）
7. **不支持指针类型** - 没有 `*T` 语法
8. **extern 函数只在当前包可见** - 不能被其他包通过 `use` 访问，也不需要 `export`
9. **World 严格隔离** - entity 只能拥有其所在 world 的组件和 trait，不能跨 world 操作
10. **包前缀语法** - 使用其他包的 ECS 对象时必须加包前缀（如 `engine.Health`）
11. **main 包调用权限** - main 包可以通过 `export fn` 调用其他包的功能，但不能直接给其他包的 entity 添加组件
12. **exit 只能在 main 包使用** - 普通包（非 entry point）中使用 `exit` 会导致编译错误
