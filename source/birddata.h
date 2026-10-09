// Birds: a small flock that circles the park, chasing a target that runs round fixed waypoints (BirdSpawner / Boid in the original).
#pragma once
#define MAX_BIRD 6
#define MAX_BWP 16
typedef struct { V3 pos, fwd; float phase; } Bird;
static V3 bwp[MAX_BWP]; static float bcum[MAX_BWP]; static int nBwp = 0, nBirds = 0;
static float bMoveSpeed = 43, bArea = 50, bTotal = 1, bTargetT = 0; static V3 bTarget; static Bird birds[MAX_BIRD];
static u32 bRng = 12345;
static float bRand(void) { bRng = bRng * 1664525u + 1013904223u; return (float)((bRng >> 8) & 0xFFFF) / 65535.0f; }

static V3 bCatmull(V3 a, V3 b, V3 c, V3 d, float t) {
	float t2 = t * t, t3 = t2 * t;
	return v3(0.5f * ((2 * b.x) + (-a.x + c.x) * t + (2 * a.x - 5 * b.x + 4 * c.x - d.x) * t2 + (-a.x + 3 * b.x - 3 * c.x + d.x) * t3),
	          0.5f * ((2 * b.y) + (-a.y + c.y) * t + (2 * a.y - 5 * b.y + 4 * c.y - d.y) * t2 + (-a.y + 3 * b.y - 3 * c.y + d.y) * t3),
	          0.5f * ((2 * b.z) + (-a.z + c.z) * t + (2 * a.z - 5 * b.z + 4 * c.z - d.z) * t2 + (-a.z + 3 * b.z - 3 * c.z + d.z) * t3));
}
static V3 birdPath(float t) {   // smooth path through the waypoints, travelled back and forth at moveSpeed
	float m = fmodf(t, bTotal * 2.0f); if (m > bTotal) m = bTotal * 2.0f - m;
	int i = 0; while (i + 2 < nBwp && m > bcum[i + 1]) i++;
	float seg = bcum[i + 1] - bcum[i]; float u = seg > 1e-4f ? fclamp((m - bcum[i]) / seg, 0, 1) : 0;
	int i0 = i > 0 ? i - 1 : 0, i3 = i + 2 < nBwp ? i + 2 : nBwp - 1;
	return bCatmull(bwp[i0], bwp[i], bwp[i + 1], bwp[i3], u);
}
static V3 bTurn(V3 h, V3 want, float maxAng) {   // turn the heading towards 'want' by at most maxAng radians
	float c = fclamp(vdot(h, want), -1, 1), a = acosf(c); if (a < 1e-4f) return h;
	float t = fminf(1.0f, maxAng / a); V3 r = vadd(vmul(h, 1 - t), vmul(want, t));
	float l = sqrtf(vdot(r, r)); return l > 1e-5f ? vmul(r, 1.0f / l) : h;
}
static void birdsInit(void) {
	if (nBwp < 2) { nBirds = 0; return; }
	bTargetT = 0; bTarget = birdPath(0); nBirds = MAX_BIRD;
	V3 d0 = vnorm(vsub(bwp[1], bwp[0]));
	for (int i = 0; i < nBirds; i++) {
		birds[i].pos = v3(bwp[0].x + (bRand() - 0.5f) * bArea, bwp[0].y + (bRand() - 0.5f) * bArea * 0.5f, bwp[0].z + (bRand() - 0.5f) * bArea);
		birds[i].fwd = d0; birds[i].phase = bRand() * 4.0f;
	}
}
static void loadBirds(void) {
	FILE* f = fopen("romfs:/birds.bin", "rb"); if (!f) return;
	char m[4]; float h[2]; u32 hc[2];
	if (fread(m, 1, 4, f) != 4 || memcmp(m, "BRD1", 4) || fread(h, 4, 2, f) != 2 || fread(hc, 4, 2, f) != 2 || hc[1] < 2 || hc[1] > MAX_BWP) { fclose(f); return; }
	bMoveSpeed = h[0]; bArea = h[1];
	for (u32 i = 0; i < hc[1]; i++) { float p[3]; if (fread(p, 4, 3, f) != 3) { fclose(f); return; } bwp[i] = v3(p[0], p[1], p[2]); }
	nBwp = (int)hc[1]; fclose(f);
	bcum[0] = 0; for (int i = 1; i < nBwp; i++) { V3 d = vsub(bwp[i], bwp[i - 1]); bcum[i] = bcum[i - 1] + sqrtf(vdot(d, d)) / bMoveSpeed; }
	bTotal = bcum[nBwp - 1] > 1.0f ? bcum[nBwp - 1] : 1.0f;
	birdsInit();
}
static void birdsUpdate(float dt) {
	if (!nBirds) return;
	if (dt > 0.1f) dt = 0.1f;
	bTargetT += dt; bTarget = birdPath(bTargetT);
	V3 avg = v3(0, 0, 0); for (int i = 0; i < nBirds; i++) avg = vadd(avg, birds[i].fwd);
	float al = sqrtf(vdot(avg, avg)); if (al > 1e-4f) avg = vmul(avg, 1.0f / al);
	for (int i = 0; i < nBirds; i++) {
		Bird* b = &birds[i]; float best = 1e30f; V3 cl = b->pos;
		for (int j = 0; j < nBirds; j++) if (j != i) { V3 d = vsub(b->pos, birds[j].pos); float q = vdot(d, d); if (q < best) { best = q; cl = birds[j].pos; } }
		const float R = 1.5707963f;
		if (best < 400.0f) b->fwd = bTurn(b->fwd, vnorm(vsub(b->pos, cl)), R * dt);   // too close to another bird: veer away
		if (al > 1e-4f) b->fwd = bTurn(b->fwd, avg, 0.349f * dt);                      // follow the flock's average heading
		V3 to = vsub(bTarget, b->pos); float tl = sqrtf(vdot(to, to)); if (tl > 1e-3f) b->fwd = bTurn(b->fwd, vmul(to, 1.0f / tl), R * dt);
		b->pos = vadd(b->pos, vmul(b->fwd, 60.0f * dt));
	}
}
static void birdsDraw(const V3 eye) {
	static const int SEQ[4] = { 0, 1, 2, 1 };
	for (int i = 0; i < nBirds; i++) {
		const Bird* b = &birds[i];
		V3 d = vsub(b->pos, eye); if (vdot(d, d) > 900.0f * 900.0f) continue;
		V3 fw = b->fwd, upw = v3(0, 1, 0); V3 rt = vcross(upw, fw); float rl = sqrtf(vdot(rt, rt));
		if (rl < 1e-3f) rt = v3(1, 0, 0); else rt = vmul(rt, 1.0f / rl);
		V3 up = vcross(fw, rt);
		M4 model = m4_frame(rt, up, fw, b->pos);
		int frame = SEQ[((int)(animT * 9.0f + b->phase)) & 3];
		for (int k = 0; k < nMeshes; k++) if (meshes[k].kind == 10 && (int)(meshes[k].variant & 0xFF) == frame) drawMesh(&meshes[k], &model);
	}
}
