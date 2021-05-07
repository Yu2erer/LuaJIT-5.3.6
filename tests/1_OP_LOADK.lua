function test()
    local a = "1111"
end

jit("compile", test)
test()

--[[
function <../tests/1_OP_LOADK.lua:1,3> (2 instructions at 0x21c5500)
0 params, 2 slots, 0 upvalues, 1 local, 1 constant, 0 functions
	1	[2]	LOADK    	0 -1	; "1111"
	2	[3]	RETURN   	0 1
constants (1) for 0x21c5500:
	1	"1111"
locals (1) for 0x21c5500:
	0	a	2	3
upvalues (0) for 0x21c5500:
]]

--[[
ra = R(0);
rb = K(0);
setobj2s(L, ra, rb);
]]