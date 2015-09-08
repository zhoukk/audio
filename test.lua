local audio = require "audio"

local bid = audio.load(...)
local sid = audio.play(bid)

while audio.playing(sid) do

end
audio.stop(sid)
audio.unload(bid)