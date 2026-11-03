-- firefly.lua: a firefly that drifts in loops around where it starts.
-- This is a Lua script. Every firefly in the grove has a Script component
-- that runs this file. Change the numbers, save, and press Play again.
-- Help > Scripting Guide lists everything a script can use.

-- start runs once for each firefly. self is that firefly's own table:
-- self.object is the firefly, and anything else you put in self stays there.
function start(self)
    self.homeX, self.homeY = self.object:position()
    self.time = math.random() * 10   -- so the fireflies are not all in step
end

-- update runs every frame. dt is the time since the last frame, in seconds.
function update(self, dt)
    self.time = self.time + dt
    local x = self.homeX + math.cos(self.time * 1.3) * 40
    local y = self.homeY + math.sin(self.time * 2.1) * 20
    self.object:setPosition(x, y)
end
