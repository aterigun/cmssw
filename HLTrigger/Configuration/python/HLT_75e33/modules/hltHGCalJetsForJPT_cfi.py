import FWCore.ParameterSet.Config as cms


hltHGCalJetsForJPT = cms.EDProducer(
    "TICLTracksterJetProducer",
    tracksters = cms.InputTag("hltTiclCandidate"),
    vertices = cms.InputTag("hltFirstStepPrimaryVerticesUnsorted"),
    antiktRadius = cms.double(0.4),
    jetPtMin = cms.double(10.0),
    minTracksterEnergy = cms.double(0.5),
    minTracksterEt = cms.double(0.3),
    minAbsEta = cms.double(1.30),
    maxAbsEta = cms.double(3.00),
    enableTiming = cms.bool(True),
    maxTimeDifference = cms.double(0.50),
    maxTimeDifferenceWithoutPVTime = cms.double(1.00),
    timeNSigma = cms.double(3.0),
    maxTracksterTimeError = cms.double(0.20),
    useRawEnergyFallback = cms.bool(False),
)
