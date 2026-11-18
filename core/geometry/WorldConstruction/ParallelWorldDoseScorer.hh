#ifndef DOSE3D_PARALLELWORLDDOSESCORER_HH
#define DOSE3D_PARALLELWORLDDOSESCORER_HH

#include "Configurable.hh"
#include "G4Cache.hh"
#include "G4ThreeVector.hh"
#include "G4VUserParallelWorld.hh"
#include "VPatient.hh"

class G4LogicalVolume;
class VPatientSD;

/**
 * A regular, axis-aligned dose grid placed in a Geant4 parallel world.
 *
 * The ghost voxels do not replace or overlap the mass geometry. They only
 * provide step boundaries and a sensitive logical volume. Energy deposition
 * and material density are consequently read from the corresponding step in
 * the mass world.
 */
class ParallelWorldDoseScorer final : public G4VUserParallelWorld,
                                      public VPatient,
                                      public Configurable {
 public:
  static const G4String& WorldName();
  static bool ConfigEnabled();

  ParallelWorldDoseScorer();
  ~ParallelWorldDoseScorer() override = default;

  void Construct() override;
  void ConstructSD() override;

  bool IsEnabled() const;
  bool HasScoring(const G4String& name, Scoring::Type type) const override;
  std::map<std::size_t, VoxelHit> GetScoringHashedMap(
      const G4String& name, Scoring::Type type) const override;

  // IPhysicalVolume interface is unused: geometry is built by Construct().
  void Construct(G4VPhysicalVolume*) override {}
  void Destroy() override {}
  void Reset() override {}
  G4bool Update() override { return true; }
  void WriteInfo() override;
  void DefineSensitiveDetector() override { ConstructSD(); }

 protected:
  void Configure() override;
  void DefaultConfig(const std::string& unit) override;
  void ParseTomlConfig() override {}

 private:
  void ResolveGrid();

  G4ThreeVector m_size;
  G4ThreeVector m_voxelSize;
  G4ThreeVector m_centre;
  G4ThreeVector m_actualSize;
  G4int m_nX = 0;
  G4int m_nY = 0;
  G4int m_nZ = 0;
  G4LogicalVolume* m_voxelLogical = nullptr;
  G4Cache<VPatientSD*> m_parallelSD;
};

#endif
