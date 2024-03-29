/*
 * @Author: Yuerer
 * @Date: 2020-12-16 09:54:20
 * @LastEditTime: 2021-04-19 20:45:01
 */

#ifndef YGC_H
#define YGC_H

/* windows and c89 support nogc but not bggc */
#if !defined(LUA_USE_WINDOWS)
#include <pthread.h>
#endif

#include "lgc.h"

/* lgc.c */
#define Y_NOGCBIT 4 /* object not to be collected */
#define Y_isnogc(x) testbit((x)->marked, Y_NOGCBIT)

/* lbaselib.c */
int nogc (lua_State *L);
int bggc (lua_State *L);

/* lstate.c */
void Y_initgcstate (lua_State *L);

/* lgc.c */
typedef struct Y_bgjob Y_bgjob;
Y_bgjob* Y_createbgjob (lua_State *L);
void Y_submitbgjob (lua_State *L, Y_bgjob *j);
void Y_trybgfree (lua_State*,GCObject*,Y_bgjob*,void(*)(lua_State*, GCObject*));

/* lbaselib.c */
#define Y_GCBASEFUNCS \
{"nogc", nogc},       \
{"bggc", bggc}


#endif
