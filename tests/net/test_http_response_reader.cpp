#include "http_response_reader.hpp"
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>

namespace Lkt::Net
{
namespace
{
constexpr size_t MaxSize{ 64 };
constexpr std::string_view ChunkedHeaders{ "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" };

//////////////////////////////////////////////////////////////////////////
EHttpReadState Feed(CHttpResponseReader& reader, std::string_view text)
{
	return reader.Read(std::as_bytes(std::span{ text }));
}

//////////////////////////////////////////////////////////////////////////
std::string ReadReply(std::string_view reply)
{
	CHttpResponseReader reader{};

	reader.Reset(MaxSize);

	EHttpReadState const state{ Feed(reader, reply) };

	return (state == EHttpReadState::Complete) ? reader.TakeBody() : std::string{ "error: " } + reader.GetError();
}

//////////////////////////////////////////////////////////////////////////
TEST(HttpResponseReader, ChunksFedByteByByteAreJoined)
{
	std::string const reply{ std::string{ ChunkedHeaders } + "4\r\nWiki\r\nA\r\npedia in\r\n\r\n0\r\n\r\n" };
	CHttpResponseReader reader{};
	EHttpReadState state{ EHttpReadState::Headers };

	reader.Reset(MaxSize);

	for (char const& c : reply)
	{
		state = Feed(reader, std::string_view{ &c, 1 });
	}

	ASSERT_EQ(state, EHttpReadState::Complete);
	EXPECT_EQ(reader.TakeBody(), "Wikipedia in\r\n");
}

//////////////////////////////////////////////////////////////////////////
TEST(HttpResponseReader, ChunkExtensionsAndTrailerAreSkipped)
{
	EXPECT_EQ(ReadReply(std::string{ ChunkedHeaders } + "5 ;name=value\r\nhello\r\n0;last\r\nChecksum: 1\r\n\r\n"), "hello");
}

//////////////////////////////////////////////////////////////////////////
TEST(HttpResponseReader, ChunkedBodyLargerThanAllowedIsRefused)
{
	std::string const reply{ std::string{ ChunkedHeaders } + "20\r\n" + std::string(32, 'x') + "\r\n21\r\n" };

	EXPECT_EQ(ReadReply(reply), "error: sent more than the 64 bytes the file may have");
}

//////////////////////////////////////////////////////////////////////////
TEST(HttpResponseReader, ChunkLongerThanItsSizeIsRefused)
{
	EXPECT_EQ(ReadReply(std::string{ ChunkedHeaders } + "3\r\nhello\r\n0\r\n\r\n"), "error: sent a chunk longer than its size");
}

//////////////////////////////////////////////////////////////////////////
TEST(HttpResponseReader, UnreadableChunkSizeIsRefused)
{
	EXPECT_EQ(ReadReply(std::string{ ChunkedHeaders } + "x5\r\nhello\r\n0\r\n\r\n"), "error: sent a chunk size Lookout cannot read");
}

//////////////////////////////////////////////////////////////////////////
TEST(HttpResponseReader, BytesAfterTheLastChunkAreRefused)
{
	EXPECT_EQ(ReadReply(std::string{ ChunkedHeaders } + "5\r\nhello\r\n0\r\n\r\nmore"), "error: sent 4 bytes after its last chunk");
}
} // namespace
} // namespace Lkt::Net
