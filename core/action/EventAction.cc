#include "EventAction.hh"

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "RunAnalysis.hh"
#include "BeamAnalysis.hh"
#include "PrimariesAnalysis.hh"
#include "StepAnalysis.hh"
#include "NTupleEventAnalisys.hh"
#include "Services.hh"
#include "G4SDManager.hh"
#include "G4UImanager.hh"
#include "LogSvc.hh"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace {
std::atomic<G4int> completedEvents{0};
std::atomic<G4int> nextProgressEvent{1};
G4int progressInterval = 1;
G4int progressTotal = 0;
std::mutex progressMutex;
}

/////////////////////////////////////////////////////////////////////////////
///
EventAction::EventAction()
    = default;

/////////////////////////////////////////////////////////////////////////////
///
void EventAction::BeginOfEventAction(const G4Event *) {}

void EventAction::ResetProgress(G4int totalEvents, G4double frequency) {
  std::lock_guard<std::mutex> lock(progressMutex);
  progressTotal = std::max(0, totalEvents);
  const auto requestedInterval = static_cast<G4int>(std::lround(frequency * progressTotal));
  progressInterval = std::max(1, requestedInterval);
  completedEvents.store(0, std::memory_order_relaxed);
  nextProgressEvent.store(std::min(progressInterval, progressTotal), std::memory_order_release);
}

/////////////////////////////////////////////////////////////////////////////
/// Print progress information according to progress frequency defined by user
/// \param evt
void EventAction::EndOfEventAction(const G4Event *evt) {
  auto configSvc = Service<ConfigSvc>();

  if (configSvc->GetValue<bool>("RunSvc", "BeamAnalysis"))
    BeamAnalysis::GetInstance()->EndOfEventAction(evt);

  if (configSvc->GetValue<bool>("RunSvc", "PrimariesAnalysis"))
    PrimariesAnalysis::GetInstance()->EndOfEventAction(evt);

  if (configSvc->GetValue<bool>("RunSvc", "StepAnalysis"))
    StepAnalysis::GetInstance()->EndOfEventAction(evt);
  
  if (configSvc->GetValue<bool>("RunSvc", "NTupleAnalysis") && NTupleEventAnalisys::IsAnyTTreeDefined() ) //  
    NTupleEventAnalisys::GetInstance()->EndOfEventAction(evt);

  if (configSvc->GetValue<bool>("RunSvc", "RunAnalysis"))
    RunAnalysis::GetInstance()->EndOfEventAction(evt);

  // Event IDs are assigned globally, but events finish out of order in MT mode.
  // Count completions instead, and let only one worker emit each progress mark.
  const auto completed = completedEvents.fetch_add(1, std::memory_order_acq_rel) + 1;
  if (progressTotal > 0 && completed >= nextProgressEvent.load(std::memory_order_acquire)) {
    std::lock_guard<std::mutex> lock(progressMutex);
    const auto threshold = nextProgressEvent.load(std::memory_order_relaxed);
    if (completed >= threshold && threshold <= progressTotal) {
      const auto reported = std::min(completed, progressTotal);
      const auto percent = 100.0 * reported / progressTotal;
      LOGSVC_INFO("Progress", "{:5.2f}% ({}/{} events completed)",
                  percent, reported, progressTotal);
      const auto next = (reported == progressTotal)
                            ? progressTotal + 1
                            : std::min(progressTotal, threshold + progressInterval);
      nextProgressEvent.store(next, std::memory_order_release);
    }
  }
}