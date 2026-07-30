import FWCore.ParameterSet.Config as cms

from RecoVertex.PrimaryVertexProducer.OfflinePrimaryVertices_cfi import offlinePrimaryVertices
from RecoJets.JetProducers.TracksForJets_cff import trackWithVertexRefSelector, trackRefsForJets
from RecoJets.JetProducers.ak4TrackJets_cfi import ak4TrackJets
from RecoJets.JetAssociationProducers.trackExtrapolator_cfi import trackExtrapolator
from RecoJets.JetPlusTracks.JetPlusTrackCorrections_cff import jetPlusTrackAddonSeedProducer
from RecoJets.Configuration.RecoJPTJets_cff import JetPlusTrackZSPCorJetAntiKt4

hltPrimaryVerticesForJPT = offlinePrimaryVertices.clone(
    TrackLabel = cms.InputTag("hltMergedTracks"),
    beamSpotLabel = cms.InputTag("hltOnlineBeamSpot")
)

hltTrackWithVertexRefSelectorForJPT = trackWithVertexRefSelector.clone(
    src = cms.InputTag("hltMergedTracks"),
    vertexTag = cms.InputTag("hltPrimaryVerticesForJPT")
)

hltTrackRefsForJPT = trackRefsForJets.clone(
    src = cms.InputTag("hltTrackWithVertexRefSelectorForJPT")
)

hltAK4TrackJetsForJPT = ak4TrackJets.clone(
    src = cms.InputTag("hltTrackRefsForJPT"),
    srcPVs = cms.InputTag("hltPrimaryVerticesForJPT")
)

hltTrackExtrapolatorForJPT = trackExtrapolator.clone(
    trackSrc = cms.InputTag("hltMergedTracks")
)

hltJetPlusTrackAddonSeedRecoForJPT = jetPlusTrackAddonSeedProducer.clone(
    srcCaloJets = cms.InputTag("hltAK4CaloJets"),
    srcTrackJets = cms.InputTag("hltAK4TrackJetsForJPT"),
    srcPVs = cms.InputTag("hltPrimaryVerticesForJPT"),
    tracks = cms.InputTag("hltMergedTracks")
)

hltJetPlusTrackZSPCorJetAntiKt4 = JetPlusTrackZSPCorJetAntiKt4.clone(
    src = cms.InputTag("hltAK4CaloJets"),
    srcTrackJets = cms.InputTag("hltAK4TrackJetsForJPT"),
    srcAddCaloJets = cms.InputTag("hltJetPlusTrackAddonSeedRecoForJPT"),
    extrapolations = cms.InputTag("hltTrackExtrapolatorForJPT"),
    srcPVs = cms.InputTag("hltPrimaryVerticesForJPT"),
    alias = cms.untracked.string("hltJetPlusTrackZSPCorJetAntiKt4")
)
