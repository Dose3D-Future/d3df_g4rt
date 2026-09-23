#include "PatientGeometry.hh"
#include "Types.hh"
#include "VoxelHit.hh"
#include "WaterPhantom.hh"
#include "SciSlicePhantom.hh"
#include "DishCubePhantom.hh"
#include "G4ProductionCuts.hh"
#include "D3DDetector.hh"
#include "G4SystemOfUnits.hh"
#include "Services.hh"
#include "G4Box.hh"
#include "WorldConstruction.hh"
#include "IO.hh"
#include "DicomSvc.hh"
#include "IbaImRT.hh"
#include "CADMesh.hh"
#include "GeometryBuilder.hh"
#include "GeometryDBReader.hh"
#include "ModularWaterPhantom.hh"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>

namespace {
  G4Mutex phantomConstructionMutex = G4MUTEX_INITIALIZER;
}

////////////////////////////////////////////////////////////////////////////////
///
PatientGeometry::PatientGeometry()
      :IPhysicalVolume("PatientGeometry"), Configurable("PatientGeometry"){
    Configure();
  }

////////////////////////////////////////////////////////////////////////////////
///
PatientGeometry::~PatientGeometry() {
  configSvc()->Unregister(thisConfig()->GetName());
}

////////////////////////////////////////////////////////////////////////////////
///
PatientGeometry* PatientGeometry::GetInstance() {
  static PatientGeometry instance;
  return &instance;
}

////////////////////////////////////////////////////////////////////////////////
///
void PatientGeometry::Configure() {
  G4cout << "\n\n[INFO]::  Configuring the " << thisConfig()->GetName() << G4endl;
  DefineUnit<std::string>("Type");
  DefineUnit<double>("PatientIsocentreX");
  DefineUnit<double>("PatientIsocentreY");
  DefineUnit<double>("PatientIsocentreZ");
  DefineUnit<double>("EnviromentSizeX");
  DefineUnit<double>("EnviromentSizeY");
  DefineUnit<double>("EnviromentSizeZ");
  DefineUnit<std::string>("EnviromentMedium");
  DefineUnit<std::string>("EnviromentPatientEnvelop");
  DefineUnit<std::string>("SupplementaryGeometry");
  DefineUnit<std::string>("SupplementaryGeometryMaterial");
  DefineUnit<double>("SupplementaryGeometryPositionX");
  DefineUnit<double>("SupplementaryGeometryPositionY");
  DefineUnit<double>("SupplementaryGeometryPositionZ");
  DefineUnit<std::string>("ConfigFile");
  DefineUnit<std::string>("ConfigPrefix");
  DefineUnit<double>("VoxelSizeXCT");
  DefineUnit<double>("VoxelSizeYCT");
  DefineUnit<double>("VoxelSizeZCT");
  DefineUnit<std::string>("PatientDBPath");

  Configurable::DefaultConfig();   // setup the default configuration for all defined units/parameters
  // G4cout << "[DEBUG]:: PatientGeometry:: Configure: DefaultConfig"<< G4endl;
  // Configurable::PrintConfig();
}

////////////////////////////////////////////////////////////////////////////////
///



void PatientGeometry::DefaultConfig(const std::string &unit) {
  // Volume name
  if (unit.compare("Label") == 0){
    thisConfig()->SetValue(unit, std::string("Patient environmet"));
    }
  if (unit.compare("Type") == 0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None")); // "DishCubePhantom"  ,WaterPhantom , SciSlicePhantom, Dose3D
    // thisConfig()->SetValue(unit, std::string("Dose3D")); // "DishCubePhantom"  ,WaterPhantom , SciSlicePhantom, Dose3D
    }
  // default box size
  if (unit.compare("PatientIsocentreX") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }
  if (unit.compare("PatientIsocentreY") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }
  if (unit.compare("PatientIsocentreZ") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }

  if (unit.compare("EnviromentSizeX") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }

  if (unit.compare("EnviromentSizeY") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }

  if (unit.compare("EnviromentSizeZ") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }

 if (unit.compare("EnviromentMedium") == 0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
    }

  if (unit.compare("EnviromentPatientEnvelop")==0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
  }

  if (unit.compare("SupplementaryGeometryPositionX") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }
  if (unit.compare("SupplementaryGeometryPositionY") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }
  if (unit.compare("SupplementaryGeometryPositionZ") == 0){
    thisConfig()->SetTValue<double>(unit, 0.0);
    }
  if (unit.compare("SupplementaryGeometry")==0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
  }
  if (unit.compare("SupplementaryGeometryMaterial")==0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
  }

  if (unit.compare("DBGeometryPath")==0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
  }

  if (unit.compare("SupplementaryGeometry")==0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
  }

  if (unit.compare("ConfigFile") == 0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
    }
  if (unit.compare("ConfigPrefix") == 0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
    }
  if (unit.compare("VoxelSizeXCT") == 0){
    thisConfig()->SetTValue<double>(unit, double(1.00));
    }
  if (unit.compare("VoxelSizeYCT") == 0){
    thisConfig()->SetTValue<double>(unit, double(1.00));
    }
  if (unit.compare("VoxelSizeZCT") == 0){
    thisConfig()->SetTValue<double>(unit, double(1.00));
    }
  if (unit.compare("PatientDBPath") == 0){
    thisConfig()->SetTValue<std::string>(unit, std::string("None"));
    }

}

////////////////////////////////////////////////////////////////////////////////
///
bool PatientGeometry::design(void) {
  auto patientType = thisConfig()->GetValue<std::string>("Type");
  G4cout << "I'm building " << patientType << "  patient geometry" << G4endl;

  if (patientType == "WaterPhantom") {
    m_patient = new WaterPhantom();
    m_patient->TomlConfig(true);
  }
  else if (patientType == "SciSlicePhantom"){
    m_patient = new SciSlicePhantom();
  }
  else if (patientType == "DishCubePhantom"){
    m_patient = new DishCubePhantom();
  }
  else if (patientType == "D3DDetector") {
    m_patient = new D3DDetector();
    m_patient->TomlConfig(true);
  }
  else
    return false;

  // TOML-like contextual configuration
  if(m_patient->TomlConfig()){
    auto configFile =thisConfig()->GetValue<std::string>("ConfigFile");
    if(configFile.empty() || configFile=="None")
      m_patient->SetTomlConfigFile(); // get the job main file
    else{
      std::string projectPath = PROJECT_LOCATION_PATH;
      m_patient->SetTomlConfigFile(projectPath+configFile);
    }
    configFile = m_patient->GetTomlConfigFile();
    G4cout << "PatientGeometry::ConfigFile:: Importing configuration for \""<< patientType <<"\" from: "<< configFile << "\n" << G4endl;
  }

  if(thisConfig()->GetValue<std::string>("PatientDBPath") != "None"){
    auto path = std::string(PROJECT_LOCATION_PATH) + "/submodules/" + thisConfig()->GetValue<std::string>("PatientDBPath");
    GeometryDBReader::Instance().LoadDataBase(path);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
///
void PatientGeometry::Destroy() {
  auto pv = GetPhysicalVolume();
  if (pv) {
    if (m_patient) m_patient->Destroy();
    delete pv;
    SetPhysicalVolume(nullptr);
  }
  if (m_suplementary_volume){
    delete m_suplementary_volume;
    m_suplementary_volume = nullptr;
  }

}

////////////////////////////////////////////////////////////////////////////////
///
void PatientGeometry::Construct(G4VPhysicalVolume *parentPV) {
  PrintConfig();
  design(); // a call to select the right phantom
  auto envPatientEnvelop = thisConfig()->GetValue<std::string>("EnviromentPatientEnvelop");
  G4cout << "EnvPatientEnvelop: " << envPatientEnvelop << G4endl;

  auto mediumName = thisConfig()->GetValue<std::string>("EnviromentMedium");
  auto medium = Service<ConfigSvc>()->GetValue<G4MaterialSPtr>("MaterialsSvc", mediumName);

  // create an envelope box filled with seleceted medium
  auto envSize = G4ThreeVector(thisConfig()->GetValue<double>("EnviromentSizeX")/2.,thisConfig()->GetValue<double>("EnviromentSizeY")/2.,thisConfig()->GetValue<double>("EnviromentSizeZ")/2.);
  G4Box *patientEnv = new G4Box("patientEnvBox", envSize.x(),envSize.y(),envSize.z());
  G4LogicalVolume *patientEnvLV = new G4LogicalVolume(patientEnv, medium.get(), "patientEnvLV", 0, 0, 0);
  // The envelope box will bo located at given point with respect to the parentPV.
  // However it shifted to Sim locatin (ie. to the positive querter of the World coordinate system)
  auto envPosX = thisConfig()->GetValue<double>("PatientIsocentreX");
  auto envPosY = thisConfig()->GetValue<double>("PatientIsocentreY");
  auto envPosZ = thisConfig()->GetValue<double>("PatientIsocentreZ");

  if (envPatientEnvelop.compare("IbaImRT_Full") == 0){
    IbaImRT::IbaToLocalTranslation = G4ThreeVector(90.0, -165.0, 90.0);
  } else if(envPatientEnvelop.compare("IbaImRT_Box") == 0){
    IbaImRT::IbaToLocalTranslation = G4ThreeVector(90.0, -90.0, 90.0);
  }

  // Region for cuts
  auto regVol = new G4Region("phantomEnviromentRegion");
  auto cuts = new G4ProductionCuts;
  cuts->SetProductionCut(5.0 * mm);
  regVol->SetProductionCuts(cuts);
  patientEnvLV->SetRegion(regVol);
  regVol->AddRootLogicalVolume(patientEnvLV);
  SetPhysicalVolume(new G4PVPlacement(m_rotation, G4ThreeVector(envPosX,envPosY,envPosZ)-IbaImRT::IbaToLocalTranslation, "phmWorldPV", patientEnvLV, parentPV, false, 0));
  auto pv = GetPhysicalVolume();

  if (envPatientEnvelop.compare("IbaImRT_Full") == 0 || envPatientEnvelop.compare("IbaImRT_Box") == 0){
    auto ibaImRT = IbaImRT::GetInstance();
    ibaImRT->IPhysicalVolume::Construct(this);
    m_patient->IPhysicalVolume::Construct(ibaImRT);
    m_patient->WriteInfo();
  }else if(envPatientEnvelop.compare("IbaImRT_3mf") == 0){
    auto ibaImRT = IbaImRT::GetInstance();
    ibaImRT->IPhysicalVolume::Construct(this);
    SetPhysicalVolume(pv);
    m_patient->IPhysicalVolume::Construct(this);
    m_patient->WriteInfo();
  }
  else if(envPatientEnvelop.compare("ModularWaterPhantom_simplified") == 0 || envPatientEnvelop.compare("ModularWaterPhantom_3mf") == 0){
    auto modularWaterPhantom = ModularWaterPhantom::GetInstance();
    modularWaterPhantom->SetRotation(m_rotation);
    modularWaterPhantom->IPhysicalVolume::Construct(this);
    modularWaterPhantom->WriteInfo();
    m_patient->IPhysicalVolume::Construct(this);
    m_patient->WriteInfo();
  }
  else{
    auto geometryBuilder = GeometryBuilder::GetInstance();
    geometryBuilder->Build(pv);
    m_patient->IPhysicalVolume::Construct(this);
    m_patient->WriteInfo();
  }


// Creation of bed?

 auto tableMaterial = ConfigSvc::GetInstance()->GetValue<G4MaterialSPtr>("MaterialsSvc", "G4_POLYACRYLONITRILE");
 auto tableHeight =  7.0*mm;
 auto tableBox = new G4Box("TableBox", 1100.0*mm, 225.0*mm, tableHeight);
 auto dcoverLV = new G4LogicalVolume(tableBox, tableMaterial.get(), "TableBoxLV");
 auto table = new G4PVPlacement(nullptr, G4ThreeVector(900.0,0.0,((1.0*mm)+tableHeight+envPosZ+envSize.z())), "TableBoxPV", dcoverLV, parentPV, false, 0);

 if (thisConfig()->GetValue<std::string>("SupplementaryGeometry").compare("None")!=0) {
  auto supplementaryGeometryPath = thisConfig()->GetValue<std::string>("SupplementaryGeometry");
  if (supplementaryGeometryPath.at(0)!='/'){
    std::string data_path = PROJECT_DATA_PATH;
    supplementaryGeometryPath = data_path+"/"+supplementaryGeometryPath;
  }
  auto supplementaryGeometryMaterial = thisConfig()->GetValue<std::string>("SupplementaryGeometryMaterial");
  auto suppGeoPosX = thisConfig()->GetValue<double>("SupplementaryGeometryPositionX");
  auto suppGeoPosY = thisConfig()->GetValue<double>("SupplementaryGeometryPositionY");
  auto suppGeoPosZ = thisConfig()->GetValue<double>("SupplementaryGeometryPositionZ");

  auto mesh = CADMesh::TessellatedMesh::FromSTL(supplementaryGeometryPath);
  G4VSolid* solid = mesh->GetSolid();
  auto Medium = ConfigSvc::GetInstance()->GetValue<G4MaterialSPtr>("MaterialsSvc", supplementaryGeometryMaterial);
  auto supplementaryGeometryLV = new G4LogicalVolume(solid, Medium.get(), "LVStl_Supplementary");
  m_suplementary_volume = new G4PVPlacement(nullptr, G4ThreeVector(suppGeoPosX,suppGeoPosY,suppGeoPosZ), "PVStl_Supplementary", supplementaryGeometryLV, parentPV, false, 0);

}

}
////////////////////////////////////////////////////////////////////////////////
///
std::unique_ptr<G4Navigator> PatientGeometry::CreateNavigator() const {
    auto nav = std::make_unique<G4Navigator>();
    nav->SetWorldVolume(Service<GeoSvc>()->World()->GetPhysicalVolume());
    return nav;
}

////////////////////////////////////////////////////////////////////////////////
/// Builds a CT voxel grid configuration aligned with the patient isocentre.
///
/// The first voxel position is defined as the CENTER of the first voxel:
///     init = -env/2 + iso + size/2
///
/// This ensures:
/// - voxel sampling is done at centers
/// - the full voxel grid spans exactly:
///       [-env/2 + iso, +env/2 + iso]
///   when considering voxel boundaries
CtTubeConfig PatientGeometry::BuildCtTubeConfig(const std::string& name) const {
    CtTubeConfig cfg;
    cfg.name = name;

    cfg.sizeX = thisConfig()->GetValue<double>("VoxelSizeXCT");
    cfg.sizeY = thisConfig()->GetValue<double>("VoxelSizeYCT");
    cfg.sizeZ = thisConfig()->GetValue<double>("VoxelSizeZCT");

    cfg.envX = thisConfig()->GetValue<double>("EnviromentSizeX");
    cfg.envY = thisConfig()->GetValue<double>("EnviromentSizeY");
    cfg.envZ = thisConfig()->GetValue<double>("EnviromentSizeZ");

    auto isoX = thisConfig()->GetValue<double>("PatientIsocentreX");
    auto isoY = thisConfig()->GetValue<double>("PatientIsocentreY");
    auto isoZ = thisConfig()->GetValue<double>("PatientIsocentreZ");

    auto calcInit = [&](double env, double iso, double size) {
        return svc::round_with_prec(-env / 2.0 + iso + size / 2.0, 4);
    };

    cfg.initX = calcInit(cfg.envX, isoX, cfg.sizeX);
    cfg.initY = calcInit(cfg.envY, isoY, cfg.sizeY);
    cfg.initZ = calcInit(cfg.envZ, isoZ, cfg.sizeZ);

    cfg.xRes = static_cast<int>(std::round(cfg.envX / cfg.sizeX));
    cfg.yRes = static_cast<int>(std::round(cfg.envY / cfg.sizeY));
    cfg.zRes = static_cast<int>(std::round(cfg.envZ / cfg.sizeZ));

    return cfg;
}

////////////////////////////////////////////////////////////////////////////////
/// Writes CT grid metadata to CSV.
///
/// This function explicitly distinguishes between:
///
/// 1. Voxel CENTER range (discrete sampling positions):
///    x_center_min = init
///    x_center_max = init + (N-1)*size
///
/// 2. Physical BOUNDARY range (continuous volume extent):
///    x_min = init - size/2
///    x_max = init + (N-1)*size + size/2
///
/// These definitions ensure:
/// - consistency with voxel-centered sampling
/// - compatibility with imaging toolkits (ITK, SimpleITK, etc.)
/// - enable exporting data to DICOM-CT format
///
/// In practice, this layout is directly used for DICOM generation in:
/// d3df_g4rt/core/utilities/python/dicom_ct.py
///
/// IMPORTANT:
/// - center_* values describe where data points exist
/// - min/max values describe the physical extent of the volume
/// - these must NOT be mixed or interpreted interchangeably
void PatientGeometry::WriteCtMetadata(const std::string& path, const CtTubeConfig& cfg) const {
    std::ofstream file(path, std::ios::out);

    file << "name," << cfg.name << "\n";

    auto centerMax = [&](double init, int res, double step) {
        return init + (res - 1) * step;
    };

    auto boundaryMin = [&](double init, double step) {
        return init - step / 2.0;
    };

    auto boundaryMax = [&](double init, int res, double step) {
        return init + (res - 1) * step + step / 2.0;
    };

    // centra
    file << "x_center_min," << cfg.initX << "\n";
    file << "y_center_min," << cfg.initY << "\n";
    file << "z_center_min," << cfg.initZ << "\n";

    file << "x_center_max," << centerMax(cfg.initX, cfg.xRes, cfg.sizeX) << "\n";
    file << "y_center_max," << centerMax(cfg.initY, cfg.yRes, cfg.sizeY) << "\n";
    file << "z_center_max," << centerMax(cfg.initZ, cfg.zRes, cfg.sizeZ) << "\n";

    // granice
    file << "x_min," << boundaryMin(cfg.initX, cfg.sizeX) << "\n";
    file << "y_min," << boundaryMin(cfg.initY, cfg.sizeY) << "\n";
    file << "z_min," << boundaryMin(cfg.initZ, cfg.sizeZ) << "\n";

    file << "x_max," << boundaryMax(cfg.initX, cfg.xRes, cfg.sizeX) << "\n";
    file << "y_max," << boundaryMax(cfg.initY, cfg.yRes, cfg.sizeY) << "\n";
    file << "z_max," << boundaryMax(cfg.initZ, cfg.zRes, cfg.sizeZ) << "\n";

    file << "x_resolution," << cfg.xRes << "\n";
    file << "y_resolution," << cfg.yRes << "\n";
    file << "z_resolution," << cfg.zRes << "\n";

    file << "x_step," << cfg.sizeX << "\n";
    file << "y_step," << cfg.sizeY << "\n";
    file << "z_step," << cfg.sizeZ << "\n";

    double SSD = 1000;
    file << "SSD," << svc::round_with_prec(SSD, 4) << "\n";
}

////////////////////////////////////////////////////////////////////////////////
///
G4bool PatientGeometry::Update() {
  // TODO:: Update this GetPhysicalVolume(); then the daughter
  if (m_patient) {
    if (!m_patient->Update()) return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
///
void PatientGeometry::WriteInfo() {
  auto envPosX = thisConfig()->GetValue<double>("PatientIsocentreX");
  auto envPosY = thisConfig()->GetValue<double>("PatientIsocentreY");
  auto envPosZ = thisConfig()->GetValue<double>("PatientIsocentreZ");
  auto centre = G4ThreeVector(envPosX,envPosY,envPosZ);
  G4cout << "Phantom centre: " << centre / cm << " [cm] " << G4endl;
}

////////////////////////////////////////////////////////////////////////////////
/// NOTE: This method is called from WorldConstruction::ConstructSDandField
///       which is being called in workers in MT mode
void PatientGeometry::DefineSensitiveDetector() {
  if (m_patient){
    // check if there is any analysis switched on in patient:
    auto configSvc = Service<ConfigSvc>();
    if(configSvc->GetValue<bool>("RunSvc", "StepAnalysis") ||
       configSvc->GetValue<bool>("RunSvc", "RunAnalysis") ||
       configSvc->GetValue<bool>("RunSvc", "NTupleAnalysis") ) {
      G4AutoLock lock(&phantomConstructionMutex);
      m_patient->DefineSensitiveDetector();
    } else {
      std::string worker = G4Threading::IsWorkerThread() ? "worker" : "master";
      WARN_GEO("No sensitive detector defined for patient. Any analysis is switched on ({})!",worker);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
///
/// This function exports the patient geometry to CSV files for CT imaging.
/// It creates a set of CSV files for each slice of the patient and saves
/// the position and material of each voxel in the CSV format. The CSV files
/// are saved in the specified output directory.
///
void PatientGeometry::ExportToCsvCT(const std::string& path_to_output_dir) const {
    auto patientEnv = Service<GeoSvc>()->World()->PatientEnvironment();
    if (!patientEnv) return;

    IO::CreateDirIfNotExits(path_to_output_dir);

    auto cfg = BuildCtTubeConfig();
    auto nav = CreateNavigator();

    INFO_GEO("ExportToCsvCT [{}]: Resolution x={}, y={}, z={}",
             cfg.name, cfg.xRes, cfg.yRes, cfg.zRes);

    INFO_GEO("ExportToCsvCT [{}]: Path={}", cfg.name, path_to_output_dir);

    // ----------------------------
    // Metadata
    // ----------------------------
    WriteCtMetadata(path_to_output_dir + "/ct_series_metadata.csv", cfg);

    // ----------------------------
    // Slice-by-slice export (Y axis)
    // ----------------------------
    for (int y = 0; y < cfg.yRes; y++) {

        std::ostringstream ss;
        ss << std::setw(4) << std::setfill('0') << (y + 1);

        std::string filePath = path_to_output_dir + "/img" + ss.str() + ".csv";
        std::ofstream file(filePath, std::ios::out);

        file << "X,Y,Z,Material\n";

        for (int x = 0; x < cfg.xRes; x++) {
            for (int z = 0; z < cfg.zRes; z++) {

                G4ThreeVector pos;
                pos.setX(cfg.initX + cfg.sizeX * x);
                pos.setY(cfg.initY + cfg.sizeY * y);
                pos.setZ(cfg.initZ + cfg.sizeZ * z);

                auto volume = nav->LocateGlobalPointAndSetup(pos);
                auto material = volume->GetLogicalVolume()
                                       ->GetMaterial()
                                       ->GetName();

                file << pos.x() << ","
                     << pos.y() << ","
                     << pos.z() << ","
                     << material << "\n";
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/// Export dose distribution mapped onto a unified CT grid.
///
/// Projection of the simulation scoring data (Voxel and Cell) onto a regular
/// CT grid and exports results into a single CSV file.
/// The CT grid acts as the single source of truth for spatial sampling.
///
/// Output:
///  - *_ct_dose.csv  → contains both Voxel and Cell dose evaluated at CT voxel centres
///  - *_ct_dose_series_metadata.csv → CT metadata
///
/// ======= High-level workflow =======
/// 1. Build CT grid definition (CtTubeConfig)
/// 2. Extract scoring data from simulation (Voxel + Cell)
/// 3. Map scoring data into optimized spatial lookup structures
/// 4. Iterate over CT grid (ForEachVoxel)
/// 5. Sample:
///      - material (via navigator)
///      - voxel dose (high resolution)
///      - cell dose (coarse resolution)
/// 6. Export all values into a single CSV row per CT voxel
///
/// ======= CT grid (reference space) =======
/// The CT grid is defined by CtTubeConfig and represents a regular 3D lattice:
/// - origin: (initX, initY, initZ) shifted by half-voxel sizes to define grid boundaries
/// - spacing: (sizeX, sizeY, sizeZ)
/// - resolution: (xRes, yRes, zRes)
///
/// Each sampled point corresponds to the centre of a CT voxel.
///
/// IMPORTANT:
/// - Both Voxel and Cell scoring data are resampled onto this grid
/// - This guarantees strict spatial alignment between datasets
/// - Nearest neighbour sampling is performed based on grid spatial alignment
///
/// ======= Lookup & Mapping strategy =======
/// To prevent massive performance bottlenecks during grid iteration, scoring
/// data is processed into two distinct layouts:
///
/// - Voxel Lookup (Dense, High-Resolution):
///   Mapped into a `std::unordered_map` using a discrete 3D `SpatialKey` (i, j, k)
///   computed relative to the grid origin.
///     - Complexity: O(1) average lookup per CT voxel.
///     - Yields precise, local mapping alongside structural IDs (IdX, IdY, IdZ)
///       and scaling factors (FSF, ASF).
/// - Cell Lookup (Sparse, Coarse Resolution):
///   Stored in a flat linear vector (`cellHits`).
///   For each CT voxel, candidates are filtered using an axis-aligned boundary
///   tolerance window (half the CT voxel size: +-sizeX/2). The closest match
///   by Euclidean distance is selected.
///
/// ======= Coordinate system =======
/// - All coordinates are in global Geant4 space
/// - The CT grid is structurally aligned with the simulation world
///
void PatientGeometry::ExportDoseToCsvCT(const G4Run* runPtr) const {
    auto patientEnv = Service<GeoSvc>()->World()->PatientEnvironment();
    if (!patientEnv) return;

    auto cp = Service<RunSvc>()->CurrentControlPoint();

    auto planName = std::filesystem::path(cp->GetPlanFile()).stem().string();
    auto outDir = cp->GetOutputDir() + "/" + planName;

    IO::CreateDirIfNotExits(outDir);

    auto cfg = BuildCtTubeConfig();
    auto nav = CreateNavigator();

    RUNSVC_INFO("ExportDoseToCsvCT [{}]: xRes={}, yRes={}, zRes={}",
             cfg.name, cfg.xRes, cfg.yRes, cfg.zRes);

    const auto& scoring_maps = cp->GetRun()->GetScoringCollections();

    auto metaDataFile = outDir + "/" + planName + "_ct_series_metadata.csv";
    WriteCtMetadata(metaDataFile, cfg);

    const double doseVoxelSizeX = cfg.sizeX;
    const double doseVoxelSizeY = cfg.sizeY;
    const double doseVoxelSizeZ = cfg.sizeZ;

    G4ThreeVector doseGridOrigin(
        cfg.initX - (doseVoxelSizeX / 2.0),
        cfg.initY - (doseVoxelSizeY / 2.0),
        cfg.initZ - (doseVoxelSizeZ / 2.0)
    );

    // =====================================================
    // HASHING
    // =====================================================
    struct SpatialKey {
        int i, j, k;
        SpatialKey(int i, int j, int k) : i(i), j(j), k(k) {}
        bool operator==(const SpatialKey& other) const {
            return i == other.i && j == other.j && k == other.k;
        }
    };

    struct SpatialKeyHasher {
        std::size_t operator()(const SpatialKey& k) const {
            std::size_t h = 0;
            h ^= std::hash<int>{}(k.i) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.j) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.k) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<SpatialKey, const VoxelHit*, SpatialKeyHasher> voxelLookupMap;
    std::vector<const VoxelHit*> cellHits;

    for (auto& sm : scoring_maps) {
        for (auto& scoring : sm.second) {
            for (auto& entry : scoring.second) {
                auto& hit_data = entry.second;

                if (scoring.first == Scoring::Type::Voxel) {
                    int i = static_cast<int>(std::floor((hit_data.GetCentre().x() - doseGridOrigin.x()) / doseVoxelSizeX));
                    int j = static_cast<int>(std::floor((hit_data.GetCentre().y() - doseGridOrigin.y()) / doseVoxelSizeY));
                    int k = static_cast<int>(std::floor((hit_data.GetCentre().z() - doseGridOrigin.z()) / doseVoxelSizeZ));

                    SpatialKey key(i, j, k);
                    voxelLookupMap[key] = &hit_data;
                }
                else if (scoring.first == Scoring::Type::Cell) {
                    cellHits.push_back(&hit_data);
                }
            }
        }
    }
    RUNSVC_INFO("VoxelLookupMap size = {}", voxelLookupMap.size());
    RUNSVC_INFO("CellHits vector size = {}", cellHits.size());

    // =====================================================
    std::vector<double> rtdose_data(cfg.zRes * cfg.yRes * cfg.xRes, 0.0);
    ForEachVoxel(cfg, [&](int x, int y, int z, const G4ThreeVector& pos) {
        auto materialName = nav->LocateGlobalPointAndSetup(pos)
                               ->GetLogicalVolume()
                               ->GetMaterial()
                               ->GetName();
        auto materialHU = DicomSvc::GetHounsfieldScaleValue(materialName, true);

        int idX = -1, idY = -1, idZ = -1;
        double doseVoxel = 0.0;
        double doseCell = 0.0;
        double fsf = 0.0;
        double asf = 0.0;

        int target_i = static_cast<int>(std::floor((pos.x() - doseGridOrigin.x()) / doseVoxelSizeX));
        int target_j = static_cast<int>(std::floor((pos.y() - doseGridOrigin.y()) / doseVoxelSizeY));
        int target_k = static_cast<int>(std::floor((pos.z() - doseGridOrigin.z()) / doseVoxelSizeZ));

        SpatialKey searchKey(target_i, target_j, target_k);
        auto voxelIt = voxelLookupMap.find(searchKey);

        if (voxelIt != voxelLookupMap.end()) {
            const auto* hit = voxelIt->second;
            idX = hit->GetGlobalID(0);
            idY = hit->GetGlobalID(1);
            idZ = hit->GetGlobalID(2);
            doseVoxel = hit->GetDose();
            fsf = hit->GetFieldScalingFactor();
            asf = hit->GetAngleScalingFactor();
        }

        double bestDist2 = std::numeric_limits<double>::max();
        for (const auto* hit : cellHits) {
            double dx = hit->GetCentre().x() - pos.x();
            double dy = hit->GetCentre().y() - pos.y();
            double dz = hit->GetCentre().z() - pos.z();

            if (std::abs(dx) <= cfg.sizeX/2
             && std::abs(dy) <= cfg.sizeX/2
             && std::abs(dz) <= cfg.sizeX/2) {
                double d2 = dx*dx + dy*dy + dz*dz;
                if (d2 < bestDist2) {
                    bestDist2 = d2;
                    doseCell = hit->GetDose();
                }
            }
        }
        size_t flat_index = z * (cfg.yRes * cfg.xRes) + y * cfg.xRes + x;
        rtdose_data[flat_index] = doseVoxel;
      });
      std::string dcmOutputFile = outDir + "/" + planName + "_ct_rtdose.dcm";
      ExportToRTDose(cfg, rtdose_data, dcmOutputFile);
}

void PatientGeometry::ExportToRTDose(const CtTubeConfig& cfg, const std::vector<double>& dose_data, const std::string& output_file) const {
    RUNSVC_INFO("ExportToRTDose [{}]: xRes={}, yRes={}, zRes={}",
    cfg.name, cfg.xRes, cfg.yRes, cfg.zRes);
    py::module_ dicom_rtdose = py::module::import("dicom_rtdose");
    py::dict metadata;

    std::vector<double> x_u, y_u, z_u;
    for (int i = 0; i < cfg.xRes; ++i) x_u.push_back(cfg.initX + i * cfg.sizeX);
    for (int j = 0; j < cfg.yRes; ++j) y_u.push_back(cfg.initY + j * cfg.sizeY);
    for (int k = 0; k < cfg.zRes; ++k) z_u.push_back(cfg.initZ + k * cfg.sizeZ);

    metadata["x_unique"] = x_u;
    metadata["y_unique"] = y_u;
    metadata["z_unique"] = z_u;
    metadata["PatientName"] = "Kowalski^Jan";
    metadata["PatientID"] = "123456789";

    size_t frames = cfg.zRes;
    size_t rows = cfg.yRes;
    size_t cols = cfg.xRes;

    std::vector<size_t> shape = {frames, rows, cols};
    std::vector<size_t> strides = {rows * cols * sizeof(double), cols * sizeof(double), sizeof(double)};

    py::array_t<double> dose_grid(shape, strides, dose_data.data());
    dicom_rtdose.attr("write_rtdose")(metadata, dose_grid, output_file);
}
