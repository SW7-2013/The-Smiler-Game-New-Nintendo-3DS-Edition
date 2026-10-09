// Ride effects, ported from the original game: wheel sparks, boost flames and the smile icon that pops over each rider.
// They are drawn as additive sprites on the top screen, positioned with the same camera as the 3D world.
#pragma once

static u32 fxRng = 0x9E3779B9u;
static float fxRand(void) { fxRng ^= fxRng << 13; fxRng ^= fxRng >> 17; fxRng ^= fxRng << 5; return (float)(fxRng & 0xFFFFFF) / 16777216.0f; }

typedef struct { V3 p, v; float age, life, size, rot; int kind, side, frame; bool local; } Particle;   // kind 0 spark, 1 flame
#define FX_MAX 96
static Particle fxP[FX_MAX]; static int fxN = 0;
static float fxSparkAcc[2], fxFlameAcc[2];

// smile icons over the riders: 16 riders, a random one lights up each time another rider is marmalised
typedef struct { float t; int rider; } SmileIcon;
static SmileIcon fxIcons[16]; static int fxNIcons = 0, fxPerm[16], fxShown = 0;
static void fxShuffle(void) { for (int i = 0; i < 16; i++) fxPerm[i] = i; for (int i = 15; i > 0; i--) { int j = (int)(fxRand() * (i + 1)); int t = fxPerm[i]; fxPerm[i] = fxPerm[j]; fxPerm[j] = t; } }
static void fxReset(void) { fxN = 0; fxNIcons = 0; fxShown = 0; fxSparkAcc[0] = fxSparkAcc[1] = fxFlameAcc[0] = fxFlameAcc[1] = 0; fxShuffle(); }

// a point in the cart's own space -> world (the same model matrix drawWorld uses)
static M4 fxCartModel(int c) {
	static const float follow[4] = { 0, -5, -10, -15 };
	float dead = G.meshTilt - (G.meshTilt >= 0 ? 1.0f : -1.0f) * fminf(fabsf(G.meshTilt), 0.05f);
	Frame fr = frameAt(G.dist + follow[c]);
	float tilt = c == 0 ? dead : G.slaveTilt[c - 1];
	float px = tilt > 0 ? CART_WHEEL_WIDTH : -CART_WHEEL_WIDTH;
	M4 roll = m4_mul(m4_translate(px, 0, 0), m4_mul(m4_rotz(tilt * -180.0f * (float)M_PI / 180.0f), m4_translate(-px, 0, 0)));
	return m4_mul(m4_frame(fr.right, fr.up, fr.fwd, fr.pos), roll);
}
static V3 fxXform(const M4* model_, float lx, float ly, float lz) {
	const M4 model = *model_;
	return v3(model.m[0][0] * lx + model.m[0][1] * ly + model.m[0][2] * lz + model.m[0][3],
	          model.m[1][0] * lx + model.m[1][1] * ly + model.m[1][2] * lz + model.m[1][3],
	          model.m[2][0] * lx + model.m[2][1] * ly + model.m[2][2] * lz + model.m[2][3]);
}
static V3 fxCartPoint(int c, float lx, float ly, float lz) { M4 m = fxCartModel(c); return fxXform(&m, lx, ly, lz); }
static V3 fxCartDir(int c, float dx, float dy, float dz) {   // direction in cart space -> world (no translation)
	V3 a = fxCartPoint(c, 0, 0, 0), b = fxCartPoint(c, dx, dy, dz); return vsub(b, a);
}
static void fxAdd(Particle p) { if (fxN < FX_MAX) fxP[fxN++] = p; }

static void fxUpdate(float dt, bool running) {
	if (G.smiles < fxShown) fxReset();   // a new ride started
	// age and remove
	for (int i = 0; i < fxN; ) {
		Particle* p = &fxP[i]; p->age += dt;
		if (p->age >= p->life || !running) { fxP[i] = fxP[--fxN]; continue; }
		p->p = vadd(p->p, vmul(p->v, dt));      // sparks move in the cart's own space, flames hang in the world
		i++;
	}
	if (!running) return;
	const int lastCart = 3;   // the effects sit on the rear carriage, the one nearest the chase camera
	// sparks: the wheel that is scraping the rail, whenever you are leaning clearly wrong (original: |tilt| > 0.25)
	bool wrong = fabsf(G.trueTilt) > 0.25f && !G.runOver;
	int side = G.meshTilt > 0 ? 1 : 0;
	for (int s = 0; s < 2; s++) {
		bool on = wrong && s == side;
		if (on) { fxSparkAcc[s] += dt * 40.0f; while (fxSparkAcc[s] >= 1.0f) {
			fxSparkAcc[s] -= 1.0f;
			float sx = s ? 5.2f : -4.8f;
			Particle p; memset(&p, 0, sizeof p);
			p.kind = 0; p.side = s; p.age = fxRand() * 0.05f; p.life = 0.45f + fxRand() * 0.2f; p.size = 0.5f;
			V3 d = v3((s ? 0.27f : -0.28f) + (fxRand() - 0.5f) * 0.3f, 0.67f + (fxRand() - 0.5f) * 0.25f, -0.69f + (fxRand() - 0.5f) * 0.3f);
			p.local = true; p.p = v3(sx, 0.1f, 0.6f); p.v = vmul(d, 14.0f + fxRand() * 6.0f);
			fxAdd(p);
		} } else fxSparkAcc[s] = 0;
	}
	// boost flames from both sides (the side that is scraping is switched off, like the original)
	for (int s = 0; s < 2; s++) {
		bool on = G.boosting && !(wrong && s != side);   // like the original, the far-side flame goes out while one wheel scrapes
		if (on) { fxFlameAcc[s] += dt * 26.0f; while (fxFlameAcc[s] >= 1.0f) {
			fxFlameAcc[s] -= 1.0f;
			Particle p; memset(&p, 0, sizeof p);
			p.kind = 1; p.side = s; p.age = 0; p.life = 0.5f; p.size = 1.0f; p.rot = (fxRand() - 0.5f) * 0.7f; p.frame = (int)(fxRand() * 4) & 3;
			p.p = fxCartPoint(lastCart, s ? 4.99f : -4.28f, s ? -0.18f : -0.39f, s ? 1.4f : 1.95f);
			p.v = v3((fxRand() - 0.5f) * 1.5f, 0.6f + fxRand() * 0.8f, (fxRand() - 0.5f) * 1.5f);
			fxAdd(p);
		} } else fxFlameAcc[s] = 0;
	}
	// riders marmalised since last frame: pop a smile over a random rider
	while (fxShown < G.smiles && fxShown < 16) {
		if (fxNIcons < 16) { fxIcons[fxNIcons].t = 0; fxIcons[fxNIcons].rider = fxPerm[fxShown]; fxNIcons++; }
		fxShown++;
	}
	for (int i = 0; i < fxNIcons; ) { fxIcons[i].t += dt; if (fxIcons[i].t >= 3.5f) { fxIcons[i] = fxIcons[--fxNIcons]; continue; } i++; }
}

// world -> top-screen pixels with the current camera (viewM); false when behind the camera
static bool fxProject(V3 w, float* sx, float* sy, float* sc) {
	const M4* v = &viewM;
	float cx = v->m[0][0] * w.x + v->m[0][1] * w.y + v->m[0][2] * w.z + v->m[0][3];
	float cy = v->m[1][0] * w.x + v->m[1][1] * w.y + v->m[1][2] * w.z + v->m[1][3];
	float cz = v->m[2][0] * w.x + v->m[2][1] * w.y + v->m[2][2] * w.z + v->m[2][3];
	if (cz < 2.0f) return false;
	float th = tanf(CAM_FOV_DEG * 0.5f * (float)M_PI / 180.0f);
	*sx = 200 + cx / (cz * th * (400.0f / 240.0f)) * 200; *sy = 120 - cy / (cz * th) * 120; *sc = 120.0f / (cz * th);   // pixels per world unit
	return true;
}
static void uiAdditive(bool on) {
	uiFlushRun();
	if (on) C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
	else C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}
// flame colour over its life, as in the original gradient: blue-white, white, pale yellow, yellow, orange, red, fading out
static void fxFlameColor(float t, float* r, float* g, float* b, float* a) {
	static const float k[6][4] = { { 0.55f, 0.85f, 1.0f, 1 }, { 1, 1, 1, 1 }, { 1, 1, 0.70f, 1 }, { 1, 0.89f, 0.1f, 0.9f }, { 1, 0.6f, 0.15f, 0.6f }, { 1, 0.15f, 0.05f, 0 } };
	static const float tt[6] = { 0, 0.04f, 0.1f, 0.2f, 0.7f, 1.0f };
	int i = 0; while (i < 4 && t > tt[i + 1]) i++;
	float u = fclamp((t - tt[i]) / (tt[i + 1] - tt[i]), 0, 1);
	*r = flerp(k[i][0], k[i + 1][0], u); *g = flerp(k[i][1], k[i + 1][1], u); *b = flerp(k[i][2], k[i + 1][2], u); *a = flerp(k[i][3], k[i + 1][3], u);
}
static void fxDraw(void) {
	M4 rear = fxCartModel(3);
	if (G.state != ST_RUN && fxN == 0 && fxNIcons == 0) return;   // (the leg overlays are drawn separately)
	uiAdditive(true);
	for (int i = 0; i < fxN; i++) {
		const Particle* p = &fxP[i]; if (p->kind == 2) continue; float sx, sy, sc; V3 wp = p->local ? fxXform(&rear, p->p.x, p->p.y, p->p.z) : p->p; if (!fxProject(wp, &sx, &sy, &sc)) continue;
		float t = p->age / p->life;
		if (p->kind == 1) {
			float r, g, b, a; fxFlameColor(t, &r, &g, &b, &a);
			float size = p->size * (0.9f + 4.1f * t) * 0.9f * sc; if (size > 90) size = 90;   // grows to ~5x its size, as in the original
			uiColor(r, g, b, a * 0.85f);
			int f = p->frame;
			{ float u0 = (f & 1) * 0.5f, v0 = 1.0f - (f >> 1) * 0.5f; uiRectRotUV(T_flames, sx - size * 0.5f, sy - size * 0.5f, size, size, p->rot, sx, sy, u0, v0, u0 + 0.5f, v0 - 0.5f); }
		} else {   // spark: a short bright streak along its direction of travel on screen
			V3 ql = vadd(p->p, vmul(p->v, -0.06f)); V3 q = fxXform(&rear, ql.x, ql.y, ql.z); float tx, ty, ts; if (!fxProject(q, &tx, &ty, &ts)) continue;
			float dx = sx - tx, dy = sy - ty, len = sqrtf(dx * dx + dy * dy) + 0.001f, w = p->size * sc * 1.2f; if (w < 2.5f) w = 2.5f; if (w > 10) w = 10; if (len > 60) len = 60;
			float fade = 1.0f - t, cr = 1.0f, cg = flerp(1.0f, 0.55f, t), cb = flerp(0.85f, 0.1f, t);
			uiColor(cr, cg, cb, fade);
			float ang = atan2f(dy, dx);
			uiRectRot(T_sparticle, sx - len * 0.5f - 1.0f, sy - w * 0.5f, len + 2.0f, w, ang, sx, sy);
		}
	}
	uiAdditive(false); uiWhite(1);
	for (int i = 0; i < fxN; i++) {   // gas clouds from the Giggler: big soft puffs that swell and fade
		const Particle* p = &fxP[i]; if (p->kind != 2) continue;
		float sx, sy, sc; if (!fxProject(p->p, &sx, &sy, &sc)) continue;
		float t = p->age / p->life, size = (8.0f + 20.0f * t) * sc; if (size > 260) size = 260;
		uiColor(0.95f, 0.97f, 0.35f, 0.5f * (t < 0.15f ? t / 0.15f : 1.0f - (t - 0.15f) / 0.85f));
		uiRectRot(T_gasserCloud_Alpha, sx - size * 0.5f, sy - size * 0.5f, size, size, p->rot + t, sx, sy);
	}
	uiWhite(1);
	// smile icons float above the riders: grow in over 0.5s, hold, shrink away at 3s
	for (int i = 0; i < fxNIcons; i++) {
		const SmileIcon* ic = &fxIcons[i]; int cart = ic->rider / 4, seat = ic->rider % 4;
		float k = 1.0f; if (ic->t < 0.5f) k = ic->t * 2.0f; if (ic->t > 3.0f) k = 1.0f - (ic->t - 3.0f) * 2.0f;
		M4 cm = fxCartModel(cart); V3 w = fxXform(&cm, -3.3f + seat * 2.2f, 10.5f + 0.4f * sinf(ic->t * 5.0f), 1.4f);
		float sx, sy, sc; if (!fxProject(w, &sx, &sy, &sc)) continue;
		float size = 4.2f * sc * k; if (size > 70) size = 70; if (size < 1) continue;
		uiWhite(1); uiRect(T_smileTag, sx - size * 0.5f, sy - size * 0.8f, size, size);
	}
	uiWhite(1);
}
