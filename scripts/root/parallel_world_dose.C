// Quick visual QA for a G4RT parallel-world dose CSV.
//
// Interactive use (the canvas stays open):
//   root -l
//   root [0] .x scripts/root/parallel_world_dose.C("dose.csv", "z", 0.0, "dose_qa.pdf")

#include <TCanvas.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TPad.h>
#include <TStyle.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ParallelWorldDosePlot {

struct DoseRow {
  double x;
  double y;
  double z;
  double dose;
};

// These objects intentionally outlive the macro function. ROOT therefore keeps
// the canvas responsive at its prompt after the image has been saved.
TCanvas* canvas = nullptr;
TH2D* doseSlice = nullptr;
TH1D* doseDistribution = nullptr;
TGraph* centralProfile = nullptr;

double coordinate(const DoseRow& row, int axis) {
  if (axis == 0) return row.x;
  if (axis == 1) return row.y;
  return row.z;
}

int axisIndex(const char* axis) {
  if (axis[0] == 'x' || axis[0] == 'X') return 0;
  if (axis[0] == 'y' || axis[0] == 'Y') return 1;
  if (axis[0] == 'z' || axis[0] == 'Z') return 2;
  throw std::runtime_error("Axis must be x, y, or z");
}

std::vector<DoseRow> readDoseCsv(const char* filename) {
  std::ifstream input(filename);
  if (!input) {
    throw std::runtime_error(std::string("Cannot open dose CSV: ") + filename);
  }

  std::vector<DoseRow> rows;
  std::string line;

  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#' || line.rfind("Label,", 0) == 0) {
      continue;
    }

    std::stringstream lineStream(line);
    std::vector<std::string> fields;
    std::string field;
    while (std::getline(lineStream, field, ',')) {
      fields.push_back(field);
    }

    // Voxel CSV columns 7--10 contain X, Y, Z, and Dose.
    if (fields.size() >= 13) {
      rows.push_back({std::stod(fields[7]), std::stod(fields[8]),
                      std::stod(fields[9]), std::stod(fields[10])});
    }
  }

  if (rows.empty()) {
    throw std::runtime_error("Dose CSV contains no voxel rows");
  }
  return rows;
}

std::vector<double> sortedUnique(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

double nearestPlane(const std::vector<DoseRow>& rows, int normalAxis,
                    double requestedCoordinate) {
  double selected = coordinate(rows.front(), normalAxis);
  for (const auto& row : rows) {
    const double candidate = coordinate(row, normalAxis);
    if (std::abs(candidate - requestedCoordinate) <
        std::abs(selected - requestedCoordinate)) {
      selected = candidate;
    }
  }
  return selected;
}

}  // namespace ParallelWorldDosePlot

void parallel_world_dose(const char* filename, const char* axis = "z",
                         double coordinate_mm = 0.0,
                         const char* output = "parallel_world_dose_qa.pdf") {
  using namespace ParallelWorldDosePlot;

  const auto rows = readDoseCsv(filename);
  const int normalAxis = axisIndex(axis);
  const int horizontalAxis = normalAxis == 0 ? 1 : 0;
  const int verticalAxis = normalAxis == 2 ? 1 : 2;
  const double selectedPlane = nearestPlane(rows, normalAxis, coordinate_mm);

  std::vector<double> horizontalCoordinates;
  std::vector<double> verticalCoordinates;
  for (const auto& row : rows) {
    if (std::abs(coordinate(row, normalAxis) - selectedPlane) < 1e-9) {
      horizontalCoordinates.push_back(coordinate(row, horizontalAxis));
      verticalCoordinates.push_back(coordinate(row, verticalAxis));
    }
  }

  const auto uniqueHorizontal = sortedUnique(horizontalCoordinates);
  const auto uniqueVertical = sortedUnique(verticalCoordinates);
  if (uniqueHorizontal.empty() || uniqueVertical.empty()) {
    throw std::runtime_error("The selected dose slice is empty");
  }

  const double voxelWidth = uniqueHorizontal.size() > 1
                                ? uniqueHorizontal[1] - uniqueHorizontal[0]
                                : 1.0;
  const double voxelHeight = uniqueVertical.size() > 1
                                 ? uniqueVertical[1] - uniqueVertical[0]
                                 : 1.0;

  doseSlice = new TH2D(
      "parallelWorldDoseSlice",
      Form("Dose slice, %c = %.3g mm;%c [mm];%c [mm]", "XYZ"[normalAxis],
           selectedPlane, "XYZ"[horizontalAxis], "XYZ"[verticalAxis]),
      uniqueHorizontal.size(), uniqueHorizontal.front() - voxelWidth / 2.0,
      uniqueHorizontal.back() + voxelWidth / 2.0, uniqueVertical.size(),
      uniqueVertical.front() - voxelHeight / 2.0,
      uniqueVertical.back() + voxelHeight / 2.0);

  const auto maximumDoseRow = std::max_element(
      rows.begin(), rows.end(),
      [](const DoseRow& left, const DoseRow& right) {
        return left.dose < right.dose;
      });
  const double histogramMaximum =
      maximumDoseRow->dose > 0.0 ? maximumDoseRow->dose * 1.001 : 1.0;
  doseDistribution = new TH1D(
      "parallelWorldDoseDistribution",
      "Scored-voxel dose distribution;Dose [Gy];Voxels", 80, 0.0,
      histogramMaximum);

  for (const auto& row : rows) {
    doseDistribution->Fill(row.dose);
    if (std::abs(coordinate(row, normalAxis) - selectedPlane) < 1e-9) {
      doseSlice->Fill(coordinate(row, horizontalAxis),
                      coordinate(row, verticalAxis), row.dose);
    }
  }

  // Pick the scored line closest to the origin for a quick central profile.
  double centralHorizontal = coordinate(rows.front(), horizontalAxis);
  double centralVertical = coordinate(rows.front(), verticalAxis);
  for (const auto& row : rows) {
    if (std::abs(coordinate(row, horizontalAxis)) <
        std::abs(centralHorizontal)) {
      centralHorizontal = coordinate(row, horizontalAxis);
    }
    if (std::abs(coordinate(row, verticalAxis)) < std::abs(centralVertical)) {
      centralVertical = coordinate(row, verticalAxis);
    }
  }

  std::vector<std::pair<double, double>> profilePoints;
  for (const auto& row : rows) {
    const bool onCentralLine =
        std::abs(coordinate(row, horizontalAxis) - centralHorizontal) < 1e-9 &&
        std::abs(coordinate(row, verticalAxis) - centralVertical) < 1e-9;
    if (onCentralLine) {
      profilePoints.emplace_back(coordinate(row, normalAxis), row.dose);
    }
  }
  std::sort(profilePoints.begin(), profilePoints.end());

  std::vector<double> profileCoordinates;
  std::vector<double> profileDoses;
  for (const auto& [profileCoordinate, dose] : profilePoints) {
    profileCoordinates.push_back(profileCoordinate);
    profileDoses.push_back(dose);
  }

  centralProfile = new TGraph(profileCoordinates.size(),
                              profileCoordinates.data(), profileDoses.data());
  centralProfile->SetTitle(
      Form("Central profile;%c [mm];Dose [Gy]", "XYZ"[normalAxis]));
  centralProfile->SetMarkerStyle(20);

  gStyle->SetOptStat(1110);
  canvas = new TCanvas("parallelWorldDoseCanvas", "Parallel-world dose QA",
                       1500, 500);
  canvas->Divide(3, 1);

  canvas->cd(1);
  doseSlice->Draw("COLZ");
  canvas->cd(2);
  centralProfile->Draw("APL");
  canvas->cd(3);
  gPad->SetLogy();
  doseDistribution->Draw();

  canvas->SaveAs(output);
  canvas->Modified();
  canvas->Update();

  std::cout << "Saved " << output << '\n'
            << "Canvas remains open. Close ROOT or the window when finished."
            << std::endl;
}
