#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt


def main():
    df = pd.read_csv('results/single_node_test_B.csv')

    required_cols = {"algo", "threads", "sort_time"}
    missing = required_cols - set(df.columns)
    if missing:
        raise SystemExit(f"Colonne mancanti nel CSV: {sorted(missing)}")

    df["threads"] = pd.to_numeric(df["threads"], errors="coerce")
    df["sort_time"] = pd.to_numeric(df["sort_time"], errors="coerce")
    df = df.dropna(subset=["threads", "sort_time", "algo"])

    # Media di time per (algo, threads)
    means = (
        df.groupby(["algo", "threads"], as_index=False)["sort_time"]
        .mean()
        .rename(columns={"sort_time": "sort_time_mean"})
    )

    # Curve
    ff = means[means["algo"] == "ff"].sort_values("threads")
    omp = means[means["algo"] == "omp"].sort_values("threads")

    # Seq baseline: prendo il miglior tempo
    seq_rows = df[df["algo"] == "seq"]
    seq_value = float(seq_rows["sort_time"].min()) if not seq_rows.empty else None

    x_threads = sorted(means["threads"].unique().tolist())
    if not x_threads:
        raise SystemExit("Nessun valore threads nel CSV")

    # TIME VS THREADS
    plt.figure(figsize=(8, 5))

    if not ff.empty:
        plt.plot(ff["threads"], ff["sort_time_mean"], marker="o",
                 linewidth=2, label="ff")
    if not omp.empty:
        plt.plot(omp["threads"], omp["sort_time_mean"], marker="o",
                 linewidth=2, label="omp")

    if seq_value is not None:
        plt.hlines(seq_value, xmin=min(x_threads), xmax=max(x_threads),
                   linewidth=2, label=f"seq = {seq_value:.6g}")

    plt.xlabel("threads")
    plt.ylabel("time [s]")
    plt.title("Single node: time vs threads")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    # SPEEDUP VS THREADS
    # speedup = T_seq / T_algo
    if seq_value is not None:
        plt.figure(figsize=(8, 5))

        if not ff.empty:
            speedup_ff = seq_value / ff["sort_time_mean"]
            plt.plot(ff["threads"], speedup_ff, marker="o",
                     linewidth=2, label="ff speedup")

        if not omp.empty:
            speedup_omp = seq_value / omp["sort_time_mean"]
            plt.plot(omp["threads"], speedup_omp, marker="o",
                     linewidth=2, label="omp speedup")

        plt.axhline(1.0, linestyle="--", linewidth=1.5, label="seq baseline")
        plt.xlabel("threads")
        plt.ylabel("speedup (T_seq / T_algo)")
        plt.title("Single node: speedup vs threads")
        plt.grid(True)
        plt.legend()
        plt.tight_layout()

        # EFFICIENCY VS THREADS
        # efficiency = speedup / threads
        plt.figure(figsize=(8, 5))

        if not ff.empty:
            eff_ff = (seq_value / ff["sort_time_mean"]) / ff["threads"]
            plt.plot(ff["threads"], eff_ff, marker="o",
                     linewidth=2, label="ff efficiency")

        if not omp.empty:
            eff_omp = (seq_value / omp["sort_time_mean"]) / omp["threads"]
            plt.plot(omp["threads"], eff_omp, marker="o",
                     linewidth=2, label="omp efficiency")

        plt.xlabel("threads")
        plt.ylabel("efficiency (speedup / threads)")
        plt.title("Single node: efficiency vs threads")
        plt.grid(True)
        plt.legend()
        plt.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()