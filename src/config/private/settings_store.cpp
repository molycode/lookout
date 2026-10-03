#include "config/settings_store.hpp"
#include "loggers.hpp"
#include "settings_json.hpp"
#include <cerrno>
#include <cstdlib>
#include <expected>
#include <fcntl.h>
#include <format>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace Lkt::Config
{
namespace
{
constexpr std::string_view FileName{ "config.json" };
constexpr std::string_view BackupSuffix{ ".bad" };
constexpr std::string_view TemporarySuffix{ ".XXXXXX" };
constexpr off_t MaxFileSize{ 1024 * 1024 };
constexpr mode_t NewFileMode{ 0644 };
constexpr mode_t PermissionBits{ 07777 };

//////////////////////////////////////////////////////////////////////////
std::error_code LastError()
{
	return std::error_code{ errno, std::generic_category() };
}

//////////////////////////////////////////////////////////////////////////
// O_NONBLOCK so a FIFO in the file's place cannot hang the start; it does not affect a regular file.
std::expected<std::string, std::error_code> ReadSettingsFile(std::filesystem::path const& path)
{
	std::expected<std::string, std::error_code> result{ std::unexpected{ std::error_code{} } };
	int const fd{ ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK) };

	if (fd >= 0)
	{
		struct stat status{};

		if (::fstat(fd, &status) != 0)
		{
			result = std::unexpected{ LastError() };
		}
		else if (S_ISDIR(status.st_mode))
		{
			result = std::unexpected{ std::make_error_code(std::errc::is_a_directory) };
		}
		else if (!S_ISREG(status.st_mode))
		{
			result = std::unexpected{ std::make_error_code(std::errc::invalid_argument) };
		}
		else if (status.st_size > MaxFileSize)
		{
			result = std::unexpected{ std::make_error_code(std::errc::file_too_large) };
		}
		else
		{
			std::string text{};
			std::error_code readError{};
			size_t numRead{ 0 };
			bool isDone{ false };

			text.resize(static_cast<size_t>(status.st_size));

			while (!isDone)
			{
				ssize_t const count{ ::read(fd, text.data() + numRead, text.size() - numRead) };

				if (count > 0)
				{
					numRead += static_cast<size_t>(count);
					isDone = numRead == text.size();
				}
				else if (count == 0)
				{
					text.resize(numRead);
					isDone = true;
				}
				else if (errno != EINTR)
				{
					readError = LastError();
					isDone = true;
				}
			}

			if (readError.value() == 0)
			{
				result = std::move(text);
			}
			else
			{
				result = std::unexpected{ readError };
			}
		}

		::close(fd);
	}
	else
	{
		result = std::unexpected{ LastError() };
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
std::error_code WriteAll(int fd, std::string_view text)
{
	std::error_code error{};
	size_t numWritten{ 0 };

	while (error.value() == 0 && numWritten < text.size())
	{
		ssize_t const count{ ::write(fd, text.data() + numWritten, text.size() - numWritten) };

		if (count > 0)
		{
			numWritten += static_cast<size_t>(count);
		}
		else if (count == 0)
		{
			error = std::make_error_code(std::errc::io_error);
		}
		else if (errno != EINTR)
		{
			error = LastError();
		}
	}

	return error;
}

//////////////////////////////////////////////////////////////////////////
// A link a dotfile manager put in the file's place survives, since the rename replaces its target instead.
std::filesystem::path ResolveLink(std::filesystem::path const& path)
{
	std::error_code error{};
	std::filesystem::path const target{ std::filesystem::canonical(path, error) };

	return (error.value() == 0) ? target : path;
}

//////////////////////////////////////////////////////////////////////////
// An existing file keeps its permissions: a custom command can hold a server password.
mode_t GetFileMode(std::filesystem::path const& path)
{
	struct stat status{};

	return (::stat(path.c_str(), &status) == 0) ? (status.st_mode & PermissionBits) : NewFileMode;
}

//////////////////////////////////////////////////////////////////////////
// A temporary file of its own, so neither a crash nor a second Lookout writing at once can tear the settings.
bool WriteAtomically(std::filesystem::path const& path, std::string_view text)
{
	std::filesystem::path const target{ ResolveLink(path) };
	std::string temporaryPath{ target.string() + std::string{ TemporarySuffix } };
	std::string failure{};
	int const fd{ ::mkostemp(temporaryPath.data(), O_CLOEXEC) };

	if (fd >= 0)
	{
		std::error_code error{};

		if (::fchmod(fd, GetFileMode(target)) != 0)
		{
			failure = std::format("cannot set the permissions of '{}': {}", temporaryPath, LastError().message());
		}
		else if (error = WriteAll(fd, text); error.value() != 0)
		{
			failure = std::format("cannot write '{}': {}", temporaryPath, error.message());
		}
		else if (::fsync(fd) != 0)
		{
			failure = std::format("cannot flush '{}': {}", temporaryPath, LastError().message());
		}

		if (::close(fd) != 0 && failure.empty())
		{
			failure = std::format("cannot close '{}': {}", temporaryPath, LastError().message());
		}

		if (failure.empty())
		{
			std::filesystem::rename(temporaryPath, target, error);

			if (error.value() != 0)
			{
				failure = std::format("cannot replace it with '{}': {}", temporaryPath, error.message());
			}
		}

		if (!failure.empty())
		{
			std::error_code removeError{};

			std::filesystem::remove(temporaryPath, removeError);

			if (removeError.value() != 0)
			{
				gLog.Warning("Cannot remove the temporary settings file '{}': {}", temporaryPath, removeError.message());
			}
		}
	}
	else
	{
		failure = std::format("cannot create a temporary file beside it: {}", LastError().message());
	}

	if (!failure.empty())
	{
		gLog.Error("Cannot save the settings to '{}': {}", target.string(), failure);
	}

	return failure.empty();
}
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
	SSettings settings{};

	m_written.reset();
	m_canSave = false;

	if (!m_path.empty())
	{
		std::expected<std::string, std::error_code> const text{ ReadSettingsFile(m_path) };

		if (text.has_value())
		{
			std::expected<SSettingsDocument, ESettingsJsonError> const document{ ReadSettingsJson(*text) };

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
	if (m_canSave && m_written != settings && WriteAtomically(m_path, WriteSettingsJson(settings)))
	{
		m_written = settings;
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
