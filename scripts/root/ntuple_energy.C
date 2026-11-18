// Energy QA for a G4RT event TTree.
//
// Interactive use (the canvas stays open):
//   root -l
//   root [0] .x scripts/root/ntuple_energy.C("job.root", "ParentWorldDoseTTree", "energy.pdf")

#include <TCanvas.h>
#include <TDirectory.h>
#include <TFile.h>
#include <TH1.h>
#include <TPad.h>
#include <TTree.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace NtupleEnergyPlot {

struct Observable {
  const char* branchName;
  const char* histogramTitle;
};

const std::vector<Observable> supportedObservables = {
    {"VoxelEDeposit", "Voxel energy deposit;Edep [MeV];Entries"},
    {"VoxelMeanEDeposit",
     "Mean voxel energy deposit;Mean Edep [MeV];Entries"},
    {"VoxelTrkE", "Track energy at scored step;Energy [MeV];Entries"},
    {"G4EvtPrimaryE", "Primary energy;Energy [MeV];Entries"}};

// Keep both the ROOT file and drawn objects alive after the function returns.
TFile* inputFile = nullptr;
TCanvas* canvas = nullptr;
std::vector<TH1*> histograms;

}  // namespace NtupleEnergyPlot

void ntuple_energy(const char* filename,
                   const char* treeName = "ParentWorldDoseTTree",
                   const char* output = "ntuple_energy.pdf") {
  using namespace NtupleEnergyPlot;

  inputFile = TFile::Open(filename, "READ");
  if (!inputFile || inputFile->IsZombie()) {
    throw std::runtime_error(std::string("Cannot open ROOT file: ") + filename);
  }

  TTree* tree = nullptr;
  inputFile->GetObject(treeName, tree);
  if (!tree) {
    throw std::runtime_error(std::string("Missing TTree: ") + treeName);
  }

  std::vector<Observable> availableObservables;
  for (const auto& observable : supportedObservables) {
    if (tree->GetBranch(observable.branchName)) {
      availableObservables.push_back(observable);
    } else {
      std::cout << "[skip] Branch not stored: " << observable.branchName
                << '\n';
    }
  }
  if (availableObservables.empty()) {
    throw std::runtime_error("The TTree has no supported energy branches");
  }

  canvas = new TCanvas("ntupleEnergyCanvas", "G4RT energy QA",
                       700 * availableObservables.size(), 600);
  canvas->Divide(availableObservables.size(), 1);
  histograms.clear();

  for (std::size_t index = 0; index < availableObservables.size(); ++index) {
    const auto& observable = availableObservables[index];
    const std::string histogramName =
        "energyHistogram" + std::to_string(index);
    const std::string drawExpression =
        std::string(observable.branchName) + ">>" + histogramName + "(100)";

    tree->Draw(drawExpression.c_str(), "", "goff");
    auto* histogram = dynamic_cast<TH1*>(gDirectory->Get(histogramName.c_str()));
    if (!histogram) {
      std::cout << "[skip] Could not draw branch: " << observable.branchName
                << '\n';
      continue;
    }

    histogram->SetTitle(observable.histogramTitle);
    histograms.push_back(histogram);

    canvas->cd(index + 1);
    gPad->SetLogy();
    histogram->Draw();
  }

  canvas->SaveAs(output);
  canvas->Modified();
  canvas->Update();

  std::cout << "Saved " << output << '\n'
            << "Canvas remains open. Close ROOT or the window when finished."
            << std::endl;
}
