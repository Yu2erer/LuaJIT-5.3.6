function test(x)
    print("test", "call")
    if x ~= 0 then
        test(0)
    end
end

jit("compile", test)
test(1)

--[[
function <../tests/36_OP_CALL.lua:1,6> (10 instructions at 0x1bd6500)
1 param, 4 slots, 1 upvalue, 1 local, 4 constants, 0 functions
	1	[2]	GETTABUP 	1 0 -1	; _ENV "print"
	2	[2]	LOADK    	2 -2	; "test"
	3	[2]	LOADK    	3 -3	; "call"
	4	[2]	CALL     	1 3 1
	5	[3]	EQ       	1 0 -4	; - 0
	6	[3]	JMP      	0 3	; to 10
	7	[4]	GETTABUP 	1 0 -2	; _ENV "test"
	8	[4]	LOADK    	2 -4	; 0
	9	[4]	CALL     	1 2 1
	10	[6]	RETURN   	0 1
constants (4) for 0x1bd6500:
	1	"print"
	2	"test"
	3	"call"
	4	0
locals (1) for 0x1bd6500:
	0	x	1	11
upvalues (1) for 0x1bd6500:
	0	_ENV	0	0
]]

--[[
ra = R(1);
L->top = R(4);
ci->u.l.savedpc = &cl->p->code[4];
result = luaD_precall(L, ra, 0);
if (result) {
  if (result == 1 && 0 >= 0) {
      L->top = ci->top;
  }
} else {
  luaV_execute(L);
}
base = ci->u.l.base;
]]