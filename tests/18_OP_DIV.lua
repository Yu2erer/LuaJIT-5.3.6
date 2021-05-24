function test()
    local a, b = 13.5, 5.3
    return a / b
end

jit("compile", test)
local res = test()
assert(res == 13.5 / 5.3)

--[[
function <../tests/18_OP_DIV.lua:1,4> (5 instructions at 0x739590)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 13.5
	2	[2]	LOADK    	1 -2	; 5.3
	3	[3]	DIV      	2 0 1
	4	[3]	RETURN   	2 2
	5	[4]	RETURN   	0 1
constants (2) for 0x739590:
	1	13.5
	2	5.3
locals (2) for 0x739590:
	0	a	3	6
	1	b	3	6
upvalues (0) for 0x739590:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
  setfltvalue(ra, luai_numdiv(L, nb, nc));
} else {
ci->u.l.savedpc = &cl->p->code[3];
  luaT_trybinTM(L, rb, rc, ra, TM_DIV);
base = ci->u.l.base;
}
]]