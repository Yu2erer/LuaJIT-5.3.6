b = "hello world"
function test()
    local a = b
    return a
end

jit("compile", test)
local res = test()
assert(res == b)

--[[
function <../tests/6_OP_GETTABUP.lua:2,5> (3 instructions at 0x1bc9610)
0 params, 2 slots, 1 upvalue, 1 local, 1 constant, 0 functions
	1	[3]	GETTABUP 	0 0 -1	; _ENV "b"
	2	[4]	RETURN   	0 2
	3	[5]	RETURN   	0 1
constants (1) for 0x1bc9610:
	1	"b"
locals (1) for 0x1bc9610:
	0	a	2	4
upvalues (1) for 0x1bc9610:
	0	_ENV	0	0
]]

--[[
ra = R(0);
rc = K(0);
ci->u.l.savedpc = &cl->p->code[1];
Y_luaV_gettable(L, cl->upvals[0]->v, rc, ra);
base = ci->u.l.base;
]]