/* Host shim for CMSIS cmsis_gcc.h (force-included first): the real header is included (its inline-asm
 * helpers are never emitted when unused), the intrinsics the audio engine uses
 * are replaced by C versions with the same bit-level results. */
#ifndef HOST_CMSIS_GCC_SHIM_H
#define HOST_CMSIS_GCC_SHIM_H
#include <stdint.h>

#define __QADD16 arm_QADD16_unused
#define __QSUB16 arm_QSUB16_unused
#define __QADD   arm_QADD_unused
#define __QSUB   arm_QSUB_unused
#define __get_APSR arm_get_APSR_unused
#include <cmsis_gcc.h>
#undef __QADD16
#undef __QSUB16
#undef __QADD
#undef __QSUB
#undef __get_APSR
#undef __SSAT
#undef __PKHBT

static inline int32_t host_sat(int64_t v, int bits)
{
	const int64_t max = ((int64_t)1 << (bits - 1)) - 1, min = -((int64_t)1 << (bits - 1));
	return (int32_t)(v > max ? max : (v < min ? min : v));
}
static inline uint32_t __QADD16(uint32_t a, uint32_t b)
{
	const uint32_t lo = (uint16_t)host_sat((int16_t)a + (int16_t)b, 16);
	const uint32_t hi = (uint16_t)host_sat((int16_t)(a >> 16) + (int16_t)(b >> 16), 16);
	return lo | (hi << 16);
}
static inline uint32_t __QSUB16(uint32_t a, uint32_t b)
{
	const uint32_t lo = (uint16_t)host_sat((int16_t)a - (int16_t)b, 16);
	const uint32_t hi = (uint16_t)host_sat((int16_t)(a >> 16) - (int16_t)(b >> 16), 16);
	return lo | (hi << 16);
}
static inline int32_t __QADD(int32_t a, int32_t b) { return host_sat((int64_t)a + b, 32); }
static inline int32_t __QSUB(int32_t a, int32_t b) { return host_sat((int64_t)a - b, 32); }
/* Oscillator.c calcNoise() reads the overflow flag after a C addition; on the
 * target that flag is not reliably set by the add. Host: always clear. */
static inline uint32_t __get_APSR(void) { return 0; }
#define __SSAT(ARG1, ARG2) host_sat((int32_t)(ARG1), (ARG2))
#define __PKHBT(ARG1, ARG2, ARG3) ((((uint32_t)(ARG1)) & 0x0000FFFFUL) | ((((uint32_t)(ARG2)) << (ARG3)) & 0xFFFF0000UL))

#endif
