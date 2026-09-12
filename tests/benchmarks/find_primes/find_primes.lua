
local n = 10000

local function is_prime(n)
    if n == 0 or n == 1 then
        return false
    end
    for i = 2, n - 1 do
        if n % i == 0 then
            return false
        end
    end
    return true
end

for i = 0, n - 1 do
    if is_prime(i) then
        -- print(i)
    end
end
