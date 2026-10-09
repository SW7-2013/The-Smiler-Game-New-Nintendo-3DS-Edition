// The menu's "creepy frames": now and then the menu screens flash a static burst, a creepy picture, and another burst.
// Port of the original CreepFrame script (Main scene: MinWait 6, MaxWait 25, MinFrameTime 0.05, TransitionTime 0.25).
#pragma once
#define CREEP_MIN_WAIT   6.0f
#define CREEP_MAX_WAIT   25.0f
#define CREEP_FRAME_TIME 0.05f
#define CREEP_TRANSITION 0.25f
static const int CREEP_PICS[6] = { T_smile, T_joy, T_kitchener, T_mona, T_xray, T_obedience };
static const int CREEP_STATIC[3] = { T_smiler_static1, T_smiler_static2, T_smiler_static3 };

static u32 creepRng = 0x2545F491u;
static float creepRand(void) { creepRng ^= creepRng << 13; creepRng ^= creepRng >> 17; creepRng ^= creepRng << 5; return (float)(creepRng & 0xFFFFFF) / 16777216.0f; }

static int creepSeq[8], creepN = 0, creepI = 0, creepPic = 0;   // 1 = static burst, 2 = creepy picture
static float creepSlotT = 0, creepWait = 0;
static bool creepActive = false, creepFirstLoad = true, creepNoiseOn = false;

static void creepSet(bool noise) { if (noise != creepNoiseOn) { creepNoiseOn = noise; if (noise) sfxOnce(SFX_NOISE, 0.6f); else chStop(CH_NOISE); } }
static void creepStartSeq(const int* s, int n) {
	for (int i = 0; i < n; i++) creepSeq[i] = s[i];
	creepN = n; creepI = 0; creepSlotT = CREEP_FRAME_TIME; creepPic = (int)(creepRand() * 6) % 6;
	creepSet(creepSeq[0] == 1);
}
// called when a menu screen opens; the first time the menu loads there is no transition flicker
static void creepEnterMenu(void) {
	creepActive = true; creepN = 0; creepWait = CREEP_MIN_WAIT + creepRand() * (CREEP_MAX_WAIT - CREEP_MIN_WAIT);
	if (!creepFirstLoad) {
		int s[8], n = (int)(CREEP_TRANSITION / CREEP_FRAME_TIME + 0.5f);
		for (int i = 0; i < n; i++) s[i] = creepRand() > 0.5f ? 1 : 2;
		creepStartSeq(s, n);
	}
	creepFirstLoad = false;
}
static void creepLeaveMenu(void) { creepActive = false; creepN = 0; creepSet(false); }
static void creepUpdate(float dt) {
	if (!creepActive) return;
	if (creepN > 0) {
		creepSlotT -= dt;
		while (creepN > 0 && creepSlotT <= 0) {
			creepSlotT += CREEP_FRAME_TIME; creepI++;
			if (creepI >= creepN) { creepN = 0; creepSet(false); break; }
			creepPic = (int)(creepRand() * 6) % 6; creepSet(creepSeq[creepI] == 1);
		}
	} else {
		creepWait -= dt;
		if (creepWait <= 0) {
			static const int loop[3] = { 1, 2, 1 };
			creepStartSeq(loop, 3);
			creepWait = CREEP_MIN_WAIT + creepRand() * (CREEP_MAX_WAIT - CREEP_MIN_WAIT);
		}
	}
}
static void creepDraw(float w, float h) {
	if (!creepActive || creepN == 0) return;
	uiWhite(1);
	int t = creepSeq[creepI] == 1 ? CREEP_STATIC[(int)(creepRand() * 3) % 3] : CREEP_PICS[creepPic];
	uiRect(t, 0, 0, w, h); uiWhite(1);
}
