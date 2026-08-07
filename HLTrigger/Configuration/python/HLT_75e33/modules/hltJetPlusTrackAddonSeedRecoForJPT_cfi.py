import FWCore.ParameterSet.Config as cms

hltJetPlusTrackAddonSeedRecoForJPT = cms.EDProducer("JetPlusTrackAddonSeedProducer",
    srcCaloJets = cms.InputTag("hltAk4CaloJetsForTrk"),
    srcTrackJets = cms.InputTag("hltAK4TrackJetsForJPT"),
    srcPVs = cms.InputTag("hltPhase2PixelVertices"),
    towerMaker = cms.InputTag("hltPhase2TowerMakerForAll"),
    PFCandidates = cms.InputTag("hltParticleFlowTmp"),
    UsePAT = cms.bool(False),
    dRcone = cms.double(0.4),
)
