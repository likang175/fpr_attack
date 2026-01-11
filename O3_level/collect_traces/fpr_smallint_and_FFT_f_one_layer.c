#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <string.h>

#include "inner.h"
#define FALCON_N 512

/* experiment settings *******************************************************/
#include "hal.h"
#include "simpleserial.h"
#include "tracewhisperer.h"
int8_t sk_f[FALCON_N];
uint8_t block_id=0;
// extern uint32_t zv_cycles;

/* BELOW are from sign.c line 36-39, 179-191, resp. **************************/

/*
 * Compute degree N from logarithm 'logn'.
 */
#define MKN(logn)   ((size_t)1 << (logn))

/*
 * Convert an integer polynomial (with small values) into the
 * representation with complex numbers.
 */
static void
smallints_to_fpr(fpr *r, const int8_t *t, unsigned logn) {
    size_t n, u;

    n = MKN(logn);
    for (u = 0; u < n; u ++) {
        r[u] = fpr_of(t[u]);
    }
}

/* ABOVE are from sign.c line 36-39, 179-191, resp. **************************/

/* BELOW are from fft.c, line 34-86 and line 145-247, resp.
 * Attention: in function PQCLEAN_FALCON512_CLEAN_FFT we modified "u < logn" in
 * line 173, to "u < 2", since we only consider the 1st layer. ***************/

/*
 * Rules for complex number macros:
 * --------------------------------
 *
 * Operand order is: destination, source1, source2...
 *
 * Each operand is a real and an imaginary part.
 *
 * All overlaps are allowed.
 */

/*
 * Addition of two complex numbers (d = a + b).
 */
#define FPC_ADD(d_re, d_im, a_re, a_im, b_re, b_im)   do { \
        fpr fpct_re, fpct_im; \
        fpct_re = fpr_add(a_re, b_re); \
        fpct_im = fpr_add(a_im, b_im); \
        (d_re) = fpct_re; \
        (d_im) = fpct_im; \
    } while (0)

/*
 * Subtraction of two complex numbers (d = a - b).
 */
#define FPC_SUB(d_re, d_im, a_re, a_im, b_re, b_im)   do { \
        fpr fpct_re, fpct_im; \
        fpct_re = fpr_sub(a_re, b_re); \
        fpct_im = fpr_sub(a_im, b_im); \
        (d_re) = fpct_re; \
        (d_im) = fpct_im; \
    } while (0)

/*
 * Multplication of two complex numbers (d = a * b).
 */
#define FPC_MUL(d_re, d_im, a_re, a_im, b_re, b_im)   do { \
        fpr fpct_a_re, fpct_a_im; \
        fpr fpct_b_re, fpct_b_im; \
        fpr fpct_d_re, fpct_d_im; \
        fpct_a_re = (a_re); \
        fpct_a_im = (a_im); \
        fpct_b_re = (b_re); \
        fpct_b_im = (b_im); \
        fpct_d_re = fpr_sub( \
                             fpr_mul(fpct_a_re, fpct_b_re), \
                             fpr_mul(fpct_a_im, fpct_b_im)); \
        fpct_d_im = fpr_add( \
                             fpr_mul(fpct_a_re, fpct_b_im), \
                             fpr_mul(fpct_a_im, fpct_b_re)); \
        (d_re) = fpct_d_re; \
        (d_im) = fpct_d_im; \
    } while (0)

/*
 * Let w = exp(i*pi/N); w is a primitive 2N-th root of 1. We define the
 * values w_j = w^(2j+1) for all j from 0 to N-1: these are the roots
 * of X^N+1 in the field of complex numbers. A crucial property is that
 * w_{N-1-j} = conj(w_j) = 1/w_j for all j.
 *
 * FFT representation of a polynomial f (taken modulo X^N+1) is the
 * set of values f(w_j). Since f is real, conj(f(w_j)) = f(conj(w_j)),
 * thus f(w_{N-1-j}) = conj(f(w_j)). We thus store only half the values,
 * for j = 0 to N/2-1; the other half can be recomputed easily when (if)
 * needed. A consequence is that FFT representation has the same size
 * as normal representation: N/2 complex numbers use N real numbers (each
 * complex number is the combination of a real and an imaginary part).
 *
 * We use a specific ordering which makes computations easier. Let rev()
 * be the bit-reversal function over log(N) bits. For j in 0..N/2-1, we
 * store the real and imaginary parts of f(w_j) in slots:
 *
 *    Re(f(w_j)) -> slot rev(j)/2
 *    Im(f(w_j)) -> slot rev(j)/2+N/2
 *
 * (Note that rev(j) is even for j < N/2.)
 */

/* see inner.h */
void
PQCLEAN_FALCON512_CLEAN_FFT(fpr *f, unsigned logn) {
    /*
     * FFT algorithm in bit-reversal order uses the following
     * iterative algorithm:
     *
     *   t = N
     *   for m = 1; m < N; m *= 2:
     *       ht = t/2
     *       for i1 = 0; i1 < m; i1 ++:
     *           j1 = i1 * t
     *           s = GM[m + i1]
     *           for j = j1; j < (j1 + ht); j ++:
     *               x = f[j]
     *               y = s * f[j + ht]
     *               f[j] = x + y
     *               f[j + ht] = x - y
     *       t = ht
     *
     * GM[k] contains w^rev(k) for primitive root w = exp(i*pi/N).
     *
     * In the description above, f[] is supposed to contain complex
     * numbers. In our in-memory representation, the real and
     * imaginary parts of f[k] are in array slots k and k+N/2.
     *
     * We only keep the first half of the complex numbers. We can
     * see that after the first iteration, the first and second halves
     * of the array of complex numbers have separate lives, so we
     * simply ignore the second part.
     */

    unsigned u;
    size_t t, n, hn, m;

    /*
     * First iteration: compute f[j] + i * f[j+N/2] for all j < N/2
     * (because GM[1] = w^rev(1) = w^(N/2) = i).
     * In our chosen representation, this is a no-op: everything is
     * already where it should be.
     */

    /*
     * Subsequent iterations are truncated to use only the first
     * half of values.
     */
    n = (size_t)1 << logn;
    hn = n >> 1;
    t = hn;
    for (u = 1, m = 2; u < 2; u ++) { /* HERE: 'u < logn' => 'u < 2'; rm 'm<<=1' */
        size_t ht, hm, i1, j1;

        ht = t >> 1;
        hm = m >> 1;
        for (i1 = 0, j1 = 0; i1 < hm; i1 ++, j1 += t) {
            size_t j, j2;

            j2 = j1 + ht;
            fpr s_re, s_im;

            s_re = fpr_gm_tab[((m + i1) << 1) + 0];
            s_im = fpr_gm_tab[((m + i1) << 1) + 1];
            for (j = j1; j < j2; j ++) {
                fpr x_re, x_im, y_re, y_im;

                x_re = f[j];
                x_im = f[j + hn];
                y_re = f[j + ht];
                y_im = f[j + ht + hn];
                FPC_MUL(y_re, y_im, y_re, y_im, s_re, s_im);
                FPC_ADD(f[j], f[j + hn],
                        x_re, x_im, y_re, y_im);
                FPC_SUB(f[j + ht], f[j + ht + hn],
                        x_re, x_im, y_re, y_im);
            }
        }
        t = ht;
    }
}

/* ABOVE are from fft.c, line 34-86 and line 145-247, resp.
 * Attention: in function PQCLEAN_FALCON512_CLEAN_FFT we modified "u < logn" in
 * line 173, to "u < 2", since we only consider the 1st layer. ***************/

static uint8_t transit_sk_f(uint8_t* pt, uint8_t len) {
    uint8_t ack;

   memcpy(sk_f + block_id * 32, pt, 32);

    if (block_id == (FALCON_N / 32 - 1)) {
        block_id = 0;
    } else {
        block_id++;
    }

    ack = 1;
    simpleserial_put('r', 1, &ack);
    return 0;
}

uint8_t do_fpr_scaled(uint8_t *x, uint8_t len)
{
	uint64_t i;
	volatile fpr a;
	char buf[15];
	memcpy(buf, x, 14);
	buf[14]='\0';
	i = atoll(buf);
	/* Use '1' to indicate whether m is negative */
	if (x[15] == '1')
	    i = -i;
	trigger_high_pcsamp();
	a = fpr_scaled(i, 0);
	trigger_low_pcsamp();
	return SS_ERR_OK;
}

uint8_t do_fpr_mul(uint8_t *x, uint8_t len)
{
	volatile fpr a;
	char buf[15];
	memcpy(buf, x, 14);
	fpr x_fpr = fpr_of((int8_t)x[0]);
	fpr y = 4604544271217802189U;
	trigger_high_pcsamp();
	a = fpr_mul(x_fpr, y);
	trigger_low_pcsamp();
	return SS_ERR_OK;
}

uint8_t get_fpn(uint8_t *x, uint8_t len)
{
	
    uint8_t log_n;
    fpr b01[FALCON_N];

    log_n = x[0];

	trigger_high_pcsamp();
	smallints_to_fpr(b01,sk_f,log_n);
	/* FFT_one_layer(b01, log_n); */
	PQCLEAN_FALCON512_CLEAN_FFT(b01, log_n);
	trigger_low_pcsamp();
	simpleserial_put('r', 1, (uint8_t*)sk_f);
	/* simpleserial_put('r', 4, (uint8_t*)zv_cycles); */
	return SS_ERR_OK;
}

int main(int argc, char *argv[])
{
	platform_init();
	init_uart();
	trigger_setup();
	simpleserial_init();
	simpleserial_addcmd('l', 16,  do_fpr_scaled);
	simpleserial_addcmd('m', 1, do_fpr_mul);
	/* simpleserial_addcmd('n', 10, do_fpr_add_store); */
	simpleserial_addcmd('p', 1,  get_fpn);
	simpleserial_addcmd('k', 32,  transit_sk_f);

	/* TraceWhisperer settings */
	simpleserial_addcmd('c', 4, set_pcsample_params);
	simpleserial_addcmd('s', 5, setreg);
	simpleserial_addcmd('g', 5, getreg);
	simpleserial_addcmd('i', 0, info);
	enable_trace();

	while (1)
		simpleserial_get();
}
