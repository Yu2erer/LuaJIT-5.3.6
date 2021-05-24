function test()
    local a = 333.1
    local b = 243.4
    return a - b
end

jit("compile", test)
local res = test()
assert(res - 89.7 < 0.0001)

--[[
function <../tests/14_OP_SUB.lua:1,5> (5 instructions at 0x1940590)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 333.1
	2	[3]	LOADK    	1 -2	; 243.4
	3	[4]	SUB      	2 0 1
	4	[4]	RETURN   	2 2
	5	[5]	RETURN   	0 1
constants (2) for 0x1940590:
	1	333.1
	2	243.4
locals (2) for 0x1940590:
	0	a	2	6
	1	b	3	6
upvalues (0) for 0x1940590:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
if (ttisinteger(rb) && ttisinteger(rc)) {
  ib = ivalue(rb); ic = ivalue(rc);
  setivalue(ra, intop(-, ib, ic));
} else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
  setfltvalue(ra, luai_numsub(L, nb, nc));
} else {
ci->u.l.savedpc = &cl->p->code[3];
  luaT_trybinTM(L, rb, rc, ra, TM_SUB);
base = ci->u.l.base;
}
]]
