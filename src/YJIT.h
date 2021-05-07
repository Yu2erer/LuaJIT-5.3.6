/*
 * @Author: Yuerer
 * @Date: 2021-05-19 20:03:20
 * @LastEditTime: 2021-05-19 20:03:20
 */

#ifndef YJIT_H
#define YJIT_H

#include "lua.h"
#include "llimits.h"

#include "mir.h"
#include "c2mir.h"
#include "mir-gen.h"

typedef struct Proto Proto;

/* lbaselib.c */
int jit (lua_State *L);


/* lstate.c */
typedef struct YJIT_State {
  MIR_context_t Y_mirctx;
  struct c2mir_options Y_c2miropts;
  lu_byte Y_jitrunning;
  int Y_limitsize;
  int Y_limitcount;
  int Y_jitcount;
} YJIT_State;

void Y_initjitstate (lua_State *L);
void Y_closejitstate (lua_State *L);

/* lbaselib.c */
#define Y_JITBASEFUNCS \
{"jit", jit}

typedef enum {
  YJIT_NOT_COMPILE = 0,
  YJIT_CANT_COMPILE = 1,
  YJIT_MUST_COMPILE = 2,
  YJIT_IS_COMPILED = 3,
} YJIT_Status;

/* lfunc.c */
typedef struct YJIT_Proto {
  lua_CFunction Y_jitfunc;
  YJIT_Status Y_jitstatus;
  int Y_execcount;
} YJIT_Proto;

void Y_initjitproto (lua_State *L, Proto *p);
void Y_closejitproto (lua_State *L, Proto *p);

int Y_compile (lua_State *L, Proto *p);

/*
jit("compile", function)
jit("iscompiled", function)
jit("isrunning")
jit("open")
jit("stop")
jit("count") -- return jit functions numbers
jit("codegen", function)
jit("setlimitsize", x)
jit("setlimitcount", x)
*/

#endif