function test()
    local b = 10
    local a = b
    return a
end

jit("compile", test)
local res = test()
assert(res == 10)

--[[
function <../tests/0_OP_MOVE.lua:1,5> (4 instructions at 0xd1a500)
0 params, 2 slots, 0 upvalues, 2 locals, 1 constant, 0 functions
	1	[2]	LOADK    	0 -1	; 10
	2	[3]	MOVE     	1 0
	3	[4]	RETURN   	1 2
	4	[5]	RETURN   	0 1
constants (1) for 0xd1a500:
	1	10
locals (2) for 0xd1a500:
	0	b	2	5
	1	a	3	5
upvalues (0) for 0xd1a500:
]]

--[[
ra = R(1);
rb = R(0);
setobj2s(L, ra, rb);
]]