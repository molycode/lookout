#include "json_fields.hpp"
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace Lkt::Games
{
namespace
{
constexpr std::string_view CommentPrefix{ "//" };
} // namespace

//////////////////////////////////////////////////////////////////////////
std::string JoinPath(std::string_view parent, std::string_view key)
{
	return parent.empty() ? std::string{ key } : std::format("{}.{}", parent, key);
}

//////////////////////////////////////////////////////////////////////////
// Only the first problem is kept, so the message names where the file first went wrong.
void Fail(SFieldProblem& problem, std::string_view path, std::string_view reason)
{
	if (problem.reason.empty())
	{
		problem = SFieldProblem{ std::string{ path }, std::string{ reason } };
	}
}

//////////////////////////////////////////////////////////////////////////
// Every string names a rule, a host or a path, so an empty one or a NUL is always a mistake.
void ReadString(nlohmann::ordered_json const& json, std::string_view path, std::string& value, SFieldProblem& problem)
{
	bool const isValid{ json.is_string() && !json.get_ref<std::string const&>().empty() && !json.get_ref<std::string const&>().contains('\0') };

	if (isValid)
	{
		value = json.get<std::string>();
	}
	else
	{
		Fail(problem, path, "must be a non-empty string without NUL");
	}
}

//////////////////////////////////////////////////////////////////////////
bool IsComment(std::string_view key)
{
	return key.starts_with(CommentPrefix);
}

//////////////////////////////////////////////////////////////////////////
// Its field must be beside it, so a comment left behind by a removed or renamed field is caught.
void CheckComment(nlohmann::ordered_json const& object, std::string_view path, std::string_view key, nlohmann::ordered_json const& comment,
	SFieldProblem& problem)
{
	std::string_view const field{ key.substr(CommentPrefix.size()) };
	std::string text{};

	if (object.find(field) == object.cend())
	{
		Fail(problem, JoinPath(path, key), std::format("is a comment on '{}', which is not in this object", field));
	}

	ReadString(comment, JoinPath(path, key), text, problem);
}

//////////////////////////////////////////////////////////////////////////
void ReadStrings(nlohmann::ordered_json const& json, std::string_view path, std::vector<std::string>& values, SFieldProblem& problem)
{
	if (json.is_array())
	{
		size_t index{ 0 };

		for (nlohmann::ordered_json const& element : json)
		{
			ReadString(element, std::format("{}[{}]", path, index), values.emplace_back(), problem);
			++index;
		}
	}
	else
	{
		Fail(problem, path, "must be an array of strings");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadRequiredString(nlohmann::ordered_json const& object, std::string_view parent, std::string_view key, std::string& value, SFieldProblem& problem)
{
	nlohmann::ordered_json::const_iterator const it{ object.find(key) };

	if (it != object.cend())
	{
		ReadString(*it, JoinPath(parent, key), value, problem);
	}
	else
	{
		Fail(problem, JoinPath(parent, key), "is missing");
	}
}

//////////////////////////////////////////////////////////////////////////
void ReadRequiredStrings(nlohmann::ordered_json const& object, std::string_view parent, std::string_view key, std::vector<std::string>& values,
	SFieldProblem& problem)
{
	nlohmann::ordered_json::const_iterator const it{ object.find(key) };

	if (it != object.cend())
	{
		ReadStrings(*it, JoinPath(parent, key), values, problem);
	}
	else
	{
		Fail(problem, JoinPath(parent, key), "is missing");
	}
}

//////////////////////////////////////////////////////////////////////////
nlohmann::ordered_json const* FindObject(nlohmann::ordered_json const& object, std::string_view parent, std::string_view key, bool isRequired,
	SFieldProblem& problem)
{
	nlohmann::ordered_json const* pFound{ nullptr };
	nlohmann::ordered_json::const_iterator const it{ object.find(key) };

	if (it != object.cend() && it->is_object())
	{
		pFound = &*it;
	}
	else if (it != object.cend())
	{
		Fail(problem, JoinPath(parent, key), "must be an object");
	}
	else if (isRequired)
	{
		Fail(problem, JoinPath(parent, key), "is missing");
	}

	return pFound;
}
} // namespace Lkt::Games
