-- Sends two datagrams, then counts what comes back without ever answering, and reports the count when it ends.
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
			state.count = 0

			return { send = { "one", "two" } }
		end,

		receive = function(state, datagram)
			state.count = state.count + 1
		end,

		finish = function(state)
			return { reply = { rules = { { key = "count", value = tostring(state.count) } }, players = {} } }
		end,
	},
}
