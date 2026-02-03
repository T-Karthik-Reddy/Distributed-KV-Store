init = function(args)
  local r = {}
  local depth = 32
  for i=1,depth do
    r[i] = wrk.format("GET", "/mykey")
  end
  req = table.concat(r)
end

request = function()
  return req
end
