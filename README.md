# Lua-NOGC
<p align="center">
<a href="https://github.com/Yu2erer/Lua-NOGC/actions?query=workflow%3ALua-5.3.6"><img src="https://github.com/Yu2erer/Lua-NOGC/workflows/Lua5.3.6/badge.svg"></a>
</p>

`Lua-NOGC` 是基于 Lua 5.3.6 实现的一个 `多线程` 垃圾回收优化的扩展版本。

## 实现原理
垃圾回收优化主要由以下两部分组成。
1. `BGGC` 通过创建一个 `Background` 线程，将垃圾回收的任务交给 后台线程 去处理。
2. `NOGC` 通过减少垃圾回收所要扫描及清理的对象，避免扫描和清理一些不需要回收的对象，比如 Lua 的配置表和一些全局闭包，以此来达到垃圾回收提速的效果。

通过 `BGGC` 的优化，当全量回收时，可以减少将近 `50%` 的时间消耗。

通过 `NOGC` 的优化，理论上提速的效果取决于 `(不需要回收的对象的个数 / 虚拟机中总对象个数)`

换句话说就是给不需要垃圾回收的对象打上标记，跳过对它的扫描与清理。

当不需要回收的对象越多，垃圾回收提升的效率越明显。

具体参见 [Lua GC垃圾回收优化方案](https://yuerer.com/Lua-GC%E5%9E%83%E5%9C%BE%E5%9B%9E%E6%94%B6%E4%BC%98%E5%8C%96%E6%96%B9%E6%A1%88/)


## 使用方法
Lua-NOGC 只提供了两个 Lua 函数，分别是 `bggc` 和 `nogc`。
> bggc([opt)

bggc 只有一个参数 opt，通过参数 opt 它提供一组不同的功能。
* "close": 表示关闭 后台线程 进行垃圾回收
* "open": 表示开启 后台线程 进行垃圾回收

### BGGC 示例
```lua
bggc("close") -- 关闭后台线程 垃圾回收

bggc("open") -- 开启后台线程 垃圾回收
```

> nogc([opt, [, arg])

这个函数是 NOGC 的通用接口，通过参数 opt 它提供了一组不同的功能，第二参数始终只接受 table 或 nil（当不需要时）：
* "open": 表示对这个 table 进行递归标记，保证其不被扫描与清理，需要第二参数
* "close": 相当于关闭 nogc 的功能，使其恢复 Lua 垃圾回收的接管，需要第二参数
* "len": 返回当前不被 Lua 垃圾回收管理的对象个数，不需要第二参数
* "count": 返回当前不被 Lua 垃圾回收管理的对象的总内存大小，其单位为K，不需要第二参数

### NOGC 示例
```lua
configTable = {a = "test", b = true, c = 100} -- 创建一张 配置表

nogc("open", configTable) -- 标记该 table 不被 Lua 垃圾回收管理，提速

print(nogc("len")) -- 返回不被 Lua 垃圾回收管理的对象个数

print(nogc("count")) -- 返回当前不被 Lua 垃圾回收管理的对象的总内存大小，其单位为K

nogc("close", configTable) -- 恢复该 table 的标记，使其能够被 Lua 垃圾回收
```

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

## 如何接入项目？
跟 官方的 `Lua5.3` 相同的使用方式。

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