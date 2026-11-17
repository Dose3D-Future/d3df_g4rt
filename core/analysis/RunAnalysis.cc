//
// Created by brachwal on 28.04.2020.
//

#include "RunAnalysis.hh"
#include "G4Event.hh"
#include "G4Run.hh"
#include "G4SDManager.hh"
#include "VoxelHit.hh"
#include "G4AnalysisManager.hh"
#include "CsvRunAnalysis.hh"
#include "NTupleRunAnalysis.hh"
#include "Services.hh"
#ifdef G4MULTITHREADED
  #include "G4MTRunManager.hh"
#endif
#include "PatientGeometry.hh"
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace py::literals;

RunAnalysis::RunAnalysis(){
  if(!m_is_initialized){
    if(!m_csv_run_analysis) // TODO: && RUN_CSV_ANALYSIS
        m_csv_run_analysis = CsvRunAnalysis::GetInstance();
    if(!m_ntuple_run_analysis && Service<ConfigSvc>()->GetValue<bool>("RunSvc", "NTupleAnalysis")) // TODO: && RUN_NTUPLE_ANALYSIS
        m_ntuple_run_analysis = NTupleRunAnalysis::GetInstance();
    // TODO: RUN_HDF5_ANALYSIS
  }
  m_is_initialized = true;
}


////////////////////////////////////////////////////////////////////////////////
///
RunAnalysis *RunAnalysis::GetInstance() {
    static RunAnalysis instance = RunAnalysis();
    return &instance;
}


////////////////////////////////////////////////////////////////////////////////
///
void RunAnalysis::BeginOfRun(const G4Run* runPtr, G4bool isMaster){
    m_current_cp = Service<RunSvc>()->CurrentControlPoint();
    std::string worker = G4Threading::IsWorkerThread() ? "*WORKER*" : " *MASTER* ";
    ANA_DEBUG("RunAnalysis:: begin of run at {} thread.",worker);
    // Note: Everything is being care by ControlPointRun::InitializeScoringCollection
}

////////////////////////////////////////////////////////////////////////////////
/// This member is called at the end of every event from EventAction::EndOfEventAction
void RunAnalysis::EndOfEventAction(const G4Event *evt){
    auto hCofThisEvent = evt->GetHCofThisEvent();
    m_current_cp->FillEventCollections(hCofThisEvent);
}

////////////////////////////////////////////////////////////////////////////////
///
void RunAnalysis::EndOfRun(const G4Run* runPtr){
    ANA_INFO("RunAnalysis::EndOfRun:: CtrlPoint-{} / G4Run-{}", m_current_cp->GetId(), runPtr->GetRunID());
    // Note: Multithreading merging is being performed before...
    m_current_cp->GetRun()->EndOfRun();
    if(m_csv_run_analysis){
        m_csv_run_analysis->WriteDoseToCsv(runPtr);
        if(Service<ConfigSvc>()->GetValue<bool>("RunSvc", "WriteFieldMaskToCsv"))
            m_csv_run_analysis->WriteFieldMaskToCsv(runPtr);
        if(Service<ConfigSvc>()->GetValue<bool>("RunSvc", "GenerateCT"))
            PatientGeometry::GetInstance()->ExportDoseToCsvCT(runPtr);
    }

    if(m_ntuple_run_analysis){
        m_ntuple_run_analysis->WriteDoseToTFile(runPtr);
        m_ntuple_run_analysis->WriteFieldMaskToTFile(runPtr);
    }

    auto Mask2Matrix = [](const std::string& input_dir){
        std::string command =
            "python3 "+std::string(PROJECT_LOCATION_PATH)+"/submodules/d3df-nn3dsr/utils/mask2matrix.py "
            "-d " + input_dir +
            " -o " + input_dir +
            " --num_leaves 26" +
            " --no_pickle";

        int status = std::system(command.c_str());

        ANA_INFO(command.c_str());

        if (status != 0) {
            throw std::runtime_error("mask2matrix.py execution failed");
        }
    };

    auto DataAugmentation = [](const std::string& simCtDoseFile, const std::string& planDatFile, const std::string& augmDir){
        std::string command =
            "python3 "+std::string(PROJECT_LOCATION_PATH)+"/submodules/d3df-nn3dsr/utils/run_augm.py "
            "--data " + simCtDoseFile +
            " --mlc " + planDatFile +
            " --outdir " + augmDir;

        int status = std::system(command.c_str());

        ANA_INFO(command.c_str());

        if (status != 0) {
            throw std::runtime_error("run_augm.py execution failed");
        }
    };

    auto outputDir = m_current_cp->GetPlanOutputDir();
    Mask2Matrix(outputDir+"/input");

    auto planDatFile = m_current_cp->GetPlanFile();
    auto planName = m_current_cp->GetPlanName();
    DataAugmentation(outputDir+"/"+planName+"_ct_dose.csv",planDatFile,outputDir+"/augm");
    Mask2Matrix(outputDir+"/augm");

}
