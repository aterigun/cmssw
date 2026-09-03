import FWCore.ParameterSet.Config as cms


hltTrackExtrapolatorForJPT = cms.EDProducer(
    "JPTAnalyticTrackExtrapolator",
    tracks = cms.InputTag("hltTrackWithVertexRefSelectorForJPT"),
    vertices = cms.InputTag("hltFirstStepPrimaryVerticesUnsorted"),
    seedJets = cms.InputTag("hltCaloJetsForJPTMerged"),
    trackJetSeeds = cms.InputTag("hltAK4TrackJetsForJPT"),
    minTrackPt = cms.double(0.18),
    minAbsPzForEndcap = cms.double(1.16),
    maxAbsDz = cms.double(0.30),
    minSeedJetPt = cms.double(5.0),
    maxDeltaRToSeed = cms.double(1.0),
    gridMaxAbsEta = cms.double(5.2),
    requireSeedMatch = cms.bool(True),
    barrelRadius = cms.double(129.0),
    barrelHalfLength = cms.double(315.0),
    endcapZ = cms.double(320.0),
    endcapMinRadius = cms.double(30.0),
    endcapMaxRadius = cms.double(170.0),
)
