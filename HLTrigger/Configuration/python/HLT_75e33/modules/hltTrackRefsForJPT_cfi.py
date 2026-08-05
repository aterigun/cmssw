import FWCore.ParameterSet.Config as cms

hltTrackRefsForJPT = cms.EDProducer("ChargedRefCandidateProducer",
    src = cms.InputTag("hltTrackWithVertexRefSelectorForJPT"),
    particleType = cms.string("pi+"),
)
