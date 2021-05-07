function test()
    local a, b = 10, "1241"
    return a + b
end

jit("compile", test)
local res = test()
assert(res == 1251.0)

--[[
function <../tests/13_OP_ADD.lua:1,4> (5 instructions at 0xfb5500)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 10
	2	[2]	LOADK    	1 -2	; "1241"
	3	[3]	ADD      	2 0 1
	4	[3]	RETURN   	2 2
	5	[4]	RETURN   	0 1
constants (2) for 0xfb5500:
	1	10
	2	"1241"
locals (2) for 0xfb5500:
	0	a	3	6
	1	b	3	6
upvalues (0) for 0xfb5500:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
ci->u.l.savedpc = &cl->p->code[3];
luaO_arith(L, 0, rb, rc, ra);
base = ci->u.l.base;
]]