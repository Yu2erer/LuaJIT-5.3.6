function test()
    local a = "hello" .. " ".. "World" .. " " .. "jit"
    return a
end

jit("compile", test)
local res = test()
assert(res == "hello" .. " ".. "World" .. " " .. "jit")

--[[
function <../tests/29_OP_CONCAT.lua:1,4> (8 instructions at 0x1057500)
0 params, 5 slots, 0 upvalues, 1 local, 4 constants, 0 functions
	1	[2]	LOADK    	0 -1	; "hello"
	2	[2]	LOADK    	1 -2	; " "
	3	[2]	LOADK    	2 -3	; "World"
	4	[2]	LOADK    	3 -2	; " "
	5	[2]	LOADK    	4 -4	; "jit"
	6	[2]	CONCAT   	0 0 4
	7	[3]	RETURN   	0 2
	8	[4]	RETURN   	0 1
constants (4) for 0x1057500:
	1	"hello"
	2	" "
	3	"World"
	4	"jit"
locals (1) for 0x1057500:
	0	a	7	9
upvalues (0) for 0x1057500:
]]

--[[
Y_luaV_concat(L, ci, 0, 0, 4);
base = ci->u.l.base;
]]