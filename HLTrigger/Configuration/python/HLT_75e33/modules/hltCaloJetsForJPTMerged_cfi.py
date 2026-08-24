import FWCore.ParameterSet.Config as cms

# Barrel CaloJets (from CaloTowers) plus HGCAL endcap jets, in one collection
# spanning the full eta range. This is what JPT consumes.
hltCaloJetsForJPTMerged = cms.EDProducer("CaloJetMerger",
    src = cms.VInputTag(
        "hltAk4CaloJetsForTrk",
        "hltHGCalJetsForJPT"
    ),
    sortByPt = cms.bool(True)
)
