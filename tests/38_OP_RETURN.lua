function test()
    return "13", {}, 11, 134134.31
end

jit("compile", test)
local a, b, c, d = test()
assert(a == "13")
assert(type(b) == "table")
assert(c == 11)
assert(d == 134134.31)

--[[
function <../tests/38_OP_RETURN.lua:1,3> (6 instructions at 0x23b2500)
0 params, 4 slots, 0 upvalues, 0 locals, 3 constants, 0 functions
	1	[2]	LOADK    	0 -1	; "13"
	2	[2]	NEWTABLE 	1 0 0
	3	[2]	LOADK    	2 -2	; 11
	4	[2]	LOADK    	3 -3	; 134134.31
	5	[2]	RETURN   	0 5
	6	[3]	RETURN   	0 1
constants (3) for 0x23b2500:
	1	"13"
	2	11
	3	134134.31
locals (0) for 0x23b2500:
upvalues (0) for 0x23b2500:
]]

--[[
ra = R(0);
if (cl->p->sizep > 0) luaF_close(L, base);
return luaD_poscall(L, ci, ra, (5 != 0 ? 5 - 1 : cast_int(L->top - ra)));
]]