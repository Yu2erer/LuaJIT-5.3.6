local o = {}
function o:f()
end
function test()
    o:f()
end

jit("compile", test)
test()

--[[
function <../tests/12_OP_SELF.lua:4,6> (4 instructions at 0xb817d0)
0 params, 2 slots, 1 upvalue, 0 locals, 1 constant, 0 functions
	1	[5]	GETUPVAL 	0 0	; o
	2	[5]	SELF     	0 0 -1	; "f"
	3	[5]	CALL     	0 2 1
	4	[6]	RETURN   	0 1
constants (1) for 0xb817d0:
	1	"f"
locals (0) for 0xb817d0:
upvalues (1) for 0xb817d0:
	0	o	1	0
]]

--[[
ra = R(0);
rb = R(0);
rc = K(0);
setobj2s(L, ra + 1, rb);
ci->u.l.savedpc = &cl->p->code[2];
Y_luaV_gettable(L, rb, rc, ra);
base = ci->u.l.base;
]]