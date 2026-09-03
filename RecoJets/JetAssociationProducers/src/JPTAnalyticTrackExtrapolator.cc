#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "DataFormats/GeometryVector/interface/GlobalPoint.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/JetReco/interface/TrackJetCollection.h"
#include "DataFormats/JetReco/interface/TrackExtrapolation.h"
#include "DataFormats/Math/interface/deltaPhi.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"

namespace {
  constexpr double kPi = 3.14159265358979323846;
  constexpr double kTwoPi = 2.0 * kPi;
  // GlobalTrajectoryParameters::transverseCurvature convention.
  constexpr double kBending = 2.99792458e-3;
  constexpr double kPathEpsilonCm = 1.e-6;
  constexpr std::size_t kEtaBins = 64;
  constexpr std::size_t kPhiBins = 64;

  struct TrackState {
    double x;
    double y;
    double z;
    double px;
    double py;
    double pz;
  };

  struct CaloImpact {
    reco::TrackBase::Point position;
    reco::TrackBase::Vector momentum;
    double transversePath = std::numeric_limits<double>::infinity();
  };

  bool finite(TrackState const& state) {
    return std::isfinite(state.x) && std::isfinite(state.y) && std::isfinite(state.z) &&
           std::isfinite(state.px) && std::isfinite(state.py) && std::isfinite(state.pz);
  }

  double normalizedPhi(double phi) { return std::atan2(std::sin(phi), std::cos(phi)); }

  // Stable sin(alpha)/alpha and (1-cos(alpha))/alpha evaluations.
  double sinc(double alpha) {
    if (std::abs(alpha) < 1.e-4) {
      double const alpha2 = alpha * alpha;
      return 1.0 - alpha2 / 6.0 + alpha2 * alpha2 / 120.0;
    }
    return std::sin(alpha) / alpha;
  }

  double cosc(double alpha) {
    if (std::abs(alpha) < 1.e-4) {
      double const alpha2 = alpha * alpha;
      return alpha * (0.5 - alpha2 / 24.0 + alpha2 * alpha2 / 720.0);
    }
    return (1.0 - std::cos(alpha)) / alpha;
  }

  TrackState stateAtTransversePath(TrackState const& start, double transversePath, double kappa) {
    double const pt = std::hypot(start.px, start.py);
    double const phi0 = std::atan2(start.py, start.px);

    TrackState result = start;
    if (std::abs(kappa) < 1.e-12) {
      result.x += transversePath * std::cos(phi0);
      result.y += transversePath * std::sin(phi0);
    } else {
      double const alpha = kappa * transversePath;
      result.x += transversePath * (-std::sin(phi0) * cosc(alpha) + std::cos(phi0) * sinc(alpha));
      result.y += transversePath * (std::cos(phi0) * cosc(alpha) + std::sin(phi0) * sinc(alpha));
      result.px = pt * std::cos(phi0 + alpha);
      result.py = pt * std::sin(phi0 + alpha);
    }
    result.z += transversePath * start.pz / pt;
    return result;
  }

  bool makeImpact(TrackState const& state, double transversePath, CaloImpact& impact) {
    if (!(transversePath > kPathEpsilonCm) || !std::isfinite(transversePath) || !finite(state)) {
      return false;
    }
    impact.position = reco::TrackBase::Point(state.x, state.y, state.z);
    impact.momentum = reco::TrackBase::Vector(state.px, state.py, state.pz);
    impact.transversePath = transversePath;
    return true;
  }

  bool intersectBarrelStraight(TrackState const& start,
                               double barrelRadius,
                               double barrelHalfLength,
                               CaloImpact& result) {
    double const pt = std::hypot(start.px, start.py);
    double const ux = start.px / pt;
    double const uy = start.py / pt;
    double const b = 2.0 * (start.x * ux + start.y * uy);
    double const c = start.x * start.x + start.y * start.y - barrelRadius * barrelRadius;
    double const discriminant = b * b - 4.0 * c;
    if (discriminant < 0.0) {
      return false;
    }

    double const root = std::sqrt(std::max(0.0, discriminant));
    std::array<double, 2> const paths{{(-b - root) * 0.5, (-b + root) * 0.5}};
    for (double const transversePath : paths) {
      if (!(transversePath > kPathEpsilonCm)) {
        continue;
      }
      TrackState const state = stateAtTransversePath(start, transversePath, 0.0);
      if (std::abs(state.z) <= barrelHalfLength && makeImpact(state, transversePath, result)) {
        return true;  // The quadratic roots are ordered.
      }
    }
    return false;
  }

  bool intersectBarrelHelix(TrackState const& start,
                            double kappa,
                            double barrelRadius,
                            double barrelHalfLength,
                            CaloImpact& result) {
    double const phi0 = std::atan2(start.py, start.px);
    double const rho = 1.0 / std::abs(kappa);
    double const centerX = start.x - std::sin(phi0) / kappa;
    double const centerY = start.y + std::cos(phi0) / kappa;
    double const centerDistance = std::hypot(centerX, centerY);

    double const scale = std::max({1.0, rho, barrelRadius, centerDistance});
    double const tolerance = 64.0 * std::numeric_limits<double>::epsilon() * scale;
    if (centerDistance <= tolerance || centerDistance > rho + barrelRadius + tolerance ||
        centerDistance < std::abs(rho - barrelRadius) - tolerance) {
      return false;
    }

    double const a = (barrelRadius * barrelRadius - rho * rho + centerDistance * centerDistance) /
                     (2.0 * centerDistance);
    double h2 = barrelRadius * barrelRadius - a * a;
    double const h2Tolerance = 128.0 * std::numeric_limits<double>::epsilon() * scale * scale;
    if (h2 < -h2Tolerance) {
      return false;
    }
    h2 = std::max(0.0, h2);

    double const baseX = a * centerX / centerDistance;
    double const baseY = a * centerY / centerDistance;
    double const offsetX = -centerY * std::sqrt(h2) / centerDistance;
    double const offsetY = centerX * std::sqrt(h2) / centerDistance;

    CaloImpact best;
    bool found = false;
    for (int const sign : {-1, 1}) {
      double const hitX = baseX + sign * offsetX;
      double const hitY = baseY + sign * offsetY;
      double const sinPhi = std::clamp(kappa * (hitX - centerX), -1.0, 1.0);
      double const cosPhi = std::clamp(kappa * (centerY - hitY), -1.0, 1.0);
      double const phiAtHit = std::atan2(sinPhi, cosPhi);
      double alpha = std::remainder(phiAtHit - phi0, kTwoPi);

      // Select the first phase-equivalent crossing along the momentum.
      if (kappa > 0.0 && alpha <= 1.e-12) {
        alpha += kTwoPi;
      } else if (kappa < 0.0 && alpha >= -1.e-12) {
        alpha -= kTwoPi;
      }
      double const transversePath = alpha / kappa;
      if (!(transversePath > kPathEpsilonCm)) {
        continue;
      }

      TrackState const state = stateAtTransversePath(start, transversePath, kappa);
      CaloImpact candidate;
      if (std::abs(state.z) <= barrelHalfLength && makeImpact(state, transversePath, candidate) &&
          (!found || candidate.transversePath < best.transversePath)) {
        best = candidate;
        found = true;
      }
    }

    if (found) {
      result = best;
    }
    return found;
  }

  bool intersectEndcap(TrackState const& start,
                       double kappa,
                       double endcapZ,
                       double endcapMinRadius,
                       double endcapMaxRadius,
                       CaloImpact& result) {
    double const pt = std::hypot(start.px, start.py);
    if (start.pz == 0.0) {
      return false;
    }
    double const transversePath = (endcapZ - start.z) * pt / start.pz;
    if (!(transversePath > kPathEpsilonCm)) {
      return false;
    }

    TrackState const state = stateAtTransversePath(start, transversePath, kappa);
    double const radius = std::hypot(state.x, state.y);
    if (radius < endcapMinRadius || radius > endcapMaxRadius) {
      return false;
    }
    return makeImpact(state, transversePath, result);
  }
}  // namespace

class JPTAnalyticTrackExtrapolator final : public edm::global::EDProducer<> {
public:
  explicit JPTAnalyticTrackExtrapolator(edm::ParameterSet const& config)
      : tracksToken_(consumes<reco::TrackRefVector>(config.getParameter<edm::InputTag>("tracks"))),
        verticesToken_(consumes<reco::VertexCollection>(config.getParameter<edm::InputTag>("vertices"))),
        seedJetsToken_(consumes<reco::CaloJetCollection>(config.getParameter<edm::InputTag>("seedJets"))),
        trackJetSeedsToken_(
            consumes<reco::TrackJetCollection>(config.getParameter<edm::InputTag>("trackJetSeeds"))),
        fieldToken_(esConsumes<MagneticField, IdealMagneticFieldRecord>()),
        outputToken_(produces<std::vector<reco::TrackExtrapolation>>()),
        minTrackPt_(config.getParameter<double>("minTrackPt")),
        minAbsPzForEndcap_(config.getParameter<double>("minAbsPzForEndcap")),
        maxAbsDz_(config.getParameter<double>("maxAbsDz")),
        minSeedJetPt_(config.getParameter<double>("minSeedJetPt")),
        maxDeltaRToSeed_(config.getParameter<double>("maxDeltaRToSeed")),
        gridMaxAbsEta_(config.getParameter<double>("gridMaxAbsEta")),
        requireSeedMatch_(config.getParameter<bool>("requireSeedMatch")),
        barrelRadius_(config.getParameter<double>("barrelRadius")),
        barrelHalfLength_(config.getParameter<double>("barrelHalfLength")),
        endcapZ_(config.getParameter<double>("endcapZ")),
        endcapMinRadius_(config.getParameter<double>("endcapMinRadius")),
        endcapMaxRadius_(config.getParameter<double>("endcapMaxRadius")) {
    if (!(minTrackPt_ > 0.0) || !(minAbsPzForEndcap_ >= 0.0) || !(maxAbsDz_ > 0.0) ||
        !(maxDeltaRToSeed_ > 0.0) || !(gridMaxAbsEta_ > 0.0) || !(barrelRadius_ > 0.0) ||
        !(barrelHalfLength_ > 0.0) || !(endcapZ_ > 0.0) || !(endcapMinRadius_ >= 0.0) ||
        !(endcapMaxRadius_ > endcapMinRadius_)) {
      throw cms::Exception("Configuration") << "Invalid JPTAnalyticTrackExtrapolator geometry or selection bounds";
    }
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("tracks", edm::InputTag("hltTrackWithVertexRefSelectorForJPT"))
        ->setComment("PV-selected reco::TrackRefVector; no second scan of hltGeneralTracks");
    desc.add<edm::InputTag>("vertices", edm::InputTag("hltFirstStepPrimaryVerticesUnsorted"));
    desc.add<edm::InputTag>("seedJets", edm::InputTag("hltCaloJetsForJPTMerged"));
    desc.add<edm::InputTag>("trackJetSeeds", edm::InputTag("hltAK4TrackJetsForJPT"))
        ->setComment("Track-jet ROIs retain the calorimeter-invisible addon-seed population");
    desc.add<double>("minTrackPt", 0.18)->setComment("Analytic reachability pre-cut [GeV]");
    desc.add<double>("minAbsPzForEndcap", 1.16)->setComment("Endcap-only |pz| reachability pre-cut [GeV]");
    desc.add<double>("maxAbsDz", 0.30)->setComment("Maximum |dz(track,PV)| [cm]");
    desc.add<double>("minSeedJetPt", 5.0)->setComment("Minimum seed-jet pT used to populate the ROI grid [GeV]");
    desc.add<double>("maxDeltaRToSeed", 1.0)
        ->setComment("Conservative vertex-direction ROI; includes low-pT magnetic bending");
    desc.add<double>("gridMaxAbsEta", 5.2);
    desc.add<bool>("requireSeedMatch", true);
    desc.add<double>("barrelRadius", 129.0)->setComment("Effective barrel calorimeter entrance radius [cm]");
    desc.add<double>("barrelHalfLength", 315.0)->setComment("Finite barrel half-length [cm]");
    desc.add<double>("endcapZ", 320.0)->setComment("Absolute HGCAL entrance z [cm]");
    desc.add<double>("endcapMinRadius", 30.0)->setComment("Inner HGCAL entrance radius [cm]");
    desc.add<double>("endcapMaxRadius", 170.0)->setComment("Outer HGCAL entrance radius [cm]");
    descriptions.addWithDefaultLabel(desc);
  }

private:
  using SeedGrid = std::array<std::uint8_t, kEtaBins * kPhiBins>;

  int etaBin(double eta) const {
    if (!std::isfinite(eta) || eta < -gridMaxAbsEta_ || eta >= gridMaxAbsEta_) {
      return -1;
    }
    double const fraction = (eta + gridMaxAbsEta_) / (2.0 * gridMaxAbsEta_);
    return std::clamp(static_cast<int>(fraction * kEtaBins), 0, static_cast<int>(kEtaBins) - 1);
  }

  static int phiBin(double phi) {
    double const wrapped = normalizedPhi(phi);
    double const fraction = (wrapped + kPi) / kTwoPi;
    return std::clamp(static_cast<int>(fraction * kPhiBins), 0, static_cast<int>(kPhiBins) - 1);
  }

  template <typename JetCollection>
  void markSeedGrid(JetCollection const& jets, SeedGrid& grid) const {
    double const etaWidth = 2.0 * gridMaxAbsEta_ / kEtaBins;
    double const phiWidth = kTwoPi / kPhiBins;
    double const halfDiagonal = 0.5 * std::hypot(etaWidth, phiWidth);
    double const markingRadius = maxDeltaRToSeed_ + halfDiagonal;

    for (auto const& jet : jets) {
      if (jet.pt() < minSeedJetPt_ || !std::isfinite(jet.eta()) || !std::isfinite(jet.phi())) {
        continue;
      }
      int const firstEta = std::max(0, etaBin(jet.eta() - markingRadius));
      int const lastCandidate = etaBin(jet.eta() + markingRadius);
      int const lastEta = lastCandidate < 0 ? static_cast<int>(kEtaBins) - 1 : lastCandidate;
      for (int iEta = firstEta; iEta <= lastEta; ++iEta) {
        double const etaCenter = -gridMaxAbsEta_ + (iEta + 0.5) * etaWidth;
        if (std::abs(etaCenter - jet.eta()) > markingRadius) {
          continue;
        }
        for (std::size_t iPhi = 0; iPhi < kPhiBins; ++iPhi) {
          double const phiCenter = -kPi + (iPhi + 0.5) * phiWidth;
          double const dPhi = reco::deltaPhi(phiCenter, jet.phi());
          if (std::hypot(etaCenter - jet.eta(), dPhi) <= markingRadius) {
            grid[static_cast<std::size_t>(iEta) * kPhiBins + iPhi] = 1;
          }
        }
      }
    }
  }

  SeedGrid makeSeedGrid(reco::CaloJetCollection const& caloJets, reco::TrackJetCollection const& trackJets) const {
    SeedGrid grid{};
    markSeedGrid(caloJets, grid);
    markSeedGrid(trackJets, grid);
    return grid;
  }

  bool isInSeedGrid(reco::Track const& track, SeedGrid const& grid) const {
    int const iEta = etaBin(track.eta());
    if (iEta < 0) {
      return false;
    }
    int const iPhi = phiBin(track.phi());
    return grid[static_cast<std::size_t>(iEta) * kPhiBins + static_cast<std::size_t>(iPhi)] != 0;
  }

  bool propagate(reco::Track const& track, double bzTesla, CaloImpact& result) const {
    TrackState start{track.vx(), track.vy(), track.vz(), track.px(), track.py(), track.pz()};
    if (track.outerOk()) {
      auto const& position = track.outerPosition();
      auto const& momentum = track.outerMomentum();
      start = {position.x(), position.y(), position.z(), momentum.x(), momentum.y(), momentum.z()};
    }
    if (!finite(start)) {
      return false;
    }

    double const pt = std::hypot(start.px, start.py);
    if (!(pt > 0.0)) {
      return false;
    }
    double const kappa = -kBending * static_cast<double>(track.charge()) * bzTesla / pt;

    CaloImpact best;
    bool found = false;
    CaloImpact candidate;
    bool const barrelValid = std::abs(kappa) < 1.e-12
                                 ? intersectBarrelStraight(start, barrelRadius_, barrelHalfLength_, candidate)
                                 : intersectBarrelHelix(start, kappa, barrelRadius_, barrelHalfLength_, candidate);
    if (barrelValid) {
      best = candidate;
      found = true;
    }

    // Endcap candidates are independent of the cylinder result.
    if (std::abs(start.pz) > minAbsPzForEndcap_) {
      for (double const z : {-endcapZ_, endcapZ_}) {
        if (intersectEndcap(start, kappa, z, endcapMinRadius_, endcapMaxRadius_, candidate) &&
            (!found || candidate.transversePath < best.transversePath)) {
          best = candidate;
          found = true;
        }
      }
    }

    if (found) {
      result = best;
    }
    return found;
  }

  void produce(edm::StreamID, edm::Event& event, edm::EventSetup const& setup) const override {
    auto const& tracks = event.get(tracksToken_);
    auto const& vertices = event.get(verticesToken_);
    auto const& seedJets = event.get(seedJetsToken_);
    auto const& trackJetSeeds = event.get(trackJetSeedsToken_);

    auto primaryVertex = std::find_if(vertices.begin(), vertices.end(), [](reco::Vertex const& vertex) {
      return vertex.isValid() && !vertex.isFake();
    });
    if (primaryVertex == vertices.end()) {
      primaryVertex = std::find_if(vertices.begin(), vertices.end(), [](reco::Vertex const& vertex) {
        return vertex.isValid();
      });
    }

    std::vector<reco::TrackExtrapolation> output;
    output.reserve(tracks.size());
    if (primaryVertex == vertices.end()) {
      event.emplace(outputToken_, std::move(output));
      return;
    }

    SeedGrid const seedGrid = makeSeedGrid(seedJets, trackJetSeeds);
    double const bzTesla = setup.getData(fieldToken_).inTesla(GlobalPoint(0.0, 0.0, 0.0)).z();

    std::vector<reco::TrackBase::Point> positions;
    std::vector<reco::TrackBase::Vector> momenta;
    positions.reserve(1);
    momenta.reserve(1);
    positions.resize(1);
    momenta.resize(1);

    for (auto const& trackRef : tracks) {
      if (trackRef.isNull() || !trackRef.isAvailable()) {
        continue;
      }
      reco::Track const& track = *trackRef;
      if (!(track.pt() > minTrackPt_) || std::abs(track.dz(primaryVertex->position())) >= maxAbsDz_ ||
          (requireSeedMatch_ && !isInSeedGrid(track, seedGrid))) {
        continue;
      }

      CaloImpact impact;
      if (!propagate(track, bzTesla, impact)) {
        continue;
      }
      positions[0] = impact.position;
      momenta[0] = impact.momentum;
      output.emplace_back(trackRef, positions, momenta);
    }

    event.emplace(outputToken_, std::move(output));
  }

  edm::EDGetTokenT<reco::TrackRefVector> const tracksToken_;
  edm::EDGetTokenT<reco::VertexCollection> const verticesToken_;
  edm::EDGetTokenT<reco::CaloJetCollection> const seedJetsToken_;
  edm::EDGetTokenT<reco::TrackJetCollection> const trackJetSeedsToken_;
  edm::ESGetToken<MagneticField, IdealMagneticFieldRecord> const fieldToken_;
  edm::EDPutTokenT<std::vector<reco::TrackExtrapolation>> const outputToken_;

  double const minTrackPt_;
  double const minAbsPzForEndcap_;
  double const maxAbsDz_;
  double const minSeedJetPt_;
  double const maxDeltaRToSeed_;
  double const gridMaxAbsEta_;
  bool const requireSeedMatch_;

  double const barrelRadius_;
  double const barrelHalfLength_;
  double const endcapZ_;
  double const endcapMinRadius_;
  double const endcapMaxRadius_;
};

DEFINE_FWK_MODULE(JPTAnalyticTrackExtrapolator);
