local M = {}

function M.init()
    local node = {}
    setmetatable(node,{
        __call = function(self, ...)
            return self:run(...)
        end,
    })
    return node
end

return M