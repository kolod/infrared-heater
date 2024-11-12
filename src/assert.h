#pragma once
void __assert(const char *file, const int line, char *failedexpr);

#ifdef NDEBUG
	#define ASSERT(x) __assert(__FILE__, __LINE__, nullptr)
#else
	#define ASSERT(x) __assert(__FILE__, __LINE__, x)
#endif
