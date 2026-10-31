#pragma once

#include <tge/non_copyable.hpp>

namespace Lkt::Net
{
class CCryptoThreading final : private Tge::SNoCopyNoMove
{
public:

	CCryptoThreading() = default;
	~CCryptoThreading() = default;

	void Initialize();
	void Terminate();

private:

	bool m_isInitialized{ false };
};
} // namespace Lkt::Net
