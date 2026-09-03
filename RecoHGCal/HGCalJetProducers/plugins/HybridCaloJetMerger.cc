#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/Math/interface/deltaPhi.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/InputTag.h"

namespace {
  constexpr double kPi = 3.14159265358979323846;
}

class HybridCaloJetMerger final : public edm::global::EDProducer<> {
public:
  explicit HybridCaloJetMerger(edm::ParameterSet const& config)
      : barrelJetsToken_(consumes<reco::CaloJetCollection>(config.getParameter<edm::InputTag>("barrelJets"))),
        hgcalJetsToken_(consumes<reco::CaloJetCollection>(config.getParameter<edm::InputTag>("hgcalJets"))),
        outputToken_(produces<reco::CaloJetCollection>()),
        transitionMinAbsEta_(config.getParameter<double>("transitionMinAbsEta")),
        transitionMaxAbsEta_(config.getParameter<double>("transitionMaxAbsEta")),
        overlapDeltaR_(config.getParameter<double>("overlapDeltaR")),
        fallbackJetArea_(config.getParameter<double>("fallbackJetArea")),
        requireConstituents_(config.getParameter<bool>("requireConstituents")) {
    if (!(transitionMinAbsEta_ >= 0.0) || !(transitionMaxAbsEta_ > transitionMinAbsEta_) ||
        !(overlapDeltaR_ > 0.0) || !(fallbackJetArea_ > 0.0)) {
      throw cms::Exception("Configuration") << "Invalid HybridCaloJetMerger transition or area bounds";
    }
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("barrelJets", edm::InputTag("hltAk4CaloJetsForTrk"));
    desc.add<edm::InputTag>("hgcalJets", edm::InputTag("hltTICLTracksterJetsForJPT"));
    desc.add<double>("transitionMinAbsEta", 1.30);
    desc.add<double>("transitionMaxAbsEta", 1.80);
    desc.add<double>("overlapDeltaR", 0.40);
    desc.add<double>("fallbackJetArea", kPi * 0.4 * 0.4)
        ->setComment("Applied only when an input jet has no finite positive area");
    desc.add<bool>("requireConstituents", true)
        ->setComment("Fail early rather than publish a jet that violates the JPT constituent contract");
    descriptions.addWithDefaultLabel(desc);
  }

private:
  enum class Source : unsigned char { barrel, hgcal };

  struct JetIndex {
    Source source;
    std::size_t index;
  };

  static reco::CaloJet const& jet(JetIndex const& key,
                                  reco::CaloJetCollection const& barrelJets,
                                  reco::CaloJetCollection const& hgcalJets) {
    return key.source == Source::barrel ? barrelJets[key.index] : hgcalJets[key.index];
  }

  bool inTransition(reco::CaloJet const& candidate) const {
    double const absEta = std::abs(candidate.eta());
    return absEta > transitionMinAbsEta_ && absEta < transitionMaxAbsEta_;
  }

  bool overlapsAcceptedOppositeSource(JetIndex const& candidate,
                                      std::vector<JetIndex> const& accepted,
                                      reco::CaloJetCollection const& barrelJets,
                                      reco::CaloJetCollection const& hgcalJets) const {
    reco::CaloJet const& candidateJet = jet(candidate, barrelJets, hgcalJets);
    double const maxDistance2 = overlapDeltaR_ * overlapDeltaR_;
    for (JetIndex const& kept : accepted) {
      if (kept.source == candidate.source) {
        continue;
      }
      reco::CaloJet const& keptJet = jet(kept, barrelJets, hgcalJets);
      if (!inTransition(candidateJet) && !inTransition(keptJet)) {
        continue;
      }
      double const dEta = candidateJet.eta() - keptJet.eta();
      double const dPhi = reco::deltaPhi(candidateJet.phi(), keptJet.phi());
      if (dEta * dEta + dPhi * dPhi < maxDistance2) {
        return true;
      }
    }
    return false;
  }

  void produce(edm::StreamID, edm::Event& event, edm::EventSetup const&) const override {
    auto const& barrelJets = event.get(barrelJetsToken_);
    auto const& hgcalJets = event.get(hgcalJetsToken_);

    // Sorting these compact keys avoids repeatedly moving CaloJet payloads and
    // constituent Ptr vectors. The selected jets are copied exactly once.
    std::vector<JetIndex> ordered;
    ordered.reserve(barrelJets.size() + hgcalJets.size());
    for (std::size_t index = 0; index < barrelJets.size(); ++index) {
      ordered.push_back({Source::barrel, index});
    }
    for (std::size_t index = 0; index < hgcalJets.size(); ++index) {
      ordered.push_back({Source::hgcal, index});
    }

    auto const safePt = [&](JetIndex const& key) {
      double const pt = jet(key, barrelJets, hgcalJets).pt();
      return std::isfinite(pt) ? pt : -std::numeric_limits<double>::infinity();
    };
    std::sort(ordered.begin(), ordered.end(), [&](JetIndex const& left, JetIndex const& right) {
      double const leftPt = safePt(left);
      double const rightPt = safePt(right);
      if (leftPt != rightPt) {
        return leftPt > rightPt;
      }
      if (left.source != right.source) {
        return left.source == Source::barrel;  // deterministic equal-pT tie break
      }
      return left.index < right.index;
    });

    std::vector<JetIndex> accepted;
    accepted.reserve(ordered.size());
    for (JetIndex const& candidate : ordered) {
      if (!overlapsAcceptedOppositeSource(candidate, accepted, barrelJets, hgcalJets)) {
        accepted.push_back(candidate);
      }
    }

    reco::CaloJetCollection output;
    output.reserve(accepted.size());
    for (JetIndex const& key : accepted) {
      reco::CaloJet const& input = jet(key, barrelJets, hgcalJets);
      if (requireConstituents_ && input.numberOfDaughters() == 0U) {
        throw cms::Exception("InvalidReference")
            << "HybridCaloJetMerger received a constituent-free "
            << (key.source == Source::barrel ? "barrel" : "HGCAL") << " CaloJet at index " << key.index
            << ". Refusing to fabricate CaloTower references.";
      }

      output.emplace_back(input);  // preserves p4, vertex, Specific, and persistent constituent Ptrs
      if (!std::isfinite(output.back().jetArea()) || !(output.back().jetArea() > 0.0)) {
        output.back().setJetArea(static_cast<float>(fallbackJetArea_));
      }
    }

    // 'accepted' inherits the global descending-pT order from 'ordered'.
    event.emplace(outputToken_, std::move(output));
  }

  edm::EDGetTokenT<reco::CaloJetCollection> const barrelJetsToken_;
  edm::EDGetTokenT<reco::CaloJetCollection> const hgcalJetsToken_;
  edm::EDPutTokenT<reco::CaloJetCollection> const outputToken_;
  double const transitionMinAbsEta_;
  double const transitionMaxAbsEta_;
  double const overlapDeltaR_;
  double const fallbackJetArea_;
  bool const requireConstituents_;
};

DEFINE_FWK_MODULE(HybridCaloJetMerger);
