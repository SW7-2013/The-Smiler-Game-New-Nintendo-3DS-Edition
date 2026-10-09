// The riders' arms: each of the 4 carts has 4 riders, each rider two arm bones. Arms are skinned on the CPU every frame
// and drawn with the cart's matrix. Behaviour follows the original Man script.
#pragma once
#define ARM_CARTS 4
#define ARM_MAXBONE 8
typedef struct { float pos[3], uv[2], n[3]; u8 bi[4]; float w[4]; } ArmVert;
typedef struct { float P[16], rot[4], pos[3], scl[3], bind[16]; int side, man; } ArmBone;
static ArmVert* armSrc = NULL; static ArmBone armBone[ARM_MAXBONE]; static int armNV = 0, armNI = 0, armTex = 0, armNB = 0;
static u16* armIdx = NULL; static Vertex* armOut[ARM_CARTS]; static Mesh armMesh;
static float armQ[ARM_CARTS][ARM_MAXBONE][4]; static int armOrder[16];
static unsigned armRng = 777u;

static void qmul(const float* a, const float* b, float* o) {   // quaternions as x,y,z,w
	float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
	float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
	o[0] = x; o[1] = y; o[2] = z; o[3] = w;
}
static void qeuler(float xd, float yd, float zd, float* o) {   // Unity order: Z, then X, then Y
	float k = (float)M_PI / 360.0f, qx[4] = { sinf(xd * k), 0, 0, cosf(xd * k) }, qy[4] = { 0, sinf(yd * k), 0, cosf(yd * k) }, qz[4] = { 0, 0, sinf(zd * k), cosf(zd * k) }, t[4];
	qmul(qx, qz, t); qmul(qy, t, o);
}
static void qslerp(const float* a, const float* b, float t, float* o) {
	float bb[4] = { b[0], b[1], b[2], b[3] }, d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	if (d < 0) { d = -d; for (int i = 0; i < 4; i++) bb[i] = -bb[i]; }
	float s0, s1;
	if (d > 0.9995f) { s0 = 1 - t; s1 = t; } else { float th = acosf(d), sn = sinf(th); s0 = sinf((1 - t) * th) / sn; s1 = sinf(t * th) / sn; }
	float l = 0; for (int i = 0; i < 4; i++) { o[i] = s0 * a[i] + s1 * bb[i]; l += o[i] * o[i]; }
	l = 1.0f / sqrtf(l); for (int i = 0; i < 4; i++) o[i] *= l;
}
static M4 qmat(const float* q, const float* scl, const float* pos) {   // translate * rotate * scale
	float x = q[0], y = q[1], z = q[2], w = q[3]; M4 m = m4_identity();
	m.m[0][0] = (1 - 2 * (y * y + z * z)) * scl[0]; m.m[0][1] = 2 * (x * y - z * w) * scl[1];       m.m[0][2] = 2 * (x * z + y * w) * scl[2];
	m.m[1][0] = 2 * (x * y + z * w) * scl[0];       m.m[1][1] = (1 - 2 * (x * x + z * z)) * scl[1]; m.m[1][2] = 2 * (y * z - x * w) * scl[2];
	m.m[2][0] = 2 * (x * z - y * w) * scl[0];       m.m[2][1] = 2 * (y * z + x * w) * scl[1];       m.m[2][2] = (1 - 2 * (x * x + y * y)) * scl[2];
	m.m[0][3] = pos[0]; m.m[1][3] = pos[1]; m.m[2][3] = pos[2]; return m;
}
static M4 m4_from16(const float* f) { M4 m; for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) m.m[r][c] = f[r * 4 + c]; return m; }

static bool loadArms(void) {
	FILE* f = fopen("romfs:/arms.bin", "rb"); if (!f) return false;
	char mg[4]; u32 h[4];
	if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "ARM1", 4) || fread(h, 4, 4, f) != 4 || h[3] > ARM_MAXBONE) { fclose(f); return false; }
	armNV = (int)h[0]; armNI = (int)h[1]; armTex = (int)h[2]; armNB = (int)h[3];
	armSrc = (ArmVert*)malloc(sizeof(ArmVert) * armNV);
	size_t ib = ((size_t)armNI * 2 + 3) & ~3u;
	armIdx = (u16*)linearAlloc(ib);
	for (int c = 0; c < ARM_CARTS; c++) armOut[c] = (Vertex*)linearAlloc(sizeof(Vertex) * armNV);
	bool ok = armSrc && armIdx && armOut[0] && armOut[1] && armOut[2] && armOut[3] && fread(armSrc, sizeof(ArmVert), armNV, f) == (size_t)armNV && fread(armIdx, 1, ib, f) == ib;
	for (int b = 0; ok && b < armNB; b++) ok = fread(&armBone[b], sizeof(ArmBone), 1, f) == 1;
	fclose(f);
	if (!ok) { armNV = 0; return false; }
	GSPGPU_FlushDataCache(armIdx, ib);
	memset(&armMesh, 0, sizeof armMesh); armMesh.kind = 6; armMesh.tex = (u32)armTex; armMesh.nV = (u32)armNV; armMesh.nI = (u32)armNI; armMesh.idx = armIdx;
	return true;
}
static void armReset(void) {
	for (int c = 0; c < ARM_CARTS; c++) for (int b = 0; b < armNB; b++) memcpy(armQ[c][b], armBone[b].rot, sizeof(float) * 4);
	for (int i = 0; i < 16; i++) armOrder[i] = i;
	armRng ^= (unsigned)osGetTime();
	for (int i = 15; i > 0; i--) { armRng = armRng * 1664525u + 1013904223u; int j = (int)((armRng >> 8) % (unsigned)(i + 1)); int t = armOrder[i]; armOrder[i] = armOrder[j]; armOrder[j] = t; }   // which riders get marmalised first
}
// called while the ride is running (the original only animates the arms then)
static void armUpdate(float dt, float tiltX, int accIcon, bool crank, int smiles, float t) {
	if (!armNV) return;
	float wave = fmodf(t * 160.0f, 180.0f), pp = wave <= 90 ? wave : 180 - wave, k = fminf(dt * 4.0f, 1.0f);
	for (int c = 0; c < ARM_CARTS; c++) for (int b = 0; b < armNB; b++) {
		const ArmBone* ab = &armBone[b]; int man = c * 4 + ab->man;
		bool mar = armOrder[man] < smiles;   // this rider's rank among the marmalised
		float e[4], tg[4];
		if (mar) qeuler(0, 0, pp - 45.0f, e);
		else if (accIcon > 1 && !crank) qeuler(0, 0, 60.0f * -tiltX, e);
		else qeuler(0, -150.0f, ab->side == 0 ? -30.0f : 30.0f, e);
		qmul(ab->rot, e, tg); qslerp(armQ[c][b], tg, k, armQ[c][b]);
	}
}
static void armSkin(int c) {   // pose the arms of cart c into its own vertex buffer (cart space)
	M4 K[ARM_MAXBONE];
	for (int b = 0; b < armNB; b++) {
		const ArmBone* ab = &armBone[b];
		M4 L = qmat(armQ[c][b], ab->scl, ab->pos), W = m4_mul(m4_from16(ab->P), L); K[b] = m4_mul(W, m4_from16(ab->bind));
	}
	for (int i = 0; i < armNV; i++) {
		const ArmVert* s = &armSrc[i]; float p[3] = { 0, 0, 0 }, n[3] = { 0, 0, 0 };
		for (int j = 0; j < 4; j++) {
			float w = s->w[j]; if (w < 1e-4f) continue; const M4* k = &K[s->bi[j] < armNB ? s->bi[j] : 0];
			for (int r = 0; r < 3; r++) {
				p[r] += w * (k->m[r][0] * s->pos[0] + k->m[r][1] * s->pos[1] + k->m[r][2] * s->pos[2] + k->m[r][3]);
				n[r] += w * (k->m[r][0] * s->n[0] + k->m[r][1] * s->n[1] + k->m[r][2] * s->n[2]);
			}
		}
		float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]); if (nl < 1e-6f) nl = 1;
		Vertex* o = &armOut[c][i]; o->x = p[0]; o->y = p[1]; o->z = p[2]; o->u = s->uv[0]; o->v = s->uv[1]; o->nx = n[0] / nl; o->ny = n[1] / nl; o->nz = n[2] / nl;
	}
	GSPGPU_FlushDataCache(armOut[c], sizeof(Vertex) * armNV);
}
static void armDraw(int c, const M4* model) {
	if (!armNV) return;
	armSkin(c); armMesh.verts = armOut[c]; drawMesh(&armMesh, model);
}
