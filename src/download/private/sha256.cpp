#include "sha256.hpp"
#include <psa/crypto.h>
#include <array>
#include <cstdint>
#include <format>

namespace Lkt::Download
{
//////////////////////////////////////////////////////////////////////////
// An empty result when hashing fails, which matches no index entry.
std::string HashSha256(std::string_view bytes)
{
	std::array<uint8_t, PSA_HASH_LENGTH(PSA_ALG_SHA_256)> hash{};
	size_t length{ 0 };
	std::string hex{};
	bool const isHashed{ psa_crypto_init() == PSA_SUCCESS
		&& psa_hash_compute(PSA_ALG_SHA_256, reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size(), hash.data(), hash.size(), &length) == PSA_SUCCESS };

	for (size_t index{ 0 }; isHashed && index < length; ++index)
	{
		hex += std::format("{:02x}", hash[index]);
	}

	return hex;
}
} // namespace Lkt::Download
