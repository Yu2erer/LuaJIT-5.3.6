function test()
    local a, b = 3, 4
    return a | b
end

jit("compile", test)
local res = test()
assert(res == 3 | 4)

--[[
function <../tests/21_OP_BOR.lua:1,4> (5 instructions at 0x7ca590)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 3
	2	[2]	LOADK    	1 -2	; 4
	3	[3]	BOR      	2 0 1
	4	[3]	RETURN   	2 2
	5	[4]	RETURN   	0 1
constants (2) for 0x7ca590:
	1	3
	2	4
locals (2) for 0x7ca590:
	0	a	3	6
	1	b	3	6
upvalues (0) for 0x7ca590:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
  setivalue(ra, intop(|, ib, ic));
} else {
ci->u.l.savedpc = &cl->p->code[3];
  luaT_trybinTM(L, rb, rc, ra, TM_BOR);
base = ci->u.l.base;
}
]]