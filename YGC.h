/*
 * @Author: Yuerer
 * @Date: 2020-12-16 09:54:20
 */

#ifndef YGC_H
#define YGC_H

#include "lgc.h"

// lgc.c
#define Y_NOGCBIT 4 /* object not to be collected */
#define Y_isnogc(x) testbit((x)->marked, Y_NOGCBIT)
#define Y_makenogc(x) l_setbit((x)->marked, Y_NOGCBIT)
#define Y_clearnogc(x) resetbit((x)->marked, Y_NOGCBIT)

// lbaselib.c
int nogc (lua_State *L);

#endif