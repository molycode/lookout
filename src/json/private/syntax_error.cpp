#include "json/syntax_error.hpp"
#include "syntax_error_handler.hpp"

namespace Lkt::Json
{
//////////////////////////////////////////////////////////////////////////
std::string DescribeSyntaxError(std::string_view text)
{
	CSyntaxErrorHandler handler{};

	CSyntaxErrorHandler::JsonValue::sax_parse(text, &handler);

	return handler.GetError();
}
} // namespace Lkt::Json
