import FWCore.ParameterSet.Config as cms

# Anti-kT R=0.4 jets from HGCAL layer clusters, covering the endcap region
# that CaloTowers (and hence hltAk4CaloJetsForTrk) do not reach.
# The barrel CaloJets are copied through unchanged, so the output is a single
# CaloJetCollection spanning the full eta range for downstream JPT.
hltHGCalJetsForJPT = cms.EDProducer("HGCalJetProducer",
    layerClusters = cms.InputTag("hltMergeLayerClusters"),
    barrelJets = cms.InputTag("hltAk4CaloJetsForTrk"),
    antiktRadius = cms.double(0.4),
    jetPtMin = cms.double(10.0),
    inputEtMin = cms.double(0.3),
    minEta = cms.double(1.5)
)
