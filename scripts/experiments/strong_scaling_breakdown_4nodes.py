#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt


def main():

    df = pd.read_csv("results/strong_mpi+omp.csv")

    NODES_TO_PLOT = 4   

    required = {
        "algo", "np", "rpn",
        "sort_time", "part_time", "merge_time"
    }
    missing = required - set(df.columns)
    if missing:
        raise SystemExit(f"Colonne mancanti nel CSV: {sorted(missing)}")

    # numeric conversion
    num_cols = [
        "np", "rpn",
        "sort_time", "part_time", "merge_time"
    ]
    for c in num_cols:
        df[c] = pd.to_numeric(df[c], errors="coerce")

    df["nodes"] = (df["np"] / df["rpn"]).astype(int)

    df = df.dropna(subset=num_cols + ["nodes"])

    if df.empty:
        raise SystemExit("DataFrame vuoto")

    # ---- filtro nodi ----
    df = df[df["nodes"] == NODES_TO_PLOT]

    if df.empty:
        raise SystemExit(f"Nessun dato per nodes={NODES_TO_PLOT}")

    # ---- media sulle ripetizioni ----
    group_cols = ["algo", "rpn"]
    agg_df = df.groupby(group_cols, as_index=False)[
        ["sort_time", "part_time", "merge_time"]
    ].mean()

    agg_df = agg_df.sort_values("rpn")

    # ---- plot ----
    plt.figure(figsize=(8,5))

    plt.plot(
        agg_df["rpn"],
        agg_df["sort_time"],
        marker="o",
        linewidth=2,
        label="sort_time"
    )

    plt.plot(
        agg_df["rpn"],
        agg_df["part_time"],
        marker="o",
        linewidth=2,
        label="part_time"
    )

    plt.plot(
        agg_df["rpn"],
        agg_df["merge_time"],
        marker="o",
        linewidth=2,
        label="merge_time"
    )

    plt.xlabel("rpn (ranks per node)")
    plt.ylabel("time [s]")
    plt.title(f"Time breakdown vs rpn (nodes={NODES_TO_PLOT})")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()