#include "ParallelWorldDoseScorer.hh"

#include <cmath>
#include <limits>

#include "ControlPoint.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4PVReplica.hh"
#include "G4SDManager.hh"
#include "Services.hh"
#include "VPatientSD.hh"

namespace {
const G4String kParallelWorldName = "DoseGridParallelWorld";

class ParallelWorldDoseSD final : public VPatientSD {
 public:
  ParallelWorldDoseSD(const G4String& sdName, const G4ThreeVector& centre,
                      const G4String& runCollection, const G4String& hitsCollection,
                      const G4Box& scoringBox, G4int nx, G4int ny, G4int nz)
      : VPatientSD(sdName, centre), m_hitsCollection(hitsCollection) {
    m_id_x = 0;
    m_id_y = 0;
    m_id_z = 0;
    SetStepWiseDose(true);
    AddScoringVolume(runCollection, hitsCollection, scoringBox, nx, ny, nz,
                     G4ThreeVector(), true);
  }

  G4bool ProcessHits(G4Step* step, G4TouchableHistory*) override {
    // Parallel navigation also produces boundary-only steps. They carry no
    // dose and previously dominated the scoring cost for fine grids.
    if (step->GetTotalEnergyDeposit() <= 0.) return false;
    ProcessHitsCollection(m_hitsCollection, step);
    return true;
  }

 private:
  G4String m_hitsCollection;
};
}  // namespace

const G4String& ParallelWorldDoseScorer::WorldName() { return kParallelWorldName; }

bool ParallelWorldDoseScorer::ConfigEnabled() {
  return Service<ConfigSvc>()->GetValue<bool>("ParallelWorldDoseScorer", "Enabled");
}

ParallelWorldDoseScorer::ParallelWorldDoseScorer()
    : G4VUserParallelWorld(WorldName()),
      VPatient("ParallelWorldDoseScorer"),
      Configurable("ParallelWorldDoseScorer") {
  m_parallelSD.Put(nullptr);
  Configure();
}

void ParallelWorldDoseScorer::Configure() {
  DefineUnit<bool>("Enabled");
  DefineUnit<double>("SizeX");
  DefineUnit<double>("SizeY");
  DefineUnit<double>("SizeZ");
  DefineUnit<double>("CentreX");
  DefineUnit<double>("CentreY");
  DefineUnit<double>("CentreZ");
  DefineUnit<double>("VoxelSizeX");
  DefineUnit<double>("VoxelSizeY");
  DefineUnit<double>("VoxelSizeZ");
  DefineUnit<std::string>("CollectionName");
  Configurable::DefaultConfig();
}

void ParallelWorldDoseScorer::DefaultConfig(const std::string& unit) {
  if (unit == "Label") thisConfig()->SetValue(unit, std::string("Parallel world dose grid"));
  if (unit == "Enabled") thisConfig()->SetTValue<bool>(unit, false);
  // A zero size derives the scoring extent and centre from PatientGeometry.
  if (unit == "SizeX" || unit == "SizeY" || unit == "SizeZ" ||
      unit == "CentreX" || unit == "CentreY" || unit == "CentreZ")
    thisConfig()->SetTValue<double>(unit, 0.);
  if (unit == "VoxelSizeX" || unit == "VoxelSizeY" || unit == "VoxelSizeZ")
    thisConfig()->SetTValue<double>(unit, 1.);
  if (unit == "CollectionName")
    thisConfig()->SetTValue<std::string>(unit, std::string("ParentWorldDose"));
}

bool ParallelWorldDoseScorer::IsEnabled() const {
  return thisConfig()->GetValue<bool>("Enabled");
}

void ParallelWorldDoseScorer::ResolveGrid() {
  m_size = G4ThreeVector(thisConfig()->GetValue<double>("SizeX"),
                         thisConfig()->GetValue<double>("SizeY"),
                         thisConfig()->GetValue<double>("SizeZ"));
  m_voxelSize = G4ThreeVector(thisConfig()->GetValue<double>("VoxelSizeX"),
                              thisConfig()->GetValue<double>("VoxelSizeY"),
                              thisConfig()->GetValue<double>("VoxelSizeZ"));
  m_centre = G4ThreeVector(thisConfig()->GetValue<double>("CentreX"),
                           thisConfig()->GetValue<double>("CentreY"),
                           thisConfig()->GetValue<double>("CentreZ"));

  if (m_size.x() <= 0. || m_size.y() <= 0. || m_size.z() <= 0.) {
    m_size = G4ThreeVector(
        Service<ConfigSvc>()->GetValue<double>("PatientGeometry", "EnviromentSizeX"),
        Service<ConfigSvc>()->GetValue<double>("PatientGeometry", "EnviromentSizeY"),
        Service<ConfigSvc>()->GetValue<double>("PatientGeometry", "EnviromentSizeZ"));
    m_centre = G4ThreeVector(
        Service<ConfigSvc>()->GetValue<double>("PatientGeometry", "PatientIsocentreX"),
        Service<ConfigSvc>()->GetValue<double>("PatientGeometry", "PatientIsocentreY"),
        Service<ConfigSvc>()->GetValue<double>("PatientGeometry", "PatientIsocentreZ"));
  }

  if (m_voxelSize.x() <= 0. || m_voxelSize.y() <= 0. || m_voxelSize.z() <= 0.) {
    G4Exception("ParallelWorldDoseScorer", "InvalidVoxelSize", FatalErrorInArgument,
                "VoxelSize components must be greater than zero.");
  }

  // Cover the requested extent completely. The actual extent is reported and
  // may be up to one voxel larger on each axis.
  m_nX = static_cast<G4int>(std::ceil(m_size.x() / m_voxelSize.x()));
  m_nY = static_cast<G4int>(std::ceil(m_size.y() / m_voxelSize.y()));
  m_nZ = static_cast<G4int>(std::ceil(m_size.z() / m_voxelSize.z()));
  const auto count = static_cast<long long>(m_nX) * m_nY * m_nZ;
  if (count <= 0 || count > std::numeric_limits<G4int>::max()) {
    G4Exception("ParallelWorldDoseScorer", "InvalidVoxelCount", FatalErrorInArgument,
                "Dose grid voxel count must fit in a positive G4int.");
  }
  m_actualSize = G4ThreeVector(m_nX * m_voxelSize.x(),
                               m_nY * m_voxelSize.y(),
                               m_nZ * m_voxelSize.z());
}

void ParallelWorldDoseScorer::Construct() {
  if (!IsEnabled()) return;
  ResolveGrid();

  auto ghostWorld = GetWorld();
  auto ghostWorldLogical = ghostWorld->GetLogicalVolume();

  // Follow Geant4's native scoring-box layout: three nested replicas divide the
  // container along X, Y, and Z. This keeps O(1) geometry objects, exposes every
  // scoring boundary, and avoids activating mass-world score splitting.
  auto containerSolid = new G4Box("ParallelDoseGridBox", 0.5 * m_actualSize.x(),
                                  0.5 * m_actualSize.y(), 0.5 * m_actualSize.z());
  auto containerLogical =
      new G4LogicalVolume(containerSolid, nullptr, "ParallelDoseGridLV");
  new G4PVPlacement(nullptr, m_centre, containerLogical, "ParallelDoseGridPV",
                    ghostWorldLogical, false, 0, false);

  auto xSliceSolid = new G4Box("ParallelDoseXSliceBox", 0.5 * m_voxelSize.x(),
                               0.5 * m_actualSize.y(), 0.5 * m_actualSize.z());
  auto xSliceLogical =
      new G4LogicalVolume(xSliceSolid, nullptr, "ParallelDoseXSliceLV");
  if (m_nX > 1) {
    new G4PVReplica("ParallelDoseXSlicePV", xSliceLogical, containerLogical,
                    kXAxis, m_nX, m_voxelSize.x());
  } else {
    new G4PVPlacement(nullptr, G4ThreeVector(), xSliceLogical,
                      "ParallelDoseXSlicePV", containerLogical, false, 0, false);
  }

  auto ySliceSolid = new G4Box("ParallelDoseYSliceBox", 0.5 * m_voxelSize.x(),
                               0.5 * m_voxelSize.y(), 0.5 * m_actualSize.z());
  auto ySliceLogical =
      new G4LogicalVolume(ySliceSolid, nullptr, "ParallelDoseYSliceLV");
  if (m_nY > 1) {
    new G4PVReplica("ParallelDoseYSlicePV", ySliceLogical, xSliceLogical,
                    kYAxis, m_nY, m_voxelSize.y());
  } else {
    new G4PVPlacement(nullptr, G4ThreeVector(), ySliceLogical,
                      "ParallelDoseYSlicePV", xSliceLogical, false, 0, false);
  }

  auto voxelSolid = new G4Box("ParallelDoseVoxelBox", 0.5 * m_voxelSize.x(),
                              0.5 * m_voxelSize.y(), 0.5 * m_voxelSize.z());
  m_voxelLogical =
      new G4LogicalVolume(voxelSolid, nullptr, "ParallelDoseVoxelLV");
  if (m_nZ > 1) {
    new G4PVReplica("ParallelDoseVoxelPV", m_voxelLogical, ySliceLogical,
                    kZAxis, m_nZ, m_voxelSize.z());
  } else {
    new G4PVPlacement(nullptr, G4ThreeVector(), m_voxelLogical,
                      "ParallelDoseVoxelPV", ySliceLogical, false, 0, false);
  }
  WriteInfo();
}

void ParallelWorldDoseScorer::ConstructSD() {
  if (!IsEnabled() || !m_voxelLogical || m_parallelSD.Get()) return;
  const G4String collection = thisConfig()->GetValue<std::string>("CollectionName");
  const auto hitsCollection = collection + "_HC";
  G4Box scoringBox("ParallelDoseScoringBox", 0.5 * m_actualSize.x(),
                   0.5 * m_actualSize.y(), 0.5 * m_actualSize.z());
  auto sd = new ParallelWorldDoseSD("ParallelWorldDoseSD", m_centre, collection,
                                    hitsCollection, scoringBox, m_nX, m_nY, m_nZ);
  m_parallelSD.Put(sd);
  m_patientSD.Put(sd);
  G4SDManager::GetSDMpointer()->AddNewDetector(sd);
  G4VUserParallelWorld::SetSensitiveDetector(m_voxelLogical, sd);
}

bool ParallelWorldDoseScorer::HasScoring(const G4String& name, Scoring::Type type) const {
  return IsEnabled() && type == Scoring::Type::Voxel &&
         name == thisConfig()->GetValue<std::string>("CollectionName");
}

std::map<std::size_t, VoxelHit> ParallelWorldDoseScorer::GetScoringHashedMap(
    const G4String&, Scoring::Type) const {
  // Intentionally sparse. ControlPoint inserts a voxel on its first hit.
  return {};
}

void ParallelWorldDoseScorer::WriteInfo() {
  if (!IsEnabled()) return;
  INFO_GEO("Parallel dose grid '{}' centre={} requested-size={} actual-size={} voxel-size={} resolution={}x{}x{}",
           thisConfig()->GetValue<std::string>("CollectionName"), m_centre, m_size,
           m_actualSize, m_voxelSize, m_nX, m_nY, m_nZ);
}
