/*
 * @Author: Yuerer
 * @Date: 2021-05-19 20:05:20
 * @LastEditTime: 2021-05-27 14:25:20
 */

#include "YJIT.h"

#include <string.h>

#include "YMEM.h"

#include "lvm.h"
#include "lopcodes.h"
#include "lauxlib.h"
#include "lfunc.h"
#include "ldebug.h"


void Y_initjitstate (lua_State *L) {
  global_State *g = G(L);
  g->Y_jitstate = Y_luaM_new(L, YJIT_State);
  g->Y_jitstate->Y_mirctx = MIR_init();
  memset(&g->Y_jitstate->Y_c2miropts, 0, sizeof(struct c2mir_options));
  g->Y_jitstate->Y_c2miropts.message_file = stderr;
  g->Y_jitstate->Y_c2miropts.module_num = 0;
  g->Y_jitstate->Y_jitrunning = 1;
  g->Y_jitstate->Y_limitsize = 100;
  g->Y_jitstate->Y_limitcount = 50;
  g->Y_jitstate->Y_jitcount = 0;
}

void Y_closejitstate (lua_State *L) {
  global_State *g = G(L);
  MIR_finish(g->Y_jitstate->Y_mirctx);
  Y_luaM_free(L, g->Y_jitstate);
}

void Y_initjitproto (lua_State *L, Proto *p) {
  p->Y_jitproto = Y_luaM_new(L, YJIT_Proto);
  p->Y_jitproto->Y_jitfunc = NULL;
  p->Y_jitproto->Y_jitstatus = YJIT_NOT_COMPILE;
  p->Y_jitproto->Y_execcount = 0;
}

void Y_closejitproto (lua_State *L, Proto *p) {
  Y_luaM_free(L, p->Y_jitproto);
}

/* internal use */
typedef struct Y_jitbuffer {
  char *buffer;
  size_t p;
  size_t size;
  size_t capacity;
  void *ud;
} Y_jitbuffer;

static int Y_getc (void *data) {
  Y_jitbuffer *buff = data;
  if (buff->p >= buff->size) return EOF;
  int c = buff->buffer[buff->p];
  if (c == 0) {
    c = EOF;
  } else {
    buff->p ++;
  }
  return c;
}

static void Y_initbuffer (lua_State *L, Y_jitbuffer *buff, size_t capacity) {
  buff->size = buff->p = 0;
  if (capacity <= 0) {
    buff->buffer = NULL;
    buff->capacity = 0;
    return;
  }
  buff->buffer = Y_luaM_newvector(L, capacity, char);
  memset(buff->buffer, 0, capacity * sizeof(char));
  buff->capacity = capacity;
  buff->ud = L;
}

static void Y_resizebuffer (Y_jitbuffer *buff, size_t buffsize) {
  lua_State *L = cast(lua_State*, buff->ud);
  if (buff->capacity >= buffsize) return;
  size_t newsize = buffsize * 2;
  buff->buffer = Y_luaM_reallocv(L, buff->buffer, buff->capacity, newsize, char);
  buff->capacity = newsize;
}

#define Y_freebuffer(buff) \
  Y_luaM_freemem(cast(lua_State*, ((buff)->ud)), (buff)->buffer, (buff)->capacity);

static const char LUA_HEADER[];

static void Y_str2buffer (Y_jitbuffer *buff, const char *str) {
  size_t len = strlen(str);
  size_t newsize = buff->size + len + 2;
  Y_resizebuffer(buff, newsize);
  strcpy(&buff->buffer[buff->size], str);
  buff->size += len;
  buff->buffer[buff->size++] = '\n';
  buff->buffer[buff->size] = 0;
}

static void Y_fstr2buffer (Y_jitbuffer *buff, const char *fmt, ...) {
  va_list args;
  char local_buffer[1024];
  va_start(args, fmt);
  int n = vsnprintf(local_buffer, sizeof(local_buffer), fmt, args);
  if (n < 0) abort();
  va_end(args);
  Y_str2buffer(buff, local_buffer);
}

typedef struct Y_luafunction {
  const char *name;
  void *func;
} Y_luafunction;

static Y_luafunction LUA_FUNCTIONS[] = {
  {"Y_luaV_gettable", Y_luaV_gettable},
  {"Y_luaV_settable", Y_luaV_settable},
  {"Y_luaV_newtable", Y_luaV_newtable},
  {"Y_luaV_concat", Y_luaV_concat},
  {"Y_luaV_closure", Y_luaV_closure},
  {"Y_luaV_setupval", Y_luaV_setupval},
  {"Y_luaV_setlist", Y_luaV_setlist},
  {"Y_luaV_vararg", Y_luaV_vararg},
  {"luaD_precall", luaD_precall},
  {"luaD_poscall", luaD_poscall},
  {"luaD_call", luaD_call},
  {"luaG_runerror", luaG_runerror},
  {"luaO_arith", luaO_arith},
  {"luaV_objlen", luaV_objlen},
  {"luaV_execute", luaV_execute},
  {"luaV_equalobj", luaV_equalobj},
  {"luaV_lessthan", luaV_lessthan},
  {"luaV_lessequal", luaV_lessequal},
  {"luaV_tonumber_", luaV_tonumber_},
  {"luaV_tointeger", luaV_tointeger},
  {"luaT_trybinTM", luaT_trybinTM},
  {"luaF_close", luaF_close},
  {NULL, NULL},
};

static void *import_resolver (const char *name) {
  for (int i = 0; LUA_FUNCTIONS[i].name; i ++) {
    if (!strcmp(name, LUA_FUNCTIONS[i].name)) {
      return LUA_FUNCTIONS[i].func;
    }
  }
  return NULL;
}

/* ---------------------------- codegen ---------------------------- */
#define CODE(fmt, ...) Y_fstr2buffer(buff, fmt ,##__VA_ARGS__)

#define RA CODE("ra = R(%d);", A)
#define RB CODE("rb = R(%d);", B)
#define RA_EQ_RB CODE("setobj2s(L, ra, rb);")
#define RB_EQ_RA CODE("setobj2s(L, rb, ra);")
#define RB_EQ_RC CODE("setobj2s(L, rb, rc);")

#define RKB \
  do { \
    if (!ISK(B)) { \
      CODE("rb = R(%d);", B); \
    } else { \
      CODE("rb = K(%d);", INDEXK(B)); \
    } \
  } while (0)

#define RKC \
  do { \
    if (!ISK(C)) { \
      CODE("rc = R(%d);", C); \
    } else { \
      CODE("rc = K(%d);", INDEXK(C)); \
    } \
  } while (0)

#define KBX CODE("rb = K(%d);", Bx)

/* stack expansion may occur when calling a lua function */
#define PROTECT_BASE CODE("base = ci->u.l.base;")

/* in order to display the correct line number when an error is reported */
#define UPDATE_SAVEDPC CODE("ci->u.l.savedpc = &cl->p->code[%d];", savedpc)

#define SET_LABEL \
  do { \
    if (is_jmps[pc]) { \
      CODE("Label_%d:", pc); \
    } \
  } while (0)

#define JMP CODE("goto Label_%d;", pc)
#define ERROR(str) CODE("luaG_runerror(L, %s);", #str)

/* 0: R(A) := R(B) */
static void emit_move (int A, int B, Y_jitbuffer *buff) {
  RA;
  RB;
  RA_EQ_RB;
}

/* 1-2: R(A) := K(Bx) */
static void emit_loadk (int A, int Bx, Y_jitbuffer *buff) {
  RA;
  KBX;
  RA_EQ_RB;
}

/* 3: R(A) := (Bool)B if (C) pc++ */
static void emit_loadbool (int A, int B, int C, int pc, Y_jitbuffer *buff) {
  RA;
  CODE("setbvalue(ra, %d);", B);
  if (C) JMP;
}

/* 4: R(A), R(A+1), ..., R(A+B) := nil */
static void emit_loadnil (int A, int B, Y_jitbuffer *buff) {
  RA;
  int b = B;
  do {
    CODE("setnilvalue(ra++);");
  } while (b--);
}

/* 5: R(A) := UpValue[B] */
static void emit_getupval (int A, int B, Y_jitbuffer *buff) {
  RA;
  CODE("setobj2s(L, ra, cl->upvals[%d]->v);", B);
}

/* 6: R(A) := UpValue[B][RK(C)] */
static void emit_gettabup (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKC;
  UPDATE_SAVEDPC;
  CODE("Y_luaV_gettable(L, cl->upvals[%d]->v, rc, ra);", B);
  PROTECT_BASE;
}

/* 7: R(A) := R(B)[RK(C)] */
static void emit_gettable (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("Y_luaV_gettable(L, rb, rc, ra);");
  PROTECT_BASE;
}

/* 8: UpValue[A][RK(B)] := RK(C) */
static void emit_settabup (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("Y_luaV_settable(L, cl->upvals[%d]->v, rb, rc);", A);
  PROTECT_BASE;
}

/* 9: UpValue[B] := R(A) */
static void emit_setupval (int A, int B, int savedpc, Y_jitbuffer *buff) {
  RA;
  UPDATE_SAVEDPC;
  CODE("Y_luaV_setupval(L, cl, ra, %d);", B);
}

/* 10: R(A)[RK(B)] := RK(C) */
static void emit_settable (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("Y_luaV_settable(L, ra, rb, rc);");
  PROTECT_BASE;
}

/* 11: R(A) := {} (arraysize = B, hashsize = C) */
static void emit_newtable (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  unsigned int nasize = luaO_fb2int(B), nhsize = luaO_fb2int(C);
  RA;
  UPDATE_SAVEDPC;
  CODE("Y_luaV_newtable(L, ci, ra, %u, %u);", nasize, nhsize);
  PROTECT_BASE;
}

/* 12: R(A + 1) := R(B); R(A) := R(B)[RK(C)] */
static void emit_self (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RB;
  RKC;
  CODE("setobj2s(L, ra + 1, rb);");
  UPDATE_SAVEDPC;
  CODE("Y_luaV_gettable(L, rb, rc, ra);");
  PROTECT_BASE;
}

/* 13: R(A) := RK(B) + RK(C) */
static void emit_add (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  CODE("if (ttisinteger(rb) && ttisinteger(rc)) {");
  CODE("  ib = ivalue(rb); ic = ivalue(rc);");
  CODE("  setivalue(ra, intop(+, ib, ic));");
  CODE("} else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {");
  CODE("  setfltvalue(ra, luai_numadd(L, nb, nc));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rc, ra, TM_ADD);");
  PROTECT_BASE;
  CODE("}");
}

/* 14: R(A) := RK(B) - RK(C) */
static void emit_sub (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  CODE("if (ttisinteger(rb) && ttisinteger(rc)) {");
  CODE("  ib = ivalue(rb); ic = ivalue(rc);");
  CODE("  setivalue(ra, intop(-, ib, ic));");
  CODE("} else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {");
  CODE("  setfltvalue(ra, luai_numsub(L, nb, nc));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rc, ra, TM_SUB);");
  PROTECT_BASE;
  CODE("}");
}

/* 15: R(A) := RK(B) * RK(C) */
static void emit_mul (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  CODE("if (ttisinteger(rb) && ttisinteger(rc)) {");
  CODE("  ib = ivalue(rb); ic = ivalue(rc);");
  CODE("  setivalue(ra, intop(*, ib, ic));");
  CODE("} else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {");
  CODE("  setfltvalue(ra, luai_nummul(L, nb, nc));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rc, ra, TM_MUL);");
  PROTECT_BASE;
  CODE("}");
}

/* 16: R(A) := RK(B) % RK(C) */
static void emit_mod (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("luaO_arith(L, %d, rb, rc, ra);", LUA_OPMOD);
  PROTECT_BASE;
}

/* 17: R(A) := RK(B) ^ RK(C) */
static void emit_pow (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("luaO_arith(L, %d, rb, rc, ra);", LUA_OPPOW);
  PROTECT_BASE;
}

/* 18: R(A) := RK(B) / RK(C) */
static void emit_div (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  CODE("if (tonumber(rb, &nb) && tonumber(rc, &nc)) {");
  CODE("  setfltvalue(ra, luai_numdiv(L, nb, nc));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rc, ra, TM_DIV);");
  PROTECT_BASE;
  CODE("}");
}

/* 19: R(A) := RK(B) // RK(C) */
static void emit_idiv (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("luaO_arith(L, %d, rb, rc, ra);", LUA_OPIDIV);
  PROTECT_BASE;
}

/* 20: R(A) := RK(B) & RK(C) */
static void emit_band (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  CODE("if (tointeger(rb, &ib) && tointeger(rc, &ic)) {");
  CODE("  setivalue(ra, intop(&, ib, ic));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rc, ra, TM_BAND);");
  PROTECT_BASE;
  CODE("}");
}

/* 21: R(A) := RK(B) | RK(C) */
static void emit_bor (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  CODE("if (tointeger(rb, &ib) && tointeger(rc, &ic)) {");
  CODE("  setivalue(ra, intop(|, ib, ic));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rc, ra, TM_BOR);");
  PROTECT_BASE;
  CODE("}");
}

/* 22: R(A) := RK(B) ~ RK(C) */
static void emit_bxor (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  CODE("if (tointeger(rb, &ib) && tointeger(rc, &ic)) {");
  CODE("  setivalue(ra, intop(^, ib, ic));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rc, ra, TM_BXOR);");
  PROTECT_BASE;
  CODE("}");
}

/* 23: R(A) := RK(B) << RK(C) */
static void emit_shl (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("luaO_arith(L, %d, rb, rc, ra);", LUA_OPSHL);
  PROTECT_BASE;
}

/* 24: R(A) := RK(B) >> RK(C) */
static void emit_shr (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("luaO_arith(L, %d, rb, rc, ra);", LUA_OPSHR);
  PROTECT_BASE;
}

/* 25: R(A) := -R(B) */
static void emit_unm (int A, int B, int savedpc, Y_jitbuffer *buff) {
  RA;
  RB;
  CODE("if (ttisinteger(rb)) {");
  CODE("  ib = ivalue(rb);");
  CODE("  setivalue(ra, intop(-, 0, ib));");
  CODE("} else if (tonumber(rb, &nb)) {");
  CODE("  setfltvalue(ra, luai_numunm(L, nb));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rb, ra, TM_UNM);");
  PROTECT_BASE;
  CODE("}");
}

/* 26: R(A) := ~R(B) */
static void emit_bnot (int A, int B, int savedpc, Y_jitbuffer *buff) {
  RA;
  RB;
  CODE("if (tointeger(rb, &ib)) {");
  CODE("  setivalue(ra, intop(^, ~l_castS2U(0), ib));");
  CODE("} else {");
  UPDATE_SAVEDPC;
  CODE("  luaT_trybinTM(L, rb, rb, ra, TM_BNOT);");
  PROTECT_BASE;
  CODE("}");
}

/* 27: R(A) := not R(B) */
static void emit_not (int A, int B, Y_jitbuffer *buff) {
  RA;
  RB;
  CODE("result = l_isfalse(rb);");
  CODE("setbvalue(ra, result);");
}

/* 28: R(A) := length of R(B) */
static void emit_len (int A, int B, int savedpc, Y_jitbuffer *buff) {
  RA;
  RB;
  UPDATE_SAVEDPC;
  CODE("luaV_objlen(L, ra, rb);");
  PROTECT_BASE;
}

/* 29: R(A) := [R(B) .. R(C)] */
static void emit_concat (int A, int B, int C, Y_jitbuffer *buff) {
  CODE("Y_luaV_concat(L, ci, %d, %d, %d);", A, B, C);
  PROTECT_BASE;
}

/* 30: jmp to pc; if (A) close UpValues >= R(A - 1) */
static void emit_jmp (int A, int pc, Y_jitbuffer *buff) {
  if (A) {
    CODE("ra = R(%d);", A - 1);
    CODE("luaF_close(L, ra);");
  }
  JMP;
}

/* 31-33: if ((RK(B) op RK(C)) != A pc ++; else; donextjmp to pc */
static void emit_binary_test (int A, int B, int C, int savedpc, int pc, int jmp_A, OpCode op, Y_jitbuffer *buff) {
  const char *testfunc;
  switch (op) {
    case OP_EQ: testfunc = "luaV_equalobj"; break;
    case OP_LT: testfunc = "luaV_lessthan"; break;
    case OP_LE: testfunc = "luaV_lessequal"; break;
    default: lua_assert(0);
  }
  RKB;
  RKC;
  UPDATE_SAVEDPC;
  CODE("result = %s(L, rb, rc);", testfunc);
  PROTECT_BASE;
  CODE("if (result == %d) {", A);
  if (jmp_A) {
    /* close UpValues >= R(jmp_A - 1) */
    CODE("  ra = R(%d);", jmp_A - 1);
    CODE("  luaF_close(L, ra);");
  }
  JMP;
  CODE("}");
}

/* 34: if R(A) != (Bool)C pc ++; else; donextjmp to pc;
  local a, b; a = a and b */
static void emit_test (int A, int C, int pc, int jmp_A, Y_jitbuffer *buff) {
  RA;
  if (C) {
    CODE("result = l_isfalse(ra);");
  } else {
    CODE("result = !l_isfalse(ra);");
  }
  CODE("if (!result) {");
  if (jmp_A) {
    /* close UpValues >= R(jmp_A - 1) */
    CODE("  ra = R(%d);", jmp_A - 1);
    CODE("  luaF_close(L, ra);");
  }
  JMP;
  CODE("}");
}

/* 35: if R(B) != (Bool)C pc ++; else; ra = rb; donextjmp to pc;
  local a, b, c; a = b and c */
static void emit_testset (int A, int B, int C, int pc, int jmp_A, Y_jitbuffer *buff) {
  RA;
  RB;
  if (C) {
    CODE("result = l_isfalse(rb);");
  } else {
    CODE("result = !l_isfalse(rb);");
  }
  CODE("if (!result) {");
  RA_EQ_RB;
  if (jmp_A) {
    /* close UpValues >= R(jmp_A - 1) */
    CODE("  ra = R(%d);", jmp_A - 1);
    CODE("  luaF_close(L, ra);");
  }
  JMP;
  CODE("}");
}

/* 36-37: [R(A) .. R(A + C - 2)] := [R(A) .. R(A + 1) .. R(A + B - 1)] */
static void emit_call (int A, int B, int C, int savedpc, Y_jitbuffer *buff) {
  RA;
  /* when C == 0, nresults = -1 == LUA_MULTRET */
  int nresults = C - 1;
  if (B) CODE("L->top = R(%d);", A + B);
  UPDATE_SAVEDPC;
  CODE("result = luaD_precall(L, ra, %d, 1);", nresults);
  CODE("if (result) {");
  CODE("  if (result == 1 && %d >= 0) {", nresults);
  CODE("      L->top = ci->top;");
  CODE("  }");
  CODE("} else {");
  /* when we call a lua function in the jit function
    , we need to fix the top of the stack by ourselves */
  CODE("  result = luaV_execute(L);");
  CODE("  if (result) L->top = ci->top;");
  CODE("}");
  PROTECT_BASE;
}

/* 38: return R(A) ... R(A + B - 2) */
static void emit_return (int A, int B, Y_jitbuffer *buff) {
  /* return to the luaD_precall function and fix the top of the stack! */
  RA;
  CODE("if (cl->p->sizep > 0) luaF_close(L, base);");
  CODE("return luaD_poscall(L, ci, ra, (%d != 0 ? %d - 1 : cast_int(L->top - ra)));", B, B);
}

/* 39: R(A) += R(A + 2); if R(A) <= R(A + 1)  pc += sBx; R(A + 3) = R(A) */
static void emit_forloop (int A, int pc, Y_jitbuffer *buff) {
  CODE("if (is_iloop_%d) {", A);
  CODE("  iinit_%d += istep_%d;", A, A);
  CODE("  if ((0 < istep_%d) ? (iinit_%d <= ilimit_%d) : (ilimit_%d <= iinit_%d)) {", A, A, A, A, A);
  /* R(A + 3) is external index */
  CODE("  setivalue(R(%d), iinit_%d);", A + 3, A);
  JMP;
  CODE("  }");
  CODE("} else {");
  CODE("  ninit_%d += nstep_%d;", A, A);
  CODE("  if ((0.0 < nstep_%d) ? (ninit_%d <= nlimit_%d) : (nlimit_%d <= ninit_%d)) {", A, A, A, A, A);
  /* R(A + 3) is external index */
  CODE("  setfltvalue(R(%d), ninit_%d);", A + 3, A);
  JMP;
  CODE("  }");
  CODE("}");
}

/* 40: R(A) -= R(A + 2); pc += sBx donextjmp to pc */
static void emit_forprep (int A, int pc, int savedpc, lu_byte need_define, Y_jitbuffer *buff) {
  /* avoid redefinition */
  if (need_define) {
    CODE("lua_Integer iinit_%d = 0, ilimit_%d = 0, istep_%d = 0;", A, A, A);
    CODE("lua_Number ninit_%d = 0, nlimit_%d = 0, nstep_%d = 0;", A, A, A);
    /* for OP_FORLOOP */
    CODE("int is_iloop_%d = 0;", A);
  }
  RA; /* init */
  CODE("rb = ra + 1;"); /* plimit */
  CODE("rc = ra + 2;"); /* pstep */
  UPDATE_SAVEDPC;
  CODE("if (ttisinteger(ra) && ttisinteger(rc) && ttisinteger(rb)) { ");
  CODE("  ilimit_%d = ivalue(rb);", A);
  CODE("  iinit_%d = ivalue(ra);", A);
  CODE("  istep_%d = ivalue(rc);", A);
  CODE("  iinit_%d -= istep_%d;", A, A);
  CODE("  is_iloop_%d = 1;", A);
  CODE("} else { ");
  CODE("  if (!tonumber(rb, &nlimit_%d)) {", A);
  ERROR("'for' limit must be a number");
  CODE("  }");
  CODE(" if (!tonumber(rc, &nstep_%d)) {", A);
  ERROR("'for' step must be a number");
  CODE("  }");
  CODE(" if (!tonumber(ra, &ninit_%d)) {", A);
  ERROR("'for' initial value must be a number");
  CODE("  }");
  CODE("  ninit_%d -= nstep_%d;", A, A);
  CODE("  is_iloop_%d = 0;", A);
  CODE("}");
  JMP;
}

/* 41: R(A + 3), ..., R(A + 2 + C) := Function(R(A)) (R(A + 1), R(A + 2))*/
static void emit_tforcall (int A, int C, int tforloop_A, int pc, Y_jitbuffer *buff) {
  RA;
  /* ra + 3 = cb */
  CODE("rb = ra + 5;"); /* (ra + 3) + 2 */
  CODE("rc = ra + 2;");
  RB_EQ_RC;
  CODE("rb = ra + 4;"); /* (ra + 3) + 1 */
  CODE("rc = ra + 1;");
  RB_EQ_RC;
  CODE("rb = ra + 3;"); /* cb */
  RB_EQ_RA;
  CODE("L->top = rb + 3;"); /* func + 2 args */
  CODE("luaD_call(L, rb, %d);", C);
  PROTECT_BASE;
  CODE("L->top = ci->top;");
  /* enter OP_TFORLOOP */
  A = tforloop_A;
  RA;
  CODE("if (!ttisnil(ra + 1)) {"); /* continue loop? */
  CODE("  setobj2s(L, ra, ra + 1);");
  JMP;
  CODE("}");
}

/* 42: if R(A + 1) != nil then R(A) = R(A + 1) donextjmp to pc */
static void emit_tforloop (int A, int pc, Y_jitbuffer *buff) {
  RA;
  CODE("if (!ttisnil(ra + 1) {"); /* continue loop? */
  CODE("  setobj2s(L, ra, ra + 1);");
  JMP;
  CODE("}");
}

/* 43: R(A)[(C - 1) * FPF + i] := R(A + i) */
static void emit_setlist (int A, int B, int C, Y_jitbuffer *buff) {
  RA;
  CODE("Y_luaV_setlist(L, ci, ra, %d, %d);", B, C);
}

/* 44: R(A) := closure(Proto[Bx]) */
static void emit_closure (int A, int Bx, Y_jitbuffer *buff) {
  CODE("Y_luaV_closure(L, ci, cl, %d, %d);", A, Bx);
  PROTECT_BASE;
}

/* 45: R(A), R(A + 1), ..., R(A + B - 2) = vararg */
static void emit_vararg (int A, int B, Y_jitbuffer *buff) {
  CODE("Y_luaV_vararg(L, ci, cl, %d, %d);", A, B);
  /* lua function is not called, but the stack may be expanded */
  PROTECT_BASE;
}

#define OPEN_FUNC(name)                             \
  do {                                              \
    CODE("int %s (lua_State *L) {", name);          \
    CODE("int result = 0;");                        \
    CODE("lua_Integer ib = 0, ic = 0;");            \
    CODE("lua_Number nb = 0.0, nc = 0.0;");         \
    CODE("StkId ra = NULL, rb = NULL, rc = NULL;"); \
    CODE("CallInfo *ci = L->ci;");                  \
    CODE("LClosure *cl = clLvalue(ci->func);");     \
    CODE("TValue *k = cl->p->k;");                  \
    CODE("StkId base = ci->u.l.base;");             \
  } while (0)

#define CLOSE_FUNC        \
  do {                    \
    CODE("return 0;");    \
    CODE("}");            \
  } while (0)

#define GET_OPERANDS      \
  do {                    \
    A = GETARG_A(i);      \
    B = GETARG_B(i);      \
    C = GETARG_C(i);      \
    Bx = GETARG_Bx(i);    \
    sBx = GETARG_sBx(i);  \
  } while (0)

static void Y_setjmps (Proto *p, lu_byte *is_jmps) {
  Instruction i;
  for (int pc = 0; pc < p->sizecode; pc ++) {
    i = p->code[pc];
    switch (GET_OPCODE(i)) {
      case OP_LOADBOOL: {
        if (GETARG_C(i)) is_jmps[pc + 2] = 1;
        break;
      }
      case OP_JMP:
      case OP_FORLOOP:
      case OP_FORPREP:
      case OP_TFORLOOP: {
        is_jmps[GETARG_sBx(i) + pc + 1] = 1;
        break;
      }
      default: break;
    }
  }
}

static void Y_codegen (lua_State *L, Proto *p, const char *name, Y_jitbuffer *buff) {
  Y_str2buffer(buff, LUA_HEADER);
  OPEN_FUNC(name);
  const Instruction *code = p->code;
  Instruction i;
  lu_byte *is_jmps = Y_luaM_newvector(L, p->sizecode, lu_byte);
  memset(is_jmps, 0, p->sizecode * sizeof(is_jmps[0]));
  /* avoid forprep redefinition */
  lu_byte *is_defineds = Y_luaM_newvector(L, p->maxstacksize, lu_byte);
  memset(is_defineds, 0, p->maxstacksize * sizeof(is_defineds[0]));
  Y_setjmps(p, is_jmps);
  int A, B, C, Bx, sBx;
  for (int pc = 0; pc < p->sizecode; pc ++) {
    i = code[pc];
    OpCode op = GET_OPCODE(i);
    SET_LABEL;
    GET_OPERANDS;
    switch (op) {
      case OP_MOVE: {
        emit_move(A, B, buff);
        break;
      }
      case OP_LOADK: {
        emit_loadk(A, Bx, buff);
        break;
      }
      case OP_LOADKX: {
        /* skip OP_LOADKX */
        i = code[++ pc];
        op = GET_OPCODE(i);
        lua_assert(op == OP_EXTRAARG);
        /* OP_EXTRAARG now */
        int extra_Ax = GETARG_Ax(i);
        emit_loadk(A, extra_Ax, buff);
        break;
      }
      case OP_LOADBOOL: {
        emit_loadbool(A, B, C, pc + 2, buff);
        break;
      }
      case OP_LOADNIL: {
        emit_loadnil(A, B, buff);
        break;
      }
      case OP_GETUPVAL: {
        emit_getupval(A, B, buff);
        break;
      }
      case OP_GETTABUP: {
        emit_gettabup(A, B, C, pc + 1, buff);
        break;
      }
      case OP_GETTABLE: {
        emit_gettable(A, B, C, pc + 1, buff);
        break;
      }
      case OP_SETTABUP: {
        emit_settabup(A, B, C, pc + 1, buff);
        break;
      }
      case OP_SETUPVAL: {
        emit_setupval(A, B, pc + 1, buff);
        break;
      }
      case OP_SETTABLE: {
        emit_settable(A, B, C, pc + 1, buff);
        break;
      }
      case OP_NEWTABLE: {
        emit_newtable(A, B, C, pc + 1, buff);
        break;
      }
      case OP_SELF: {
        emit_self(A, B, C, pc + 1, buff);
        break;
      }
      case OP_ADD: {
        emit_add(A, B, C, pc + 1, buff);
        break;
      }
      case OP_SUB: {
        emit_sub(A, B, C, pc + 1, buff);
        break;
      }
      case OP_MUL: {
        emit_mul(A, B, C, pc + 1, buff);
        break;
      }
      case OP_MOD: {
        emit_mod(A, B, C, pc + 1, buff);
        break;
      }
      case OP_POW: {
        emit_pow(A, B, C, pc + 1, buff);
        break;
      }
      case OP_DIV: {
        emit_div(A, B, C, pc + 1, buff);
        break;
      }
      case OP_IDIV: {
        emit_idiv(A, B, C, pc + 1, buff);
        break;
      }
      case OP_BAND: {
        emit_band(A, B, C, pc + 1, buff);
        break;
      }
      case OP_BOR: {
        emit_bor(A, B, C, pc + 1, buff);
        break;
      }
      case OP_BXOR: {
        emit_bxor(A, B, C, pc + 1, buff);
        break;
      }
      case OP_SHL: {
        emit_shl(A, B, C, pc + 1, buff);
        break;
      }
      case OP_SHR: {
        emit_shr(A, B, C, pc + 1, buff);
        break;
      }
      case OP_UNM: {
        emit_unm(A, B, pc + 1, buff);
        break;
      }
      case OP_BNOT: {
        emit_bnot(A, B, pc + 1, buff);
        break;
      }
      case OP_NOT: {
        emit_not(A, B, buff);
        break;
      }
      case OP_LEN: {
        emit_len(A, B, pc + 1, buff);
        break;
      }
      case OP_CONCAT: {
        emit_concat(A, B, C, buff);
        break;
      }
      case OP_JMP: {
        emit_jmp(A, pc + sBx + 1, buff);
        break;
      }
      case OP_EQ:
      case OP_LT:
      case OP_LE: {
        /* skip OP_JMP */
        i = code[++ pc];
        lua_assert(GET_OPCODE(i) == OP_JMP);
        /* OP_JMP now */
        int jmp_sBx = GETARG_sBx(i);
        int jmp_A = GETARG_A(i);
        /* pc has been increased */
        emit_binary_test(A, B, C, pc, jmp_sBx + pc + 1, jmp_A, op, buff);
        break;
      }
      case OP_TEST: {
        /* skip OP_JMP */
        i = code[++ pc];
        op = GET_OPCODE(i);
        lua_assert(op == OP_JMP);
        /* OP_JMP now */
        int jmp_sBx = GETARG_sBx(i);
        int jmp_A = GETARG_A(i);
        emit_test(A, C, jmp_sBx + pc + 1, jmp_A, buff);
        break;
      }
      case OP_TESTSET: {
        /* skip OP_JMP */
        i = code[++ pc];
        op = GET_OPCODE(i);
        lua_assert(op == OP_JMP);
        /* OP_JMP now */
        int jmp_sBx = GETARG_sBx(i);
        int jmp_A = GETARG_A(i);
        emit_testset(A, B, C, jmp_sBx + pc + 1, jmp_A, buff);
        break;
      }
      case OP_CALL:
      case OP_TAILCALL: {
        emit_call(A, B, C, pc + 1, buff);
        break;
      }
      case OP_RETURN: {
        emit_return(A, B, buff);
        break;
      }
      /* Numberical for loop */
      case OP_FORLOOP: {
        emit_forloop(A, pc + sBx + 1, buff);
        break;
      }
      case OP_FORPREP: {
        lu_byte need_define = !is_defineds[A];
        if (need_define) is_defineds[A] = 1;
        emit_forprep(A, pc + sBx + 1, pc, need_define, buff);
        break;
      }
      case OP_TFORCALL: {
        /* skip OP_TFORCALL */
        i = code[++ pc];
        op = GET_OPCODE(i);
        lua_assert(op == OP_TFORLOOP);
        /* OP_TFORLOOP now */
        emit_tforcall(A, C, GETARG_A(i), GETARG_sBx(i) + pc + 1, buff);
        break;
      }
      case OP_TFORLOOP: {
        emit_tforloop(A, sBx + pc + 1, buff);
        break;
      }
      case OP_SETLIST: {
        if (C == 0) {
          /* skip OP_SETLIST */
          i = code[++ pc];
          op = GET_OPCODE(i);
          lua_assert(op == OP_EXTRAARG);
          /* OP_EXTRAARG now */
          int extra_Ax = GETARG_Ax(i);
          C = extra_Ax;
        }
        /* if C == 0 C = next instruciton Ax */
        emit_setlist(A, B, C, buff);
        break;
      }
      case OP_CLOSURE: {
        emit_closure(A, Bx, buff);
        break;
      }
      case OP_VARARG: {
        emit_vararg(A, B, buff);
        break;
      }
      case OP_EXTRAARG: {
        lua_assert(0);
        break;
      }
      default: break;
    }
  }
  CLOSE_FUNC;
  Y_luaM_freearray(L, is_jmps, p->sizecode);
  Y_luaM_freearray(L, is_defineds, p->maxstacksize);
  // printf("%s\n", &buff->buffer[strlen(LUA_HEADER)]);
  // FILE *f = fopen("test.log", "w+");
  // fprintf(f, "%s\n", buff->buffer);
  // fclose(f);
}

int Y_compile (lua_State *L, Proto *p) {
  global_State *g = G(L);
  YJIT_Status status = p->Y_jitproto->Y_jitstatus;
  if (status == YJIT_IS_COMPILED) return 1;
  int tocompile = 0;
  if (status == YJIT_MUST_COMPILE) {
    tocompile = 1;
  } else if (p->sizecode > g->Y_jitstate->Y_limitsize && 
      p->Y_jitproto->Y_execcount > g->Y_jitstate->Y_limitcount) {
    tocompile = 1;
  }
  if (!tocompile) return 0;
  int is_success = 1;
  MIR_context_t ctx = g->Y_jitstate->Y_mirctx;
  c2mir_init(ctx);
  MIR_gen_init(ctx, 2);
  Y_jitbuffer buff;
  Y_initbuffer(L, &buff, strlen(LUA_HEADER) + 4096);

  char name[32];
  snprintf(name, sizeof(name), "Y_func_%ld", 
    ++ g->Y_jitstate->Y_c2miropts.module_num);
  Y_codegen(L, p, name, &buff);

  if (!c2mir_compile(ctx, &g->Y_jitstate->Y_c2miropts, Y_getc, &buff, name, NULL)) {
    p->Y_jitproto->Y_jitstatus = YJIT_CANT_COMPILE;
    is_success = 0;
    goto CLEANUP;
  }
  /* c2mir_compile will clear the name */
  snprintf(name, sizeof(name), "Y_func_%ld", 
    g->Y_jitstate->Y_c2miropts.module_num);

  MIR_module_t module = DLIST_TAIL(MIR_module_t, *MIR_get_module_list(ctx));
  MIR_load_module(ctx, module);
  MIR_link(ctx, MIR_set_gen_interface, import_resolver);
  MIR_item_t func = DLIST_HEAD(MIR_item_t, module->items);
  while (func) {
    if (func->item_type == MIR_func_item && !strcmp(name, func->u.func->name)) {
      break;
    }
    func = DLIST_NEXT(MIR_item_t, func);
  }
  if (func == NULL) {
    p->Y_jitproto->Y_jitstatus = YJIT_CANT_COMPILE;
    is_success = 0;
    goto CLEANUP;
  }
  int (*fp)(lua_State *L) = MIR_gen(ctx, 0, func);
  if (fp) {
    p->Y_jitproto->Y_jitfunc = fp;
    p->Y_jitproto->Y_jitstatus = YJIT_IS_COMPILED;
  } else {
    p->Y_jitproto->Y_jitfunc = NULL;
    p->Y_jitproto->Y_jitstatus = YJIT_CANT_COMPILE;
    is_success = 0;
    goto CLEANUP;
  }
CLEANUP:
  MIR_gen_finish(ctx);
  c2mir_finish(ctx);
  Y_freebuffer(&buff);
  return is_success;
}

#define Y_JITCLOSE          0
#define Y_JITOPEN           1
#define Y_JITCOUNT          2
#define Y_JITCODEGEN        3
#define Y_JITCOMPILE        4
#define Y_JITISCOMPILED     5
#define Y_JITISRUNNING      6
#define Y_JITSETLIMITSIZE   7
#define Y_JITSETLIMITCOUNT  8

static Proto* Y_checktoproto (lua_State *L, int arg) {
  luaL_argcheck(L, lua_isfunction(L, arg) && !lua_iscfunction(L, arg), 2,
      "Must be a Lua function");
  LClosure *cl = cast(LClosure*, lua_topointer(L, arg));
  return cl->p;
}

static int Y_jit (lua_State *L, int what) {
  int res = 0;
  global_State *g = G(L);
  switch (what) {
    case Y_JITCLOSE: {
      g->Y_jitstate->Y_jitrunning = 0;
      break;
    }
    case Y_JITOPEN: {
      g->Y_jitstate->Y_jitrunning = 1;
      break;
    }
    case Y_JITCOUNT: {
      res = g->Y_jitstate->Y_jitcount;
      break;
    }
    /* jit(L) will handle this case */
    case Y_JITCODEGEN: break;
    case Y_JITCOMPILE: {
      Proto *p = Y_checktoproto(L, 2);
      if (p->Y_jitproto->Y_jitstatus != YJIT_IS_COMPILED)
        p->Y_jitproto->Y_jitstatus = YJIT_MUST_COMPILE;
      Y_compile(L, p);
      break;
    }
    case Y_JITISCOMPILED: {
      Proto *p = Y_checktoproto(L, 2);
      res = (p->Y_jitproto->Y_jitstatus == YJIT_IS_COMPILED);
      break;
    }
    case Y_JITISRUNNING: {
      res = g->Y_jitstate->Y_jitrunning;
      break;
    }
    case Y_JITSETLIMITSIZE: {
      res = g->Y_jitstate->Y_limitsize;
      int ex = (int)luaL_optinteger(L, 2, 0);
      g->Y_jitstate->Y_limitsize = ex;
      break;
    }
    case Y_JITSETLIMITCOUNT: {
      res = g->Y_jitstate->Y_limitsize;
      int ex = (int)luaL_optinteger(L, 2, 0);
      g->Y_jitstate->Y_limitcount = ex;
      break;
    }
    default: res = -1;
  }
  return res;
}

int jit (lua_State *L) {
  static const char* const opts[] = {"close", "open", "count",
    "codegen", "compile", "iscompiled", "isrunning",
     "setlimitsize", "setlimitcount", NULL};
  static const int optsnum[] = {Y_JITCLOSE, Y_JITOPEN, Y_JITCOUNT,
    Y_JITCODEGEN, Y_JITCOMPILE, Y_JITISCOMPILED, Y_JITISRUNNING, 
    Y_JITSETLIMITSIZE, Y_JITSETLIMITCOUNT};

  int o = optsnum[luaL_checkoption(L, 1, "isrunning", opts)];
  int res = Y_jit(L, o);
  switch (o) {
    case Y_JITCOUNT: {
      lua_pushinteger(L, res);
      return 1;
    }
    case Y_JITCODEGEN: {
      Proto *p = Y_checktoproto(L, 2);
      Y_jitbuffer buff;
      Y_initbuffer(L, &buff, strlen(LUA_HEADER) + 4096);
      Y_codegen(L, p, "Y_codegen_", &buff);
      lua_pushlstring(L, buff.buffer, buff.size);
      Y_freebuffer(&buff);
      return 1;
    }
    case Y_JITCOMPILE:
    case Y_JITISCOMPILED:
    case Y_JITISRUNNING: {
      lua_pushboolean(L, res);
      return 1;
    }
    case Y_JITSETLIMITSIZE:
    case Y_JITSETLIMITCOUNT: {
      lua_pushinteger(L, res);
      return 1;
    }
    default: break;
  }
  return 0;
}

static const char LUA_HEADER[] = {
  "#ifdef __MIRC__\n"
  "#endif\n"
    "typedef long long size_t;\n"
    "typedef long long ptrdiff_t;\n"
    "typedef long long intptr_t;\n"
    "typedef long long int64_t;\n"
    "typedef unsigned long long uint64_t;\n"
    "typedef int int32_t;\n"
    "typedef unsigned int uint32_t;\n"
    "typedef short int16_t;\n"
    "typedef unsigned short uint16_t;\n"
    "typedef char int8_t;\n"
    "typedef unsigned char uint8_t;\n"
    "typedef size_t lu_mem;\n"
    "typedef ptrdiff_t l_mem;\n"
    "typedef unsigned char lu_byte;\n"
    "typedef uint16_t LuaType;\n"
    "#define NULL ((void *)0)\n"
    "typedef struct lua_State lua_State;\n"
    "#define LUA_TNONE		(-1)\n"
    "#define LUA_TNIL		0\n"
    "#define LUA_TBOOLEAN		1\n"
    "#define LUA_TLIGHTUSERDATA	2\n"
    "#define LUA_TNUMBER		3\n"
    "#define LUA_TSTRING		4\n"
    "#define LUA_TTABLE		5\n"
    "#define LUA_TFUNCTION		6\n"
    "#define LUA_TUSERDATA		7\n"
    "#define LUA_TTHREAD		8\n"
    "#define LUA_OK  0\n"
    "typedef enum {TM_INDEX,TM_NEWINDEX,TM_GC,\n"
    "	TM_MODE,TM_LEN,TM_EQ,TM_ADD,TM_SUB,TM_MUL,\n"
    "	TM_MOD,TM_POW,TM_DIV,TM_IDIV,TM_BAND,TM_BOR,\n"
    "	TM_BXOR,TM_SHL,TM_SHR,TM_UNM,TM_BNOT,TM_LT,\n"
    "	TM_LE,TM_CONCAT,TM_CALL,TM_N\n"
    "} TMS;\n"
    "typedef double lua_Number;\n"
    "typedef int64_t lua_Integer;\n"
    "typedef uint64_t lua_Unsigned;\n"
    "typedef int (*lua_CFunction) (lua_State *L);\n"
    "typedef union {\n"
    "	lua_Number n;\n"
    "	double u;\n"
    "	void *s;\n"
    "	lua_Integer i;\n"
    "	long l;\n"
    "} L_Umaxalign;\n"
    "#define lua_assert(c)		((void)0)\n"
    "#define check_exp(c,e)		(e)\n"
    "#define lua_longassert(c)	((void)0)\n"
    "#define cast(t, exp)	((t)(exp))\n"
    "#define cast_void(i)	cast(void, (i))\n"
    "#define cast_byte(i)	cast(lu_byte, (i))\n"
    "#define cast_num(i)	cast(lua_Number, (i))\n"
    "#define cast_int(i)	cast(int, (i))\n"
    "#define cast_uchar(i)	cast(unsigned char, (i))\n"
    "#define l_castS2U(i)	((lua_Unsigned)(i))\n"
    "#define l_castU2S(i)	((lua_Integer)(i))\n"
    "#define l_noret		void\n"
    "typedef unsigned int Instruction;\n"
    "#define luai_numdiv(L,a,b)      ((a)/(b))\n"
    "#define luai_numadd(L,a,b)      ((a)+(b))\n"
    "#define luai_numsub(L,a,b)      ((a)-(b))\n"
    "#define luai_nummul(L,a,b)      ((a)*(b))\n"
    "#define luai_numunm(L,a)        (-(a))\n"
    "#define luai_numeq(a,b)         ((a)==(b))\n"
    "#define LUA_TLCL	(LUA_TFUNCTION | (0 << 4))\n"
    "#define LUA_TLCF	(LUA_TFUNCTION | (1 << 4))\n"
    "#define LUA_TCCL	(LUA_TFUNCTION | (2 << 4))\n"
    "#define LUA_TSHRSTR	(LUA_TSTRING | (0 << 4))\n"
    "#define LUA_TLNGSTR	(LUA_TSTRING | (1 << 4))\n"
    "#define LUA_TNUMFLT	(LUA_TNUMBER | (0 << 4))\n"
    "#define LUA_TNUMINT	(LUA_TNUMBER | (1 << 4))\n"
    "#define BIT_ISCOLLECTABLE	(1 << 15)\n"
    "#define ctb(t)			((t) | BIT_ISCOLLECTABLE)\n"
    "typedef struct GCObject GCObject;\n"
    "#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked\n"
    "struct GCObject {\n"
    "  CommonHeader;\n"
    "};\n"
    "typedef union Value {\n"
    "  GCObject *gc;\n"
    "  void *p;\n"
    "  int b;\n"
    "  lua_CFunction f;\n"
    "  lua_Integer i;\n"
    "  lua_Number n;\n"
    "} Value;\n"
    "#define TValuefields	Value value_; LuaType tt_\n"
    "typedef struct lua_TValue {\n"
    "  TValuefields;\n"
    "} TValue;\n"
    "#define NILCONSTANT	{NULL}, LUA_TNIL\n"
    "#define val_(o)		((o)->value_)\n"
    "#define rttype(o)	((o)->tt_)\n"
    "#define novariant(x)	((x) & 0x0F)\n"
    "#define ttype(o)	(rttype(o) & 0x7F)\n"
    "#define ttnov(o)	(novariant(rttype(o)))\n"
    "#define checktag(o,t)		(rttype(o) == (t))\n"
    "#define checktype(o,t)		(ttnov(o) == (t))\n"
    "#define ttisnumber(o)		checktype((o), LUA_TNUMBER)\n"
    "#define ttisfloat(o)		checktag((o), LUA_TNUMFLT)\n"
    "#define ttisinteger(o)		checktag((o), LUA_TNUMINT)\n"
    "#define ttisnil(o)		checktag((o), LUA_TNIL)\n"
    "#define ttisboolean(o)		checktag((o), LUA_TBOOLEAN)\n"
    "#define ttislightuserdata(o)	checktag((o), LUA_TLIGHTUSERDATA)\n"
    "#define ttisstring(o)		checktype((o), LUA_TSTRING)\n"
    "#define ttisshrstring(o)	checktag((o), ctb(LUA_TSHRSTR))\n"
    "#define ttislngstring(o)	checktag((o), ctb(LUA_TLNGSTR))\n"
    "#define ttistable(o)		checktype((o), LUA_TTABLE)\n"
    "#define ttisarray(o)     (ttisiarray(o) || ttisfarray(o))\n"
    "#define ttisLtable(o)    checktag((o), ctb(LUA_TTABLE))\n"
    "#define ttisfunction(o)		checktype(o, LUA_TFUNCTION)\n"
    "#define ttisclosure(o)		((rttype(o) & 0x1F) == LUA_TFUNCTION)\n"
    "#define ttisCclosure(o)		checktag((o), ctb(LUA_TCCL))\n"
    "#define ttisLclosure(o)		checktag((o), ctb(LUA_TLCL))\n"
    "#define ttislcf(o)		checktag((o), LUA_TLCF)\n"
    "#define ttisfulluserdata(o)	checktag((o), ctb(LUA_TUSERDATA))\n"
    "#define ttisthread(o)		checktag((o), ctb(LUA_TTHREAD))\n"
    "#define ttisdeadkey(o)		checktag((o), LUA_TDEADKEY)\n"
    "#define ivalue(o)	check_exp(ttisinteger(o), val_(o).i)\n"
    "#define fltvalue(o)	check_exp(ttisfloat(o), val_(o).n)\n"
    "#define nvalue(o)	check_exp(ttisnumber(o), \\\n"
    "	(ttisinteger(o) ? cast_num(ivalue(o)) : fltvalue(o)))\n"
    "#define gcvalue(o)	check_exp(iscollectable(o), val_(o).gc)\n"
    "#define pvalue(o)	check_exp(ttislightuserdata(o), val_(o).p)\n"
    "#define tsvalue(o)	check_exp(ttisstring(o), gco2ts(val_(o).gc))\n"
    "#define uvalue(o)	check_exp(ttisfulluserdata(o), gco2u(val_(o).gc))\n"
    "#define clvalue(o)	check_exp(ttisclosure(o), gco2cl(val_(o).gc))\n"
    "#define clLvalue(o)	check_exp(ttisLclosure(o), gco2lcl(val_(o).gc))\n"
    "#define clCvalue(o)	check_exp(ttisCclosure(o), gco2ccl(val_(o).gc))\n"
    "#define fvalue(o)	check_exp(ttislcf(o), val_(o).f)\n"
    "#define fcfvalue(o) check_exp(ttisfcf(o), val_(o).p)\n"
    "#define hvalue(o)	check_exp(ttistable(o), gco2t(val_(o).gc))\n"
    "#define bvalue(o)	check_exp(ttisboolean(o), val_(o).b)\n"
    "#define thvalue(o)	check_exp(ttisthread(o), gco2th(val_(o).gc))\n"
    "#define deadvalue(o)	check_exp(ttisdeadkey(o), cast(void *, val_(o).gc))\n"
    "#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))\n"
    "#define iscollectable(o)	(rttype(o) & BIT_ISCOLLECTABLE)\n"
    "#define righttt(obj)		(ttype(obj) == gcvalue(obj)->tt)\n"
    "#define checkliveness(L,obj) \\\n"
    "	lua_longassert(!iscollectable(obj) || \\\n"
    "		(righttt(obj) && (L == NULL || !isdead(G(L),gcvalue(obj)))))\n"
    "#define settt_(o,t)	((o)->tt_=(t))\n"
    "#define setfltvalue(obj,x) \\\n"
    "  { TValue *io=(obj); val_(io).n=(x); settt_(io, LUA_TNUMFLT); }\n"
    "#define chgfltvalue(obj,x) \\\n"
    "  { TValue *io=(obj); lua_assert(ttisfloat(io)); val_(io).n=(x); }\n"
    "#define setivalue(obj,x) \\\n"
    "  { TValue *io=(obj); val_(io).i=(x); settt_(io, LUA_TNUMINT); }\n"
    "#define chgivalue(obj,x) \\\n"
    "  { TValue *io=(obj); lua_assert(ttisinteger(io)); val_(io).i=(x); }\n"
    "#define setnilvalue(obj) settt_(obj, LUA_TNIL)\n"
    "#define setfvalue(obj,x) \\\n"
    "  { TValue *io=(obj); val_(io).f=(x); settt_(io, LUA_TLCF); }\n"
    "#define setfvalue_fastcall(obj, x, tag) \\\n"
    "{ \\\n"
    "    TValue *io = (obj);   \\\n"
    "    lua_assert(tag >= 1 && tag < 0x80); \\\n"
    "    val_(io).p = (x);     \\\n"
    "}\n"
    "#define setpvalue(obj,x) \\\n"
    "  { TValue *io=(obj); val_(io).p=(x); settt_(io, LUA_TLIGHTUSERDATA); }\n"
    "#define setbvalue(obj,x) \\\n"
    "  { TValue *io=(obj); val_(io).b=(x); settt_(io, LUA_TBOOLEAN); }\n"
    "#define setgcovalue(L,obj,x) \\\n"
    "  { TValue *io = (obj); GCObject *i_g=(x); \\\n"
    "    val_(io).gc = i_g; settt_(io, ctb(i_g->tt)); }\n"
    "#define setsvalue(L,obj,x) \\\n"
    "  { TValue *io = (obj); TString *x_ = (x); \\\n"
    "    val_(io).gc = obj2gco(x_); settt_(io, ctb(x_->tt)); \\\n"
    "    checkliveness(L,io); }\n"
    "#define setuvalue(L,obj,x) \\\n"
    "  { TValue *io = (obj); Udata *x_ = (x); \\\n"
    "    val_(io).gc = obj2gco(x_); settt_(io, ctb(LUA_TUSERDATA)); \\\n"
    "    checkliveness(L,io); }\n"
    "#define setthvalue(L,obj,x) \\\n"
    "  { TValue *io = (obj); lua_State *x_ = (x); \\\n"
    "    val_(io).gc = obj2gco(x_); settt_(io, ctb(LUA_TTHREAD)); \\\n"
    "    checkliveness(L,io); }\n"
    "#define setclLvalue(L,obj,x) \\\n"
    "  { TValue *io = (obj); LClosure *x_ = (x); \\\n"
    "    val_(io).gc = obj2gco(x_); settt_(io, ctb(LUA_TLCL)); \\\n"
    "    checkliveness(L,io); }\n"
    "#define setclCvalue(L,obj,x) \\\n"
    "  { TValue *io = (obj); CClosure *x_ = (x); \\\n"
    "    val_(io).gc = obj2gco(x_); settt_(io, ctb(LUA_TCCL)); \\\n"
    "    checkliveness(L,io); }\n"
    "#define sethvalue(L,obj,x) \\\n"
    "  { TValue *io = (obj); Table *x_ = (x); \\\n"
    "    val_(io).gc = obj2gco(x_); settt_(io, ctb(LUA_TTABLE)); \\\n"
    "    checkliveness(L,io); }\n"
    "#define setdeadvalue(obj)	settt_(obj, LUA_TDEADKEY)\n"
    "#define setobj(L,obj1,obj2) \\\n"
    "	{ TValue *io1=(obj1); const TValue *io2=(obj2); io1->tt_ = io2->tt_; val_(io1).n = val_(io2).n; \\\n"
    "	  (void)L; }\n"
    "#define setobjs2s	setobj\n"
    "#define setobj2s	setobj\n"
    "#define setsvalue2s	setsvalue\n"
    "#define sethvalue2s	sethvalue\n"
    "#define setptvalue2s	setptvalue\n"
    "#define setobjt2t	setobj\n"
    "#define setobj2n	setobj\n"
    "#define setsvalue2n	setsvalue\n"
    "#define setobj2t	setobj\n"
    "typedef TValue *StkId;\n"
    "typedef struct TString {\n"
    "	CommonHeader;\n"
    "	lu_byte extra;\n"
    "	lu_byte shrlen;\n"
    "	unsigned int hash;\n"
    "	union {\n"
    "		size_t lnglen;\n"
    "		struct TString *hnext;\n"
    "	} u;\n"
    "} TString;\n"
    "typedef union UTString {\n"
    "	L_Umaxalign dummy;\n"
    "	TString tsv;\n"
    "} UTString;\n"
    "#define getstr(ts)  \\\n"
    "  check_exp(sizeof((ts)->extra), cast(char *, (ts)) + sizeof(UTString))\n"
    "#define svalue(o)       getstr(tsvalue(o))\n"
    "#define tsslen(s)	((s)->tt == LUA_TSHRSTR ? (s)->shrlen : (s)->u.lnglen)\n"
    "#define vslen(o)	tsslen(tsvalue(o))\n"
    "typedef struct Udata {\n"
    "	CommonHeader;\n"
    "	LuaType ttuv_;\n"
    "	struct Table *metatable;\n"
    "	size_t len;\n"
    "	union Value user_;\n"
    "} Udata;\n"
    "typedef union UUdata {\n"
    "	L_Umaxalign dummy;\n"
    "	Udata uv;\n"
    "} UUdata;\n"
    "#define getudatamem(u)  \\\n"
    "  check_exp(sizeof((u)->ttuv_), (cast(char*, (u)) + sizeof(UUdata)))\n"
    "#define setuservalue(L,u,o) \\\n"
    "	{ const TValue *io=(o); Udata *iu = (u); \\\n"
    "	  iu->user_ = io->value_; iu->ttuv_ = rttype(io); \\\n"
    "	  checkliveness(L,io); }\n"
    "#define getuservalue(L,u,o) \\\n"
    "	{ TValue *io=(o); const Udata *iu = (u); \\\n"
    "	  io->value_ = iu->user_; settt_(io, iu->ttuv_); \\\n"
    "	  checkliveness(L,io); }\n"
    "typedef struct Upvaldesc {\n"
    "	TString *name;\n"
    "	TString *usertype;\n"
    "	lu_byte instack;\n"
    "	lu_byte idx;\n"
    "} Upvaldesc;\n"
    "typedef struct LocVar {\n"
    "	TString *varname;\n"
    "	TString *usertype;\n"
    "	int startpc;\n"
    "	int endpc;\n"
    "} LocVar;\n"
    "typedef struct Proto {\n"
    "	CommonHeader;\n"
    "	lu_byte numparams;\n"
    "	lu_byte is_vararg;\n"
    "	lu_byte maxstacksize;\n"
    "	int sizeupvalues;\n"
    "	int sizek;\n"
    "	int sizecode;\n"
    "	int sizelineinfo;\n"
    "	int sizep;\n"
    "	int sizelocvars;\n"
    "	int linedefined;\n"
    "	int lastlinedefined;\n"
    "	TValue *k;\n"
    "	Instruction *code;\n"
    "	struct Proto **p;\n"
    "	int *lineinfo;\n"
    "	LocVar *locvars;\n"
    "	Upvaldesc *upvalues;\n"
    "	struct LClosure *cache;\n"
    "	TString  *source;\n"
    "	GCObject *gclist;\n"
    "} Proto;\n"
    "typedef struct UpVal UpVal;\n"
    "#define ClosureHeader \\\n"
    "	CommonHeader; lu_byte nupvalues; GCObject *gclist\n"
    "typedef struct CClosure {\n"
    "	ClosureHeader;\n"
    "	lua_CFunction f;\n"
    "	TValue upvalue[1];\n"
    "} CClosure;\n"
    "typedef struct LClosure {\n"
    "	ClosureHeader;\n"
    "	struct Proto *p;\n"
    "	UpVal *upvals[1];\n"
    "} LClosure;\n"
    "typedef union Closure {\n"
    "	CClosure c;\n"
    "	LClosure l;\n"
    "} Closure;\n"
    "#define isLfunction(o)	ttisLclosure(o)\n"
    "#define getproto(o)	(clLvalue(o)->p)\n"
    "typedef union TKey {\n"
    "	struct {\n"
    "		TValuefields;\n"
    "		int next;\n"
    "	} nk;\n"
    "	TValue tvk;\n"
    "} TKey;\n"
    "#define setnodekey(L,key,obj) \\\n"
    "	{ TKey *k_=(key); const TValue *io_=(obj); \\\n"
    "	  k_->nk.value_ = io_->value_; k_->nk.tt_ = io_->tt_; \\\n"
    "	  (void)L; checkliveness(L,io_); }\n"
    "typedef struct Node {\n"
    "	TValue i_val;\n"
    "	TKey i_key;\n"
    "} Node;\n"
    "typedef struct Table {\n"
    " CommonHeader;\n"
    " lu_byte flags;\n"
    " lu_byte lsizenode;\n"
    " unsigned int sizearray;\n"
    " TValue *array;\n"
    " Node *node;\n"
    " Node *lastfree;\n"
    " struct Table *metatable;\n"
    " GCObject *gclist;\n"
    " unsigned int hmask;\n"
    "} Table;\n"
    "typedef struct Mbuffer {\n"
    "	char *buffer;\n"
    "	size_t n;\n"
    "	size_t buffsize;\n"
    "} Mbuffer;\n"
    "typedef struct stringtable {\n"
    "	TString **hash;\n"
    "	int nuse;\n"
    "	int size;\n"
    "} stringtable;\n"
    "struct lua_Debug;\n"
    "typedef intptr_t lua_KContext;\n"
    "typedef int(*lua_KFunction)(struct lua_State *L, int status, lua_KContext ctx);\n"
    "typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize,\n"
    "	size_t nsize);\n"
    "typedef void(*lua_Hook)(struct lua_State *L, struct lua_Debug *ar);\n"
    "typedef struct CallInfo {\n"
    "	StkId func;\n"
    "	StkId	top;\n"
    "	struct CallInfo *previous, *next;\n"
    "	union {\n"
    "		struct {\n"
    "			StkId base;\n"
    "			const Instruction *savedpc;\n"
    "		} l;\n"
    "		struct {\n"
    "			lua_KFunction k;\n"
    "			ptrdiff_t old_errfunc;\n"
    "			lua_KContext ctx;\n"
    "		} c;\n"
    "	} u;\n"
    "	ptrdiff_t extra;\n"
    "	short nresults;\n"
    "	unsigned short callstatus;\n"
    "	unsigned short stacklevel;\n"
    "	lu_byte jitstatus;\n"
    "   lu_byte magic;\n"
    "} CallInfo;\n"
    "typedef struct global_State global_State;\n"
    "struct lua_State {\n"
    "	CommonHeader;\n"
    "	lu_byte status;\n"
    "	StkId top;\n"
    "	global_State *l_G;\n"
    "	CallInfo *ci;\n"
    "	const Instruction *oldpc;\n"
    "	StkId stack_last;\n"
    "	StkId stack;\n"
    "	UpVal *openupval;\n"
    "	GCObject *gclist;\n"
    "	struct lua_State *twups;\n"
    "	struct lua_longjmp *errorJmp;\n"
    "	CallInfo base_ci;\n"
    "	volatile lua_Hook hook;\n"
    "	ptrdiff_t errfunc;\n"
    "	int stacksize;\n"
    "	int basehookcount;\n"
    "	int hookcount;\n"
    "	unsigned short nny;\n"
    "	unsigned short nCcalls;\n"
    "	lu_byte hookmask;\n"
    "	lu_byte allowhook;\n"
    "	unsigned short nci;\n"
    "   lu_byte magic;\n"
    "};\n"
    "#define G(L)	(L->l_G)\n"
    "union GCUnion {\n"
    "	GCObject gc;\n"
    "	struct TString ts;\n"
    "	struct Udata u;\n"
    "	union Closure cl;\n"
    "	struct Table h;\n"
    "	struct Proto p;\n"
    "	struct lua_State th;\n"
    "};\n"
    "struct UpVal {\n"
    "	TValue *v;\n"
    "	lu_mem refcount;\n"
    "	union {\n"
    "		struct {\n"
    "			UpVal *next;\n"
    "			int touched;\n"
    "		} open;\n"
    "		TValue value;\n"
    "	} u;\n"
    "};\n"
    "#define cast_u(o)	cast(union GCUnion *, (o))\n"
    "#define gco2ts(o)  \\\n"
    "	check_exp(novariant((o)->tt) == LUA_TSTRING, &((cast_u(o))->ts))\n"
    "#define gco2u(o)  check_exp((o)->tt == LUA_TUSERDATA, &((cast_u(o))->u))\n"
    "#define gco2lcl(o)  check_exp((o)->tt == LUA_TLCL, &((cast_u(o))->cl.l))\n"
    "#define gco2ccl(o)  check_exp((o)->tt == LUA_TCCL, &((cast_u(o))->cl.c))\n"
    "#define gco2cl(o)  \\\n"
    "	check_exp(novariant((o)->tt) == LUA_TFUNCTION, &((cast_u(o))->cl))\n"
    "#define gco2t(o)  check_exp((o)->tt == LUA_TTABLE, &((cast_u(o))->h))\n"
    "#define gco2p(o)  check_exp((o)->tt == LUA_TPROTO, &((cast_u(o))->p))\n"
    "#define gco2th(o)  check_exp((o)->tt == LUA_TTHREAD, &((cast_u(o))->th))\n"
    "#define obj2gco(v) \\\n"
    "	check_exp(novariant((v)->tt) < LUA_TDEADKEY, (&(cast_u(v)->gc)))\n"
    "#define LUA_FLOORN2I		0\n"
    "#define tonumber(o,n) \\\n"
    "  (ttisfloat(o) ? (*(n) = fltvalue(o), 1) : luaV_tonumber_(o,n))\n"
    "#define tointeger(o,i) \\\n"
    "  (ttisinteger(o) ? (*(i) = ivalue(o), 1) : luaV_tointeger(o,i,LUA_FLOORN2I))\n"
    "#define R(i) (base + i)\n"
    "#define K(i) (k + i)\n"
    "#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))\n"
    "#define inf (1./0.)\n\n"
};