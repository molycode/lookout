#include "query/utf8.hpp"

namespace Lkt::Query
{
//////////////////////////////////////////////////////////////////////////
// Rejects overlong forms, surrogates and code points past U+10FFFF, which narrow the first continuation byte.
bool IsValidUtf8(std::string_view text)
{
	bool valid{ true };
	size_t index{ 0 };

	while (valid && index < text.size())
	{
		unsigned char const lead{ static_cast<unsigned char>(text[index]) };
		size_t numContinuations{ 0 };
		unsigned char firstMin{ 0x80 };
		unsigned char firstMax{ 0xBF };

		if (lead >= 0xC2 && lead <= 0xDF)
		{
			numContinuations = 1;
		}
		else if (lead >= 0xE0 && lead <= 0xEF)
		{
			numContinuations = 2;
			firstMin = (lead == 0xE0) ? 0xA0 : 0x80;
			firstMax = (lead == 0xED) ? 0x9F : 0xBF;
		}
		else if (lead >= 0xF0 && lead <= 0xF4)
		{
			numContinuations = 3;
			firstMin = (lead == 0xF0) ? 0x90 : 0x80;
			firstMax = (lead == 0xF4) ? 0x8F : 0xBF;
		}
		else
		{
			valid = lead < 0x80;
		}

		valid = valid && index + numContinuations < text.size();

		for (size_t offset{ 1 }; valid && offset <= numContinuations; ++offset)
		{
			unsigned char const continuation{ static_cast<unsigned char>(text[index + offset]) };

			valid = (offset == 1) ? (continuation >= firstMin && continuation <= firstMax) : ((continuation & 0xC0) == 0x80);
		}

		index += numContinuations + 1;
	}

	return valid;
}
} // namespace Lkt::Query
