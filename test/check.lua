-- Z39.50

local CB, t, qi = {}, {}, QI

-- initResponse
local function initRp(pdu)
    t = {
     [2] = "\224",
     [3] = "\233\162",
     [4] = 64536,
     [5] = 64536,
     [6] = 1,
     [8] = "Z3950/LuaBER",
     [9] = "0.1"
    }
    return 2
end

-- searchResponse
local function searchRp(pdu)
    t = {
     [2] = 0,
     [3] = 0,
     [4] = 0,
     [5] = 0
    }
    return 4
end

CB = {[1] = initRp, [3] = searchRp}
local k, v = next(qi[1])
--QO = QI
QO = {[1] = {}}; if CB[k] then QO[1][CB[k](v)] = t end
