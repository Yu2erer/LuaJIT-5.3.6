t = {"123", 11, "test", "world", "hello"}
function test()
    return #t
end

jit("compile", test)
local res = test()
assert(res == #t)

--[[
function <../tests/28_OP_LEN.lua:2,4> (4 instructions at 0x79b7e0)
0 params, 2 slots, 1 upvalue, 0 locals, 1 constant, 0 functions
	1	[3]	GETTABUP 	0 0 -1	; _ENV "t"
	2	[3]	LEN      	0 0
	3	[3]	RETURN   	0 2
	4	[4]	RETURN   	0 1
constants (1) for 0x79b7e0:
	1	"t"
locals (0) for 0x79b7e0:
upvalues (1) for 0x79b7e0:
	0	_ENV	0	0
]]

--[[
ra = R(0);
rb = R(0);
luaV_objlen(L, ra, rb);
base = ci->u.l.base;
]]