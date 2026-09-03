#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/Candidate/interface/LeafCandidate.h"
#include "DataFormats/Common/interface/Ptr.h"
#include "DataFormats/HGCalReco/interface/Trackster.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/Math/interface/LorentzVector.h"
#include "DataFormats/Math/interface/Point3D.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "fastjet/ClusterSequence.hh"

namespace {
  constexpr double kSpeedOfLightCmPerNs = 29.9792458;

  struct EnergyParts {
    float cee = 0.f;
    float ceh = 0.f;
  };

  struct PrimaryVertexState {
    math::XYZPoint position{0.0, 0.0, 0.0};
    double time = 0.0;
    double timeError = 0.0;
    bool hasVertex = false;
    bool hasMeasuredTime = false;
  };

  PrimaryVertexState primaryVertex(reco::VertexCollection const& vertices) {
    auto selected = std::find_if(vertices.begin(), vertices.end(), [](reco::Vertex const& vertex) {
      return vertex.isValid() && !vertex.isFake();
    });
    if (selected == vertices.end()) {
      selected = std::find_if(vertices.begin(), vertices.end(), [](reco::Vertex const& vertex) {
        return vertex.isValid();
      });
    }
    if (selected == vertices.end()) {
      return {};
    }

    PrimaryVertexState result;
    result.position = selected->position();
    result.time = std::isfinite(selected->t()) ? selected->t() : 0.0;
    result.timeError = std::isfinite(selected->tError()) && selected->tError() > 0.0 ? selected->tError() : 0.0;
    result.hasVertex = true;
    result.hasMeasuredTime = std::isfinite(selected->t()) && result.timeError > 0.0;
    return result;
  }

  // Integral of a circle's width from -R to x. It is used to clip the
  // nominal anti-kT pi*R^2 area at the configured HGCAL eta boundaries
  // without introducing FastJet ghost-area CPU and memory overhead.
  double circleAreaPrimitive(double x, double radius) {
    x = std::clamp(x, -radius, radius);
    double const root = std::sqrt(std::max(0.0, radius * radius - x * x));
    return x * root + radius * radius * std::asin(x / radius);
  }

  double clippedJetArea(double absEta, double radius, double minAbsEta, double maxAbsEta) {
    double const lower = std::clamp(minAbsEta - absEta, -radius, radius);
    double const upper = std::clamp(maxAbsEta - absEta, -radius, radius);
    return std::max(0.0, circleAreaPrimitive(upper, radius) - circleAreaPrimitive(lower, radius));
  }
}  // namespace

class TICLTracksterJetProducer final : public edm::global::EDProducer<> {
public:
  using ConstituentCollection = std::vector<reco::LeafCandidate>;

  explicit TICLTracksterJetProducer(edm::ParameterSet const& config)
      : trackstersToken_(consumes<ticl::TracksterCollection>(config.getParameter<edm::InputTag>("tracksters"))),
        verticesToken_(consumes<reco::VertexCollection>(config.getParameter<edm::InputTag>("vertices"))),
        constituentPutToken_(produces<ConstituentCollection>("constituents")),
        jetsPutToken_(produces<reco::CaloJetCollection>()),
        antiktRadius_(config.getParameter<double>("antiktRadius")),
        jetPtMin_(config.getParameter<double>("jetPtMin")),
        minTracksterEnergy_(config.getParameter<double>("minTracksterEnergy")),
        minTracksterEt_(config.getParameter<double>("minTracksterEt")),
        minAbsEta_(config.getParameter<double>("minAbsEta")),
        maxAbsEta_(config.getParameter<double>("maxAbsEta")),
        enableTiming_(config.getParameter<bool>("enableTiming")),
        maxTimeDifference_(config.getParameter<double>("maxTimeDifference")),
        maxTimeDifferenceWithoutPVTime_(config.getParameter<double>("maxTimeDifferenceWithoutPVTime")),
        timeNSigma_(config.getParameter<double>("timeNSigma")),
        maxTracksterTimeError_(config.getParameter<double>("maxTracksterTimeError")),
        useRawEnergyFallback_(config.getParameter<bool>("useRawEnergyFallback")) {
    if (!(antiktRadius_ > 0.0) || !(jetPtMin_ >= 0.0) || !(minTracksterEnergy_ >= 0.0) ||
        !(minTracksterEt_ >= 0.0) || !(minAbsEta_ >= 0.0) || !(maxAbsEta_ > minAbsEta_) ||
        !(maxTimeDifference_ >= 0.0) || !(maxTimeDifferenceWithoutPVTime_ >= maxTimeDifference_) ||
        !(timeNSigma_ >= 0.0) || !(maxTracksterTimeError_ > 0.0)) {
      throw cms::Exception("Configuration") << "Invalid TICLTracksterJetProducer clustering, acceptance, or timing bounds";
    }
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("tracksters", edm::InputTag("hltTiclCandidate"))
        ->setComment("Regressed ticl::TracksterCollection emitted by TICLCandidateProducer");
    desc.add<edm::InputTag>("vertices", edm::InputTag("hltFirstStepPrimaryVerticesUnsorted"))
        ->setComment("Use a pre-calorimeter PV producer to avoid a JPT/PV scheduling cycle");
    desc.add<double>("antiktRadius", 0.4);
    desc.add<double>("jetPtMin", 10.0)->setComment("Minimum output jet pT [GeV]");
    desc.add<double>("minTracksterEnergy", 0.5)
        ->setComment("Loose shower-level energy cut; no per-layer-cluster transverse-energy cut");
    desc.add<double>("minTracksterEt", 0.3)
        ->setComment("Minimum PV-relative transverse energy of an aggregated Trackster [GeV]");
    desc.add<double>("minAbsEta", 1.30)->setComment("Retain overlap for barrel/HGCAL stitching");
    desc.add<double>("maxAbsEta", 3.00);
    desc.add<bool>("enableTiming", true);
    desc.add<double>("maxTimeDifference", 0.50)->setComment("Base |t0(trackster)-t(PV)| window [ns]");
    desc.add<double>("maxTimeDifferenceWithoutPVTime", 1.00)
        ->setComment("Bunch-time window when the selected vertex has no measured time [ns]");
    desc.add<double>("timeNSigma", 3.0)->setComment("Additional uncertainty-scaled timing allowance");
    desc.add<double>("maxTracksterTimeError", 0.20)
        ->setComment("Tracksters with worse or absent timing are accepted without a timing veto [ns]");
    desc.add<bool>("useRawEnergyFallback", false)
        ->setComment("Use raw energy only if regression is absent; keep false for hltTiclCandidate");
    descriptions.addWithDefaultLabel(desc);
  }

private:
  bool passTiming(ticl::Trackster const& trackster,
                  PrimaryVertexState const& vertex,
                  double flightDistanceCm) const {
    if (!enableTiming_ || !vertex.hasVertex || !std::isfinite(trackster.time()) ||
        !std::isfinite(trackster.timeError()) || !(trackster.timeError() > 0.0) ||
        trackster.timeError() > maxTracksterTimeError_) {
      return true;
    }

    double const interactionTime = trackster.time() - flightDistanceCm / kSpeedOfLightCmPerNs;
    if (!vertex.hasMeasuredTime) {
      return std::abs(interactionTime) <= maxTimeDifferenceWithoutPVTime_;
    }
    double const uncertainty = std::hypot(static_cast<double>(trackster.timeError()), vertex.timeError);
    double const allowed = maxTimeDifference_ + timeNSigma_ * uncertainty;
    return std::abs(interactionTime - vertex.time) <= allowed;
  }

  void produce(edm::StreamID, edm::Event& event, edm::EventSetup const&) const override {
    auto const& tracksters = event.get(trackstersToken_);
    PrimaryVertexState const vertex = primaryVertex(event.get(verticesToken_));

    ConstituentCollection constituents;
    std::vector<EnergyParts> energyParts;
    std::vector<fastjet::PseudoJet> fastjetInputs;
    constituents.reserve(tracksters.size());
    energyParts.reserve(tracksters.size());
    fastjetInputs.reserve(tracksters.size());

    for (auto const& trackster : tracksters) {
      double energy = trackster.regressed_energy();
      if ((!std::isfinite(energy) || !(energy > 0.0)) && useRawEnergyFallback_) {
        energy = trackster.raw_energy();
      }
      if (!std::isfinite(energy) || energy < minTracksterEnergy_) {
        continue;
      }

      auto const& barycenter = trackster.barycenter();
      double const dx = barycenter.x() - vertex.position.x();
      double const dy = barycenter.y() - vertex.position.y();
      double const dz = barycenter.z() - vertex.position.z();
      double const distance = std::sqrt(dx * dx + dy * dy + dz * dz);
      double const transverseDistance = std::hypot(dx, dy);
      if (!(distance > 0.0) || !(transverseDistance > 0.0) || !std::isfinite(distance)) {
        continue;
      }
      double const eta = std::asinh(dz / transverseDistance);
      double const transverseEnergy = energy * transverseDistance / distance;
      if (transverseEnergy < minTracksterEt_ || std::abs(eta) < minAbsEta_ ||
          std::abs(eta) > maxAbsEta_ || !passTiming(trackster, vertex, distance)) {
        continue;
      }

      double const inverseDistance = 1.0 / distance;
      math::XYZTLorentzVector const p4(
          energy * dx * inverseDistance, energy * dy * inverseDistance, energy * dz * inverseDistance, energy);
      constituents.emplace_back(0, p4, vertex.position, 0, 1);

      double const rawEnergy = trackster.raw_energy();
      double const reportedRawEmEnergy = trackster.raw_em_energy();
      double const rawEmEnergy = std::isfinite(reportedRawEmEnergy) && std::isfinite(rawEnergy) && rawEnergy > 0.0
                                     ? std::clamp(reportedRawEmEnergy, 0.0, rawEnergy)
                                     : 0.0;
      EnergyParts parts;
      if (std::isfinite(rawEnergy) && rawEnergy > 0.0) {
        double const scale = energy / rawEnergy;
        parts.cee = static_cast<float>(rawEmEnergy * scale);
        parts.ceh = static_cast<float>((rawEnergy - rawEmEnergy) * scale);
      } else {
        parts.ceh = static_cast<float>(energy);
      }
      energyParts.push_back(parts);

      fastjet::PseudoJet pseudojet(p4.px(), p4.py(), p4.pz(), p4.energy());
      pseudojet.set_user_index(static_cast<int>(constituents.size() - 1));
      fastjetInputs.push_back(pseudojet);
    }

    auto const constituentHandle = event.emplace(constituentPutToken_, std::move(constituents));

    reco::CaloJetCollection jets;
    if (fastjetInputs.empty()) {
      event.emplace(jetsPutToken_, std::move(jets));
      return;
    }

    fastjet::JetDefinition const jetDefinition(fastjet::antikt_algorithm, antiktRadius_);
    fastjet::ClusterSequence const sequence(fastjetInputs, jetDefinition);
    std::vector<fastjet::PseudoJet> const clusteredJets =
        fastjet::sorted_by_pt(sequence.inclusive_jets(jetPtMin_));
    jets.reserve(clusteredJets.size());

    for (auto const& clusteredJet : clusteredJets) {
      std::vector<fastjet::PseudoJet> const clusteredConstituents = clusteredJet.constituents();
      reco::Jet::Constituents jetConstituents;
      jetConstituents.reserve(clusteredConstituents.size());

      double ceeEnergy = 0.0;
      double cehEnergy = 0.0;
      for (auto const& constituent : clusteredConstituents) {
        int const index = constituent.user_index();
        if (index < 0 || static_cast<std::size_t>(index) >= energyParts.size()) {
          throw cms::Exception("LogicError") << "FastJet returned an invalid TICL constituent index " << index;
        }
        edm::Ptr<reco::LeafCandidate> const leafPtr(constituentHandle, static_cast<std::size_t>(index));
        jetConstituents.emplace_back(leafPtr);

        EnergyParts const& parts = energyParts[static_cast<std::size_t>(index)];
        ceeEnergy += parts.cee;
        cehEnergy += parts.ceh;
      }

      double const area = std::max(
          1.e-6, clippedJetArea(std::abs(clusteredJet.eta()), antiktRadius_, minAbsEta_, maxAbsEta_));
      double const componentEnergy = ceeEnergy + cehEnergy;
      reco::CaloJet::Specific specific;
      specific.mEmEnergyInEE = static_cast<float>(ceeEnergy);
      specific.mHadEnergyInHE = static_cast<float>(cehEnergy);
      specific.mEnergyFractionEm = componentEnergy > 0.0 ? static_cast<float>(ceeEnergy / componentEnergy) : 0.f;
      specific.mEnergyFractionHadronic =
          componentEnergy > 0.0 ? static_cast<float>(cehEnergy / componentEnergy) : 0.f;

      math::XYZTLorentzVector const jetP4(
          clusteredJet.px(), clusteredJet.py(), clusteredJet.pz(), clusteredJet.e());
      jets.emplace_back(jetP4, vertex.position, specific, jetConstituents);
      jets.back().setJetArea(static_cast<float>(area));
    }

    event.emplace(jetsPutToken_, std::move(jets));
  }

  edm::EDGetTokenT<ticl::TracksterCollection> const trackstersToken_;
  edm::EDGetTokenT<reco::VertexCollection> const verticesToken_;
  edm::EDPutTokenT<ConstituentCollection> const constituentPutToken_;
  edm::EDPutTokenT<reco::CaloJetCollection> const jetsPutToken_;

  double const antiktRadius_;
  double const jetPtMin_;
  double const minTracksterEnergy_;
  double const minTracksterEt_;
  double const minAbsEta_;
  double const maxAbsEta_;
  bool const enableTiming_;
  double const maxTimeDifference_;
  double const maxTimeDifferenceWithoutPVTime_;
  double const timeNSigma_;
  double const maxTracksterTimeError_;
  bool const useRawEnergyFallback_;
};

DEFINE_FWK_MODULE(TICLTracksterJetProducer);
