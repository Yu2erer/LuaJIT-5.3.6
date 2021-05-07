function test()
    local a = 10
    local b = ~a
    return b
end

jit("compile", test)
local res = test()
assert(res == -11)


--[[
function <../tests/26_OP_BNOT.lua:1,5> (4 instructions at 0x1459500)
0 params, 2 slots, 0 upvalues, 2 locals, 1 constant, 0 functions
	1	[2]	LOADK    	0 -1	; 10
	2	[3]	BNOT     	1 0
	3	[4]	RETURN   	1 2
	4	[5]	RETURN   	0 1
constants (1) for 0x1459500:
	1	10
locals (2) for 0x1459500:
	0	a	2	5
	1	b	3	5
upvalues (0) for 0x1459500:
]]

--[[
ra = R(1);
rb = R(0);
rc = R(0);
ci->u.l.savedpc = &cl->p->code[2];
luaO_arith(L, 13, rb, rc, ra);
base = ci->u.l.base;
]]