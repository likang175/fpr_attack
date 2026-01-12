#include "gaussian_solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <omp.h>

/**
 * Compute modular multiplicative inverse using Extended Euclidean Algorithm
 */
int32_t modinv(int32_t a, int32_t q) {
    a %= q;
    if (a < 0) a += q;
    if (a == 0) return -1;

    int32_t t = 0, newt = 1;
    int32_t r = q, newr = a;

    while (newr != 0) {
        int32_t quotient = r / newr;
        int32_t tmp_t = t;
        t = newt;
        newt = tmp_t - quotient * newt;

        int32_t tmp_r = r;
        r = newr;
        newr = tmp_r - quotient * newr;
    }

    if (r > 1) return -1;  // a is not invertible
    if (t < 0) t += q;
    return t;
}

/**
 * Map x (mod q) to centered interval [-q/2, q/2]
 */
int32_t to_centered_mod(int32_t x, int32_t q) {
    x %= q;
    if (x < 0) x += q;
    if (x > q / 2) {
        return x - q;
    }
    return x;
}

/**
 * Solve A*x ≡ b (mod q) using Gaussian elimination
 */
solve_result_t gaussian_mod_q_solve(int32_t **A, int32_t *b, int n, int32_t q) {
    solve_result_t result;
    result.x = NULL;
    result.n = n;
    result.status = SOLVE_OK;
    result.message[0] = '\0';

    // Debug: Check OpenMP
    #ifdef _OPENMP
    int num_threads = omp_get_max_threads();
    printf("[gaussian_solve] OpenMP enabled with %d threads (n=%d)\n", num_threads, n);
    #else
    printf("[gaussian_solve] OpenMP NOT enabled (compiled without -fopenmp)\n");
    #endif

    // Allocate augmented matrix [A|b]
    int32_t **M = (int32_t **)malloc(n * sizeof(int32_t *));
    if (!M) {
        result.status = SOLVE_NO_PIVOT;
        snprintf(result.message, sizeof(result.message), "memory allocation failed");
        return result;
    }

    for (int i = 0; i < n; i++) {
        M[i] = (int32_t *)malloc((n + 1) * sizeof(int32_t));
        if (!M[i]) {
            // Free already allocated rows
            for (int j = 0; j < i; j++) free(M[j]);
            free(M);
            result.status = SOLVE_NO_PIVOT;
            snprintf(result.message, sizeof(result.message), "memory allocation failed");
            return result;
        }
        // Copy A[i] and b[i]
        for (int j = 0; j < n; j++) {
            M[i][j] = A[i][j] % q;
            if (M[i][j] < 0) M[i][j] += q;
        }
        M[i][n] = b[i] % q;
        if (M[i][n] < 0) M[i][n] += q;
    }

    // Gaussian elimination
    for (int col = 0; col < n; col++) {
        // Find pivot
        int pivot_row = -1;
        for (int row = col; row < n; row++) {
            if (M[row][col] % q != 0) {
                int32_t inv = modinv(M[row][col], q);
                if (inv != -1) {
                    pivot_row = row;
                    break;
                }
            }
        }

        if (pivot_row == -1) {
            // Cleanup
            for (int i = 0; i < n; i++) free(M[i]);
            free(M);
            result.status = SOLVE_NO_PIVOT;
            snprintf(result.message, sizeof(result.message), "no pivot in column %d, singular matrix", col);
            return result;
        }

        // Swap rows
        if (pivot_row != col) {
            int32_t *tmp = M[col];
            M[col] = M[pivot_row];
            M[pivot_row] = tmp;
        }

        // Normalize pivot row
        int32_t pivot_val = M[col][col] % q;
        int32_t inv_pivot = modinv(pivot_val, q);
        if (inv_pivot == -1) {
            for (int i = 0; i < n; i++) free(M[i]);
            free(M);
            result.status = SOLVE_NO_INVERSE;
            snprintf(result.message, sizeof(result.message), "modular inverse not found for pivot %d", pivot_val);
            return result;
        }

        for (int j = col; j <= n; j++) {
            M[col][j] = ((int64_t)M[col][j] * inv_pivot) % q;
            if (M[col][j] < 0) M[col][j] += q;
        }

        // Eliminate other rows (parallelized with OpenMP)
#pragma omp parallel for schedule(dynamic) if(n > 100)
        for (int row = 0; row < n; row++) {
            if (row == col) continue;
            int32_t factor = M[row][col] % q;
            if (factor < 0) factor += q;
            for (int j = col; j <= n; j++) {
                M[row][j] = (M[row][j] - (int64_t)factor * M[col][j]) % q;
                if (M[row][j] < 0) M[row][j] += q;
            }
        }
    }

    // Extract solution
    result.x = (int32_t *)malloc(n * sizeof(int32_t));
    if (!result.x) {
        for (int i = 0; i < n; i++) free(M[i]);
        free(M);
        result.status = SOLVE_NO_PIVOT;
        snprintf(result.message, sizeof(result.message), "memory allocation failed for solution");
        return result;
    }

    for (int i = 0; i < n; i++) {
        result.x[i] = to_centered_mod(M[i][n], q);
    }

    // Cleanup
    for (int i = 0; i < n; i++) free(M[i]);
    free(M);

    strcpy(result.message, "ok");
    return result;
}

/**
 * Solve for unknown f coefficients
 */
solve_result_t solve_unknown_f(
    const int32_t *g, const bool *g_mask,
    const int32_t *f, const bool *f_mask,
    const int32_t *h,
    int n, int32_t mod
) {
    solve_result_t result;
    result.x = NULL;
    result.n = 0;
    result.status = SOLVE_OK;
    result.message[0] = '\0';

    // Count known g and unknown f
    int num_known_g = 0;
    int num_unknown_f = 0;

    for (int i = 0; i < n; i++) {
        if (g_mask[i]) num_known_g++;
        if (!f_mask[i]) num_unknown_f++;
    }

    if (num_known_g == 0) {
        result.status = SOLVE_NO_KNOWN_G;
        strcpy(result.message, "no known g");
        return result;
    }

    // Build index lists
    int *I_g = (int *)malloc(num_known_g * sizeof(int));
    int *I_f_known = (int *)malloc(n * sizeof(int));
    int *I_f_unk = (int *)malloc(num_unknown_f * sizeof(int));

    int idx_g = 0, idx_f_known = 0, idx_f_unk = 0;
    for (int i = 0; i < n; i++) {
        if (g_mask[i]) I_g[idx_g++] = i;
        if (f_mask[i]) I_f_known[idx_f_known++] = i;
        else I_f_unk[idx_f_unk++] = i;
    }

    // Build system A*x = b
    int32_t **A = (int32_t **)malloc(num_known_g * sizeof(int32_t *));
    int32_t *bvec = (int32_t *)malloc(num_known_g * sizeof(int32_t));

    for (int i = 0; i < num_known_g; i++) {
        A[i] = (int32_t *)malloc(num_unknown_f * sizeof(int32_t));
        int k = I_g[i];

        // Build Hk (circulant row from h)
        int32_t *Hk = (int32_t *)malloc(n * sizeof(int32_t));
        for (int j = 0; j < n; j++) {
            if (k >= j) {
                Hk[j] = h[k - j] % mod;
            } else {
                Hk[j] = (-h[n + k - j]) % mod;
            }
            if (Hk[j] < 0) Hk[j] += mod;
        }

        // Compute rhs = g[k] - sum(Hk[j] * f[j] for known j)
        int64_t total = 0;
        for (int idx = 0; idx < idx_f_known; idx++) {
            int j = I_f_known[idx];
            total += (int64_t)Hk[j] * f[j];
        }
        int32_t rhs = (g[k] - total) % mod;
        if (rhs < 0) rhs += mod;
        bvec[i] = rhs;

        // Build A row for unknown f positions
        for (int idx = 0; idx < num_unknown_f; idx++) {
            int j = I_f_unk[idx];
            A[i][idx] = Hk[j] % mod;
            if (A[i][idx] < 0) A[i][idx] += mod;
        }

        free(Hk);
    }

    // Solve the system
    solve_result_t gauss_result = gaussian_mod_q_solve(A, bvec, num_known_g, mod);

    // Cleanup
    for (int i = 0; i < num_known_g; i++) free(A[i]);
    free(A);
    free(bvec);
    free(I_g);
    free(I_f_known);
    free(I_f_unk);

    return gauss_result;
}

/**
 * Free memory allocated for solve_result_t
 */
void free_solve_result(solve_result_t *result) {
    if (result && result->x) {
        free(result->x);
        result->x = NULL;
    }
}
