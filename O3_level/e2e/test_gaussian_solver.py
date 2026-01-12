#!/usr/bin/env python3
"""
Test script for gaussian_solver.so
"""

import numpy as np
import gaussian_solver as gs

def test_gaussian_solve():
    """Test the Gaussian elimination solver"""
    print("=== Test Gaussian Elimination ===")

    # Simple 3x3 system mod 17:
    # 2x + 3y + z ≡ 10 (mod 17)
    # x + 4y + 2z ≡ 8 (mod 17)
    # 3x + y + 5z ≡ 15 (mod 17)

    A = np.array([
        [2, 3, 1],
        [1, 4, 2],
        [3, 1, 5]
    ], dtype=np.int32)

    b = np.array([10, 8, 15], dtype=np.int32)
    q = 17

    success, result, message = gs.gaussian_solve(A, b, q)

    print(f"Success: {success}")
    print(f"Message: {message}")
    if success:
        print(f"Solution: {result}")

        # Verify solution
        for i in range(3):
            lhs = sum(A[i][j] * result[j] for j in range(3)) % q
            print(f"Row {i}: {lhs} ≡ {b[i]} (mod {q})")


def test_solve_unknown_f_simple():
    """Test solve_unknown_f with a simple example"""
    print("\n=== Test Solve Unknown F (Simple) ===")

    n = 8  # Small example
    q = 17

    # Create simple test case
    # Let's say we know some g values and some f values
    # and we want to recover unknown f values

    g = np.array([1, 2, 0, 0, 0, 0, 0, 0], dtype=np.int32)
    g_mask = np.array([True, True, False, False, False, False, False, False], dtype=bool)

    f = np.array([3, 0, 0, 0, 0, 0, 0, 0], dtype=np.int32)
    f_mask = np.array([True, False, False, False, False, False, False, False], dtype=bool)

    h = np.array([5, 7, 2, 1, 0, 0, 0, 0], dtype=np.int32)

    success, result, message = gs.solve_unknown_f(g, g_mask, f, f_mask, h, q)

    print(f"Success: {success}")
    print(f"Message: {message}")
    if success:
        print(f"Recovered unknown f coefficients: {result}")
        print(f"Number of unknowns solved: {len(result)}")


def test_solve_unknown_f_falcon512():
    """Test with Falcon-512 size (but random data)"""
    print("\n=== Test Solve Unknown F (Falcon-512 size) ===")

    n = 512
    q = 12289

    # Create random test data
    np.random.seed(42)

    # Simulate partial knowledge
    g = np.random.randint(-100, 100, n, dtype=np.int32)
    g_mask = np.random.rand(n) < 0.6  # 60% known

    f = np.random.randint(-100, 100, n, dtype=np.int32)
    f_mask = np.random.rand(n) < 0.5  # 50% known

    h = np.random.randint(0, q, n, dtype=np.int32)

    print(f"n = {n}")
    print(f"Known g coefficients: {g_mask.sum()}")
    print(f"Known f coefficients: {f_mask.sum()}")
    print(f"Unknown f coefficients: {(~f_mask).sum()}")

    success, result, message = gs.solve_unknown_f(g, g_mask, f, f_mask, h, q)

    print(f"Success: {success}")
    print(f"Message: {message}")
    if success:
        print(f"Recovered {len(result)} unknown f coefficients")


if __name__ == "__main__":
    print("Testing gaussian_solver.so\n")
    print("=" * 50)

    test_gaussian_solve()
    test_solve_unknown_f_simple()
    test_solve_unknown_f_falcon512()

    print("\n" + "=" * 50)
    print("All tests completed!")
