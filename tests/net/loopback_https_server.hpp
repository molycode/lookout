#pragma once

#include "https_reply.hpp"
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <tge/non_copyable.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace Lkt::Fixtures
{
// An HTTPS/1.1 server on 127.0.0.1 with tests/fixtures/tls/server.pem, its certificate signed by ca.pem. It serves one
// connection at a time on its own thread, answering each request by its path; an unknown path gets a 404.
class CLoopbackHttpsServer final : private Tge::SNoCopyNoMove
{
public:

	CLoopbackHttpsServer() = default;
	~CLoopbackHttpsServer() = default;

	bool Start();
	void Stop();

	void SetReply(std::string_view path, SHttpsReply reply);
	uint16_t GetPort() const;
	size_t GetNumConnections() const;
	size_t GetNumRequests() const;

private:

	void Serve();
	void ServeConnection(int descriptor);
	SHttpsReply FindReply(std::string_view path) const;

	mbedtls_ssl_config m_config{};
	mbedtls_x509_crt m_certificate{};
	mbedtls_pk_context m_key{};
	int m_listener{ -1 };
	uint16_t m_port{ 0 };
	std::thread m_thread;
	std::atomic<bool> m_isStopping{ false };
	std::atomic<size_t> m_numConnections{ 0 };
	std::atomic<size_t> m_numRequests{ 0 };
	mutable std::mutex m_mutex;
	std::map<std::string, SHttpsReply, std::less<>> m_replies;
};
} // namespace Lkt::Fixtures
