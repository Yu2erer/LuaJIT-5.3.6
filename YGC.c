/*
 * @Author: Yuerer
 * @Date: 2020-12-16 10:01:20
 */

#include "YGC.h"

#include "lfunc.h"
#include "ltable.h"
#include "lstring.h"
#include "lauxlib.h"

#define Y_NOGCCCLOSE 0
#define Y_NOGCOPEN   1
#define Y_NOGCCOUNT  2
#define Y_NOGCLEN    3

static void Y_linkrevert (global_State *g, GCObject *o);
static void Y_closeupvalue (lua_State *L, UpVal *u);
static void Y_reallymarkobject (global_State *g, GCObject *o, int b);
static void Y_traverseproto (global_State *g, Proto *f, int b);
static void Y_traverseLclosure (global_State *g, LClosure *cl, int b);
static void Y_traversetable (global_State *g, Table *h, int b);

#define Y_valis(x, b) (iscollectable(x) && (b ? !Y_isnogc(gcvalue(x)) : Y_isnogc(gcvalue(x))))
#define Y_markvalue(g, o, b) { if (Y_valis(o, b)) Y_reallymarkobject(g, gcvalue(o), b); }
#define Y_markobject(g, t, b) { Y_reallymarkobject(g, obj2gco(t), b); }
#define Y_markobjectN(g, t, b) { if (t) Y_markobject(g, t, b); }
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

static void Y_reallymarkobject (global_State *g, GCObject *o, int b) {
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
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      Y_traversetable(g, gco2t(o), b);
      break;
    }
    case LUA_TLCL: {
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      Y_traverseLclosure(g, gco2lcl(o), b);
      break;
    }
    case LUA_TPROTO: {
      if (b) {
        Y_makenogc(o);
      } else {
        Y_resetobject(g, o);
      }
      Y_traverseproto(g, gco2p(o), b);
      break;
    }
    default: break;
  }
}

static void Y_traverseproto (global_State *g, Proto *f, int b) {
  if (f->cache && !Y_isnogc(f->cache)) {
    f->cache = NULL;
  }
  int i;
  Y_markobjectN(g, f->source, b);
  for (i = 0; i < f->sizek; i ++) {
    Y_markvalue(g, &f->k[i], b);
  }
  for (i = 0; i < f->sizeupvalues; i ++) {
    Y_markobjectN(g, f->upvalues[i].name, b)
  }
  for (i = 0; i < f->sizep; i ++) {
    Y_markobjectN(g, f->p[i], b);
  }
  for (i = 0; i <f->sizelocvars; i ++) {
    Y_markobjectN(g, f->locvars[i].varname, b);
  }
  lu_mem mem = sizeof(Proto) + sizeof(Instruction) * f->sizecode +
                         sizeof(Proto *) * f->sizep +
                         sizeof(TValue) * f->sizek +
                         sizeof(int) * f->sizelineinfo +
                         sizeof(LocVar) * f->sizelocvars +
                         sizeof(Upvaldesc) * f->sizeupvalues;
  g->Y_GCmemnogc += (b ? mem : -mem);
}

static void Y_traverseLclosure (global_State *g, LClosure *cl, int b) {
  int i;
  Y_markobjectN(g, cl->p, b);
  Table *reg = hvalue(&g->l_registry);
  const TValue *gt = luaH_getint(reg, LUA_RIDX_GLOBALS);
  GCObject *gto = gcvalue(gt);

  for (i = 0; i < cl->nupvalues; i ++) {
    UpVal *uv = cl->upvals[i];
    if (uv != NULL) {
      GCObject *o = gcvalue(uv->v);
      if (o == gto || !b) continue;
      if (upisopen(uv)) {
        Y_closeupvalue(g->mainthread, uv);
      }
      Y_markvalue(g, uv->v, 1);
    }
  }
  lu_mem mem = sizeLclosure(cl->nupvalues);
  g->Y_GCmemnogc += (b ? mem : -mem);
}

static void Y_traversetable (global_State *g, Table *h, int b) {
  Y_markobjectN(g, h->metatable, b);
  Node *n, *limit = gnode(h, cast(size_t, sizenode(h)));
  unsigned int i;
  for (i = 0; i < h->sizearray; i++) {
    Y_markvalue(g, &h->array[i], b);
  }
  for (n = gnode(h, 0); n < limit; n++) {
    if (!ttisnil(gval(n))) {
      Y_markvalue(g, gkey(n), b);
      Y_markvalue(g, gval(n), b);
    }
  }
  lu_mem mem = sizeof(Table) + sizeof(TValue) * h->sizearray +
                         sizeof(Node) * cast(size_t, allocsizenode(h));
  g->Y_GCmemnogc += (b ? mem : -mem);
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
      Y_markobject(g, h, Y_NOGCCCLOSE);
      break;
    }
    case Y_NOGCOPEN: {
      if (!h) {
        luaL_argerror(L, 2, "Missing a table object");
        break;
      }
      Y_markobject(g, h, Y_NOGCOPEN);
      break;
    }
    case Y_NOGCCOUNT: {
      res = cast_int(g->Y_GCmemnogc >> 10);
      break;
    }
    case Y_NOGCLEN: {
      GCObject *o = G(L)->Y_nogc;
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