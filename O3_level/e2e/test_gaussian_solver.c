// gcc gaussian_solver.c test_gaussian_solver.c -O3 -Wall -Wextra -std=c11 -fopenmp -o test_gaussian_solver
#include "gaussian_solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * Simple test of the Gaussian elimination
 */
void test_gaussian() {
    printf("=== Test Gaussian Elimination ===\n");

    // Simple 3x3 system mod 17:
    // 2x + 3y + z ≡ 10 (mod 17)
    // x + 4y + 2z ≡ 8 (mod 17)
    // 3x + y + 5z ≡ 15 (mod 17)

    int n = 3;
    int32_t q = 17;

    int32_t **A = (int32_t **)malloc(n * sizeof(int32_t *));
    A[0] = (int32_t *)malloc(n * sizeof(int32_t));
    A[1] = (int32_t *)malloc(n * sizeof(int32_t));
    A[2] = (int32_t *)malloc(n * sizeof(int32_t));

    A[0][0] = 2; A[0][1] = 3; A[0][2] = 1;
    A[1][0] = 1; A[1][1] = 4; A[1][2] = 2;
    A[2][0] = 3; A[2][1] = 1; A[2][2] = 5;

    int32_t b[3] = {10, 8, 15};

    solve_result_t result = gaussian_mod_q_solve(A, b, n, q);

    if (result.status == SOLVE_OK) {
        printf("Solution found: x = [%d, %d, %d]\n",
               result.x[0], result.x[1], result.x[2]);
        printf("Status: %s\n", result.message);

        // Verify solution
        printf("\nVerification:\n");
        for (int i = 0; i < n; i++) {
            int32_t lhs = 0;
            for (int j = 0; j < n; j++) {
                lhs += A[i][j] * result.x[j];
            }
            lhs %= q;
            if (lhs < 0) lhs += q;
            printf("Row %d: %d ≡ %d (mod %d) %s\n",
                   i, lhs, b[i], q, (lhs == b[i]) ? "✓" : "✗");
        }
    } else {
        printf("Failed: %s\n", result.message);
    }

    free_solve_result(&result);
    for (int i = 0; i < n; i++) free(A[i]);
    free(A);
}

/**
 * Test modular inverse
 */
void test_modinv() {
    printf("\n=== Test Modular Inverse ===\n");

    int32_t a = 3, q = 11;
    int32_t inv = modinv(a, q);
    printf("modinv(%d, %d) = %d\n", a, q, inv);
    printf("Verification: %d * %d mod %d = %d\n", a, inv, q, (a * inv) % q);

    a = 7; q = 17;
    inv = modinv(a, q);
    printf("modinv(%d, %d) = %d\n", a, q, inv);
    printf("Verification: %d * %d mod %d = %d\n", a, inv, q, (a * inv) % q);

    a = 5; q = 12289;
    inv = modinv(a, q);
    printf("modinv(%d, %d) = %d\n", a, q, inv);
    printf("Verification: %d * %d mod %d = %d\n", a, inv, q, ((int64_t)a * inv) % q);
}

/**
 * Test to_centered_mod
 */
void test_centered_mod() {
    printf("\n=== Test Centered Mod ===\n");

    int32_t q = 17;
    printf("Mapping to [-%d, %d]:\n", q/2, q/2);
    for (int x = 0; x < 20; x++) {
        int32_t centered = to_centered_mod(x, q);
        printf("to_centered_mod(%2d, %d) = %3d\n", x, q, centered);
    }

    printf("\nFor q=12289:\n");
    q = 12289;
    int test_values[] = {0, 1, 6144, 6145, 12288, 12289, 12290};
    for (int i = 0; i < 7; i++) {
        int32_t x = test_values[i];
        int32_t centered = to_centered_mod(x, q);
        printf("to_centered_mod(%5d, %d) = %6d\n", x, q, centered);
    }
}

/**
 * Test solve_unknown_f with a simple example
 */
void test_solve_unknown_f_simple() {
    printf("\n=== Test Solve Unknown F (Simple) ===\n");

    int n = 8;
    int32_t mod = 17;

    // Create simple arrays
    int32_t g[8] = {1, 2, 0, 0, 0, 0, 0, 0};
    bool g_mask[8] = {true, true, false, false, false, false, false, false};

    int32_t f[8] = {3, 0, 0, 0, 0, 0, 0, 0};
    bool f_mask[8] = {true, false, false, false, false, false, false, false};

    int32_t h[8] = {5, 7, 2, 1, 0, 0, 0, 0};

    printf("Known g coefficients: 2\n");
    printf("Known f coefficients: 1\n");
    printf("Unknown f coefficients: 7\n");

    solve_result_t result = solve_unknown_f(g, g_mask, f, f_mask, h, n, mod);

    printf("Status: %s\n", result.message);
    if (result.status == SOLVE_OK) {
        printf("Recovered %d unknown f coefficients:\n", result.n);
        for (int i = 0; i < result.n; i++) {
            printf("  f[%d] = %d\n", i, result.x[i]);
        }
    } else {
        printf("Failed to solve (this is expected for random data)\n");
    }

    free_solve_result(&result);
}

int main() {
    printf("Solve f from h and g - C Implementation Test\n");
    printf("=============================================\n\n");

    test_modinv();
    test_centered_mod();
    test_gaussian();
    test_solve_unknown_f_simple();

    printf("\n=============================================\n");
    printf("All tests completed.\n");
    return 0;
}
