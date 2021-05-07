local a
function test()
    a = 10
end

jit("compile", test)
test()
assert(a == 10)

--[[
function <../tests/9_OP_SETUPVAL.lua:2,4> (3 instructions at 0x9f2600)
0 params, 2 slots, 1 upvalue, 0 locals, 1 constant, 0 functions
	1	[3]	LOADK    	0 -1	; 10
	2	[3]	SETUPVAL 	0 0	; a
	3	[4]	RETURN   	0 1
constants (1) for 0x9f2600:
	1	10
locals (0) for 0x9f2600:
upvalues (1) for 0x9f2600:
	0	a	1	0
]]

--[[
ra = R(0);
ci->u.l.savedpc = &cl->p->code[2];
Y_luaV_setupval(L, cl, ra, 0);
]]