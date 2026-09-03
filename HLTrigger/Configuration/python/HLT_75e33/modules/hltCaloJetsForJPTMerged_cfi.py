import math

import FWCore.ParameterSet.Config as cms


hltCaloJetsForJPTMerged = cms.EDProducer(
    "HybridCaloJetMerger",
    barrelJets = cms.InputTag("hltAk4CaloJetsForTrk"),
    hgcalJets = cms.InputTag("hltHGCalJetsForJPT"),
    transitionMinAbsEta = cms.double(1.30),
    transitionMaxAbsEta = cms.double(1.80),
    overlapDeltaR = cms.double(0.40),
    fallbackJetArea = cms.double(math.pi * 0.4 * 0.4),
    requireConstituents = cms.bool(True),
)
