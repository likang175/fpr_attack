import pickle
import csv
import os
import numpy as np
import time

# ==== 引入 C 模块 ====
import gaussian_solver as gs

# ==== 辅助函数 ====

def select_known_positions(
    d: dict,
    max_known: int = 512,
    n: int = 1024
):
    """
    将字典转换为带掩码的numpy数组
    """
    arr = np.zeros(n, dtype=np.int32)
    mask = np.zeros(n, dtype=bool)

    count = 0
    for k in sorted(d.keys()):
        if k < n:
            arr[k] = d[k]
            mask[k] = True
            count += 1

    if count < max_known:
        return arr, mask, f"not enough known values: only {count}/{max_known}"
    return arr, mask, "ok"


def mask_lists(a_arr, a_mask, b_arr, b_mask, falcon_n):
    """
    限制已知值的数量到 falcon_n/2
    """
    count_a_total = a_mask.sum()
    count_b_total = b_mask.sum()

    if count_a_total > round(falcon_n/2) and count_b_total > round(falcon_n/2):
        # 处理 a - 保留前 falcon_n/2 个已知值
        known_indices_a = np.where(a_mask)[0]
        new_a_mask = np.zeros_like(a_mask)
        new_a_mask[known_indices_a[:round(falcon_n/2)]] = True

        # 处理 b
        known_indices_b = np.where(b_mask)[0]
        new_b_mask = np.zeros_like(b_mask)
        new_b_mask[known_indices_b[:round(falcon_n/2)]] = True

        return a_arr, new_a_mask, b_arr, new_b_mask
    else:
        return a_arr, a_mask, b_arr, b_mask


def solve_unknown_f_c(g_arr, g_mask, f_arr, f_mask, h_arr, MOD=12289):
    """
    使用 C 模块求解未知的 f 系数

    Args:
        g_arr: numpy array of int32, g 系数
        g_mask: numpy array of bool, g 的掩码 (True=已知)
        f_arr: numpy array of int32, f 系数
        f_mask: numpy array of bool, f 的掩码 (True=已知)
        h_arr: numpy array of int32, h 系数
        MOD: 模数 (默认 12289)

    Returns:
        (x_sol, msg): 解向量和状态消息
    """
    start = time.time()
    success, result, message = gs.solve_unknown_f(g_arr, g_mask, f_arr, f_mask, h_arr, MOD)
    end = time.time()

    if success:
        return result, message, end-start
    else:
        return None, message, end-start


def run_experiments(f_dir: str, g_dir: str, h_csv_path: str, out_csv_path: str, row_indices, falcon_n):
    results = []

    # 读取 h.csv
    h_dict = {}
    with open(h_csv_path, newline="") as csvfile:
        reader = csv.reader(csvfile)
        for i, row in enumerate(reader):
            if i in row_indices:
                h_dict[i] = list(map(int, row))

    # 确保每个 index 都读到
    assert all(idx in h_dict for idx in row_indices)

    # 遍历索引
    used_time = 0
    for idx in row_indices:
        # 加载 f,g dict
        with open(os.path.join(f_dir, f"f_{idx}_guess.pkl"), "rb") as ffile:
            f_dict = pickle.load(ffile)
        with open(os.path.join(g_dir, f"g_{idx}_guess.pkl"), "rb") as gfile:
            g_dict = pickle.load(gfile)

        # 转换为 numpy 数组和掩码
        f_arr, f_mask, msg_f = select_known_positions(f_dict, max_known=round(falcon_n/2), n=falcon_n)
        g_arr, g_mask, msg_g = select_known_positions(g_dict, max_known=round(falcon_n/2), n=falcon_n)

        # 应用掩码限制
        f_arr, f_mask, g_arr, g_mask = mask_lists(f_arr, f_mask, g_arr, g_mask, falcon_n)

        # 转换 h 为 numpy 数组
        h_arr = np.array(h_dict[idx], dtype=np.int32)

        # 使用 C 模块求解
        f_solution, msg, time_solve = solve_unknown_f_c(g_arr, g_mask, f_arr, f_mask, h_arr, 12289)
        used_time += time_solve

        if f_solution is None:
            print(f"Experiment {idx}: failed ({msg})")
            results.append(["FAIL"])
        else:
            print(f"Experiment {idx}: success ({msg})")

            # 重建完整的 f 向量
            f_recovered = f_arr.copy()
            unknown_indices = np.where(~f_mask)[0]

            # 将求解的未知值填入
            for i, val in zip(unknown_indices, f_solution):
                f_recovered[i] = val

            results.append(f_recovered.tolist())

    # 写出结果
    with open(out_csv_path, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        for row in results:
            writer.writerow(row)
    print(f"average solving the equation is {used_time/len(row_indices)}")


# ==== 使用示例 ====
if __name__ == "__main__":

    # sets_both指的是成功恢复出512个系数的key的索引
    sets_both = np.load("./data_k_guess_isd/Falcon512/key_idx_list.npy")
    # sets_both = np.load("./data_k_guess_isd/Falcon1024/key_idx_list.npy")
    print(sets_both)
    print(len(sets_both))
    sets_both = np.tile(sets_both, 200)
    print(len(sets_both))
    test_len = 10000
    falcon_n = 512
    # falcon_n = 1024

    # Falcon-512 示例 (注释掉)
    run_experiments(
        f_dir="./data_k_guess_isd/Falcon512",
        g_dir="./data_k_guess_isd/Falcon512",
        h_csv_path="./k_guess_data/Falcon_h_512_1000.csv",
        out_csv_path="./data_k_guess_isd/f_results_512_isd_c.csv",
        row_indices=sets_both[:test_len],
        falcon_n=512
    )

    # Falcon-1024
    # run_experiments(
    #     f_dir="./data_k_guess_isd/Falcon1024",
    #     g_dir="./data_k_guess_isd/Falcon1024",
    #     h_csv_path="./k_guess_data/Falcon_h_1024_1000.csv",
    #     out_csv_path="./data_k_guess_isd/f_results_1024_isd_c.csv",
    #     row_indices=sets_both[:test_len],
    #     falcon_n=falcon_n
    # )
