#pragma once

#include "http_read_state.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Net
{
// One HTTP/1.1 response, read as it arrives. Only what raw.githubusercontent.com sends is accepted: a 200 with a
// Content-Length and no transfer or content encoding. Redirects are never followed, so Lookout talks to one host only.
class CHttpResponseReader final
{
public:

	void Reset(size_t maxBodySize);
	EHttpReadState Read(std::span<std::byte const> data);

	std::string TakeBody();
	std::string const& GetError() const;
	// False when the server closes the connection after this response.
	bool IsKeptAlive() const;

private:

	void ReadHeaders(std::string_view headers);
	void Fail(std::string error);

	std::string m_headers;
	std::string m_body;
	std::string m_error;
	size_t m_maxBodySize{ 0 };
	size_t m_bodySize{ 0 };
	EHttpReadState m_state{ EHttpReadState::Headers };
	bool m_isKeptAlive{ true };
};
} // namespace Lkt::Net
