import FWCore.ParameterSet.Config as cms


hltAK4JetTracksAssociatorAtVertexForJPT = cms.EDProducer(
    "JPTJetTracksAssociatorAtVertex",
    jets = cms.InputTag("hltCaloJetsForJPTMerged"),
    tracks = cms.InputTag("hltTrackWithVertexRefSelectorForJPT"),
    vertices = cms.InputTag("hltFirstStepPrimaryVerticesUnsorted"),
    coneSize = cms.double(0.4),
    useAssigned = cms.bool(False),
)
