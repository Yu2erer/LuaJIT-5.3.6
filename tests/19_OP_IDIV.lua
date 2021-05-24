function test()
    local a, b = 13.5, 5.3
    return a // b
end

jit("compile", test)
local res = test()
assert(res == 13.5 // 5.3)

--[[
function <../tests/19_OP_IDIV.lua:1,4> (5 instructions at 0x17a9590)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 13.5
	2	[2]	LOADK    	1 -2	; 5.3
	3	[3]	IDIV     	2 0 1
	4	[3]	RETURN   	2 2
	5	[4]	RETURN   	0 1
constants (2) for 0x17a9590:
	1	13.5
	2	5.3
locals (2) for 0x17a9590:
	0	a	3	6
	1	b	3	6
upvalues (0) for 0x17a9590:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
ci->u.l.savedpc = &cl->p->code[3];
luaO_arith(L, 6, rb, rc, ra);
base = ci->u.l.base;
]]