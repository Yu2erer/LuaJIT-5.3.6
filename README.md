# Lua-NOGC
<p align="center">
<a href="https://github.com/Yu2erer/Lua-NOGC/actions?query=workflow%3ALua-5.3.6"><img src="https://github.com/Yu2erer/Lua-NOGC/workflows/Lua5.3.6/badge.svg"></a>
<a href="https://github.com/Yu2erer/Lua-NOGC/actions?query=workflow%3ALua-5.3.5"><img src="https://github.com/Yu2erer/Lua-NOGC/workflows/Lua5.3.5/badge.svg"></a>
</p>

`Lua-NOGC` 是基于 Lua 5.3.x 实现的一个垃圾回收优化的扩展版本，其兼容 Lua 5.3 的各个版本，目前提供 `Lua 5.3.5` 与 `Lua 5.3.6` 两个可直接编译的版本，可在本仓库的相应分支中获得。

## 实现原理
通过减少垃圾回收所要扫描及清理的对象，避免扫描和清理一些一定不需要回收的对象，特别是 Lua 的配置表和一些全局闭包，以此来达到垃圾回收提速的效果。

理论上提速的效果取决于 `(不需要回收的对象的个数 / 虚拟机中总对象个数)`

换句话说就是给不需要垃圾回收的对象打上标记，跳过对它的扫描与清理。

当不需要回收的对象越多，垃圾回收提升的效率越明显。

具体参见 [Lua GC垃圾回收优化方案](https://yuerer.com/Lua-GC%E5%9E%83%E5%9C%BE%E5%9B%9E%E6%94%B6%E4%BC%98%E5%8C%96%E6%96%B9%E6%A1%88/)


## 使用方法
Lua-NOGC 只提供了一个 Lua 函数。
> nogc([opt, [, arg])

这个函数是 NOGC 的通用接口，通过参数 opt 它提供了一组不同的功能，第二参数始终只接受 table 或 nil（当不需要时）：
* "open": 表示对这个 table 进行递归标记，保证其不被扫描与清理，需要第二参数
* "close": 相当于关闭 nogc 的功能，使其恢复 Lua 垃圾回收的接管，需要第二参数
* "len": 返回当前不被 Lua 垃圾回收管理的对象个数，不需要第二参数
* "count": 返回当前不被 Lua 垃圾回收管理的对象的总内存大小，其单位为K，不需要第二参数


### 示例
```lua
configTable = {a = "test", b = true, c = 100} -- 创建一张 配置表

nogc("open", configTable) -- 标记该 table 不被 Lua 垃圾回收管理，提速

print(nogc("len")) -- 返回不被 Lua 垃圾回收管理的对象个数

print(nogc("count")) -- 返回当前不被 Lua 垃圾回收管理的对象的总内存大小，其单位为K

nogc("close", configTable) -- 恢复该 table 的标记，使其能够被 Lua 垃圾回收
```

## 注意事项
table 中支持以下类型
* table（支持嵌套）
* metatable
* Lua function
* string
* number
* boolean

不支持以下类型（会报错）
* userdata
* thread
* C function

其中 table 都不支持 weak table（会报错）

同时被 NOGC 接管的 table 都不支持增加 必须是只读！（因为新的对象插入到不被扫描到的 table 中，会导致该对象不会被标记，进而被垃圾回收，导致段错误）

如果需要往 table 中增加对象，请通过以下顺序进行操作。

```lua
nogc("close", table) -- 先关闭 NOGC
table[#table] = ? -- 插入新的值
nogc("open", table) -- 重新打开 NOGC
```

## 如何接入项目？

```sh
git clone https://github.com/Yu2erer/Lua-NOGC.git
git checkout Lua5.3.6-NOGC # 如果需要 Lua5.3.5 请修改为 Lua5.3.5-NOGC
make linux test # 其中 linux 为相应平台，Lua 官方支持的平台都支持
```

## FAQ
1. 只有 Lua 5.3.5 和 Lua 5.3.6 吗？
> 目前仅提供了 Lua 5.3.5 和 Lua 5.3.6，如果需要其他 Lua 版本，可以参考 下面的详细修改过程。
2. 你修改了 Lua 中的什么内容？
> 也请参考 下面的详细修改过程。
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
> 根据用户反馈，全量回收提升的速率大约在 2.5 - 5倍左右，取决于你的虚拟机中 NOGC 的对象 占 虚拟机总对象的比重。

---

## 对比官方版本，我做了什么修改？
添加了 YGC.c，YGC.h，同时添加到 `src/Makefile`，使其编译通过。

修改了以下五个文件
* lbaselib.c
* lvm.c
* lstate.h
* lstate.c
* lgc.c

### lbaselib.c
添加头文件，导出 `nogc` 函数给 Lua 使用。

```c
#include "YGC.h"

static const luaL_Reg base_funcs[] = {
  ....
  {"nogc", nogc},
  {NULL, NULL}
};

```

### lvm.c
添加头文件，在 `pushclosure` 函数这里，`if (!isblack(p))` 改为以下的代码。

这是因为，当我们标记的 table 中含有的闭包，被执行到的时候，会动态的生成 `Closure` ，但是这个 `Closure` 是没办法被标记到的，因为是动态生成的，因此不应该指过去（即不缓存该闭包）。

```c
#include "YGC.h"

if (!isblack(p) && !Y_isnogc(p) && !Y_isnogc(ncl))
    p->cache = ncl;
}

```

### lstate.h
在 `global_State` 记录两个辅助的值，其中一个是 nogc 的对象内存大小，另一个是 不参与GC的链表，都是为了方便调试用的。

```c
typedef struct global_State {
	....
	lu_mem Y_GCmemnogc; /* memory size of nogc linked list */
	GCObject *Y_nogc;  /* list of objects not to be traversed or collected */
	....
}
```

### lstate.c
初始化 global_State 新增的对象。

```c
LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
	....
	g->Y_GCmemnogc = 0;
	g->Y_nogc = NULL;
	....
}
```

### lgc.c
这是最后一个文件了，依然是添加头文件。然后将以下代码进行对比替换。

```c
#include "YGC.h"
```

将 以下代码 进行替换，简单的来说就是将不需要GC的对象，移出 `allgc` 链表。

```c
static GCObject **sweeplist (lua_State *L, GCObject **p, lu_mem count) {
  global_State *g = G(L);
  int ow = otherwhite(g);
  int white = luaC_white(g);  /* current white */
  while (*p != NULL && count-- > 0) {
    GCObject *curr = *p;
    int marked = curr->marked;
    if (isdeadm(ow, marked)) {  /* is 'curr' dead? */
      *p = curr->next;  /* remove 'curr' from list */
      freeobj(L, curr);  /* erase 'curr' */
    }
    else {  /* change mark to 'white' */
      curr->marked = cast_byte((marked & maskcolors) | white);
      p = &curr->next;  /* go to next element */
    }
  }
  return (*p == NULL) ? NULL : p;
}
```

替换为以下这段。

```c
static GCObject **sweeplist (lua_State *L, GCObject **p, lu_mem count) {
  global_State *g = G(L);
  int ow = otherwhite(g);
  int white = luaC_white(g);  /* current white */
  while (*p != NULL && count-- > 0) {
    GCObject *curr = *p;
    if (g->gcstate == GCSswpallgc && Y_isnogc(curr)) {
      *p = curr->next;
      curr->next = g->Y_nogc;
      g->Y_nogc = curr;
      continue;
    }
    int marked = curr->marked;
    if (isdeadm(ow, marked)) {  /* is 'curr' dead? */
      *p = curr->next;  /* remove 'curr' from list */
      freeobj(L, curr);  /* erase 'curr' */
    }
    else {  /* change mark to 'white' */
      curr->marked = cast_byte((marked & maskcolors) | white);
      p = &curr->next;  /* go to next element */
    }
  }
  return (*p == NULL) ? NULL : p;
}
```

propagatemark 的修改主要是为了提前返回，不要遍历不需要GC的对象（能节约大量的时间）。

```c
static void propagatemark (global_State *g) {
  lu_mem size;
  GCObject *o = g->gray;
  lua_assert(isgray(o));
  gray2black(o);
  switch (o->tt) {
    case LUA_TTABLE: {
      Table *h = gco2t(o);
      g->gray = h->gclist;  /* remove from 'gray' list */
      size = traversetable(g, h);
      break;
    }
    case LUA_TLCL: {
      LClosure *cl = gco2lcl(o);
      g->gray = cl->gclist;  /* remove from 'gray' list */
      size = traverseLclosure(g, cl);
      break;
    }
    case LUA_TCCL: {
      CClosure *cl = gco2ccl(o);
      g->gray = cl->gclist;  /* remove from 'gray' list */
      size = traverseCclosure(g, cl);
      break;
    }
    case LUA_TTHREAD: {
      lua_State *th = gco2th(o);
      g->gray = th->gclist;  /* remove from 'gray' list */
      linkgclist(th, g->grayagain);  /* insert into 'grayagain' list */
      black2gray(o);
      size = traversethread(g, th);
      break;
    }
    case LUA_TPROTO: {
      Proto *p = gco2p(o);
      g->gray = p->gclist;  /* remove from 'gray' list */
      size = traverseproto(g, p);
      break;
    }
    default: lua_assert(0); return;
  }
  g->GCmemtrav += size;
}
```

替换为以下代码。

```c
static void propagatemark (global_State *g) {
  lu_mem size;
  GCObject *o = g->gray;
  lua_assert(isgray(o));
  gray2black(o);
  switch (o->tt) {
    case LUA_TTABLE: {
      Table *h = gco2t(o);
      g->gray = h->gclist;  /* remove from 'gray' list */
      size = (Y_isnogc(o) ? 0 : traversetable(g, h));
      break;
    }
    case LUA_TLCL: {
      LClosure *cl = gco2lcl(o);
      g->gray = cl->gclist;  /* remove from 'gray' list */
      size = (Y_isnogc(cl) ? 0 : traverseLclosure(g, cl));
      break;
    }
    case LUA_TCCL: {
      CClosure *cl = gco2ccl(o);
      g->gray = cl->gclist;  /* remove from 'gray' list */
      size = traverseCclosure(g, cl);
      break;
    }
    case LUA_TTHREAD: {
      lua_State *th = gco2th(o);
      g->gray = th->gclist;  /* remove from 'gray' list */
      linkgclist(th, g->grayagain);  /* insert into 'grayagain' list */
      black2gray(o);
      size = traversethread(g, th);
      break;
    }
    case LUA_TPROTO: {
      Proto *p = gco2p(o);
      g->gray = p->gclist;  /* remove from 'gray' list */
      size = (Y_isnogc(p) ? 0 : traverseproto(g, p));
      break;
    }
    default: lua_assert(0); return;
  }
  g->GCmemtrav += size;
}
```
