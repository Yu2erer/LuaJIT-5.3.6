/*
 * @Author: Yuerer
 * @Date: 2020-12-16 09:54:20
 * @LastEditTime: 2021-04-14 16:47:01
 */

#ifndef YGC_H
#define YGC_H

#include "lgc.h"

#include <pthread.h>

/* lgc.c */
#define Y_NOGCBIT 4 /* object not to be collected */
#define Y_isnogc(x) testbit((x)->marked, Y_NOGCBIT)

/* lbaselib.c */
int nogc (lua_State *L);
int bggc (lua_State *L);

/* lstate.c may init by myself. */
void *Y_BGThread (void *arg);

typedef struct Y_BGJobObject Y_BGJobObject;

Y_BGJobObject *Y_createbgjob (lua_State *L);
void Y_trybgfreeobj (lua_State *L, GCObject *o, Y_BGJobObject *j, 
  void(*fgfreeobj)(lua_State*, GCObject*));
void Y_submitbgjob (lua_State *L, Y_BGJobObject *j);


#endif