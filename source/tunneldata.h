// Tunnel lighting: the station building darkens the view when you enter and flashes bright when you leave (TunnelLighting in the original).
// The "world exposure" follows a curve; the "eye" catches up slowly; the difference is drawn as a black or white wash over the top screen.
#pragma once

typedef struct { int mode; float amb; int n[2]; float k[2][16]; float cen[3], ax[9], half[3]; bool wasIn; } Tunnel;
#define MAX_TUN 8
static Tunnel tun[MAX_TUN]; static int nTun = 0;
static float tunWorld = 0, tunEye = 0, tunAmb = 0, tunT = 0, tunTotal = 0, tunLeft = 0; static const float* tunCurve = NULL; static int tunN = 0; static float tunKeys[16];

static void loadTunnels(void) {
	FILE* f = fopen("romfs:/tunnel.bin", "rb"); if (!f) return;
	char m[4]; u32 n; if (fread(m, 1, 4, f) != 4 || memcmp(m, "TUN1", 4) || fread(&n, 4, 1, f) != 1) { fclose(f); return; }
	for (u32 i = 0; i < n && i < MAX_TUN; i++) {
		Tunnel* t = &tun[nTun]; int h[2]; float amb;
		if (fread(h, 4, 1, f) != 1 || fread(&amb, 4, 1, f) != 1 || fread(t->n, 4, 2, f) != 2 || fread(t->k, 4, 32, f) != 32 || fread(t->cen, 4, 3, f) != 3 || fread(t->ax, 4, 9, f) != 9 || fread(t->half, 4, 3, f) != 3) break;
		t->mode = h[0]; t->amb = amb; t->wasIn = false; nTun++;
	}
	fclose(f);
}
static float tunEval(const float* k, int n, float t) {   // Hermite curve through (time, value, inSlope, outSlope) keys, clamped at both ends
	if (n <= 0) return 0;
	if (t <= k[0]) return k[1];
	if (t >= k[(n - 1) * 4]) return k[(n - 1) * 4 + 1];
	for (int i = 0; i + 1 < n; i++) {
		const float* a = k + i * 4; const float* b = k + (i + 1) * 4;
		if (t >= a[0] && t <= b[0]) {
			float dt = b[0] - a[0], u = (t - a[0]) / (dt > 1e-6f ? dt : 1e-6f), u2 = u * u, u3 = u2 * u;
			return (2 * u3 - 3 * u2 + 1) * a[1] + (u3 - 2 * u2 + u) * dt * a[3] + (-2 * u3 + 3 * u2) * b[1] + (u3 - u2) * dt * b[2];
		}
	}
	return k[(n - 1) * 4 + 1];
}
static void tunReset(void) { tunWorld = tunEye = tunAmb = 0; tunLeft = 0; for (int i = 0; i < nTun; i++) tun[i].wasIn = false; }
static void tunFire(const Tunnel* t, int which, float ambient) {   // start an exposure change: from wherever it is now to the ambient level
	tunAmb = ambient;
	int n = t->n[which]; if (n < 2) return;
	memcpy(tunKeys, t->k[which], sizeof tunKeys);
	tunKeys[1] = tunWorld; tunKeys[(n - 1) * 4 + 1] = ambient;
	tunN = n; tunTotal = tunKeys[(n - 1) * 4]; tunLeft = tunTotal;
}
static bool tunPointIn(const Tunnel* t, V3 p) {
	float d[3] = { p.x - t->cen[0], p.y - t->cen[1], p.z - t->cen[2] };
	for (int i = 0; i < 3; i++) { float l = d[0] * t->ax[i * 3] + d[1] * t->ax[i * 3 + 1] + d[2] * t->ax[i * 3 + 2]; if (fabsf(l) > t->half[i]) return false; }
	return true;
}
static void tunUpdate(float dt, V3 p, bool active) {
	if (active) for (int i = 0; i < nTun; i++) {
		Tunnel* t = &tun[i]; bool in = tunPointIn(t, p);
		if (in && !t->wasIn) { if (t->mode == 1 || t->mode == 3) tunFire(t, 0, t->amb); else if (t->mode == 2) tunFire(t, 1, 0.0f); }
		else if (!in && t->wasIn && t->mode == 3) tunFire(t, 1, 0.0f);
		t->wasIn = in;
	}
	if (tunLeft > 0) { tunLeft -= dt; tunWorld = tunEval(tunKeys, tunN, tunTotal - fmaxf(tunLeft, 0)); }
	else tunWorld = tunAmb;
	float eyeTgt = fclamp(tunWorld, -0.5f, 0.5f);
	tunEye += (eyeTgt - tunEye) * fminf(1.0f, dt);
}
static void tunDrawOverlay(void) {
	float b = tunWorld - tunEye;
	if (fabsf(b) < 0.01f) return;
	if (b > 0) { uiColor(1, 1, 1, fminf(b, 1.0f)); uiFill(0, 0, 400, 240); }
	else { uiColor(0, 0, 0, fminf(-b, 1.0f)); uiFill(0, 0, 400, 240); }
	uiWhite(1);
}
