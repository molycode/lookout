#pragma once

#include "games/field_problem.hpp"
#include "json/json.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Lkt::Games
{
std::string JoinPath(std::string_view parent, std::string_view key);
void Fail(SFieldProblem& problem, std::string_view path, std::string_view reason);
void ReadString(nlohmann::ordered_json const& json, std::string_view path, std::string& value, SFieldProblem& problem);
bool IsComment(std::string_view key);
void CheckComment(nlohmann::ordered_json const& object, std::string_view path, std::string_view key, nlohmann::ordered_json const& comment,
	SFieldProblem& problem);
void ReadStrings(nlohmann::ordered_json const& json, std::string_view path, std::vector<std::string>& values, SFieldProblem& problem);
void ReadRequiredString(nlohmann::ordered_json const& object, std::string_view parent, std::string_view key, std::string& value, SFieldProblem& problem);
void ReadRequiredStrings(nlohmann::ordered_json const& object, std::string_view parent, std::string_view key, std::vector<std::string>& values,
	SFieldProblem& problem);
nlohmann::ordered_json const* FindObject(nlohmann::ordered_json const& object, std::string_view parent, std::string_view key, bool isRequired,
	SFieldProblem& problem);

template<typename TEnum, size_t NumNames>
void ReadName(nlohmann::ordered_json const& object, std::string_view parent, std::string_view key,
	std::array<std::pair<std::string_view, TEnum>, NumNames> const& names, TEnum& value, SFieldProblem& problem)
{
	std::string name{};

	ReadRequiredString(object, parent, key, name, problem);

	auto const it{ std::ranges::find(names, std::string_view{ name }, &std::pair<std::string_view, TEnum>::first) };

	if (it != names.end())
	{
		value = it->second;
	}
	else if (!name.empty())
	{
		std::string known{};

		for (std::pair<std::string_view, TEnum> const& entry : names)
		{
			known += std::format("{}{}", known.empty() ? "" : ", ", entry.first);
		}

		Fail(problem, JoinPath(parent, key), std::format("'{}' is not one of {}", name, known));
	}
}
} // namespace Lkt::Games
