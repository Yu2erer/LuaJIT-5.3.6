
#ifndef YMEM_H
#define YMEM_H

#include "lstate.h"
#include "llimits.h"

void Y_luaM_free_ (lua_State *L, void *block, size_t osize);
void *Y_luaM_malloc (lua_State *L, size_t nsize);
void *Y_luaM_realloc (lua_State *L, void *block, size_t osize, size_t nsize);

#define Y_luaM_free(L, b) Y_luaM_free_(L, (b), sizeof(*(b)))
#define Y_luaM_new(L, t) cast(t*, Y_luaM_malloc(L, sizeof(t)))
#define Y_luaM_newvector(L,n,t) \
		cast(t*, Y_luaM_malloc(L, (n)*(sizeof(t))))
#define Y_luaM_reallocv(L, b, on, n, t) \
		cast(t*, Y_luaM_realloc(L, (b), (on)*(sizeof(t)), (n)*(sizeof(t))))

#define Y_luaM_freemem(L, b, s) Y_luaM_free_(L, (b), (s))
#define Y_luaM_freearray(L, b, n) Y_luaM_free_(L, (b), (n)*sizeof(*(b)))

#endif