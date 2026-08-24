// Merges several reco::CaloJetCollections into a single collection.
//
// Used to combine the barrel CaloJets (from CaloTowers, via
// hltAk4CaloJetsForTrk) with the HGCAL endcap jets from HGCalJetProducer,
// so that downstream JPT consumes one collection covering the full eta range.
// Nothing here is HGCAL-specific: any number of CaloJet collections can be
// given in 'src'.

#include <algorithm>
#include <memory>
#include <vector>

#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"

#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"

class CaloJetMerger : public edm::stream::EDProducer<> {
public:
  explicit CaloJetMerger(const edm::ParameterSet&);
  ~CaloJetMerger() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void produce(edm::Event&, const edm::EventSetup&) override;

  std::vector<edm::EDGetTokenT<reco::CaloJetCollection>> srcTokens_;
  const bool sortByPt_;
};

CaloJetMerger::CaloJetMerger(const edm::ParameterSet& conf)
    : sortByPt_(conf.getParameter<bool>("sortByPt")) {
  for (auto const& tag : conf.getParameter<std::vector<edm::InputTag>>("src"))
    srcTokens_.push_back(consumes<reco::CaloJetCollection>(tag));

  produces<reco::CaloJetCollection>();
}

void CaloJetMerger::produce(edm::Event& event, const edm::EventSetup&) {
  auto result = std::make_unique<reco::CaloJetCollection>();

  for (auto const& token : srcTokens_) {
    auto const& jets = event.get(token);
    result->insert(result->end(), jets.begin(), jets.end());
  }

  // Each input is pT-ordered internally, but their concatenation is not.
  // Sorting keeps the usual convention that the first jet is the leading one.
  if (sortByPt_)
    std::sort(result->begin(), result->end(),
              [](reco::CaloJet const& a, reco::CaloJet const& b) { return a.pt() > b.pt(); });

  event.put(std::move(result));
}

void CaloJetMerger::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<std::vector<edm::InputTag>>(
          "src", {edm::InputTag("hltAk4CaloJetsForTrk"), edm::InputTag("hltHGCalJetsForJPT")})
      ->setComment("CaloJet collections to concatenate, in order");
  desc.add<bool>("sortByPt", true)->setComment("Sort the merged collection by descending pT");
  descriptions.add("caloJetMerger", desc);
}

DEFINE_FWK_MODULE(CaloJetMerger);
