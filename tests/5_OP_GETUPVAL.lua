local b = "upvalue"
function test()
    local a = b
    return a
end

jit("compile", test)
local res = test()
assert(res == b)

--[[
function <../tests/5_OP_GETUPVAL.lua:2,5> (3 instructions at 0xb11710)
0 params, 2 slots, 1 upvalue, 1 local, 0 constants, 0 functions
	1	[3]	GETUPVAL 	0 0	; b
	2	[4]	RETURN   	0 2
	3	[5]	RETURN   	0 1
constants (0) for 0xb11710:
locals (1) for 0xb11710:
	0	a	2	4
upvalues (1) for 0xb11710:
	0	b	1	0
]]

--[[
ra = R(0);
setobj2s(L, ra, cl->upvals[0]->v);
]]