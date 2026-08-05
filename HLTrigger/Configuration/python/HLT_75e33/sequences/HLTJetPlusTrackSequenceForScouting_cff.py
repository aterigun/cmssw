import FWCore.ParameterSet.Config as cms

from HLTrigger.Configuration.HLT_75e33.modules.hltTrackWithVertexRefSelectorForJPT_cfi import hltTrackWithVertexRefSelectorForJPT
from HLTrigger.Configuration.HLT_75e33.modules.hltTrackRefsForJPT_cfi import hltTrackRefsForJPT
from HLTrigger.Configuration.HLT_75e33.modules.hltAK4TrackJetsForJPT_cfi import hltAK4TrackJetsForJPT
from HLTrigger.Configuration.HLT_75e33.modules.hltTrackExtrapolatorForJPT_cfi import hltTrackExtrapolatorForJPT
from HLTrigger.Configuration.HLT_75e33.modules.hltJetPlusTrackAddonSeedRecoForJPT_cfi import hltJetPlusTrackAddonSeedRecoForJPT
from HLTrigger.Configuration.HLT_75e33.modules.hltJetPlusTrackZSPCorJetAntiKt4_cfi import hltJetPlusTrackZSPCorJetAntiKt4

HLTJetPlusTrackForScoutingTask = cms.Task(
    hltTrackWithVertexRefSelectorForJPT,
    hltTrackRefsForJPT,
    hltAK4TrackJetsForJPT,
    hltTrackExtrapolatorForJPT,
    hltJetPlusTrackAddonSeedRecoForJPT,
    hltJetPlusTrackZSPCorJetAntiKt4,
)

HLTJetPlusTrackForScoutingSequence = cms.Sequence(HLTJetPlusTrackForScoutingTask)
