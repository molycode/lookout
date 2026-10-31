#include "net/crypto_threading.hpp"
#include <tge/assert.hpp>
#include <tge/platform.hpp>

#if defined(TGE_PLATFORM_WINDOWS)
#include <mbedtls/threading.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <windows.h>
#endif // defined(TGE_PLATFORM_WINDOWS)

namespace Lkt::Net
{
#if defined(TGE_PLATFORM_WINDOWS)
namespace
{
// threading_alt.h stores each lock in one pointer, so no file that includes Mbed TLS has to see <windows.h>.
static_assert(sizeof(SRWLOCK) == sizeof(mbedtls_platform_mutex_t) && alignof(SRWLOCK) == alignof(mbedtls_platform_mutex_t));
static_assert(sizeof(CONDITION_VARIABLE) == sizeof(mbedtls_platform_condition_variable_t)
	&& alignof(CONDITION_VARIABLE) == alignof(mbedtls_platform_condition_variable_t));

//////////////////////////////////////////////////////////////////////////
SRWLOCK* ToLock(mbedtls_platform_mutex_t* pMutex)
{
	return reinterpret_cast<SRWLOCK*>(pMutex);
}

//////////////////////////////////////////////////////////////////////////
CONDITION_VARIABLE* ToCondition(mbedtls_platform_condition_variable_t* pCondition)
{
	return reinterpret_cast<CONDITION_VARIABLE*>(pCondition);
}

//////////////////////////////////////////////////////////////////////////
int InitializeMutex(mbedtls_platform_mutex_t* pMutex)
{
	InitializeSRWLock(ToLock(pMutex));
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void DestroyMutex(mbedtls_platform_mutex_t*)
{
}

//////////////////////////////////////////////////////////////////////////
int LockMutex(mbedtls_platform_mutex_t* pMutex)
{
	AcquireSRWLockExclusive(ToLock(pMutex));
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int UnlockMutex(mbedtls_platform_mutex_t* pMutex)
{
	ReleaseSRWLockExclusive(ToLock(pMutex));
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int InitializeCondition(mbedtls_platform_condition_variable_t* pCondition)
{
	InitializeConditionVariable(ToCondition(pCondition));
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void DestroyCondition(mbedtls_platform_condition_variable_t*)
{
}

//////////////////////////////////////////////////////////////////////////
int SignalCondition(mbedtls_platform_condition_variable_t* pCondition)
{
	WakeConditionVariable(ToCondition(pCondition));
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int BroadcastCondition(mbedtls_platform_condition_variable_t* pCondition)
{
	WakeAllConditionVariable(ToCondition(pCondition));
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int WaitCondition(mbedtls_platform_condition_variable_t* pCondition, mbedtls_platform_mutex_t* pMutex)
{
	return (SleepConditionVariableSRW(ToCondition(pCondition), ToLock(pMutex), INFINITE, 0) != 0) ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR;
}
} // namespace
#endif // defined(TGE_PLATFORM_WINDOWS)

//////////////////////////////////////////////////////////////////////////
// Before anything calls psa_crypto_init: Mbed TLS's own locks are made from these.
void CCryptoThreading::Initialize()
{
	TGE_ASSERT(!m_isInitialized, "Crypto threading initialized twice");

#if defined(TGE_PLATFORM_WINDOWS)
	mbedtls_threading_set_alt(InitializeMutex, DestroyMutex, LockMutex, UnlockMutex,
		InitializeCondition, DestroyCondition, SignalCondition, BroadcastCondition, WaitCondition);
#endif // defined(TGE_PLATFORM_WINDOWS)

	m_isInitialized = true;
}

//////////////////////////////////////////////////////////////////////////
void CCryptoThreading::Terminate()
{
#if defined(TGE_PLATFORM_WINDOWS)
	if (m_isInitialized)
	{
		mbedtls_threading_free_alt();
	}
#endif // defined(TGE_PLATFORM_WINDOWS)

	m_isInitialized = false;
}
} // namespace Lkt::Net
