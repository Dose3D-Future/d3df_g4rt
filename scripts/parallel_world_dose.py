#!/usr/bin/env python3
"""Plot slices and line profiles from a G4RT voxel-dose CSV."""
from __future__ import annotations
import argparse
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

AXES = {"x": ("X [mm]", "Voxel IdX"), "y": ("Y [mm]", "Voxel IdY"), "z": ("Z [mm]", "Voxel IdZ")}

def load(path):
    df = pd.read_csv(path, comment="#")
    required = {"X [mm]", "Y [mm]", "Z [mm]", "Dose [Gy]"}
    if missing := required.difference(df.columns):
        raise ValueError(f"Missing CSV columns: {', '.join(sorted(missing))}")
    return df

def nearest(df, axis, requested):
    values = np.sort(df[AXES[axis][0]].dropna().unique())
    return float(values[np.argmin(abs(values - requested))])

def finish(fig, output, no_show):
    fig.tight_layout()
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(output, dpi=180, bbox_inches="tight")
        print(f"Saved {output}")
    if not no_show:
        plt.show()

def plot_slice(args, df):
    coord_col, index_col = AXES[args.axis]
    if args.index is not None:
        plane = df[df[index_col] == args.index]
        if plane.empty: raise ValueError(f"No voxels at {args.axis.upper()} index {args.index}")
        selected, note = float(plane[coord_col].median()), f"index {args.index}"
    else:
        values = np.sort(df[coord_col].dropna().unique())
        requested = values[len(values)//2] if args.coordinate is None else args.coordinate
        selected = float(values[np.argmin(abs(values-requested))])
        plane, note = df[np.isclose(df[coord_col], selected)], "nearest voxel plane"
    horizontal, vertical = [a for a in "xyz" if a != args.axis]
    xcol, ycol = AXES[horizontal][0], AXES[vertical][0]
    image = plane.pivot_table(index=ycol, columns=xcol, values="Dose [Gy]", aggfunc="sum").sort_index().sort_index(axis=1)
    values = image.to_numpy(float)
    if args.fill_missing is not None: values = np.nan_to_num(values, nan=args.fill_missing)
    if args.normalize and np.nanmax(values) > 0: values *= 100/np.nanmax(values)
    fig, ax = plt.subplots(figsize=(8, 6))
    extent = [image.columns.min(), image.columns.max(), image.index.min(), image.index.max()]
    im = ax.imshow(values, origin="lower", extent=extent, aspect="equal", interpolation="nearest", cmap=args.cmap)
    fig.colorbar(im, ax=ax, label="% of slice maximum" if args.normalize else "Dose [Gy]")
    ax.set(xlabel=xcol, ylabel=ycol, title=f"Dose slice: {args.axis.upper()} = {selected:g} mm ({note})")
    finish(fig, args.output, args.no_show)

def plot_profile(args, df):
    fixed_axes = [a for a in "xyz" if a != args.axis]
    selected = {a: nearest(df, a, value) for a, value in zip(fixed_axes, args.at)}
    profile = df
    for axis, value in selected.items(): profile = profile[np.isclose(profile[AXES[axis][0]], value)]
    coord = AXES[args.axis][0]
    profile = profile.groupby(coord, as_index=False)["Dose [Gy]"].sum().sort_values(coord)
    if profile.empty: raise ValueError("No voxels found for requested profile")
    dose = profile["Dose [Gy]"].to_numpy()
    if args.normalize and dose.max(initial=0) > 0: dose = 100*dose/dose.max()
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(profile[coord], dose, marker="o", markersize=3)
    fixed = ", ".join(f"{a.upper()}={v:g} mm" for a, v in selected.items())
    ax.set(xlabel=coord, ylabel="Dose [% of profile maximum]" if args.normalize else "Dose [Gy]",
           title=f"Dose profile along {args.axis.upper()} at {fixed}")
    ax.grid(alpha=.3)
    finish(fig, args.output, args.no_show)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, help="G4RT *_voxel.csv file")
    commands = parser.add_subparsers(dest="command", required=True)
    ps = commands.add_parser("slice", help="plot a plane normal to an axis")
    ps.add_argument("--axis", choices=AXES, default="z")
    selection = ps.add_mutually_exclusive_group()
    selection.add_argument("--coordinate", type=float, help="coordinate in mm (nearest plane is used)")
    selection.add_argument("--index", type=int, help="exact voxel index")
    ps.add_argument("--fill-missing", type=float, metavar="DOSE", help="fill sparse/unvisited voxels, commonly 0")
    ps.add_argument("--cmap", default="inferno"); ps.add_argument("--normalize", action="store_true"); ps.add_argument("--output", type=Path)
    ps.add_argument("--no-show", action="store_true", help="save without opening an interactive window")
    pp = commands.add_parser("profile", help="plot a line along an axis")
    pp.add_argument("--axis", choices=AXES, default="z")
    pp.add_argument("--at", nargs=2, type=float, default=(0., 0.), metavar=("C1", "C2"), help="coordinates on remaining axes in XYZ order")
    pp.add_argument("--normalize", action="store_true"); pp.add_argument("--output", type=Path)
    pp.add_argument("--no-show", action="store_true", help="save without opening an interactive window")
    args = parser.parse_args(); df = load(args.csv)
    plot_slice(args, df) if args.command == "slice" else plot_profile(args, df)

if __name__ == "__main__": main()
