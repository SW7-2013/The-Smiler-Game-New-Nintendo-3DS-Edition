// Drone trains: while the scenic cameras play, extra trains run around the Smiler (and one cart around Oblivion), with the
// same physics as the original CartDrone: gravity along the track, chain lift minimum speed, brakes. Some cameras follow them.
#pragma once
#define MAX_SPAWN 32
#define MAX_DRONE 8
typedef struct { int cam; float start[3], end[3], speed; u32 flags; float cen[3], ax[9], half[3]; } DroneSpawn;
static DroneSpawn dsp[MAX_SPAWN]; static int nDsp = 0;
typedef struct { float len, mult, crankSpeed, brakeSpeed, kickin; u32 flags, n; float* data; float start; } OblSec;
static OblSec oblSec[8]; static int nOblSec = 0; static float oblLen = 0;
typedef struct { bool obl; float dist, speed; } Drone;
static Drone drones[MAX_DRONE]; static int nDrones = 0; static int droneFollowIdx = -1;
static bool droneFollow = false; static V3 dronePos;
static LegSpin animSpin[8]; static int nAnimSpin = 0;
static bool loadAnim(void) {
	FILE* f = fopen("romfs:/anim.bin", "rb"); if (!f) return false;
	char mg[4]; u32 n = 0;
	if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "ANM1", 4) || fread(&n, 4, 1, f) != 1 || n > 8 || fread(animSpin, sizeof(LegSpin), n, f) != n) { fclose(f); return false; }
	fclose(f); nAnimSpin = (int)n; return true;
}

static bool loadDrones(void) {
	FILE* f = fopen("romfs:/drones.bin", "rb"); if (!f) return false;
	char mg[4]; u32 n = 0;
	if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "DRN1", 4) || fread(&n, 4, 1, f) != 1 || n > MAX_SPAWN) { fclose(f); return false; }
	for (u32 i = 0; i < n; i++) if (fread(&dsp[i], sizeof(DroneSpawn), 1, f) != 1) { fclose(f); return false; }
	fclose(f); nDsp = (int)n;
	f = fopen("romfs:/obl.bin", "rb"); if (!f) return true;   // the Smiler's own drones still work without Oblivion
	if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "OBL1", 4) || fread(&n, 4, 1, f) != 1 || n > 8) { fclose(f); return true; }
	float acc = 0; nOblSec = 0;
	for (u32 i = 0; i < n; i++) {
		OblSec* o = &oblSec[i];
		if (fread(o, 4, 7, f) != 7) break;   // len, mult, crankSpeed, brakeSpeed, kickin, flags, n
		o->data = (float*)malloc((size_t)o->n * 9 * sizeof(float));
		if (!o->data || fread(o->data, 4, (size_t)o->n * 9, f) != (size_t)o->n * 9) break;
		o->start = acc; acc += o->len; nOblSec = (int)i + 1;
	}
	oblLen = acc; fclose(f);
	if (nOblSec != (int)n) { nOblSec = 0; oblLen = 0; }
	return true;
}
static float dWrap(float d, float len) { if (len <= 1) return 0; d = fmodf(d, len); return d < 0 ? d + len : d; }
static int oblSecAt(float d) { for (int i = nOblSec - 1; i >= 0; i--) if (d >= oblSec[i].start) return i; return 0; }
static Frame oblFrameAt(float d) {
	d = dWrap(d, oblLen); const OblSec* o = &oblSec[oblSecAt(d)]; float ld = fminf(d - o->start, (float)(o->n - 1) - 0.001f); if (ld < 0) ld = 0;
	int i = (int)ld; float t = ld - (float)i; const float* a = &o->data[i * 9]; const float* b = &o->data[(i + 1) * 9];
	Frame fr; fr.pos = v3(flerp(a[0], b[0], t), flerp(a[1], b[1], t), flerp(a[2], b[2], t));
	fr.fwd = vnorm(v3(flerp(a[3], b[3], t), flerp(a[4], b[4], t), flerp(a[5], b[5], t)));
	V3 up = v3(flerp(a[6], b[6], t), flerp(a[7], b[7], t), flerp(a[8], b[8], t)); up = vnorm(vsub(up, vmul(fr.fwd, vdot(up, fr.fwd))));
	fr.up = up; fr.right = vcross(up, fr.fwd); return fr;
}
static float dLen(bool obl) { return obl ? oblLen : trackLen; }
static Frame dFrame(bool obl, float d) { return obl ? oblFrameAt(d) : frameAt(dWrap(d, trackLen)); }
static float droneLookUp(bool obl, V3 p) {   // where on the track is this point? (the original's ReverseLookUp: nearest point, 2 units apart)
	float best = 1e30f, bd = 0, len = dLen(obl);
	for (float d = 0; d < len; d += 2.0f) { Frame fr = dFrame(obl, d); V3 q = vsub(fr.pos, p); float e = vdot(q, q); if (e < best) { best = e; bd = d; } }
	return bd;
}
static void droneStop(void) { nDrones = 0; droneFollow = false; droneFollowIdx = -1; }
static void droneStart(int cam) {   // the scenic camera 'cam' has just started: spawn its drones
	droneStop();
	for (int i = 0; i < nDsp && nDrones < MAX_DRONE; i++) {
		const DroneSpawn* s = &dsp[i]; if (s->cam != cam || (s->flags & 4)) continue;
		bool obl = (s->flags & 2) != 0; if (obl && nOblSec == 0) continue;
		Drone* k = &drones[nDrones]; k->obl = obl; k->speed = s->speed; k->dist = droneLookUp(obl, v3(s->start[0], s->start[1], s->start[2]));
		if (s->flags & 1) droneFollowIdx = nDrones;   // the camera looks at this train instead of its own path
		nDrones++;
	}
	if (droneFollowIdx >= 0) { droneFollow = true; dronePos = dFrame(drones[droneFollowIdx].obl, drones[droneFollowIdx].dist).pos; }
}
static void droneStep(Drone* k, float dt) {
	Frame fr = dFrame(k->obl, k->dist);
	k->speed += fr.fwd.y * GRAVITY_Y * dt; k->dist += k->speed * dt;   // the distance keeps counting up; everything below wraps it round the lap
	float wd = dWrap(k->dist, dLen(k->obl));
	u32 flags; float crankSpeed, brakeSpeed, toEnd;
	if (k->obl) { const OblSec* o = &oblSec[oblSecAt(wd)]; flags = o->flags; crankSpeed = o->crankSpeed; brakeSpeed = o->brakeSpeed; toEnd = o->start + o->len - wd; }
	else { const Section* s = &secs[sectionAt(wd)]; flags = s->flags; crankSpeed = s->crankSpeed; brakeSpeed = s->brakeSpeed; toEnd = s->start + s->len - wd; }
	if ((flags & 1) && k->speed < crankSpeed) k->speed = crankSpeed;
	if (flags & 2) { if (k->speed > brakeSpeed) k->speed -= (k->speed - brakeSpeed) / fmaxf(toEnd, 1.0f) * dt * k->speed; else k->speed = brakeSpeed; }
	if (k->speed < 0) k->speed = 0;
}
static void droneUpdate(float dt) {
	if (!nDrones) return;
	while (dt > 0) { float h = fminf(dt, 0.02f); for (int i = 0; i < nDrones; i++) droneStep(&drones[i], h); dt -= h; }
	if (droneFollowIdx >= 0) dronePos = dFrame(drones[droneFollowIdx].obl, drones[droneFollowIdx].dist).pos;
}

// ---- trains that appear during the ride: passing a trigger zone sets one off, and it runs until it reaches its end point
typedef struct { Drone d; bool on, wasIn; float die; } RideDrone;
static RideDrone rdr[MAX_SPAWN];
static void rideDronesReset(void) { memset(rdr, 0, sizeof rdr); }
static bool dronePointIn(const DroneSpawn* s, V3 p) {
	float d[3] = { p.x - s->cen[0], p.y - s->cen[1], p.z - s->cen[2] };
	for (int i = 0; i < 3; i++) { float l = d[0] * s->ax[i * 3] + d[1] * s->ax[i * 3 + 1] + d[2] * s->ax[i * 3 + 2]; if (fabsf(l) > s->half[i]) return false; }
	return true;
}
static void rideDronesUpdate(float dt, V3 playerPos) {
	for (int i = 0; i < nDsp; i++) {
		const DroneSpawn* s = &dsp[i]; if (!(s->flags & 4)) continue;
		RideDrone* r = &rdr[i]; bool in = dronePointIn(s, playerPos);
		if (in && !r->wasIn) {   // the player's cart has just entered the zone: (re)start the train
			bool obl = (s->flags & 2) != 0; if (!obl || nOblSec) {
				r->d.obl = obl; r->d.speed = s->speed; r->d.dist = droneLookUp(obl, v3(s->start[0], s->start[1], s->start[2]));
				r->die = droneLookUp(obl, v3(s->end[0], s->end[1], s->end[2])); if (r->die < r->d.dist) r->die += dLen(obl);   // the end point is ahead of the start, possibly past the finish line
				r->on = true;
			}
		}
		r->wasIn = in;
		if (r->on) {
			float left = dt; while (left > 0) { float h = fminf(left, 0.02f); droneStep(&r->d, h); left -= h; }
			if (r->d.dist > r->die) r->on = false;
		}
	}
}
