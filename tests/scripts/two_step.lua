-- A server that first hands out a token, and answers only a status request carrying it.
return {
	api = 1,

	master = {
		transport = "udp",

		start = function(options, state)
			return { send = { "list" } }
		end,

		receive = function(state, datagram)
			return { quiet = 100 }
		end,
	},

	server = {
		start = function(options, state)
			return { send = { "token?" } }
		end,

		receive = function(state, datagram)
			if state.token == nil then
				state.token = datagram

				return { send = { "status " .. datagram } }
			end

			return { reply = { rules = { { key = "hostname", value = datagram } }, players = {} } }
		end,
	},
}
