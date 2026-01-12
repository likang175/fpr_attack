import pickle
import csv
import os
import numpy as np
import time

# ==== 引入 solve_unknown_f 和依赖 ====
from typing import List, Optional, Tuple

def modinv(a: int, q: int) -> Optional[int]:
    """计算 a 在 mod q 下的乘法逆元。"""
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
    """将模 q 下的 x 映射到 [-q//2, q//2] 区间"""
    x = x % q
    if x > q // 2:
        return x - q
    return x

def gaussian_mod_q_solve(A: List[List[int]], b: List[int], q: int):
    """
    解 A x ≡ b (mod q)，其中 A 是 n×n 矩阵，b 是长度 n 的向量。
    返回 x（解向量）和状态消息。
    """
    import copy
    n = len(A)
    assert all(len(row) == n for row in A), "A 必须是方阵"
    assert len(b) == n, "b 的长度必须与 A 的行数一致"

    # 构造增广矩阵
    M = [row[:] + [bb] for row, bb in zip(copy.deepcopy(A), b)]

    for col in range(n):
        # 查找当前列中非零的主元
        pivot_row = None
        for row in range(col, n):
            if M[row][col] % q != 0:
                inv = modinv(M[row][col], q)
                if inv is not None:
                    pivot_row = row
                    break
        if pivot_row is None:
            return None, f"no pivot in column {col}, singular matrix"

        # 交换当前行和主元行
        M[col], M[pivot_row] = M[pivot_row], M[col]

        # 归一化主元行
        pivot_val = M[col][col] % q
        inv_pivot = modinv(pivot_val, q)
        if inv_pivot is None:
            return None, f"modular inverse not found for pivot {pivot_val}"
        for j in range(col, n + 1):
            M[col][j] = (M[col][j] * inv_pivot) % q

        # 消元其他行
        for row in range(n):
            if row == col:
                continue
            factor = M[row][col] % q
            for j in range(col, n + 1):
                M[row][j] = (M[row][j] - factor * M[col][j]) % q

    # 提取解
    # x = [M[i][n] % q for i in range(n)]
    x = [to_centered_mod(M[i][n], q) for i in range(n)]
    return x, "ok"


def solve_unknown_f(g: List[Optional[int]], f: List[Optional[int]], h: List[int], MOD: int = 12289):
    n = len(h)
    I_g = [i for i, val in enumerate(g) if val is not None]
    I_f_known = [i for i, val in enumerate(f) if val is not None]
    I_f_unk = [i for i, val in enumerate(f) if val is None]
    # if len(I_g)>256 and len(I_f_known)>256:
    #     I_g = I_g[:256]
    #     I_f_known = I_f_known[:256]
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

# ==== 辅助函数 ====

def select_known_positions(
    d: dict,
    max_known: int = 512,
    n: int = 1024
) -> Tuple[List[Optional[int]], str]:

    lst = [None] * n
    count = 0

    # Step 1: 选取优先集合
    for k in sorted(d.keys()):
        # if count >= max_known:
        #     break
        lst[k] = d[k]
        count += 1

    if count < max_known:
        return lst, f"not enough known values: only {count}/256"
    return lst, "ok"

# ==== 主流程 ====
def mask_lists(a, b, falcon_n):
    # 统计整数数量
    count_a_total = sum(1 for x in a if x is not None)
    count_b_total = sum(1 for x in b if x is not None)

    if count_a_total > round(falcon_n/2) and count_b_total > round(falcon_n/2):
        # 处理 a
        kept_a = 0
        new_a = []
        for x in a:
            if x is not None and kept_a < round(falcon_n/2):
                new_a.append(x)
                kept_a += 1
            elif x is not None:
                new_a.append(None)  # 超过 256 个整数后改成 None
            else:
                new_a.append(None)  # 原本就是 None
        # 处理 b
        kept_b = 0
        new_b = []
        for x in b:
            if x is not None and kept_b < round(falcon_n/2):
                new_b.append(x)
                kept_b += 1
            elif x is not None:
                new_b.append(None)
            else:
                new_b.append(None)
        return new_a, new_b
    else:
        return a, b

def run_experiments(f_dir: str, g_dir: str, h_csv_path: str, out_csv_path: str, row_indices, falcon_n):
    results = []
    # 读取 h.csv
    # 读取 h.csv 中指定行
    h_dict = {}
    # row_indices = set(row_indices)  # 转成集合，查找更快
    with open(h_csv_path, newline="") as csvfile:
        reader = csv.reader(csvfile)
        for i, row in enumerate(reader):
            if i in row_indices:
                h_dict[i] = list(map(int, row))

    # 确保每个 index 都读到
    assert all(idx in h_dict for idx in row_indices)

    # 遍历 sets_both 中的索引
    for idx in row_indices:
        # 加载 f,g dict
        with open(os.path.join(f_dir, f"f_{idx}_guess.pkl"), "rb") as ffile:
            f_dict = pickle.load(ffile)
        with open(os.path.join(g_dir, f"g_{idx}_guess.pkl"), "rb") as gfile:
            g_dict = pickle.load(gfile)

        f_vec, msg_f = select_known_positions(f_dict, max_known=round(falcon_n/2), n=falcon_n)
        g_vec, msg_g = select_known_positions(g_dict, max_known=round(falcon_n/2), n=falcon_n)

        f_vec, g_vec = mask_lists(f_vec, g_vec, falcon_n)

        # if msg_f != "ok" or msg_g != "ok":
        #     print(f"Experiment {idx}: skipped (f: {msg_f}, g: {msg_g})")
        #     results.append(["SKIP"])
        #     continue

        h_vec = h_dict[idx]
        f_full, msg = solve_unknown_f(g_vec, f_vec, h_vec, 12289)

        if f_full is None:
            print(f"Experiment {idx}: failed ({msg})")
            results.append(["FAIL"])
        else:
            print(f"Experiment {idx}: success ({msg})")
            f_recovered = f_vec[:]
            unk_indices = [i for i, v in enumerate(f_recovered) if v is None]
            for i2, val in zip(unk_indices, f_full):
                f_recovered[i2] = val
            results.append(f_recovered)

    # 写出
    with open(out_csv_path, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        for row in results:
            writer.writerow(row)

# ==== 使用示例 ====
if __name__ == "__main__":

    # sets_both指的是成功恢复出512个系数的key的索引
    # sets_both = np.load("./data_k_guess_isd/Falcon512/key_idx_list.npy")
    sets_both = np.load("./data_k_guess_isd/Falcon1024/key_idx_list.npy")
    print(sets_both)
    print(len(sets_both))
    print(sets_both)
    test_len = 10
    # falcon_n = 512
    falcon_n = 1024

    start = time.time()
    # run_experiments(
    #     f_dir="./data_k_guess_isd/Falcon512",    # 存放 f_guess_i.pkl 的目录
    #     g_dir="./data_k_guess_isd/Falcon512",    # 存放 g_guess_i.pkl 的目录
    #     h_csv_path="./k_guess_data/Falcon_h_512_1000.csv",
    #     out_csv_path="./data_k_guess_isd/f_results_512_isd_831_10.csv",
    #     row_indices = sets_both[:test_len],
    #     falcon_n = falcon_n
    # )
    run_experiments(
        f_dir="./data_k_guess_isd/Falcon1024",  # 存放 f_guess_i.pkl 的目录
        g_dir="./data_k_guess_isd/Falcon1024",  # 存放 g_guess_i.pkl 的目录
        h_csv_path="./k_guess_data/Falcon_h_1024_1000.csv",
        out_csv_path="./data_k_guess_isd/f_results_1024_isd_1125_10.csv",
        row_indices=sets_both[:test_len],
        falcon_n=falcon_n
    )
    end = time.time()
    print(f"average solving the equation is {(end-start)/test_len}")

