#include "http_response_reader.hpp"
#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <utility>

namespace Lkt::Net
{
namespace
{
constexpr std::string_view HeaderEnd{ "\r\n\r\n" };
constexpr std::string_view LineEnd{ "\r\n" };
constexpr size_t MaxHeaderSize{ 16 * 1024 };
constexpr unsigned OkStatus{ 200 };
constexpr unsigned FirstRedirect{ 300 };
constexpr unsigned FirstClientError{ 400 };

//////////////////////////////////////////////////////////////////////////
bool EqualsIgnoringCase(std::string_view lhs, std::string_view rhs)
{
	auto const lower{ [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; } };

	return lhs.size() == rhs.size() && std::ranges::equal(lhs, rhs, {}, lower, lower);
}

//////////////////////////////////////////////////////////////////////////
std::string_view Trim(std::string_view text)
{
	size_t const first{ text.find_first_not_of(" \t") };
	size_t const last{ text.find_last_not_of(" \t") };

	return (first == std::string_view::npos) ? std::string_view{} : text.substr(first, last - first + 1);
}

//////////////////////////////////////////////////////////////////////////
std::optional<size_t> ParseSize(std::string_view text, int base = 10)
{
	size_t value{ 0 };
	std::from_chars_result const result{ std::from_chars(text.data(), text.data() + text.size(), value, base) };
	bool const isValid{ !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size() };

	return isValid ? std::optional<size_t>{ value } : std::nullopt;
}
} // namespace

//////////////////////////////////////////////////////////////////////////
void CHttpResponseReader::Reset(size_t maxBodySize)
{
	m_headers.clear();
	m_chunked.clear();
	m_body.clear();
	m_error.clear();
	m_maxBodySize = maxBodySize;
	m_bodySize = 0;
	m_state = EHttpReadState::Headers;
	m_isChunked = false;
	m_isKeptAlive = true;
}

//////////////////////////////////////////////////////////////////////////
EHttpReadState CHttpResponseReader::Read(std::span<std::byte const> data)
{
	std::string_view text{ reinterpret_cast<char const*>(data.data()), data.size() };
	std::string rest{};

	if (m_state == EHttpReadState::Headers)
	{
		size_t const searchFrom{ (m_headers.size() >= HeaderEnd.size()) ? m_headers.size() - HeaderEnd.size() + 1 : 0 };

		m_headers.append(text);

		size_t const end{ m_headers.find(HeaderEnd, searchFrom) };

		if (end != std::string::npos)
		{
			rest.assign(std::string_view{ m_headers }.substr(end + HeaderEnd.size()));
			m_headers.resize(end);
			ReadHeaders(m_headers);
			text = rest;
		}
		else if (m_headers.size() > MaxHeaderSize)
		{
			Fail(std::format("sent more than {} KiB of headers", MaxHeaderSize / 1024));
		}
	}

	if (m_state == EHttpReadState::Body && m_isChunked)
	{
		m_chunked.append(text);
		ReadChunks();
	}
	else if (m_state == EHttpReadState::Body)
	{
		m_body.append(text);

		if (m_body.size() > m_bodySize)
		{
			Fail(std::format("sent {} bytes after the {} it announced", m_body.size() - m_bodySize, m_bodySize));
		}
		else if (m_body.size() == m_bodySize)
		{
			m_state = EHttpReadState::Complete;
		}
	}

	return m_state;
}

//////////////////////////////////////////////////////////////////////////
std::string CHttpResponseReader::TakeBody()
{
	return std::move(m_body);
}

//////////////////////////////////////////////////////////////////////////
std::string const& CHttpResponseReader::GetError() const
{
	return m_error;
}

//////////////////////////////////////////////////////////////////////////
bool CHttpResponseReader::IsKeptAlive() const
{
	return m_isKeptAlive;
}

//////////////////////////////////////////////////////////////////////////
void CHttpResponseReader::ReadHeaders(std::string_view headers)
{
	size_t const statusEnd{ std::min(headers.find(LineEnd), headers.size()) };
	std::string_view const statusLine{ headers.substr(0, statusEnd) };
	std::string_view const version{ statusLine.substr(0, statusLine.find(' ')) };
	std::string_view const afterVersion{ (version.size() < statusLine.size()) ? statusLine.substr(version.size() + 1) : std::string_view{} };
	std::optional<size_t> const status{ ParseSize(afterVersion.substr(0, afterVersion.find(' '))) };
	std::optional<size_t> contentLength{};
	std::string_view location{};
	std::string_view encoding{};
	bool isChunked{ false };

	m_isKeptAlive = version == "HTTP/1.1";

	for (size_t at{ statusEnd + LineEnd.size() }; at < headers.size();)
	{
		size_t const lineEnd{ std::min(headers.find(LineEnd, at), headers.size()) };
		std::string_view const line{ headers.substr(at, lineEnd - at) };
		size_t const colon{ line.find(':') };
		std::string_view const name{ line.substr(0, std::min(colon, line.size())) };
		std::string_view const value{ (colon != std::string_view::npos) ? Trim(line.substr(colon + 1)) : std::string_view{} };

		if (EqualsIgnoringCase(name, "content-length"))
		{
			contentLength = ParseSize(value);
		}
		else if (EqualsIgnoringCase(name, "transfer-encoding") && EqualsIgnoringCase(value, "chunked"))
		{
			isChunked = true;
		}
		else if (EqualsIgnoringCase(name, "transfer-encoding") ||(EqualsIgnoringCase(name, "content-encoding") && !EqualsIgnoringCase(value, "identity")))
		{
			encoding = value;
		}
		else if (EqualsIgnoringCase(name, "connection"))
		{
			m_isKeptAlive = m_isKeptAlive && !EqualsIgnoringCase(value, "close");
		}
		else if (EqualsIgnoringCase(name, "location"))
		{
			location = value;
		}

		at = lineEnd + LineEnd.size();
	}

	if (!version.starts_with("HTTP/1.") || !status.has_value())
	{
		Fail("did not answer in HTTP/1.1");
	}
	else if (*status >= FirstRedirect && *status < FirstClientError)
	{
		Fail(std::format("redirects to {}, which Lookout does not follow", location.empty() ? std::string_view{ "another address" } : location));
	}
	else if (*status != OkStatus)
	{
		Fail(std::format("answered {}", Trim(afterVersion)));
	}
	else if (!encoding.empty())
	{
		Fail(std::format("sent the reply {}-encoded, which Lookout does not read", encoding));
	}
	else if (isChunked)
	{
		m_isChunked = true;
		m_state = EHttpReadState::Body;
	}
	else if (!contentLength.has_value())
	{
		Fail("did not say how long its reply is");
	}
	else if (*contentLength > m_maxBodySize)
	{
		Fail(std::format("sent {} bytes, more than the {} the file may have", *contentLength, m_maxBodySize));
	}
	else
	{
		m_bodySize = *contentLength;
		m_state = EHttpReadState::Body;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHttpResponseReader::ReadChunks()
{
	size_t at{ 0 };
	bool isWaiting{ false };

	while (m_state == EHttpReadState::Body && !isWaiting)
	{
		std::string_view const pending{ std::string_view{ m_chunked }.substr(at) };
		size_t const lineEnd{ pending.find(LineEnd) };
		std::string_view const sizeLine{ pending.substr(0, std::min(lineEnd, pending.size())) };
		std::optional<size_t> const size{ ParseSize(Trim(sizeLine.substr(0, sizeLine.find(';'))), 16) };
		size_t const dataStart{ lineEnd + LineEnd.size() };

		if (lineEnd == std::string_view::npos && pending.size() > MaxHeaderSize)
		{
			Fail(std::format("sent a chunk line longer than {} KiB", MaxHeaderSize / 1024));
		}
		else if (lineEnd == std::string_view::npos)
		{
			isWaiting = true;
		}
		else if (!size.has_value())
		{
			Fail("sent a chunk size Lookout cannot read");
		}
		else if (*size > m_maxBodySize - m_body.size())
		{
			Fail(std::format("sent more than the {} bytes the file may have", m_maxBodySize));
		}
		else if (*size == 0)
		{
			// Searching from the size line's own line end finds an empty trailer too.
			size_t const end{ pending.find(HeaderEnd, lineEnd) };

			if (end == std::string_view::npos && pending.size() > MaxHeaderSize)
			{
				Fail(std::format("sent more than {} KiB of trailer", MaxHeaderSize / 1024));
			}
			else if (end == std::string_view::npos)
			{
				isWaiting = true;
			}
			else if (end + HeaderEnd.size() < pending.size())
			{
				Fail(std::format("sent {} bytes after its last chunk", pending.size() - end - HeaderEnd.size()));
			}
			else
			{
				at += end + HeaderEnd.size();
				m_state = EHttpReadState::Complete;
			}
		}
		else if (pending.size() < dataStart + *size + LineEnd.size())
		{
			isWaiting = true;
		}
		else if (pending.substr(dataStart + *size, LineEnd.size()) != LineEnd)
		{
			Fail("sent a chunk longer than its size");
		}
		else
		{
			m_body.append(pending.substr(dataStart, *size));
			at += dataStart + *size + LineEnd.size();
		}
	}

	m_chunked.erase(0, at);
}

//////////////////////////////////////////////////////////////////////////
void CHttpResponseReader::Fail(std::string error)
{
	m_error = std::move(error);
	m_state = EHttpReadState::Failed;
	m_isKeptAlive = false;
}
} // namespace Lkt::Net
