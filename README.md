# Concurrent Data Structures in C

用 C11 实现的一组并发数据结构，用于学习操作系统并发原语（mutex、条件变量、原子操作、内存序），并对比不同实现方案的取舍。

## 目录

- [设计原则](#设计原则)
- [已实现的结构](#已实现的结构)
  - [Blocking Queue（有界阻塞队列）](#blocking-queue有界阻塞队列)
  - [SPSC Queue（无锁单生产者单消费者队列）](#spsc-queue无锁单生产者单消费者队列)
- [验证方法](#验证方法)
- [目录结构](#目录结构)
- [构建与测试](#构建与测试)
- [规划](#规划)
- [参考](#参考)

## 设计原则

- **C11 标准**：使用 `<stdatomic.h>`、`<stdalign.h>`、`<threads.h>`（可选）
- **接口最小**：每个结构只暴露必要的创建 / 操作 / 销毁接口
- **明确所有权**：容器只存 `void*`，不负责数据的分配与释放
- **明确前提**：每个结构在文档里写清「调用前提」和「局限」
- **可验证**：每个结构配功能测试 + TSan + Valgrind

## 已实现的结构

### Blocking Queue（有界阻塞队列）

**定位**：通用、多生产者多消费者、阻塞式有界队列。

| 项 | 说明 |
|----|------|
| 实现 | 环形数组 + `pthread_mutex_t` + 两个条件变量 |
| 容量 | 有界，创建时指定 |
| 线程模型 | MPMC（多生产者多消费者） |
| 空 / 满行为 | 阻塞等待（条件变量） |
| 数据所有权 | 调用者负责 |

**核心设计**：

- `not_empty`：队列空时消费者等待
- `not_full`：队列满时生产者等待
- `head` / `tail` / `count` 由 mutex 保护

**局限**：

- 消费者在空队列上会永久阻塞，无关闭语义
- `destroy` 前必须保证无并发使用者
- 不释放 `data`

**接口**：见 `include/blocking_queue.h`

**验证**：

- 功能测试：`tests/test_blocking_queue.c`
- TSan：无 data race
- Valgrind：0 leak / 0 error

### SPSC Queue（无锁单生产者单消费者队列）

**定位**：单生产者单消费者、非阻塞、高吞吐场景。

| 项 | 说明 |
|----|------|
| 实现 | 环形数组 + `_Atomic` head/tail + acquire/release |
| 容量 | 有界 |
| 线程模型 | SPSC（严格 1 生产者 1 消费者） |
| 空 / 满行为 | 非阻塞，返回 `NULL` / `false` |
| 数据所有权 | 调用者负责 |

**核心设计**：

- `head` 只被消费者写，`tail` 只被生产者写 → 无需 CAS
- `head` / `tail` 分处不同 cache line → 避免 false sharing
- 生产者写 `tail` 用 `release`，消费者读 `tail` 用 `acquire` → 保证 buffer 可见顺序
- 预分配数组 → 无内存回收问题

**局限**：

- 只支持 1P1C
- 非阻塞，调用者自己处理满 / 空
- 有界，容量固定

**接口**：见 `include/spsc_queue.h`

**验证**：

- 功能测试：`tests/test_spsc.c`
- TSan：0 warning（对照 `naive_queue`：3 warning）
- Valgrind：0 leak / 0 error

## 验证方法

每个结构都用以下手段验证：

| 手段 | 目的 | 命令示例 |
|------|------|----------|
| 功能测试 | 验证 FIFO、满 / 空、边界 | `./test_spsc` |
| TSan | 检测 data race | `gcc -fsanitize=thread ...` |
| Valgrind | 检测内存泄漏、越界 | `valgrind --leak-check=full ./test_dbg` |

**TSan 说明**：

- SPSC 用 `_Atomic` + `memory_order`，TSan 0 warning；
- 对照的 `naive_queue`（无同步）TSan 报 3 处 data race；
- 这说明原子和内存序是消除 race 的必需手段。

**注意**：TSan 不依赖「运行时真的撞上」，它检测访问模式。在 x86 上运行时求和可能碰巧正确，但 TSan 能确定性报出 race。

## 目录结构

```text
concurrent-data-structure/
├── README.md
├── include/
│   ├── blocking_queue.h
│   ├── spsc_queue.h
│   └── naive_queue.h          # 对照用，不作为正式接口
├── src/
│   ├── blocking_queue.c
│   ├── spsc_queue.c
│   └── naive_queue.c
└── tests/
    ├── test_blocking_queue.c
    ├── test_spsc.c
    ├── test_spsc_tsan.c
    └── test_naive_tsan.c
```

## 构建与测试

### Blocking Queue

```bash
# 编译测试
gcc -Wall -Wextra -g -O2 -pthread -std=c11 \
    tests/test_blocking_queue.c src/blocking_queue.c \
    -Iinclude -o test_blocking_queue

# 运行
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

## 规划

- ☑ Blocking Queue（MPMC，有界阻塞）
- ☑ SPSC Queue（无锁，非阻塞）
- ☐ Hash Map（计划：全局锁版 + 分段锁版）
- ☐ Thread Pool（基于 Blocking Queue，带优雅关闭）
- ☐ 无锁 MPMC Queue（可选，难度高）

## 参考

- CSAPP 第 12 章（并发编程）
- C11 标准 `<stdatomic.h>`
- OSTEP 第 29 章（并发数据结构）
