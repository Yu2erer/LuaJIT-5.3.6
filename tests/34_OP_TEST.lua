function test()
    local a = 1
    local b = 2
    a = a and b
    return a
end

jit("compile", test)
local res = test()
assert(res == 2)

--[[
function <../tests/34_OP_TEST.lua:1,6> (7 instructions at 0x12f1500)
0 params, 2 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 1
	2	[3]	LOADK    	1 -2	; 2
	3	[4]	TEST     	0 0
	4	[4]	JMP      	0 1	; to 6
	5	[4]	MOVE     	0 1
	6	[5]	RETURN   	0 2
	7	[6]	RETURN   	0 1
constants (2) for 0x12f1500:
	1	1
	2	2
locals (2) for 0x12f1500:
	0	a	2	8
	1	b	3	8
upvalues (0) for 0x12f1500:
]]

--[[
ra = R(0);
result = !l_isfalse(ra);
if (!result) {
goto Label_5;
}
]]