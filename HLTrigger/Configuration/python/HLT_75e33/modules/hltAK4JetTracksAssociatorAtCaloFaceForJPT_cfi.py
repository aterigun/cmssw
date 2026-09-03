import FWCore.ParameterSet.Config as cms

hltAK4JetTracksAssociatorAtCaloFaceForJPT = cms.EDProducer("JetTracksAssociatorAtCaloFace",
    jets = cms.InputTag("hltCaloJetsForJPTMerged"),
    extrapolations = cms.InputTag("hltTrackExtrapolatorForJPT"),
    coneSize = cms.double(0.4),
)
