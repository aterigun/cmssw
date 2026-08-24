import FWCore.ParameterSet.Config as cms

hltAK4JetTracksAssociatorAtVertexForJPT = cms.EDProducer("JetTracksAssociatorAtVertex",
    jets = cms.InputTag("hltHGCalJetsForJPT"),
    tracks = cms.InputTag("hltGeneralTracks"),
    coneSize = cms.double(0.4),
    useAssigned = cms.bool(False),
    pvSrc = cms.InputTag("hltOfflinePrimaryVertices"),
)
