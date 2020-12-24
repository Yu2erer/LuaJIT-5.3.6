# Lua-GC 优化
Lua GC 提速

## 思路
减少垃圾回收所要遍历的对象，特别是 Lua 的配置表和一些全局闭包，避免反复遍历一定不需要回收的对象。

换句话说就是给不需要垃圾回收的对象打上标记，跳过对它的扫描与清理。

当不需要回收的对象越多，垃圾回收提升的效率越明显。

打上标记的对象不能够修改其内部的任何数据（有可能会分配新的对象，但是没能成功标记上，被 Lua 的垃圾回收
给清理）

具体参见 [Lua GC垃圾回收优化方案](https://yuerer.com/Lua-GC%E5%9E%83%E5%9C%BE%E5%9B%9E%E6%94%B6%E4%BC%98%E5%8C%96%E6%96%B9%E6%A1%88/)

## 使用方法
支持 table, Lua Closure, string, number, boolean
不支持弱表

```lua
nogc("open", Table) -- 这一整个 Table 都不被扫描不被清理
nogc("close", Table) -- 相当于 open 的反方法
nogc("len") -- 当前不被 垃圾回收管理的对象个数
nogc("count") -- 当前不被 垃圾回收管理的对象的总内存大小 单位为k
```


## 如何接入项目？

引入我写的两个文件, YGC.c, YGC.h.

修改以下几个文件.

### lbaselib.c

添加头文件，然后导出  `nogc` 函数给 Lua 使用。

```c
#include "YGC.h"

static const luaL_Reg base_funcs[] = {
	....
  {"nogc", nogc},
  {NULL, NULL}
};

```

### lvm.c

添加头文件，在 `pushclosure` 函数这里， `if (!isblack(p))`  改为以下的代码。

这是因为，当我们标记的 Table 中含有的闭包，被执行到的时候，会动态的生成 `Closure` ，但是这个 `Closure` 是没办法被标记到的，因为是动态生成的，因此不应该指过去。

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

初始化上面的对象

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

提前返回对象，减少垃圾回收耗时。

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

propagatemark 的修改主要是为了提前返回，不要遍历不需要GC的对象。

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

替换为。

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

至此完结，享受提速后的快感吧。
