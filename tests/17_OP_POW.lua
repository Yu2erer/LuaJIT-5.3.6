function test()
    local a, b = 3.3, 12
    return a ^ b
end

jit("compile", test)
local res = test()
assert(res == 3.3 ^ 12)

--[[
function <../tests/17_OP_POW.lua:1,4> (5 instructions at 0x1d5d590)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 3.3
	2	[2]	LOADK    	1 -2	; 12
	3	[3]	POW      	2 0 1
	4	[3]	RETURN   	2 2
	5	[4]	RETURN   	0 1
constants (2) for 0x1d5d590:
	1	3.3
	2	12
locals (2) for 0x1d5d590:
	0	a	3	6
	1	b	3	6
upvalues (0) for 0x1d5d590:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
ci->u.l.savedpc = &cl->p->code[3];
luaO_arith(L, 4, rb, rc, ra);
base = ci->u.l.base;
]]