function test()
    local res = 0
    for i="1", 1000.4134, 3 do
        res = res + i
    end
    return res
end

jit("compile", test)
local res = test()
assert(res == 167167.0)

--[[
function <../tests/40_OP_FORPREP.lua:1,7> (9 instructions at 0x2369500)
0 params, 5 slots, 0 upvalues, 5 locals, 4 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 0
	2	[3]	LOADK    	1 -2	; "1"
	3	[3]	LOADK    	2 -3	; 1000.4134
	4	[3]	LOADK    	3 -4	; 3
	5	[3]	FORPREP  	1 1	; to 7
	6	[4]	ADD      	0 0 4
	7	[3]	FORLOOP  	1 -2	; to 6
	8	[6]	RETURN   	0 2
	9	[7]	RETURN   	0 1
constants (4) for 0x2369500:
	1	0
	2	"1"
	3	1000.4134
	4	3
locals (5) for 0x2369500:
	0	res	2	10
	1	(for index)	5	8
	2	(for limit)	5	8
	3	(for step)	5	8
	4	i	6	7
upvalues (0) for 0x2369500:
]]

--[[
lua_Integer iinit_1 = 0, ilimit_1 = 0, istep_1 = 0;
lua_Number ninit_1 = 0, nlimit_1 = 0, nstep_1 = 0;
int is_iloop_1 = 0;
ra = R(1);
rb = ra + 1;
rc = ra + 2;
ci->u.l.savedpc = &cl->p->code[4];
if (ttisinteger(ra) && ttisinteger(rc) && ttisinteger(rb)) { 
  ilimit_1 = ivalue(rb);
  iinit_1 = ivalue(ra);
  istep_1 = ivalue(rc);
  iinit_1 -= istep_1;
  is_iloop_1 = 1;
} else { 
  if (!tonumber(rb, &nlimit_1)) {
luaG_runerror(L, "'for' limit must be a number");
  }
 if (!tonumber(rc, &nstep_1)) {
luaG_runerror(L, "'for' step must be a number");
  }
 if (!tonumber(ra, &ninit_1)) {
luaG_runerror(L, "'for' initial value must be a number");
  }
  ninit_1 -= nstep_1;
  is_iloop_1 = 0;
}
goto Label_6;
Label_5:
ra = R(0);
rb = R(0);
rc = R(4);
ci->u.l.savedpc = &cl->p->code[6];
luaO_arith(L, 0, rb, rc, ra);
base = ci->u.l.base;
Label_6:
if (is_iloop_1) {
  iinit_1 += istep_1;
  if ((0 < istep_1) ? (iinit_1 <= ilimit_1) : (ilimit_1 <= iinit_1)) {
  setivalue(R(4), iinit_1);
goto Label_5;
  }
} else {
  ninit_1 += nstep_1;
  if ((0.0 < nstep_1) ? (ninit_1 <= nlimit_1) : (nlimit_1 <= ninit_1)) {
  setfltvalue(R(4), ninit_1);
goto Label_5;
  }
]]