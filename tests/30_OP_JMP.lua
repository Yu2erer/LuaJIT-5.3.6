function test()
    local x
    goto label
    x = "error"
::label::
    x = "goto"
    return x
end

jit("compile", test)
local res = test()
assert(res == "goto")

--[[
function <../tests/30_OP_JMP.lua:1,8> (6 instructions at 0x2684500)
0 params, 2 slots, 0 upvalues, 1 local, 2 constants, 0 functions
	1	[2]	LOADNIL  	0 0
	2	[2]	JMP      	0 1	; to 4
	3	[4]	LOADK    	0 -1	; "error"
	4	[6]	LOADK    	0 -2	; "goto"
	5	[7]	RETURN   	0 2
	6	[8]	RETURN   	0 1
constants (2) for 0x2684500:
	1	"error"
	2	"goto"
locals (1) for 0x2684500:
	0	x	2	7
upvalues (0) for 0x2684500:
]]

--[[
goto Label_3;
]]