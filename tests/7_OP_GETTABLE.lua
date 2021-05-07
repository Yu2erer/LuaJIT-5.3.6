t = {}
local key, value = "index", "value"
t[key] = value
function test()
    return t[key]
end

jit("compile", test)
local res = test()
assert(res == value)

--[[
function <../tests/7_OP_GETTABLE.lua:4,6> (5 instructions at 0x83d770)
0 params, 2 slots, 2 upvalues, 0 locals, 1 constant, 0 functions
	1	[5]	GETTABUP 	0 0 -1	; _ENV "t"
	2	[5]	GETUPVAL 	1 1	; key
	3	[5]	GETTABLE 	0 0 1
	4	[5]	RETURN   	0 2
	5	[6]	RETURN   	0 1
constants (1) for 0x83d770:
	1	"t"
locals (0) for 0x83d770:
upvalues (2) for 0x83d770:
	0	_ENV	0	0
	1	key	1	0
]]

--[[
ra = R(0);
rb = R(0);
rc = R(1);
ci->u.l.savedpc = &cl->p->code[3];
Y_luaV_gettable(L, rb, rc, ra);
base = ci->u.l.base;
]]