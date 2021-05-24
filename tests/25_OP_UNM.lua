function test()
    local a = 11.31313
    return -a
end

jit("compile", test)
local res = test()
assert(res == -11.31313)

--[[
function <../tests/25_OP_UNM.lua:1,4> (4 instructions at 0x1904590)
0 params, 2 slots, 0 upvalues, 1 local, 1 constant, 0 functions
	1	[2]	LOADK    	0 -1	; 11.31313
	2	[3]	UNM      	1 0
	3	[3]	RETURN   	1 2
	4	[4]	RETURN   	0 1
constants (1) for 0x1904590:
	1	11.31313
locals (1) for 0x1904590:
	0	a	2	5
upvalues (0) for 0x1904590:
]]

--[[
ra = R(1);
rb = R(0);
if (ttisinteger(rb)) {
  ib = ivalue(rb);
  setivalue(ra, intop(-, 0, ib));
} else if (tonumber(rb, &nb)) {
  setfltvalue(ra, luai_numunm(L, nb));
} else {
ci->u.l.savedpc = &cl->p->code[2];
  luaT_trybinTM(L, rb, rb, ra, TM_UNM);
base = ci->u.l.base;
}
]]