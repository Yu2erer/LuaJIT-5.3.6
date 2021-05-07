local t = {}
function test()
    t["key"] = "value"
end

jit("compile", test)
test()
assert(t["key"] == "value")

--[[
function <../tests/8_OP_SETTABUP.lua:2,4> (2 instructions at 0x1e44600)
0 params, 2 slots, 1 upvalue, 0 locals, 2 constants, 0 functions
	1	[3]	SETTABUP 	0 -1 -2	; t "key" "value"
	2	[4]	RETURN   	0 1
constants (2) for 0x1e44600:
	1	"key"
	2	"value"
locals (0) for 0x1e44600:
upvalues (1) for 0x1e44600:
	0	t	1	0
]]

--[[
rb = K(0);
rc = K(1);
ci->u.l.savedpc = &cl->p->code[1];
Y_luaV_settable(L, cl->upvals[0]->v, rb, rc);
base = ci->u.l.base;
]]