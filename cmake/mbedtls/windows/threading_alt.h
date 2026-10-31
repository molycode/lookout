#pragma once

typedef struct
{
	void* lock;
} mbedtls_platform_mutex_t;

typedef struct
{
	void* condition;
} mbedtls_platform_condition_variable_t;
