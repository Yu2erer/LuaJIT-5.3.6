function test()
    local a, b, c
end

jit("compile", test)
test()

--[[
function <../tests/4_OP_LOADNIL.lua:1,3> (2 instructions at 0x1800500)
0 params, 3 slots, 0 upvalues, 3 locals, 0 constants, 0 functions
	1	[2]	LOADNIL  	0 2
	2	[3]	RETURN   	0 1
constants (0) for 0x1800500:
locals (3) for 0x1800500:
	0	a	2	3
	1	b	2	3
	2	c	2	3
upvalues (0) for 0x1800500:
]]

--[[
ra = R(0);
setnilvalue(ra++);
setnilvalue(ra++);
setnilvalue(ra++);
]]