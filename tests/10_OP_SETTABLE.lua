a = {}
function test()
    a[1] = 10
end

jit("compile", test)
test()
assert(a[1] == 10)

--[[
function <../tests/10_OP_SETTABLE.lua:2,4> (3 instructions at 0x13b15e0)
0 params, 2 slots, 1 upvalue, 0 locals, 3 constants, 0 functions
	1	[3]	GETTABUP 	0 0 -1	; _ENV "a"
	2	[3]	SETTABLE 	0 -2 -3	; 1 10
	3	[4]	RETURN   	0 1
constants (3) for 0x13b15e0:
	1	"a"
	2	1
	3	10
locals (0) for 0x13b15e0:
upvalues (1) for 0x13b15e0:
	0	_ENV	0	0
]]

--[[
ra = R(0);
rb = K(1);
rc = K(2);
ci->u.l.savedpc = &cl->p->code[2];
Y_luaV_settable(L, ra, rb, rc);
base = ci->u.l.base;
]]