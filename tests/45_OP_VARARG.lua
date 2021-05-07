function test(...)
    local a, b, c, d, e = 100, ...
    return e
end

jit("compile", test)
local res = test(3, "4", {}, 533.1, "hello world")
assert(res == 533.1)

--[[
function <../tests/45_OP_VARARG.lua:1,4> (4 instructions at 0x75d500)
0+ params, 5 slots, 0 upvalues, 5 locals, 1 constant, 0 functions
	1	[2]	LOADK    	0 -1	; 100
	2	[2]	VARARG   	1 5
	3	[3]	RETURN   	4 2
	4	[4]	RETURN   	0 1
constants (1) for 0x75d500:
	1	100
locals (5) for 0x75d500:
	0	a	3	5
	1	b	3	5
	2	c	3	5
	3	d	3	5
	4	e	3	5
upvalues (0) for 0x75d500:
]]

--[[
ra = R(0);
rb = K(0);
setobj2s(L, ra, rb);
Y_luaV_vararg(L, ci, cl, 1, 5);
]]