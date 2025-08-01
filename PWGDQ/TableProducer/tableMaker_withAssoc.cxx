// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//
// Contact: iarsene@cern.ch, i.c.arsene@fys.uio.no
//
// TableMaker produces skimmed data using the DQ data model
// Events to be written are filtered using a user provided event cut and optionally the filterPPwithAssociation task
// Barrel and muon (collision-track) associations are filtered using multiple parallel selections (currently limited to 8 but easily extendable up to 64)
// The skimming can optionally produce just the barrel, muon, or both barrel and muon tracks
// The event filtering, centrality, and V0Bits (from v0-selector) can be switched on/off by selecting one
//  of the process functions
// C++ includes
#include <map>
#include <memory>
#include <string>
#include <vector>
// other includes
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "Framework/DataTypes.h"
#include "Framework/runDataProcessing.h"
#include "CCDB/BasicCCDBManager.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Centrality.h"
#include "Common/CCDB/TriggerAliases.h"
#include "Common/DataModel/PIDResponse.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/DataModel/MftmchMatchingML.h"
#include "Common/DataModel/FwdTrackReAlignTables.h"
#include "Common/Core/TableHelper.h"
#include "PWGDQ/DataModel/ReducedInfoTables.h"
#include "PWGDQ/Core/VarManager.h"
#include "PWGDQ/Core/HistogramManager.h"
#include "PWGDQ/Core/AnalysisCut.h"
#include "PWGDQ/Core/AnalysisCompositeCut.h"
#include "PWGDQ/Core/HistogramsLibrary.h"
#include "PWGDQ/Core/CutsLibrary.h"
#include "DataFormatsGlobalTracking/RecoContainerCreateTracksVariadic.h"
#include "DetectorsVertexing/VertexTrackMatcher.h"
#include "ReconstructionDataFormats/PrimaryVertex.h"
#include "ReconstructionDataFormats/VtxTrackIndex.h"
#include "ReconstructionDataFormats/VtxTrackRef.h"
#include "DataFormatsITSMFT/ROFRecord.h"
#include "CommonDataFormat/InteractionRecord.h"
#include "DetectorsVertexing/PVertexerParams.h"
#include "MathUtils/Primitive2D.h"
#include "DataFormatsGlobalTracking/RecoContainer.h"
#include "Common/DataModel/CollisionAssociationTables.h"
#include "DataFormatsParameters/GRPMagField.h"
#include "DataFormatsParameters/GRPObject.h"
#include "Field/MagneticField.h"
#include "TGeoGlobalMagField.h"
#include "DetectorsBase/Propagator.h"
#include "DetectorsBase/GeometryManager.h"
#include "EventFiltering/Zorro.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::aod;

Zorro zorro;

// Declaration of various Joins used in the different process functions
// TODO: Since DCA depends on which collision the track is associated to, we should remove writing and subscribing to DCA tables, to optimize on CPU / memory
using MyBarrelTracks = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksDCA,
                                 aod::pidTPCFullEl, aod::pidTPCFullMu, aod::pidTPCFullPi,
                                 aod::pidTPCFullKa, aod::pidTPCFullPr,
                                 aod::pidTOFFullEl, aod::pidTOFFullMu, aod::pidTOFFullPi,
                                 aod::pidTOFFullKa, aod::pidTOFFullPr, aod::pidTOFbeta>;
using MyBarrelTracksWithCov = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCov, aod::TracksDCA,
                                        aod::pidTPCFullEl, aod::pidTPCFullMu, aod::pidTPCFullPi,
                                        aod::pidTPCFullKa, aod::pidTPCFullPr,
                                        aod::pidTOFFullEl, aod::pidTOFFullMu, aod::pidTOFFullPi,
                                        aod::pidTOFFullKa, aod::pidTOFFullPr, aod::pidTOFbeta>;
using MyBarrelTracksWithCovNoTOF = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCov, aod::TracksDCA,
                                             aod::pidTPCFullEl, aod::pidTPCFullPi,
                                             aod::pidTPCFullKa, aod::pidTPCFullPr>;
using MyBarrelTracksWithV0Bits = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCov, aod::TracksDCA,
                                           aod::pidTPCFullEl, aod::pidTPCFullMu, aod::pidTPCFullPi,
                                           aod::pidTPCFullKa, aod::pidTPCFullPr,
                                           aod::pidTOFFullEl, aod::pidTOFFullMu, aod::pidTOFFullPi,
                                           aod::pidTOFFullKa, aod::pidTOFFullPr, aod::pidTOFbeta, aod::V0Bits>;
using MyBarrelTracksWithV0BitsNoTOF = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksDCA,
                                                aod::pidTPCFullEl, aod::pidTPCFullPi,
                                                aod::pidTPCFullKa, aod::pidTPCFullPr, aod::V0Bits>;
using MyBarrelTracksWithDalitzBits = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCov, aod::TracksDCA,
                                               aod::pidTPCFullEl, aod::pidTPCFullMu, aod::pidTPCFullPi,
                                               aod::pidTPCFullKa, aod::pidTPCFullPr,
                                               aod::pidTOFFullEl, aod::pidTOFFullMu, aod::pidTOFFullPi,
                                               aod::pidTOFFullKa, aod::pidTOFFullPr, aod::pidTOFbeta, aod::DalitzBits>;
using MyEvents = soa::Join<aod::Collisions, aod::EvSels>;
using MyEventsWithMults = soa::Join<aod::Collisions, aod::EvSels, aod::Mults>;
using MyEventsWithFilter = soa::Join<aod::Collisions, aod::EvSels, aod::DQEventFilter>;
using MyEventsWithMultsAndFilter = soa::Join<aod::Collisions, aod::EvSels, aod::Mults, aod::MultsExtra, aod::DQEventFilter>;
using MyEventsWithMultsAndRapidityGapFilter = soa::Join<aod::Collisions, aod::EvSels, aod::Mults, aod::MultsExtra, aod::DQRapidityGapFilter>;
using MyEventsWithCent = soa::Join<aod::Collisions, aod::EvSels, aod::CentFT0Cs, aod::CentFT0As, aod::CentFT0Ms>;
using MyEventsWithCentAndMults = soa::Join<aod::Collisions, aod::EvSels, aod::CentFT0Cs, aod::CentFT0As, aod::CentFT0Ms, aod::Mults, aod::MultsExtra>;
using MyEventsWithMultsExtra = soa::Join<aod::Collisions, aod::EvSels, aod::Mults, aod::MultsExtra>;
using MyMuons = soa::Join<aod::FwdTracks, aod::FwdTracksDCA>;
using MyMuonsWithCov = soa::Join<aod::FwdTracks, aod::FwdTracksCov, aod::FwdTracksDCA>;
using MyMuonsRealignWithCov = soa::Join<aod::FwdTracksReAlign, aod::FwdTrksCovReAlign, aod::FwdTracksDCA>;
using MyMuonsColl = soa::Join<aod::FwdTracks, aod::FwdTracksDCA, aod::FwdTrkCompColls>;
using MyMuonsCollWithCov = soa::Join<aod::FwdTracks, aod::FwdTracksCov, aod::FwdTracksDCA, aod::FwdTrkCompColls>;
using MyBCs = soa::Join<aod::BCs, aod::Timestamps, aod::Run3MatchedToBCSparse>;
using ExtBCs = soa::Join<aod::BCs, aod::Timestamps, aod::MatchedBCCollisionsSparseMulti>;

// Declaration of various bit maps containing information on which tables are included in a Join
//   These are used as template arguments and checked at compile time
// constexpr static uint32_t gkEventFillMap = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision;
// constexpr static uint32_t gkEventFillMapWithFilter = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::EventFilter;
constexpr static uint32_t gkEventFillMapWithMults = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionMult;
constexpr static uint32_t gkEventFillMapWithMultsZdc = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionMult | VarManager::ObjTypes::Zdc;
constexpr static uint32_t gkEventFillMapWithMultsAndEventFilter = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionMult | VarManager::ObjTypes::CollisionMultExtra | VarManager::ObjTypes::EventFilter;
constexpr static uint32_t gkEventFillMapWithMultsEventFilterZdc = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionMult | VarManager::ObjTypes::CollisionMultExtra | VarManager::ObjTypes::EventFilter | VarManager::ObjTypes::Zdc;
constexpr static uint32_t gkEventFillMapWithMultsRapidityGapFilterZdc = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionMult | VarManager::ObjTypes::CollisionMultExtra | VarManager::ObjTypes::RapidityGapFilter | VarManager::ObjTypes::Zdc;
// constexpr static uint32_t gkEventFillMapWithCent = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionCent;
constexpr static uint32_t gkEventFillMapWithCentAndMults = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionCent | VarManager::CollisionMult | VarManager::ObjTypes::CollisionMultExtra;
constexpr static uint32_t gkEventFillMapWithMultsExtra = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::CollisionMult | VarManager::ObjTypes::CollisionMultExtra;
//  constexpr static uint32_t gkEventFillMapWithCentRun2 = VarManager::ObjTypes::BC | VarManager::ObjTypes::Collision | VarManager::ObjTypes::CollisionCentRun2; // Unused variable
// constexpr static uint32_t gkTrackFillMap = VarManager::ObjTypes::Track | VarManager::ObjTypes::TrackExtra | VarManager::ObjTypes::TrackDCA | VarManager::ObjTypes::TrackPID | VarManager::ObjTypes::TrackPIDExtra;
constexpr static uint32_t gkTrackFillMapWithCov = VarManager::ObjTypes::Track | VarManager::ObjTypes::TrackExtra | VarManager::ObjTypes::TrackDCA | VarManager::ObjTypes::TrackCov | VarManager::ObjTypes::TrackPID | VarManager::ObjTypes::TrackPIDExtra;
constexpr static uint32_t gkTrackFillMapWithV0Bits = gkTrackFillMapWithCov | VarManager::ObjTypes::TrackV0Bits;
constexpr static uint32_t gkTrackFillMapWithV0BitsNoTOF = VarManager::ObjTypes::Track | VarManager::ObjTypes::TrackExtra | VarManager::ObjTypes::TrackDCA | VarManager::ObjTypes::TrackV0Bits | VarManager::ObjTypes::TrackTPCPID;
constexpr static uint32_t gkTrackFillMapNoTOF = VarManager::ObjTypes::Track | VarManager::ObjTypes::TrackExtra | VarManager::ObjTypes::TrackDCA | VarManager::ObjTypes::TrackTPCPID;
// constexpr static uint32_t gkTrackFillMapWithDalitzBits = gkTrackFillMap | VarManager::ObjTypes::DalitzBits;
// constexpr static uint32_t gkMuonFillMap = VarManager::ObjTypes::Muon;
constexpr static uint32_t gkMuonFillMapWithCov = VarManager::ObjTypes::Muon | VarManager::ObjTypes::MuonCov;
constexpr static uint32_t gkMuonRealignFillMapWithCov = VarManager::ObjTypes::MuonRealign | VarManager::ObjTypes::MuonCovRealign;
// constexpr static uint32_t gkMuonFillMapWithAmbi = VarManager::ObjTypes::Muon | VarManager::ObjTypes::AmbiMuon;
// constexpr static uint32_t gkMuonFillMapWithCovAmbi = VarManager::ObjTypes::Muon | VarManager::ObjTypes::MuonCov | VarManager::ObjTypes::AmbiMuon;
// constexpr static uint32_t gkTrackFillMapWithAmbi = VarManager::ObjTypes::Track | VarManager::ObjTypes::AmbiTrack;
constexpr static uint32_t gkMFTFillMap = VarManager::ObjTypes::TrackMFT;

// Enum containing the ordering of statistics histograms to be written in the QA file
enum SkimStatsHists {
  kStatsEvent = 0,
  kStatsTracks,
  kStatsMuons,
  kStatsOrphanTracks,
  kStatsZorroInfo,
  kStatsZorroSel
};

struct TableMaker {

  Produces<ReducedEvents> event;
  Produces<ReducedEventsExtended> eventExtended;
  Produces<ReducedEventsVtxCov> eventVtxCov;
  Produces<ReducedEventsInfo> eventInfo;
  Produces<ReducedZdcs> zdc;
  Produces<ReducedEventsMultPV> multPV;
  Produces<ReducedEventsMultAll> multAll;
  Produces<ReducedTracksBarrelInfo> trackBarrelInfo;
  Produces<ReducedTracks> trackBasic;
  Produces<ReducedTracksBarrel> trackBarrel;
  Produces<ReducedTracksBarrelCov> trackBarrelCov;
  Produces<ReducedTracksBarrelPID> trackBarrelPID;
  Produces<ReducedTracksAssoc> trackBarrelAssoc;
  Produces<ReducedMuons> muonBasic;
  Produces<ReducedMuonsExtra> muonExtra;
  Produces<ReducedMuonsCov> muonCov;
  Produces<ReducedMuonsInfo> muonInfo;
  Produces<ReducedMuonsAssoc> muonAssoc;
  Produces<ReducedMFTs> mftTrack;
  Produces<ReducedMFTsExtra> mftTrackExtra;
  Produces<ReducedMFTAssoc> mftAssoc;

  OutputObj<THashList> fOutputList{"output"}; //! the histogram manager output list
  OutputObj<TList> fStatsList{"Statistics"};  //! skimming statistics

  HistogramManager* fHistMan;

  // Event and track AnalysisCut configurables
  struct : ConfigurableGroup {
    Configurable<std::string> fConfigEventCuts{"cfgEventCuts", "eventStandardNoINT7", "Event selection"};
    Configurable<std::string> fConfigTrackCuts{"cfgBarrelTrackCuts", "jpsiO2MCdebugCuts2", "Comma separated list of barrel track cuts"};
    Configurable<std::string> fConfigMuonCuts{"cfgMuonCuts", "muonQualityCuts", "Comma separated list of muon cuts"};
    Configurable<std::string> fConfigEventCutsJSON{"cfgEventCutsJSON", "", "Additional event selection in JSON format"};
    Configurable<std::string> fConfigTrackCutsJSON{"cfgBarrelTrackCutsJSON", "", "Additional list of barrel track cuts in JSON format"};
    Configurable<std::string> fConfigMuonCutsJSON{"cfgMuonCutsJSON", "", "Additional list of muon cuts in JSON format"};
  } fConfigCuts;

  // Zorro selection
  struct : ConfigurableGroup {
    Configurable<bool> fConfigRunZorro{"cfgRunZorro", false, "Enable event selection with zorro"};
    Configurable<std::string> fConfigZorroTrigMask{"cfgZorroTriggerMask", "fDiMuon", "DQ Trigger masks: fSingleE,fLMeeIMR,fLMeeHMR,fDiElectron,fSingleMuLow,fSingleMuHigh,fDiMuon"};
    Configurable<bool> fConfigRunZorroSel{"cfgRunZorroSel", false, "Select events with trigger mask"};
    Configurable<uint64_t> fBcTolerance{"cfgBcTolerance", 100, "Number of BCs of margin for software triggers"};
  } fConfigZorro;

  // Steer QA output
  struct : ConfigurableGroup {
    Configurable<bool> fConfigQA{"cfgQA", false, "If true, fill QA histograms"};
    Configurable<bool> fConfigDetailedQA{"cfgDetailedQA", false, "If true, include more QA histograms (BeforeCuts classes)"};
    Configurable<std::string> fConfigAddEventHistogram{"cfgAddEventHistogram", "", "Comma separated list of histograms"};
    Configurable<std::string> fConfigAddTrackHistogram{"cfgAddTrackHistogram", "", "Comma separated list of histograms"};
    Configurable<std::string> fConfigAddMuonHistogram{"cfgAddMuonHistogram", "", "Comma separated list of histograms"};
    Configurable<std::string> fConfigAddJSONHistograms{"cfgAddJSONHistograms", "", "Histograms in JSON format"};
  } fConfigHistOutput;

  Configurable<bool> fIsRun2{"cfgIsRun2", false, "Whether we analyze Run-2 or Run-3 data"};

  // Selections to be applied as Filter on the Track and FwdTrack
  /*struct : ConfigurableGroup {
    Configurable<float> fConfigBarrelTrackMaxAbsEta{"cfgBarrelMaxAbsEta", 0.9f, "Eta absolute value cut for tracks in the barrel"};
    Configurable<float> fConfigBarrelTrackMinPt{"cfgBarrelMinPt", 0.5f, "Minimum pt for tracks in the barrel"};
    Configurable<bool> fConfigBarrelRequireTPC{"cfgBarrelRequireTPC", true, "Require TPC for tracks in the barrel"};
    Configurable<float> fConfigBarrelMinTPCncls{"cfgBarrelMinTPCncls", 50.0f, "Minimum TPC cls for tracks in the barrel"};
    Configurable<float> fConfigBarrelMaxTPCchi2{"cfgBarrelMaxTPCchi2", 10.0f, "Maximum TPC chi2/ndf for tracks in the barrel"};
    Configurable<bool> fConfigBarrelRequireITS{"cfgBarrelRequireITS", true, "Require ITS for tracks in the barrel"};
    Configurable<float> fConfigBarrelMaxITSchi2{"cfgBarrelMaxITSchi2", 36.0f, "Maximum ITS chi2/ndf for tracks in the barrel"};
    Configurable<float> fConfigMuonPtLow{"cfgMuonLowPt", 1.0f, "Low pt cut for muons"};
  } fConfigFilter;*/

  // CCDB connection configurables
  struct : ConfigurableGroup {
    Configurable<string> fConfigCcdbUrl{"ccdb-url", "http://alice-ccdb.cern.ch", "url of the ccdb repository"};
    Configurable<string> fConfigCcdbPathTPC{"ccdb-path-tpc", "Users/z/zhxiong/TPCPID/PostCalib", "base path to the ccdb object"};
    Configurable<string> fConfigCcdbPathZorro{"ccdb-path-zorro", "/Users/m/mpuccio/EventFiltering/OTS/Chunked/", "base path to the ccdb object for zorro"};
    Configurable<int64_t> fConfigNoLaterThan{"ccdb-no-later-than", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), "latest acceptable timestamp of creation for the object"};
    Configurable<std::string> fConfigGeoPath{"geoPath", "GLO/Config/GeometryAligned", "Path of the geometry file"};
    Configurable<std::string> fConfigGrpMagPath{"grpmagPath", "GLO/Config/GRPMagField", "CCDB path of the GRPMagField object"};
    Configurable<std::string> fConfigGrpMagPathRun2{"grpmagPathRun2", "GLO/GRP/GRP", "CCDB path of the GRPObject (Usage for Run 2)"};
  } fConfigCCDB;

  // TPC postcalibration related options
  struct : ConfigurableGroup {
    Configurable<bool> fConfigComputeTPCpostCalib{"cfgTPCpostCalib", false, "If true, compute TPC post-calibrated n-sigmas(electrons, pions, protons)"};
    Configurable<bool> fConfigComputeTPCpostCalibKaon{"cfgTPCpostCalibKaon", false, "If true, compute TPC post-calibrated n-sigmas for kaons"};
    Configurable<bool> fConfigIsOnlyforMaps{"cfgIsforMaps", false, "If true, run for postcalibration maps only"};
    Configurable<bool> fConfigSaveElectronSample{"cfgSaveElectronSample", false, "If true, only save electron sample"};
  } fConfigPostCalibTPC;

  struct : ConfigurableGroup {
    // Track related options
    Configurable<bool> fPropTrack{"cfgPropTrack", true, "Propagate tracks to associated collision to recalculate DCA and momentum vector"};
    // Muon related options
    Configurable<bool> fPropMuon{"cfgPropMuon", true, "Propagate muon tracks through absorber (do not use if applying pairing)"};
    Configurable<bool> fRefitGlobalMuon{"cfgRefitGlobalMuon", true, "Correct global muon parameters"};
    Configurable<float> fMuonMatchEtaMin{"cfgMuonMatchEtaMin", -4.0f, "Definition of the acceptance of muon tracks to be matched with MFT"};
    Configurable<float> fMuonMatchEtaMax{"cfgMuonMatchEtaMax", -2.5f, "Definition of the acceptance of muon tracks to be matched with MFT"};

    // TPC occupancy related variables
    Configurable<float> fTPCShortPast{"cfgTPCShortPast", 8.0f, "Time in short past to look for occupancy (micro-seconds)"};
    Configurable<float> fTPCShortFuture{"cfgTPCShortFuture", 8.0f, "Time in short future to look for occupancy (micro-seconds)"};
    Configurable<float> fTPCLongPast{"cfgTPCLongPast", 100.0f, "Time in long past to look for occupancy (micro-seconds)"};
    Configurable<float> fTPCLongFuture{"cfgTPCLongFuture", 100.0f, "Time in long future to look for occupancy (micro-seconds)"};
    Configurable<bool> fExcludeShort{"cfgTPCExcludeShort", true, "Exclude short term from long term occupancy (micro-seconds)"};
  } fConfigVariousOptions;

  Service<o2::ccdb::BasicCCDBManager> fCCDB;
  o2::ccdb::CcdbApi fCCDBApi;

  o2::parameters::GRPObject* fGrpMagRun2 = nullptr; // for run 2, we access the GRPObject from GLO/GRP/GRP
  o2::parameters::GRPMagField* fGrpMag = nullptr;   // for run 3, we access GRPMagField from GLO/Config/GRPMagField

  AnalysisCompositeCut* fEventCut;               //! Event selection cut
  std::vector<AnalysisCompositeCut*> fTrackCuts; //! Barrel track cuts
  std::vector<AnalysisCompositeCut*> fMuonCuts;  //! Muon track cuts

  bool fDoDetailedQA = false; // Bool to set detailed QA true, if QA is set true
  int fCurrentRun;            // needed to detect if the run changed and trigger update of calibrations etc.

  // maps used to store index info; NOTE: std::map are sorted in ascending order by default (needed for track to collision indices)
  std::map<uint32_t, uint32_t> fCollIndexMap;             // key: old collision index, value: skimmed collision index
  std::map<uint32_t, uint32_t> fTrackIndexMap;            // key: old track global index, value: new track global index
  std::map<uint32_t, uint32_t> fFwdTrackIndexMap;         // key: fwd-track global index, value: new fwd-track global index
  std::map<uint32_t, uint32_t> fFwdTrackIndexMapReversed; // key: new fwd-track global index, value: fwd-track global index
  std::map<uint32_t, uint8_t> fFwdTrackFilterMap;         // key: fwd-track global index, value: fwd-track filter map
  std::map<uint32_t, uint32_t> fMftIndexMap;              // key: MFT tracklet global index, value: new MFT tracklet global index

  // FIXME: For now, the skimming is done using the Common track-collision association task, which does not allow to use
  //       our own Filtered tracks. If the filter is very selective, then it may be worth to run the association in this workflow
  //       using the Common/CollisionAssociation class
  /*Filter barrelSelectedTracks = ifnode(fIsRun2.node() == true, track::trackType == uint8_t(track::Run2Track), track::trackType == uint8_t(track::Track))
                              && track::pt > fConfigFilter.fConfigBarrelTrackMinPt
                              && nabs(track::eta) <= fConfigFilter.fConfigBarrelTrackMaxAbsEta
                              && ifnode(fConfigFilter.fConfigBarrelRequireITS.node() == true, track::itsChi2NCl < fConfigFilter.fConfigBarrelMaxITSchi2, true)
                              && ifnode(fConfigFilter.fConfigBarrelRequireTPC.node() == true, track::tpcNClsFound > fConfigFilter.fConfigBarrelMinTPCncls && track::tpcChi2NCl < fConfigFilter.fConfigBarrelMaxTPCchi2, true);

  Filter muonFilter = o2::aod::fwdtrack::pt >= fConfigFilter.fConfigMuonPtLow;*/

  Preslice<aod::TrackAssoc> trackIndicesPerCollision = aod::track_association::collisionId;
  Preslice<aod::FwdTrackAssoc> fwdtrackIndicesPerCollision = aod::track_association::collisionId;
  Preslice<aod::MFTTrackAssoc> mfttrackIndicesPerCollision = aod::track_association::collisionId;

  Preslice<MyBarrelTracksWithV0Bits> preslice = aod::track::collisionId;
  Partition<MyBarrelTracksWithV0Bits> tracksPos = (((aod::track::flags & static_cast<uint32_t>(o2::aod::track::PVContributor)) == static_cast<uint32_t>(o2::aod::track::PVContributor)) && (aod::track::tgl > static_cast<float>(0.05)));
  Partition<MyBarrelTracksWithV0Bits> tracksNeg = (((aod::track::flags & static_cast<uint32_t>(o2::aod::track::PVContributor)) == static_cast<uint32_t>(o2::aod::track::PVContributor)) && (aod::track::tgl < static_cast<float>(-0.05)));

  Preslice<MyBarrelTracksWithV0BitsNoTOF> presliceNoTOF = aod::track::collisionId;
  Partition<MyBarrelTracksWithV0BitsNoTOF> tracksPosNoTOF = (((aod::track::flags & static_cast<uint32_t>(o2::aod::track::PVContributor)) == static_cast<uint32_t>(o2::aod::track::PVContributor)) && (aod::track::tgl > static_cast<float>(0.05)));
  Partition<MyBarrelTracksWithV0BitsNoTOF> tracksNegNoTOF = (((aod::track::flags & static_cast<uint32_t>(o2::aod::track::PVContributor)) == static_cast<uint32_t>(o2::aod::track::PVContributor)) && (aod::track::tgl < static_cast<float>(-0.05)));

  Preslice<MyBarrelTracksWithCovNoTOF> presliceWithCovNoTOF = aod::track::collisionId;
  Partition<MyBarrelTracksWithCovNoTOF> tracksPosWithCovNoTOF = (((aod::track::flags & static_cast<uint32_t>(o2::aod::track::PVContributor)) == static_cast<uint32_t>(o2::aod::track::PVContributor)) && (aod::track::tgl > static_cast<float>(0.05)));
  Partition<MyBarrelTracksWithCovNoTOF> tracksNegWithCovNoTOF = (((aod::track::flags & static_cast<uint32_t>(o2::aod::track::PVContributor)) == static_cast<uint32_t>(o2::aod::track::PVContributor)) && (aod::track::tgl < static_cast<float>(-0.05)));

  struct {
    std::map<int32_t, float> oMeanTimeShortA;
    std::map<int32_t, float> oMeanTimeShortC;
    std::map<int32_t, float> oMeanTimeLongA;
    std::map<int32_t, float> oMeanTimeLongC;
    std::map<int32_t, float> oMedianTimeShortA;
    std::map<int32_t, float> oMedianTimeShortC;
    std::map<int32_t, float> oMedianTimeLongA;
    std::map<int32_t, float> oMedianTimeLongC;
    std::map<int32_t, int> oContribShortA;
    std::map<int32_t, int> oContribShortC;
    std::map<int32_t, int> oContribLongA;
    std::map<int32_t, int> oContribLongC;
  } fOccup;

  void init(o2::framework::InitContext& context)
  {
    // CCDB configuration
    if (fConfigPostCalibTPC.fConfigComputeTPCpostCalib) {
      fCCDB->setURL(fConfigCCDB.fConfigCcdbUrl.value);
      fCCDB->setCaching(true);
      fCCDB->setLocalObjectValidityChecking();
      // Not later than now objects
      fCCDB->setCreatedNotAfter(fConfigCCDB.fConfigNoLaterThan.value);
    }
    fCCDBApi.init(fConfigCCDB.fConfigCcdbUrl.value);

    if (!o2::base::GeometryManager::isGeometryLoaded()) {
      fCCDB->get<TGeoManager>(fConfigCCDB.fConfigGeoPath);
    }
    VarManager::SetDefaultVarNames(); // Important that this is called before DefineCuts() !!

    // Define the event, track and muon cuts
    DefineCuts();

    // Initialize the histogram manager
    fHistMan = new HistogramManager("analysisHistos", "aa", VarManager::kNVars);
    fHistMan->SetUseDefaultVariableNames(kTRUE);
    fHistMan->SetDefaultVarNames(VarManager::fgVariableNames, VarManager::fgVariableUnits);

    // Only use detailed QA when QA is set true
    if (fConfigHistOutput.fConfigQA && fConfigHistOutput.fConfigDetailedQA) {
      fDoDetailedQA = true;
    }

    // Create the histogram class names to be added to the histogram manager
    // The histogram class names are added into a string and then passed to the DefineHistograms() function which
    //   actually configures the HistogramManager
    // Histograms are defined as histogram classes / groups and filled at specific points in the analysis flow
    TString histClasses = "";
    // Event-wise histograms, before selection cuts
    if (fDoDetailedQA) {
      histClasses += "Event_BeforeCuts;";
    }
    // Event-wise histograms, after selection cuts
    if (fConfigHistOutput.fConfigQA) {
      histClasses += "Event_AfterCuts;";
    }

    // Check whether we have to define barrel or muon histograms
    bool enableBarrelHistos = (context.mOptions.get<bool>("processPPWithFilter") || context.mOptions.get<bool>("processPPWithFilterBarrelOnly") || context.mOptions.get<bool>("processPPBarrelOnly") ||
                               context.mOptions.get<bool>("processPbPb") || context.mOptions.get<bool>("processPbPbBarrelOnly") || context.mOptions.get<bool>("processPbPbBarrelOnlyWithV0Bits") || context.mOptions.get<bool>("processPbPbBarrelOnlyWithV0BitsNoTOF")) ||
                              context.mOptions.get<bool>("processPbPbWithFilterBarrelOnly") || context.mOptions.get<bool>("processPPBarrelOnlyWithV0s") || context.mOptions.get<bool>("processPbPbBarrelOnlyNoTOF");

    bool enableMuonHistos = (context.mOptions.get<bool>("processPPWithFilter") || context.mOptions.get<bool>("processPPWithFilterMuonOnly") || context.mOptions.get<bool>("processPPWithFilterMuonMFT") || context.mOptions.get<bool>("processPPMuonOnly") || context.mOptions.get<bool>("processPPRealignedMuonOnly") || context.mOptions.get<bool>("processPPMuonMFT") || context.mOptions.get<bool>("processPPMuonMFTWithMultsExtra") ||
                             context.mOptions.get<bool>("processPbPb") || context.mOptions.get<bool>("processPbPbMuonOnly") || context.mOptions.get<bool>("processPbPbRealignedMuonOnly") || context.mOptions.get<bool>("processPbPbMuonMFT"));

    if (enableBarrelHistos) {
      // Barrel track histograms, before selections
      if (fDoDetailedQA) {
        histClasses += "TrackBarrel_BeforeCuts;";
      }
      if (fConfigHistOutput.fConfigQA) {
        // Barrel track histograms after selections; one histogram directory for each user specified selection
        for (auto& cut : fTrackCuts) {
          histClasses += Form("TrackBarrel_%s;", cut->GetName());
        }
      }
      // Barrel histograms for clean samples of V0 legs used for post-calibration
      if (fConfigPostCalibTPC.fConfigIsOnlyforMaps) {
        histClasses += "TrackBarrel_PostCalibElectron;";
        histClasses += "TrackBarrel_PostCalibPion;";
        histClasses += "TrackBarrel_PostCalibProton;";
      }
    }
    if (enableMuonHistos) {
      // Muon tracks before cuts and MFT tracks
      if (fDoDetailedQA) {
        histClasses += "Muons_BeforeCuts;";
        histClasses += "MftTracks;";
      }
      if (fConfigHistOutput.fConfigQA) {
        // Muon tracks after selections; one directory per selection
        for (auto& muonCut : fMuonCuts) {
          histClasses += Form("Muons_%s;", muonCut->GetName());
        }
      }
    }

    DefineHistograms(histClasses); // define all histograms
    // Additional histogram via the JSON configurable
    TString addHistsStr = fConfigHistOutput.fConfigAddJSONHistograms.value;
    if (fConfigHistOutput.fConfigQA && addHistsStr != "") {
      dqhistograms::AddHistogramsFromJSON(fHistMan, addHistsStr.Data());
    }
    VarManager::SetUseVars(fHistMan->GetUsedVars()); // provide the list of required variables so that VarManager knows what to fill
    fOutputList.setObject(fHistMan->GetMainHistogramList());
  }

  void DefineCuts()
  {
    // Event cuts
    fEventCut = new AnalysisCompositeCut(true);
    TString eventCutStr = fConfigCuts.fConfigEventCuts.value;
    if (eventCutStr != "") {
      fEventCut->AddCut(dqcuts::GetAnalysisCut(eventCutStr.Data()));
    }
    // Extra event cuts via JSON
    TString addEvCutsStr = fConfigCuts.fConfigEventCutsJSON.value;
    if (addEvCutsStr != "") {
      std::vector<AnalysisCut*> addEvCuts = dqcuts::GetCutsFromJSON(addEvCutsStr.Data());
      for (auto& cutIt : addEvCuts) {
        fEventCut->AddCut(cutIt);
      }
    }

    // Barrel track cuts
    TString cutNamesStr = fConfigCuts.fConfigTrackCuts.value;
    if (!cutNamesStr.IsNull()) {
      std::unique_ptr<TObjArray> objArray(cutNamesStr.Tokenize(","));
      for (int icut = 0; icut < objArray->GetEntries(); ++icut) {
        fTrackCuts.push_back(dqcuts::GetCompositeCut(objArray->At(icut)->GetName()));
      }
    }
    // extra cuts via JSON
    TString addTrackCutsStr = fConfigCuts.fConfigTrackCutsJSON.value;
    if (addTrackCutsStr != "") {
      std::vector<AnalysisCut*> addTrackCuts = dqcuts::GetCutsFromJSON(addTrackCutsStr.Data());
      for (auto& t : addTrackCuts) {
        fTrackCuts.push_back(reinterpret_cast<AnalysisCompositeCut*>(t));
      }
    }

    // Muon cuts
    cutNamesStr = fConfigCuts.fConfigMuonCuts.value;
    if (!cutNamesStr.IsNull()) {
      std::unique_ptr<TObjArray> objArray(cutNamesStr.Tokenize(","));
      for (int icut = 0; icut < objArray->GetEntries(); ++icut) {
        fMuonCuts.push_back(dqcuts::GetCompositeCut(objArray->At(icut)->GetName()));
      }
    }
    // Extra cuts via JSON
    TString addMuonCutsStr = fConfigCuts.fConfigMuonCutsJSON.value;
    if (addMuonCutsStr != "") {
      std::vector<AnalysisCut*> addMuonCuts = dqcuts::GetCutsFromJSON(addMuonCutsStr.Data());
      for (auto& t : addMuonCuts) {
        fMuonCuts.push_back(reinterpret_cast<AnalysisCompositeCut*>(t));
      }
    }

    VarManager::SetUseVars(AnalysisCut::fgUsedVars); // provide the list of required variables so that VarManager knows what to fill
  }

  void DefineHistograms(TString histClasses)
  {
    // Create histograms via HistogramManager
    std::unique_ptr<TObjArray> objArray(histClasses.Tokenize(";"));
    for (Int_t iclass = 0; iclass < objArray->GetEntries(); ++iclass) {
      TString classStr = objArray->At(iclass)->GetName();
      if (fConfigHistOutput.fConfigQA) {
        fHistMan->AddHistClass(classStr.Data());
      }

      // fill the THn histograms
      if (fConfigPostCalibTPC.fConfigIsOnlyforMaps) {
        if (classStr.Contains("PostCalibElectron")) {
          dqhistograms::DefineHistograms(fHistMan, objArray->At(iclass)->GetName(), "track", "postcalib_electron");
        }
        if (classStr.Contains("PostCalibPion")) {
          dqhistograms::DefineHistograms(fHistMan, objArray->At(iclass)->GetName(), "track", "postcalib_pion");
        }
        if (classStr.Contains("PostCalibProton")) {
          dqhistograms::DefineHistograms(fHistMan, objArray->At(iclass)->GetName(), "track", "postcalib_proton");
        }
      }

      TString histEventName = fConfigHistOutput.fConfigAddEventHistogram.value;
      if (classStr.Contains("Event")) {
        if (fConfigHistOutput.fConfigQA) {
          dqhistograms::DefineHistograms(fHistMan, objArray->At(iclass)->GetName(), "event", histEventName);
        }
      }

      TString histTrackName = fConfigHistOutput.fConfigAddTrackHistogram.value;
      if (classStr.Contains("Track")) {
        if (fConfigHistOutput.fConfigQA) {
          dqhistograms::DefineHistograms(fHistMan, objArray->At(iclass)->GetName(), "track", histTrackName);
        }
      }

      TString histMuonName = fConfigHistOutput.fConfigAddMuonHistogram.value;
      if (classStr.Contains("Muons")) {
        if (fConfigHistOutput.fConfigQA) {
          dqhistograms::DefineHistograms(fHistMan, objArray->At(iclass)->GetName(), "track", histMuonName);
        }
      }

      TString histMftName = fConfigHistOutput.fConfigAddMuonHistogram.value;
      if (classStr.Contains("Mft")) {
        if (fConfigHistOutput.fConfigDetailedQA) {
          dqhistograms::DefineHistograms(fHistMan, objArray->At(iclass)->GetName(), "track", histMftName);
        }
      }
    }

    // Create statistics histograms which will be stored in the QA output
    // Event statistics: kStatsEvent
    // Track statistics: kStatsTracks
    // Muon statistics: kStatsMuons
    // Orphan track statistics: kStatsOrphanTracks
    // Zorro information: kStatsZorroInfo
    // Zorro trigger selection: kStatsZorroSel
    fStatsList.setObject(new TList());
    fStatsList->SetOwner(kTRUE);
    std::vector<TString> eventLabels{"BCs", "Collisions before filtering", "Before cuts", "After cuts"};
    TH2D* histEvents = new TH2D("EventStats", "Event statistics", eventLabels.size(), -0.5, eventLabels.size() - 0.5, o2::aod::evsel::kNsel + 1, -0.5, static_cast<double>(o2::aod::evsel::kNsel) + 0.5);
    int ib = 1;
    for (auto label = eventLabels.begin(); label != eventLabels.end(); label++, ib++) {
      histEvents->GetXaxis()->SetBinLabel(ib, (*label).Data());
    }
    for (int ib = 1; ib <= o2::aod::evsel::kNsel; ib++) {
      histEvents->GetYaxis()->SetBinLabel(ib, o2::aod::evsel::selectionLabels[ib - 1]);
    }
    histEvents->GetYaxis()->SetBinLabel(o2::aod::evsel::kNsel + 1, "Total");
    fStatsList->AddAt(histEvents, kStatsEvent);

    // Track statistics: one bin for each track selection and 5 bins for V0 tags (gamma, K0s, Lambda, anti-Lambda, Omega)
    TH1D* histTracks = new TH1D("TrackStats", "Track statistics", fTrackCuts.size() + 5.0, -0.5, fTrackCuts.size() - 0.5 + 5.0);
    ib = 1;
    for (auto cut = fTrackCuts.begin(); cut != fTrackCuts.end(); cut++, ib++) {
      histTracks->GetXaxis()->SetBinLabel(ib, (*cut)->GetName());
    }
    const char* v0TagNames[5] = {"Photon conversion", "K^{0}_{s}", "#Lambda", "#bar{#Lambda}", "#Omega"};
    for (ib = 0; ib < 5; ib++) {
      histTracks->GetXaxis()->SetBinLabel(fTrackCuts.size() + 1 + ib, v0TagNames[ib]);
    }
    fStatsList->AddAt(histTracks, kStatsTracks);

    TH1D* histMuons = new TH1D("MuonStats", "Muon statistics", fMuonCuts.size(), -0.5, fMuonCuts.size() - 0.5);
    ib = 1;
    for (auto cut = fMuonCuts.begin(); cut != fMuonCuts.end(); cut++, ib++) {
      histMuons->GetXaxis()->SetBinLabel(ib, (*cut)->GetName());
    }
    fStatsList->AddAt(histMuons, kStatsMuons);

    TH1D* histOrphanTracks = new TH1D("histOrphanTracks", "Orphan Track statistics", 2, -1, 1);
    histOrphanTracks->GetXaxis()->SetBinLabel(1, "Track w/o collision ID");
    histOrphanTracks->GetXaxis()->SetBinLabel(2, "Track with +ve collision ID");
    fStatsList->AddAt(histOrphanTracks, kStatsOrphanTracks);

    TH2D* histZorroInfo = new TH2D("ZorroInfo", "Zorro information", 1, -0.5, 0.5, 1, -0.5, 0.5);
    fStatsList->AddAt(histZorroInfo, kStatsZorroInfo);

    TH2D* histZorroSel = new TH2D("ZorroSel", "trigger of interested", 1, -0.5, 0.5, 1, -0.5, 0.5);
    fStatsList->AddAt(histZorroSel, kStatsZorroSel);
  }

  template <typename TEvents, typename TTracks, typename TBCs>
  void computeOccupancyEstimators(TEvents const& collisions, Partition<TTracks> const& tracksPos, Partition<TTracks> const& tracksNeg, Preslice<TTracks>& preslice, TBCs const&)
  {

    // clear the occupancy maps for this time frame
    fOccup.oMeanTimeLongA.clear();
    fOccup.oMeanTimeLongC.clear();
    fOccup.oMeanTimeShortA.clear();
    fOccup.oMeanTimeShortC.clear();
    fOccup.oMedianTimeLongA.clear();
    fOccup.oMedianTimeLongC.clear();
    fOccup.oMedianTimeShortA.clear();
    fOccup.oMedianTimeShortC.clear();
    fOccup.oContribLongA.clear();
    fOccup.oContribLongC.clear();
    fOccup.oContribShortA.clear();
    fOccup.oContribShortC.clear();

    std::map<int64_t, int64_t> oBC;                      // key: collision index; value: global BC
    std::map<int64_t, std::vector<int64_t>> oBCreversed; // key: global BC, value: list of collisions attached to this BC
    std::map<int64_t, float> oVtxZ;                      // key: collision index; value: vtx-z position
    std::map<int64_t, int32_t> collMultPos;              // key: collision index; value: tpc multiplicity on the A side
    std::map<int64_t, int32_t> collMultNeg;              // key: collision index; value: tpc multiplicity on the C side

    const double bcUS = o2::constants::lhc::LHCBunchSpacingNS / 1000.0;               // BC spacing in micro-seconds
    const double vdrift = 2.5;                                                        // cm / mus
    int32_t bcShortPast = std::lrint(fConfigVariousOptions.fTPCShortPast / bcUS);     // (close in time collisions) 8 micro-seconds in BC intervals
    int32_t bcShortFuture = std::lrint(fConfigVariousOptions.fTPCShortFuture / bcUS); // (close in time collisions) 8 micro-seconds in BC intervals
    int32_t bcLongPast = std::lrint(fConfigVariousOptions.fTPCLongPast / bcUS);       // (wide time range collisions) past 40 micro-seconds in BC intervals
    int32_t bcLongFuture = std::lrint(fConfigVariousOptions.fTPCLongFuture / bcUS);   // // (wide time range collisions) future 100 micro-seconds in BC intervals

    // Loop over collisions and extract needed info (BC, vtxZ, multiplicity separately in A and C sides)
    for (const auto& collision : collisions) {

      auto bcEvSel = collision.template foundBC_as<TBCs>();
      int64_t bc = bcEvSel.globalBC();
      oBC[collision.globalIndex()] = bc;
      oVtxZ[collision.globalIndex()] = collision.posZ();

      // if more than one collision per bunch, add that collision to the list for that bunch
      if (oBCreversed.find(bc) == oBCreversed.end()) {
        std::vector<int64_t> evs = {collision.globalIndex()};
        oBCreversed[bc] = evs;
      } else {
        auto& evs = oBCreversed[bc];
        evs.push_back(collision.globalIndex());
      }

      // make a slice for this collision and get the number of tracks
      auto thisCollTrackPos = tracksPos.sliceBy(preslice, collision.globalIndex());
      auto thisCollTrackNeg = tracksNeg.sliceBy(preslice, collision.globalIndex());
      collMultPos[collision.globalIndex()] = thisCollTrackPos.size();
      collMultNeg[collision.globalIndex()] = thisCollTrackNeg.size();
    }

    // loop over collisions and sum the multiplicity in the past and future
    for (const auto& [collision, bc] : oBC) {

      int64_t pastShortBC = oBCreversed.lower_bound(bc - bcShortPast)->first;
      int64_t futureShortBC = oBCreversed.lower_bound(bc + bcShortFuture)->first;
      int64_t pastLongBC = oBCreversed.lower_bound(bc - bcLongPast)->first;
      int64_t futureLongBC = oBCreversed.lower_bound(bc + bcLongFuture)->first;

      fOccup.oContribLongA[collision] = 0;
      fOccup.oContribLongC[collision] = 0;
      fOccup.oMeanTimeLongA[collision] = 0.0;
      fOccup.oMeanTimeLongC[collision] = 0.0;
      fOccup.oContribShortA[collision] = 0;
      fOccup.oContribShortC[collision] = 0;
      fOccup.oMeanTimeShortA[collision] = 0.0;
      fOccup.oMeanTimeShortC[collision] = 0.0;
      std::map<float, int> oTimeMapShortA;
      std::map<float, int> oTimeMapShortC;
      std::map<float, int> oTimeMapLongA;
      std::map<float, int> oTimeMapLongC;
      // loop over the BCs in the past and future wrt this one
      for (auto bcIt = oBCreversed.find(pastLongBC); bcIt != oBCreversed.find(futureLongBC); ++bcIt) {
        int64_t thisBC = bcIt->first;
        auto colls = bcIt->second;
        // delta time due to the different BCs
        float dt = (thisBC - bc) * bcUS;
        // check if this collision is also within the short time range
        bool isShort = (thisBC >= pastShortBC && thisBC < futureShortBC);
        // loop over all collisions in this BC
        for (auto& thisColl : colls) {
          // skip if this is the same collision
          if (thisColl == collision) {
            continue;
          }
          // compute the delta time due to the difference in longitudinal position
          float dtDrift = (oVtxZ[thisColl] - oVtxZ[collision]) / vdrift;

          if (!(fConfigVariousOptions.fExcludeShort && isShort)) {
            // sum the collision multiplicity on A and C sides
            fOccup.oContribLongA[collision] += collMultPos[thisColl];
            fOccup.oContribLongC[collision] += collMultNeg[thisColl];
            // compute the multiplicity weighted average time
            fOccup.oMeanTimeLongA[collision] += collMultPos[thisColl] * (dt + dtDrift);
            fOccup.oMeanTimeLongC[collision] += collMultNeg[thisColl] * (dt - dtDrift);
            // fill the time map
            oTimeMapLongA[dt + dtDrift] = collMultPos[thisColl];
            oTimeMapLongC[dt - dtDrift] = collMultNeg[thisColl];
          }

          if (isShort) {
            fOccup.oContribShortA[collision] += collMultPos[thisColl];
            fOccup.oContribShortC[collision] += collMultNeg[thisColl];
            fOccup.oMeanTimeShortA[collision] += collMultPos[thisColl] * (dt + dtDrift);
            fOccup.oMeanTimeShortC[collision] += collMultNeg[thisColl] * (dt - dtDrift);
            oTimeMapShortA[dt + dtDrift] = collMultPos[thisColl];
            oTimeMapShortC[dt - dtDrift] = collMultNeg[thisColl];
          }
        }
      }
      // normalize to obtain the mean time
      if (fOccup.oContribLongA[collision] > 0) {
        fOccup.oMeanTimeLongA[collision] /= fOccup.oContribLongA[collision];
      }
      if (fOccup.oContribLongC[collision] > 0) {
        fOccup.oMeanTimeLongC[collision] /= fOccup.oContribLongC[collision];
      }
      if (fOccup.oContribShortA[collision] > 0) {
        fOccup.oMeanTimeShortA[collision] /= fOccup.oContribShortA[collision];
      }
      if (fOccup.oContribShortC[collision] > 0) {
        fOccup.oMeanTimeShortC[collision] /= fOccup.oContribShortC[collision];
      }
      // iterate over the time maps to obtain the median time
      fOccup.oMedianTimeLongA[collision] = 0.0;
      float sumMult = 0.0;
      if (oTimeMapLongA.size() > 0) {
        for (auto& [dt, mult] : oTimeMapLongA) {
          sumMult += mult;
          if (sumMult > fOccup.oContribLongA[collision] / 2.0) {
            fOccup.oMedianTimeLongA[collision] = dt;
            break;
          }
        }
      }
      fOccup.oMedianTimeLongC[collision] = 0.0;
      sumMult = 0.0;
      if (oTimeMapLongC.size() > 0) {
        for (auto& [dt, mult] : oTimeMapLongC) {
          sumMult += mult;
          if (sumMult > fOccup.oContribLongC[collision] / 2.0) {
            fOccup.oMedianTimeLongC[collision] = dt;
            break;
          }
        }
      }
      fOccup.oMedianTimeShortA[collision] = 0.0;
      sumMult = 0.0;
      if (oTimeMapShortA.size() > 0) {
        for (auto& [dt, mult] : oTimeMapShortA) {
          sumMult += mult;
          if (sumMult > fOccup.oContribShortA[collision] / 2.0) {
            fOccup.oMedianTimeShortA[collision] = dt;
            break;
          }
        }
      }
      fOccup.oMedianTimeShortC[collision] = 0.0;
      sumMult = 0.0;
      if (oTimeMapShortC.size() > 0) {
        for (auto& [dt, mult] : oTimeMapShortC) {
          sumMult += mult;
          if (sumMult > fOccup.oContribShortC[collision] / 2.0) {
            fOccup.oMedianTimeShortC[collision] = dt;
            break;
          }
        }
      }
    } // end loop over collisions
  }

  template <uint32_t TEventFillMap, uint32_t TTrackFillMap, typename TEvents, typename TBCs, typename TZdcs, typename TTrackAssoc, typename TTracks>
  void skimCollisions(TEvents const& collisions, TBCs const& /*bcs*/, TZdcs const& /*zdcs*/,
                      TTrackAssoc const& trackAssocs, TTracks const& tracks)
  {
    // Skim collisions
    // NOTE: So far, collisions are filtered based on the user specified analysis cuts AND the filterPP or Zorro event filter.
    //      The collision-track associations which point to an event that is not selected for writing are discarded!

    fCollIndexMap.clear();
    int multTPC = -1.0;
    float multFV0A = -1.0;
    float multFV0C = -1.0;
    float multFT0A = -1.0;
    float multFT0C = -1.0;
    float multFDDA = -1.0;
    float multFDDC = -1.0;
    float multZNA = -1.0;
    float multZNC = -1.0;
    int multTracklets = -1.0;
    int multTracksPV = -1.0;
    float centFT0C = -1.0;
    float centFT0A = -1.0;
    float centFT0M = -1.0;

    for (const auto& collision : collisions) {

      for (int i = 0; i < o2::aod::evsel::kNsel; i++) {
        if (collision.selection_bit(i)) {
          (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(1.0, static_cast<float>(i));
        }
      }
      (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(1.0, static_cast<float>(o2::aod::evsel::kNsel));

      // apply the event filter computed by filter-PP
      if constexpr ((TEventFillMap & VarManager::ObjTypes::EventFilter) > 0 || (TEventFillMap & VarManager::ObjTypes::RapidityGapFilter) > 0) {
        if (!collision.eventFilter()) {
          continue;
        }
      }

      auto bc = collision.template bc_as<TBCs>();
      // store the selection decisions
      uint64_t tag = 0;
      // store some more information in the tag
      // if the BC found by event selection does not coincide with the collision.bc()
      auto bcEvSel = collision.template foundBC_as<TBCs>();
      if (bcEvSel.globalIndex() != bc.globalIndex()) {
        tag |= (static_cast<uint64_t>(1) << 0);
      }
      // Put the 8 first bits of the event filter in the last 8 bits of the tag
      if constexpr ((TEventFillMap & VarManager::ObjTypes::EventFilter) > 0 || (TEventFillMap & VarManager::ObjTypes::RapidityGapFilter) > 0) {
        tag |= (collision.eventFilter() << 56);
      }

      VarManager::ResetValues(0, VarManager::kNEventWiseVariables);
      VarManager::FillBC(bc);
      VarManager::FillEvent<TEventFillMap>(collision); // extract event information and place it in the fValues array
      if constexpr ((TEventFillMap & VarManager::ObjTypes::Zdc) > 0) {
        if constexpr ((TEventFillMap & VarManager::ObjTypes::RapidityGapFilter) > 0) {
          // Collision table already has ZDC info
          VarManager::FillZDC(collision);
        } else if (bcEvSel.has_zdc()) {
          auto bc_zdc = bcEvSel.zdc();
          VarManager::FillZDC(bc_zdc);
        }
      }
      if constexpr ((TEventFillMap & VarManager::ObjTypes::CollisionMultExtra) > 0 && (TTrackFillMap & VarManager::ObjTypes::Track) > 0 && (TTrackFillMap & VarManager::ObjTypes::TrackDCA) > 0) {
        auto groupedTrackIndices = trackAssocs.sliceBy(trackIndicesPerCollision, collision.globalIndex());
        VarManager::FillEventTrackEstimators<TTrackFillMap>(collision, groupedTrackIndices, tracks);
      }
      // Exceptionally fill the TPC occupancy quantities here
      if constexpr ((TEventFillMap & VarManager::ObjTypes::CollisionMultExtra) > 0) {
        VarManager::fgValues[VarManager::kNTPCcontribLongA] = fOccup.oContribLongA[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCcontribLongC] = fOccup.oContribLongC[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmeanTimeLongA] = fOccup.oMeanTimeLongA[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmeanTimeLongC] = fOccup.oMeanTimeLongC[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmedianTimeLongA] = fOccup.oMedianTimeLongA[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmedianTimeLongC] = fOccup.oMedianTimeLongC[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCcontribShortA] = fOccup.oContribShortA[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCcontribShortC] = fOccup.oContribShortC[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmeanTimeShortA] = fOccup.oMeanTimeShortA[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmeanTimeShortC] = fOccup.oMeanTimeShortC[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmedianTimeShortA] = fOccup.oMedianTimeShortA[collision.globalIndex()];
        VarManager::fgValues[VarManager::kNTPCmedianTimeShortC] = fOccup.oMedianTimeShortC[collision.globalIndex()];
      }
      if (fDoDetailedQA) {
        fHistMan->FillHistClass("Event_BeforeCuts", VarManager::fgValues);
      }

      // fill stats information, before selections
      for (int i = 0; i < o2::aod::evsel::kNsel; i++) {
        if (collision.selection_bit(i)) {
          (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(2.0, static_cast<float>(i));
        }
      }
      (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(2.0, static_cast<float>(o2::aod::evsel::kNsel));

      if (fConfigZorro.fConfigRunZorro) {
        zorro.setBaseCCDBPath(fConfigCCDB.fConfigCcdbPathZorro.value);
        zorro.setBCtolerance(fConfigZorro.fBcTolerance);
        zorro.initCCDB(fCCDB.service, fCurrentRun, bc.timestamp(), fConfigZorro.fConfigZorroTrigMask.value);
        zorro.populateExternalHists(fCurrentRun, reinterpret_cast<TH2D*>(fStatsList->At(kStatsZorroInfo)), reinterpret_cast<TH2D*>(fStatsList->At(kStatsZorroSel)));
        bool zorroSel = zorro.isSelected(bc.globalBC(), fConfigZorro.fBcTolerance, reinterpret_cast<TH2D*>(fStatsList->At(kStatsZorroSel)));
        if (zorroSel) {
          tag |= (static_cast<uint64_t>(true) << 56); // the same bit is used for this zorro selections from ccdb
        }
        if (fConfigZorro.fConfigRunZorroSel && (!zorroSel || !fEventCut->IsSelected(VarManager::fgValues))) {
          continue;
        }
      } else {
        if (!fEventCut->IsSelected(VarManager::fgValues)) {
          continue;
        }
      }

      // fill stats information, after selections
      for (int i = 0; i < o2::aod::evsel::kNsel; i++) {
        if (collision.selection_bit(i)) {
          (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(3.0, static_cast<float>(i));
        }
      }
      (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(3.0, static_cast<float>(o2::aod::evsel::kNsel));

      fHistMan->FillHistClass("Event_AfterCuts", VarManager::fgValues);

      // create the event tables
      event(tag, bc.runNumber(), collision.posX(), collision.posY(), collision.posZ(), collision.numContrib(), collision.collisionTime(), collision.collisionTimeRes());
      if constexpr ((TEventFillMap & VarManager::ObjTypes::CollisionMult) > 0) {
        multFV0C = collision.multFV0C();
        multTPC = collision.multTPC();
        multZNA = collision.multZNA();
        multZNC = collision.multZNC();
        multTracklets = collision.multTracklets();
        multTracksPV = collision.multNTracksPV();
        if constexpr ((TEventFillMap & VarManager::ObjTypes::RapidityGapFilter) > 0) {
          // Use the FIT signals from the nearest BC with FIT amplitude above threshold
          multFV0A = collision.newBcMultFV0A();
          multFT0A = collision.newBcMultFT0A();
          multFT0C = collision.newBcMultFT0C();
          multFDDA = collision.newBcMultFDDA();
          multFDDC = collision.newBcMultFDDC();
        } else {
          multFV0A = collision.multFV0A();
          multFT0A = collision.multFT0A();
          multFT0C = collision.multFT0C();
          multFDDA = collision.multFDDA();
          multFDDC = collision.multFDDC();
        }
      }
      if constexpr ((TEventFillMap & VarManager::ObjTypes::CollisionCent) > 0) {
        centFT0C = collision.centFT0C();
        centFT0A = collision.centFT0A();
        centFT0M = collision.centFT0M();
      }
      eventExtended(bc.globalBC(), collision.alias_raw(), collision.selection_raw(), bc.timestamp(), VarManager::fgValues[VarManager::kCentVZERO],
                    multTPC, multFV0A, multFV0C, multFT0A, multFT0C, multFDDA, multFDDC, multZNA, multZNC, multTracklets, multTracksPV, centFT0C, centFT0A, centFT0M);
      eventVtxCov(collision.covXX(), collision.covXY(), collision.covXZ(), collision.covYY(), collision.covYZ(), collision.covZZ(), collision.chi2());
      eventInfo(collision.globalIndex());
      if constexpr ((TEventFillMap & VarManager::ObjTypes::Zdc) > 0) {
        if constexpr ((TEventFillMap & VarManager::ObjTypes::RapidityGapFilter) > 0) {
          // ZDC information is already in the DQRapidityGapFilter
          zdc(collision.energyCommonZNA(), collision.energyCommonZNC(), collision.energyCommonZPA(), collision.energyCommonZPC(),
              collision.timeZNA(), collision.timeZNC(), collision.timeZPA(), collision.timeZPC());
        } else if (bcEvSel.has_zdc()) {
          auto bc_zdc = bcEvSel.zdc();
          zdc(bc_zdc.energyCommonZNA(), bc_zdc.energyCommonZNC(), bc_zdc.energyCommonZPA(), bc_zdc.energyCommonZPC(),
              bc_zdc.timeZNA(), bc_zdc.timeZNC(), bc_zdc.timeZPA(), bc_zdc.timeZPC());
        } else {
          zdc(-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0);
        }
      }
      if constexpr ((TEventFillMap & VarManager::ObjTypes::CollisionMultExtra) > 0) {
        multPV(collision.multNTracksHasITS(), collision.multNTracksHasTPC(), collision.multNTracksHasTOF(), collision.multNTracksHasTRD(),
               collision.multNTracksITSOnly(), collision.multNTracksTPCOnly(), collision.multNTracksITSTPC(),
               collision.multNTracksPVeta1(), collision.multNTracksPVetaHalf(), collision.trackOccupancyInTimeRange(), collision.ft0cOccupancyInTimeRange());

        multAll(collision.multAllTracksTPCOnly(), collision.multAllTracksITSTPC(),
                fOccup.oContribLongA[collision.globalIndex()], fOccup.oContribLongC[collision.globalIndex()],
                fOccup.oMeanTimeLongA[collision.globalIndex()], fOccup.oMeanTimeLongC[collision.globalIndex()],
                fOccup.oMedianTimeLongA[collision.globalIndex()], fOccup.oMedianTimeLongC[collision.globalIndex()],
                fOccup.oContribShortA[collision.globalIndex()], fOccup.oContribShortC[collision.globalIndex()],
                fOccup.oMeanTimeShortA[collision.globalIndex()], fOccup.oMeanTimeShortC[collision.globalIndex()],
                fOccup.oMedianTimeShortA[collision.globalIndex()], fOccup.oMedianTimeShortC[collision.globalIndex()]);
      }

      fCollIndexMap[collision.globalIndex()] = event.lastIndex();
    }
  }

  template <uint32_t TTrackFillMap, typename TEvent, typename TBCs, typename TTracks>
  void skimTracks(TEvent const& collision, TBCs const& /*bcs*/, TTracks const& /*tracks*/, TrackAssoc const& assocs)
  {
    // Skim the barrel tracks
    // Loop over the collision-track associations, retrieve the track, and apply track cuts for selection
    //     One can apply here cuts which depend on the association (e.g. DCA), which will discard (hopefully most) wrong associations.
    //     Tracks are written only once, even if they constribute to more than one association

    uint64_t trackFilteringTag = static_cast<uint64_t>(0);
    uint32_t trackTempFilterMap = static_cast<uint32_t>(0);

    // material correction for track propagation
    // TODO: Do we need a configurable to switch between different material correction options?
    // o2::base::Propagator::MatCorrType matCorr = o2::base::Propagator::MatCorrType::USEMatCorrLUT;
    o2::base::Propagator::MatCorrType noMatCorr = o2::base::Propagator::MatCorrType::USEMatCorrNONE;

    for (const auto& assoc : assocs) {
      // get the track
      auto track = assoc.template track_as<TTracks>();

      // If the original collision of this track was not selected for skimming, then we skip this track.
      //  Normally, the filter-pp is selecting all collisions which contain the tracks which contributed to the triggering
      //    of an event, so this is rejecting possibly a few tracks unrelated to the trigger, originally associated with collisions distant in time.
      if (fCollIndexMap.find(track.collisionId()) == fCollIndexMap.end()) {
        continue;
      }

      trackFilteringTag = static_cast<uint64_t>(0);
      trackTempFilterMap = static_cast<uint32_t>(0);
      VarManager::FillTrack<TTrackFillMap>(track);

      // compute quantities which depend on the associated collision, such as DCA
      if (fConfigVariousOptions.fPropTrack && (track.collisionId() != collision.globalIndex())) {
        VarManager::FillTrackCollisionMatCorr<TTrackFillMap>(track, collision, noMatCorr, o2::base::Propagator::Instance());
      }

      if (fDoDetailedQA) {
        fHistMan->FillHistClass("TrackBarrel_BeforeCuts", VarManager::fgValues);
      }

      // apply track cuts and fill stats histogram
      int i = 0;
      for (auto cut = fTrackCuts.begin(); cut != fTrackCuts.end(); cut++, i++) {
        if ((*cut)->IsSelected(VarManager::fgValues)) {
          trackTempFilterMap |= (static_cast<uint32_t>(1) << i);
          // NOTE: the QA is filled here just for the first occurence of this track.
          //    So if there are histograms of quantities which depend on the collision association, these will not be accurate
          if (fConfigHistOutput.fConfigQA && (fTrackIndexMap.find(track.globalIndex()) == fTrackIndexMap.end())) {
            fHistMan->FillHistClass(Form("TrackBarrel_%s", (*cut)->GetName()), VarManager::fgValues);
          }
          (reinterpret_cast<TH1D*>(fStatsList->At(kStatsTracks)))->Fill(static_cast<float>(i));
        }
      }
      if (!trackTempFilterMap) {
        continue;
      }

      // If this track is already present in the index map, it means it was already skimmed,
      // so we just store the association and we skip the track
      if (fTrackIndexMap.find(track.globalIndex()) != fTrackIndexMap.end()) {
        trackBarrelAssoc(fCollIndexMap[collision.globalIndex()], fTrackIndexMap[track.globalIndex()]);
        continue;
      }

      // store selection information in the track tag
      if constexpr (static_cast<bool>(TTrackFillMap & VarManager::ObjTypes::TrackV0Bits)) { // BIT0-4: V0Bits
        trackFilteringTag |= static_cast<uint64_t>(track.pidbit());
        for (int iv0 = 0; iv0 < 5; iv0++) {
          if (track.pidbit() & (uint8_t(1) << iv0)) {
            (reinterpret_cast<TH1D*>(fStatsList->At(kStatsTracks)))->Fill(fTrackCuts.size() + static_cast<float>(iv0));
          }
        }
        if (fConfigPostCalibTPC.fConfigIsOnlyforMaps) {
          if (trackFilteringTag & (static_cast<uint64_t>(1) << VarManager::kIsConversionLeg)) { // for electron
            fHistMan->FillHistClass("TrackBarrel_PostCalibElectron", VarManager::fgValues);
          }
          if (trackFilteringTag & (static_cast<uint64_t>(1) << VarManager::kIsK0sLeg)) { // for pion
            fHistMan->FillHistClass("TrackBarrel_PostCalibPion", VarManager::fgValues);
          }
          if ((static_cast<bool>(trackFilteringTag & (static_cast<uint64_t>(1) << VarManager::kIsLambdaLeg)) * (track.sign()) > 0)) { // for proton from Lambda
            fHistMan->FillHistClass("TrackBarrel_PostCalibProton", VarManager::fgValues);
          }
          if ((static_cast<bool>(trackFilteringTag & (static_cast<uint64_t>(1) << VarManager::kIsALambdaLeg)) * (track.sign()) < 0)) { // for proton from AntiLambda
            fHistMan->FillHistClass("TrackBarrel_PostCalibProton", VarManager::fgValues);
          }
        }
        if (fConfigPostCalibTPC.fConfigSaveElectronSample) { // only save electron sample
          if (!(trackFilteringTag & (static_cast<uint64_t>(1) << VarManager::kIsConversionLeg))) {
            continue;
          }
        }
      } // end if V0Bits
      if constexpr (static_cast<bool>(TTrackFillMap & VarManager::ObjTypes::DalitzBits)) {
        trackFilteringTag |= (static_cast<uint64_t>(track.dalitzBits()) << VarManager::kDalitzBits); // BIT5-12: Dalitz
      }
      trackFilteringTag |= (static_cast<uint64_t>(trackTempFilterMap) << VarManager::kBarrelUserCutsBits); // BIT13-...:  user track filters

      if constexpr (static_cast<bool>(TTrackFillMap & VarManager::ObjTypes::TrackPID)) {
        if (fConfigPostCalibTPC.fConfigComputeTPCpostCalib) {
          trackFilteringTag |= (static_cast<uint64_t>(1) << VarManager::kIsTPCPostcalibrated);
        }
      }

      // Calculating the percentage of orphan tracks i.e., tracks which have no collisions associated to it
      if (!track.has_collision()) {
        (reinterpret_cast<TH1D*>(fStatsList->At(kStatsOrphanTracks)))->Fill(static_cast<float>(-1));
      } else {
        (reinterpret_cast<TH1D*>(fStatsList->At(kStatsOrphanTracks)))->Fill(0.9);
      }

      // NOTE: The collision ID written in the table is the one of the original collision assigned in the AO2D.
      //      The reason is that the time associated to the track is wrt that collision.
      //      If new associations are done with the skimmed data, the track time wrt new collision can then be recomputed based on the
      //        relative difference in time between the original and the new collision.
      uint32_t reducedEventIdx = fCollIndexMap[track.collisionId()]; // This gives the original iD of the track

      // NOTE: trackBarrelInfo stores the index of the collision as in AO2D (for use in some cases where the analysis on skims is done
      //   in workflows where the original AO2Ds are also present)
      trackBarrelInfo(collision.globalIndex(), collision.posX(), collision.posY(), collision.posZ(), track.globalIndex());
      trackBasic(reducedEventIdx, trackFilteringTag, track.pt(), track.eta(), track.phi(), track.sign(), 0);
      trackBarrel(track.x(), track.alpha(), track.y(), track.z(), track.snp(), track.tgl(), track.signed1Pt(),
                  track.tpcInnerParam(), track.flags(), track.itsClusterMap(), track.itsChi2NCl(),
                  track.tpcNClsFindable(), track.tpcNClsFindableMinusFound(), track.tpcNClsFindableMinusCrossedRows(),
                  track.tpcNClsShared(), track.tpcChi2NCl(),
                  track.trdChi2(), track.trdPattern(), track.tofChi2(),
                  track.length(), track.dcaXY(), track.dcaZ(),
                  track.trackTime(), track.trackTimeRes(), track.tofExpMom(),
                  track.detectorMap());
      if constexpr (static_cast<bool>(TTrackFillMap & VarManager::ObjTypes::TrackCov)) {
        trackBarrelCov(track.cYY(), track.cZY(), track.cZZ(), track.cSnpY(), track.cSnpZ(),
                       track.cSnpSnp(), track.cTglY(), track.cTglZ(), track.cTglSnp(), track.cTglTgl(),
                       track.c1PtY(), track.c1PtZ(), track.c1PtSnp(), track.c1PtTgl(), track.c1Pt21Pt2());
      }
      if constexpr (static_cast<bool>(TTrackFillMap & VarManager::ObjTypes::TrackPID)) {
        float nSigmaEl = (fConfigPostCalibTPC.fConfigComputeTPCpostCalib ? VarManager::fgValues[VarManager::kTPCnSigmaEl_Corr] : track.tpcNSigmaEl());
        float nSigmaPi = (fConfigPostCalibTPC.fConfigComputeTPCpostCalib ? VarManager::fgValues[VarManager::kTPCnSigmaPi_Corr] : track.tpcNSigmaPi());
        float nSigmaKa = ((fConfigPostCalibTPC.fConfigComputeTPCpostCalib && fConfigPostCalibTPC.fConfigComputeTPCpostCalibKaon) ? VarManager::fgValues[VarManager::kTPCnSigmaKa_Corr] : track.tpcNSigmaKa());
        float nSigmaPr = (fConfigPostCalibTPC.fConfigComputeTPCpostCalib ? VarManager::fgValues[VarManager::kTPCnSigmaPr_Corr] : track.tpcNSigmaPr());
        trackBarrelPID(track.tpcSignal(),
                       nSigmaEl, track.tpcNSigmaMu(), nSigmaPi, nSigmaKa, nSigmaPr,
                       track.beta(), track.tofNSigmaEl(), track.tofNSigmaMu(), track.tofNSigmaPi(), track.tofNSigmaKa(), track.tofNSigmaPr(),
                       track.trdSignal());
      } else if constexpr (static_cast<bool>(TTrackFillMap & VarManager::ObjTypes::TrackTPCPID)) {
        float nSigmaEl = (fConfigPostCalibTPC.fConfigComputeTPCpostCalib ? VarManager::fgValues[VarManager::kTPCnSigmaEl_Corr] : track.tpcNSigmaEl());
        float nSigmaPi = (fConfigPostCalibTPC.fConfigComputeTPCpostCalib ? VarManager::fgValues[VarManager::kTPCnSigmaPi_Corr] : track.tpcNSigmaPi());
        float nSigmaKa = ((fConfigPostCalibTPC.fConfigComputeTPCpostCalib && fConfigPostCalibTPC.fConfigComputeTPCpostCalibKaon) ? VarManager::fgValues[VarManager::kTPCnSigmaKa_Corr] : track.tpcNSigmaKa());
        float nSigmaPr = (fConfigPostCalibTPC.fConfigComputeTPCpostCalib ? VarManager::fgValues[VarManager::kTPCnSigmaPr_Corr] : track.tpcNSigmaPr());
        trackBarrelPID(track.tpcSignal(),
                       nSigmaEl, -999.0, nSigmaPi, nSigmaKa, nSigmaPr,
                       -999.0, -999.0, -999.0, -999.0, -999.0, -999.0,
                       -999.0);
      }

      fTrackIndexMap[track.globalIndex()] = trackBasic.lastIndex();

      // write the skimmed collision - track association
      trackBarrelAssoc(fCollIndexMap[collision.globalIndex()], fTrackIndexMap[track.globalIndex()]);
    } // end loop over associations
  } // end skimTracks

  template <uint32_t TMFTFillMap, typename TEvent, typename TBCs>
  void skimMFT(TEvent const& collision, TBCs const& /*bcs*/, MFTTracks const& /*mfts*/, MFTTrackAssoc const& mftAssocs)
  {
    // Skim MFT tracks
    // So far no cuts are applied here

    for (const auto& assoc : mftAssocs) {
      auto track = assoc.template mfttrack_as<MFTTracks>();

      if (fConfigHistOutput.fConfigQA) {
        VarManager::FillTrack<TMFTFillMap>(track);
        fHistMan->FillHistClass("MftTracks", VarManager::fgValues);
      }

      // write the MFT track global index in the map for skimming (to make sure we have it just once)
      if (fMftIndexMap.find(track.globalIndex()) == fMftIndexMap.end()) {
        uint32_t reducedEventIdx = fCollIndexMap[collision.globalIndex()];
        mftTrack(reducedEventIdx, static_cast<uint64_t>(0), track.pt(), track.eta(), track.phi());
        // TODO: We are not writing the DCA at the moment, because this depend on the collision association
        mftTrackExtra(track.mftClusterSizesAndTrackFlags(), track.sign(), 0.0, 0.0, track.nClusters());

        fMftIndexMap[track.globalIndex()] = mftTrack.lastIndex();
      }
      mftAssoc(fCollIndexMap[collision.globalIndex()], fMftIndexMap[track.globalIndex()]);
    }
  }

  template <uint32_t TMuonFillMap, uint32_t TMFTFillMap, typename TEvent, typename TBCs, typename TMuons, typename TMFTTracks>
  void skimMuons(TEvent const& collision, TBCs const& /*bcs*/, TMuons const& muons, FwdTrackAssoc const& muonAssocs, TMFTTracks const& /*mftTracks*/)
  {
    // Skim the fwd-tracks (muons)
    // Loop over the collision-track associations, recompute track properties depending on the collision assigned, and apply track cuts for selection
    //     Muons are written only once, even if they constribute to more than one association,
    //         which means that in the case of multiple associations, the track parameters are wrong and should be computed again at analysis time.

    // TODO: Currently, the TMFTFillMap is not used in this function. Is it needed ?

    uint8_t trackFilteringTag = static_cast<uint8_t>(0);
    uint8_t trackTempFilterMap = static_cast<uint8_t>(0);
    fFwdTrackIndexMapReversed.clear();

    uint32_t offset = muonBasic.lastIndex();
    uint32_t counter = 0;
    for (const auto& assoc : muonAssocs) {
      // get the muon
      auto muon = muons.rawIteratorAt(assoc.fwdtrackId());

      trackFilteringTag = static_cast<uint8_t>(0);
      trackTempFilterMap = static_cast<uint8_t>(0);

      if constexpr (static_cast<bool>(TMuonFillMap & VarManager::ObjTypes::MuonRealign)) {
        // Check refit flag in case of realigned muons
        if (static_cast<bool>(muon.isRemovable())) {
          continue;
        }
      }

      VarManager::FillTrack<TMuonFillMap>(muon);
      // NOTE: Muons are propagated to the current associated collisions.
      //       So if a muon is associated to multiple collisions, depending on the selections,
      //       it may be accepted for some associations and rejected for other
      if (fConfigVariousOptions.fPropMuon) {
        VarManager::FillPropagateMuon<TMuonFillMap>(muon, collision);
      }
      // recalculate pDca and global muon kinematics
      if (static_cast<int>(muon.trackType()) < 2 && fConfigVariousOptions.fRefitGlobalMuon) {
        auto muontrack = muon.template matchMCHTrack_as<TMuons>();
        if (muontrack.eta() < fConfigVariousOptions.fMuonMatchEtaMin || muontrack.eta() > fConfigVariousOptions.fMuonMatchEtaMax) {
          continue;
        }
        VarManager::FillTrackCollision<TMuonFillMap>(muontrack, collision);
        // NOTE: the MFT track originally associated to the MUON track is currently used in the global muon refit
        //       Should MUON - MFT time ambiguities be taken into account ?
        auto mfttrack = muon.template matchMFTTrack_as<MFTTracks>();
        VarManager::FillGlobalMuonRefit<TMuonFillMap>(muontrack, mfttrack, collision);
      } else {
        VarManager::FillTrackCollision<TMuonFillMap>(muon, collision);
      }

      if (fDoDetailedQA) {
        fHistMan->FillHistClass("Muons_BeforeCuts", VarManager::fgValues);
      }
      // check the cuts and filters
      int i = 0;
      for (auto cut = fMuonCuts.begin(); cut != fMuonCuts.end(); cut++, i++) {
        if ((*cut)->IsSelected(VarManager::fgValues)) {
          trackTempFilterMap |= (static_cast<uint8_t>(1) << i);
          // NOTE: the QA is filled here just for the first occurence of this muon, which means the current association
          //     will be skipped from histograms if this muon was already filled in the skimming map.
          //    So if there are histograms of quantities which depend on the collision association, these histograms will not be completely accurate
          if (fConfigHistOutput.fConfigQA && (fFwdTrackIndexMap.find(muon.globalIndex()) == fFwdTrackIndexMap.end())) {
            fHistMan->FillHistClass(Form("Muons_%s", (*cut)->GetName()), VarManager::fgValues);
          }
          (reinterpret_cast<TH1D*>(fStatsList->At(kStatsMuons)))->Fill(static_cast<float>(i));
        }
      }

      // don't skim the muon if no cut has passed
      if (!trackTempFilterMap) {
        continue;
      }
      trackFilteringTag = trackTempFilterMap; // BIT0-7:  user selection cuts

      // update the index map if this is a new muon (it can already exist in the map from a different collision association)
      if (fFwdTrackIndexMap.find(muon.globalIndex()) == fFwdTrackIndexMap.end()) {
        counter++;
        fFwdTrackIndexMap[muon.globalIndex()] = offset + counter;
        fFwdTrackIndexMapReversed[offset + counter] = muon.globalIndex();
        fFwdTrackFilterMap[muon.globalIndex()] = trackFilteringTag;                                                    // store here the filtering tag so we don't repeat the cuts in the second iteration
        if (muon.has_matchMCHTrack() && (fFwdTrackIndexMap.find(muon.matchMCHTrackId()) == fFwdTrackIndexMap.end())) { // write also the matched MCH track
          counter++;
          fFwdTrackIndexMap[muon.matchMCHTrackId()] = offset + counter;
          fFwdTrackIndexMapReversed[offset + counter] = muon.matchMCHTrackId();
          fFwdTrackFilterMap[muon.matchMCHTrackId()] = trackFilteringTag; // store here the filtering tag so we don't repeat the cuts in the second iteration
        }
      } else {
        fFwdTrackFilterMap[muon.globalIndex()] |= trackFilteringTag; // make a bitwise OR with previous existing cuts
      }
      // write the association table
      muonAssoc(fCollIndexMap[collision.globalIndex()], fFwdTrackIndexMap[muon.globalIndex()]);
    } // end loop over assocs

    // Now we have the full index map of selected muons so we can proceed with writing the muon tables
    // Special care needed for the MCH and MFT indices
    for (const auto& [skimIdx, origIdx] : fFwdTrackIndexMapReversed) {
      // get the muon
      auto muon = muons.rawIteratorAt(origIdx);
      uint32_t reducedEventIdx = fCollIndexMap[collision.globalIndex()];
      // NOTE: Currently, one writes in the tables the momentum-vector (pt, eta and phi) of the first collision association for this muon,
      //       while multiple collision associations (and multiple mom vectors can exist).
      //       The momentum can be recomputed at the analysis time based on the associations written in the skims
      //   So all the information which pertains to collision association or MFT associations should not be taken from the skimmed data, but recomputed at analysis time.
      uint32_t mchIdx = -1;
      uint32_t mftIdx = -1;
      if (muon.trackType() == static_cast<uint8_t>(0) || muon.trackType() == static_cast<uint8_t>(2)) { // MCH-MID (2) or global (0)
        if (fFwdTrackIndexMap.find(muon.matchMCHTrackId()) != fFwdTrackIndexMap.end()) {
          mchIdx = fFwdTrackIndexMap[muon.matchMCHTrackId()];
        }
        if (fMftIndexMap.find(muon.matchMFTTrackId()) != fMftIndexMap.end()) {
          mftIdx = fMftIndexMap[muon.matchMFTTrackId()];
        }
      }

      if constexpr (static_cast<bool>(TMuonFillMap & VarManager::ObjTypes::MuonRealign)) {
        // Check refit flag in case of realigned muons
        if (static_cast<bool>(muon.isRemovable())) {
          continue;
        }
      }

      VarManager::FillTrack<TMuonFillMap>(muon);
      if (fConfigVariousOptions.fPropMuon) {
        VarManager::FillPropagateMuon<TMuonFillMap>(muon, collision);
      }
      // recalculte pDca and global muon kinematics
      int globalClusters = muon.nClusters();
      if (static_cast<int>(muon.trackType()) < 2 && fConfigVariousOptions.fRefitGlobalMuon) {
        auto muontrack = muon.template matchMCHTrack_as<TMuons>();
        auto mfttrack = muon.template matchMFTTrack_as<MFTTracks>();
        globalClusters += mfttrack.nClusters();
        VarManager::FillTrackCollision<TMuonFillMap>(muontrack, collision);
        VarManager::FillGlobalMuonRefit<TMuonFillMap>(muontrack, mfttrack, collision);
      } else {
        VarManager::FillTrackCollision<TMuonFillMap>(muon, collision);
      }
      muonBasic(reducedEventIdx, mchIdx, mftIdx, fFwdTrackFilterMap[muon.globalIndex()], VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kPhi], muon.sign(), 0);
      muonExtra(globalClusters, VarManager::fgValues[VarManager::kMuonPDca], VarManager::fgValues[VarManager::kMuonRAtAbsorberEnd],
                VarManager::fgValues[VarManager::kMuonChi2], muon.chi2MatchMCHMID(), muon.chi2MatchMCHMFT(),
                muon.matchScoreMCHMFT(),
                muon.mchBitMap(), muon.midBitMap(),
                muon.midBoards(), muon.trackType(), VarManager::fgValues[VarManager::kMuonDCAx], VarManager::fgValues[VarManager::kMuonDCAy],
                muon.trackTime(), muon.trackTimeRes());
      muonInfo(muon.collisionId(), collision.posX(), collision.posY(), collision.posZ());
      if constexpr (static_cast<bool>(TMuonFillMap & VarManager::ObjTypes::MuonCov) || static_cast<bool>(TMuonFillMap & VarManager::ObjTypes::MuonCovRealign)) {
        muonCov(VarManager::fgValues[VarManager::kX], VarManager::fgValues[VarManager::kY], VarManager::fgValues[VarManager::kZ], VarManager::fgValues[VarManager::kPhi], VarManager::fgValues[VarManager::kTgl], muon.sign() / VarManager::fgValues[VarManager::kPt],
                VarManager::fgValues[VarManager::kMuonCXX], VarManager::fgValues[VarManager::kMuonCXY], VarManager::fgValues[VarManager::kMuonCYY], VarManager::fgValues[VarManager::kMuonCPhiX], VarManager::fgValues[VarManager::kMuonCPhiY], VarManager::fgValues[VarManager::kMuonCPhiPhi],
                VarManager::fgValues[VarManager::kMuonCTglX], VarManager::fgValues[VarManager::kMuonCTglY], VarManager::fgValues[VarManager::kMuonCTglPhi], VarManager::fgValues[VarManager::kMuonCTglTgl], VarManager::fgValues[VarManager::kMuonC1Pt2X], VarManager::fgValues[VarManager::kMuonC1Pt2Y],
                VarManager::fgValues[VarManager::kMuonC1Pt2Phi], VarManager::fgValues[VarManager::kMuonC1Pt2Tgl], VarManager::fgValues[VarManager::kMuonC1Pt21Pt2]);
      }
    } // end loop over selected muons
  } // end skimMuons

  // Produce standard barrel + muon tables with event filter (typically for pp and p-Pb) ------------------------------------------------------
  template <uint32_t TEventFillMap, uint32_t TTrackFillMap, uint32_t TMuonFillMap, uint32_t TMFTFillMap,
            typename TEvents, typename TBCs, typename TZdcs, typename TTracks, typename TMuons, typename TMFTTracks,
            typename TTrackAssoc, typename TFwdTrackAssoc, typename TMFTTrackAssoc>
  void fullSkimming(TEvents const& collisions, TBCs const& bcs, TZdcs const& zdcs,
                    TTracks const& tracksBarrel, TMuons const& muons, TMFTTracks const& mftTracks,
                    TTrackAssoc const& trackAssocs, TFwdTrackAssoc const& fwdTrackAssocs, TMFTTrackAssoc const& mftAssocs)
  {

    if (bcs.size() > 0 && fCurrentRun != bcs.begin().runNumber()) {
      if (fConfigPostCalibTPC.fConfigComputeTPCpostCalib) {
        auto calibList = fCCDB->getForTimeStamp<TList>(fConfigCCDB.fConfigCcdbPathTPC.value, bcs.begin().timestamp());
        VarManager::SetCalibrationObject(VarManager::kTPCElectronMean, calibList->FindObject("mean_map_electron"));
        VarManager::SetCalibrationObject(VarManager::kTPCElectronSigma, calibList->FindObject("sigma_map_electron"));
        VarManager::SetCalibrationObject(VarManager::kTPCPionMean, calibList->FindObject("mean_map_pion"));
        VarManager::SetCalibrationObject(VarManager::kTPCPionSigma, calibList->FindObject("sigma_map_pion"));
        VarManager::SetCalibrationObject(VarManager::kTPCProtonMean, calibList->FindObject("mean_map_proton"));
        VarManager::SetCalibrationObject(VarManager::kTPCProtonSigma, calibList->FindObject("sigma_map_proton"));
        if (fConfigPostCalibTPC.fConfigComputeTPCpostCalibKaon) {
          VarManager::SetCalibrationObject(VarManager::kTPCKaonMean, calibList->FindObject("mean_map_kaon"));
          VarManager::SetCalibrationObject(VarManager::kTPCKaonSigma, calibList->FindObject("sigma_map_kaon"));
        }
      }
      if (fIsRun2 == true) {
        fGrpMagRun2 = fCCDB->getForTimeStamp<o2::parameters::GRPObject>(fConfigCCDB.fConfigGrpMagPathRun2, bcs.begin().timestamp());
        if (fGrpMagRun2 != nullptr) {
          o2::base::Propagator::initFieldFromGRP(fGrpMagRun2);
        }
      } else {
        fGrpMag = fCCDB->getForTimeStamp<o2::parameters::GRPMagField>(fConfigCCDB.fConfigGrpMagPath, bcs.begin().timestamp());
        if (fGrpMag != nullptr) {
          o2::base::Propagator::initFieldFromGRP(fGrpMag);
        }
        if (fConfigVariousOptions.fPropMuon) {
          VarManager::SetupMuonMagField();
        }
      }
      std::map<string, string> metadataRCT, header;
      header = fCCDBApi.retrieveHeaders(Form("RCT/Info/RunInformation/%i", bcs.begin().runNumber()), metadataRCT, -1);
      uint64_t sor = std::atol(header["SOR"].c_str());
      uint64_t eor = std::atol(header["EOR"].c_str());
      VarManager::SetSORandEOR(sor, eor);

      fCurrentRun = bcs.begin().runNumber();
    } // end updating the CCDB quantities at change of run

    // skim collisions
    event.reserve(collisions.size());
    eventExtended.reserve(collisions.size());
    eventVtxCov.reserve(collisions.size());

    skimCollisions<TEventFillMap, TTrackFillMap>(collisions, bcs, zdcs, trackAssocs, tracksBarrel);
    if (fCollIndexMap.size() == 0) {
      return;
    }

    if constexpr (static_cast<bool>(TTrackFillMap)) {
      fTrackIndexMap.clear();
      trackBarrelInfo.reserve(tracksBarrel.size());
      trackBasic.reserve(tracksBarrel.size());
      trackBarrel.reserve(tracksBarrel.size());
      trackBarrelCov.reserve(tracksBarrel.size());
      trackBarrelPID.reserve(tracksBarrel.size());
      trackBarrelAssoc.reserve(tracksBarrel.size());
    }

    if constexpr (static_cast<bool>(TMFTFillMap)) {
      fMftIndexMap.clear();
      mftTrack.reserve(mftTracks.size());
      mftTrackExtra.reserve(mftTracks.size());
      mftAssoc.reserve(mftTracks.size());
    }

    if constexpr (static_cast<bool>(TMuonFillMap)) {
      fFwdTrackIndexMap.clear();
      fFwdTrackFilterMap.clear();
      muonBasic.reserve(muons.size());
      muonExtra.reserve(muons.size());
      muonInfo.reserve(muons.size());
      muonCov.reserve(muons.size());
      muonAssoc.reserve(muons.size());
    }

    // loop over selected collisions, group the compatible associations, and run the skimming
    for (auto const& [origIdx, skimIdx] : fCollIndexMap) {
      auto collision = collisions.rawIteratorAt(origIdx);
      // group the barrel track associations for this collision
      if constexpr (static_cast<bool>(TTrackFillMap)) {
        auto groupedTrackIndices = trackAssocs.sliceBy(trackIndicesPerCollision, origIdx);
        skimTracks<TTrackFillMap>(collision, bcs, tracksBarrel, groupedTrackIndices);
      }
      // group the MFT associations for this collision
      if constexpr (static_cast<bool>(TMFTFillMap)) {
        auto groupedMFTIndices = mftAssocs.sliceBy(mfttrackIndicesPerCollision, origIdx);
        skimMFT<TMFTFillMap>(collision, bcs, mftTracks, groupedMFTIndices);
      }
      // group the muon associations for this collision
      if constexpr (static_cast<bool>(TMuonFillMap)) {
        if constexpr (static_cast<bool>(TMFTFillMap)) {
          auto groupedMuonIndices = fwdTrackAssocs.sliceBy(fwdtrackIndicesPerCollision, origIdx);
          skimMuons<TMuonFillMap, TMFTFillMap>(collision, bcs, muons, groupedMuonIndices, mftTracks);
        } else {
          auto groupedMuonIndices = fwdTrackAssocs.sliceBy(fwdtrackIndicesPerCollision, origIdx);
          skimMuons<TMuonFillMap, 0u>(collision, bcs, muons, groupedMuonIndices, nullptr);
        }
      }
    } // end loop over skimmed collisions

    // LOG(info) << "Skims in this TF: " << fCollIndexMap.size() << " collisions; " << trackBasic.lastIndex() << " barrel tracks; "
    //<< muonBasic.lastIndex() << " muon tracks; " << mftTrack.lastIndex() << " MFT tracks; ";
    // LOG(info) << "      " << trackBarrelAssoc.lastIndex() << " barrel assocs; " << muonAssoc.lastIndex() << " muon assocs; " << mftAssoc.lastIndex() << " MFT assoc";
  }

  // produce the full DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), subscribe to the DQ event filter (filter-pp or filter-PbPb)
  void processPPWithFilter(MyEventsWithMultsAndFilter const& collisions, BCsWithTimestamps const& bcs,
                           MyBarrelTracksWithCov const& tracksBarrel,
                           MyMuonsWithCov const& muons, MFTTracks const& mftTracks,
                           TrackAssoc const& trackAssocs, FwdTrackAssoc const& fwdTrackAssocs,
                           MFTTrackAssoc const& mftAssocs)
  {
    fullSkimming<gkEventFillMapWithMultsAndEventFilter, gkTrackFillMapWithCov, gkMuonFillMapWithCov, gkMFTFillMap>(collisions, bcs, nullptr, tracksBarrel, muons, mftTracks, trackAssocs, fwdTrackAssocs, mftAssocs);
  }

  // produce the barrel-only DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), subscribe to the DQ event filter (filter-pp or filter-PbPb)
  void processPPWithFilterBarrelOnly(MyEventsWithMultsAndFilter const& collisions, MyBCs const& bcs, aod::Zdcs& zdcs,
                                     MyBarrelTracksWithCov const& tracksBarrel,
                                     TrackAssoc const& trackAssocs)
  {
    fullSkimming<gkEventFillMapWithMultsEventFilterZdc, gkTrackFillMapWithCov, 0u, 0u>(collisions, bcs, zdcs, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the muon-only DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), subscribe to the DQ event filter (filter-pp or filter-PbPb)
  void processPPWithFilterMuonOnly(MyEventsWithMultsAndFilter const& collisions, BCsWithTimestamps const& bcs,
                                   MyMuonsWithCov const& muons, FwdTrackAssoc const& fwdTrackAssocs)
  {
    fullSkimming<gkEventFillMapWithMultsAndEventFilter, 0u, gkMuonFillMapWithCov, 0u>(collisions, bcs, nullptr, nullptr, muons, nullptr, nullptr, fwdTrackAssocs, nullptr);
  }

  // produce the muon+mft DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), subscribe to the DQ event filter (filter-pp or filter-PbPb)
  void processPPWithFilterMuonMFT(MyEventsWithMultsAndFilter const& collisions, BCsWithTimestamps const& bcs,
                                  MyMuonsWithCov const& muons, MFTTracks const& mftTracks,
                                  FwdTrackAssoc const& fwdTrackAssocs, MFTTrackAssoc const& mftAssocs)
  {
    fullSkimming<gkEventFillMapWithMultsAndEventFilter, 0u, gkMuonFillMapWithCov, gkMFTFillMap>(collisions, bcs, nullptr, nullptr, muons, mftTracks, nullptr, fwdTrackAssocs, mftAssocs);
  }

  // produce the barrel-only DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), meant to run on skimmed data
  void processPPBarrelOnly(MyEventsWithMults const& collisions, MyBCs const& bcs, aod::Zdcs& zdcs,
                           MyBarrelTracksWithCov const& tracksBarrel,
                           TrackAssoc const& trackAssocs)
  {
    fullSkimming<gkEventFillMapWithMultsZdc, gkTrackFillMapWithCov, 0u, 0u>(collisions, bcs, zdcs, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the barrel-only DQ skimmed barrel data model, with V0 tagged tracks
  void processPPBarrelOnlyWithV0s(MyEventsWithMults const& collisions, MyBCs const& bcs,
                                  MyBarrelTracksWithV0BitsNoTOF const& tracksBarrel,
                                  TrackAssoc const& trackAssocs)
  {
    fullSkimming<gkEventFillMapWithMults, gkTrackFillMapWithV0BitsNoTOF, 0u, 0u>(collisions, bcs, nullptr, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the muon-only DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), meant to run on skimmed data
  void processPPMuonOnly(MyEventsWithMults const& collisions, BCsWithTimestamps const& bcs,
                         MyMuonsWithCov const& muons, FwdTrackAssoc const& fwdTrackAssocs)
  {
    fullSkimming<gkEventFillMapWithMults, 0u, gkMuonFillMapWithCov, 0u>(collisions, bcs, nullptr, nullptr, muons, nullptr, nullptr, fwdTrackAssocs, nullptr);
  }

  // produce the realigned muon-only DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), meant to run on skimmed data
  void processPPRealignedMuonOnly(MyEventsWithMults const& collisions, BCsWithTimestamps const& bcs,
                                  MyMuonsRealignWithCov const& muons, FwdTrackAssoc const& fwdTrackAssocs)
  {
    fullSkimming<gkEventFillMapWithMults, 0u, gkMuonRealignFillMapWithCov, 0u>(collisions, bcs, nullptr, nullptr, muons, nullptr, nullptr, fwdTrackAssocs, nullptr);
  }

  // produce the muon+mft DQ skimmed data model typically for pp/p-Pb or UPC Pb-Pb (no centrality), meant to run on skimmed data
  void processPPMuonMFT(MyEventsWithMults const& collisions, BCsWithTimestamps const& bcs,
                        MyMuonsWithCov const& muons, MFTTracks const& mftTracks,
                        FwdTrackAssoc const& fwdTrackAssocs, MFTTrackAssoc const& mftAssocs)
  {
    fullSkimming<gkEventFillMapWithMults, 0u, gkMuonFillMapWithCov, gkMFTFillMap>(collisions, bcs, nullptr, nullptr, muons, mftTracks, nullptr, fwdTrackAssocs, mftAssocs);
  }

  // Central barrel multiplicity estimation
  void processPPMuonMFTWithMultsExtra(MyEventsWithMultsExtra const& collisions, BCsWithTimestamps const& bcs,
                                      MyMuonsWithCov const& muons, MFTTracks const& mftTracks,
                                      FwdTrackAssoc const& fwdTrackAssocs, MFTTrackAssoc const& mftAssocs)
  {
    fullSkimming<gkEventFillMapWithMultsExtra, 0u, gkMuonFillMapWithCov, gkMFTFillMap>(collisions, bcs, nullptr, nullptr, muons, mftTracks, nullptr, fwdTrackAssocs, mftAssocs);
  }

  // produce the full DQ skimmed data model typically for Pb-Pb (with centrality), no subscribtion to the DQ event filter
  void processPbPb(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                   MyBarrelTracksWithCov const& tracksBarrel,
                   MyMuonsWithCov const& muons, MFTTracks const& mftTracks,
                   TrackAssoc const& trackAssocs, FwdTrackAssoc const& fwdTrackAssocs,
                   MFTTrackAssoc const& mftAssocs)
  {
    fullSkimming<gkEventFillMapWithCentAndMults, gkTrackFillMapWithCov, gkMuonFillMapWithCov, gkMFTFillMap>(collisions, bcs, nullptr, tracksBarrel, muons, mftTracks, trackAssocs, fwdTrackAssocs, mftAssocs);
  }

  // produce the barrel only DQ skimmed data model typically for Pb-Pb (with centrality), no subscribtion to the DQ event filter
  void processPbPbBarrelOnly(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                             MyBarrelTracksWithCov const& tracksBarrel,
                             TrackAssoc const& trackAssocs)
  {
    fullSkimming<gkEventFillMapWithCentAndMults, gkTrackFillMapWithCov, 0u, 0u>(collisions, bcs, nullptr, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the barrel only DQ skimmed data model typically for Pb-Pb (with centrality), no TOF
  void processPbPbBarrelOnlyNoTOF(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                                  MyBarrelTracksWithCovNoTOF const& tracksBarrel,
                                  TrackAssoc const& trackAssocs)
  {
    computeOccupancyEstimators(collisions, tracksPosWithCovNoTOF, tracksNegWithCovNoTOF, presliceWithCovNoTOF, bcs);
    fullSkimming<gkEventFillMapWithCentAndMults, gkTrackFillMapNoTOF, 0u, 0u>(collisions, bcs, nullptr, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the barrel-only DQ skimmed data model typically for UPC Pb-Pb (no centrality), subscribe to the DQ rapidity gap event filter (filter-PbPb)
  void processPbPbWithFilterBarrelOnly(MyEventsWithMultsAndRapidityGapFilter const& collisions, MyBCs const& bcs, aod::Zdcs& zdcs,
                                       MyBarrelTracksWithCov const& tracksBarrel,
                                       TrackAssoc const& trackAssocs)
  {
    fullSkimming<gkEventFillMapWithMultsRapidityGapFilterZdc, gkTrackFillMapWithCov, 0u, 0u>(collisions, bcs, zdcs, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the barrel only DQ skimmed data model typically for Pb-Pb (with centrality), no subscribtion to the DQ event filter
  void processPbPbBarrelOnlyWithV0Bits(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                                       MyBarrelTracksWithV0Bits const& tracksBarrel,
                                       TrackAssoc const& trackAssocs)
  {
    computeOccupancyEstimators(collisions, tracksPos, tracksNeg, preslice, bcs);
    fullSkimming<gkEventFillMapWithCentAndMults, gkTrackFillMapWithV0Bits, 0u, 0u>(collisions, bcs, nullptr, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the barrel only DQ skimmed data model typically for Pb-Pb (with centrality), no subscribtion to the DQ event filter
  void processPbPbBarrelOnlyWithV0BitsNoTOF(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                                            MyBarrelTracksWithV0BitsNoTOF const& tracksBarrel,
                                            TrackAssoc const& trackAssocs)
  {
    computeOccupancyEstimators(collisions, tracksPosNoTOF, tracksNegNoTOF, presliceNoTOF, bcs);
    fullSkimming<gkEventFillMapWithCentAndMults, gkTrackFillMapWithV0BitsNoTOF, 0u, 0u>(collisions, bcs, nullptr, tracksBarrel, nullptr, nullptr, trackAssocs, nullptr, nullptr);
  }

  // produce the muon only DQ skimmed data model typically for Pb-Pb (with centrality), no subscribtion to the DQ event filter
  void processPbPbMuonOnly(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                           MyMuonsWithCov const& muons, FwdTrackAssoc const& fwdTrackAssocs)
  {
    fullSkimming<gkEventFillMapWithCentAndMults, 0u, gkMuonFillMapWithCov, 0u>(collisions, bcs, nullptr, nullptr, muons, nullptr, nullptr, fwdTrackAssocs, nullptr);
  }

  // produce the realigned muon only DQ skimmed data model typically for Pb-Pb (with centrality), no subscribtion to the DQ event filter
  void processPbPbRealignedMuonOnly(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                                    MyMuonsRealignWithCov const& muons, FwdTrackAssoc const& fwdTrackAssocs)
  {
    fullSkimming<gkEventFillMapWithCentAndMults, 0u, gkMuonRealignFillMapWithCov, 0u>(collisions, bcs, nullptr, nullptr, muons, nullptr, nullptr, fwdTrackAssocs, nullptr);
  }

  // produce the muon+mft DQ skimmed data model typically for Pb-Pb (with centrality), no subscribtion to the DQ event filter
  void processPbPbMuonMFT(MyEventsWithCentAndMults const& collisions, BCsWithTimestamps const& bcs,
                          MyMuonsWithCov const& muons, MFTTracks const& mftTracks,
                          FwdTrackAssoc const& fwdTrackAssocs, MFTTrackAssoc const& mftAssocs)
  {
    fullSkimming<gkEventFillMapWithCentAndMults, 0u, gkMuonFillMapWithCov, gkMFTFillMap>(collisions, bcs, nullptr, nullptr, muons, mftTracks, nullptr, fwdTrackAssocs, mftAssocs);
  }

  // Process the BCs and store stats for luminosity retrieval -----------------------------------------------------------------------------------
  void processOnlyBCs(soa::Join<aod::BCs, aod::BcSels>::iterator const& bc)
  {
    for (int i = 0; i < o2::aod::evsel::kNsel; i++) {
      if (bc.selection_bit(i) > 0) {
        (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(0.0, static_cast<float>(i));
      }
    }
    (reinterpret_cast<TH2D*>(fStatsList->At(kStatsEvent)))->Fill(0.0, static_cast<float>(o2::aod::evsel::kNsel));
  }

  PROCESS_SWITCH(TableMaker, processPPWithFilter, "Build full DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb, w/ event filtering", false);
  PROCESS_SWITCH(TableMaker, processPPWithFilterBarrelOnly, "Build barrel only DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb, w/ event filtering", false);
  PROCESS_SWITCH(TableMaker, processPPWithFilterMuonOnly, "Build muon only DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb, w/ event filtering", false);
  PROCESS_SWITCH(TableMaker, processPPWithFilterMuonMFT, "Build muon + mft DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb, w/ event filtering", false);
  PROCESS_SWITCH(TableMaker, processPPBarrelOnly, "Build barrel only DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb", false);
  PROCESS_SWITCH(TableMaker, processPPBarrelOnlyWithV0s, "Build barrel only DQ skimmed data model, pp like, with V0 tagged tracks", false);
  PROCESS_SWITCH(TableMaker, processPPMuonOnly, "Build muon only DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb", false);
  PROCESS_SWITCH(TableMaker, processPPRealignedMuonOnly, "Build realigned muon only DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb", false);
  PROCESS_SWITCH(TableMaker, processPPMuonMFT, "Build muon + mft DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb", false);
  PROCESS_SWITCH(TableMaker, processPPMuonMFTWithMultsExtra, "Build muon + mft DQ skimmed data model typically for pp/p-Pb and UPC Pb-Pb", false);
  PROCESS_SWITCH(TableMaker, processPbPb, "Build full DQ skimmed data model typically for Pb-Pb, w/o event filtering", false);
  PROCESS_SWITCH(TableMaker, processPbPbBarrelOnly, "Build barrel only DQ skimmed data model typically for Pb-Pb, w/o event filtering", false);
  PROCESS_SWITCH(TableMaker, processPbPbBarrelOnlyNoTOF, "Build barrel only DQ skimmed data model typically for Pb-Pb, w/o event filtering, no TOF", false);
  PROCESS_SWITCH(TableMaker, processPbPbWithFilterBarrelOnly, "Build barrel only DQ skimmed data model typically for UPC Pb-Pb, w/ event filtering", false);
  PROCESS_SWITCH(TableMaker, processPbPbBarrelOnlyWithV0Bits, "Build barrel only DQ skimmed data model typically for Pb-Pb, w/ V0 bits, w/o event filtering", false);
  PROCESS_SWITCH(TableMaker, processPbPbBarrelOnlyWithV0BitsNoTOF, "Build barrel only DQ skimmed data model typically for Pb-Pb, w/ V0 bits, no TOF, w/o event filtering", false);
  PROCESS_SWITCH(TableMaker, processPbPbMuonOnly, "Build muon only DQ skimmed data model typically for Pb-Pb, w/o event filtering", false);
  PROCESS_SWITCH(TableMaker, processPbPbRealignedMuonOnly, "Build realigned muon only DQ skimmed data model typically for Pb-Pb, w/o event filtering", false);
  PROCESS_SWITCH(TableMaker, processPbPbMuonMFT, "Build muon + mft DQ skimmed data model typically for Pb-Pb, w/o event filtering", false);
  PROCESS_SWITCH(TableMaker, processOnlyBCs, "Analyze the BCs to store sampled lumi", false);
};

void DefineHistograms(HistogramManager* histMan, TString histClasses, Configurable<std::string> configVar)
{
  //
  // Define here the histograms for all the classes required in analysis.
  //  The histogram classes are provided in the histClasses string, separated by semicolon ";"
  //  The histogram classes and their components histograms are defined below depending on the name of the histogram class
  //
  std::unique_ptr<TObjArray> objArray(histClasses.Tokenize(";"));
  for (Int_t iclass = 0; iclass < objArray->GetEntries(); ++iclass) {
    TString classStr = objArray->At(iclass)->GetName();
    histMan->AddHistClass(classStr.Data());

    TString histName = configVar.value;
    // NOTE: The level of detail for histogramming can be controlled via configurables
    if (classStr.Contains("Event")) {
      dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "event", histName);
    }

    if (classStr.Contains("Track") && !classStr.Contains("Pairs")) {
      if (classStr.Contains("Barrel")) {
        dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "track", histName);
        if (classStr.Contains("PIDCalibElectron")) {
          dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "track", "postcalib_electron");
        }
        if (classStr.Contains("PIDCalibPion")) {
          dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "track", "postcalib_pion");
        }
        if (classStr.Contains("PIDCalibProton")) {
          dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "track", "postcalib_proton");
        }
      }
      if (classStr.Contains("Muon")) {
        dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "track", histName);
      }
    }

    if (classStr.Contains("Pairs")) {
      dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "pair", histName);
    }

    if (classStr.Contains("DileptonsSelected")) {
      dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "pair", "barrel");
    }

    if (classStr.Contains("HadronsSelected")) {
      dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "track", histName);
    }

    if (classStr.Contains("DileptonHadronInvMass")) {
      dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "dilepton-hadron-mass");
    }

    if (classStr.Contains("DileptonHadronCorrelation")) {
      dqhistograms::DefineHistograms(histMan, objArray->At(iclass)->GetName(), "dilepton-hadron-correlation");
    }
  } // end loop over histogram classes
}

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<TableMaker>(cfgc)};
}
