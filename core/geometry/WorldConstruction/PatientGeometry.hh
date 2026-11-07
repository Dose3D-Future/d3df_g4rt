/**
*
* \author B.Rachwal (brachwal@agh.edu.pl)
* \date 10.12.2017
*
*/

#ifndef Dose3D_PHANTOMCONSTRUCTION_HH
#define Dose3D_PHANTOMCONSTRUCTION_HH

#include "G4GeometryManager.hh"
#include "G4RunManager.hh"
#include "G4UImanager.hh"
#include "G4UImessenger.hh"
#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "globals.hh"
#include "IPhysicalVolume.hh"
#include <G4SubtractionSolid.hh>
#include <G4UnionSolid.hh>


class VPatient;

/**
 * Configuration describing a regular CT-like voxel grid ("tube").
 * - The grid is defined in the global (world) coordinate system and is aligned 
 * with the patient isocentre.
 *
 * IMPORTANT CONVENTIONS:
 *
 * 1. Voxel positions represent voxel CENTERS (not boundaries).
 *    - The first voxel center is stored in initX/initY/initZ.
 *    - All exported data (CT, dose) are sampled at voxel centers.
 *
 * 2. The grid is uniform:
 *    pos(i) = init + i * size
 *
 * 3. The grid is symmetric around the isocentre in terms of physical extent:
 *    boundary_min = -env/2 + iso
 *    boundary_max = +env/2 + iso
 *
 * 4. Two coordinate ranges exist:
 *
 *    a) Center range (discrete sampling points):
 *       [init, init + (N-1)*size]
 *
 *    b) Physical boundary (continuous volume):
 *       [init - size/2, init + (N-1)*size + size/2]
 *
 */
struct CtTubeConfig {
    std::string name;

    double sizeX, sizeY, sizeZ;

    double envX, envY, envZ;

    double initX, initY, initZ; // center of first voxel

    int xRes, yRes, zRes;
};

///\class PatientGeometry
///\brief The liniac Phantom volume construction.
class PatientGeometry : public IPhysicalVolume,
                            public Configurable {
  public:
  ///
  static PatientGeometry *GetInstance();
                    
  ///
  void Construct(G4VPhysicalVolume *parentPV) override;

  ///
  void Destroy() override;

  ///
  G4bool Update() override;

  ///
  void Reset() override {}

  ///
  void WriteInfo() override;

  ///
  void DefineSensitiveDetector();

  ///
  void DefaultConfig(const std::string &unit) override;

  ///
  CtTubeConfig GetCtConfig() {return m_ctConfig;}
  
  ///
  VPatient* GetPatient() const { return m_patient; }

  ///
  void ExportToDicomCT(const std::string& path_to_output_dir) const;
  void ExportDoseToCsvCT(const G4Run* runPtr) const;
  void ExportToRTDose(const CtTubeConfig& cfg, const std::vector<double>& dose_data, const std::string& output_file) const;

  private:
  ///
  CtTubeConfig m_ctConfig;
  
  ///
  PatientGeometry();

  ///
  ~PatientGeometry();

  /// Delete the copy and move constructors
  PatientGeometry(const PatientGeometry &) = delete;

  PatientGeometry &operator=(const PatientGeometry &) = delete;

  PatientGeometry(PatientGeometry &&) = delete;

  PatientGeometry &operator=(PatientGeometry &&) = delete;

  ///
  bool design();

  ///
  void Configure() override;

  ///
  CtTubeConfig BuildCtTubeConfig(const std::string& name="tube_64_64_64") const;

  ///
  void WriteCtMetadata(const std::string& path, const CtTubeConfig& cfg) const;

  ///
  VPatient* m_patient;

  ///
  G4RotationMatrix* m_rotation;

  ///
  struct pair_hash {
      template <class T1, class T2>
      std::size_t operator() (const std::pair<T1, T2> &pair) const {
          return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
      }
  };

  ///
  G4PVPlacement* m_suplementary_volume = nullptr;

  ///
  std::unique_ptr<G4Navigator> CreateNavigator() const;

  /// Iterate over all voxel centers in the CT grid.
  /// For each voxel index (x, y, z), the position is computed as:
  ///     pos = init + index * size
  /// This corresponds to voxel CENTER positions.
  template<typename Func>
    void ForEachVoxel(const CtTubeConfig& cfg, Func&& f) const {
        G4ThreeVector pos;

        for (int y = 0; y < cfg.yRes; y++) {
            for (int x = 0; x < cfg.xRes; x++) {
                for (int z = 0; z < cfg.zRes; z++) {

                    pos.setX(cfg.initX + cfg.sizeX * x);
                    pos.setY(cfg.initY + cfg.sizeY * y);
                    pos.setZ(cfg.initZ + cfg.sizeZ * z);

                    f(x, y, z, pos);
                }
            }
        }
    }
};

#endif // Dose3D_PHANTOMCONSTRUCTION_HH
