// All the screens (splash, intro video, menus, options, pause, HUD, results, upgrade shop) and the main loop. Included by main.c.
#pragma once

enum { SC_SPLASH, SC_INTRO, SC_MENU, SC_FACTS, SC_OPTIONS, SC_TIPS, SC_STATS, SC_QUIT, SC_RIDE, SC_PAUSE, SC_SHOP, SC_ERROR, SC_TUT };
static int scene = SC_SPLASH, nextScene = -1, optionsBack = SC_MENU;
static float sceneT = 0, fade = 1.0f, uiClock = 0;
static bool wantQuit = false, pauseConfirm = false, launchReq = false, touchBoost = false;
static int shopBack = SC_MENU, tutLastSm = 0;
static float physAcc = 0;

#define YEL   1.00f, 0.93f, 0.12f
#define NAVY  0.13f, 0.06f, 0.30f
#define PINK  1.00f, 0.93f, 0.12f   // (the old pink text is yellow now)

// ---------------------------------------------------------------- the intro video
static FILE* vidF = NULL; static u32 vidN = 0, vidFps = 15, vidW = 256, vidH = 192, vidFrame = 0xFFFFFFFFu; static float vidT = 0;

static bool vidOpen(void) {
	vidF = fopen("romfs:/intro.vid", "rb"); if (!vidF) return false;
	char m[4]; u32 h[4];
	if (fread(m, 1, 4, vidF) != 4 || memcmp(m, "VID1", 4) || fread(h, 4, 4, vidF) != 4 || h[2] != 256 || h[0] == 0) { fclose(vidF); vidF = NULL; return false; }
	vidN = h[0]; vidFps = h[1]; vidW = h[2]; vidH = h[3];
	if (!vidTexOk) {
		if (!C3D_TexInit(&vidTex, 256, 256, GPU_RGB565)) { fclose(vidF); vidF = NULL; return false; }
		C3D_TexSetFilter(&vidTex, GPU_LINEAR, GPU_LINEAR); C3D_TexSetWrap(&vidTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
		memset(vidTex.data, 0, 256 * 256 * 2); vidTexOk = true;
	}
	return true;
}
static void vidShow(u32 fr) {
	size_t bytes = (size_t)vidW * vidH * 2;
	fseek(vidF, 20 + (long)fr * (long)bytes, SEEK_SET);
	if (fread(vidTex.data, 1, bytes, vidF) == bytes) { GSPGPU_FlushDataCache(vidTex.data, (u32)bytes); vidFrame = fr; }
}
static void vidClose(void) { if (vidF) { fclose(vidF); vidF = NULL; } }

// ---------------------------------------------------------------- scene changes
static void sceneGo(int s) { if (nextScene < 0) nextScene = s; }
static bool modelFull = false;
static void sceneEnter(int s) {
	if (modelFull && s != SC_FACTS) { applyUpgrades(); modelFull = false; }   // back to the player's own track
	if (s == SC_FACTS && !modelFull) {   // the fact-sheet model always shows the track with every upgrade
		for (int k = 0; k < nSecDefs; k++) variantSel[k + 1] = secDefs[k].nVar > 1 ? 1 : 0;
		buildTrack(); modelFull = true;
	}
	bool wasMenuScene = creepActive; bool isMenuScene = (s == SC_MENU || s == SC_FACTS || s == SC_OPTIONS || s == SC_TIPS || s == SC_STATS || s == SC_QUIT);
	if (isMenuScene && !wasMenuScene) creepEnterMenu(); else if (!isMenuScene && wasMenuScene) creepLeaveMenu();
	scene = s; sceneT = 0; pauseConfirm = false; shopCutT = -1;
	droneStop(); if (s == SC_RIDE && G.state == ST_READY && readyTrack >= 0) droneStart(readyTrack);
	switch (s) {
	case SC_INTRO:
		vidT = 0; vidFrame = 0xFFFFFFFFu;
		if (vidOpen()) { vidShow(0); musicPlay("intro", false); } else sceneEnter(SC_MENU);
		break;
	case SC_RIDE: musicPlay("ingame_loop", true); break;
	case SC_MENU: introPlayed = false; musicPlay("menu_loop", true); break;
	case SC_PAUSE: break;
	case SC_TUT: break;
	case SC_SPLASH: break;
	default: if (s != SC_ERROR) musicPlay("menu_loop", true); break;
	}
}
static void startRide(void) {
	float top = (float)savedTopScore;
	applyUpgrades(); resetGame(); G.topScore = top; physAcc = 0; launchReq = false; readyStart(); armReset(); rideDronesReset(); tunReset(); tutLastSm = 0;
	achSet(A_THROTTLE, 0);
	sceneGo(SC_RIDE);
}

// ---------------------------------------------------------------- small widgets
static void bgPoly(float w) { uiWhite(1); uiRect(T_gui_bg_poly, 0, 0, w, 240); }
static void dimScreen(float w, float a) { uiColor(0, 0, 0, a); uiFill(0, 0, w, 240); uiWhite(1); }
static void shadowText(int f, const char* s, float x, float cy, float sc, int al, float r, float g, float b) {
	uiColor(r, g, b, 1); uiTextS(f, s, x, cy, sc, al); uiWhite(1);
}
static bool uiToggle2(float cx, float cy, float sc, bool on, const char* a, const char* b) {   // a = left (off) label, b = right (on) label
	float w = uiSW(T_gui_toggle_2_left_options) * sc, h = uiSH(T_gui_toggle_2_left_options) * sc;
	bool hit = uiHit(cx - w * 0.5f, cy - h * 0.5f, w, h, NULL);
	uiRect(on ? T_gui_toggle_2_right_options : T_gui_toggle_2_left_options, cx - w * 0.5f, cy - h * 0.5f, w, h);
	float ts = h / 71.0f * 1.05f;
	if (!on) uiColor(NAVY, 1); else uiColor(0.95f, 0.95f, 0.9f, 1);
	uiText(F_SMALL, a, cx - w * 0.27f, cy, ts, 1);
	if (on) uiColor(NAVY, 1); else uiColor(0.95f, 0.95f, 0.9f, 1);
	uiText(F_SMALL, b, cx + w * 0.27f, cy, ts, 1);
	uiWhite(1);
	return hit ? !on : on;
}
static int uiToggle3(float cx, float cy, float sc, int idx, const char* l0, const char* l1, const char* l2) {
	float w = uiSW(T_gui_toggle_3_bg_options) * sc, h = uiSH(T_gui_toggle_3_bg_options) * sc;
	float x = cx - w * 0.5f, y = cy - h * 0.5f;
	uiRect(T_gui_toggle_3_bg_options, x, y, w, h);
	uiRect(idx == 0 ? T_gui_toggle_3_left_option : (idx == 1 ? T_gui_toggle_3_mid_option : T_gui_toggle_3_right_option), x, y, w, h);
	const char* lb[3] = { l0, l1, l2 }; float ts = h / 70.0f * 0.92f;
	int res = idx;
	for (int i = 0; i < 3; i++) {
		float tx = x + w * (0.17f + 0.33f * i);
		if (uiHit(x + w * 0.33f * i, y, w * 0.34f, h, NULL)) res = i;
		if (i == idx) uiColor(NAVY, 1); else uiColor(0.95f, 0.95f, 0.9f, 1);
		uiText(F_SMALL, lb[i], tx, cy, ts, 1);
	}
	uiWhite(1);
	return res;
}

// ---------------------------------------------------------------- splash / intro
static void drawSplashTop(void) {
	uiColor(0, 0, 0, 1); uiFill(0, 0, 400, 240);
	float a = fminf(1.0f, sceneT / 0.6f) * fminf(1.0f, (3.8f - sceneT) / 0.5f);
	uiColor(1, 1, 1, fclamp(a, 0, 1)); uiRect(T_splash_art, 13.5f, 0, 373, 240); uiWhite(1);
}
static void drawSplashBottom(void) {
	uiColor(0, 0, 0, 1); uiFill(0, 0, 320, 240);
	float a = fclamp(sceneT / 0.8f, 0, 1);
	uiColor(1, 1, 1, a); uiSprC(T_gui_exclamation_icon, 160, 62, 0.34f);
	uiColor(1, 1, 1, a);
	uiText(F_SMALL, "PHOTOSENSITIVITY WARNING", 160, 112, 0.58f, 1);
	uiText(F_SMALL, "The intro contains flashing images.", 160, 134, 0.44f, 1);
	uiColor(0.8f, 0.8f, 0.8f, a);
	uiText(F_SMALL, "A fan-made port of The Smiler mobile game.", 160, 176, 0.38f, 1);
	uiText(F_SMALL, "Not affiliated with Alton Towers Resort.", 160, 192, 0.38f, 1);
	uiColor(YEL, a * (0.6f + 0.4f * sinf(uiClock * 4)));
	uiText(F_SMALL, "Tap or press A to continue", 160, 222, 0.42f, 1);
	uiWhite(1);
}
static void drawIntroTop(void) {
	uiColor(0, 0, 0, 1); uiFill(0, 0, 400, 240); uiWhite(1);
	uiRectUV(TEX_VIDEO, 40, 0, 320, 240, 0, 1.0f, 1, 0.0f);   // the frame fills all 256 rows of the texture
}
static bool introSkip = false;
static void drawIntroBottom(void) {
	uiColor(0, 0, 0, 1); uiFill(0, 0, 320, 240); uiWhite(1);
	if (uiSprBtn(T_gui_button_skip, 160, 150, 0.7f, NULL, F_SMALL, 1)) introSkip = true;
	uiColor(0.7f, 0.7f, 0.7f, 1); uiText(F_SMALL, "Tap to skip", 160, 214, 0.42f, 1); uiWhite(1);
}

// ---------------------------------------------------------------- main menu
static float faceAng = 0, accBaseX = 0;
static void updateFace(float dt) {   // the face only tilts (rolls) when the console is tilted left/right
	accelVector av; hidAccelRead(&av);
	accBaseX += ((float)av.x - accBaseX) * fminf(1.0f, dt * 0.6f);
	float tgt = fclamp(((float)av.x - accBaseX) / 200.0f, -1, 1) * -0.6f;
	faceAng += (tgt - faceAng) * fminf(1.0f, dt * 8.0f);
}
static void drawMenuTop(void) {
	bgPoly(400);
	float bob = 1.0f;
	{ float fw = uiSW(T_gui_smiler_logo) * 0.38f * bob, fh = uiSH(T_gui_smiler_logo) * 0.38f * bob;
	  uiRectRot(T_gui_smiler_logo, 300 - fw * 0.5f, 120 - fh * 0.5f, fw, fh, faceAng, 300, 120); }
	uiSprC(T_gui_thesmiler_logo, 98, 84, 0.64f);
	uiColor(0.85f, 0.85f, 0.7f, 1); uiText(F_SMALL, "NEW NINTENDO 3DS EDITION", 98, 136, 0.36f, 1);
	uiColor(0.7f, 0.7f, 0.55f, 1); uiText(F_SMALL, "V" GAME_VERSION, 390, 228, 0.42f, 2);   // bottom-right corner, under the face
	char b[32], t[40]; uiNum(b, savedTopScore); snprintf(t, sizeof t, "%s", b);
	uiColor(1, 1, 1, 0.75f); uiText(F_SMALL, "TOP SCORE", 22, 192, 0.45f, 0);
	uiColor(YEL, 1); uiText(F_LCD, t, 22, 216, 0.85f, 0); uiWhite(1);
}
static void drawMenuBottom(u32 down) {
	bgPoly(320);
	if (uiSprBtn(T_gui_ride_button, 160, 60, 0.72f, NULL, F_SMALL, 1) || (down & KEY_A)) startRide();
	if (uiSprBtn(T_gui_poly_smilerfacts_main_menu, 100, 142, 0.56f, NULL, F_SMALL, 1) || (down & KEY_Y)) sceneGo(SC_FACTS);
	if (uiSprBtn(T_gui_credits, 244, 140, 0.44f, NULL, F_SMALL, 1) || (down & KEY_X)) { shopBack = SC_MENU; sceneGo(SC_SHOP); }
	char b[16]; snprintf(b, sizeof b, "%d", credits);
	uiColor(YEL, 1); uiText(F_SMALL, "UPGRADES", 244, 184, 0.42f, 1);
	uiColor(1, 1, 1, 1); uiText(F_LCD, b, 244, 172, 0.42f, 1);
	uiWhite(1);
	if (uiSprBtn(T_gui_settings_icon, 52, 213, 0.5f, NULL, F_SMALL, 1) || (down & KEY_START)) { optionsBack = SC_MENU; sceneGo(SC_OPTIONS); }
	if (uiSprBtn(T_gui_trophies_icon, 124, 213, 0.5f, NULL, F_SMALL, 1)) sceneGo(SC_STATS);
	if (uiSprBtn(T_gui_tips_icon, 196, 213, 0.5f, NULL, F_SMALL, 1)) sceneGo(SC_TIPS);
	if (uiSprBtn(T_gui_quit_icon, 268, 213, 0.5f, NULL, F_SMALL, 1)) sceneGo(SC_QUIT);
}

// ---------------------------------------------------------------- options (also used inside the pause screen)
static int sensIdx(void) { return sensitivity < 0.4f ? 0 : (sensitivity < 0.65f ? 1 : 2); }
static int speedIdx(void) { return gravScale < 0.9f ? 0 : (gravScale < 1.15f ? 1 : 2); }
static void drawOptionsTop(const char* title) {
	bgPoly(400);
	uiColor(1, 1, 1, 0.9f); uiSprC(T_gui_smiler_logo, 320, 150, 0.36f); uiWhite(1);
	shadowText(F_TITLE, title, 24, 40, 0.8f, 0, 1, 1, 1);
	uiColor(0.85f, 0.85f, 0.7f, 1);
	uiText(F_SMALL, "Circle Pad: lean    A / L / R: boost", 24, 150, 0.42f, 0);
	uiText(F_SMALL, "START: pause       B: back", 24, 170, 0.42f, 0);
	uiColor(0.7f, 0.7f, 0.55f, 1); uiText(F_SMALL, "Created by bruhreversed", 24, 200, 0.42f, 0);
	uiWhite(1);
}
static int optConfirm = 0;   // 0 none, 1 replay the tutorials?, 2 clear all progress?
static void drawOptionsBottom(u32 down) {
	bgPoly(320);
	if (optConfirm) {
		uiColor(1, 1, 1, 1);
		if (optConfirm == 1) { uiText(F_SMALL, "Switch the tutorials back on?", 160, 56, 0.55f, 1); uiText(F_SMALL, "They will show again as you ride.", 160, 86, 0.42f, 1); }
		else { uiText(F_SMALL, "Clear all your progress?", 160, 50, 0.6f, 1); uiText(F_SMALL, "Credits, upgrades, trophies and your", 160, 82, 0.42f, 1); uiText(F_SMALL, "top score will be deleted.", 160, 100, 0.42f, 1); }
		uiWhite(1);
		if (uiSprBtn(T_gui_yes_button, 90, 160, 0.45f, NULL, F_SMALL, 1) || (down & KEY_A)) {
			if (optConfirm == 1) { tutSeen = 0; tutEnabled = true; saveGame(); }
			else { clearMemory(); applyUpgrades(); G.topScore = 0; }
			optConfirm = 0;
		}
		if (uiSprBtn(T_gui_no_button, 230, 160, 0.45f, NULL, F_SMALL, 1) || (down & KEY_B)) optConfirm = 0;
		return;
	}
	static const float SENS[3] = { 0.30f, 0.50f, 0.80f }, SPD[3] = { 0.8f, 1.0f, 1.3f };
	bool changed = false;
	uiColor(1, 1, 1, 1);
	uiText(F_SMALL, "MUSIC", 16, 16, 0.5f, 0);
	bool m = uiToggle2(252, 16, 0.46f, musicEnabled, "OFF", "ON");
	if (m != musicEnabled) { musicEnabled = m; musicApply(); changed = true; }
	uiColor(1, 1, 1, 1);
	uiText(F_SMALL, "SOUND FX", 16, 46, 0.5f, 0);
	bool sf = uiToggle2(252, 46, 0.46f, sfxEnabled, "OFF", "ON");
	if (sf != sfxEnabled) { sfxEnabled = sf; changed = true; }
	uiColor(1, 1, 1, 1);
	uiText(F_SMALL, "LEAN SENSITIVITY", 16, 76, 0.42f, 0);
	int si = uiToggle3(252, 76, 0.38f, sensIdx(), "LOW", "MED", "HIGH");
	if (si != sensIdx()) { sensitivity = SENS[si]; changed = true; }
	uiColor(1, 1, 1, 1);
	uiText(F_SMALL, "RIDE SPEED", 16, 106, 0.5f, 0);
	int gi = uiToggle3(252, 106, 0.38f, speedIdx(), "SLOW", "NORM", "FAST");
	if (gi != speedIdx()) { gravScale = SPD[gi]; changed = true; }
	uiColor(1, 1, 1, 1);
	uiText(F_SMALL, "LEAN DIRECTION", 16, 136, 0.42f, 0);
	bool inv = uiToggle2(252, 136, 0.46f, stickSign < 0, "NORM", "FLIP");
	if (inv != (stickSign < 0)) { stickSign = inv ? -1.0f : 1.0f; changed = true; }
	uiColor(1, 1, 1, 1);
	uiText(F_SMALL, "TUTORIALS", 16, 166, 0.5f, 0);
	bool tu = uiToggle2(252, 166, 0.46f, tutEnabled, "OFF", "ON");
	if (tu != tutEnabled) { tutEnabled = tu; changed = true; }
	uiWhite(1);
	if (changed) saveGame();
	if (uiSprBtn(T_gui_poly_popup_1, 56, 212, 0.36f, "REPLAY TUTS", F_SMALL, 0.36f)) optConfirm = 1;
	if (uiSprBtn(T_gui_poly_popup_1, 160, 212, 0.36f, "CLEAR DATA", F_SMALL, 0.36f)) optConfirm = 2;
	if (uiSprBtn(T_gui_poly_popup_1, 264, 212, 0.36f, "BACK", F_SMALL, 0.45f) || (down & KEY_B)) sceneGo(optionsBack);
}

// ---------------------------------------------------------------- the ride facts (blueprint) screen
static void drawGradient(float w) {   // dark olive fading to black, like the menu background
	for (int i = 0; i < 24; i++) { float k = (float)i / 23.0f; uiColor(0.20f * (1 - k), 0.20f * (1 - k), 0.05f * (1 - k), 1); uiFill(0, i * 10.0f, w, 10.0f); }
	uiWhite(1);
}
static void drawFactsTop(void) {   // the 3D track model is drawn behind this
	uiSpr(T_BP_title, 400 - 256 * 1.0f - 8, 8, 1.0f);
	shadowText(F_TITLE, "THE RIDE", 14, 214, 0.7f, 0, 1, 1, 1);
	uiColor(1, 1, 1, 0.85f); uiText(F_SMALL, "Drag the bottom screen or use the Circle Pad to look around", 14, 188, 0.36f, 0); uiWhite(1);
}
static void drawFactsBottom(u32 down) {
	drawGradient(320);
	static const int cards[7] = { T_BP_callout1, T_BP_callout2, T_BP_callout3, T_BP_callout4, T_BP_callout5, T_BP_callout6, T_BP_callout7 };
	for (int i = 0; i < 7; i++) {
		float cx = 82 + (i % 2) * 158, cy = 36 + (i / 2) * 58;
		uiSprC(cards[i], cx, cy, 0.62f);
	}
	if (uiSprBtn(T_gui_button_continue, 240, 210, 0.36f, NULL, F_SMALL, 1) || (down & KEY_B)) sceneGo(SC_MENU);
}

// ---------------------------------------------------------------- tips / stats / quit
static void drawTipsTop(void) {
	bgPoly(400);
	shadowText(F_TITLE, "HOW TO RIDE", 24, 34, 0.75f, 0, 1, 1, 1);
	static const char* tips[5] = {
		"Lean the Circle Pad the way the track bends. Keep the pin on the dial in the middle.",
		"Leaning perfectly scores the most points and smiles your riders.",
		"Every smile earns boost time. Hold A, L, R or the boost button to use it.",
		"Spend credits in UPGRADES to add loops to the track.",
		"Press START to pause. Lean too far and you slow right down!" };
	for (int i = 0; i < 5; i++) {
		uiColor(YEL, 1); uiText(F_SMALL, "\x2a", 24, 82 + i * 30, 0.5f, 0);
		uiColor(1, 1, 1, 1); uiWrap(F_SMALL, tips[i], 40, 82 + i * 30, 340, 0.43f, 14, 0);
	}
	uiWhite(1);
}
static float achScroll = 0;
static void drawStatsTop(void) {
	bgPoly(400);
	shadowText(F_TITLE, "STATS", 24, 34, 0.75f, 0, 1, 1, 1);
	char b[48]; int owned = 0;
	for (int i = 0; i < N_UPGRADES; i++) if (upLevel[i] > 0) owned++;
	struct { const char* l; char v[32]; } rows[5];
	rows[0].l = "TOP SCORE"; uiNum(rows[0].v, savedTopScore);
	rows[1].l = "RIDES PLAYED"; snprintf(rows[1].v, 32, "%d", ridesPlayed);
	rows[2].l = "CREDITS"; snprintf(rows[2].v, 32, "%d", credits);
	rows[3].l = "UPGRADES OWNED"; snprintf(rows[3].v, 32, "%d / %d", owned, N_UPGRADES);
	rows[4].l = "ACHIEVEMENTS UNLOCKED"; snprintf(rows[4].v, 32, "%d / %d", achCount(), N_ACH);
	for (int i = 0; i < 5; i++) {
		float y = 74 + i * 32;
		uiColor(0.85f, 0.85f, 0.7f, 1); uiText(F_SMALL, rows[i].l, 28, y, 0.5f, 0);
		uiColor(YEL, 1); uiText(F_LCD, rows[i].v, 372, y, 0.8f, 2);
		uiColor(1, 1, 1, 0.15f); uiFill(24, y + 14, 352, 1);
	}
	(void)b; uiWhite(1);
}
static void drawAchBottom(u32 down) {
	drawGradient(320);
	const float RH = 62.0f;
	for (int i = 0; i < N_ACH; i++) {
		float y = 6 + i * RH - achScroll;
		if (y > 196 || y < -RH) continue;
		bool got = achEarned[i] != 0;
		uiColor(1, 1, 1, got ? 0.14f : 0.07f); uiFill(6, y, 308, RH - 6);
		float k = got ? 1.0f : 0.28f; uiColor(k, k, k, 1); uiRect(achIcon(i), 10, y + 3, 50, 50); uiWhite(1);
		if (got) uiColor(YEL, 1); else uiColor(0.7f, 0.7f, 0.65f, 1);
		uiText(F_SMALL, ACH[i].name, 68, y + 13, 0.5f, 0);
		uiColor(1, 1, 1, got ? 0.9f : 0.55f); uiWrap(F_SMALL, ACH[i].desc, 68, y + 30, 240, 0.36f, 12, 0);
		if (!got) {
			char t[32]; snprintf(t, sizeof t, "%d / %d", (int)achProg[i], (int)ACH[i].target);
			uiColor(1, 1, 1, 0.5f); uiText(F_SMALL, t, 308, y + 13, 0.4f, 2);
			uiColor(1, 1, 1, 0.15f); uiFill(68, y + 42, 236, 4); uiColor(YEL, 0.8f); uiFill(68, y + 42, 236 * fclamp(achProg[i] / ACH[i].target, 0, 1), 4);
		} else { uiColor(YEL, 1); uiText(F_SMALL, "UNLOCKED", 308, y + 13, 0.4f, 2); }
		uiWhite(1);
	}
	for (int i = 0; i < 24; i++) if (i * 10 + 10 > 196) { float kk = (float)i / 23.0f; uiColor(0.20f * (1 - kk), 0.20f * (1 - kk), 0.05f * (1 - kk), 1); uiFill(0, i * 10.0f, 320, 10.0f); }   // covers rows scrolled under the button bar
	uiWhite(1);
	if (uiSprBtn(T_gui_button_continue, 160, 220, 0.3f, NULL, F_SMALL, 1) || (down & (KEY_B | KEY_A))) sceneGo(SC_MENU);
}
static void drawAchPopup(void) {   // banner that slides down from the top of the upper screen
	if (achShowing < 0) return;
	float t = achShowT, off = t < 0.4f ? (1 - t / 0.4f) : (t > 3.8f ? (t - 3.8f) / 0.4f : 0.0f);
	float y = 6 - off * 100, sc = 0.78f;
	uiWhite(1); uiSprC(T_gui_trophy_popup_black, 200, y + 101 * sc * 0.5f, sc);
	uiRect(achIcon(achShowing), 56, y + 8, 56, 56);
	uiColor(YEL, 1); uiText(F_SMALL, "ACHIEVEMENT UNLOCKED", 124, y + 24, 0.4f, 0);
	uiColor(1, 1, 1, 1); uiText(F_SMALL, ACH[achShowing].name, 124, y + 48, 0.58f, 0);
	uiWhite(1);
}
static void drawBackBottom(u32 down) {
	bgPoly(320);
	if (uiSprBtn(T_gui_button_continue, 160, 120, 0.7f, NULL, F_SMALL, 1) || (down & (KEY_B | KEY_A))) sceneGo(SC_MENU);
}
static void drawQuitTop(void) {
	bgPoly(400); uiSprC(T_gui_smiler_logo, 200, 120, 0.5f);
	dimScreen(400, 0.4f);
	shadowText(F_TITLE, "LEAVING ALREADY?", 200, 112, 0.8f, 1, 1, 1, 1);
}
static void drawQuitBottom(u32 down) {
	bgPoly(320);
	uiColor(1, 1, 1, 1); uiText(F_SMALL, "Quit the game?", 160, 40, 0.7f, 1); uiWhite(1);
	if (uiSprBtn(T_gui_yes_button, 90, 130, 0.45f, NULL, F_SMALL, 1) || (down & KEY_A)) wantQuit = true;
	if (uiSprBtn(T_gui_no_button, 230, 130, 0.45f, NULL, F_SMALL, 1) || (down & KEY_B)) sceneGo(SC_MENU);
	uiColor(1, 1, 1, 0.8f); uiText(F_SMALL, "A: yes     B: no", 160, 214, 0.42f, 1); uiWhite(1);
}

// ---------------------------------------------------------------- HUD
static float marmProgress(void) { return fclamp(G.score / FULL_MARMALIZE, 0, 1); }
// the smiley is one rider: it fills up, reaches 1, empties again for the next rider...
static void drawMarm(float x, float y, float size) {
	float n = G.score / FULL_MARMALIZE * RIDERS, fr = n - floorf(n);
	if (G.smiles >= RIDERS) fr = 1.0f;
	int f = (int)(fr * 15.0f + 0.5f);
	uiCell(T_gui_marmaliseFillUp, x, y, size, size, f % 4, f / 4, 4, 4);
}
static int accTex(void) { return G.accIcon == 3 ? T_perfect_orbitron : (G.accIcon == 2 ? T_good_orbitron : (G.accIcon == 1 ? T_ok_orbitron : T_bad_orbitron)); }
static void accColor(float a) {   // bright yellow for PERFECT, darker and redder the worse it gets, red for BAD
	static const float c[4][3] = { { 1.00f, 0.12f, 0.10f }, { 0.78f, 0.42f, 0.04f }, { 0.92f, 0.70f, 0.06f }, { 1.00f, 0.93f, 0.12f } };
	uiColor(c[G.accIcon][0], c[G.accIcon][1], c[G.accIcon][2], a);
}

static void drawHudTop(void) {
	char b[32];
	if (!(scene == SC_RIDE && G.state == ST_READY)) tunDrawOverlay();
	fxDraw(); legDrawOverlays();
	if (G.state == ST_READY) {   // scenic view before the ride: no HUD, just a hint
		uiColor(0, 0, 0, 0.5f); uiFill(0, 208, 400, 32); uiColor(1, 1, 1, 1);
		uiTextS(F_SMALL, introActive ? "Welcome to The Smiler!" : "Lean with the track. Hold A / L / R to boost.", 200, 226, 0.42f, 1); uiWhite(1);
		return;
	}
	// score, top left (yellow panel, dark lettering)
	uiWhite(1); uiSpr(T_gui_scoreArea, 4, 3, 0.6f);
	uiColor(NAVY, 1); uiText(F_SMALL, "SCORE", 18, 18, 0.4f, 0);
	uiNum(b, (int)G.score); uiColor(NAVY, 1); uiText(F_LCD, b, 18, 40, 0.78f, 0);
	// the rider being marmalised, top centre
	uiWhite(1); drawMarm(168, 2, 64);
	snprintf(b, sizeof b, "%d/%d", G.smiles, RIDERS); uiColor(1, 1, 1, 1); uiTextS(F_LCD, b, 200, 74, 0.5f, 1);
	// multiplier, top right
	uiWhite(1); uiSpr(T_gui_multiplier, 400 - 0.46f * 252 - 4, 1, 0.46f);
	snprintf(b, sizeof b, "x%d", (int)secs[G.section].mult); uiColor(YEL, 1); uiTextS(F_LCD, b, 400 - 0.46f * 126 - 4 + 4, 33, 1.0f, 1);
	uiWhite(1);
	if (G.state == ST_RUN && !G.runOver) {
		accColor(1); uiSprC(accTex(), 200, 218, 0.5f); uiWhite(1);
		float err = G.trueTilt; float ae = fabsf(err);
		if (ae > 0.06f) {   // arrows on the side you should lean towards
			int row = ae < 0.2f ? 0 : (ae < 0.45f ? 1 : 2);
			float pulse = 0.65f + 0.35f * sinf(uiClock * 12.0f);
			uiColor(1, 1, 1, pulse);
			float w = 162 * 0.38f, h = 170.67f * 0.38f;
			if (err < 0) uiRectUV(T_gui_balancePrompts, 400 - w - 14, 108, w, h, 0, 1.0f - row / 3.0f, 1, 1.0f - (row + 1) / 3.0f);
			else uiRectUV(T_gui_balancePrompts, 14, 108, w, h, 1, 1.0f - row / 3.0f, 0, 1.0f - (row + 1) / 3.0f);
			uiWhite(1);
		}
	}
}
static void drawHudBottom(u32 down) {
	char b[32]; bool pr;
	bgPoly(320);
	// pause button
	if (uiSprBtn(T_gui_pauseButton, 36, 28, 0.58f, NULL, F_SMALL, 1)) { launchReq = false; sceneGo(SC_PAUSE); }
	// speed panel
	uiSpr(T_gui_scoreArea, 80, 4, 0.62f);
	uiColor(NAVY, 1); uiText(F_SMALL, "SPEED", 96, 22, 0.4f, 0);
	snprintf(b, sizeof b, "%d", (int)G.speed); uiColor(NAVY, 1); uiText(F_LCD, b, 96, 42, 0.78f, 0);
	// the rider being marmalised
	uiWhite(1); drawMarm(6, 58, 66);
	snprintf(b, sizeof b, "%d/%d", G.smiles, RIDERS); uiColor(1, 1, 1, 1); uiTextS(F_LCD, b, 39, 132, 0.45f, 1); uiWhite(1);
	// boost bar (right edge) and button
	float frac = fclamp(G.boostCount / 10.0f, 0, 1);
	uiColor(1, 0.93f, 0.12f, 0.22f); uiFill(300, 118, 14, 112);
	uiColor(1, 0.93f, 0.12f, 1); uiFill(301, 119 + 110 * (1 - frac), 12, 110 * frac);
	uiColor(1, 0.93f, 0.12f, 1); uiRect(T_boostBarEmpty, 297, 112, 20, 124); uiWhite(1);
	int bt = G.boosting ? T_boostButtonBoosting : (G.boostCount > 0 ? T_boostButtonRed : T_boostButtonGrey);
	float bx = 262, by = 64, bs = 0.62f, bw = 160 * bs;
	bool hit = uiHit(bx - bw * 0.6f, by - bw * 0.6f, bw * 1.2f, bw * 1.2f, &pr); (void)hit;
	touchBoost = pr && G.state == ST_RUN;
	uiWhite(1); uiRect(bt, bx - bw * 0.5f * (pr ? 0.94f : 1), by - bw * 0.5f * (pr ? 0.94f : 1), bw * (pr ? 0.94f : 1), bw * (pr ? 0.94f : 1));
	snprintf(b, sizeof b, "%.1fs", G.boostCount > 0 ? G.boostCount : 0.0f);
	uiColor(YEL, 1); uiTextS(F_SMALL, "BOOST", bx, by + 52, 0.38f, 1); uiTextS(F_SMALL, b, bx, by + 66, 0.38f, 1);
	// lean dial: the pin shows how far off the track's lean you are; the middle is perfect
	uiWhite(1); uiRect(T_gui_BalanceMeter2, 32, 140, 256, 99);
	static float dialAng = 0, dialClk = 0;
	float want = fclamp((G.userTilt - G.targetTilt) / 0.45f, -1, 1) * 0.95f;
	if (G.state != ST_RUN || G.runOver) want = 0;
	float ddt = fclamp(uiClock - dialClk, 0, 0.1f); dialClk = uiClock;
	dialAng += (want - dialAng) * fminf(1.0f, ddt * 14.0f);
	float ang = dialAng;
	uiRectRot(T_balanceOmeterArrow, 160 - 20, 238 - 96 - 52, 40, 52, ang, 160, 238);
	snprintf(b, sizeof b, "x%d", (int)secs[G.section].mult); uiColor(YEL, 1); uiTextS(F_LCD, b, 160, 214, 0.7f, 1);
	if (G.state == ST_READY) {
		dimScreen(320, 0.35f);
		if (readyWait < READY_WAIT) { uiColor(1, 1, 1, 0.8f); uiTextS(F_SMALL, "Enjoy the view...", 160, 110, 0.5f, 1); uiWhite(1); }
		else if (introActive) { if (uiSprBtn(T_gui_poly_popup_1, 160, 110, 0.7f, "SKIP INTRO", F_SMALL, 0.55f) || (down & KEY_A)) { readyNext(); readyWait = READY_WAIT; } }
		else {
			if (uiSprBtn(T_gui_ride_button, 160, 100, 0.72f, NULL, F_SMALL, 1) || (down & KEY_A)) launchReq = true;
			uiColor(1, 1, 1, 1); uiTextS(F_SMALL, "TAP TO LAUNCH", 160, 178, 0.5f, 1); uiWhite(1);
		}
	}
}

// ---------------------------------------------------------------- results
static void drawResultsTop(void) {
	char b[64];
	dimScreen(400, 0.5f);
	shadowText(F_TITLE, G.dist >= trackLen - 1 ? "RIDE COMPLETE" : "RIDE OVER", 200, 30, 0.8f, 1, YEL);
	uiColor(1, 1, 1, 0.9f); uiText(F_SMALL, "SCORE", 200, 72, 0.5f, 1);
	uiNum(b, (int)G.score); uiColor(1, 1, 1, 1); uiTextS(F_LCD, b, 200, 104, 1.5f, 1);
	if (G.newBest) { uiColor(PINK, 0.7f + 0.3f * sinf(uiClock * 8)); uiTextS(F_SMALL, "NEW TOP SCORE!", 200, 138, 0.6f, 1); }
	else { uiNum(b, (int)G.topScore); char t[80]; snprintf(t, sizeof t, "TOP SCORE  %s", b); uiColor(1, 1, 1, 0.8f); uiText(F_SMALL, t, 200, 138, 0.5f, 1); }
	uiWhite(1); drawMarm(40, 150, 62);
	uiColor(1, 1, 1, 0.9f); uiText(F_SMALL, "SMILES", 114, 164, 0.5f, 0);
	snprintf(b, sizeof b, "%d/%d", G.smiles, RIDERS); uiColor(1, 1, 1, 1); uiTextS(F_LCD, b, 114, 192, 0.8f, 0);
	uiWhite(1); uiSprC(T_gui_credits, 262, 166, 0.34f);
	uiColor(1, 1, 1, 0.9f); uiText(F_SMALL, "CREDITS", 296, 164, 0.5f, 0);
	snprintf(b, sizeof b, "+%d", G.creditsEarned); uiColor(YEL, 1); uiTextS(F_LCD, b, 296, 192, 0.8f, 0);
	uiWhite(1);
}
static void drawResultsBottom(u32 down) {
	bgPoly(320);
	if (uiSprBtn(T_gui_ride_button, 160, 64, 0.72f, NULL, F_SMALL, 1) || (down & KEY_A)) startRide();
	uiColor(1, 1, 1, 1); uiText(F_SMALL, "RIDE AGAIN", 160, 124, 0.45f, 1); uiWhite(1);
	if (uiSprBtn(T_gui_credits, 90, 180, 0.42f, NULL, F_SMALL, 1) || (down & KEY_X)) { shopBack = SC_RIDE; sceneGo(SC_SHOP); }
	uiColor(YEL, 1); uiText(F_SMALL, "UPGRADES", 90, 220, 0.42f, 1); uiWhite(1);
	if (uiSprBtn(T_gui_quit_icon, 230, 180, 0.62f, NULL, F_SMALL, 1) || (down & KEY_B)) { sceneGo(SC_MENU); }
	uiColor(1, 1, 1, 1); uiText(F_SMALL, "MAIN MENU", 230, 220, 0.42f, 1); uiWhite(1);
}

// ---------------------------------------------------------------- the in-ride tutorials (shown once each, in the original's four situations)
static const int TUT_N[4] = { 4, 8, 2, 4 }, TUT_PAGES[4] = { 1, 2, 1, 1 };
static const int TUT_TEX[4][8] = {
	{ T_gui_tutorial_01_1, T_gui_tutorial_01_2, T_gui_tutorial_01_3, T_gui_tutorial_01_4 },
	{ T_gui_tutorial_02a_1, T_gui_tutorial_02a_2, T_gui_tutorial_02a_3, T_gui_tutorial_02a_4, T_gui_tutorial_02b_1, T_gui_tutorial_02b_2, T_gui_tutorial_02b_3, T_gui_tutorial_02b_4 },
	{ T_gui_tutorial_03_1, T_gui_tutorial_03_2 },
	{ T_gui_tutorial_04_1, T_gui_tutorial_04_2, T_gui_tutorial_04_3, T_gui_tutorial_04_4 } };
static const char* TUT_NAME[4] = { "BALANCE", "BOOST", "MULTIPLIER", "EFFECTS" };
static const char* TUT_TXT[4][2] = {
	{ "Use the Circle Pad to balance the ride car around corners.", NULL },
	{ "By completing sections perfectly, riders become MARMALISED.", "This gives you boost to increase the speed of the ride car." },
	{ "Upgraded sections earn more points.", NULL },
	{ "Legs will try to distract you with effects. Survive them for bonus points!", NULL } };
static int tutNum = 0, tutPage = 0; static float tutClock = 0;
static bool tutWants(int n) { return tutEnabled && !((tutSeen >> n) & 1) && scene == SC_RIDE; }
static void tutTrigger(int n) {
	if (!tutWants(n)) return;
	tutSeen |= 1 << n; saveGame(); tutNum = n; tutPage = 0; tutClock = 0; touchBoost = false; launchReq = false;
	sceneEnter(SC_TUT);
}
static void drawTutTop(void) {
	dimScreen(400, 0.55f);
	shadowText(F_TITLE, "TUTORIAL", 200, 84, 1.0f, 1, 1, 1, 1);
	uiColor(YEL, 1); uiTextS(F_SMALL, TUT_NAME[tutNum], 200, 132, 0.7f, 1); uiWhite(1);
}
static void drawTutBottom(u32 down) {
	bgPoly(320);
	int fpp = TUT_N[tutNum] / TUT_PAGES[tutNum], fr = tutPage * fpp + ((int)(tutClock / 0.5f) % fpp);
	uiWhite(1); uiSprC(TUT_TEX[tutNum][fr], 160, 70, 1.0f);
	uiColor(1, 1, 1, 1); uiWrap(F_SMALL, TUT_TXT[tutNum][tutPage], 160, 150, 280, 0.45f, 16, 1); uiWhite(1);
	bool last = tutPage >= TUT_PAGES[tutNum] - 1;
	if (uiSprBtn(T_gui_poly_popup_1, 160, 212, 0.6f, last ? "CONTINUE" : "NEXT", F_SMALL, 0.5f) || (down & (KEY_A | KEY_START))) {
		if (!last) { tutPage++; tutClock = 0; } else { physAcc = 0; sceneEnter(SC_RIDE); }
	}
}

// ---------------------------------------------------------------- pause
static void drawPauseTop(void) {
	char b[32];
	dimScreen(400, 0.55f);
	shadowText(F_TITLE, "PAUSED", 200, 80, 1.3f, 1, 1, 1, 1);
	uiNum(b, (int)G.score); uiColor(YEL, 1); uiTextS(F_LCD, b, 200, 140, 1.0f, 1);
	uiColor(1, 1, 1, 0.85f); uiText(F_SMALL, "SCORE", 200, 168, 0.45f, 1); uiWhite(1);
}
static void drawPauseBottom(u32 down) {
	bgPoly(320);
	if (pauseConfirm) {
		uiColor(1, 1, 1, 1); uiText(F_SMALL, "Quit this ride?", 160, 50, 0.7f, 1);
		uiText(F_SMALL, "Your score will be lost.", 160, 80, 0.45f, 1); uiWhite(1);
		if (uiSprBtn(T_gui_yes_button, 90, 145, 0.45f, NULL, F_SMALL, 1) || (down & KEY_A)) { audioRideStop(); float top = (float)savedTopScore; resetGame(); G.topScore = top; sceneGo(SC_MENU); }
		if (uiSprBtn(T_gui_no_button, 230, 145, 0.45f, NULL, F_SMALL, 1) || (down & KEY_B)) pauseConfirm = false;
		return;
	}
	if (uiSprBtn(T_gui_button_continue, 160, 52, 0.62f, NULL, F_SMALL, 1) || (down & (KEY_START | KEY_B))) { physAcc = 0; sceneEnter(SC_RIDE); }
	bool changed = false;
	uiColor(1, 1, 1, 1); uiText(F_SMALL, "MUSIC", 16, 112, 0.55f, 0);
	bool m = uiToggle2(252, 112, 0.52f, musicEnabled, "OFF", "ON");
	if (m != musicEnabled) { musicEnabled = m; musicApply(); changed = true; }
	uiColor(1, 1, 1, 1); uiText(F_SMALL, "SOUND FX", 16, 150, 0.55f, 0);
	bool s = uiToggle2(252, 150, 0.52f, sfxEnabled, "OFF", "ON");
	if (s != sfxEnabled) { sfxEnabled = s; changed = true; }
	uiWhite(1); if (changed) saveGame();
	if (uiSprBtn(T_gui_poly_popup_1, 160, 206, 0.6f, "QUIT TO MENU", F_SMALL, 0.5f) || (down & KEY_X)) pauseConfirm = true;
}

// ---------------------------------------------------------------- upgrade shop
static int upIcon(int i) {
	static const int tr[10] = { T_gui_indoorHeartlineIcon, T_gui_invertedFirstDrop, T_gui_butterfly, T_gui_batwing, T_gui_camelback,
		T_gui_twistToCorner, T_gui_pretzelLoop, T_gui_cobraRoll, T_gui_doubleHeartline, T_gui_loopIcon };
	static const int legBase[5] = { T_gui_gas_1, T_gui_hypno_1, T_gui_flasher_1, T_gui_tickler_1, T_gui_innoc_1 };
	if (i < N_TRACK_UPGRADES) return tr[i];
	int lv = upLevel[i]; if (lv < 1) lv = 1; if (lv > 3) lv = 3;
	return legBase[i - N_TRACK_UPGRADES] + lv - 1;
}
static bool upLocked(int i) { return upLevel[i] < UPGRADES[i].maxLevel && upLevel[i] >= effMax(i); }
static void shopStatus(int i, char* a, size_t n) {
	const UpgradeDef* u = &UPGRADES[i];
	if (upLevel[i] >= u->maxLevel) snprintf(a, n, "%s", "FULLY UPGRADED");
	else if (upLocked(i)) snprintf(a, n, "LOCKED  -  unlock for %d credits", UNLOCK_COST);
	else snprintf(a, n, "NEXT LEVEL:  %d credits", u->costs[upLevel[i]]);
}
static void drawShopTop(void) {
	char b[64];
	bgPoly(400);
	shadowText(F_TITLE, "UPGRADES", 20, 26, 0.62f, 0, 1, 1, 1);
	uiWhite(1); uiSprC(T_gui_credits, 372, 26, 0.3f);
	snprintf(b, sizeof b, "%d", credits); uiColor(YEL, 1); uiTextS(F_LCD, b, 348, 26, 0.9f, 2);
	int i = (shopPage == 0 ? 0 : N_TRACK_UPGRADES) + shopSel; const UpgradeDef* u = &UPGRADES[i];
	// big icon
	bool locked = upLocked(i) && upLevel[i] == 0;
	uiWhite(1);
	uiColor(YEL, 1); uiFill(16, 62, 124, 124);
	uiWhite(1); if (locked && UPGRADES[i].maxUnlocked == 0) uiRect(T_gui_secretIcon, 18, 64, 120, 120); else { uiColor(1, 1, 1, upLevel[i] > 0 ? 1.0f : 0.75f); uiRect(upIcon(i), 18, 64, 120, 120); }
	uiWhite(1);
	uiColor(YEL, 1); uiWrap(F_SMALL, u->display, 158, 74, 232, 0.7f, 24, 0);
	uiColor(1, 1, 1, 1); uiWrap(F_SMALL, u->desc, 158, 108, 232, 0.42f, 16, 0);
	if (u->maxLevel > 1) {
		snprintf(b, sizeof b, "LEVEL %d / %d", upLevel[i], u->maxLevel); uiColor(0.85f, 0.85f, 0.7f, 1); uiText(F_SMALL, b, 158, 172, 0.5f, 0);
	}
	shopStatus(i, b, sizeof b); uiColor(PINK, 1); uiText(F_SMALL, b, 158, 190, 0.46f, 0);
	if (shopMsg[0]) { uiColor(YEL, 1); uiText(F_SMALL, shopMsg, 200, 222, 0.5f, 1); }
	uiWhite(1);
}
static void drawCutCaption(void) {   // over the upgrade fly-by on the top screen
	const UpgradeDef* u = &UPGRADES[shopCutUp];
	float tot = camTracks[shopCutTrack].total, a = fminf(fminf(shopCutT / 0.5f, (tot - shopCutT) / 0.5f), 1.0f); if (a < 0) a = 0;
	uiColor(0, 0, 0, 0.55f * a); uiFill(0, 0, 400, 52); uiFill(0, 214, 400, 26);
	uiColor(YEL, a); uiText(F_SMALL, "UPGRADE", 200, 14, 0.38f, 1);
	uiColor(1, 1, 1, a); uiTextS(F_TITLE, u->display, 200, 34, 0.5f, 1);
	uiColor(1, 1, 1, 0.8f * a); uiText(F_SMALL, "B: skip", 200, 227, 0.36f, 1);
	uiWhite(1);
}
static void drawShopBottom(u32 down) {
	bgPoly(320);
	// tabs
	for (int t = 0; t < 2; t++) {
		float cx = 85 + t * 150;
		uiColor(1, 1, 1, shopPage == t ? 1.0f : 0.55f);
		if (uiSprBtn(T_gui_poly_popup_1, cx, 28, 0.5f, t == 0 ? "TRACK" : "LEGS", F_SMALL, 0.55f) && shopPage != t) { shopPage = t; shopSel = 0; shopMsg[0] = 0; }
	}
	uiWhite(1);
	int first = pageFirst(shopPage), n = pageCount(shopPage);
	for (int k = 0; k < n; k++) {
		int i = first + k, col = k % 5, row = k / 5;
		float x = 12 + col * 59.5f, y = 62 + row * 60;
		if (k == shopSel) { uiColor(1, 1, 1, 1); uiFill(x - 4, y - 4, 60, 60); uiColor(0.13f, 0.06f, 0.30f, 1); uiFill(x - 2, y - 2, 56, 56); }
		bool pr; if (uiHit(x - 3, y - 3, 58, 58, &pr)) shopSel = k;
		bool dim = upLevel[i] == 0 && upLocked(i);
		uiColor(dim ? 0.5f : 1, dim ? 0.5f : 1, dim ? 0.5f : 1, 1);
		uiRect((dim && UPGRADES[i].maxUnlocked == 0) ? T_gui_secretIcon : upIcon(i), x, y, 52, 52);
		uiWhite(1);
		if (upLevel[i] > 0) {
			if (UPGRADES[i].maxLevel == 1) uiRect(T_checkbox_ticked, x + 30, y + 30, 24, 24);
			else { char l[16]; snprintf(l, sizeof l, "%d", upLevel[i]); uiColor(0, 0, 0, 0.7f); uiFill(x + 36, y + 36, 16, 16); uiColor(YEL, 1); uiText(F_SMALL, l, x + 44, y + 44, 0.5f, 1); uiWhite(1); }
		}
	}
	int i = first + shopSel; bool owned = upLevel[i] >= UPGRADES[i].maxLevel;
	char lab[24];
	if (owned) snprintf(lab, sizeof lab, "OWNED");
	else if (upLocked(i)) snprintf(lab, sizeof lab, "UNLOCK  %d", UNLOCK_COST);
	else snprintf(lab, sizeof lab, "BUY  %d", UPGRADES[i].costs[upLevel[i]]);
	uiColor(1, 1, 1, owned ? 0.5f : 1.0f);
	if (uiSprBtn(T_gui_poly_popup_1, 85, 208, 0.6f, lab, F_SMALL, 0.55f) || (down & KEY_A)) shopBuy();
	uiWhite(1);
	if (uiSprBtn(T_gui_poly_popup_1, 235, 208, 0.6f, "BACK", F_SMALL, 0.55f) || (down & KEY_B)) { saveGame(); sceneGo(shopBack); }
	uiWhite(1);
}

// ---------------------------------------------------------------- scene dispatch
static void drawFade(float w) { if (fade > 0.001f) { uiColor(0, 0, 0, fade); uiFill(0, 0, w, 240); uiWhite(1); } }
static bool sceneHasWorld(void) { return scene == SC_RIDE || scene == SC_PAUSE || scene == SC_TUT; }
static bool sceneHasModel(void) { return scene == SC_FACTS; }

static void drawTopScreen(void) {
	switch (scene) {
	case SC_SPLASH: drawSplashTop(); break;
	case SC_INTRO: drawIntroTop(); break;
	case SC_MENU: drawMenuTop(); break;
	case SC_FACTS: drawFactsTop(); break;
	case SC_OPTIONS: drawOptionsTop("OPTIONS"); break;
	case SC_TIPS: drawTipsTop(); break;
	case SC_STATS: drawStatsTop(); break;
	case SC_QUIT: drawQuitTop(); break;
	case SC_SHOP: if (shopCutT >= 0) drawCutCaption(); else drawShopTop(); break;
	case SC_RIDE: if (G.state == ST_RESULTS) drawResultsTop(); else drawHudTop(); break;
	case SC_PAUSE: drawPauseTop(); break;
	case SC_TUT: drawTutTop(); break;
	default: break;
	}
	if (scene != SC_INTRO) { creepDraw(400, 240); drawAchPopup(); drawFade(400); }
}
static void drawBottomScreen(u32 down) {
	switch (scene) {
	case SC_SPLASH: drawSplashBottom(); break;
	case SC_INTRO: drawIntroBottom(); break;
	case SC_MENU: drawMenuBottom(down); break;
	case SC_FACTS: drawFactsBottom(down); break;
	case SC_OPTIONS: drawOptionsBottom(down); break;
	case SC_TIPS: drawBackBottom(down); break;
	case SC_STATS: drawAchBottom(down); break;
	case SC_QUIT: drawQuitBottom(down); break;
	case SC_SHOP: drawShopBottom(down); break;
	case SC_RIDE: if (G.state == ST_RESULTS) drawResultsBottom(down); else drawHudBottom(down); break;
	case SC_PAUSE: drawPauseBottom(down); break;
	case SC_TUT: drawTutBottom(down); break;
	default: break;
	}
	if (scene != SC_INTRO) { creepDraw(320, 240); drawFade(320); }
}

// ---------------------------------------------------------------- game logic per frame
static void sceneUpdate(float dt, u32 down, u32 held) {
	sceneT += dt;
	achUpdate(dt); creepUpdate(dt); fxUpdate(dt, scene == SC_RIDE && G.state == ST_RUN); legUpdate(dt, scene == SC_RIDE && G.state == ST_RUN);
	if (scene == SC_MENU) updateFace(dt);
	if (scene == SC_TUT) tutClock += dt;
	if (scene == SC_RIDE) birdsUpdate(dt);
	if (scene == SC_RIDE) tunUpdate(dt, frameAt(G.dist).pos, G.state != ST_READY);
	if (scene == SC_SPLASH) {
		if (sceneT > 3.8f || (sceneT > 0.5f && (tUp || (down & (KEY_A | KEY_START))))) sceneGo(SC_INTRO);
	} else if (scene == SC_INTRO) {
		vidT += dt;
		u32 fr = (u32)(vidT * (float)vidFps);
		if (introSkip || (down & (KEY_A | KEY_START))) { introSkip = false; vidClose(); musicStop(); sceneGo(SC_MENU); }
		else if (fr >= vidN) { vidClose(); sceneGo(SC_MENU); }
		else if (fr != vidFrame && vidF) vidShow(fr);
	} else if (scene == SC_STATS) {
		static int py2 = 0; circlePosition c3; hidCircleRead(&c3);
		if (tHeld && !tDown && !inRect(tSX, tSY, 100, 196, 120, 44)) achScroll -= (tY - py2);
		if (tHeld || tDown) py2 = tY;
		if (abs(c3.dy) > 20) achScroll -= (float)c3.dy / 156.0f * dt * 420.0f;
		if (down & KEY_DDOWN) achScroll += 62; if (down & KEY_DUP) achScroll -= 62;
		achScroll = fclamp(achScroll, 0, N_ACH * 62.0f - 180.0f);
	} else if (scene == SC_FACTS) {
		static int px = 0, py = 0; static float idle = 0;
		circlePosition c2; hidCircleRead(&c2);
		bool onBtn = inRect(tSX, tSY, 196, 184, 90, 56);
		if (tHeld && !tDown && !onBtn && !uiBlocked) { orbYaw -= (tX - px) * 0.012f; orbPitch = fclamp(orbPitch + (tY - py) * 0.008f, -0.2f, 1.45f); idle = 0; }
		if (tHeld) { px = tX; py = tY; } else idle += dt;
		if (tDown) { px = tX; py = tY; }
		if (abs(c2.dx) > 20) { orbYaw += (float)c2.dx / 156.0f * dt * 1.6f; idle = 0; }
		if (abs(c2.dy) > 20) { orbPitch = fclamp(orbPitch + (float)c2.dy / 156.0f * dt * 1.0f, -0.2f, 1.45f); idle = 0; }
		if (idle > 2.0f) orbYaw += dt * 0.25f;
	} else if (scene == SC_RIDE) {
		circlePosition cp; hidCircleRead(&cp);
		float raw = fclamp((float)cp.dx / 156.0f, -1.0f, 1.0f), a = fabsf(raw);
		bool idle = a < STICK_DEADZONE;
		raw = idle ? 0.0f : (raw < 0 ? -1.0f : 1.0f) * (a - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
		float stick = stickSign * raw * sensitivity;
		bool boost = (held & (KEY_A | KEY_L | KEY_R)) != 0 || touchBoost;
		if (G.state == ST_READY) {
			readyUpdate(dt);
			if (launchReq && readyWait >= READY_WAIT && !introActive) { G.state = ST_RUN; launchReq = false; physAcc = 0; droneStop(); tutTrigger(0); } else launchReq = false;
			if (down & KEY_START) { sceneGo(SC_PAUSE); }
		} else if (G.state == ST_RUN) {
			if (down & KEY_START) sceneGo(SC_PAUSE);
			physAcc += dt; int steps = 0;
			while (physAcc >= FIXED_DT && steps < MAX_STEPS) { updateRide(FIXED_DT, stick, idle, boost); physAcc -= FIXED_DT; steps++; }
			if (steps == MAX_STEPS) physAcc = 0;
			if (scene == SC_RIDE) { if (G.smiles > tutLastSm) { tutLastSm = G.smiles; tutTrigger(1); } if (secs[G.section].mult > 1.0f) tutTrigger(2); }
			if (!endCamLatched && endCamOk && endInside(frameAt(G.dist).pos)) endCamLatched = true;
			if (G.state == ST_RESULTS) { if (G.newBest) sfxOnce(SFX_ACHIEVE, 0.8f); touchBoost = false; }
		}
		if (G.state != ST_READY) rideDronesUpdate(dt, frameAt(G.dist).pos);
		if (G.state == ST_RUN && !G.runOver) armUpdate(dt, G.meshTilt, G.accIcon, (secs[G.section].flags & 1) != 0, G.smiles, animT);
		updateCameraAndTrain(dt);
	}
	bool running = scene == SC_RIDE && G.state == ST_RUN;
	audioRide(running, G.speed, G.boosting, running && (secs[G.section].flags & 1), running && !G.runOver && !G.tiltOK && !(secs[G.section].flags & 1), running && !G.runOver && G.accIcon == 3 && !(secs[G.section].flags & 1), dt);
}
