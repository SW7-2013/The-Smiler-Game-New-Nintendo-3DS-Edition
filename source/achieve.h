// Achievements: the original game's list, minus the ones that need the AR scanner, the website, sharing or the prize wheel.
#pragma once
typedef struct { const char* name; const char* desc; float target; int tex; } AchDef;
static const AchDef ACH[N_ACH] = {
	{ "Junior Mechanic",     "Upgrade a ride segment",              1,   0 },
	{ "Marmaliser Stage One", "Upgrade a leg on the Marmaliser",    1,   0 },
	{ "Marmaliser Stage Five", "Fully upgrade all Marmaliser legs", 5,   0 },
	{ "Pro Mechanic",        "Upgrade all ride segments",           10,  0 },
	{ "Tilt Trainee",        "Only get OK or better around the ride", 1, 0 },
	{ "Tilt Master",         "Only get Good or better around the ride", 2, 0 },
	{ "My First Smile",      "Play the game for the first time",    1,   0 },
	{ "Super Marmaliser",    "Marmalise 100% of the riders",        1,   0 },
	{ "Repetetron",          "Play the game 100 times",             100, 0 },
	{ "Sisyphean",           "Play the game 500 times",             500, 0 },
	{ "Happy Hoarder",       "Hoard 700 credits",                   700, 0 },
	{ "Rail Rush",           "Use the boost",                       1,   0 },
	{ "Throttle Thriller",   "Boost for 14 seconds in one ride",    14,  0 },
	{ "Round The Twist",     "Get PERFECT around a loop",           1,   0 },
};
static int achIcon(int i) {
	static const int t[N_ACH] = { T_trophy_05_JuniorMechanic256, T_trophy_06_PimpmySpyder256, T_trophy_07_FullyPimpedSpyder256, T_trophy_08_ProMechanic256,
		T_trophy_09_TiltTrainee256, T_trophy_10_TiltMaster256, T_trophy_11_WelcometoJoy256, T_trophy_12_MarmalisationComplete256, T_trophy_13_Repetetron256,
		T_trophy_14_Sisyphean256, T_trophy_17_HappyHoarder256, T_trophy_18_RailRush256, T_trophy_19_ThrottleThriller256, T_trophy_20_SmileAlways256 };
	return t[i];
}
static int achQueue[N_ACH + 2], achQN = 0; static int achShowing = -1; static float achShowT = 0;

static int achCount(void) { int n = 0; for (int i = 0; i < N_ACH; i++) n += achEarned[i] ? 1 : 0; return n; }
static void achCheck(int id) {
	if (!achEarned[id] && achProg[id] >= ACH[id].target) {
		achEarned[id] = 1; achProg[id] = ACH[id].target;
		if (achQN < N_ACH) achQueue[achQN++] = id;
	}
}
static void achAdd(int id, float v) { if (!achEarned[id]) { achProg[id] += v; achCheck(id); } }
static void achSet(int id, float v) { if (!achEarned[id]) { achProg[id] = v; achCheck(id); } }

// call every frame: starts the next "achievement unlocked" banner when the last one is done
static void achUpdate(float dt) {
	if (achShowing >= 0) { achShowT += dt; if (achShowT > 4.2f) achShowing = -1; }
	if (achShowing < 0 && achQN > 0) {
		achShowing = achQueue[0]; for (int i = 1; i < achQN; i++) achQueue[i - 1] = achQueue[i]; achQN--;
		achShowT = 0; if (sfxEnabled) sfxOnce(SFX_ACHIEVE, 0.8f);
	}
}
