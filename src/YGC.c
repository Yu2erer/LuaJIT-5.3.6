/*
 * @Author: Yuerer
 * @Date: 2020-12-16 10:01:20
 * @LastEditTime: 2021-04-19 16:47:36
 */

#include "YGC.h"

#include <string.h>

#include "lfunc.h"
#include "ltable.h"
#include "lstring.h"
#include "lauxlib.h"

#define Y_NOGCCCLOSE 0
#define Y_NOGCOPEN   1
#define Y_NOGCCOUNT  2
#define Y_NOGCLEN    3

#define Y_makenogc(x) l_setbit((x)->marked, Y_NOGCBIT)
#define Y_clearnogc(x) resetbit((x)->marked, Y_NOGCBIT)

static void Y_linkrevert (global_State *g, GCObject *o);
static void Y_closeupvalue (lua_State *L, UpVal *u);
static void Y_reallymarkobject (lua_State *L, GCObject *o, int b);
static void Y_traverseproto (lua_State *L, Proto *f, int b);
static void Y_traverseLclosure (lua_State *L, LClosure *cl, int b);
static void Y_traversetable (lua_State *L, Table *h, int b);
static int  Y_isweaktable (lua_State *L, const struct Table *h);

#define Y_valis(x, b) (iscollectable(x) && (b ? !Y_isnogc(gcvalue(x)) : Y_isnogc(gcvalue(x))))
#define Y_markvalue(L, o, b) { if (Y_valis(o, b)) Y_reallymarkobject(L, gcvalue(o), b); }
#define Y_markobject(L, t, b) { Y_reallymarkobject(L, obj2gco(t), b); }
#define Y_markobjectN(L, t, b) { if (t) Y_markobject(L, t, b); }
#define Y_maskcolors (~(bitmask(BLACKBIT) | WHITEBITS))
#define Y_makeblack(x) \
    (x->marked = cast_byte((x->marked & Y_maskcolors) | cast(lu_byte, bitmask(BLACKBIT))))
#define Y_resetobject(g,o) \
    { Y_clearnogc(o); Y_makeblack(o); Y_linkrevert(g, o);  }


static void Y_linkrevert (global_State *g, GCObject *o) {
  GCObject **nogc = &g->Y_nogc;
  while (*nogc != NULL) {
    GCObject *curr = *nogc;
    if (curr == o) {
      *nogc = curr->next;
      curr->next = g->allgc;
      g->allgc = curr;
      break;
    }
    nogc = &curr->next;
  }
}

static void Y_closeupvalue (lua_State *L, UpVal *u) {
  UpVal **up = &L->openupval;
  while (*up != NULL) {
    UpVal *uv = *up;
    if (uv == u) {
      *up = uv->u.open.next;
      setobj(L, &uv->u.value, uv->v);
      uv->v = &uv->u.value;
      break;
    }
    up = &uv->u.open.next;
  }
}

static void Y_reallymarkobject (lua_State *L, GCObject *o, int b) {
  global_State *g = G(L);
  switch (o->tt) {
    case LUA_TSHRSTR: {
      lu_mem mem = sizelstring(gco2ts(o)->shrlen);
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      g->Y_GCmemnogc += (b ? mem : -mem);
      break;
    }
    case LUA_TLNGSTR: {
      lu_mem mem = sizelstring(gco2ts(o)->u.lnglen);
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      g->Y_GCmemnogc += (b ? mem : -mem);
      break;
    }
    case LUA_TTABLE: {
      if (Y_isweaktable(L, gco2t(o))) {
        luaL_error(L, "Not support weak tables");
        break;
      }
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      Y_traversetable(L, gco2t(o), b);
      break;
    }
    case LUA_TLCL: {
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      Y_traverseLclosure(L, gco2lcl(o), b);
      break;
    }
    case LUA_TPROTO: {
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      Y_traverseproto(L, gco2p(o), b);
      break;
    }
    case LUA_TUSERDATA: {
      luaL_error(L, "Not support userdata");
      break;
    }
    case LUA_TTHREAD: {
      luaL_error(L, "Not support thread");
      break;
    }
    case LUA_TCCL: {
      luaL_error(L, "Not support C function");
      break;
    }
    default: break;
  }
}

static void Y_traverseproto (lua_State *L, Proto *f, int b) {
  if (f->cache && !Y_isnogc(f->cache)) {
    f->cache = NULL;
  }
  int i;
  Y_markobjectN(L, f->source, b);
  for (i = 0; i < f->sizek; i ++) {
    Y_markvalue(L, &f->k[i], b);
  }
  for (i = 0; i < f->sizeupvalues; i ++) {
    Y_markobjectN(L, f->upvalues[i].name, b)
  }
  for (i = 0; i < f->sizep; i ++) {
    Y_markobjectN(L, f->p[i], b);
  }
  for (i = 0; i <f->sizelocvars; i ++) {
    Y_markobjectN(L, f->locvars[i].varname, b);
  }
  lu_mem mem = sizeof(Proto) + sizeof(Instruction) * f->sizecode +
                         sizeof(Proto *) * f->sizep +
                         sizeof(TValue) * f->sizek +
                         sizeof(int) * f->sizelineinfo +
                         sizeof(LocVar) * f->sizelocvars +
                         sizeof(Upvaldesc) * f->sizeupvalues;
  G(L)->Y_GCmemnogc += (b ? mem : -mem);
}

static void Y_traverseLclosure (lua_State *L, LClosure *cl, int b) {
  int i;
  Y_markobjectN(L, cl->p, b);
  Table *reg = hvalue(&G(L)->l_registry);
  const TValue *gt = luaH_getint(reg, LUA_RIDX_GLOBALS);
  GCObject *gto = gcvalue(gt);

  for (i = 0; i < cl->nupvalues; i ++) {
    UpVal *uv = cl->upvals[i];
    if (uv != NULL) {
      GCObject *o = gcvalue(uv->v);
      /* skip _ENV */
      if (o == gto) continue;
      if (b && upisopen(uv)) {
        Y_closeupvalue(L, uv);
      }
      Y_markvalue(L, uv->v, b);
    }
  }
  lu_mem mem = sizeLclosure(cl->nupvalues);
  G(L)->Y_GCmemnogc += (b ? mem : -mem);
}

static void Y_traversetable (lua_State *L, Table *h, int b) {
  Y_markobjectN(L, h->metatable, b);
  Node *n, *limit = gnode(h, cast(size_t, sizenode(h)));
  unsigned int i;
  for (i = 0; i < h->sizearray; i++) {
    Y_markvalue(L, &h->array[i], b);
  }
  for (n = gnode(h, 0); n < limit; n++) {
    if (!ttisnil(gval(n))) {
      Y_markvalue(L, gkey(n), b);
      Y_markvalue(L, gval(n), b);
    }
  }
  lu_mem mem = sizeof(Table) + sizeof(TValue) * h->sizearray +
                         sizeof(Node) * cast(size_t, allocsizenode(h));
  G(L)->Y_GCmemnogc += (b ? mem : -mem);
}

static int Y_isweaktable (lua_State *L, const struct Table *h) {
  const char *weakkey, *weakvalue;
  const TValue *mode = gfasttm(G(L), h->metatable, TM_MODE);
  if (mode && ttisstring(mode) && 
      ((weakkey = strchr(svalue(mode), 'k')),
       (weakvalue = strchr(svalue(mode), 'v')),
       (weakkey || weakvalue))) {
    return 1;
  }
  return 0;
}

static const struct Table* Y_opttable (lua_State *L, int arg) {
  if (lua_isnoneornil(L, arg) || lua_type(L, arg) != LUA_TTABLE) {
    return NULL;
  }
  return cast(Table*, lua_topointer(L, arg));
}

static int Y_nogc (lua_State *L, int what, const struct Table *h) {
  int res = 0;
  global_State *g = G(L);
  switch (what) {
    case Y_NOGCCCLOSE: {
      if (!h) {
        luaL_argerror(L, 2, "Missing a table object");
        break;
      }
      Y_markobject(L, h, Y_NOGCCCLOSE);
      break;
    }
    case Y_NOGCOPEN: {
      if (!h) {
        luaL_argerror(L, 2, "Missing a table object");
        break;
      }
      Y_markobject(L, h, Y_NOGCOPEN);
      break;
    }
    case Y_NOGCCOUNT: {
      res = cast_int(g->Y_GCmemnogc >> 10);
      break;
    }
    case Y_NOGCLEN: {
      GCObject *o = g->Y_nogc;
      while (o) {
        res ++;
        o = o->next;
      }
      break;
    }
    default: res = -1;
    
  }
  return res;
}

int nogc (lua_State *L) {
  static const char* const opts[] = {"close", "open", "count",
    "len", NULL};
  static const int optsum[] = {Y_NOGCCCLOSE, Y_NOGCOPEN, Y_NOGCCOUNT,
    Y_NOGCLEN};
  int o = optsum[luaL_checkoption(L, 1, "count", opts)];
  const struct Table *ex = Y_opttable(L, 2);
  int res = Y_nogc(L, o, ex);
  switch (o) {
    case Y_NOGCCOUNT: {
      lua_pushnumber(L, (lua_Number)res + ((lua_Number)res/1024));
      return 1;
    }
    case Y_NOGCLEN: {
      lua_pushinteger(L, res);
      return 1;
    }
    default: return 0;
  }
  return 0;
}

/* ------------------------ Background Garbage Collect ------------------------ */

#define Y_BGGCCLOSE 0
#define Y_BGGCOPEN 1
#define Y_BGGCISRUNNING 2

static void Y_luaM_free_ (lua_State *L, void *block, size_t osize);
static void *Y_luaM_malloc (lua_State *L, size_t nsize);
static void Y_luaF_freeproto (lua_State *L, Proto *f);
static void Y_luaH_free (lua_State *L, Table *t);
static size_t Y_linkbgjob (Y_bgjob *j, GCObject *o);
static void Y_upvdeccount (lua_State *L, LClosure *cl);
static void Y_freeobj (lua_State *L, GCObject *o);
static void *Y_bgProcessJobs (void *arg);

#define Y_luaM_freemem(L, b, s) Y_luaM_free_(L, (b), (s))
#define Y_luaM_free(L, b) Y_luaM_free_(L, (b), sizeof(*(b)))
#define Y_luaM_freearray(L, b, n) Y_luaM_free_(L, (b), (n)*sizeof(*(b)))
#define Y_luaM_new(L, t) cast(t*, Y_luaM_malloc(L, sizeof(t)))

static inline void Y_luaM_free_ (lua_State *L, void *block, size_t osize) {
  global_State *g = G(L);
  (*g->frealloc)(g->ud, block, osize, 0);
}

static inline void *Y_luaM_malloc (lua_State *L, size_t nsize) {
  global_State *g = G(L);
  void *newblock = (*g->frealloc)(g->ud, NULL, 0, nsize);
  return newblock;
}

static inline void Y_luaF_freeproto (lua_State *L, Proto *f) {
  Y_luaM_freearray(L, f->code, f->sizecode);
  Y_luaM_freearray(L, f->p, f->sizep);
  Y_luaM_freearray(L, f->k, f->sizek);
  Y_luaM_freearray(L, f->lineinfo, f->sizelineinfo);
  Y_luaM_freearray(L, f->locvars, f->sizelocvars);
  Y_luaM_freearray(L, f->upvalues, f->sizeupvalues);
  Y_luaM_free(L, f);
}

static inline void Y_luaH_free (lua_State *L, Table *t) {
  if (!isdummy(t))
    Y_luaM_freearray(L, t->node, cast(size_t, sizenode(t)));
  Y_luaM_freearray(L, t->array, t->sizearray);
  Y_luaM_free(L, t);
}

static Y_bgjob *Y_jobs = NULL;
struct Y_bgjob {
  Y_bgjob *next;
  GCObject *Y_bggc;
};

/* link GCObject to a background job 
  and return the size of the GCObject 
  that will be released */
static size_t Y_linkbgjob (Y_bgjob *j, GCObject *o) {
  o->next = j->Y_bggc;
  j->Y_bggc = o;
  size_t osize = 0;
  switch (o->tt) {
    case LUA_TPROTO: {
      Proto *f = gco2p(o);
      osize += sizeof(*(f->code)) * f->sizecode;
      osize += sizeof(*(f->p)) * f->sizep;
      osize += sizeof(*(f->k)) * f->sizek;
      osize += sizeof(*(f->lineinfo)) * f->sizelineinfo;
      osize += sizeof(*(f->locvars)) * f->sizelocvars;
      osize += sizeof(*(f->upvalues)) * f->sizeupvalues;
      osize += sizeof(*(f));
      break;
    }
    case LUA_TLCL: osize = sizeLclosure(gco2lcl(o)->nupvalues); break;
    case LUA_TCCL: osize = sizeCclosure(gco2ccl(o)->nupvalues); break;
    case LUA_TTABLE: {
      Table *t = gco2t(o);
      if (!isdummy(t)) osize += sizeof(*(t->node)) * cast(size_t, sizenode(t));
      osize += sizeof(*(t->array)) * t->sizearray;
      osize += sizeof(*(t));
      break;
    }
    case LUA_TUSERDATA: osize = sizeudata(gco2u(o)); break;
    case LUA_TSHRSTR: osize = sizelstring(gco2ts(o)->shrlen); break;
    case LUA_TLNGSTR: osize = sizelstring(gco2ts(o)->u.lnglen); break;
    default: lua_assert(0);
  }
  return osize;
}

static void Y_upvdeccount (lua_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    UpVal *uv = cl->upvals[i];
    if (uv)
      luaC_upvdeccount(L, uv);
  }
}

static void Y_freeobj (lua_State *L, GCObject *o) {
  /* threads are released in the main thread */
  switch (o->tt) {
    case LUA_TPROTO: Y_luaF_freeproto(L, gco2p(o)); break;
    case LUA_TLCL: {
      /* dec upvalue refcount in the main thread */
      luaM_freemem(L, gco2lcl(o), sizeLclosure(gco2lcl(o)->nupvalues));
      break;
    }
    case LUA_TCCL: {
      Y_luaM_freemem(L, o, sizeCclosure(gco2ccl(o)->nupvalues));
      break;
    }
    case LUA_TTABLE: Y_luaH_free(L, gco2t(o)); break;
    case LUA_TUSERDATA: Y_luaM_freemem(L, o, sizeudata(gco2u(o))); break;
    case LUA_TSHRSTR: {
      /* remove it from hash table has been executed in the main thread */
      Y_luaM_freemem(L, o, sizelstring(gco2ts(o)->shrlen));
      break;
    }
    case LUA_TLNGSTR: {
      Y_luaM_freemem(L, o, sizelstring(gco2ts(o)->u.lnglen));
      break;
    }
    default: lua_assert(0);
  }
}

void *Y_bgProcessJobs (void *arg) {
  lua_State *L= cast(lua_State*, arg);
  global_State *g = G(L);
  pthread_mutex_lock(&g->Y_bgmutex);
  while (1) {
    Y_bgjob **p = &Y_jobs;
    if (*p == NULL) {
      pthread_cond_wait(&g->Y_bgcond, &g->Y_bgmutex);
      continue;
    }
    Y_bgjob *curr = *p;
    *p = curr->next;
    pthread_mutex_unlock(&g->Y_bgmutex);
    GCObject **op = &curr->Y_bggc;
    while (*op != NULL) {
      GCObject *curr = *op;
      *op = curr->next;
      Y_freeobj(L, curr);
    }
    Y_luaM_free(L, curr);
    pthread_mutex_lock(&g->Y_bgmutex);
  }
  return NULL;
}

Y_bgjob *Y_createbgjob (lua_State *L) {
  if (!G(L)->Y_bgrunning) return NULL;
  Y_bgjob *j = Y_luaM_new(L, Y_bgjob);
  j->next = NULL;
  j->Y_bggc = NULL;
  return j;
}

inline void Y_submitbgjob (lua_State *L, Y_bgjob *j) {
  global_State *g = G(L);
  if (!g->Y_bgrunning) return;
  pthread_mutex_lock(&g->Y_bgmutex);
  j->next = Y_jobs;
  Y_jobs = j;
  pthread_cond_signal(&g->Y_bgcond);
  pthread_mutex_unlock(&g->Y_bgmutex);
}

void Y_trybgfree (lua_State *L, GCObject *o, Y_bgjob *j, void(*fgfreeobj)(lua_State*, GCObject*)) {
  global_State *g = G(L);
  if (!g->Y_bgrunning) {
    return fgfreeobj(L, o);
  }
  size_t osize = 0;
  switch (o->tt) {
    case LUA_TPROTO: osize = Y_linkbgjob(j, o); break;
    case LUA_TLCL: {
      Y_upvdeccount(L, gco2lcl(o));
      Y_linkbgjob(j, o);
      break;
    }
    case LUA_TCCL: osize = Y_linkbgjob(j, o); break;
    case LUA_TTABLE: osize = Y_linkbgjob(j, o); break;
    case LUA_TTHREAD: {
      /* release the memory by the main thread */
      luaE_freethread(L, gco2th(o));
      break;
    }
    case LUA_TUSERDATA: osize = Y_linkbgjob(j, o); break;
    case LUA_TSHRSTR:
      /* remove it from hash table by main thread */
      luaS_remove(L, gco2ts(o));
      /* release the memory by background thread */
      osize = Y_linkbgjob(j, o);
      break;
    case LUA_TLNGSTR: osize = Y_linkbgjob(j, o); break;
    default: lua_assert(0);
  }
  g->GCdebt -= osize;
}

inline void Y_initstate (lua_State *L) {
  global_State *g = G(L);
  g->Y_nogc = NULL;
  g->Y_GCmemnogc = 0;
  g->Y_bgrunning = 0;
  pthread_mutex_init(&g->Y_bgmutex, NULL);
  pthread_cond_init(&g->Y_bgcond, NULL);
  /* fixme: check return value */
  pthread_create(&g->Y_bgthread, NULL, Y_bgProcessJobs, cast(void*, L));
}

static int Y_bggc (lua_State *L, int what) {
  int res = 0;
  global_State *g = G(L);
  switch (what) {
    case Y_BGGCCLOSE: {
      g->Y_bgrunning = 0;
      break;
    }
    case Y_BGGCOPEN: {
      g->Y_bgrunning = 1;
      break;
    }
    case Y_BGGCISRUNNING: {
      res = g->Y_bgrunning;
      break;
    }
    default: res = -1;
  }
  return res;
}

int bggc (lua_State *L) {
  static const char* const opts[] = {"close", "open", "isrunning", NULL};
  static const int optsum[] = {Y_BGGCCLOSE, Y_BGGCOPEN, Y_BGGCISRUNNING};
  int o = optsum[luaL_checkoption(L, 1, "isrunning", opts)];
  int res = Y_bggc(L, o);
  if (o == Y_BGGCISRUNNING) {
    lua_pushinteger(L, res);
    return 1;
  }
  return 0;
}