# Concurrent Data Structures in C

用 C11 实现的一组并发数据结构，用于学习操作系统并发原语（mutex、条件变量、原子操作、内存序），并对比不同实现方案的取舍。

## 目录

- [设计原则](#设计原则)
- [已实现的结构](#已实现的结构)
  - [Blocking Queue（有界阻塞队列）](#blocking-queue有界阻塞队列)
  - [SPSC Queue（无锁单生产者单消费者队列）](#spsc-queue无锁单生产者单消费者队列)
  - [Hash Map（单线程哈希表）](#hash-map单线程哈希表)
  - [Locked Hash Map（全局锁哈希表）](#locked-hash-map全局锁哈希表)
  - [Striped Hash Map（分段锁哈希表）](#striped-hash-map分段锁哈希表)
- [示例](#示例)
  - [echo_server](#echo_server)
  - [char_count](#char_count)
- [验证方法](#验证方法)
- [目录结构](#目录结构)
- [构建与测试](#构建与测试)
- [规划](#规划)
- [参考](#参考)

## 设计原则

- **C11 标准**：`<stdatomic.h>`、`<stdalign.h>`、`<pthread.h>`
- **接口最小**：每个结构只暴露创建 / 操作 / 销毁接口
- **明确所有权**：容器只存 `void*`，不负责数据的分配与释放
- **明确前提**：文档写清「调用前提」和「局限」
- **可验证**：每个结构配功能测试 + TSan + Valgrind

## 已实现的结构

### Blocking Queue（有界阻塞队列）

**定位**：通用、多生产者多消费者、阻塞式有界队列。

| 项 | 说明 |
|----|------|
| 实现 | 环形数组 + mutex + 两个条件变量 |
| 线程模型 | MPMC |
| 空 / 满行为 | 阻塞等待 |
| 数据所有权 | 调用者负责 |

**核心设计**：`not_empty` / `not_full` 两个条件变量分别处理空、满；`head` / `tail` / `count` 由 mutex 保护。

**局限**：消费者在空队列上永久阻塞，无关闭语义；`destroy` 前必须无并发使用者；不释放 `data`。

**验证**：功能测试、TSan 无 race、Valgrind 0 leak / 0 error。

### SPSC Queue（无锁单生产者单消费者队列）

**定位**：单生产者单消费者、非阻塞、高吞吐。

| 项 | 说明 |
|----|------|
| 实现 | 环形数组 + `_Atomic` head/tail + acquire/release |
| 线程模型 | SPSC（严格 1P1C） |
| 空 / 满行为 | 非阻塞，返回 `NULL` / `false` |
| 数据所有权 | 调用者负责 |

**核心设计**：

- `head` 只被消费者写、`tail` 只被生产者写 → 无需 CAS
- `head` / `tail` 分处不同 cache line → 避免 false sharing
- 生产者写 `tail` 用 `release`、消费者读 `tail` 用 `acquire` → 保证 buffer 可见顺序
- 预分配数组 → 无内存回收问题

**局限**：只支持 1P1C；非阻塞，调用者自己处理满 / 空；有界。

**验证**：功能测试、TSan 0 warning、Valgrind 0 leak / 0 error。对照 `naive_queue`（无同步）TSan 报 3 处 race，说明原子和内存序是消除 race 的必需手段。

### Hash Map（单线程哈希表）

**定位**：单线程链地址法哈希表，作为后续并发版本的功能与性能基线。

| 项 | 说明 |
|----|------|
| 实现 | 桶数组 + 链表（链地址法） |
| 线程安全 | ❌ 单线程 |
| 扩容 | 负载因子 > 0.75 时翻倍 |
| 数据所有权 | 调用者负责 |

**核心设计**：调用者提供 `hash_fn` / `equal_fn`；`put` 先查 key 存在则更新，否则头插新建节点；扩容时所有节点重新 hash。

**局限**：单线程；`destroy` 不释放 key / value。

**验证**：功能测试、Valgrind 0 leak / 0 error。

### Locked Hash Map（全局锁哈希表）

**定位**：用一把全局 mutex 保护整个哈希表的并发版本，作为并发哈希表的基线——最简单，并发度最低。

| 项 | 说明 |
|----|------|
| 实现 | 单线程版 + `pthread_mutex_t` |
| 线程模型 | 多线程，全局锁串行化 |
| 数据所有权 | 调用者负责 |

**核心设计**：所有公开接口在入口加锁、出口解锁；`resize` 是 static 内部函数，继承调用者的锁状态，不自己加锁；`destroy` 不加锁，前提是外部保证无并发使用者。

**局限**：并发度 = 1（同一时刻只有一个线程能操作）；扩容时持锁时间长，是性能瓶颈。

**验证**：并发测试（4 线程 × 1 万 key，size 精确）、TSan 0 warning、Valgrind 0 leak / 0 error。

### Striped Hash Map（分段锁哈希表）

**定位**：把哈希表分成 N 段，每段一个独立小哈希表 + 一把锁。操作时只锁目标段，不同段可并发。作为"锁粒度优化"的对比版本。

| 项 | 说明 |
|----|------|
| 实现 | N 个独立 `Stripe`（桶数组 + size + mutex） |
| 线程模型 | 多线程，段间并发 |
| 并发度 | 理想情况 = 段数 |
| 数据所有权 | 调用者负责 |

**核心设计**：

- 先算段号（`hash % num_stripes`），只锁目标段
- 每段独立扩容，不影响其他段
- `size()` 遍历各段求和，是近似值
- 提供 `striped_hashmap_add`：对 key 的计数原子加 delta，用于计数场景

**局限**：段内仍串行；`size()` 不精确；段数过多反而变慢（管理开销）。

**验证**：并发测试（8 线程 × 50 万 key，size 精确）、TSan 0 warning、Valgrind 0 leak / 0 error。

**性能对比**（4 核，200 万 ops，桶数对等 1048576）：

| 配置 | put | get |
|------|-----|-----|
| 全局锁 | 0.677s | 0.152s |
| 分段锁（4 段） | 0.778s | 0.144s |
| 分段锁（8 段） | 0.854s | 0.219s |

**结论**：

- **get（纯读）**：分段锁快，并发读无冲突
- **put（写 + malloc）**：全局锁反而快，因为多线程并发 `malloc` 时分配器内部锁竞争激烈
- **段数 = 核数时最优**（4 段 > 8 段），段数超过核数后管理开销抵消并发收益
- 每段桶数太少会导致频繁扩容，反而拖累性能

**分段锁不是"一定快"，适合读多写少、分配少的场景。**

## 示例

`examples/` 下是验证性示例，用于展示数据结构在真实场景下的可用性。

### echo_server

用 `blocking_queue` 搭的并发 echo 服务器：主线程 accept 连接，把 fd 放入队列，4 个工作线程从队列取出并处理。

**编译**：

```bash
gcc -Wall -Wextra -g -O2 -pthread -std=c11 \
    examples/echo_server.c src/blocking_queue.c \
    -Iinclude -o echo_server
```

### char_count

用 `striped_hash_map` 做的并发字符统计：主线程读文件，按字节切成 N 段，
4 个线程各统计一段中 a-z 的出现次数，最后汇总。

```bash
gcc -Wall -Wextra -g -O2 -pthread -std=c11 \
    examples/char_count.c src/striped_hash_map.c \
    -Iinclude -o char_count

./char_count examples/sample.txt
```

## 验证方法

| 手段 | 目的 |
|------|------|
| 功能测试 | 验证 FIFO、满 / 空、边界、扩容 |
| TSan | 检测 data race |
| Valgrind | 检测内存泄漏、越界 |

**TSan 说明**：TSan 检测访问模式，不依赖「运行时真的撞上」。x86 上运行时可能碰巧正确，但 TSan 能确定性报出 race。

## 目录结构

```text
concurrent-data-structure/
├── README.md
├── include/
│   ├── blocking_queue.h
│   ├── spsc_queue.h
│   ├── hash_map.h
│   ├── locked_hash_map.h
│   ├── striped_hash_map.h
│   └── naive_queue.h
├── src/
│   ├── blocking_queue.c
│   ├── spsc_queue.c
│   ├── hash_map.c
│   ├── locked_hash_map.c
│   ├── striped_hash_map.c
│   └── naive_queue.c
├── tests/
│   └── ...
└── examples/
    ├── echo_server.c
    ├── char_count.c
    └── sample.txt
```

## 构建与测试

统一编译参数：`-Wall -Wextra -pthread -std=c11`；功能测试用 `-O2`，Valgrind 用 `-O0 -g`，TSan 用 `-fsanitize=thread -O1 -g`。

### Blocking Queue

```bash
gcc -Wall -Wextra -g -O2 -pthread -std=c11 \
    tests/test_blocking_queue.c src/blocking_queue.c \
    -Iinclude -o test_blocking_queue
./test_blocking_queue

# Valgrind
gcc -Wall -Wextra -g -O0 -pthread -std=c11 \
    tests/test_blocking_queue.c src/blocking_queue.c \
    -Iinclude -o test_blocking_queue_dbg
valgrind --leak-check=full ./test_blocking_queue_dbg
```

### SPSC Queue

```bash
# 功能测试
gcc -Wall -Wextra -g -O2 -pthread -std=c11 \
    tests/test_spsc.c src/spsc_queue.c \
    -Iinclude -o test_spsc
./test_spsc

# TSan（SPSC：期望 0 warning）
gcc -fsanitize=thread -g -O1 -pthread -std=c11 \
    tests/test_spsc_tsan.c src/spsc_queue.c \
    -Iinclude -o spsc_tsan
setarch $(uname -m) -R ./spsc_tsan

# TSan（naive：期望 3 warning）
gcc -fsanitize=thread -g -O1 -pthread -std=c11 \
    tests/test_naive_tsan.c src/naive_queue.c \
    -Iinclude -o naive_tsan
setarch $(uname -m) -R ./naive_tsan

# Valgrind
gcc -Wall -Wextra -g -O0 -pthread -std=c11 \
    tests/test_spsc.c src/spsc_queue.c \
    -Iinclude -o test_spsc_dbg
valgrind --leak-check=full ./test_spsc_dbg
```

### Hash Map / Locked Hash Map / Striped Hash Map

```bash
# 单线程哈希表
gcc -Wall -Wextra -g -O2 -pthread -std=c11 \
    tests/test_hash_map.c src/hash_map.c \
    -Iinclude -o test_hash_map
./test_hash_map

# 全局锁哈希表：并发正确性 + TSan
gcc -fsanitize=thread -g -O1 -pthread -std=c11 \
    tests/test_locked_hash_map.c src/locked_hash_map.c \
    -Iinclude -o locked_hash_map_tsan
setarch $(uname -m) -R ./locked_hash_map_tsan

# 分段锁哈希表：并发 + 性能对比（需同时编译 locked 版）
gcc -Wall -Wextra -g -O2 -pthread -std=c11 \
    tests/test_striped_hash_map.c \
    src/striped_hash_map.c src/locked_hash_map.c \
    -Iinclude -o test_striped_hash_map
./test_striped_hash_map
```

## 规划

**数据结构**：

- ☑ Blocking Queue（MPMC，有界阻塞）
- ☑ SPSC Queue（无锁，非阻塞）
- ☑ Hash Map（单线程基线）
- ☑ Locked Hash Map（全局锁并发版）
- ☑ Striped Hash Map（分段锁并发版）

**示例**：

- ☑ echo_server（基于 blocking_queue）
- ☑ char_count（基于 striped_hash_map）


## 参考

- CSAPP 第 12 章（并发编程）
- C11 标准 `<stdatomic.h>`
- OSTEP 第 29 章（并发数据结构）
