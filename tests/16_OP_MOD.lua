function test()
    local a = 22.2
    local mod = 5
    return a % mod
end

jit("compile", test)
local res = test()
assert(res == 22.2 % 5)

--[[
function <../tests/16_OP_MOD.lua:1,5> (5 instructions at 0x13ba590)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 22.2
	2	[3]	LOADK    	1 -2	; 5
	3	[4]	MOD      	2 0 1
	4	[4]	RETURN   	2 2
	5	[5]	RETURN   	0 1
constants (2) for 0x13ba590:
	1	22.2
	2	5
locals (2) for 0x13ba590:
	0	a	2	6
	1	mod	3	6
upvalues (0) for 0x13ba590:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
ci->u.l.savedpc = &cl->p->code[3];
luaO_arith(L, 3, rb, rc, ra);
base = ci->u.l.base;
]]