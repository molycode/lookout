-- A master over TCP that greets first and then sends seven-byte frames: S and an address lists a server, N is
-- noise, E ends the list. It tries the stream plumbing without any real game's framing.
local Greeting = "HELLO"
local Request = "LIST"
local FrameSize = 7
local ServerFrame = 0x53
local EndFrame = 0x45
local NoiseFrame = 0x4E
local QuietMs = 500

return {
	api = 1,

	master = {
		transport = "tcp",

		start = function(options, state)
			state.buffer = ""
			state.isGreeted = false
		end,

		receive = function(state, data)
			if data == "" then
				return (state.buffer == "") and { done = true } or { reason = "truncated" }
			end

			state.buffer = state.buffer .. data

			if not state.isGreeted then
				if #state.buffer < #Greeting then
					return nil
				end

				assert(string.sub(state.buffer, 1, #Greeting) == Greeting, "the master did not greet")
				state.isGreeted = true
				state.buffer = string.sub(state.buffer, #Greeting + 1)

				return { send = { Request } }
			end

			local servers = {}
			local isDone = false
			local position = 1

			while not isDone and #state.buffer - position + 1 >= FrameSize do
				local kind = string.byte(state.buffer, position)

				if kind == ServerFrame then
					local ip, port = string.unpack(">I4I2", state.buffer, position + 1)

					servers[#servers + 1] = { ip = ip, port = port }
				elseif kind == EndFrame then
					isDone = true
				else
					assert(kind == NoiseFrame, "an unknown frame")
				end

				position = position + FrameSize
			end

			state.buffer = string.sub(state.buffer, position)

			if isDone then
				return { servers = servers, done = true }
			end

			return { servers = servers, quiet = QuietMs }
		end,
	},

	server = {
		start = function(options, state)
			return { send = { "status" } }
		end,

		receive = function(state, datagram)
			return { reply = { rules = { { key = "hostname", value = datagram } }, players = {} } }
		end,
	},
}
