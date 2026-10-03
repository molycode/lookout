#include "address_entry.hpp"
#include <tge/assert.hpp>

namespace Lkt::Query
{
namespace
{
//////////////////////////////////////////////////////////////////////////
uint32_t ByteAt(std::string_view bytes, size_t index)
{
	return static_cast<uint32_t>(static_cast<unsigned char>(bytes[index]));
}
} // namespace

//////////////////////////////////////////////////////////////////////////
SServerAddress ReadAddressEntry(std::string_view entry)
{
	TGE_ASSERT(entry.size() == AddressEntrySize, "an address entry is four address bytes and two port bytes");

	return SServerAddress{
		(ByteAt(entry, 0) << 24) | (ByteAt(entry, 1) << 16) | (ByteAt(entry, 2) << 8) | ByteAt(entry, 3),
		static_cast<uint16_t>((ByteAt(entry, 4) << 8) | ByteAt(entry, 5))
	};
}
} // namespace Lkt::Query
