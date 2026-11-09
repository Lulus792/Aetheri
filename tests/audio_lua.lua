-- Aetheri regression inputs for the four pinned upstream Lua 5.5.1 fixes.
-- MIT for this test; upstream runtime and patches retain their own notices.
local selected = assert(arg[1])
assert(_VERSION == 'Lua 5.5')
if selected == 'nan' then
  local nan = 0/0
  for _, start in ipairs({1,1.0}) do
    for _, step in ipairs({-1,-1.0,1,1.0}) do
      for _ = start,nan,step do error('NaN limit executed loop') end
    end
  end
elseif selected == 'zero' then
  local mz = -0.0
  local variable = 0
  assert(1/(mz-0) == -math.huge)
  assert(1/(mz-0) == 1/(mz-variable))
elseif selected == 'close' then
  local wrapped = coroutine.wrap(function()
    local value <close> = setmetatable({}, {__close=function()
      coroutine.close(coroutine.running())
    end})
    error('original failure')
  end)
  assert(select('#',wrapped()) == 0)
elseif selected == 'gc' then
  local lim = 1000000
  collectgarbage('param','stepmul',0)
  collectgarbage('param','majorminor',2000)
  collectgarbage();collectgarbage()
  local before = collectgarbage('count')
  local values = {}
  for i=1,lim do values[i]=true end
  assert(collectgarbage('count') > before+(lim*string.packsize('j'))/1024)
  collectgarbage('step');collectgarbage('step');collectgarbage('step')
  for i=1,lim do values[i]=nil end
  values[2*lim]=true
  assert(collectgarbage('count') < before*5/4)
  collectgarbage('step')
else
  error('unknown regression')
end
print('LUA_REGRESSION_PASS '..selected)
