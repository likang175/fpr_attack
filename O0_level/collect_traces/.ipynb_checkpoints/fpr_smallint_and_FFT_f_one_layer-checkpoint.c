#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <string.h>
#include "inner.h"

#include "hal.h"
#include "simpleserial.h"
// #define MEASURE_CYCLES
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

#define MKN(logn)   ((size_t)1 << (logn))
typedef uint64_t fpr;
int8_t sk_f[1024];
uint8_t block_id=0;

// extern uint32_t zv_cycles;

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

static uint8_t transit_sk_f(uint8_t* pt, uint8_t len) {
    uint8_t ii;
    uint8_t ack;

    // ii=0;
    
   memcpy(sk_f + block_id * 32, pt, 32);

    if (block_id == 31) {
        block_id = 0;
    } else {
        block_id++;
    }

    ack = 1; // 任意确认字节
    simpleserial_put('r', 1, &ack);
    return 0;
}


static void
smallints_to_fpr(fpr *r, const int8_t *t, unsigned logn) {
    size_t n, u;

    n = MKN(logn);
    for (u = 0; u < n; u ++) {
        r[u] = fpr_of(t[u]);
    }
}

// __attribute__((noinline))
static void FFT_one_layer(fpr *f, unsigned logn) {
   
    size_t t, n, hn, j1, j2, j, ht;
    fpr s_re, s_im;

   
    n = (size_t)1 << logn;
    hn = n >> 1;
    t = hn;
    ht = t>>1;

    j1=0;
    j2=j1+ht;


    s_re = fpr_gm_tab[((2 + 0) << 1) + 0];
    s_im = fpr_gm_tab[((2 + 0) << 1) + 1];
    
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

uint8_t get_fpn(uint8_t *x, uint8_t len)
{
	
    uint8_t log_n;
    // fpr aa;
    fpr b01[1024];
    
    uint32_t cycles = 0;
	char str[256] = "";

    
    log_n = x[0];
    
    
#ifdef USE_TRACEWHISPERER
	trigger_high_pcsamp();
#else
	trigger_high();
#endif
    // smallints_to_fpr(b01,sk_f,log_n);
#ifdef MEASURE_CYCLES
	// print_binary(m);
	__asm__ volatile ("" ::: "memory");
	DWT->CYCCNT = 0;
#endif  
    smallints_to_fpr(b01,sk_f,log_n);
    FFT_one_layer(b01, log_n);
    // aa = b01[0];
    
#ifdef MEASURE_CYCLES
	cycles = DWT->CYCCNT;
	__asm__ volatile ("" ::: "memory");
	sprintf(str, "Cycles: %" PRIu32 "\n", cycles);
	hal_send_str(str);
	// print_binary(m);
#endif
    // PQCLEAN_FALCON512_CLEAN_FFT(b01, log_n);
#ifdef USE_TRACEWHISPERER
	trigger_low_pcsamp();
#else
	trigger_low();
#endif
    simpleserial_put('r', 1, (uint8_t*)sk_f);
    // simpleserial_put('r', 4, (uint8_t*)zv_cycles);
	return SS_ERR_OK;
}

int main(int argc, char *argv[])
{
	platform_init();
	init_uart();
	trigger_setup();
	simpleserial_init();
	simpleserial_addcmd('p', 1,  get_fpn);
    simpleserial_addcmd('k', 32,  transit_sk_f);   
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
