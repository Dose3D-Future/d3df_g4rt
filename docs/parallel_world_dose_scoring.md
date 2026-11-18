# Parallel-world dose scoring

`ParallelWorldDoseScorer` creates an axis-aligned regular grid in a Geant4
parallel world. The grid overlays the mass geometry, so it records energy
deposited in the patient environment and in all physical daughter volumes,
including `D3DDetector` cells.

## Configuration

Add the following section to a job TOML file:

```toml
[ParallelWorldDoseScorer]
Enabled = true

# Requested scoring extent and centre in world coordinates, in mm.
# If any Size component is non-positive, all three sizes and the centre are
# derived from [PatientGeometry].
SizeX = 0.0
SizeY = 0.0
SizeZ = 0.0
CentreX = 0.0
CentreY = 0.0
CentreZ = 0.0

VoxelSizeX = 1.0
VoxelSizeY = 1.0
VoxelSizeZ = 1.0
CollectionName = "ParentWorldDose"
```

When a requested size is not divisible by its voxel size, the number of
voxels is rounded up. The resulting grid remains centred at the configured
centre and may be up to one voxel larger along an axis.

The total number of voxels must fit in a positive 32-bit integer, which is a
Geant4 parameterisation constraint. Event and run hit maps are sparse: only
voxels reached by a scored step consume hit-map memory.

## Output selection

The scorer uses the existing analysis configuration:

- `RunAnalysis = true` accumulates the `ParentWorldDose` run collection and
  writes its voxel dose to CSV and to the run-level ROOT output.
- `NTupleAnalysis = true` creates the event-level ntuple. Its optional columns
  continue to follow `StoreTracks`, `StorePositions`, `StoreEnergies`,
  `StorePrimaries`, `StoreRunInfo`, and `MinimalMode`.
- Both modes can be enabled at the same time.

With run analysis enabled, the CSV name follows the existing convention and
ends with `_<collection-name>_voxel.csv` in lowercase.

## Dose definition

The grid reports a volume-average dose-to-local-medium. Each mass-world step
contributes

```text
step energy deposit / (step material density * voxel volume)
```

This avoids assigning the density of the first material hit to a voxel that
crosses a material boundary. Parallel-world boundaries also constrain the
transport step at voxel faces, avoiding pre-step-position binning across more
than one scoring voxel.

The parallel world has no material of its own and does not replace or modify
the simulation mass geometry.

## Visualization

See [parallel_world_visualization.md](parallel_world_visualization.md) for the
Python slice/profile tool and ROOT dose and energy QA macros.
