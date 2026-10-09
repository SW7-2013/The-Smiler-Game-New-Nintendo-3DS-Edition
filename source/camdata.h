// scenic camera tracks (the original's ViewTrack/ViewPoints): fly-bys of the ride, used for upgrade cutscenes
#pragma once
#define CAM_MAXTRACK 24
#define CAM_MAXPT 12
typedef struct { char assoc[16]; int n; float pos[CAM_MAXPT][3], look[CAM_MAXPT][3], t[CAM_MAXPT], roll[CAM_MAXPT]; float total; } CamTrack;
static CamTrack camTracks[CAM_MAXTRACK]; static int nCamTracks = 0;
static bool camOn = false; static V3 camEye, camTgt; static float camRoll = 0;   // when camOn, drawWorld looks through this camera
static float shopCutT = -1; static int shopCutTrack = -1; static int shopCutUp = -1;

static bool loadCams(void) {
	FILE* f = fopen("romfs:/cams.bin", "rb"); if (!f) return false;
	char mg[4]; unsigned n = 0;
	if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "CAM1", 4) || fread(&n, 4, 1, f) != 1) { fclose(f); return false; }
	nCamTracks = 0;
	for (unsigned i = 0; i < n; i++) {
		CamTrack tmp; memset(&tmp, 0, sizeof tmp); unsigned k = 0;
		if (fread(tmp.assoc, 1, 16, f) != 16 || fread(&k, 4, 1, f) != 1) break;
		tmp.assoc[15] = 0; float tt[CAM_MAXPT]; memset(tt, 0, sizeof tt); float acc = 0;
		for (unsigned j = 0; j < k; j++) {
			float v[8]; if (fread(v, 4, 8, f) != 8) { k = 0; break; }
			if (j < CAM_MAXPT) { for (int c = 0; c < 3; c++) { tmp.pos[j][c] = v[c]; tmp.look[j][c] = v[3 + c]; } tmp.t[j] = acc; tmp.roll[j] = v[7]; }
			acc += v[6];
		}
		if (k < 2 || k > CAM_MAXPT || nCamTracks >= CAM_MAXTRACK) continue;
		tmp.n = (int)k; tmp.total = tmp.t[k - 1]; if (tmp.total < 0.1f) tmp.total = 0.1f;
		camTracks[nCamTracks++] = tmp;
	}
	fclose(f); return nCamTracks > 0;
}
// cubic hermite through the keys, with smoothed tangents (Unity's SmoothTangents)
static float camCurve(const CamTrack* c, const float* v, int stride, float t) {
	int n = c->n; int i = 0; while (i < n - 2 && t > c->t[i + 1]) i++;
	float t0 = c->t[i], t1 = c->t[i + 1], dt = t1 - t0; if (dt < 1e-4f) return v[(i + 1) * stride];
	float v0 = v[i * stride], v1 = v[(i + 1) * stride];
	float m0 = i == 0 ? (v1 - v0) / dt : (v1 - v[(i - 1) * stride]) / (t1 - c->t[i - 1]);
	float m1 = i + 1 == n - 1 ? (v1 - v0) / dt : (v[(i + 2) * stride] - v0) / (c->t[i + 2] - t0);
	float s = fclamp((t - t0) / dt, 0, 1), s2 = s * s, s3 = s2 * s;
	return (2 * s3 - 3 * s2 + 1) * v0 + (s3 - 2 * s2 + s) * dt * m0 + (-2 * s3 + 3 * s2) * v1 + (s3 - s2) * dt * m1;
}
static void camSample(int ti, float time, V3* eye, V3* tgt, float* roll) {
	const CamTrack* c = &camTracks[ti]; float T = c->total;
	float p = fmodf(time, 2 * T); if (p > T) p = 2 * T - p;     // ping-pong
	float x = p / T, t2 = T * (x * x * (3 - 2 * x));            // smooth-step ease in and out
	*eye = v3(camCurve(c, &c->pos[0][0], 3, t2), camCurve(c, &c->pos[0][1], 3, t2), camCurve(c, &c->pos[0][2], 3, t2));
	*tgt = v3(camCurve(c, &c->look[0][0], 3, t2), camCurve(c, &c->look[0][1], 3, t2), camCurve(c, &c->look[0][2], 3, t2));
	*roll = camCurve(c, c->roll, 1, t2);
	if (droneFollow) *tgt = dronePos;   // a camera that follows a drone train looks at its lead cart
}
static int camTrackFor(const char* name) {
	for (int i = 0; i < nCamTracks; i++) if (!strcmp(camTracks[i].assoc, name)) return i;
	return -1;
}

// ---- end camera: once the train reaches the station approach the camera stops and watches it roll in
typedef struct { float pos[3], fwd[3], up[3], c[3], ax[9], half[3]; } EndCamData;
static EndCamData endCam; static bool endCamOk = false;
static bool loadEndCam(void) {
	FILE* f = fopen("romfs:/endcam.bin", "rb"); if (!f) return false;
	char mg[4]; bool ok = fread(mg, 1, 4, f) == 4 && !memcmp(mg, "END1", 4) && fread(&endCam, sizeof endCam, 1, f) == 1;
	fclose(f); endCamOk = ok; return ok;
}
static bool endInside(V3 p) {
	float d[3] = { p.x - endCam.c[0], p.y - endCam.c[1], p.z - endCam.c[2] };
	for (int i = 0; i < 3; i++) { float l = d[0] * endCam.ax[i * 3] + d[1] * endCam.ax[i * 3 + 1] + d[2] * endCam.ax[i * 3 + 2]; if (fabsf(l) > endCam.half[i]) return false; }
	return true;
}
static void endCamSet(void) {
	camEye = v3(endCam.pos[0], endCam.pos[1], endCam.pos[2]);
	V3 f = v3(endCam.fwd[0], endCam.fwd[1], endCam.fwd[2]), u = v3(endCam.up[0], endCam.up[1], endCam.up[2]);
	camTgt = vadd(camEye, vmul(f, 100.0f));
	V3 r = vnorm(vcross(v3(0, 1, 0), f)); camRoll = atan2f(vdot(u, r), vdot(u, v3(0, 1, 0))) * 180.0f / (float)M_PI;
}

// ---- before the ride: scenic fly-bys until you launch (the very first one each session is the long intro tour)
#define READY_WAIT 5.0f   // the original hides the start button for the first five seconds
static int readyTrack = -1; static float readyT = 0, readyWait = 0; static bool introPlayed = false, introActive = false;
static unsigned camRng = 12345u;
static void readyNext(void) {
	int prev = readyTrack; readyT = 0; introActive = false;
	if (!introPlayed) { int t = camTrackFor("Intro"); if (t >= 0) { readyTrack = t; introPlayed = true; introActive = true; droneStart(readyTrack); return; } }
	int cand[CAM_MAXTRACK], n = 0;
	for (int i = 0; i < nCamTracks; i++) if (strcmp(camTracks[i].assoc, "Intro") && i != prev) cand[n++] = i;
	if (!n) { readyTrack = nCamTracks > 0 ? 0 : -1; droneStart(readyTrack); return; }
	camRng = camRng * 1664525u + 1013904223u; readyTrack = cand[(camRng >> 8) % n]; droneStart(readyTrack);
}
static void readyStart(void) { camRng ^= (unsigned)osGetTime(); readyWait = 0; readyTrack = -1; readyNext(); }
static void readyUpdate(float dt) {
	readyWait += dt;
	if (readyTrack < 0) return;
	droneUpdate(dt); readyT += dt;
	if (readyT >= camTracks[readyTrack].total) readyNext();
}
