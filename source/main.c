// Smiler 3DS - fan port. Track, cart and rules come from the original game's files.
// Top screen: the ride. Bottom screen: score and status.
// Controls: Circle Pad = lean, A / L / R = boost, A = start / ride again, START = quit.

#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "vshader_shbin.h"
#include "upgrades_gen.h"

#define CLEAR_COLOR 0x78AADCFF
#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// ---- values taken from the original game's scene ----
#define FULL_MARMALIZE   13000.0f   // score at which all 16 riders are smiling
#define RIDERS           16
#define FULL_SCORE_SPEED 120.0f
#define SLOW_RATE        5.0f       // speed lost per second when leaning too far
#define BOOST_FORCE      5.0f
#define CART_WHEEL_WIDTH 4.36f
#define GRAVITY_Y        (-9.81f)
#define FIXED_DT         0.02f      // Unity ran the ride physics at 50 Hz; we do the same, whatever the frame rate
#define MAX_STEPS        10
#define UNLOCK_COST      20         // replaces the AR scanner unlock
#define GRAV_MIN 0.5f
#define GRAV_MAX 3.0f
#define CAM_FOV_DEG      81.0f
#define CAM_HEIGHT       16.0f

// If leaning feels backwards on your console, change this to -1.
#define STICK_SIGN 1.0f
// Circle Pad sensitivity: how much lean a fully pushed stick gives. Adjust in game with D-Pad up/down.
#define SENS_DEFAULT 0.50f
#define SENS_MIN     0.20f
#define SENS_MAX     1.20f
#define STICK_DEADZONE 0.08f

// ---------------------------------------------------------------- tiny maths
typedef struct { float x, y, z; } V3;
typedef struct { float m[4][4]; } M4;

static V3 v3(float x, float y, float z) { V3 r = { x, y, z }; return r; }
static V3 vadd(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 vsub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 vmul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float vdot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 vcross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static V3 vnorm(V3 a) { float l = sqrtf(vdot(a, a)); return l > 1e-6f ? vmul(a, 1.0f / l) : a; }
static float flerp(float a, float b, float t) { return a + (b - a) * t; }
static float fclamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static M4 m4_identity(void) { M4 r; memset(&r, 0, sizeof r); for (int i = 0; i < 4; i++) r.m[i][i] = 1; return r; }
static M4 m4_mul(M4 a, M4 b) {
	M4 r;
	for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
		float s = 0; for (int k = 0; k < 4; k++) s += a.m[i][k] * b.m[k][j];
		r.m[i][j] = s;
	}
	return r;
}
static M4 m4_translate(float x, float y, float z) { M4 r = m4_identity(); r.m[0][3] = x; r.m[1][3] = y; r.m[2][3] = z; return r; }
static M4 m4_rotz(float a) {
	M4 r = m4_identity(); float c = cosf(a), s = sinf(a);
	r.m[0][0] = c; r.m[0][1] = -s; r.m[1][0] = s; r.m[1][1] = c; return r;
}
// columns are the local axes, so local (x,y,z) -> right*x + up*y + fwd*z + pos
static M4 m4_frame(V3 r, V3 u, V3 f, V3 p) {
	M4 m = m4_identity();
	m.m[0][0] = r.x; m.m[0][1] = u.x; m.m[0][2] = f.x; m.m[0][3] = p.x;
	m.m[1][0] = r.y; m.m[1][1] = u.y; m.m[1][2] = f.y; m.m[1][3] = p.y;
	m.m[2][0] = r.z; m.m[2][1] = u.z; m.m[2][2] = f.z; m.m[2][3] = p.z;
	return m;
}
// left-handed view matrix (camera looks along +z, like Unity)
static M4 m4_lookat(V3 eye, V3 target, V3 upv) {
	V3 f = vnorm(vsub(target, eye));
	V3 r = vcross(upv, f);
	if (vdot(r, r) < 1e-6f) r = vcross(v3(0, 1, 0), f);   // looking straight along "up"
	if (vdot(r, r) < 1e-6f) r = v3(1, 0, 0);
	r = vnorm(r);
	V3 u = vcross(f, r);
	M4 m = m4_identity();
	m.m[0][0] = r.x; m.m[0][1] = r.y; m.m[0][2] = r.z; m.m[0][3] = -vdot(r, eye);
	m.m[1][0] = u.x; m.m[1][1] = u.y; m.m[1][2] = u.z; m.m[1][3] = -vdot(u, eye);
	m.m[2][0] = f.x; m.m[2][1] = f.y; m.m[2][2] = f.z; m.m[2][3] = -vdot(f, eye);
	return m;
}
static void m4_to_c3d(const M4* a, C3D_Mtx* o) {
	for (int i = 0; i < 4; i++) { o->r[i].x = a->m[i][0]; o->r[i].y = a->m[i][1]; o->r[i].z = a->m[i][2]; o->r[i].w = a->m[i][3]; }
}

// ---------------------------------------------------------------- data
typedef struct { float x, y, z, u, v, nx, ny, nz; } Vertex;
typedef struct { u32 kind, tex, nV, nI, section, variant; Vertex* verts; u16* idx; float cx, cy, cz, cr; } Mesh;
typedef struct { float start, len, mult, crankSpeed, brakeSpeed, kickin; u32 flags; } Section;
typedef struct { V3 pos, fwd, up, right; } Frame;

#define MAX_MESH 2048
#define MAX_TEX  256
#define LOD_DIST 250.0f      // track sections nearer than this use the detailed model, further away the low-detail one
#define LOD_BAND 70.0f
#define CULL_DIST 1100.0f
static Mesh meshes[MAX_MESH]; static int nMeshes = 0;
static C3D_Tex textures[MAX_TEX]; static int nTex = 0; static bool texAlpha[MAX_TEX], texMarm[MAX_TEX];
static Section secs[16]; static int nSecs = 0;
static float* tdata = NULL; static int nPts = 0; static float trackLen = 0;
// every section exists as a base version and (for upgradable ones) a loop version
typedef struct { float len, mult, crankSpeed, brakeSpeed, kickin; u32 flags, n; float* data; } Variant;
typedef struct { int nVar, upId; Variant var[2]; } SecDef;
#define MAX_SAMPLES 9600
static SecDef secDefs[16]; static int nSecDefs = 0;
static int variantSel[17];   // chosen version for section numbers 1..16
static float gravScale = 1.0f;

static char errmsg[128] = "";
static float sensitivity = SENS_DEFAULT;
static float stickSign = STICK_SIGN;   // -1 = inverted lean, set in the options

static bool loadMeshes(void) {
	FILE* f = fopen("romfs:/meshes.bin", "rb");
	if (!f) { snprintf(errmsg, sizeof errmsg, "meshes.bin missing"); return false; }
	char magic[4]; u32 n;
	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "MSH2", 4) || fread(&n, 4, 1, f) != 1 || n > MAX_MESH) {
		fclose(f); snprintf(errmsg, sizeof errmsg, "meshes.bin bad header"); return false;
	}
	for (u32 i = 0; i < n; i++) {
		Mesh* m = &meshes[i]; u32 h[6];
		if (fread(h, 4, 6, f) != 6) { fclose(f); snprintf(errmsg, sizeof errmsg, "meshes.bin truncated"); return false; }
		m->kind = h[0]; m->tex = h[1]; m->nV = h[2]; m->nI = h[3]; m->section = h[4]; m->variant = h[5];
		size_t vb = (size_t)m->nV * sizeof(Vertex), ib = ((size_t)m->nI * 2 + 3) & ~3u;
		void* blk = linearAlloc(vb + ib);
		if (!blk) { fclose(f); snprintf(errmsg, sizeof errmsg, "out of linear memory"); return false; }
		if (fread(blk, 1, vb + ib, f) != vb + ib) { fclose(f); snprintf(errmsg, sizeof errmsg, "meshes.bin short read"); return false; }
		GSPGPU_FlushDataCache(blk, vb + ib);
			m->verts = (Vertex*)blk; m->idx = (u16*)((u8*)blk + vb);
		{   // bounding sphere for culling
			float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
			for (u32 k = 0; k < m->nV; k++) {
				const float* p = &m->verts[k].x;
				for (int c = 0; c < 3; c++) { if (p[c] < mn[c]) mn[c] = p[c]; if (p[c] > mx[c]) mx[c] = p[c]; }
			}
			m->cx = (mn[0] + mx[0]) * 0.5f; m->cy = (mn[1] + mx[1]) * 0.5f; m->cz = (mn[2] + mx[2]) * 0.5f;
			float r2 = 0;
			for (u32 k = 0; k < m->nV; k++) {
				float dx = m->verts[k].x - m->cx, dy = m->verts[k].y - m->cy, dz = m->verts[k].z - m->cz;
				float d2 = dx * dx + dy * dy + dz * dz; if (d2 > r2) r2 = d2;
			}
			m->cr = sqrtf(r2);
		}
	}
	fclose(f); nMeshes = (int)n; return true;
}

static bool loadTextures(void) {
	FILE* f = fopen("romfs:/textures.txt", "r");
	if (!f) { snprintf(errmsg, sizeof errmsg, "textures.txt missing"); return false; }
	char line[96];
	while (nTex < MAX_TEX && fgets(line, sizeof line, f)) {
		size_t l = strlen(line); while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
		if (!l) continue;
		char path[160]; snprintf(path, sizeof path, "romfs:/gfx/%s.t3x", line);
		FILE* t = fopen(path, "rb");
		if (!t) { fclose(f); snprintf(errmsg, sizeof errmsg, "missing %s.t3x", line); return false; }
		Tex3DS_Texture tx = Tex3DS_TextureImportStdio(t, &textures[nTex], NULL, false);
		fclose(t);
		if (!tx) { fclose(f); snprintf(errmsg, sizeof errmsg, "bad texture %s", line); return false; }
		Tex3DS_TextureFree(tx);
		texMarm[nTex] = !strncmp(line, "marmaliser", 10);   // the Marmaliser machine's own textures
		texAlpha[nTex] = (l > 2 && line[l - 2] == '_' && line[l - 1] == 'a');   // "_a" textures have see-through parts
		C3D_TexSetFilter(&textures[nTex], GPU_LINEAR, GPU_LINEAR);
		if (strncmp(line, "SKY_", 4) == 0 || strncmp(line, "ui_", 3) == 0 || strncmp(line, "font_", 5) == 0) C3D_TexSetWrap(&textures[nTex], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);   // no seams between sky faces
		else C3D_TexSetWrap(&textures[nTex], GPU_REPEAT, GPU_REPEAT);
		nTex++;
	}
	fclose(f); return true;
}

static bool loadTrack(void) {
	FILE* f = fopen("romfs:/track2.bin", "rb");
	if (!f) { snprintf(errmsg, sizeof errmsg, "track2.bin missing"); return false; }
	char magic[4]; u32 ns;
	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "TRK2", 4) || fread(&ns, 4, 1, f) != 1 || ns != 16) {
		fclose(f); snprintf(errmsg, sizeof errmsg, "track2.bin bad header"); return false;
	}
	for (u32 k = 0; k < ns; k++) {
		s32 h[2];
		if (fread(h, 4, 2, f) != 2 || h[0] < 1 || h[0] > 2) { fclose(f); snprintf(errmsg, sizeof errmsg, "track2.bin bad section"); return false; }
		secDefs[k].nVar = h[0]; secDefs[k].upId = h[1];
		for (int v = 0; v < h[0]; v++) {
			Variant* vr = &secDefs[k].var[v];
			if (fread(vr, 4, 7, f) != 7) { fclose(f); snprintf(errmsg, sizeof errmsg, "track2.bin truncated"); return false; }
			vr->data = (float*)malloc(sizeof(float) * 9 * vr->n);
			if (!vr->data || fread(vr->data, sizeof(float) * 9, vr->n, f) != vr->n) { fclose(f); snprintf(errmsg, sizeof errmsg, "track2.bin truncated"); return false; }
		}
	}
	fclose(f);
	nSecDefs = (int)ns;
	tdata = (float*)malloc(sizeof(float) * 9 * MAX_SAMPLES);
	if (!tdata) { snprintf(errmsg, sizeof errmsg, "out of memory"); return false; }
	return true;
}

// stitch the chosen version of every section into one continuous path (one sample per unit)
static void buildTrack(void) {
	float pos = 0;
	for (int k = 0; k < nSecDefs; k++) {
		Variant* v = &secDefs[k].var[variantSel[k + 1]];
		secs[k].start = pos; secs[k].len = v->len; secs[k].mult = v->mult; secs[k].crankSpeed = v->crankSpeed;
		secs[k].brakeSpeed = v->brakeSpeed; secs[k].kickin = v->kickin; secs[k].flags = v->flags;
		pos += v->len;
	}
	nSecs = nSecDefs; trackLen = pos;
	int n = (int)pos + 2; if (n > MAX_SAMPLES) n = MAX_SAMPLES;
	nPts = n;
	int k = 0;
	for (int i = 0; i < n; i++) {
		float g = (float)i;
		while (k < nSecs - 1 && g >= secs[k + 1].start) k++;
		Variant* v = &secDefs[k].var[variantSel[k + 1]];
		float t = g - secs[k].start; if (t > v->len) t = v->len; if (t < 0) t = 0;
		float u = t / v->len * (float)(v->n - 1);
		int i0 = (int)u; if (i0 > (int)v->n - 2) i0 = (int)v->n - 2;
		float fr = u - (float)i0;
		const float* pa = &v->data[i0 * 9]; const float* pb = &v->data[(i0 + 1) * 9];
		for (int c = 0; c < 9; c++) tdata[i * 9 + c] = flerp(pa[c], pb[c], fr);
	}
}

// ---------------------------------------------------------------- achievements (the ones that can be earned in this port)
#define N_ACH 14
enum { A_JUNIOR, A_STAGE1, A_STAGE5, A_PRO, A_TRAINEE, A_TMASTER, A_FIRST, A_SUPER, A_REPEAT, A_SISYPH, A_HOARD, A_RAIL, A_THROTTLE, A_TWIST };
static float achProg[N_ACH]; static u8 achEarned[N_ACH]; static int ridesPlayed = 0;
static void achAdd(int id, float v);
static void achSet(int id, float v);

// ---------------------------------------------------------------- upgrades, credits and saving
static int upLevel[N_UPGRADES];
static u8 upUnlocked[N_UPGRADES];
static int credits = 0, savedTopScore = 0;

static int effMax(int i) {   // highest level that can be bought right now
	const UpgradeDef* u = &UPGRADES[i];
	if (upUnlocked[i]) return u->maxLevel;
	return u->maxLevel < u->maxUnlocked ? u->maxLevel : u->maxUnlocked;
}

static void applyUpgrades(void) {
	for (int k = 0; k <= 16; k++) variantSel[k] = 0;
	for (int k = 0; k < nSecDefs; k++) {
		int up = secDefs[k].upId;
		if (up >= 0 && secDefs[k].nVar > 1 && up < N_UPGRADES && upLevel[up] > 0) variantSel[k + 1] = 1;
	}
	buildTrack();
}

#define SAVE_DIR  "sdmc:/3ds/smiler3ds"
#define SAVE_FILE "sdmc:/3ds/smiler3ds/save.dat"
typedef struct { char magic[4]; u32 version; s32 credits, topScore; s32 level[N_UPGRADES]; u8 unlocked[N_UPGRADES]; float gravScale, sensitivity;
	u8 musicOn, sfxOn, invert, pad;
	s32 rides; float achProg[N_ACH]; u8 achEarned[N_ACH]; u8 pad2[2];
	s32 tutSeen, tutOff; } SaveData;   // version 2 files end before musicOn, version 3 before rides, version 4 before tutSeen
static bool musicEnabled = true, sfxEnabled = true;
static int tutSeen = 0; static bool tutEnabled = true;   // which of the four in-ride tutorials have been shown, and whether they are switched on
#define GAME_VERSION "1.4"

static void saveGame(void) {
	SaveData d; memset(&d, 0, sizeof d);
	memcpy(d.magic, "SMLR", 4); d.version = 5; d.tutSeen = tutSeen; d.tutOff = !tutEnabled; d.credits = credits; d.topScore = savedTopScore;
	d.gravScale = gravScale; d.sensitivity = sensitivity;
	d.rides = ridesPlayed; for (int i = 0; i < N_ACH; i++) { d.achProg[i] = achProg[i]; d.achEarned[i] = achEarned[i]; }
	d.musicOn = musicEnabled; d.sfxOn = sfxEnabled; d.invert = stickSign < 0;
	for (int i = 0; i < N_UPGRADES; i++) { d.level[i] = upLevel[i]; d.unlocked[i] = upUnlocked[i]; }
	mkdir("sdmc:/3ds", 0777); mkdir(SAVE_DIR, 0777);
	FILE* f = fopen(SAVE_FILE, "wb");
	if (f) { fwrite(&d, sizeof d, 1, f); fclose(f); }
}

static void clearMemory(void) {   // wipe all progress: credits, score, upgrades, achievements, tutorials (the sound and control settings stay)
	credits = 0; savedTopScore = 0; ridesPlayed = 0; tutSeen = 0; tutEnabled = true;
	for (int i = 0; i < N_ACH; i++) { achProg[i] = 0; achEarned[i] = false; }
	for (int i = 0; i < N_UPGRADES; i++) { upLevel[i] = 0; upUnlocked[i] = 0; }
	saveGame();
}

static void loadGame(void) {
	FILE* f = fopen(SAVE_FILE, "rb");
	if (!f) return;
	SaveData d; memset(&d, 0, sizeof d);
	size_t n = fread(&d, 1, sizeof d, f);
	if (n >= offsetof(SaveData, musicOn) && !memcmp(d.magic, "SMLR", 4) && (d.version >= 2 && d.version <= 5)) {
		credits = d.credits < 0 ? 0 : d.credits; savedTopScore = d.topScore;
		if (d.gravScale >= GRAV_MIN && d.gravScale <= GRAV_MAX) gravScale = d.gravScale;
		if (d.sensitivity >= SENS_MIN && d.sensitivity <= SENS_MAX) sensitivity = d.sensitivity;
		if (d.version >= 4 && n >= offsetof(SaveData, tutSeen)) { ridesPlayed = d.rides; for (int i = 0; i < N_ACH; i++) { achProg[i] = d.achProg[i]; achEarned[i] = d.achEarned[i] != 0; } }
		if (d.version >= 3 && n >= offsetof(SaveData, rides)) { musicEnabled = d.musicOn != 0; sfxEnabled = d.sfxOn != 0; stickSign = d.invert ? -1.0f : 1.0f; }
		if (d.version >= 5 && n >= sizeof d) { tutSeen = d.tutSeen & 15; tutEnabled = d.tutOff == 0; }
		for (int i = 0; i < N_UPGRADES; i++) {
			upUnlocked[i] = d.unlocked[i] ? 1 : 0;
			upLevel[i] = d.level[i] < 0 ? 0 : (d.level[i] > UPGRADES[i].maxLevel ? UPGRADES[i].maxLevel : d.level[i]);
		}
	}
	fclose(f);
}

// position and orientation of the track at distance d
static Frame frameAt(float d) {
	float last = (float)(nPts - 1) - 0.001f, extra = 0.0f;
	if (d < 0.0f) { extra = fmaxf(d, -300.0f); d = 0.0f; }          // behind the start: carry on in a straight line
	else if (d > last) { extra = fminf(d - last, 300.0f); d = last; } // past the end

	int i = (int)d; float t = d - (float)i;
	const float* a = &tdata[i * 9]; const float* b = &tdata[(i + 1) * 9];
	Frame fr;
	fr.pos = v3(flerp(a[0], b[0], t), flerp(a[1], b[1], t), flerp(a[2], b[2], t));
	fr.fwd = vnorm(v3(flerp(a[3], b[3], t), flerp(a[4], b[4], t), flerp(a[5], b[5], t)));
	V3 up = v3(flerp(a[6], b[6], t), flerp(a[7], b[7], t), flerp(a[8], b[8], t));
	up = vnorm(vsub(up, vmul(fr.fwd, vdot(up, fr.fwd))));
	fr.up = up; fr.right = vcross(up, fr.fwd);
	if (extra != 0.0f) fr.pos = vadd(fr.pos, vmul(fr.fwd, extra));
	return fr;
}

static int sectionAt(float d) {
	for (int i = nSecs - 1; i >= 0; i--) if (d >= secs[i].start) return i;
	return 0;
}

// ---------------------------------------------------------------- game state
enum { ST_READY, ST_RUN, ST_RESULTS, ST_SHOP };
static struct {
	int state;
	float dist, speed, score;
	int smiles; float boostCount; bool boosting;
	float userTilt, targetTilt, trueTilt, meshTilt;
	bool tiltOK, runOver; float endTimer;
	int accIcon; // 3 perfect .. 0 too far
	int section;
	float viewLead, smoothedSpeed;
	float slaveTilt[3];
	float topScore;
	int creditsEarned; bool newBest;
	int worstAcc, secWorst, prevSection; float boostTime; bool wasBoosting;
} G;

static bool endCamLatched = false;
static void resetGame(void) {
	endCamLatched = false;
	float top = G.topScore;
	memset(&G, 0, sizeof G);
	G.topScore = top;
	G.state = ST_READY; G.speed = 0.1f; G.dist = 0.1f; G.tiltOK = true; G.accIcon = 3; G.worstAcc = 3; G.secWorst = 3;
	G.viewLead = 10.77f;
}

static float userAccuracy(void) {
	float t = fabsf(G.trueTilt);
	if (t < 0.06f) { G.accIcon = 3; return 1.0f; }
	if (t < 0.20f) { G.accIcon = 2; return 0.6f; }
	if (t < 0.45f) { G.accIcon = 1; return 0.3f; }
	G.accIcon = 0; return 0.0f;
}

static void updateRide(float dt, float stick, bool idle, bool boostHeld) {
	// leaning input (idle = stick is in the dead zone)
	if (idle) G.userTilt = flerp(G.userTilt, 0, fminf(1, dt * 2));
	else G.userTilt = flerp(G.userTilt, fclamp(stick, -1, 1), fminf(1, dt * 12));

	// how far the track bends ahead = the lean you should be holding
	Frame now = frameAt(G.dist), ahead = frameAt(G.dist + G.speed);
	G.targetTilt = vdot(ahead.fwd, now.right) * 0.5f;
	G.trueTilt = G.userTilt - G.targetTilt;
	if (idle) G.trueTilt *= 2;
	G.meshTilt = flerp(G.meshTilt, G.trueTilt, fminf(1, dt * 2));

	// boost
	if (boostHeld && !G.boosting && G.boostCount > 0) G.boosting = true;
	if (G.boosting && (!boostHeld || G.boostCount <= 0)) G.boosting = false;
	if (G.boosting && !G.wasBoosting) achAdd(A_RAIL, 1);
	G.wasBoosting = G.boosting;
	if (G.boosting) { G.speed += BOOST_FORCE * dt; G.boostCount -= dt; G.boostTime += dt; achSet(A_THROTTLE, G.boostTime); }

	// move along the track
	G.dist += G.speed * dt;
	if (G.dist > trackLen) G.dist = trackLen;
	G.section = sectionAt(G.dist);
	if (G.section != G.prevSection) {   // left a section: a loop (multiplier above 1) flown perfectly counts
		if (secs[G.prevSection].mult > 1.0f && G.secWorst == 3 && !G.runOver) achAdd(A_TWIST, 1);
		G.secWorst = 3; G.prevSection = G.section;
	}
	Section* s = &secs[G.section];
	bool crank = s->flags & 1, brake = s->flags & 2;
	if (!crank) G.speed += frameAt(G.dist).fwd.y * GRAVITY_Y * gravScale * dt;

	if (!G.runOver) {
		float acc = userAccuracy();
		if (!crank) { if (G.accIcon < G.worstAcc) G.worstAcc = G.accIcon; if (G.accIcon < G.secWorst) G.secWorst = G.accIcon; }
		G.score += fabsf(G.speed * dt * s->mult) * acc * (G.speed / FULL_SCORE_SPEED);
		int n = (int)(G.score / FULL_MARMALIZE * RIDERS);
		if (n > G.smiles && G.smiles < RIDERS) { G.smiles++; G.boostCount += 2; }
	}
	if (!G.tiltOK && !crank) G.speed -= dt * SLOW_RATE;   // the chain lift keeps hauling you up regardless
	G.tiltOK = fabsf(G.trueTilt) < 0.45f;

	if (brake) {
		if (G.speed > s->brakeSpeed) {
			float toEnd = s->start + s->len - G.dist;
			if (toEnd < s->len * s->kickin) G.speed -= (G.speed - s->brakeSpeed) / fmaxf(toEnd, 1.0f) * dt * G.speed;
		} else G.speed = s->brakeSpeed;
	}
	if (crank && G.speed < s->crankSpeed) G.speed = fmaxf(G.speed + s->crankSpeed * 0.3333f * dt, 1.0f);

	if (G.dist >= trackLen || (G.speed < 10.0f && !crank && !brake)) G.runOver = true;
	if (G.runOver) {
		G.speed *= (1.0f - dt); G.endTimer += dt;
		if (G.endTimer > 2.0f) {
			if (G.score > G.topScore) { G.topScore = G.score; G.newBest = true; }
			savedTopScore = (int)G.topScore;
			G.creditsEarned = (int)(G.score / 500.0f);   // 1 upgrade credit per 500 points, as in the original
			credits += G.creditsEarned;
			ridesPlayed++; achAdd(A_FIRST, 1); achAdd(A_REPEAT, 1); achAdd(A_SISYPH, 1);
			if (G.score >= FULL_MARMALIZE) achAdd(A_SUPER, 1);
			achSet(A_TRAINEE, (float)G.worstAcc); achSet(A_TMASTER, (float)G.worstAcc); achSet(A_HOARD, (float)credits);
			saveGame();
			G.state = ST_RESULTS;
		}
	}
}

// ---------------------------------------------------------------- rendering
static C3D_RenderTarget* topTarget; static C3D_RenderTarget* topTargetR; static C3D_RenderTarget* botTarget;   // topTargetR = the right eye (stereoscopic 3D)
#define STEREO_IOD    1.2f    // eye separation at full slider, in world units
#define STEREO_FOCUS  18.0f   // distance (world units) at which things sit exactly on the screen
static int texForce = -1;     // >= 0: draw every mesh with this texture instead of its own (used to paint the model-view track yellow)
static DVLB_s* vsh_dvlb; static shaderProgram_s program;
static int uLoc_projection, uLoc_modelView, uLoc_uvxf, uLoc_lightK, uLoc_tint;
#include "legdata.h"
#include "dronedata.h"
#include "camdata.h"
static float animT = 0; static int curAnim = -1;   // animation clock (seconds) and the animation currently loaded in the shader
static C3D_Mtx projection;
static M4 viewM;

static bool meshVisible(const Mesh* m, V3 eye, V3 dir) {
	V3 c = v3(m->cx - eye.x, m->cy - eye.y, m->cz - eye.z);
	float d = sqrtf(vdot(c, c));
	if (d - m->cr > CULL_DIST) return false;              // too far away to matter
	if (d <= m->cr) return true;                          // the camera is inside this chunk
	// view cone: the screen's corners are about 59 degrees off-centre, so use 62 degrees plus the chunk's own size
	float sb = m->cr / d, cb = sqrtf(1.0f - sb * sb);
	const float ca = 0.4695f, sa = 0.8829f;               // cos/sin of 62 degrees
	return vdot(c, dir) >= (ca * cb - sa * sb) * d;
}

// texture animations: 0 none, 1 marmaliser screen (16 frames, scrolling), 2 lift-approach screen (3-frame flicker), 3-5 lift chain scrolling
static void setAnim(int a) {
	if (a == curAnim) return;
	curAnim = a;
	float sx = 1, sy = 1, ox = 0, oy = 0;
	if (a == 1) { int fr = (int)(animT / 0.9f) % 16; sy = 1.0f / 16; ox = fmodf(animT * 0.2f, 1.0f); oy = fr / 16.0f; }
	else if (a == 2) { static const float o[3] = { -1.0f / 3, 0, 1.0f / 3 }; oy = o[(int)(animT * 8.0f) % 3]; }
	else if (a >= 3 && a <= 5) { static const float sp[3] = { -0.5f, -1.0f, -1.5f }; oy = fmodf(animT * sp[a - 3], 1.0f); }
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_uvxf, sx, sy, ox, oy);
}

static int alphaState = -1, lastTex = -1, drawCalls = 0, drawTris = 0;
static void drawMesh(const Mesh* m, const M4* model) {
	setAnim((int)(m->variant >> 16));
	M4 mv = m4_mul(viewM, *model);
	C3D_Mtx cm; m4_to_c3d(&mv, &cm);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &cm);
	C3D_BufInfo* bi = C3D_GetBufInfo(); BufInfo_Init(bi);
	BufInfo_Add(bi, m->verts, sizeof(Vertex), 3, 0x210);
	u32 ti = texForce >= 0 ? (u32)texForce : (m->tex < (u32)nTex ? m->tex : 0);
	if ((int)texAlpha[ti] != alphaState) { alphaState = texAlpha[ti]; C3D_AlphaTest(alphaState != 0, GPU_GREATER, 0x80); }
	if ((int)ti != lastTex) { C3D_TexBind(0, &textures[ti]); lastTex = (int)ti; }
	drawCalls++; drawTris += m->nI / 3;
	C3D_DrawElements(GPU_TRIANGLES, (int)m->nI, C3D_UNSIGNED_SHORT, m->idx);
}

static M4 m4_axis_angle(V3 ax, float a) {   // rotation about a unit axis (Rodrigues)
	float c = cosf(a), s = sinf(a), t = 1 - c; M4 r = m4_identity();
	r.m[0][0] = t * ax.x * ax.x + c;        r.m[0][1] = t * ax.x * ax.y - s * ax.z; r.m[0][2] = t * ax.x * ax.z + s * ax.y;
	r.m[1][0] = t * ax.x * ax.y + s * ax.z; r.m[1][1] = t * ax.y * ax.y + c;        r.m[1][2] = t * ax.y * ax.z - s * ax.x;
	r.m[2][0] = t * ax.x * ax.z - s * ax.y; r.m[2][1] = t * ax.y * ax.z + s * ax.x; r.m[2][2] = t * ax.z * ax.z + c;
	return r;
}
#include "armdata.h"
#include "birddata.h"
static void droneDrawOne(const Drone* d) {
	int carts = d->obl ? 1 : 4; u32 kind = d->obl ? 8 : 7;
	for (int c = 0; c < carts; c++) {
		Frame fr = dFrame(d->obl, d->dist - 5.0f * (float)c);
		M4 model = m4_frame(fr.right, fr.up, fr.fwd, fr.pos);
		for (int i = 0; i < nMeshes; i++) if (meshes[i].kind == kind) drawMesh(&meshes[i], &model);
	}
}
static void droneDraw(void) {   // the drone trains: four carts on the Smiler, one on Oblivion
	for (int k = 0; k < nDrones; k++) droneDrawOne(&drones[k]);
	for (int k = 0; k < nDsp; k++) if (rdr[k].on) droneDrawOne(&rdr[k].d);
}
static M4 spinMat(const LegSpin* L) {   // rotation about a pivot, as the original's RotateOnSpot / RotateAround
	V3 pv = v3(L->pv[0], L->pv[1], L->pv[2]);
	M4 rot = m4_axis_angle(v3(L->ax[0], L->ax[1], L->ax[2]), fmodf(L->dps * animT, 360.0f) * (float)M_PI / 180.0f);
	return m4_mul(m4_translate(pv.x, pv.y, pv.z), m4_mul(rot, m4_translate(-pv.x, -pv.y, -pv.z)));
}
static void drawWorld(void) {
	M4 ident = m4_identity();

	// chase camera, as in the original: trails behind the cart, above it, looking ahead
	Frame cart = frameAt(G.dist);
	Frame camF = frameAt(G.dist - G.viewLead);
	V3 eye = vadd(camF.pos, vmul(cart.up, CAM_HEIGHT));
	V3 target = frameAt(G.dist + G.smoothedSpeed).pos;
	if (legLaughT >= 0 && legLaughCurve >= 0) {   // the Tickler's laugh sways the camera, as in the original
		float st = fminf(legCurveEval(legLaughCurve, legLaughT), 1.0f) * 20.0f, cx = cosf(animT * 4.5f) * st, cy = sinf(animT * 6.0f) * st;
		V3 off = vadd(vmul(cart.right, cx), vmul(cart.up, cy)); eye = vadd(eye, off); target = vadd(target, off);
	}
	V3 upv = camF.up;
	if (camOn) {   // a scenic camera (upgrade cutscene) replaces the chase camera
		eye = camEye; target = camTgt; V3 f = vnorm(vsub(target, eye)), r = vnorm(vcross(v3(0, 1, 0), f));
		float a = camRoll * (float)M_PI / 180.0f; upv = vadd(vmul(v3(0, 1, 0), cosf(a)), vmul(r, sinf(a)));
	}
	viewM = m4_lookat(eye, target, upv);

	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	alphaState = -1; curAnim = -1; lastTex = -1; drawCalls = 0; drawTris = 0;
	{ static u64 t0 = 0; if (!t0) t0 = osGetTime(); animT = (float)(osGetTime() - t0) / 1000.0f; }
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightK, 1.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tint, 0.0f, 0.0f, 0.0f, 1.0f);

	V3 cdir = vnorm(vsub(target, eye));
	for (int i = 0; i < nMeshes; i++) {
		const Mesh* m = &meshes[i];
		if (m->kind == 1 || m->kind == 4 || m->kind == 5 || m->kind >= 7) continue;   // carts, sky, legs and drone carts are drawn below
		int var = (int)(m->variant & 0xFF), lod = (int)((m->variant >> 8) & 0xFF);
		if ((m->kind == 0 || m->kind == 3) && m->section != 0 && var != variantSel[m->section]) continue;   // other upgrade version
		if (!meshVisible(m, eye, cdir)) continue;
		if (m->kind == 0) {                                           // track: detailed nearby, simple far away
			V3 c = v3(m->cx - eye.x, m->cy - eye.y, m->cz - eye.z);
			float sd = sqrtf(vdot(c, c)) - m->cr;                         // distance to the chunk's surface
			if (lod == 0 && sd > LOD_DIST + LOD_BAND) continue;           // both versions are drawn in a short overlap band,
			if (lod == 1 && sd < LOD_DIST - LOD_BAND) continue;           // so the switch can never leave a gap
		}
		drawMesh(m, &ident);
	}

	// leg upgrades you own: level pieces 1..level, some of them spinning
	for (int i = 0; i < nMeshes; i++) {
		const Mesh* m = &meshes[i]; if (m->kind != 5) continue;
		int lv = (int)(m->variant & 0xFF), sp = (int)((m->variant >> 8) & 0xFF), leg = (int)m->section;
		if (N_TRACK_UPGRADES + leg >= N_UPGRADES || upLevel[N_TRACK_UPGRADES + leg] < lv) continue;
		M4 model = ident;
		if (sp > 0 && sp <= nLegSpin) {
			const LegSpin* L = &legSpin[sp - 1]; V3 pv = v3(L->pv[0], L->pv[1], L->pv[2]);
			M4 rot = m4_axis_angle(v3(L->ax[0], L->ax[1], L->ax[2]), fmodf(L->dps * animT, 360.0f) * (float)M_PI / 180.0f);
			model = m4_mul(m4_translate(pv.x, pv.y, pv.z), m4_mul(rot, m4_translate(-pv.x, -pv.y, -pv.z)));
		} else if (!meshVisible(m, eye, cdir)) continue;
		drawMesh(m, &model);
	}

	// animated scenery: the Enterprise wheel and the Submission arm and carriage
	for (int i = 0; i < nMeshes; i++) {
		const Mesh* m = &meshes[i]; if (m->kind != 9) continue;
		int so = (int)((m->variant >> 8) & 0xFF), si = (int)m->section; M4 model = ident; const LegSpin* piv = NULL;
		if (so > 0 && so <= nAnimSpin) { piv = &animSpin[so - 1]; model = spinMat(piv); }
		if (si > 0 && si <= nAnimSpin) { if (!piv) piv = &animSpin[si - 1]; model = m4_mul(model, spinMat(&animSpin[si - 1])); }
		if (piv) { Mesh t = *m; t.cx = piv->pv[0]; t.cy = piv->pv[1]; t.cz = piv->pv[2]; float dx = m->cx - t.cx, dy = m->cy - t.cy, dz = m->cz - t.cz; t.cr = m->cr + sqrtf(dx * dx + dy * dy + dz * dz); if (!meshVisible(&t, eye, cdir)) continue; }
		drawMesh(m, &model);
	}

	// the train: master cart plus three following carts, tilting when you lean wrongly
	static const float follow[4] = { 0, -5, -10, -15 };
	float dead = G.meshTilt - (G.meshTilt >= 0 ? 1.0f : -1.0f) * fminf(fabsf(G.meshTilt), 0.05f);
	for (int c = 0; c < 4; c++) {
		Frame fr = frameAt(G.dist + follow[c]);
		float tilt = c == 0 ? dead : G.slaveTilt[c - 1];
		float px = tilt > 0 ? CART_WHEEL_WIDTH : -CART_WHEEL_WIDTH;
		M4 roll = m4_mul(m4_translate(px, 0, 0), m4_mul(m4_rotz(tilt * -180.0f * (float)M_PI / 180.0f), m4_translate(-px, 0, 0)));
		M4 model = m4_mul(m4_frame(fr.right, fr.up, fr.fwd, fr.pos), roll);
		for (int i = 0; i < nMeshes; i++) if (meshes[i].kind == 1) drawMesh(&meshes[i], &model);
		armDraw(c, &model);
	}
	droneDraw(); birdsDraw(eye);

	// sky: drawn last and unlit, so it only fills the pixels nothing else has covered
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightK, 0.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tint, 1.0f, 1.0f, 1.0f, 1.0f);
	M4 skyModel = m4_translate(eye.x, eye.y, eye.z);
	for (int i = 0; i < nMeshes; i++) if (meshes[i].kind == 4) drawMesh(&meshes[i], &skyModel);
	// breadcrumb for crash hunting: the last line of this file shows where the ride was and how busy the frame was
	static int logTick = 0;
	if ((++logTick & 15) == 0) {
		FILE* lf = fopen("sdmc:/3ds/smiler3ds/log.txt", "w");
		if (lf) { fprintf(lf, "dist %.0f draws %d tris %d\n", G.dist, drawCalls, drawTris); fclose(lf); }
	}
}

static void updateCameraAndTrain(float dt) {
	float lead = G.boosting ? 90.0f : 45.0f;
	if (G.state == ST_RUN && !G.runOver) {
		G.viewLead = flerp(G.viewLead, lead, fminf(1, dt));
		G.smoothedSpeed = fclamp(flerp(G.smoothedSpeed, G.boosting ? 0.0f : G.speed * 0.6f, fminf(1, dt)), 0, 90);
	}
	static const float rate[3] = { 0.3f, 0.2f, 0.1f };
	float target = G.runOver ? 0.0f : (G.meshTilt - (G.meshTilt >= 0 ? 1.0f : -1.0f) * fminf(fabsf(G.meshTilt), 0.05f));
	for (int i = 0; i < 3; i++) {
		float k = 1.0f - powf(1.0f - rate[i], dt * 50.0f);
		G.slaveTilt[i] = flerp(G.slaveTilt[i], target, k);
	}
}

// ---- the track as a turntable model (used by the ride fact-sheet screen)
static float orbYaw = 0.6f, orbPitch = 0.45f;
static void drawTrackModel(void) {
	float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
	for (int i = 0; i < nPts; i += 4) for (int c = 0; c < 3; c++) { float v = tdata[i * 9 + c]; if (v < mn[c]) mn[c] = v; if (v > mx[c]) mx[c] = v; }
	V3 ctr = v3((mn[0] + mx[0]) * 0.5f, (mn[1] + mx[1]) * 0.5f, (mn[2] + mx[2]) * 0.5f);
	float rad = sqrtf((mx[0] - mn[0]) * (mx[0] - mn[0]) + (mx[2] - mn[2]) * (mx[2] - mn[2])) * 0.5f + 40.0f;
	float dist = rad * 1.05f;
	V3 eye = v3(ctr.x + dist * cosf(orbPitch) * sinf(orbYaw), ctr.y + dist * sinf(orbPitch), ctr.z + dist * cosf(orbPitch) * cosf(orbYaw));
	viewM = m4_lookat(eye, ctr, v3(0, 1, 0));
	M4 ident = m4_identity();
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	alphaState = -1; curAnim = -1; lastTex = -1; drawCalls = 0; drawTris = 0;
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightK, 1.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tint, 0.0f, 0.0f, 0.0f, 1.0f);
	for (int i = 0; i < nMeshes; i++) {
		const Mesh* m = &meshes[i];
		if (m->kind != 0 && m->kind != 3) continue;
		int var = (int)(m->variant & 0xFF), lod = (int)((m->variant >> 8) & 0xFF);
		if (m->kind == 0 && lod != 0) continue;
		if (m->section != 0 && var != variantSel[m->section]) continue;
		if (m->kind == 0) {   // the rails: flat Smiler yellow with a little shading (the white texture is T_white = 200 in ui_gen.h)
			texForce = 200; C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightK, 0.25f, 0.0f, 0.0f, 0.0f); C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tint, 0.85f, 0.72f, 0.0f, 1.0f);
			drawMesh(m, &ident);
			texForce = -1; C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightK, 1.0f, 0.0f, 0.0f, 0.0f); C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tint, 0.0f, 0.0f, 0.0f, 1.0f);
		} else drawMesh(m, &ident);
	}
	// the Marmaliser itself: its machinery and every leg upgrade, spinning
	{ static u64 t1 = 0; if (!t1) t1 = osGetTime(); animT = (float)(osGetTime() - t1) / 1000.0f; }
	for (int i = 0; i < nMeshes; i++) {
		const Mesh* m = &meshes[i];
		if (m->kind == 2 && texMarm[m->tex]) { drawMesh(m, &ident); continue; }
		if (m->kind != 5) continue;
		int sp = (int)((m->variant >> 8) & 0xFF); M4 model = ident;
		if (sp > 0 && sp <= nLegSpin) {
			const LegSpin* L = &legSpin[sp - 1]; V3 pv = v3(L->pv[0], L->pv[1], L->pv[2]);
			M4 rot = m4_axis_angle(v3(L->ax[0], L->ax[1], L->ax[2]), fmodf(L->dps * animT, 360.0f) * (float)M_PI / 180.0f);
			model = m4_mul(m4_translate(pv.x, pv.y, pv.z), m4_mul(rot, m4_translate(-pv.x, -pv.y, -pv.z)));
		}
		drawMesh(m, &model);
	}
}

#include "ui.h"
#include "audio.h"
#include "achieve.h"
#include "creep.h"
#include "fx.h"
#include "legfx.h"
#include "tunneldata.h"

// ---------------------------------------------------------------- shop (bottom screen)
static int shopPage = 0, shopSel = 0;
static char shopMsg[40] = "";
static int pageFirst(int p) { return p == 0 ? 0 : N_TRACK_UPGRADES; }
static int pageCount(int p) { return p == 0 ? N_TRACK_UPGRADES : N_UPGRADES - N_TRACK_UPGRADES; }

static void shopBuy(void) {
	int i = pageFirst(shopPage) + shopSel;
	const UpgradeDef* u = &UPGRADES[i];
	if (upLevel[i] >= u->maxLevel) { snprintf(shopMsg, sizeof shopMsg, "Already fully upgraded."); return; }
	if (upLevel[i] >= effMax(i)) {
		if (credits < UNLOCK_COST) { snprintf(shopMsg, sizeof shopMsg, "Need %d credits to unlock.", UNLOCK_COST); return; }
		credits -= UNLOCK_COST; upUnlocked[i] = 1; saveGame();
		snprintf(shopMsg, sizeof shopMsg, "Unlocked! You can buy it now."); return;
	}
	int cost = u->costs[upLevel[i]];
	if (credits < cost) { snprintf(shopMsg, sizeof shopMsg, "Not enough credits (%d).", cost); return; }
	credits -= cost; upLevel[i]++; applyUpgrades();
	if (i < N_TRACK_UPGRADES) { achAdd(A_JUNIOR, 1); if (upLevel[i] == 1) achAdd(A_PRO, 1); }
	else { achAdd(A_STAGE1, 1); if (upLevel[i] == 3) achAdd(A_STAGE5, 1); }
	saveGame();
	snprintf(shopMsg, sizeof shopMsg, "Bought %.24s!", u->display);
	int ct = camTrackFor(u->name);   // play the upgrade's own fly-by on the top screen
	if (ct >= 0) { shopCutTrack = ct; shopCutUp = i; shopCutT = 0; droneStart(ct); }
}

#include "screens.h"

// the upgrade fly-by: the ride seen from its scenic camera, with the train waiting in the station
static void drawCutWorld(void) {
	__typeof__(G) keep = G;
	G.dist = 0.1f; G.speed = 0; G.meshTilt = 0; for (int k = 0; k < 3; k++) G.slaveTilt[k] = 0; G.viewLead = 10.77f;
	camSample(shopCutTrack, shopCutT, &camEye, &camTgt, &camRoll); camOn = true;
	drawWorld();
	camOn = false; G = keep;
}

// ---------------------------------------------------------------- main
extern u32 __ctru_heap_size, __ctru_linear_heap_size;
static void bootLog(const char* what) {   // writes boot.txt so a crash shows how far startup got and how much memory was left
	static bool first = true;
	if (first) { mkdir("sdmc:/3ds", 0777); mkdir(SAVE_DIR, 0777); }
	FILE* f = fopen(SAVE_DIR "/boot.txt", first ? "w" : "a"); first = false;
	if (!f) return;
	fprintf(f, "%-12s linear free %u KB | app mem free %u KB | heap %u KB linear %u KB\n", what, (unsigned)(linearSpaceFree() >> 10),
		(unsigned)(osGetMemRegionFree(MEMREGION_APPLICATION) >> 10), (unsigned)(__ctru_heap_size >> 10), (unsigned)(__ctru_linear_heap_size >> 10));
	fclose(f);
}
int main(int argc, char* argv[]) {
	bootLog("start");
	gfxInitDefault();
	bootLog("gfx");
	osSetSpeedupEnable(true);   // New 3DS: run the CPU at full speed (ignored on the old 3DS)
	romfsInit();
	bootLog("romfs");
	C3D_Init(0x100000);
	bootLog("c3d");   // 1 MB GPU command buffer (the default 256 KB is too small for busy scenes)

	topTarget = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(topTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	topTargetR = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(topTargetR, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	vsh_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vsh_dvlb->DVLE[0]);
	C3D_BindProgram(&program);
	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLoc_modelView = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	uLoc_uvxf = shaderInstanceGetUniformLocation(program.vertexShader, "uvxf");
	uLoc_lightK = shaderInstanceGetUniformLocation(program.vertexShader, "lightK");
	uLoc_tint = shaderInstanceGetUniformLocation(program.vertexShader, "tint");

	C3D_AttrInfo* attr = C3D_GetAttrInfo(); AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);  // position
	AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2);  // texcoord
	AttrInfo_AddLoader(attr, 2, GPU_FLOAT, 3);  // normal

	C3D_TexEnv* env = C3D_GetTexEnv(0); C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

	Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(CAM_FOV_DEG), C3D_AspectRatioTop, 1.0f, 2600.0f, true);
	uiProjInit();

	bootLog("shaders");
	bool ok = loadTrack(); bootLog("track");
	ok = ok && loadMeshes(); bootLog("meshes");
	ok = ok && loadTextures(); bootLog("textures");
	ok = ok && loadFonts(); bootLog("fonts");
	ok = ok && uiInit(); bootLog("ui");
	loadLegs(); loadDrones(); loadTunnels(); loadAnim(); loadCams(); loadEndCam(); loadArms(); armReset(); loadBirds(); bootLog("extras");
	if (!ok) {   // show the reason on the bottom screen as plain text
		consoleInit(GFX_BOTTOM, NULL);
		printf("\x1b[1;1H LOAD ERROR:\n %s\n\n Did you run build_textures.sh?\n\n Press START to quit.\n", errmsg);
		while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
		gfxExit(); return 0;
	}
	botTarget = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(botTarget, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	resetGame();
	loadGame(); applyUpgrades(); G.topScore = (float)savedTopScore;
	bootLog("save");
	audioInit();
	bootLog("audio");
	HIDUSER_EnableAccelerometer();
	achSet(A_HOARD, (float)credits);

	u64 last = osGetTime();
	sceneEnter(SC_SPLASH);
	while (aptMainLoop() && !wantQuit) {
		hidScanInput();
		u32 down = hidKeysDown(), held = hidKeysHeld();
		u64 now = osGetTime(); float dt = (float)(now - last) / 1000.0f; last = now;
		if (dt > 0.25f) dt = 0.25f;
		uiClock += dt;
		uiPoll();

		// fade out, switch scene, fade in
		if (nextScene >= 0) { fade += dt * 5.0f; if (fade >= 1.0f) { fade = 1.0f; int s = nextScene; nextScene = -1; sceneEnter(s); } }
		else if (fade > 0) { fade -= dt * 4.0f; if (fade < 0) fade = 0; }
		uiBlocked = nextScene >= 0 || fade > 0.6f;
		if (uiBlocked) { down = 0; tUp = false; }

		if (scene == SC_SHOP && shopCutT >= 0) {   // B skips the fly-by
			if (down & KEY_B) { shopCutT = -1; droneStop(); down &= ~KEY_B; }
			else { shopCutT += dt; droneUpdate(dt); if (shopCutT >= camTracks[shopCutTrack].total) { shopCutT = -1; droneStop(); } }
		}
		sceneUpdate(dt, down, held);

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		uiFrameStart();
		// stereoscopic 3D: only while the top screen shows a 3D scene, and only when the slider is up
		bool top3D = (scene == SC_SHOP && shopCutT >= 0) || sceneHasWorld() || sceneHasModel();
		float slider = osGet3DSliderState();
		bool stereo = top3D && slider > 0.03f;
		gfxSet3D(stereo);
		for (int eye = 0; eye < (stereo ? 2 : 1); eye++) {
			float iod = stereo ? (eye == 0 ? -1.0f : 1.0f) * slider * STEREO_IOD : 0.0f;
			if (stereo) Mtx_PerspStereoTilt(&projection, C3D_AngleFromDegrees(CAM_FOV_DEG), C3D_AspectRatioTop, 1.0f, 2600.0f, iod, STEREO_FOCUS, true);
			else Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(CAM_FOV_DEG), C3D_AspectRatioTop, 1.0f, 2600.0f, true);
			C3D_RenderTarget* tg = eye == 0 ? topTarget : topTargetR;
			C3D_RenderTargetClear(tg, C3D_CLEAR_ALL, sceneHasModel() ? 0x1A1C0AFF : CLEAR_COLOR, 0);
			C3D_FrameDrawOn(tg);
			if (scene == SC_SHOP && shopCutT >= 0) drawCutWorld();
			else if (sceneHasWorld()) {
				camOn = false;
				if (scene == SC_RIDE && G.state == ST_READY && readyTrack >= 0) { camSample(readyTrack, readyT, &camEye, &camTgt, &camRoll); camOn = true; }   // scenic fly-by before launch
				else if (endCamLatched && endCamOk) { endCamSet(); camOn = true; }                                                                              // the station camera at the end
				drawWorld(); camOn = false;
			}
			else if (sceneHasModel()) { uiBeginScreen(0); uiWhite(1); uiRect(T_gui_bg_Blueprint, 0, 0, 400, 240); uiEndScreen(); drawTrackModel(); }
			uiBeginScreen(0); drawTopScreen(); uiEndScreen();
		}
		C3D_RenderTargetClear(botTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
		C3D_FrameDrawOn(botTarget);
		uiBeginScreen(1); drawBottomScreen(down); uiEndScreen();
		uiEndFrame();
		C3D_FrameEnd(0);
	}

	vidClose(); if (vidTexOk) C3D_TexDelete(&vidTex);
	audioExit();
	if (musicClip.data) linearFree(musicClip.data);
	for (int i = 0; i < N_SFX; i++) if (sfx[i].data) linearFree(sfx[i].data);
	for (int i = 0; i < nMeshes; i++) linearFree(meshes[i].verts);
	for (int i = 0; i < nTex; i++) C3D_TexDelete(&textures[i]);
	for (int k = 0; k < nSecDefs; k++) for (int v = 0; v < secDefs[k].nVar; v++) free(secDefs[k].var[v].data);
	free(tdata);
	shaderProgramFree(&program); DVLB_Free(vsh_dvlb);
	C3D_Fini(); romfsExit(); gfxExit();
	return 0;
}
