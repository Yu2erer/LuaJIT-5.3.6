function test()
	local a = (b == "test")
	return a
end

jit("compile", test)
local res = test()
assert(res == false)

--[[
function <../tests/3_OP_LOADBOOL.lua:1,4> (7 instructions at 0xc98500)
0 params, 2 slots, 1 upvalue, 1 local, 2 constants, 0 functions
	1	[2]	GETTABUP 	0 0 -1	; _ENV "b"
	2	[2]	EQ       	1 0 -2	; - "test"
	3	[2]	JMP      	0 1	; to 5
	4	[2]	LOADBOOL 	0 0 1
	5	[2]	LOADBOOL 	0 1 0
	6	[3]	RETURN   	0 2
	7	[4]	RETURN   	0 1
constants (2) for 0xc98500:
	1	"b"
	2	"test"
locals (1) for 0xc98500:
	0	a	6	8
upvalues (1) for 0xc98500:
	0	_ENV	0	0
]]

--[[
ra = R(0);
setbvalue(ra, 0);
goto Label_5;
Label_4:
ra = R(0);
setbvalue(ra, 1);
Label_5:
]]