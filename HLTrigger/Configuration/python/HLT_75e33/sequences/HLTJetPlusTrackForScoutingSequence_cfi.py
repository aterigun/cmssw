import FWCore.ParameterSet.Config as cms

from ..modules.hltTrackWithVertexRefSelectorForJPT_cfi import *
from ..modules.hltTrackRefsForJPT_cfi import *
from ..modules.hltAK4TrackJetsForJPT_cfi import *
from ..modules.hltTrackExtrapolatorForJPT_cfi import *
from ..modules.hltJetPlusTrackAddonSeedRecoForJPT_cfi import *
from ..modules.hltJetPlusTrackZSPCorJetAntiKt4_cfi import *
from ..modules.hltAK4JetTracksAssociatorAtVertexForJPT_cfi import *
from ..modules.hltAK4JetTracksAssociatorAtCaloFaceForJPT_cfi import *
from ..modules.hltHGCalJetsForJPT_cfi import *
from ..modules.hltCaloJetsForJPTMerged_cfi import *
HLTJetPlusTrackForScoutingSequence = cms.Sequence(
    hltHGCalJetsForJPT
    + hltCaloJetsForJPTMerged
    + hltTrackWithVertexRefSelectorForJPT
    + hltTrackRefsForJPT
    + hltAK4TrackJetsForJPT
    + hltTrackExtrapolatorForJPT
    + hltJetPlusTrackAddonSeedRecoForJPT
    + hltAK4JetTracksAssociatorAtVertexForJPT
    + hltAK4JetTracksAssociatorAtCaloFaceForJPT
    + hltJetPlusTrackZSPCorJetAntiKt4
)
