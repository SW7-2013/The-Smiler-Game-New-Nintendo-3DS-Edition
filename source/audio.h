// Sound: one music channel plus a few sound-effect channels (3DS DSP). Everything is silent if dspfirm.cdc is missing.
#pragma once

typedef struct { void* data; u32 bytes; int rate; } Clip;
enum { CH_MUSIC = 0, CH_RAIL, CH_BOOST, CH_ONE, CH_CRANK, CH_GRIND, CH_NOISE, CH_LEG, CH_CHEER };
enum { SFX_RAIL, SFX_BOOSTLOOP, SFX_BOOSTEND, SFX_CRANK, SFX_GRIND, SFX_ACHIEVE, SFX_NOISE, SFX_GAS, SFX_INOC, SFX_FLASH, SFX_FLASHOUT, SFX_LAUGH, SFX_SCREAM, N_SFX };
static const char* SFX_FILE[N_SFX] = { "pitch_shiftRailSound2d", "boost_loop", "boost_end", "crankUp", "rawGrinding_b", "AchievementEarned", "interferance", "gasser_leg", "innoculator_leg", "flasher_single", "flasher_outro", "Tickler_test", "screamLoop" };

static bool dspOn = false;
static Clip sfx[N_SFX], musicClip; static char musicName[32] = "";
static ndspWaveBuf chBuf[9]; static bool chBusy[9];
static float musicGain = 0.45f;   // music sits under the sound effects
#define SFX_BOOST_GAIN 1.9f          // sound effects are mastered quietly: lift them
static float chGain(int ch, float v) { return ch == CH_MUSIC ? v : fminf(1.0f, v * SFX_BOOST_GAIN); }

static bool clipLoad(Clip* c, const char* name) {
	char path[96]; snprintf(path, sizeof path, "romfs:/snd/%s.raw", name);
	FILE* f = fopen(path, "rb"); if (!f) return false;
	char m[4]; u32 h[3];
	if (fread(m, 1, 4, f) != 4 || memcmp(m, "MUS1", 4) || fread(h, 4, 3, f) != 3) { fclose(f); return false; }
	void* d = linearAlloc(h[2]);
	if (!d || fread(d, 1, h[2], f) != h[2]) { if (d) linearFree(d); fclose(f); return false; }
	fclose(f);
	GSPGPU_FlushDataCache(d, h[2]); DSP_FlushDataCache(d, h[2]);
	c->data = d; c->bytes = h[2]; c->rate = (int)h[0]; return true;
}
static void chStop(int ch) {
	if (!dspOn) return;
	ndspChnWaveBufClear(ch); ndspChnReset(ch); chBusy[ch] = false;
}
static void chPlay(int ch, const Clip* c, bool loop, float vol, float rateMul) {
	if (!dspOn || !c || !c->data) return;
	ndspChnWaveBufClear(ch); ndspChnReset(ch);
	ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
	ndspChnSetRate(ch, (float)c->rate * rateMul);
	ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
	float mix[12]; memset(mix, 0, sizeof mix); mix[0] = mix[1] = chGain(ch, vol);
	ndspChnSetMix(ch, mix);
	memset(&chBuf[ch], 0, sizeof chBuf[ch]);
	chBuf[ch].data_vaddr = c->data; chBuf[ch].nsamples = c->bytes / 2; chBuf[ch].looping = loop;
	ndspChnWaveBufAdd(ch, &chBuf[ch]); chBusy[ch] = true;
}
static void chSet(int ch, float vol, float rateMul, const Clip* c) {
	if (!dspOn || !chBusy[ch]) return;
	float mix[12]; memset(mix, 0, sizeof mix); mix[0] = mix[1] = chGain(ch, vol);
	ndspChnSetMix(ch, mix);
	ndspChnSetRate(ch, (float)c->rate * rateMul);
}

static void bootLog(const char* what);
static void audioInit(void) {
	{ FILE* nf = fopen("sdmc:/3ds/smiler3ds/nosound.txt", "r"); if (nf) { fclose(nf); bootLog("dsp: skipped (nosound.txt)"); return; } }   // escape hatch if the DSP ever crashes the app
	bootLog("dsp: init");
	Result rc = ndspInit();
	if (R_FAILED(rc)) { char b[40]; snprintf(b, sizeof b, "dsp: FAILED %08X", (unsigned)rc); bootLog(b); return; }   // needs dspfirm.cdc on the SD card
	bootLog("dsp: up");
	dspOn = true;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	bootLog("dsp: stereo");
	for (int i = 0; i < N_SFX; i++) { clipLoad(&sfx[i], SFX_FILE[i]); char b[40]; snprintf(b, sizeof b, "dsp: clip %d", i); bootLog(b); }
}
static void audioExit(void) {
	if (!dspOn) return;
	for (int c = 0; c < 9; c++) chStop(c);
	ndspExit(); dspOn = false;
}

// name: "intro", "menu_loop" or "ingame_loop"
static void musicPlay(const char* name, bool loop) {
	if (!dspOn) return;
	if (!strcmp(musicName, name) && (chBusy[CH_MUSIC] || !musicEnabled)) return;
	chStop(CH_MUSIC);
	if (musicClip.data) { linearFree(musicClip.data); musicClip.data = NULL; }
	musicName[0] = 0;
	if (!clipLoad(&musicClip, name)) return;
	snprintf(musicName, sizeof musicName, "%s", name);
	if (musicEnabled) chPlay(CH_MUSIC, &musicClip, loop, musicGain, 1.0f);
}
static void musicStop(void) { chStop(CH_MUSIC); }
static void musicApply(void) {          // after the music option changes
	if (!dspOn) return;
	if (!musicEnabled) { chStop(CH_MUSIC); return; }
	if (musicClip.data && !chBusy[CH_MUSIC]) chPlay(CH_MUSIC, &musicClip, true, musicGain, 1.0f);
}

static void sfxNoiseOnce(float vol) { if (sfxEnabled) chPlay(CH_NOISE, &sfx[SFX_NOISE], false, vol, 1.0f); }   // a single burst of static, never looping
static void sfxOnce(int id, float vol) { if (sfxEnabled) chPlay(id == SFX_NOISE ? CH_NOISE : CH_ONE, &sfx[id], id == SFX_NOISE, vol, 1.0f); }

// ride sounds, called every frame: speed in track units/second
static bool sndBoosting = false, sndCrank = false, sndGrind = false, sndRail = false;
static float cheerVol = 0; static bool sndCheer = false;
static void audioRide(bool running, float speed, bool boosting, bool crank, bool grinding, bool perfect, float dt) {
	if (!dspOn) return;
	bool on = running && sfxEnabled;
	{   // the riders' screaming swells while you fly perfectly, and fades when you don't (the original's screamLoop)
		cheerVol += ((on && perfect ? 0.6f : 0.0f) - cheerVol) * fminf(1.0f, dt * 2.0f);
		if (cheerVol > 0.02f) { if (!sndCheer) { chPlay(CH_CHEER, &sfx[SFX_SCREAM], true, cheerVol, 1.0f); sndCheer = true; } else chSet(CH_CHEER, cheerVol, 1.0f, &sfx[SFX_SCREAM]); }
		else if (sndCheer) { chStop(CH_CHEER); sndCheer = false; cheerVol = 0; }
	}
	if (on) {
		float sp = fclamp(speed / 100.0f, 0, 1.4f);
		float vol = fclamp(speed / 40.0f, 0, 1) * 0.55f, pitch = 0.55f + sp * 0.9f;
		if (!sndRail) { chPlay(CH_RAIL, &sfx[SFX_RAIL], true, vol, pitch); sndRail = true; }
		else chSet(CH_RAIL, vol, pitch, &sfx[SFX_RAIL]);
	} else if (sndRail) { chStop(CH_RAIL); sndRail = false; }

	if (on && boosting && !sndBoosting) { chPlay(CH_BOOST, &sfx[SFX_BOOSTLOOP], true, 0.6f, 1.0f); sndBoosting = true; }
	if ((!on || !boosting) && sndBoosting) { chStop(CH_BOOST); sndBoosting = false; if (on) sfxOnce(SFX_BOOSTEND, 0.7f); }

	if (on && crank && !sndCrank) { chPlay(CH_CRANK, &sfx[SFX_CRANK], true, 0.5f, 1.0f); sndCrank = true; }
	if ((!on || !crank) && sndCrank) { chStop(CH_CRANK); sndCrank = false; }

	if (on && grinding && !sndGrind) { chPlay(CH_GRIND, &sfx[SFX_GRIND], true, 0.45f, 1.0f); sndGrind = true; }
	if ((!on || !grinding) && sndGrind) { chStop(CH_GRIND); sndGrind = false; }
}
static void audioRideStop(void) { audioRide(false, 0, false, false, false, false, 1.0f); }
