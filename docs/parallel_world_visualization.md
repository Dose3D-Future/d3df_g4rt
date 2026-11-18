# Parallel-world dose visualization

## Python slices and profiles

The tool reads sparse G4RT voxel CSV files and ignores their `#` metadata.

```bash
mamba run -n g4rt_devel python scripts/parallel_world_dose.py \
  output/job/sim/cp/cp_parentworlddose_voxel.csv \
  slice --axis z --coordinate 0 --fill-missing 0 --output dose_z0.png

mamba run -n g4rt_devel python scripts/parallel_world_dose.py \
  output/job/sim/cp/cp_parentworlddose_voxel.csv \
  slice --axis y --index 30 --normalize --output dose_y30.png

mamba run -n g4rt_devel python scripts/parallel_world_dose.py \
  output/job/sim/cp/cp_parentworlddose_voxel.csv \
  profile --axis z --at 0 0 --normalize --output pdd.png
```

For `profile`, `--at` gives coordinates on the remaining axes in XYZ order
with the profile axis omitted. Thus a Z profile uses `--at X Y`; an X profile
uses `--at Y Z`. The nearest available voxel centres are selected.

Sparse, unvisited voxels remain transparent unless `--fill-missing 0` is used.
An absent voxel is not necessarily a voxel with a recorded zero-dose hit.

The plot window remains open after saving. Add `--no-show` for batch jobs and
CI where only the output image is needed.

## ROOT dose QA

This produces a requested 2D slice, central line profile, and log-scale voxel
dose distribution:

```text
mamba run -n g4rt_devel root -l
root [0] .x scripts/root/parallel_world_dose.C("output/job/sim/cp/cp_parentworlddose_voxel.csv", "z", 0.0, "dose_qa.pdf")
```

Do not add ROOT's `-q` option when working interactively: `-q` explicitly tells
ROOT to exit after running the macro. The canvas and its objects are retained
by the macro and remain usable at the ROOT prompt.

## ROOT event-energy QA

This plots each supported branch present in the event TTree: deposited energy,
mean deposited energy, track energy, and primary energy. Availability follows
the TOML `NTupleAnalysis`, `StoreEnergies`, and `StorePrimaries` options.

```text
mamba run -n g4rt_devel root -l
root [0] .x scripts/root/ntuple_energy.C("output/job/job.root", "ParentWorldDoseTTree", "energy_qa.pdf")
```

These energy plots are simulation QA rather than dose comparison. They are
useful for spotting source-energy changes, cutoffs, and unexpectedly large
single-hit deposits.
