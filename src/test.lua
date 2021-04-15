

t = {}
for i=1,10000000 do
    t[i] = {"1231413413"}
end

print(collectgarbage("count"))
collectgarbage("collect")
