t = {}
function test(x)
	table.insert(t, "testcall")
    if x ~= 0 then
        test(0)
    end
end

jit("compile", test)
test(1)
assert(#t == 2)

--[[
function <../tests/36_OP_CALL.lua:2,7> (11 instructions at 0x185d690)
1 param, 4 slots, 1 upvalue, 1 local, 6 constants, 0 functions
	1	[3]	GETTABUP 	1 0 -1	; _ENV "table"
	2	[3]	GETTABLE 	1 1 -2	; "insert"
	3	[3]	GETTABUP 	2 0 -3	; _ENV "t"
	4	[3]	LOADK    	3 -4	; "testcall"
	5	[3]	CALL     	1 3 1
	6	[4]	EQ       	1 0 -5	; - 0
	7	[4]	JMP      	0 3	; to 11
	8	[5]	GETTABUP 	1 0 -6	; _ENV "test"
	9	[5]	LOADK    	2 -5	; 0
	10	[5]	CALL     	1 2 1
	11	[7]	RETURN   	0 1
constants (6) for 0x185d690:
	1	"table"
	2	"insert"
	3	"t"
	4	"testcall"
	5	0
	6	"test"
locals (1) for 0x185d690:
	0	x	1	12
upvalues (1) for 0x185d690:
	0	_ENV	0	0
]]

--[[
ra = R(1);
L->top = R(3);
ci->u.l.savedpc = &cl->p->code[10];
result = luaD_precall(L, ra, 0, 1);
if (result) {
  if (result == 1 && 0 >= 0) {
      L->top = ci->top;
  }
} else {
  result = luaV_execute(L);
  if (result) L->top = ci->top;
}
base = ci->u.l.base;
]]