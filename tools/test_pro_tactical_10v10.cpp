// Verification Suite for ADVANCED TACTICAL BOT AI (CS 1.6 DE_DUST2 — 10 vs 10 / 20 Bots Pro Engine)
// Includes Verification of 50% AWP / 50% AK-47 & M4A1 Composition + Pro Primary <-> Secondary (Deagle) Switching
#include "src/copycat/copycat_core.h"
#include "src/copycat/copycat_final.h"
#include "src/copycat/copycat_aim.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace rbcc;

int main () {
  // 1. Verify 10 vs 10 Match Structure (10 T vs 10 CT = 20 Bots)
  assert (kPro10v10TeamSize == 10);
  assert (kPro10v10TotalBots == 20);

  // 2. Verify all 19 T AWP Spots on de_dust2
  const auto &tSpots = ProTactical10v10Engine::tAwpSpots ();
  assert (tSpots.size () == 19u);
  const char *expectedTSpots[19] = {
    "T Spawn", "Suicide", "Top Mid", "Xbox", "Mid to B", "T Mid Doors",
    "Long Doors", "Long Corner", "Outside Long", "A Long", "Pit", "Pit Corner",
    "A Short", "Short Corner", "Upper B Tunnels", "Lower B Tunnels",
    "B Tunnel Exit", "B Doors", "B Tunnel Stairs"
  };
  for (size_t i = 0; i < 19u; ++i) {
    assert (std::strcmp (tSpots[i].name, expectedTSpots[i]) == 0);
    assert (!tSpots[i].origin.empty ());
  }
  std::printf ("[PASS] 19 Terrorist (T) de_dust2 AWP Spots verified.\n");

  // 3. Verify all 21 CT AWP Spots on de_dust2
  const auto &ctSpots = ProTactical10v10Engine::ctAwpSpots ();
  assert (ctSpots.size () == 21u);
  const char *expectedCTSpots[21] = {
    "CT Spawn", "CT Mid", "Mid Doors", "B Window", "B Site", "B Platform",
    "Back Platform", "Back Site", "B Car", "B Doors", "B Double Box",
    "A Site", "A Ramp", "Goose", "A Site Boxes", "A Long Corner",
    "Blue", "Pit", "A Short / Catwalk", "Short Stairs", "A Site Corner"
  };
  for (size_t i = 0; i < 21u; ++i) {
    assert (std::strcmp (ctSpots[i].name, expectedCTSpots[i]) == 0);
    assert (!ctSpots[i].origin.empty ());
  }
  std::printf ("[PASS] 21 Counter-Terrorist (CT) de_dust2 AWP Spots verified.\n");

  // 4. Verify all 4 Dynamic 10v10 Team Formations (4-2-4, 3-4-3, 5-2-3, 3-2-5)
  auto &eng = proTacticalEngine ();
  eng.clear ();
  int a = 0, mid = 0, b = 0;
  eng.getFormationCounts (DynamicFormation10v10::Formation_4A_2Mid_4B, a, mid, b);
  assert (a == 4 && mid == 2 && b == 4 && (a + mid + b == 10));
  eng.getFormationCounts (DynamicFormation10v10::Formation_3A_4Mid_3B, a, mid, b);
  assert (a == 3 && mid == 4 && b == 3 && (a + mid + b == 10));
  eng.getFormationCounts (DynamicFormation10v10::Formation_5A_2Mid_3B, a, mid, b);
  assert (a == 5 && mid == 2 && b == 3 && (a + mid + b == 10));
  eng.getFormationCounts (DynamicFormation10v10::Formation_3A_2Mid_5B, a, mid, b);
  assert (a == 3 && mid == 2 && b == 5 && (a + mid + b == 10));
  std::printf ("[PASS] 4 Dynamic 10v10 Team Formations (4-2-4, 3-4-3, 5-2-3, 3-2-5) verified.\n");

  // 5. Verify 50% AWP + 50% AK-47/M4A1 Team Loadout Composition (5 AWP + 5 AK-47 on T, 5 AWP + 5 M4A1 on CT)
  assert (ProTactical10v10Engine::kDefaultAwpCompositionPercent == 50);
  assert (ProTactical10v10Engine::kDefaultRifleCompositionPercent == 50);
  assert (ProTactical10v10Engine::computeAwpQuota (10, 50, 1u) == 5);
  assert (ProTactical10v10Engine::computeAwpQuota (8, 50, 1u) == 4);
  assert (ProTactical10v10Engine::computeAwpQuota (6, 50, 1u) == 3);

  std::vector<uint8_t> tPrim, tSec, ctPrim, ctSec;
  std::vector<bool> tAwps50, ctAwps50;
  ProTactical10v10Engine::buildBalancedTeamLoadout (0u, 10, 1u, tPrim, tSec, tAwps50);
  ProTactical10v10Engine::buildBalancedTeamLoadout (1u, 10, 1u, ctPrim, ctSec, ctAwps50);

  int tAwpCount = 0, tAk47Count = 0, ctAwpCount = 0, ctM4a1Count = 0;
  for (size_t i = 0; i < 10u; ++i) {
    if (tPrim[i] == ProWeaponSwitchController::kWeaponAWP) ++tAwpCount;
    if (tPrim[i] == ProWeaponSwitchController::kWeaponAK47) ++tAk47Count;
    assert (tSec[i] == ProWeaponSwitchController::kWeaponDeagle);

    if (ctPrim[i] == ProWeaponSwitchController::kWeaponAWP) ++ctAwpCount;
    if (ctPrim[i] == ProWeaponSwitchController::kWeaponM4A1) ++ctM4a1Count;
    assert (ctSec[i] == ProWeaponSwitchController::kWeaponDeagle);
  }
  assert (tAwpCount == 5 && tAk47Count == 5);
  assert (ctAwpCount == 5 && ctM4a1Count == 5);
  std::printf ("[PASS] 50%% AWP / 50%% AK-47 & M4A1 Loadout verified (T: %d AWP + %d AK-47 | CT: %d AWP + %d M4A1, Secondary: Deagle).\n",
    tAwpCount, tAk47Count, ctAwpCount, ctM4a1Count);

  // 6. Verify 10v10 Roster Assignment with 50% AWP Snipers & Dynamic AWP Repositioning
  uint32_t seed = 0xA1B01010u;
  eng.startNewRound (1u, seed, 0.75f);
  std::vector<int> ctSlots = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  std::vector<bool> ctC4s (10, false);
  eng.assignTeamRoster (1u, ctSlots, ctAwps50, ctC4s, seed);

  std::vector<int> tSlots = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
  std::vector<bool> tC4s = { true, false, false, false, false, false, false, false, false, false };
  eng.assignTeamRoster (0u, tSlots, tAwps50, tC4s, seed);

  int ctSiteAnchors = 0, ctAwpRoles = 0, tAwpRoles = 0;
  for (int s : ctSlots) {
    if (eng.roleForSlot (s) == ProRole10v10::SiteAnchor) ++ctSiteAnchors;
    if (eng.roleForSlot (s) == ProRole10v10::AWPSniper) {
      ++ctAwpRoles;
      assert (eng.awpSpotForSlot (s) >= 0 && eng.awpSpotForSlot (s) < 21);
    }
  }
  for (int s : tSlots) {
    if (eng.roleForSlot (s) == ProRole10v10::AWPSniper) {
      ++tAwpRoles;
      assert (eng.awpSpotForSlot (s) >= 0 && eng.awpSpotForSlot (s) < 19);
    }
  }
  assert (ctSiteAnchors >= 2); // Both A Site and B Site always have dedicated Site Anchors!
  assert (ctAwpRoles == 5);    // 50% of 10 CTs are AWP Snipers
  assert (tAwpRoles == 5);     // 50% of 10 Ts are AWP Snipers

  // Test dynamic AWP repositioning after firing a shot
  const int initialSpot = eng.awpSpotForSlot (2);
  eng.notifyAwpShotFired (2, 15.0f, seed);
  const int newSpot = eng.awpSpotForSlot (2);
  assert (newSpot >= 0 && newSpot < 21 && newSpot != initialSpot);
  std::printf ("[PASS] 10v10 50%% AWP Roster Distribution & Dynamic AWP Shot Repositioning verified (%s -> %s).\n",
    ctSpots[static_cast<size_t> (initialSpot)].name, ctSpots[static_cast<size_t> (newSpot)].name);

  // 7. Verify Pro Primary <-> Secondary Weapon Switching for BOTH AWP Bots and AK-47/M4A1 Bots
  {
    const uint32_t awpDeagleMask = (1u << ProWeaponSwitchController::kWeaponAWP) | (1u << ProWeaponSwitchController::kWeaponDeagle);
    const uint32_t akDeagleMask  = (1u << ProWeaponSwitchController::kWeaponAK47) | (1u << ProWeaponSwitchController::kWeaponDeagle);
    const uint32_t m4DeagleMask  = (1u << ProWeaponSwitchController::kWeaponM4A1) | (1u << ProWeaponSwitchController::kWeaponDeagle);

    // 7A. Pro AWP Bot: fires AWP shot -> rapidly quick-switches to Deagle -> returns to AWP
    ProWeaponSwitchController awpSwitcher;
    awpSwitcher.onShotFired (10.0f, ProWeaponSwitchController::kWeaponAWP, 9, 1u, true, 520.0f, AimStyle::Awper);
    const auto swAwpToDeagle = awpSwitcher.evaluate (
      10.07f, ProWeaponSwitchController::kWeaponAWP, awpDeagleMask, 9, 7, false, true, 520.0f, true, 1u, AimStyle::Awper);
    assert (swAwpToDeagle.shouldSwitch);
    assert (swAwpToDeagle.targetWeapon == ProWeaponSwitchController::kWeaponDeagle);
    assert (swAwpToDeagle.reason == SwitchReason::AwpPostShotQuickSwitch);
    assert (awpSwitcher.holdingSecondary ());
    assert (awpSwitcher.awpToSecondarySwitches () == 1u);

    // Once threat is cleared and return timer elapses, bot switches back from Deagle to AWP
    const auto swDeagleToAwp = awpSwitcher.evaluate (
      11.50f, ProWeaponSwitchController::kWeaponDeagle, awpDeagleMask, 9, 6, false, false, 1200.0f, false, 0u, AimStyle::Awper);
    assert (swDeagleToAwp.shouldSwitch);
    assert (swDeagleToAwp.targetWeapon == ProWeaponSwitchController::kWeaponAWP);
    assert (swDeagleToAwp.reason == SwitchReason::ReturnToPrimary);
    assert (!awpSwitcher.holdingSecondary ());

    // 7B. Pro AWP Bot: walking/running to spot -> switches AWP to Deagle (250 u/s), then upon reaching spot switches back to AWP!
    ProWeaponSwitchController travelAwpSwitcher;
    const auto swTravelStart = travelAwpSwitcher.evaluate (
      12.0f, ProWeaponSwitchController::kWeaponAWP, awpDeagleMask, 10, 7, false, false, 9999.0f, false, 0u, AimStyle::Awper, true);
    assert (swTravelStart.shouldSwitch);
    assert (swTravelStart.targetWeapon == ProWeaponSwitchController::kWeaponDeagle);
    assert (swTravelStart.reason == SwitchReason::AwpTravelWithDeagle);
    assert (travelAwpSwitcher.holdingSecondary ());

    // Still walking toward spot -> keeps Deagle in hand
    const auto swTravelMid = travelAwpSwitcher.evaluate (
      13.5f, ProWeaponSwitchController::kWeaponDeagle, awpDeagleMask, 10, 7, false, false, 9999.0f, false, 0u, AimStyle::Awper, true);
    assert (!swTravelMid.shouldSwitch);
    assert (swTravelMid.holdingSecondary);

    // Arrives at spot (isTravelingToSpot = false) -> immediately switches from Deagle back to AWP!
    const auto swArriveSpot = travelAwpSwitcher.evaluate (
      15.0f, ProWeaponSwitchController::kWeaponDeagle, awpDeagleMask, 10, 7, false, false, 9999.0f, false, 0u, AimStyle::Awper, false);
    assert (swArriveSpot.shouldSwitch);
    assert (swArriveSpot.targetWeapon == ProWeaponSwitchController::kWeaponAWP);
    assert (swArriveSpot.reason == SwitchReason::ReturnToPrimary);
    assert (!travelAwpSwitcher.holdingSecondary ());

    // 7B-2. Pro AWP Bot: close-quarters enemy ambush while unscoped -> immediately switches to Deagle
    const auto swAwpClose = awpSwitcher.evaluate (
      15.0f, ProWeaponSwitchController::kWeaponAWP, awpDeagleMask, 8, 7, false, true, 260.0f, false, 0u, AimStyle::Awper);
    assert (swAwpClose.shouldSwitch);
    assert (swAwpClose.targetWeapon == ProWeaponSwitchController::kWeaponDeagle);
    assert (swAwpClose.reason == SwitchReason::AwpCloseQuartersDefense);

    // 7C. Pro AK-47 Bot: low/empty magazine mid-fight -> immediately switches from AK-47 to Deagle
    ProWeaponSwitchController akSwitcher;
    akSwitcher.onShotFired (20.0f, ProWeaponSwitchController::kWeaponAK47, 2, 5u, true, 480.0f, AimStyle::Spray);
    const auto swAkToDeagle = akSwitcher.evaluate (
      20.06f, ProWeaponSwitchController::kWeaponAK47, akDeagleMask, 2, 7, false, true, 480.0f, false, 5u, AimStyle::Spray);
    assert (swAkToDeagle.shouldSwitch);
    assert (swAkToDeagle.targetWeapon == ProWeaponSwitchController::kWeaponDeagle);
    assert (swAkToDeagle.reason == SwitchReason::RifleEmptyOrLowClipCombat);
    assert (akSwitcher.holdingSecondary ());
    assert (akSwitcher.rifleToSecondarySwitches () == 1u);

    // Return from Deagle to AK-47 once engagement clears and rifle has ammo
    const auto swDeagleToAk = akSwitcher.evaluate (
      22.0f, ProWeaponSwitchController::kWeaponDeagle, akDeagleMask, 30, 5, false, false, 1500.0f, false, 0u, AimStyle::Spray);
    assert (swDeagleToAk.shouldSwitch);
    assert (swDeagleToAk.targetWeapon == ProWeaponSwitchController::kWeaponAK47);

    // 7D. Pro M4A1 Bot: sustained burst/spray finisher OR reload interrupted by enemy -> immediately switches to Deagle
    ProWeaponSwitchController m4Switcher;
    const auto swM4ReloadInterrupt = m4Switcher.evaluate (
      30.0f, ProWeaponSwitchController::kWeaponM4A1, m4DeagleMask, 10, 7, true, true, 380.0f, false, 0u, AimStyle::Precision);
    assert (swM4ReloadInterrupt.shouldSwitch);
    assert (swM4ReloadInterrupt.cancelReload);
    assert (swM4ReloadInterrupt.targetWeapon == ProWeaponSwitchController::kWeaponDeagle);
    assert (swM4ReloadInterrupt.reason == SwitchReason::ReloadInterruptedByEnemy);

    std::printf ("[PASS] Pro Primary <-> Secondary (Deagle) Switching verified for AWP, AK-47, and M4A1 bots.\n");
  }

  // 8. Verify all 10 CT Strategies and 10 T Strategies across multi-round adaptation
  std::vector<bool> ctAwps = { false, false, true, false, false, false, false, false, false, false };
  std::vector<bool> tAwps = { false, false, true, false, false, false, false, false, false, false };
  std::set<CTAdvancedStrategy> seenCtStrategies;
  std::set<TOffensiveStrategy> seenTStrategies;
  std::set<TacticalDecision8> seenDecisions;
  for (uint32_t r = 1; r <= 40; ++r) {
    eng.startNewRound (r, seed, 0.8f);
    seenCtStrategies.insert (eng.activeCtStrategy ());
    seenTStrategies.insert (eng.activeTStrategy ());
    eng.assignTeamRoster (1u, ctSlots, ctAwps, ctC4s, seed);
    eng.assignTeamRoster (0u, tSlots, tAwps, tC4s, seed);

    for (int s : ctSlots) {
      TacticalBotInput in {};
      in.slot = s;
      in.team = 1u;
      in.origin = { -380.0f, 2120.0f, -96.0f };
      in.health = (s == 9 && r % 5 == 0) ? 20 : 100;
      in.seeingEnemy = (s == 9 && r % 5 == 0);
      in.hasAwp = (s == 2);
      in.elapsedRoundTime = (r % 2 == 0) ? 12.0f : 42.0f;
      auto dir = eng.evaluateBot (in, static_cast<float> (r * 10), seed);
      seenDecisions.insert (dir.decision);
      assert (!dir.targetOrigin.empty ());
    }
    for (int s : tSlots) {
      TacticalBotInput in {};
      in.slot = s;
      in.team = 0u;
      in.origin = { -480.0f, -780.0f, 100.0f };
      in.health = 100;
      in.hasAwp = (s == 12);
      in.hasC4 = (s == 10);
      in.elapsedRoundTime = (r % 2 == 0) ? 14.0f : 40.0f;
      auto dir = eng.evaluateBot (in, static_cast<float> (r * 10), seed);
      seenDecisions.insert (dir.decision);
      assert (!dir.targetOrigin.empty ());
    }
    eng.recordRoundResult (r % 2u, static_cast<uint8_t> (r % 3u));
  }

  // Trigger legitimate C4 alert & bomb plant to verify ROTATE and RETAKE decisions
  eng.reportLegitimateContact (1u, { -1560.0f, 2480.0f, 32.0f }, true, 500.0f);
  for (int s : ctSlots) {
    TacticalBotInput in {};
    in.slot = s;
    in.team = 1u;
    in.origin = (s < 5) ? Vec { 1120.0f, 2420.0f, 96.0f } : Vec { -1500.0f, 2300.0f, 32.0f };
    in.c4SpottedOrHeard = true;
    in.knownBombOrigin = { -1560.0f, 2480.0f, 32.0f };
    auto dir = eng.evaluateBot (in, 501.0f, seed);
    seenDecisions.insert (dir.decision);
    in.bombPlanted = true;
    auto dirPlanted = eng.evaluateBot (in, 502.0f, seed);
    seenDecisions.insert (dirPlanted.decision);
    assert (dirPlanted.decision == TacticalDecision8::RETAKE);
  }

  assert (seenCtStrategies.size () == 10u);
  assert (seenTStrategies.size () == 10u);
  assert (seenDecisions.size () == 8u);
  std::printf ("[PASS] All 10 CT Strategies, 10 T Strategies, and 8 Dynamic Decisions (ATTACK, DEFEND, PUSH, FLANK, ROTATE, RETREAT, HOLD, RETAKE) verified!\n");
  return 0;
}
