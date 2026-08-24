// Produces CaloJets from HGCAL layer clusters using FastJet anti-kT (R=0.4)
// Based on RecoHGCal/TICL/plugins/PatternRecognitionbyFastJet.cc (M. Rovere)

#include <cmath>
#include <memory>
#include <vector>

#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"

#include "DataFormats/CaloRecHit/interface/CaloCluster.h"
#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/Math/interface/LorentzVector.h"
#include "DataFormats/Math/interface/Point3D.h"

#include "fastjet/ClusterSequence.hh"

class HGCalJetProducer : public edm::stream::EDProducer<> {
public:
  explicit HGCalJetProducer(const edm::ParameterSet&);
  ~HGCalJetProducer() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void produce(edm::Event&, const edm::EventSetup&) override;

  void clusterOneSide(const std::vector<fastjet::PseudoJet>& fjInputs,
                      reco::CaloJetCollection& result) const;

  const edm::EDGetTokenT<std::vector<reco::CaloCluster>> layerClustersToken_;
  const double antiktRadius_;
  const double jetPtMin_;
  const double inputEtMin_;
  const double minEta_;
};

HGCalJetProducer::HGCalJetProducer(const edm::ParameterSet& conf)
    : layerClustersToken_(
          consumes<std::vector<reco::CaloCluster>>(conf.getParameter<edm::InputTag>("layerClusters"))),
      antiktRadius_(conf.getParameter<double>("antiktRadius")),
      jetPtMin_(conf.getParameter<double>("jetPtMin")),
      inputEtMin_(conf.getParameter<double>("inputEtMin")),
      minEta_(conf.getParameter<double>("minEta")) {
  produces<reco::CaloJetCollection>();
}

void HGCalJetProducer::clusterOneSide(const std::vector<fastjet::PseudoJet>& fjInputs,
                                      reco::CaloJetCollection& result) const {
  if (fjInputs.empty())
    return;

  fastjet::ClusterSequence sequence(fjInputs,
                                    fastjet::JetDefinition(fastjet::antikt_algorithm, antiktRadius_));
  auto jets = fastjet::sorted_by_pt(sequence.inclusive_jets(jetPtMin_));

  for (auto const& pj : jets) {
    math::XYZTLorentzVector p4(pj.px(), pj.py(), pj.pz(), pj.e());
    reco::CaloJet::Specific specific;
    result.emplace_back(p4, math::XYZPoint(0., 0., 0.), specific);
  }
}

void HGCalJetProducer::produce(edm::Event& event, const edm::EventSetup&) {
  auto const& layerClusters = event.get(layerClustersToken_);

  std::vector<fastjet::PseudoJet> fjInputsPlus;
  std::vector<fastjet::PseudoJet> fjInputsMinus;

  for (unsigned int i = 0; i < layerClusters.size(); ++i) {
    auto const& cl = layerClusters[i];

    const double eta = cl.position().eta();
    if (std::abs(eta) < minEta_)
      continue;

    const double et = cl.energy() / std::cosh(eta);
    if (et < inputEtMin_)
      continue;

    math::XYZVector direction(cl.x(), cl.y(), cl.z());
    direction = direction.Unit();
    direction *= cl.energy();

    fastjet::PseudoJet pj(direction.X(), direction.Y(), direction.Z(), cl.energy());
    pj.set_user_index(i);

    if (cl.z() > 0.)
      fjInputsPlus.push_back(pj);
    else
      fjInputsMinus.push_back(pj);
  }

  auto result = std::make_unique<reco::CaloJetCollection>();
  clusterOneSide(fjInputsPlus, *result);
  clusterOneSide(fjInputsMinus, *result);

  event.put(std::move(result));
}

void HGCalJetProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("layerClusters", edm::InputTag("hltMergeLayerClusters"))
      ->setComment("HGCAL layer clusters used as clustering input");
  desc.add<double>("antiktRadius", 0.4)->setComment("Anti-kT distance parameter");
  desc.add<double>("jetPtMin", 10.0)->setComment("Minimum pT of the output jets [GeV]");
  desc.add<double>("inputEtMin", 0.3)->setComment("Minimum Et of an input layer cluster [GeV]");
  desc.add<double>("minEta", 1.5)->setComment("Only cluster layer clusters above this |eta|");
  descriptions.add("hgcalJetProducer", desc);
}

DEFINE_FWK_MODULE(HGCalJetProducer);
