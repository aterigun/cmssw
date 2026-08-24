import FWCore.ParameterSet.Config as cms

# Anti-kT R=0.4 jets clustered from HGCAL layer clusters, covering the endcap
# region that CaloTowers (and hence hltAk4CaloJetsForTrk) do not reach.
# Endcap only: the barrel is combined in separately by hltCaloJetsForJPTMerged.
hltHGCalJetsForJPT = cms.EDProducer("HGCalJetProducer",
    layerClusters = cms.InputTag("hltMergeLayerClusters"),
    antiktRadius = cms.double(0.4),
    jetPtMin = cms.double(10.0),
    inputEtMin = cms.double(0.3),
    minEta = cms.double(1.5)
)
