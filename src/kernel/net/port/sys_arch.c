#include <lwip/sys.h>

extern volatile uint32_t g_pit_ticks;

u32_t
sys_now(void)
{
	/* The PIT runs at 100 Hz. */
	return g_pit_ticks * 10;
}

u32_t
net_rand(void)
{
	static uint32_t state;

	if (state == 0)
	{
		uint32_t lo, hi;

		asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
		state = (lo ^ hi) | 1;
	}

	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;

	return state;
}
