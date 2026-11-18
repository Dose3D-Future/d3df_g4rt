/**
*
* \author L.Grzanka
* \date 14.11.2017
*
*/

#ifndef Dose3D_EVENTACTION_HH
#define Dose3D_EVENTACTION_HH

#include <chrono>
#include "G4UserEventAction.hh"
#include "RunSvc.hh"
#include "globals.hh"

///\class EventAction
class EventAction : public G4UserEventAction {
  public:
  EventAction();

  ~EventAction() = default;

  public:
  void BeginOfEventAction(const G4Event *);

  void EndOfEventAction(const G4Event *);

  /// Reset the process-wide progress counter before a new run starts.
  static void ResetProgress(G4int totalEvents, G4double frequency);

};

#endif // Dose3D_EVENTACTION_HH