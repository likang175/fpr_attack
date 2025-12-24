#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <string.h>

#include "hal.h"
#include "simpleserial.h"
/* #define MEASURE_CYCLES */
#define USE_TRACEWHISPERER
#ifdef USE_TRACEWHISPERER
#include "tracewhisperer.h"
#endif

/*
 * Normalize a provided unsigned integer to the 2^63..2^64-1 range by
 * left-shifting it if necessary. The exponent e is adjusted accordingly
 * (i.e. if the value was left-shifted by n bits, then n is subtracted
 * from e). If source m is 0, then it remains 0, but e is altered.
 * Both m and e must be simple variables (no expressions allowed).
 */

typedef uint64_t fpr;

void print_binary(uint64_t n)
{
	char str[256] = "";
	for (int i = 63; i >= 0; i--) {
		sprintf(str+strlen(str), "%c", (n & (1ULL << i)) ? '1' : '0');
		/* Add a comma every 8 bits */
		if (i % 8 == 0 && i != 0)
			sprintf(str+strlen(str), "%c", ',');
	}
	sprintf(str+strlen(str), "\n");
	hal_send_str(str);
	/* printf("%s", str); */
}

static inline fpr
FPR(int s, int e, uint64_t m)
{
	fpr x;
	uint32_t t;
	unsigned f;

	/*
	 * If e >= -1076, then the value is "normal"; otherwise, it
	 * should be a subnormal, which we clamp down to zero.
	 */
	e += 1076;
	t = (uint32_t)e >> 31;
	m &= (uint64_t)t - 1;

	/*
	 * If m = 0 then we want a zero; make e = 0 too, but conserve
	 * the sign.
	 */
	t = (uint32_t)(m >> 54);
	e &= -(int)t;

	/*
	 * The 52 mantissa bits come from m. Value m has its top bit set
	 * (unless it is a zero); we leave it "as is": the top bit will
	 * increment the exponent by 1, except when m = 0, which is
	 * exactly what we want.
	 */
	x = (((uint64_t)s << 63) | (m >> 2)) + ((uint64_t)(uint32_t)e << 52);

	/*
	 * Rounding: if the low three bits of m are 011, 110 or 111,
	 * then the value should be incremented to get the next
	 * representable value. This implements the usual
	 * round-to-nearest rule (with preference to even values in case
	 * of a tie). Note that the increment may make a carry spill
	 * into the exponent field, which is again exactly what we want
	 * in that case.
	 */
	f = (unsigned)m & 7U;
	x += (0xC8U >> f) & 1;
	return x;
}

#define FPR_NORM64(m, e)   do { \
        uint32_t nt; \
        \
        (e) -= 63; \
        \
        nt = (uint32_t)((m) >> 32); \
        nt = (nt | -nt) >> 31; \
        (m) ^= ((m) ^ ((m) << 32)) & ((uint64_t)nt - 1); \
        (e) += (int)(nt << 5); \
        \
        nt = (uint32_t)((m) >> 48); \
        nt = (nt | -nt) >> 31; \
        (m) ^= ((m) ^ ((m) << 16)) & ((uint64_t)nt - 1); \
        (e) += (int)(nt << 4); \
        \
        nt = (uint32_t)((m) >> 56); \
        nt = (nt | -nt) >> 31; \
        (m) ^= ((m) ^ ((m) <<  8)) & ((uint64_t)nt - 1); \
        (e) += (int)(nt << 3); \
        \
        nt = (uint32_t)((m) >> 60); \
        nt = (nt | -nt) >> 31; \
        (m) ^= ((m) ^ ((m) <<  4)) & ((uint64_t)nt - 1); \
        (e) += (int)(nt << 2); \
        \
        nt = (uint32_t)((m) >> 62); \
        nt = (nt | -nt) >> 31; \
        (m) ^= ((m) ^ ((m) <<  2)) & ((uint64_t)nt - 1); \
        (e) += (int)(nt << 1); \
        \
        nt = (uint32_t)((m) >> 63); \
        (m) ^= ((m) ^ ((m) <<  1)) & ((uint64_t)nt - 1); \
        (e) += (int)(nt); \
    } while (0)


fpr
fpr_scaled(int64_t i, int sc) {
    /*
     * To convert from int to float, we have to do the following:
     *  1. Get the absolute value of the input, and its sign
     *  2. Shift right or left the value as appropriate
     *  3. Pack the result
     *
     * We can assume that the source integer is not -2^63.
     */
    int s, e;
    uint32_t t;
    uint64_t m;

    /*
     * Extract sign bit.
     * We have: -i = 1 + ~i
     */
    s = (int)((uint64_t)i >> 63);
    i ^= -(int64_t)s;
    i += s;

    /*
     * For now we suppose that i != 0.
     * Otherwise, we set m to i and left-shift it as much as needed
     * to get a 1 in the top bit. We can do that in a logarithmic
     * number of conditional shifts.
     */
    m = (uint64_t)i;
    e = 9 + sc;
    FPR_NORM64(m, e);

    /*
     * Now m is in the 2^63..2^64-1 range. We must divide it by 512;
     * if one of the dropped bits is a 1, this should go into the
     * "sticky bit".
     */
    m |= ((uint32_t)m & 0x1FF) + 0x1FF;
    m >>= 9;

    //(uint64_t)(i | -i)在i是0或非0时的hamming distance非常大，可以判断i是否是0
    /*
     * Corrective action: if i = 0 then all of the above was
     * incorrect, and we clamp e and m down to zero.
     */
    t = (uint32_t)((uint64_t)(i | -i) >> 63);
    m &= -(uint64_t)t;
    e &= -(int)t;

    /*
     * Assemble back everything. The FPR() function will handle cases
     * where e is too low.
     */
    return FPR(s, e, m);
}


static inline fpr
fpr_of(int64_t i) {
    return fpr_scaled(i, 0);
}


fpr
fpr_mul(fpr x, fpr y)
{
	uint64_t xu, yu, w, zu, zv;
	uint32_t x0, x1, y0, y1, z0, z1, z2;
	int ex, ey, d, e, s;

	/*
	 * Extract absolute values as scaled unsigned integers. We
	 * don't extract exponents yet.
	 */
	xu = (x & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
	yu = (y & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);

	/*
	 * We have two 53-bit integers to multiply; we need to split
	 * each into a lower half and a upper half. Moreover, we
	 * prefer to have lower halves to be of 25 bits each, for
	 * reasons explained later on.
	 */
	x0 = (uint32_t)xu & 0x01FFFFFF;
	x1 = (uint32_t)(xu >> 25);
	y0 = (uint32_t)yu & 0x01FFFFFF;
	y1 = (uint32_t)(yu >> 25);
	w = (uint64_t)x0 * (uint64_t)y0;
	z0 = (uint32_t)w & 0x01FFFFFF;
	z1 = (uint32_t)(w >> 25);
	w = (uint64_t)x0 * (uint64_t)y1;
	z1 += (uint32_t)w & 0x01FFFFFF;
	z2 = (uint32_t)(w >> 25);
	w = (uint64_t)x1 * (uint64_t)y0;
	z1 += (uint32_t)w & 0x01FFFFFF;
	z2 += (uint32_t)(w >> 25);
	zu = (uint64_t)x1 * (uint64_t)y1;
	z2 += (z1 >> 25);
	z1 &= 0x01FFFFFF;
	zu += z2;

	/*
	 * Since xu and yu are both in the 2^52..2^53-1 range, the
	 * product is in the 2^104..2^106-1 range. We first reassemble
	 * it and round it into the 2^54..2^56-1 range; the bottom bit
	 * is made "sticky". Since the low limbs z0 and z1 are 25 bits
	 * each, we just take the upper part (zu), and consider z0 and
	 * z1 only for purposes of stickiness.
	 * (This is the reason why we chose 25-bit limbs above.)
	 */
	zu |= ((z0 | z1) + 0x01FFFFFF) >> 25;

	/*
	 * We normalize zu to the 2^54..s^55-1 range: it could be one
	 * bit too large at this point. This is done with a conditional
	 * right-shift that takes into account the sticky bit.
	 */
	zv = (zu >> 1) | (zu & 1);
	w = zu >> 55;
	zu ^= (zu ^ zv) & -w;

	/*
	 * Get the aggregate scaling factor:
	 *
	 *   - Each exponent is biased by 1023.
	 *
	 *   - Integral mantissas are scaled by 2^52, hence an
	 *     extra 52 bias for each exponent.
	 *
	 *   - However, we right-shifted z by 50 bits, and then
	 *     by 0 or 1 extra bit (depending on the value of w).
	 *
	 * In total, we must add the exponents, then subtract
	 * 2 * (1023 + 52), then add 50 + w.
	 */
	ex = (int)((x >> 52) & 0x7FF);
	ey = (int)((y >> 52) & 0x7FF);
	e = ex + ey - 2100 + (int)w;

	/*
	 * Sign bit is the XOR of the operand sign bits.
	 */
	s = (int)((x ^ y) >> 63);

	/*
	 * Corrective actions for zeros: if either of the operands is
	 * zero, then the computations above were wrong. Test for zero
	 * is whether ex or ey is zero. We just have to set the mantissa
	 * (zu) to zero, the FPR() function will normalize e.
	 */
	d = ((ex + 0x7FF) & (ey + 0x7FF)) >> 11;
	zu &= -(uint64_t)d;

	/*
	 * FPR() packs the result and applies proper rounding.
	 */
	return FPR(s, e, zu);
}


uint8_t get_fpn(uint8_t *x, uint8_t len)
{
	
    //sqrt(2)/2
    fpr y = 4604544271217802189U;
    fpr x_fpr;
    x_fpr=fpr_of((int8_t)x[0]);

    // int sc = 0;
	uint32_t cycles = 0;
	char str[256] = "";
#ifdef USE_TRACEWHISPERER
	trigger_high_pcsamp();
#else
	trigger_high();
#endif
#ifdef MEASURE_CYCLES
	print_binary(m);
	__asm__ volatile ("" ::: "memory");
	DWT->CYCCNT = 0;
#endif
	// FPR_NORM64(m, e);
    fpr_mul(x_fpr,y);
#ifdef MEASURE_CYCLES
	cycles = DWT->CYCCNT;
	__asm__ volatile ("" ::: "memory");
	sprintf(str, "Cycles: %" PRIu32 "\n", cycles);
	hal_send_str(str);
	print_binary(m);
#endif
#ifdef USE_TRACEWHISPERER
	trigger_low_pcsamp();
#else
	trigger_low();
#endif
    // simpleserial_put('r', 1, &check);
	return SS_ERR_OK;
}

int main(int argc, char *argv[])
{
	platform_init();
	init_uart();
	trigger_setup();
	simpleserial_init();
	simpleserial_addcmd('p', 1,  get_fpn);
#ifdef USE_TRACEWHISPERER
	simpleserial_addcmd('c', 4, set_pcsample_params);
	simpleserial_addcmd('s', 5, setreg);
	simpleserial_addcmd('g', 5, getreg);
	simpleserial_addcmd('i', 0, info);
	enable_trace();
#endif
	while (1)
		simpleserial_get();
#if 0
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <number>\n", argv[0]);
		return 1;
	}
	uint64_t a = atoll(argv[1]);
	print_binary(a);
	uint64_t m = a, e = 0;
	FPR_NORM64(m, e);
	printf("(%" PRIu64" %" PRIu64")\n", m, e);
	print_binary(m);
	return 0;
#endif
}
