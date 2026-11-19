/*
 * 64-bit division helpers that gcc calls on i386 (normally from libgcc).
 */

#include <lib/c/stdint.h>
#include <lib/c/assert.h>
#include <lib/c/string.h>

uint64_t __udivdi3(uint64_t num, uint64_t den);
uint64_t __umoddi3(uint64_t num, uint64_t den);
int64_t  __divdi3(int64_t num, int64_t den);
int64_t  __moddi3(int64_t num, int64_t den);

static uint64_t
udivmod64(uint64_t num, uint64_t den, uint64_t *rem)
{
	uint64_t quot = 0;
	uint64_t bit = 1;

	assert(den != 0);

	while (den < num && !(den & (1ULL << 63)))
	{
		den <<= 1;
		bit <<= 1;
	}

	while (bit)
	{
		if (num >= den)
		{
			num -= den;
			quot |= bit;
		}

		den >>= 1;
		bit >>= 1;
	}

	if (rem)
		*rem = num;

	return quot;
}

uint64_t
__udivdi3(uint64_t num, uint64_t den)
{
	return udivmod64(num, den, NULL);
}

uint64_t
__umoddi3(uint64_t num, uint64_t den)
{
	uint64_t rem;

	udivmod64(num, den, &rem);
	return rem;
}

int64_t
__divdi3(int64_t num, int64_t den)
{
	uint64_t n = num < 0 ? -(uint64_t)num : (uint64_t)num;
	uint64_t d = den < 0 ? -(uint64_t)den : (uint64_t)den;
	uint64_t q = udivmod64(n, d, NULL);

	return ((num < 0) != (den < 0)) ? -(int64_t)q : (int64_t)q;
}

int64_t
__moddi3(int64_t num, int64_t den)
{
	uint64_t n = num < 0 ? -(uint64_t)num : (uint64_t)num;
	uint64_t d = den < 0 ? -(uint64_t)den : (uint64_t)den;
	uint64_t r;

	udivmod64(n, d, &r);
	return num < 0 ? -(int64_t)r : (int64_t)r;
}
