#ifndef SOLVE_F_FROM_H_G_H
#define SOLVE_F_FROM_H_G_H

#include <stdint.h>
#include <stdbool.h>

// Constants
#define MOD_Q 12289
#define MAX_N 1024

// Return status codes
typedef enum {
    SOLVE_OK = 0,
    SOLVE_NO_PIVOT,
    SOLVE_NO_INVERSE,
    SOLVE_NO_KNOWN_G,
    SOLVE_SINGULAR_MATRIX
} solve_status_t;

// Result structure for Gaussian elimination
typedef struct {
    int32_t *x;          // Solution vector (must be freed by caller)
    int n;               // Size of solution
    solve_status_t status;
    char message[128];   // Error/status message
} solve_result_t;

/**
 * Compute modular multiplicative inverse of a mod q
 * Returns -1 if inverse does not exist
 */
int32_t modinv(int32_t a, int32_t q);

/**
 * Map x (mod q) to centered interval [-q/2, q/2]
 */
int32_t to_centered_mod(int32_t x, int32_t q);

/**
 * Solve A*x ≡ b (mod q) using Gaussian elimination
 *
 * @param A Matrix (n x n), row-major order
 * @param b Right-hand side vector (length n)
 * @param n Dimension
 * @param q Modulus
 * @return Result structure containing solution or error
 */
solve_result_t gaussian_mod_q_solve(int32_t **A, int32_t *b, int n, int32_t q);

/**
 * Solve for unknown f coefficients given partial f, g, and h
 *
 * @param g Known g coefficients (NULL indicates unknown)
 * @param g_mask Mask indicating which g coefficients are known (1=known, 0=unknown)
 * @param f Known f coefficients (NULL indicates unknown)
 * @param f_mask Mask indicating which f coefficients are known
 * @param h Full h vector
 * @param n Polynomial degree (512 or 1024)
 * @param mod Modulus (default 12289)
 * @return Result structure containing recovered f coefficients
 */
solve_result_t solve_unknown_f(
    const int32_t *g, const bool *g_mask,
    const int32_t *f, const bool *f_mask,
    const int32_t *h,
    int n, int32_t mod
);

/**
 * Free memory allocated for solve_result_t
 */
void free_solve_result(solve_result_t *result);

#endif // SOLVE_F_FROM_H_G_H
