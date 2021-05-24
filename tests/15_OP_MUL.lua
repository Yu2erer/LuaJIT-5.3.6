function test()
    local a = 38324.0
    local b = "14159"
    return a * b
end

jit("compile", test)
local res = test()
assert(res == 542629516)

--[[
function <../tests/15_OP_MUL.lua:1,5> (5 instructions at 0x1a88590)
0 params, 3 slots, 0 upvalues, 2 locals, 2 constants, 0 functions
	1	[2]	LOADK    	0 -1	; 38324
	2	[3]	LOADK    	1 -2	; "14159"
	3	[4]	MUL      	2 0 1
	4	[4]	RETURN   	2 2
	5	[5]	RETURN   	0 1
constants (2) for 0x1a88590:
	1	38324
	2	"14159"
locals (2) for 0x1a88590:
	0	a	2	6
	1	b	3	6
upvalues (0) for 0x1a88590:
]]

--[[
ra = R(2);
rb = R(0);
rc = R(1);
if (ttisinteger(rb) && ttisinteger(rc)) {
  ib = ivalue(rb); ic = ivalue(rc);
  setivalue(ra, intop(*, ib, ic));
} else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
  setfltvalue(ra, luai_nummul(L, nb, nc));
} else {
ci->u.l.savedpc = &cl->p->code[3];
  luaT_trybinTM(L, rb, rc, ra, TM_MUL);
base = ci->u.l.base;
}
]]