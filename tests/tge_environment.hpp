#pragma once

#include <gtest/gtest.h>

namespace Lkt::Fixtures
{
// The query engine's tests run on a tge event loop, so the suite brings tge up once around all of them.
class CTgeEnvironment final : public testing::Environment
{
public:

	CTgeEnvironment() = default;
	~CTgeEnvironment() override = default;

	// testing::Environment
	void SetUp() override;
	void TearDown() override;
	// ~testing::Environment
};
} // namespace Lkt::Fixtures
