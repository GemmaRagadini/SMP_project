#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt


CSV_PATH = "results/weak_mpi+omp.csv"
BREAKDOWN_RPN = 4


def main():
    df = pd.read_csv(CSV_PATH)

    required = {
        "algo","n","p","threads","np",
        "build_time","sort_time","part_time","merge_time","total_time","kernel_time",
        "rpn"
    }
    missing = required - set(df.columns)
    if missing:
        raise SystemExit(f"Colonne mancanti nel CSV: {sorted(missing)}")

    num_cols = [
        "n","p","threads","np","rpn",
        "build_time","sort_time","part_time","merge_time","total_time","kernel_time"
    ]
    for c in num_cols:
        df[c] = pd.to_numeric(df[c], errors="coerce")

    df = df.dropna(subset=list(required))
    if df.empty:
        raise SystemExit("DataFrame vuoto")

    # derive nodes and N_per_rank 
    df["nodes"] = (df["np"] / df["rpn"]).astype(int)
    df["N_per_rank"] = df["n"] / df["np"]

    # mean per (algo, nodes, rpn)
    group_cols = ["algo", "nodes", "rpn"]
    agg = df.groupby(group_cols, as_index=False)[
        ["kernel_time","sort_time","part_time","merge_time","build_time","N_per_rank"]
    ].mean()

    agg = agg.sort_values(["rpn","nodes"])


    for (algo, rpn), sub in agg.groupby(["algo", "rpn"]):
        vmin = sub["N_per_rank"].min()
        vmax = sub["N_per_rank"].max()
        print(f"  algo={algo} rpn={int(rpn)}  N_per_rank in [{vmin:.1f}, {vmax:.1f}]")

    # kernel_time vs nodes (one curve per rpn)
    plt.figure(figsize=(8, 5))

    algos = sorted(agg["algo"].unique().tolist())
    for algo in algos:
        sub_a = agg[agg["algo"] == algo]
        for rpn, sub in sub_a.groupby("rpn"):
            sub = sub.sort_values("nodes")
            plt.plot(
                sub["nodes"], sub["kernel_time"],
                marker="o", linewidth=2,
                label=f"{algo} rpn={int(rpn)}"
            )

    plt.xlabel("nodes")
    plt.ylabel("time [s]")
    plt.title("Weak scaling: time vs nodes")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    # weak scaling efficiency on kernel_time 
    plt.figure(figsize=(8, 5))
    any_eff = False

    for algo in algos:
        sub_a = agg[agg["algo"] == algo]
        for rpn, sub in sub_a.groupby("rpn"):
            sub = sub.sort_values("nodes")

            base = sub[sub["nodes"] == 1]["kernel_time"]
            if base.empty:
                continue
            t1 = float(base.iloc[0])

            eff_weak = t1 / sub["kernel_time"]
            plt.plot(
                sub["nodes"], eff_weak,
                marker="o", linewidth=2,
                label=f"{algo} rpn={int(rpn)}"
            )
            any_eff = True

    if any_eff:
        plt.xlabel("nodes")
        plt.ylabel("weak efficiency (T1 / Tp)")
        plt.title("Weak scaling: efficiency vs nodes")
        plt.grid(True)
        plt.legend()
        plt.tight_layout()
    else:
        plt.close()

    # breakdown vs nodes for rpn=4 
    sub_b = agg[(agg["rpn"] == BREAKDOWN_RPN)]

    for algo in sorted(sub_b["algo"].unique().tolist()):
        sub = sub_b[sub_b["algo"] == algo].sort_values("nodes")

        plt.figure(figsize=(8, 5))
        plt.plot(sub["nodes"], sub["sort_time"], marker="o", linewidth=2, label="sort_time")
        plt.plot(sub["nodes"], sub["part_time"], marker="o", linewidth=2, label="part_time")
        plt.plot(sub["nodes"], sub["merge_time"], marker="o", linewidth=2, label="merge_time")

        plt.xlabel("nodes")
        plt.ylabel("time [s]")
        plt.title(f"Weak scaling breakdown vs nodes (rpn={BREAKDOWN_RPN})")
        plt.grid(True)
        plt.legend()
        plt.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()