# LuaJIT 5.3.6
<p align="center">
<a href="https://github.com/Yu2erer/Lua-NOGC/actions?query=workflow%3ALua-5.3.6"><img src="https://github.com/Yu2erer/Lua-NOGC/workflows/Lua5.3.6/badge.svg"></a>
</p>

LuaJIT 5.3.6 原名 `Lua-NOGC`，是基于 `Lua 5.3.6` 实现的一个垃圾回收优化的扩展版本。

后来又基于 `Lua 5.3.6` 实现了 `Just-In-Time Compiler(JIT)`，改名为 `LuaJIT 5.3.6`。

## 使用方法
LuaJIT 5.3.6 一共提供了三个 Lua 函数，其中关于垃圾回收优化的有 `bggc` 和 `nogc`。

#### BGGC 示例

> bggc([opt)

bggc 只有一个参数 opt，通过参数 opt 提供一组不同的功能。
* "open": 表示开启后台线程垃圾回收，默认关闭
* "close": 表示关闭后台线程垃圾回收
* "isrunning": 返回值为0或1，表示后台线程垃圾回收是否开启

#### NOGC 示例

> nogc([opt, [, arg])

这个函数是 NOGC 的通用接口，通过参数 opt 提供了一组不同的功能，第二参数始终只接受 table 或 nil（当不需要时）：
* "open": 表示对这个 table 进行递归标记，保证其及其成员不被扫描与清理，需要第二参数(table)
* "close": 相当于关闭 nogc 的功能，使其恢复 Lua 垃圾回收的接管，需要第二参数(table)
* "len": 返回当前不被 Lua 垃圾回收管理的对象个数，不需要第二参数
* "count": 返回当前不被 Lua 垃圾回收管理的对象的总内存大小，其单位为K，不需要第二参数

```lua
configTable = {a = "test", b = true, c = 100} -- 创建一张配置表
nogc("open", configTable) -- 标记该 table 不被 Lua 垃圾回收管理
print(nogc("len")) -- 返回不被 Lua 垃圾回收管理的对象个数
print(nogc("count")) -- 返回当前不被 Lua 垃圾回收管理的对象的总内存大小，其单位为K
nogc("close", configTable) -- 恢复该 table 的标记，使其能够被 Lua 垃圾回收
```

#### jit 示例

> jit([opt, [, arg])

该函数为 JIT 的通用接口，通过参数 opt 提供了一组不同的功能，第二参数只接受 `Lua function` 或者 `int`。

* "open": 表示开启自动 JIT 编译，默认开启
* "close": 表示关闭自动 JIT 编译
* "isrunning": 返回自动 JIT 编译是否开启
* "count": 返回已经 JIT 编译的函数个数
* "compile": 第二参数为 Lua function, 将其编译
* "iscompiled": 第二参数为 Lua function, 返回其是否已编译
* "setlimitsize": 第二参数为 int, 表示触发自动编译的函数大小的最小值
* "setlimitcount": 第二参数为 int, 表示触发自动编译函数执行次数的最小值

只有当 `函数的大小 > limitsize` 且 `函数的执行次数 > limitcount` 才会触发编译。

## 注意事项
`BGGC` 没有需要注意的，这意味着你可以只用 `bggc` 以一个最小的成本获得一个不错的垃圾回收效率的提升。

`NOGC` 则需要注意以下几点：
table 中仅支持以下类型
* table（支持嵌套）
* metatable
* Lua function
* string
* number
* boolean

不支持以下类型（会进行检查）
* userdata
* thread
* C function

其中 table 都不支持 weak table（会进行检查）

同时被 NOGC 接管的 table 都不支持增加 必须是只读！（因为新的对象插入到不被扫描到的 table 中，会导致该对象不会被标记，进而被垃圾回收，导致段错误）

如果需要往 table 中增加对象，请通过以下顺序进行操作。

```lua
nogc("close", table) -- 先关闭 NOGC
table[#table] = ? -- 插入新的值
nogc("open", table) -- 重新打开 NOGC
```

`JIT` 需要注意以下几点:
* JIT 函数执行是以模拟C函数的形式执行，因此限制和C函数一致，调用次数不能超过 `LUAI_MAXCCALLS(200)次`。
* `OP_TAILCALL` 默认实现为 `OP_CALL`，因此调用次数也不能超过 `LUAI_MAXCCALLS(200)`。
* 当 Lua函数 JIT编译 后，`debug` 库的相关的 hook，都会失效，因为 Lua支持按指令 hook，对性能有一定的影响。


## 如何接入项目？
跟官方的 `Lua5.3` 相同的使用方式。

`BGGC` 不支持 `windows` 主要是 `windows` 不支持 `pthread`，但是依然可以编译，只是用不了这一个功能。

```sh
git clone https://github.com/Yu2erer/Lua-NOGC.git
make linux test # 其中 linux 为相应平台，不支持 c89 编译。
```

## FAQ
1. 只有 Lua 5.3.6 吗？
> 目前仅提供了 Lua 5.3.6，如果需要其他 Lua 版本，可以参考 [patch](https://github.com/Yu2erer/Lua-NOGC/blob/master/Lua-5.3.6.patch)。
2. 你修改了 Lua 中的什么内容？
> 也请参考 [patch](https://github.com/Yu2erer/Lua-NOGC/blob/master/Lua-5.3.6.patch)。
3. table 支持嵌套吗？ metatable呢？metamethod呢？
> 统统支持，唯一不支持的就是 weak table。
4. 为什么 NOGC 后，获取 nogc("len") 或 nogc("count") 为 0？
> 因为 nogc("open", tb) 只是对 table 打上标记，真正把它从Lua垃圾回收的链表中取下来的时间取决于 垃圾回收触发的时机，如果希望直接看到结果，可以在 open 之后直接调用 collectgarbage() 进行一次全量回收，触发其下链。
5. 为什么每次 nogc("len") 或 nogc("count") 的返回值都不同？
> 参考问题4，因为Lua的垃圾回收是渐进的，在那个瞬间只下链了那么多个对象。
6. 为什么 NOGC 标记 table 后会发生段错误？
> 请确认你的 table 是不是插入了新的对象，该 table 必须是只读的！
7. 如果我要往 NOGC 标记过的 table 插入新对象怎么办？
> 参考 注意事项 中的代码示例。
8. 你说的这么好，垃圾回收提速多少呢？
> 经过测试，`bggc` 开启了后台线程优化后，大约能提升 `50%`。`nogc` 则取决于你不需要参与垃圾回收的对象数量。
9. JIT 后，能提速多少呢？
> 经过测试, `JIT` 开启后, 最大能有 7倍 的提升。

## Thanks
LuaJIT 5.3.6 backend [mir](https://github.com/vnmakarov/mir)

## LICENSE
LuaJIT 5.3.6 is released under the MIT license. See LICENSE for details.