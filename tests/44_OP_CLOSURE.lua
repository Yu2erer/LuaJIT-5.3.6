function test()
    local function inner(x)
        return x + 1
    end
    return inner(10)
end

jit("compile", test)
local res = test()
assert(res == 11)

--[[
function <../tests/44_OP_CLOSURE.lua:1,6> (6 instructions at 0x202a500)
0 params, 3 slots, 0 upvalues, 1 local, 1 constant, 1 function
	1	[4]	CLOSURE  	0 0	; 0x202a680
	2	[5]	MOVE     	1 0
	3	[5]	LOADK    	2 -1	; 10
	4	[5]	TAILCALL 	1 2 0
	5	[5]	RETURN   	1 0
	6	[6]	RETURN   	0 1
constants (1) for 0x202a500:
	1	10
locals (1) for 0x202a500:
	0	inner	2	7
upvalues (0) for 0x202a500:
]]

--[[
Y_luaV_closure(L, ci, cl, 0, 0);
]]