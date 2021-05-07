function make()
    return "a", "b", "hello world"
end
function test()
    local t = {1, "134", 520.0, {}, make()}
    return t[#t]
end

jit("compile", test)
local res = test()
assert(res == "hello world")

--[[
function <../tests/43_OP_SETLIST.lua:4,7> (12 instructions at 0xf5a820)
0 params, 6 slots, 1 upvalue, 1 local, 4 constants, 0 functions
	1	[5]	NEWTABLE 	0 4 0
	2	[5]	LOADK    	1 -1	; 1
	3	[5]	LOADK    	2 -2	; "134"
	4	[5]	LOADK    	3 -3	; 520.0
	5	[5]	NEWTABLE 	4 0 0
	6	[5]	GETTABUP 	5 0 -4	; _ENV "make"
	7	[5]	CALL     	5 1 0
	8	[5]	SETLIST  	0 0 1	; 1
	9	[6]	LEN      	1 0
	10	[6]	GETTABLE 	1 0 1
	11	[6]	RETURN   	1 2
	12	[7]	RETURN   	0 1
constants (4) for 0xf5a820:
	1	1
	2	"134"
	3	520.0
	4	"make"
locals (1) for 0xf5a820:
	0	t	9	13
upvalues (1) for 0xf5a820:
	0	_ENV	0	0
]]

--[[
ra = R(0);
Y_luaV_setlist(L, ci, ra, 0, 1);
]]