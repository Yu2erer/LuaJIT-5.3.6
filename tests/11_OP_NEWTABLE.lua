function test()
    local a = {}
end

jit("compile", test)
test()

--[[
function <../tests/11_OP_NEWTABLE.lua:1,3> (2 instructions at 0xf32500)
0 params, 2 slots, 0 upvalues, 1 local, 0 constants, 0 functions
	1	[2]	NEWTABLE 	0 0 0
	2	[3]	RETURN   	0 1
constants (0) for 0xf32500:
locals (1) for 0xf32500:
	0	a	2	3
upvalues (0) for 0xf32500:
]]

--[[
ra = R(0);
ci->u.l.savedpc = &cl->p->code[1];
Y_luaV_newtable(L, ci, ra, 0, 0);
base = ci->u.l.base;
]]