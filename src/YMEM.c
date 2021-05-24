/*
 * @Author: Yuerer
 * @Date: 2021-05-19 20:03:20
 * @LastEditTime: 2021-05-19 20:03:20
 */

#include "YMEM.h"

#include "ldo.h"


void Y_luaM_free_ (lua_State *L, void *block, size_t osize) {
  global_State *g = G(L);
  (*g->frealloc)(g->ud, block, osize, 0);
}

void *Y_luaM_malloc (lua_State *L, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  newblock = (*g->frealloc)(g->ud, NULL, 0, nsize);
  if (newblock == NULL && nsize > 0) {
    if (g->version) {
      luaC_fullgc(L, 1);
      newblock = (*g->frealloc)(g->ud, NULL, 0, nsize);
    }
    if (newblock == NULL)
      luaD_throw(L, LUA_ERRMEM);
  }
  return newblock;
}

void *Y_luaM_realloc (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  newblock = (*g->frealloc)(g->ud, block, osize, nsize);
  if (newblock == NULL && nsize > 0) {
    if (g->version) {
      luaC_fullgc(L, 1);
      newblock = (*g->frealloc)(g->ud, block, osize, nsize);
    }
    if (newblock == NULL)
      luaD_throw(L, LUA_ERRMEM);
  }
  return newblock;
}