// Leg upgrade data (legs.bin): spinning parts, trigger boxes and effect curves. Included by main.c before drawing.
#pragma once
typedef struct { float pv[3], ax[3], dps; } LegSpin;
typedef struct { int n; float* k; } LegCurve;    // keys: time, value, inSlope, outSlope
typedef struct { u32 leg; float c[3]; float ax[9]; float half[3]; float pts[4]; int curve[4]; float strobe; float flashes[4]; } LegTrig;
#define LEG_MAXSPIN 16
#define LEG_MAXCURVE 16
#define LEG_MAXTRIG 12
static LegSpin legSpin[LEG_MAXSPIN]; static int nLegSpin = 0;
static LegCurve legCurve[LEG_MAXCURVE]; static int nLegCurve = 0, legLaughCurve = -1;
static LegTrig legTrig[LEG_MAXTRIG]; static int nLegTrig = 0;

static bool loadLegs(void) {   // optional: the game still runs without legs.bin
	FILE* f = fopen("romfs:/legs.bin", "rb"); if (!f) return false;
	char m[4]; u32 n;
	if (fread(m, 1, 4, f) != 4 || memcmp(m, "LEG1", 4) || fread(&n, 4, 1, f) != 1 || n > LEG_MAXSPIN) { fclose(f); return false; }
	for (u32 i = 0; i < n; i++) if (fread(&legSpin[i], sizeof(LegSpin), 1, f) != 1) { fclose(f); return false; }
	nLegSpin = (int)n;
	if (fread(&n, 4, 1, f) != 1 || n > LEG_MAXCURVE) { fclose(f); return false; }
	for (u32 i = 0; i < n; i++) {
		u32 k; if (fread(&k, 4, 1, f) != 1 || k > 64) { fclose(f); return false; }
		legCurve[i].k = (float*)malloc(sizeof(float) * 4 * (k ? k : 1)); legCurve[i].n = (int)k;
		if (fread(legCurve[i].k, 16, k, f) != k) { fclose(f); return false; }
	}
	nLegCurve = (int)n;
	u32 nt; int lc;
	if (fread(&nt, 4, 1, f) != 1 || fread(&lc, 4, 1, f) != 1 || nt > LEG_MAXTRIG) { fclose(f); return false; }
	for (u32 i = 0; i < nt; i++) if (fread(&legTrig[i], sizeof(LegTrig), 1, f) != 1) { fclose(f); return false; }
	nLegTrig = (int)nt; legLaughCurve = lc;
	fclose(f); return true;
}
static float legCurveEnd(int id) { return (id < 0 || id >= nLegCurve || !legCurve[id].n) ? 0.0f : legCurve[id].k[(legCurve[id].n - 1) * 4]; }
static float legCurveEval(int id, float t) {   // Unity-style hermite between keyframes, clamped at the ends
	if (id < 0 || id >= nLegCurve || !legCurve[id].n) return 0;
	const float* k = legCurve[id].k; int n = legCurve[id].n;
	if (t <= k[0]) return k[1]; if (t >= k[(n - 1) * 4]) return k[(n - 1) * 4 + 1];
	int i = 0; while (i < n - 2 && t > k[(i + 1) * 4]) i++;
	const float *a = &k[i * 4], *b = &k[(i + 1) * 4]; float dt = b[0] - a[0]; if (dt < 1e-6f) return a[1];
	float u = (t - a[0]) / dt, u2 = u * u, u3 = u2 * u;
	return (2 * u3 - 3 * u2 + 1) * a[1] + (u3 - 2 * u2 + u) * dt * a[3] + (-2 * u3 + 3 * u2) * b[1] + (u3 - u2) * dt * b[2];
}
// camera sway while the Tickler is laughing (added to the eye and the target)
static float legLaughT = -1.0f;
