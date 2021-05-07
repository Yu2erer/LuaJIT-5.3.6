function test()
    local a = false
    local b = not a
    return b
end

jit("compile", test)
local res = test()
assert(res == true)

--[[
function <../tests/27_OP_NOT.lua:1,5> (4 instructions at 0x20d6500)
0 params, 2 slots, 0 upvalues, 2 locals, 0 constants, 0 functions
	1	[2]	LOADBOOL 	0 0 0
	2	[3]	NOT      	1 0
	3	[4]	RETURN   	1 2
	4	[5]	RETURN   	0 1
constants (0) for 0x20d6500:
locals (2) for 0x20d6500:
	0	a	2	5
	1	b	3	5
upvalues (0) for 0x20d6500:
]]

--[[
ra = R(1);
rb = R(0);
res = l_isfalse(rb);
setbvalue(ra, res);
]]