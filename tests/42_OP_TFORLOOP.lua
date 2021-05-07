t = {"abc", 131134.1, 520, {}}
function test()
    for k, v in pairs(t) do
    end
end

jit("compile", test)
test()

--[[
function <../tests/42_OP_TFORLOOP.lua:2,5> (7 instructions at 0xd007b0)
0 params, 6 slots, 1 upvalue, 5 locals, 2 constants, 0 functions
	1	[3]	GETTABUP 	0 0 -1	; _ENV "pairs"
	2	[3]	GETTABUP 	1 0 -2	; _ENV "t"
	3	[3]	CALL     	0 2 4
	4	[3]	JMP      	0 0	; to 5
	5	[3]	TFORCALL 	0 2
	6	[3]	TFORLOOP 	2 -2	; to 5
	7	[5]	RETURN   	0 1
constants (2) for 0xd007b0:
	1	"pairs"
	2	"t"
locals (5) for 0xd007b0:
	0	(for generator)	4	7
	1	(for state)	4	7
	2	(for control)	4	7
	3	k	5	5
	4	v	5	5
upvalues (1) for 0xd007b0:
	0	_ENV	0	0
]]

--[[
ra = R(0);
rb = ra + 5;
rc = ra + 2;
setobj2s(L, rb, rc);
rb = ra + 4;
rc = ra + 1;
setobj2s(L, rb, rc);
rb = ra + 3;
setobj2s(L, rb, ra);
L->top = rb + 3;
luaD_call(L, rb, 2);
base = ci->u.l.base;
L->top = ci->top;
ra = R(2);
if (!ttisnil(ra + 1)) {
  setobj2s(L, ra, ra + 1);
goto Label_4;
}
]]