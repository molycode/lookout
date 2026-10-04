-- Valve's server queries (A2S_INFO, A2S_PLAYER, A2S_RULES) and the Steam master's paged list.

local Single = "\xFF\xFF\xFF\xFF"
local Split = "\xFE\xFF\xFF\xFF"
local NoChallenge = "\xFF\xFF\xFF\xFF"
local InfoRequest = Single .. "TSource Engine Query\0"
local PlayerRequest = Single .. "U"
local RulesRequest = Single .. "V"
local ChallengeKind = 0x41
local InfoKind = 0x49
local PlayersKind = 0x44
local RulesKind = 0x45
local SplitHeaderSize = 12
local CompressedSplit = 0x80000000
local MasterReplyHeader = Single .. "f\n"
local MasterEntrySize = 6
local FirstSeed = "0.0.0.0:0"
local AllRegions = "\xFF"
local MaxDuration = 1e9

-- Each answers the query its stage sent; anything else is a stray reply to an earlier one.
local Expected = { info = InfoKind, players = PlayersKind, rules = RulesKind }

local function startsWith(text, prefix)
	return string.sub(text, 1, #prefix) == prefix
end

-- Reads in order; past the end it marks itself truncated and every later read gives nil.
local function newReader(data, position)
	return { data = data, position = position, isTruncated = false }
end

local function take(reader, format, size)
	if reader.isTruncated or reader.position + size - 1 > #reader.data then
		reader.isTruncated = true

		return nil
	end

	local value = string.unpack(format, reader.data, reader.position)

	reader.position = reader.position + size

	return value
end

local function takeString(reader)
	local nul = (not reader.isTruncated) and string.find(reader.data, "\0", reader.position) or nil

	if nul == nil then
		reader.isTruncated = true

		return nil
	end

	local text = string.sub(reader.data, reader.position, nul - 1)

	reader.position = nul + 1

	return text
end

local function addRule(rules, key, value)
	rules[#rules + 1] = { key = key, value = tostring(value) }
end

local function readInfo(message, state)
	local reader = newReader(message, 6)

	take(reader, "B", 1)

	local name = takeString(reader)
	local map = takeString(reader)
	local folder = takeString(reader)
	local game = takeString(reader)

	take(reader, "<I2", 2)

	local players = take(reader, "B", 1)
	local maxPlayers = take(reader, "B", 1)
	local bots = take(reader, "B", 1)

	take(reader, "B", 1)
	take(reader, "B", 1)

	local visibility = take(reader, "B", 1)
	local vac = take(reader, "B", 1)
	local version = takeString(reader)
	local keywords = nil

	-- The extra data flags say which optional fields follow, in this order.
	if not reader.isTruncated and reader.position <= #message then
		local flags = take(reader, "B", 1)

		if flags & 0x80 ~= 0 then
			local port = take(reader, "<I2", 2)

			state.joinPort = (port ~= nil and port ~= 0) and port or nil
		end

		if flags & 0x10 ~= 0 then
			take(reader, "<i8", 8)
		end

		if flags & 0x40 ~= 0 then
			take(reader, "<I2", 2)
			takeString(reader)
		end

		if flags & 0x20 ~= 0 then
			keywords = takeString(reader)
		end

		if flags & 0x01 ~= 0 then
			take(reader, "<i8", 8)
		end
	end

	if reader.isTruncated then
		return false
	end

	addRule(state.rules, "hostname", name)
	addRule(state.rules, "map", map)
	addRule(state.rules, "folder", folder)
	addRule(state.rules, "game", game)
	addRule(state.rules, "players", players)
	addRule(state.rules, "maxplayers", maxPlayers)
	addRule(state.rules, "bots", bots)
	addRule(state.rules, "password", visibility)
	addRule(state.rules, "secure", vac)
	addRule(state.rules, "version", version)

	if keywords ~= nil then
		addRule(state.rules, "keywords", keywords)
	end

	state.hasInfo = true

	return true
end

local function readPlayers(message, state)
	local reader = newReader(message, 6)
	local count = take(reader, "B", 1) or 0

	for _ = 1, count do
		take(reader, "B", 1)

		local name = takeString(reader)
		local score = take(reader, "<i4", 4)
		local duration = take(reader, "<f", 4)

		if not reader.isTruncated then
			local seconds = (duration == duration and duration >= 0 and duration < MaxDuration) and math.floor(duration) or 0

			state.players[#state.players + 1] = { name = name, score = score, fields = { { key = "time", value = string.format("%d", seconds) } } }
		end
	end

	return not reader.isTruncated
end

-- Some servers end the list short of its count; the rules that did arrive still count.
local function readRules(message, state)
	local reader = newReader(message, 6)
	local count = take(reader, "<I2", 2) or 0

	for _ = 1, count do
		local key = takeString(reader)
		local value = takeString(reader)

		if not reader.isTruncated then
			addRule(state.rules, key, value)
		end
	end
end

-- The fragments of one split reply, in any order; the whole message once every one is in.
local function gather(state, datagram)
	if #datagram < SplitHeaderSize then
		return nil, "truncated"
	end

	local id, total, number = string.unpack("<I4BB", datagram, 5)

	if id & CompressedSplit ~= 0 or total == 0 or number >= total then
		return nil, "malformed"
	end

	if state.splitId ~= id then
		state.splitId = id
		state.fragments = {}
		state.numFragments = 0
	end

	if state.fragments[number] == nil then
		state.fragments[number] = string.sub(datagram, SplitHeaderSize + 1)
		state.numFragments = state.numFragments + 1
	end

	if state.numFragments < total then
		return nil
	end

	local parts = {}

	for index = 0, total - 1 do
		parts[#parts + 1] = state.fragments[index]
	end

	state.splitId = nil

	return table.concat(parts)
end

local function ask(state, stage, request)
	state.stage = stage
	state.request = request

	return { send = { (stage == "info") and request or (request .. (state.challenge or NoChallenge)) } }
end

local function makeReply(state)
	return { reply = { rules = state.rules, players = state.players, joinPort = state.joinPort } }
end

local function pageRequest(seed, filter)
	return "1" .. AllRegions .. seed .. "\0" .. filter .. "\0"
end

local function formatAddress(ip, port)
	return string.format("%d.%d.%d.%d:%d", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port)
end

return {
	api = 1,

	options = {
		masterFilter = { required = true, description = "What the Steam master is asked to list, such as \\appid\\440" },
	},

	master = {
		transport = "udp",

		start = function(options, state)
			state.filter = options.masterFilter
			state.seed = FirstSeed

			return { send = { pageRequest(state.seed, state.filter) } }
		end,

		-- Each page names the next by its last address; 0.0.0.0:0 ends the list. A broken page is asked for again.
		receive = function(state, datagram)
			if not startsWith(datagram, MasterReplyHeader) or (#datagram - #MasterReplyHeader) % MasterEntrySize ~= 0 then
				return { reason = startsWith(datagram, MasterReplyHeader) and "truncated" or "wrongHeader", send = { pageRequest(state.seed, state.filter) } }
			end

			local servers = {}
			local isDone = false

			for position = #MasterReplyHeader + 1, #datagram, MasterEntrySize do
				local ip, port = string.unpack(">I4I2", datagram, position)

				if ip == 0 and port == 0 then
					isDone = true
				else
					servers[#servers + 1] = { ip = ip, port = port }
					state.seed = formatAddress(ip, port)
				end
			end

			if isDone then
				return { servers = servers, done = true }
			end

			return { servers = servers, send = { pageRequest(state.seed, state.filter) } }
		end,
	},

	server = {
		start = function(options, state)
			state.rules = {}
			state.players = {}

			return ask(state, "info", InfoRequest)
		end,

		receive = function(state, datagram)
			local message, reason = nil, nil

			if startsWith(datagram, Split) then
				message, reason = gather(state, datagram)
			elseif startsWith(datagram, Single) and #datagram >= 5 then
				message = datagram
			else
				reason = "wrongHeader"
			end

			if reason ~= nil then
				return { reason = reason }
			end

			local kind = (message ~= nil) and string.byte(message, 5) or nil

			if kind == ChallengeKind and #message >= 9 then
				state.challenge = string.sub(message, 6, 9)

				return { send = { state.request .. state.challenge } }
			end

			if kind == nil or kind ~= Expected[state.stage] then
				return nil
			end

			if kind == InfoKind then
				return readInfo(message, state) and ask(state, "players", PlayerRequest) or { reason = "truncated" }
			end

			if kind == PlayersKind then
				return readPlayers(message, state) and ask(state, "rules", RulesRequest) or { reason = "truncated" }
			end

			readRules(message, state)

			return makeReply(state)
		end,

		-- Rules some servers never send; what came before them is still the server's reply.
		finish = function(state)
			return state.hasInfo and makeReply(state) or nil
		end,
	},
}
