// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright
// holders. All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
///
/// \brief This task is an empty skeleton that fills a simple eta histogram.
///        it is meant to be a blank page for further developments.
/// \author everyone

#include "Common/CCDB/EventSelectionParams.h"
#include "Common/CCDB/TriggerAliases.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/runDataProcessing.h"
#include "ReconstructionDataFormats/GlobalTrackID.h"
#include "ReconstructionDataFormats/Track.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::aod::evsel;

using ColEvSels = soa::Join<aod::Collisions, aod::EvSels>;
using BCsRun3 = soa::Join<aod::BCs, aod::Timestamps, aod::BcSels,
                          aod::Run3MatchedToBCSparse>;

struct UCCZDC {
  // Configurable number of bins
  Configurable<int> nBinsAmp{"nBinsAmp", 1025, "nbinsAmp"};
  Configurable<float> MaxZN{"MaxZN", 4099.5, "Max ZN signal"};
  Configurable<float> MaxZP{"MaxZP", 3099.5, "Max ZP signal"};
  Configurable<float> MaxZEM{"MaxZEM", 3099.5, "Max ZEM signal"};
  Configurable<int> nBinsTDC{"nBinsTDC", 480, "nbinsTDC"};

  Configurable<int> nBinsFit{"nBinsFit", 1000, "nbinsFit"};
  Configurable<float> MaxMultFV0{"MaxMultFV0", 3000, "Max FV0 signal"};
  Configurable<float> MaxMultFT0{"MaxMultFT0", 3000, "Max FT0 signal"};
  Configurable<float> MaxMultFDD{"MaxMultFDD", 80000, "Max FDD signal"};

  // Histogram registry: an object to hold your histograms
  HistogramRegistry histos{
    "histos",
    {},
    OutputObjHandlingPolicy::AnalysisObject};

  void init(InitContext const&)
  {
    // define axes you want to use
    const AxisSpec axisEvent{3, 0., +3.0, ""};
    const AxisSpec axisEta{30, -1.5, +1.5, "#eta"};

    // create histograms
    histos.add("etaHistogram", "etaHistogram", kTH1F, {axisEta});
    histos.add("hEventCounter", "Event counter", kTH1F, {axisEvent});
    histos.add("ZNAcomm", "; ZNA common energy; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZN}}});
    histos.add("ZNCcomm", "; ZNC common energy; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZN}}});
    histos.add("ZNAcoll", "ZNAcoll; ZNA amplitude; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZN}}});
    histos.add("ZPAcoll", "ZPAcoll; ZPA amplitude; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZP}}});
    histos.add("ZNCcoll", "ZNCcoll; ZNC amplitude; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZN}}});
    histos.add("ZPCcoll", "ZPCcoll; ZPC amplitude; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZP}}});
    histos.add("ZEM1coll", "ZEM1coll; ZEM1 amplitude; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZEM}}});
    histos.add("ZEM2coll", "ZEM2coll; ZEM2 amplitude; Entries",
               {HistType::kTH1F, {{nBinsAmp, -0.5, MaxZEM}}});
    histos.add("ZNvsZEMcoll", "ZNvsZEMcoll; ZEM; ZNA+ZNC",
               {HistType::kTH2F,
                {{{nBinsAmp, -0.5, MaxZEM}, {nBinsAmp, -0.5, 2. * MaxZN}}}});
    histos.add("ZNAvsZNCcoll", "ZNAvsZNCcoll; ZNC; ZNA",
               {HistType::kTH2F,
                {{{nBinsAmp, -0.5, MaxZN}, {nBinsAmp, -0.5, MaxZN}}}});
    histos.add("ZPAvsZPCcoll", "ZPAvsZPCcoll; ZPA; ZPC",
               {HistType::kTH2F,
                {{{nBinsAmp, -0.5, MaxZP}, {nBinsAmp, -0.5, MaxZP}}}});
    histos.add("ZNAvsZPAcoll", "ZNAvsZPAcoll; ZPA; ZNA",
               {HistType::kTH2F,
                {{{nBinsAmp, -0.5, MaxZP}, {nBinsAmp, -0.5, MaxZN}}}});
    histos.add("ZNCvsZPCcoll", "ZNCvsZPCcoll; ZPC; ZNC",
               {HistType::kTH2F,
                {{{nBinsAmp, -0.5, MaxZP}, {nBinsAmp, -0.5, MaxZN}}}});
    //
    histos.add("ZNCvstdccoll", "ZNCvstdccoll; time ZNC; ZNC",
               {HistType::kTH2F,
                {{{nBinsTDC, -13.5, 11.45}, {nBinsAmp, -0.5, MaxZN}}}});
    histos.add("ZNAvstdccoll", "ZNAvstdccoll; time ZNA; ZNA",
               {HistType::kTH2F,
                {{{nBinsTDC, -13.5, 11.45}, {nBinsAmp, -0.5, MaxZN}}}});
    histos.add("ZPCvstdccoll", "ZPCvstdccoll; time ZPC; ZPC",
               {HistType::kTH2F,
                {{{nBinsTDC, -13.5, 11.45}, {nBinsAmp, -0.5, MaxZP}}}});
    histos.add("ZPAvstdccoll", "ZPAvstdccoll; time ZPA; ZPA",
               {HistType::kTH2F,
                {{{nBinsTDC, -13.5, 11.45}, {nBinsAmp, -0.5, MaxZP}}}});
    histos.add("ZEM1vstdccoll", "ZEM1vstdccoll; time ZEM1; ZEM1",
               {HistType::kTH2F,
                {{{nBinsTDC, -13.5, 11.45}, {nBinsAmp, -0.5, MaxZEM}}}});
    histos.add("ZEM2vstdccoll", "ZEM2vstdccoll; time ZEM2; ZEM2",
               {HistType::kTH2F,
                {{{nBinsTDC, -13.5, 11.45}, {nBinsAmp, -0.5, MaxZEM}}}});
    histos.add("debunch", "ZN sum vs. ZN diff.",
               {HistType::kTH2F, {{{240, -12., 12.}, {240, -12., 12.}}}});
    // histos.add("centroidZNA", "ZNA centroid",
    //            {HistType::kTH2F, {{{350, -1.75, 1.75}, {350,
    //            -1.75, 1.75}}}});

    // if (processZdcCollAss) {
    //   histos.add("centroidZNC", "ZNC centroid",
    //              {HistType::kTH2F, {{{350, -1.75, 1.75}, {350,
    //              -1.75, 1.75}}}});
    // }
    histos.add("ZNvsFV0Acorrel", "ZNvsFV0Acorrel",
               {HistType::kTH2F,
                {{{nBinsFit, 0., MaxMultFV0}, {nBinsAmp, -0.5, 2. * MaxZN}}}});
    histos.add("ZNvsFT0correl", "ZNvsFT0correl",
               {HistType::kTH2F,
                {{{nBinsFit, 0., MaxMultFT0}, {nBinsAmp, -0.5, 2. * MaxZN}}}});
  }

  void processZdcCollAss(
    ColEvSels const& cols, BCsRun3 const& /*bcs*/, aod::Zdcs const& /*zdcs*/,
    aod::FV0As const& /*fv0as*/, aod::FT0s const& /*ft0s*/,
    soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksDCA> const& tracks)
  {
    for (auto& collision : cols) {
      histos.fill(HIST("hEventCounter"), 0.5);
      if (!collision.sel8()) {
        return;
      }

      histos.fill(HIST("hEventCounter"), 1.5);

      const auto& foundBC = collision.foundBC_as<BCsRun3>();
      if (foundBC.has_zdc()) {
        histos.fill(HIST("hEventCounter"), 2.5);

        const auto& zdcread = foundBC.zdc();
        auto aZNA = zdcread.amplitudeZNA();
        auto aZNC = zdcread.amplitudeZNC();
        auto aZPA = zdcread.amplitudeZPA();
        auto aZPC = zdcread.amplitudeZPC();
        auto aZEM1 = zdcread.amplitudeZEM1();
        auto aZEM2 = zdcread.amplitudeZEM2();
        auto tZEM1 = zdcread.timeZEM1();
        auto tZEM2 = zdcread.timeZEM2();
        auto tZNA = zdcread.timeZNA();
        auto tZNC = zdcread.timeZNC();
        auto tZPA = zdcread.timeZPA();
        auto tZPC = zdcread.timeZPC();
        float multT0A{0.};
        float multT0C{0.};
        float multV0A{0.};

        if (foundBC.has_ft0()) {
          for (auto amplitude : foundBC.ft0().amplitudeA()) {
            multT0A += amplitude;
          }
          for (auto amplitude : foundBC.ft0().amplitudeC()) {
            multT0C += amplitude;
          }
        } else {
          multT0A = multT0C = -999;
        }

        if (foundBC.has_fv0a()) {
          for (auto amplitude : foundBC.fv0a().amplitude()) {
            multV0A += amplitude;
          }
        } else {
          multV0A = -999;
        }

        histos.get<TH1>(HIST("ZNAcomm"))->Fill(zdcread.energyCommonZNA());
        histos.get<TH1>(HIST("ZNCcomm"))->Fill(zdcread.energyCommonZNC());
        histos.get<TH1>(HIST("ZNAcoll"))->Fill(aZNA);
        histos.get<TH1>(HIST("ZNCcoll"))->Fill(aZNC);
        histos.get<TH1>(HIST("ZPAcoll"))->Fill(aZPA);
        histos.get<TH1>(HIST("ZPCcoll"))->Fill(aZPC);
        histos.get<TH2>(HIST("ZNvsZEMcoll"))->Fill(aZEM1 + aZEM2, aZNA + aZNC);
        histos.get<TH2>(HIST("ZNAvsZNCcoll"))->Fill(aZNC, aZNA);
        histos.get<TH2>(HIST("ZPAvsZPCcoll"))->Fill(aZPC, aZPA);
        histos.get<TH2>(HIST("ZNAvsZPAcoll"))->Fill(aZPA, aZNA);
        histos.get<TH2>(HIST("ZNCvsZPCcoll"))->Fill(aZPC, aZNC);

        histos.get<TH1>(HIST("ZEM1coll"))->Fill(aZEM1);
        histos.get<TH1>(HIST("ZEM2coll"))->Fill(aZEM2);
        histos.get<TH2>(HIST("ZNCvstdccoll"))->Fill(tZNC, aZNC);
        histos.get<TH2>(HIST("ZNAvstdccoll"))->Fill(tZNA, aZNA);
        histos.get<TH2>(HIST("ZPCvstdccoll"))->Fill(tZPC, aZPC);
        histos.get<TH2>(HIST("ZPAvstdccoll"))->Fill(tZPA, aZPA);
        histos.get<TH2>(HIST("ZEM1vstdccoll"))->Fill(tZEM1, aZEM1);
        histos.get<TH2>(HIST("ZEM2vstdccoll"))->Fill(tZEM2, aZEM2);
        histos.get<TH2>(HIST("debunch"))->Fill(tZNA - tZNC, tZNA + tZNC);

        histos.get<TH2>(HIST("ZNvsFV0Acorrel"))
          ->Fill(multV0A / 100., aZNA + aZNC);
        histos.get<TH2>(HIST("ZNvsFT0correl"))
          ->Fill((multT0A + multT0C) / 100., aZNC + aZNA);
      } // foundBC.has_zdc()

      for (auto& track : tracks) {
        if (track.tpcNClsCrossedRows() < 70)
          continue;
        if (fabs(track.dcaXY()) > 0.2)
          continue;

        histos.fill(HIST("etaHistogram"), track.eta());
      }
    }
  }
  PROCESS_SWITCH(UCCZDC, processZdcCollAss,
                 "Processing ZDC w. collision association", true);

  // void processZdcCorrela(
  //     soa::Join<aod::Collisions, aod::EvSels>::iterator const& coll,
  //     BCsRun3 const& /*bcs*/, aod::Zdcs const& /*zdcs*/,
  //     aod::FV0As const& /*fv0as*/, aod::FT0s const& /*ft0s*/,
  //     aod::FDDs const& /*fdds*/) {
  //   const auto& foundBC = coll.foundBC_as<BCsRun3>();
  //
  //   // FT0
  //   float multT0A = 0.;
  //   float multT0C = 0.;
  //   if (foundBC.has_ft0()) {
  //     for (auto amplitude : foundBC.ft0().amplitudeA()) {
  //       multT0A += amplitude;
  //     }
  //     for (auto amplitude : foundBC.ft0().amplitudeC()) {
  //       multT0C += amplitude;
  //     }
  //   } else {
  //     multT0A = multT0C = -999;
  //   }
  //   // FV0
  //   float multV0A = 0;
  //   if (foundBC.has_fv0a()) {
  //     for (auto amplitude : foundBC.fv0a().amplitude()) {
  //       multV0A += amplitude;
  //     }
  //   } else {
  //     multV0A = -999;
  //   }
  //   // FDD
  //   float multFDA = 0;
  //   float multFDC = 0;
  //   if (foundBC.has_fdd()) {
  //     auto const& fdd = foundBC.fdd();
  //     for (auto const& amplitude : fdd.chargeA()) {
  //       multFDA += amplitude;
  //     }
  //     for (auto const& amplitude : fdd.chargeC()) {
  //       multFDC += amplitude;
  //     }
  //   } else {
  //     multFDA = multFDC = -999;
  //   }
  //
  //   if (foundBC.has_zdc()) {
  //     const auto& zdcread = foundBC.zdc();
  //     auto aZNA = zdcread.amplitudeZNA();
  //     auto aZNC = zdcread.amplitudeZNC();
  //     histos.get<TH2>(HIST("ZNvsFV0Acorrel"))
  //         ->Fill(multV0A / 100., aZNA + aZNC);
  //     histos.get<TH2>(HIST("ZNvsFT0correl"))
  //         ->Fill((multT0A + multT0C) / 100., aZNC + aZNA);
  //     histos.get<TH2>(HIST("ZNvsFDDcorrel"))
  //         ->Fill(multFDC + multFDA, aZNC + aZNA);
  //   }
  // }
  // PROCESS_SWITCH(UCCZDC, processZdcCorrela,
  //                "Processing ZDC vs. mult. w. collision association", true);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<UCCZDC>(cfgc)};
}
