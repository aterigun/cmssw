import FWCore.ParameterSet.Config as cms

hltAK4JetTracksAssociatorAtCaloFaceForJPT = cms.EDProducer("JetTracksAssociatorAtCaloFace",
    jets = cms.InputTag("hltAk4CaloJetsForTrk"),
    tracks = cms.InputTag("hltGeneralTracks"),
    trackQuality = cms.string("goodIterative"),
    extrapolations = cms.InputTag("hltTrackExtrapolatorForJPT"),
    coneSize = cms.double(0.4),
)
