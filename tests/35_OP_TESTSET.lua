function test()
    local a, b, c = 1, 2, 3
    a = b and c
    return a
end


jit("compile", test)
local res = test()
assert(res == 3)

--[[
function <../tests/35_OP_TESTSET.lua:1,5> (8 instructions at 0xa25500)
0 params, 3 slots, 0 upvalues, 3 locals, 3 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 1
	2	[2]	LOADK    	1 -2	; 2
	3	[2]	LOADK    	2 -3	; 3
	4	[3]	TESTSET  	0 1 0
	5	[3]	JMP      	0 1	; to 7
	6	[3]	MOVE     	0 2
	7	[4]	RETURN   	0 2
	8	[5]	RETURN   	0 1
constants (3) for 0xa25500:
	1	1
	2	2
	3	3
locals (3) for 0xa25500:
	0	a	4	9
	1	b	4	9
	2	c	4	9
upvalues (0) for 0xa25500:
]]

--[[
ra = R(0);
rb = R(1);
result = !l_isfalse(rb);
if (!result) {
setobj2s(L, ra, rb);
goto Label_6;
}
]]