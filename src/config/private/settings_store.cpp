#include "config/settings_store.hpp"
#include "loggers.hpp"
#include "settings_json.hpp"
#include "config/default_settings.hpp"
#include "json/files.hpp"
#include <cstddef>
#include <expected>
#include <format>
#include <string>
#include <system_error>
#include <utility>

namespace Lkt::Config
{
namespace
{
constexpr std::string_view FileName{ "config.json" };
constexpr std::string_view BackupSuffix{ ".bad" };
constexpr size_t MaxFileSize{ 1024 * 1024 };
} // namespace

//////////////////////////////////////////////////////////////////////////
void CSettingsStore::Initialize(std::string_view configDir)
{
	if (configDir.empty())
	{
		gLog.Warning("Settings will not be saved: there is no config directory");
	}
	else
	{
		m_path = std::filesystem::path{ configDir } / FileName;
	}
}

//////////////////////////////////////////////////////////////////////////
SSettings CSettingsStore::Load()
{
	SSettings settings{ MakeDefaultSettings() };

	m_written.reset();
	m_kept.clear();
	m_canSave = false;

	if (!m_path.empty())
	{
		std::expected<std::string, std::error_code> const text{ Json::ReadFile(m_path, MaxFileSize) };

		if (text.has_value())
		{
			std::expected<SSettingsDocument, ESettingsJsonError> const document{ ReadSettingsJson(*text) };

			m_kept = *text;

			if (document.has_value())
			{
				settings = document->settings;
				m_canSave = document->version <= SettingsVersion;

				if (!m_canSave)
				{
					gLog.Warning("'{}' was written by a newer Lookout (format {}), so this one will not save over it", m_path.string(), document->version);
				}

				// Left unset, so the next save repairs the file.
				if (document->numInvalid == 0)
				{
					m_written = settings;
				}
				else
				{
					KeepBackup(std::format("{} invalid (first: {})", document->numInvalid, document->firstInvalidPath));
				}
			}
			else
			{
				MoveAside(ToString(document.error()));
			}
		}
		else if (text.error() == std::errc::no_such_file_or_directory)
		{
			m_canSave = true;
		}
		else
		{
			gLog.Warning("Cannot read the settings file '{}', so it is left alone and defaults are used: {}", m_path.string(), text.error().message());
		}
	}

	return settings;
}

//////////////////////////////////////////////////////////////////////////
void CSettingsStore::Save(SSettings const& settings)
{
	if (m_canSave && m_written != settings)
	{
		std::string text{ WriteSettingsJson(settings, m_kept) };
		std::expected<void, std::string> const written{ Json::WriteFileAtomically(m_path, text) };

		if (written.has_value())
		{
			m_written = settings;
			m_kept = std::move(text);
		}
		else
		{
			gLog.Error("Cannot save the settings to '{}': {}", m_path.string(), written.error());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// A copy, since the file stays in use; it is the only place the replaced values survive the repairing save.
void CSettingsStore::KeepBackup(std::string_view problem)
{
	std::filesystem::path const backupPath{ m_path.string() + std::string{ BackupSuffix } };
	std::error_code error{};

	std::filesystem::copy_file(m_path, backupPath, std::filesystem::copy_options::overwrite_existing, error);

	if (error.value() == 0)
	{
		gLog.Warning("Settings in '{}' were replaced by defaults, {}; the original is kept as '{}'", m_path.string(), problem, backupPath.string());
	}
	else
	{
		m_canSave = false;
		gLog.Warning("Settings in '{}' were replaced by defaults, {}, and the file cannot be backed up, so it is left alone: {}", m_path.string(), problem, error.message());
	}
}

//////////////////////////////////////////////////////////////////////////
// Kept rather than overwritten, so a typo in a hand edit never costs the favourites.
void CSettingsStore::MoveAside(std::string_view reason)
{
	std::filesystem::path const movedPath{ m_path.string() + std::string{ BackupSuffix } };
	std::error_code error{};

	std::filesystem::rename(m_path, movedPath, error);
	m_canSave = error.value() == 0;

	if (m_canSave)
	{
		gLog.Warning("The settings file '{}' is {}; it was moved to '{}' and defaults are used", m_path.string(), reason, movedPath.string());
	}
	else
	{
		gLog.Warning("The settings file '{}' is {}, and it cannot be moved aside, so it is left alone and defaults are used: {}", m_path.string(), reason, error.message());
	}
}
} // namespace Lkt::Config
