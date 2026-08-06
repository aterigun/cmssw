import FWCore.ParameterSet.Config as cms

hltJetPlusTrackAddonSeedRecoForJPT = cms.EDProducer("JetPlusTrackAddonSeedProducer",
    srcCaloJets = cms.InputTag("hltAk4CaloJetsForTrk"),
    srcTrackJets = cms.InputTag("hltAK4TrackJetsForJPT"),
    srcPVs = cms.InputTag("hltPhase2PixelVertices"),
    tracks = cms.InputTag("hltGeneralTracks"),
    dRcone = cms.double(0.4),
    ptCut = cms.double(15.0),
)
