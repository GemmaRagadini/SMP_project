#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt


def main():
    df = pd.read_csv("results/strong_mpi+omp.csv")

    required = {"np", "rpn", "algo", "kernel_time"}
    missing = required - set(df.columns)
    if missing:
        raise SystemExit(f"Colonne mancanti nel CSV: {sorted(missing)}")

    # Coerce numeric
    df["rpn"] = pd.to_numeric(df["rpn"], errors="coerce")
    df["kernel_time"] = pd.to_numeric(df["kernel_time"], errors="coerce")
    df["np"] = pd.to_numeric(df["np"], errors="coerce")

    df = df.dropna(subset=["np", "rpn", "kernel_time", "algo"])
    df["nodes"] = (df["np"] / df["rpn"]).astype(int)

    df = df.dropna(subset=["nodes"])
    if df.empty:
        raise SystemExit("DataFrame vuoto")

    # Aggregate across repetitions per (algo, nodes, rpn)
    group_cols = ["algo", "nodes", "rpn"]
    agg_df = df.groupby(group_cols, as_index=False)["kernel_time"].mean()
    agg_df = agg_df.sort_values(["rpn", "nodes"])

    # TIME vs NODES
    plt.figure(figsize=(8, 5))

    algos = sorted(agg_df["algo"].unique().tolist())
    for algo in algos:
        sub_a = agg_df[agg_df["algo"] == algo]
        for rpn, sub in sub_a.groupby("rpn"):
            sub = sub.sort_values("nodes")
            label = f"rpn={int(rpn)}"
            plt.plot(sub["nodes"], sub["kernel_time"], marker="o", linewidth=2, label=label)

    plt.xlabel("nodes")
    plt.ylabel("kernel_time [s]")
    plt.title("Strong scaling: time vs nodes (one curve per rpn)")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    # SPEEDUP vs nodes for each rpn 
    plt.figure(figsize=(8, 5))
    any_speedup = False

    speedup_series = []  

    for algo in algos:
        sub_a = agg_df[agg_df["algo"] == algo]
        for rpn, sub in sub_a.groupby("rpn"):
            sub = sub.sort_values("nodes")
            base = sub[sub["nodes"] == 1]["kernel_time"]
            if base.empty:
                continue
            t1 = float(base.iloc[0])
            speedup = t1 / sub["kernel_time"]
            label = f"rpn={int(rpn)} speedup"
            plt.plot(sub["nodes"], speedup, marker="o", linewidth=2, label=label)
            any_speedup = True

            tmp = sub[["algo", "nodes", "rpn"]].copy()
            tmp["speedup"] = speedup.values
            speedup_series.append(tmp)

    if any_speedup:
        plt.axhline(1.0, linestyle="--", linewidth=1.5, label="baseline (nodes=1)")
        plt.xlabel("nodes")
        plt.ylabel("speedup (T1 / Tp)")
        plt.title("Strong scaling: speedup vs nodes")
        plt.grid(True)
        plt.legend()
        plt.tight_layout()
    else:
        plt.close()

    # EFFICIENCY vs nodes (eff = speedup / nodes)
    if speedup_series:
        eff_df = pd.concat(speedup_series, ignore_index=True)
        eff_df["efficiency"] = eff_df["speedup"] / eff_df["nodes"]

        plt.figure(figsize=(8, 5))
        for algo in algos:
            sub_a = eff_df[eff_df["algo"] == algo]
            for rpn, sub in sub_a.groupby("rpn"):
                sub = sub.sort_values("nodes")
                plt.plot(
                    sub["nodes"],
                    sub["efficiency"],
                    marker="o",
                    linewidth=2,
                    label=f"rpn={int(rpn)} eff"
                )

        plt.xlabel("nodes")
        plt.ylabel("efficiency (speedup / nodes)")
        plt.title("Strong scaling: efficiency vs nodes")
        plt.grid(True)
        plt.legend()
        plt.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()