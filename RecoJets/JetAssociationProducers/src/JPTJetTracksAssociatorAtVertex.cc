#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "DataFormats/Common/interface/RefToBase.h"
#include "DataFormats/Common/interface/View.h"
#include "DataFormats/JetReco/interface/Jet.h"
#include "DataFormats/JetReco/interface/JetTracksAssociation.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "RecoJets/JetAssociationAlgorithms/interface/JetTracksAssociationDRVertex.h"
#include "RecoJets/JetAssociationAlgorithms/interface/JetTracksAssociationDRVertexAssigned.h"

// RefVector-aware counterpart of JetTracksAssociatorAtVertex. It deliberately
// consumes the exact product emitted by TrackWithVertexRefSelector, preventing
// excluded PU tracks from re-entering JPT through the at-vertex association.
class JPTJetTracksAssociatorAtVertex final : public edm::global::EDProducer<> {
public:
  explicit JPTJetTracksAssociatorAtVertex(edm::ParameterSet const& config)
      : jetsToken_(consumes<edm::View<reco::Jet>>(config.getParameter<edm::InputTag>("jets"))),
        tracksToken_(consumes<reco::TrackRefVector>(config.getParameter<edm::InputTag>("tracks"))),
        verticesToken_(consumes<reco::VertexCollection>(config.getParameter<edm::InputTag>("vertices"))),
        associator_(config.getParameter<double>("coneSize")),
        assignedAssociator_(config.getParameter<double>("coneSize")),
        useAssigned_(config.getParameter<bool>("useAssigned")) {
    produces<reco::JetTracksAssociation::Container>();
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription description;
    description.add<edm::InputTag>("jets", edm::InputTag("hltCaloJetsForJPTMerged"));
    description.add<edm::InputTag>("tracks", edm::InputTag("hltTrackWithVertexRefSelectorForJPT"));
    description.add<edm::InputTag>("vertices", edm::InputTag("hltFirstStepPrimaryVerticesUnsorted"));
    description.add<double>("coneSize", 0.4);
    description.add<bool>("useAssigned", false);
    descriptions.addWithDefaultLabel(description);
  }

private:
  void produce(edm::StreamID, edm::Event& event, edm::EventSetup const&) const override {
    edm::Handle<edm::View<reco::Jet>> jetsHandle;
    event.getByToken(jetsToken_, jetsHandle);
    auto const& selectedTracks = event.get(tracksToken_);

    auto output =
        std::make_unique<reco::JetTracksAssociation::Container>(reco::JetRefBaseProd(jetsHandle));

    std::vector<edm::RefToBase<reco::Jet>> jets;
    jets.reserve(jetsHandle->size());
    for (std::size_t index = 0; index < jetsHandle->size(); ++index) {
      jets.push_back(jetsHandle->refAt(index));
    }

    std::vector<reco::TrackRef> tracks;
    tracks.reserve(selectedTracks.size());
    for (auto const& track : selectedTracks) {
      if (track.isNonnull() && track.isAvailable()) {
        tracks.push_back(track);
      }
    }

    if (useAssigned_) {
      assignedAssociator_.produce(output.get(), jets, tracks, event.get(verticesToken_));
    } else {
      associator_.produce(output.get(), jets, tracks);
    }
    event.put(std::move(output));
  }

  edm::EDGetTokenT<edm::View<reco::Jet>> const jetsToken_;
  edm::EDGetTokenT<reco::TrackRefVector> const tracksToken_;
  edm::EDGetTokenT<reco::VertexCollection> const verticesToken_;
  JetTracksAssociationDRVertex const associator_;
  JetTracksAssociationDRVertexAssigned const assignedAssociator_;
  bool const useAssigned_;
};

DEFINE_FWK_MODULE(JPTJetTracksAssociatorAtVertex);
