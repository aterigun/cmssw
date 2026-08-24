import FWCore.ParameterSet.Config as cms

hltJetPlusTrackZSPCorJetAntiKt4 = cms.EDProducer("JetPlusTrackProducer",
    # --- producer level
    src = cms.InputTag("hltHGCalJetsForJPT"),
    srcTrackJets = cms.InputTag("hltAK4TrackJetsForJPT"),
    srcAddCaloJets = cms.InputTag("hltJetPlusTrackAddonSeedRecoForJPT"),
    extrapolations = cms.InputTag("hltTrackExtrapolatorForJPT"),
    srcPVs = cms.InputTag("hltOfflinePrimaryVertices"),
    alias = cms.untracked.string("hltJetPlusTrackZSPCorJetAntiKt4"),
    ptCUT = cms.double(15.0),
    dRcone = cms.double(0.4),
    UsePAT = cms.bool(False),
    Verbose = cms.bool(True),

    # --- vectorial correction
    VectorialCorrection = cms.bool(True),
    UseResponseInVecCorr = cms.bool(False),

    # --- track selection for the correction
    UseInConeTracks = cms.bool(True),
    UseOutOfConeTracks = cms.bool(True),
    UseOutOfVertexTracks = cms.bool(True),

    # --- jet-track association
    JetTracksAssociationAtVertex = cms.InputTag("hltAK4JetTracksAssociatorAtVertexForJPT"),
    JetTracksAssociationAtCaloFace = cms.InputTag("hltAK4JetTracksAssociatorAtCaloFaceForJPT"),
    JetSplitMerge = cms.int32(2),

    # --- pions
    UsePions = cms.bool(True),
    UseEfficiency = cms.bool(True),

    # --- muons
    UseMuons = cms.bool(False),
    Muons = cms.InputTag("hltPhase2L3MuonsNoID"),
    PatMuons = cms.InputTag(""),
    muonPtmatch = cms.double(0.1),
    muonEtamatch = cms.double(0.001),
    muonPhimatch = cms.double(0.001),

    # --- electrons
    UseElectrons = cms.bool(False),
    Electrons = cms.InputTag(""),
    ElectronIds = cms.InputTag(""),
    PatElectrons = cms.InputTag(""),
    electronDRmatch = cms.double(0.02),

    # --- track quality
    UseTrackQuality = cms.bool(False),
    TrackQuality = cms.string("highPurity"),
    PtErrorQuality = cms.double(0.05),
    DzVertexCut = cms.double(0.2),

    # --- maps
    ResponseMap = cms.string("CondFormats/JetMETObjects/data/CMSSW_538_response.txt"),
    EfficiencyMap = cms.string("CondFormats/JetMETObjects/data/CMSSW_538_TrackNonEff.txt"),
    LeakageMap = cms.string("CondFormats/JetMETObjects/data/CMSSW_538_TrackLeakage.txt"),
    MaxJetEta = cms.double(3.0),

    # --- ZSP
    UseZSP = cms.bool(False),
    tagName = cms.vstring("ZSP_CMSSW390_Akt_05_PU0"),
    tagNameOffset = cms.vstring(),
    PU = cms.int32(-1),
    FixedPU = cms.int32(0),
)
