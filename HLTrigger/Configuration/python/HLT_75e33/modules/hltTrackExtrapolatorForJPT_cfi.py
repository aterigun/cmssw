import FWCore.ParameterSet.Config as cms

hltTrackExtrapolatorForJPT = cms.EDProducer("TrackExtrapolator",
    trackSrc = cms.InputTag("hltGeneralTracks"),
    trackQuality = cms.string("highPurity"),
)
