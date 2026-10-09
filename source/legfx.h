// Leg upgrade effects: the five legs along the track give bonus points and each plays an effect when you pass through.
// 0 Giggler (gas cloud), 1 Hypnotiser (spiral overlay), 2 Flasher (strobe), 3 Tickler (laughing camera sway), 4 Inoculator (creepy flashes)
#pragma once
static bool legIn[LEG_MAXTRIG];
static bool tutWants(int n); static void tutTrigger(int n);
static float gasT = -1, gasAcc = 0;
static float hypT = -1; static int hypCurve = -1;
static float flashT = -1, flashLen = 0, flashPeak = 0; static bool flashLoop = false;
static float inocT = -1, inocSlot = 0; static int inocCurve = -1, inocKind = 0, inocPic = 0;   // kind 0 none, 1 static, 2 picture
static const int LEG_PICS[8] = { T_boss2, T_joy, T_kitchener, T_mona, T_obedience, T_smile, T_xray, T_boss };
#define STROBE_PERIOD 0.10776f

static bool legSoundLive = false;
static void legStopAll(void) {
	if (legSoundLive) { chStop(CH_LEG); if (!creepNoiseOn) chStop(CH_NOISE); legSoundLive = false; }
	gasT = hypT = flashT = inocT = legLaughT = -1; inocKind = 0;
	if (flashLoop) { chStop(CH_LEG); flashLoop = false; }
	chStop(CH_LEG);
}
static void legReset(void) { memset(legIn, 0, sizeof legIn); legStopAll(); }

static bool legInside(const LegTrig* t, V3 p, float margin) {
	float d[3] = { p.x - t->c[0], p.y - t->c[1], p.z - t->c[2] };
	for (int i = 0; i < 3; i++) {
		float l = d[0] * t->ax[i * 3] + d[1] * t->ax[i * 3 + 1] + d[2] * t->ax[i * 3 + 2];
		if (fabsf(l) > t->half[i] + margin) return false;
	}
	return true;
}
static void legTrigger(const LegTrig* t, int level) {
	legSoundLive = true;
	G.score += t->pts[level] * userAccuracy() * (G.speed / FULL_SCORE_SPEED);   // Cart.PointsBoost
	switch (t->leg) {
	case 0: gasT = 0; gasAcc = 0; sfxOnce(SFX_GAS, 0.8f); break;
	case 1: hypT = 0; hypCurve = t->curve[level]; break;
	case 2: flashLen = STROBE_PERIOD * t->flashes[level]; flashPeak = t->strobe; flashT = 0;
		if (flashLen > 0) { chPlay(CH_LEG, &sfx[SFX_FLASH], true, 0.6f, 1.0f); flashLoop = true; } break;
	case 3: legLaughT = 0; if (sfxEnabled) chPlay(CH_LEG, &sfx[SFX_LAUGH], true, 0.7f, 1.0f); break;
	case 4: inocT = 0; inocSlot = 0; inocCurve = t->curve[level]; inocKind = 0; sfxOnce(SFX_INOC, 0.8f); break;
	}
}
static void legUpdate(float dt, bool running) {
	if (!running) { if (gasT >= 0 || hypT >= 0 || flashT >= 0 || inocT >= 0 || legLaughT >= 0 || legSoundLive) legStopAll(); memset(legIn, 0, sizeof legIn); return; }
	V3 p = frameAt(G.dist).pos;
	for (int i = 0; i < nLegTrig; i++) {
		const LegTrig* t = &legTrig[i]; int up = N_TRACK_UPGRADES + (int)t->leg, level = up < N_UPGRADES ? upLevel[up] : 0;
		bool in = level > 0 && !G.runOver && legInside(t, p, 0.5f);
		if (in && !legIn[i]) { if (tutWants(3)) { tutTrigger(3); in = false; } else legTrigger(t, level); }   // the first effect waits for its tutorial
		legIn[i] = in;
	}
	if (gasT >= 0) {   // 4 seconds of gas pouring out around the lead cart
		gasT += dt; gasAcc += dt * 19.3f;
		while (gasAcc >= 1.0f && fxN < FX_MAX) {
			gasAcc -= 1.0f; Particle q; memset(&q, 0, sizeof q);
			q.kind = 2; q.life = 2.3f; q.size = 1.0f; q.rot = fxRand() * 6.28f;
			q.p = fxCartPoint(0, (fxRand() - 0.5f) * 8.0f, 10.5f + (fxRand() - 0.5f) * 4.0f, 10.3f);
			V3 d = fxCartDir(0, (fxRand() - 0.5f) * 6.0f, 2.0f + fxRand() * 4.0f, -(4.0f + fxRand() * 6.0f)); q.v = d;
			fxAdd(q);
		}
		if (gasT > 4.0f) gasT = -1;
	}
	if (hypT >= 0) { hypT += dt; if (hypT > legCurveEnd(hypCurve)) hypT = -1; }
	if (flashT >= 0) {
		flashT += dt;
		if (flashT > flashLen) { flashT = -1; if (flashLoop) { chStop(CH_LEG); flashLoop = false; sfxOnce(SFX_FLASHOUT, 0.7f); } }
	}
	if (legLaughT >= 0) { legLaughT += dt; if (legLaughT > legCurveEnd(legLaughCurve)) { legLaughT = -1; chStop(CH_LEG); } }
	if (inocT >= 0) {   // every quarter second: maybe a burst of static or a creepy picture, more likely at the peak of the curve
		inocSlot -= dt;
		if (inocSlot <= 0) {
			inocSlot = 0.25f; inocKind = 0;
			if (legCurveEval(inocCurve, inocT) > fxRand()) {
				if (fxRand() > 0.5f) { inocKind = 2; inocPic = (int)(fxRand() * 8) & 7; } else { inocKind = 1; sfxNoiseOnce(0.5f); }
			}
			inocT += 0.25f; if (inocT >= legCurveEnd(inocCurve)) { inocT = -1; }
		}
	} else inocKind = 0;
}
static void legDrawOverlays(void) {   // drawn over the 3D view, under the HUD
	if (hypT >= 0) {
		float f = fclamp(legCurveEval(hypCurve, hypT), 0, 1);
		if (f > 0.01f) {   // two spirals darken what is behind them (the textures are stored as darkening weights)
			uiFlushRun();
			C3D_AlphaBlend(GPU_BLEND_REVERSE_SUBTRACT, GPU_BLEND_ADD, GPU_DST_COLOR, GPU_ONE, GPU_ZERO, GPU_ONE);
			uiColor(f, f, f, 1); uiRectRot(T_hypnosis1, 200 - 260, 120 - 260, 520, 520, animT * 100.0f * (float)M_PI / 180.0f, 200, 120);
			uiRectRot(T_hypnosis4, 200 - 260, 120 - 260, 520, 520, animT * 20.0f * (float)M_PI / 180.0f, 200, 120);
			uiFlushRun(); uiWhite(1);
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
		}
	}
	if (flashT >= 0) {   // strobe: a rising and falling pulse every 0.108s
		float ph = fmodf(flashT, STROBE_PERIOD), k = ph < 0.0494f ? ph / 0.0494f : fmaxf(0.0f, 1.0f - (ph - 0.0494f) / (STROBE_PERIOD - 0.0494f));
		float a = fclamp(k * flashPeak, 0, 1);
		if (a > 0.01f) { uiAdditive(true); uiColor(1, 1, 1, a); uiFill(0, 0, 400, 240); uiAdditive(false); uiWhite(1); }
	}
	if (inocKind) {
		uiColor(1, 1, 1, 0.5f);
		if (inocKind == 1) uiRect(CREEP_STATIC[(int)(fxRand() * 3) % 3], 0, 0, 400, 240);
		else uiRect(LEG_PICS[inocPic], 0, 0, 400, 240);
		uiWhite(1);
	}
}
