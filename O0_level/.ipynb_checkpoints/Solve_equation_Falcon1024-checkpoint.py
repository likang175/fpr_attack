import pickle
import csv
import os
import numpy as np

from typing import List, Optional, Tuple

def modinv(a: int, q: int) -> Optional[int]:
    """Calculate the multiplicative inverse of a mod q."""
    a %= q
    if a == 0:
        return None
    t, newt = 0, 1
    r, newr = q, a
    while newr != 0:
        quotient = r // newr
        t, newt = newt, t - quotient * newt
        r, newr = newr, r - quotient * newr
    if r > 1:
        return None
    return t % q

def to_centered_mod(x: int, q: int) -> int:
    x = x % q
    if x > q // 2:
        return x - q
    return x

def gaussian_mod_q_solve(A: List[List[int]], b: List[int], q: int):
   
    import copy
    n = len(A)
    assert all(len(row) == n for row in A), "A must be a square matrix."
    assert len(b) == n, "The length of b must be the same as the number of rows in A."

    M = [row[:] + [bb] for row, bb in zip(copy.deepcopy(A), b)]

    for col in range(n):
        # Find the non-zero pivot element in the current column
        pivot_row = None
        for row in range(col, n):
            if M[row][col] % q != 0:
                inv = modinv(M[row][col], q)
                if inv is not None:
                    pivot_row = row
                    break
        if pivot_row is None:
            return None, f"no pivot in column {col}, singular matrix"

        # Swap the current line and the pivot line
        M[col], M[pivot_row] = M[pivot_row], M[col]

        # Normalization of principal elements
        pivot_val = M[col][col] % q
        inv_pivot = modinv(pivot_val, q)
        if inv_pivot is None:
            return None, f"modular inverse not found for pivot {pivot_val}"
        for j in range(col, n + 1):
            M[col][j] = (M[col][j] * inv_pivot) % q

        # Eliminate other lines
        for row in range(n):
            if row == col:
                continue
            factor = M[row][col] % q
            for j in range(col, n + 1):
                M[row][j] = (M[row][j] - factor * M[col][j]) % q

    # x = [M[i][n] % q for i in range(n)]
    x = [to_centered_mod(M[i][n], q) for i in range(n)]
    return x, "ok"

def solve_unknown_f(g: List[Optional[int]], f: List[Optional[int]], h: List[int], MOD: int = 12289):
    n = len(h)
    I_g = [i for i, val in enumerate(g) if val is not None]
    I_f_known = [i for i, val in enumerate(f) if val is not None]
    I_f_unk = [i for i, val in enumerate(f) if val is None]
    if len(I_g) == 0:
        return None, "no known g"
    A, bvec = [], []
    for k in I_g:
        Hk = [0] * n
        for j in range(n):
            if k >= j:
                Hk[j] = h[k-j] % MOD
            else:
                Hk[j] = (-h[n+k-j]) % MOD
        # rhs = (g[k] - sum((Hk[j] * f[j]) % MOD for j in I_f_known)) % MOD
        total = 0
        for j in I_f_known:
            total += Hk[j] * f[j]
        rhs = (g[k] - total) % MOD
        Arow = [Hk[j] % MOD for j in I_f_unk]
        A.append(Arow)
        bvec.append(rhs)
    x_sol, msg = gaussian_mod_q_solve(A, bvec, MOD)
    if x_sol is None:
        return None, msg
    # f_full = f[:]
    # for idx, j in enumerate(I_f_unk):
    #     # f_full[j] = x_sol[idx] % MOD
    #     f_full[j] = to_centered_mod(x_sol[idx], MOD)
    return x_sol, msg


def select_known_positions(
    d: dict,
    max_known: int = 512,
    n: int = 1024
) -> Tuple[List[Optional[int]], str]:
    """
Select at most `max_known` known items from the candidate set dictionary, prioritizing values of 0 and ±1, followed by ±2 and ±3.
Return a list and status information.
    """
    lst = [None] * n
    count = 0

    preferred_vals = {0, 1, -1}
    secondary_vals = {2, -2, 3, -3}
    # third_vals = {-4,-5,4,5}

    # Step 1: Selecting a priority set
    for k in sorted(d.keys()):
        if count >= max_known:
            break
        vset = d[k]
        if len(vset) == 1 and list(vset)[0] in preferred_vals:
            lst[k] = list(vset)[0]
            count += 1

    # Step 2: Selecting the suboptimal set
    for k in sorted(d.keys()):
        if count >= max_known:
            break
        if lst[k] is not None:
            continue
        vset = d[k]
        if len(vset) == 1 and list(vset)[0] in secondary_vals:
            lst[k] = list(vset)[0]
            count += 1
    # Step 3: Selecting the 3rd best set
    # for k in sorted(d.keys()):
    #     if count >= max_known:
    #         break
    #     if lst[k] is not None:
    #         continue
    #     vset = d[k]
    #     if len(vset) == 1 and list(vset)[0] in third_vals:
    #         lst[k] = list(vset)[0]
    #         count += 1

    if count < max_known:
        return lst, f"not enough known values"
    return lst, "ok"


def run_experiments(f_dir: str, g_dir: str, h_csv_path: str, out_csv_path: str, num: int = 100):
    results = []
    h_list = []
    with open(h_csv_path, newline="") as csvfile:
        reader = csv.reader(csvfile)
        for i, row in enumerate(reader):
            if i >= num:
                break
            h_row = list(map(int, row))
            h_list.append(h_row)
    assert len(h_list) == num

    for i in range(num):
        # load f,g dict
        with open(os.path.join(f_dir, f"f_{i}_guess.pkl"), "rb") as ffile:
            f_dict = pickle.load(ffile)
        with open(os.path.join(g_dir, f"g_{i}_guess.pkl"), "rb") as gfile:
            g_dict = pickle.load(gfile)

        f_vec, msg_f = select_known_positions(f_dict, max_known=512, n=1024)
        g_vec, msg_g = select_known_positions(g_dict, max_known=512, n=1024)

        if msg_f != "ok" or msg_g != "ok":
            print(f"Experiment {i}: skipped (f: {msg_f}, g: {msg_g})")
            results.append(["SKIP"])
            continue
        h_vec = h_list[i]

        f_full, msg = solve_unknown_f(g_vec, f_vec, h_vec, 12289)

        if f_full is None:
            print(f"Experiment {i}: failed ({msg})")
            results.append(["FAIL"])
        else:
            print(f"Experiment {i}: success ({msg})")
            # results.append(f_full)
            f_recovered = f_vec[:]
            unk_indices = [i for i, v in enumerate(f_recovered) if v is None]
            for idx, val in zip(unk_indices, f_full):
                f_recovered[idx] = val
            results.append(f_recovered)


    with open(out_csv_path, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        for row in results:
            writer.writerow(row)

if __name__ == "__main__":
    run_experiments(
        f_dir="./data_k_guess_1024",    # The directory where f_guess_i.pkl is located
        g_dir="./data_k_guess_1024",    # The directory where g_guess_i.pkl is located
        h_csv_path="./k_guess_data/Falcon_h_1024_1000.csv",
        out_csv_path="./data_k_guess_1024/f_results_1024.csv",
        num=100
    )
