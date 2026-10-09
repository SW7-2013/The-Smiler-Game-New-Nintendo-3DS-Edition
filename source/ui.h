// Menu / HUD drawing layer: sprites, bitmap-font text, touch buttons. Included by main.c.
#pragma once
#include "ui_gen.h"

#define F_SMALL 0
#define F_TITLE 1
#define F_LCD   2
#define UI_MAXQUADS 3600
#define UI_MAXRUN   512
#define TEX_VIDEO   (-2)

typedef struct { float u0, v0, u1, v1, x, y, w, h, adv; } Glyph;
typedef struct { int tex, lh; float capc; Glyph g[128]; u8 has[128]; } Font;
static Font fonts[3];

static bool loadFonts(void) {
	FILE* f = fopen("romfs:/fonts.bin", "rb");
	if (!f) { snprintf(errmsg, sizeof errmsg, "fonts.bin missing"); return false; }
	char m[4]; u32 n;
	if (fread(m, 1, 4, f) != 4 || memcmp(m, "FNT1", 4) || fread(&n, 4, 1, f) != 1 || n != 3) { fclose(f); snprintf(errmsg, sizeof errmsg, "fonts.bin bad"); return false; }
	for (int i = 0; i < 3; i++) {
		s32 ti, lh; u32 ng;
		if (fread(&ti, 4, 1, f) != 1 || fread(&lh, 4, 1, f) != 1 || fread(&ng, 4, 1, f) != 1) { fclose(f); snprintf(errmsg, sizeof errmsg, "fonts.bin short"); return false; }
		fonts[i].tex = ti; fonts[i].lh = lh;
		for (u32 k = 0; k < ng; k++) {
			u32 code; float v[9];
			if (fread(&code, 4, 1, f) != 1 || fread(v, 4, 9, f) != 9) { fclose(f); snprintf(errmsg, sizeof errmsg, "fonts.bin short"); return false; }
			if (code < 128) {
				Glyph* g = &fonts[i].g[code];
				g->u0 = v[0]; g->v0 = v[1]; g->u1 = v[2]; g->v1 = v[3]; g->x = v[4]; g->y = v[5]; g->w = v[6]; g->h = v[7]; g->adv = v[8];
				fonts[i].has[code] = 1;
			}
		}
		const Glyph* a = &fonts[i].g['A'];
		fonts[i].capc = -a->y - a->h * 0.5f;      // distance from the glyph origin down to the middle of a capital letter
	}
	fclose(f); return true;
}

// ---------------------------------------------------------------- quad batcher
static Vertex* uiVB = NULL; static u16* uiIdx = NULL;
static int uiQN = 0, uiRunStart = 0, uiRunN = 0, uiRunTex = -1; static u32 uiRunCol = 0;
static float uiR = 1, uiG = 1, uiB = 1, uiA = 1;
static float uiScreenW = 400;
static int uiScreenNo = 0;

#ifdef UI_SOFT
static FILE* uiLog = NULL;
#endif

static bool uiInit(void) {
	uiVB = (Vertex*)linearAlloc(sizeof(Vertex) * 4 * UI_MAXQUADS);
	uiIdx = (u16*)linearAlloc(UI_MAXRUN * 6 * 2);
	if (!uiVB || !uiIdx) { snprintf(errmsg, sizeof errmsg, "out of linear memory (ui)"); return false; }
	for (int q = 0; q < UI_MAXRUN; q++) {
		static const u8 o[6] = { 0, 1, 2, 0, 2, 3 };
		for (int k = 0; k < 6; k++) uiIdx[q * 6 + k] = (u16)(q * 4 + o[k]);
	}
	GSPGPU_FlushDataCache(uiIdx, UI_MAXRUN * 6 * 2);
	return true;
}

static u32 uiPackCol(void) {
	#define Q8(x) ((u32)(fclamp(x, 0, 1) * 255.0f + 0.5f))
	return (Q8(uiR) << 24) | (Q8(uiG) << 16) | (Q8(uiB) << 8) | Q8(uiA);
}
static void uiColor(float r, float g, float b, float a) { uiR = r; uiG = g; uiB = b; uiA = a; }
static void uiWhite(float a) { uiColor(1, 1, 1, a); }

static C3D_Tex vidTex; static bool vidTexOk = false;

static void uiFlushRun(void) {
	if (!uiRunN) return;
#ifdef UI_SOFT
	if (uiLog) for (int q = 0; q < uiRunN; q++) {
		const Vertex* v = &uiVB[(uiRunStart + q) * 4];
		fprintf(uiLog, "Q %d %d %u", uiScreenNo, uiRunTex, uiRunCol);
		for (int k = 0; k < 4; k++) fprintf(uiLog, " %.2f %.2f %.4f %.4f", v[k].x, v[k].y, v[k].u, v[k].v);
		fprintf(uiLog, "\n");
	}
#else
	C3D_BufInfo* bi = C3D_GetBufInfo(); BufInfo_Init(bi);
	BufInfo_Add(bi, &uiVB[uiRunStart * 4], sizeof(Vertex), 3, 0x210);
	C3D_TexBind(0, uiRunTex == TEX_VIDEO ? &vidTex : &textures[uiRunTex]);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tint, (float)(uiRunCol >> 24) / 255.0f, (float)((uiRunCol >> 16) & 255) / 255.0f,
		(float)((uiRunCol >> 8) & 255) / 255.0f, (float)(uiRunCol & 255) / 255.0f);
	C3D_DrawElements(GPU_TRIANGLES, uiRunN * 6, C3D_UNSIGNED_SHORT, uiIdx);
#endif
	uiRunStart += uiRunN; uiRunN = 0;
}

// four corners: top-left, top-right, bottom-right, bottom-left; (u0,v0)=top-left, (u1,v1)=bottom-right in v-up texture space
static void uiQuadP(int tex, const float* px, const float* py, float u0, float v0, float u1, float v1) {
	if (uiQN >= UI_MAXQUADS) return;
	u32 col = uiPackCol();
	if (uiRunN && (tex != uiRunTex || col != uiRunCol || uiRunN >= UI_MAXRUN)) uiFlushRun();
	if (!uiRunN) { uiRunTex = tex; uiRunCol = col; }
	Vertex* v = &uiVB[uiQN * 4];
	static const int ui_[4] = { 0, 1, 1, 0 }, vi_[4] = { 0, 0, 1, 1 };
	for (int k = 0; k < 4; k++) {
		v[k].x = px[k]; v[k].y = py[k]; v[k].z = 0;
		v[k].u = ui_[k] ? u1 : u0; v[k].v = vi_[k] ? v1 : v0;
		v[k].nx = v[k].ny = v[k].nz = 0;
	}
	uiQN++; uiRunN++;
}
static void uiRectUV(int tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1) {
	float px[4] = { x, x + w, x + w, x }, py[4] = { y, y, y + h, y + h };
	uiQuadP(tex, px, py, u0, v0, u1, v1);
}
static void uiRect(int tex, float x, float y, float w, float h) { uiRectUV(tex, x, y, w, h, 0, 1, 1, 0); }
// rows/columns of a sprite sheet: cell (col,row) with row 0 at the top
static void uiCell(int tex, float x, float y, float w, float h, int col, int row, int cols, int rows) {
	uiRectUV(tex, x, y, w, h, (float)col / cols, 1.0f - (float)row / rows, (float)(col + 1) / cols, 1.0f - (float)(row + 1) / rows);
}
// rotate a rectangle about a point (angle in radians, clockwise on screen)
static void uiRectRot(int tex, float x, float y, float w, float h, float ang, float pvx, float pvy) {
	float cx[4] = { x, x + w, x + w, x }, cy[4] = { y, y, y + h, y + h }, px[4], py[4];
	float c = cosf(ang), s = sinf(ang);
	for (int k = 0; k < 4; k++) { float dx = cx[k] - pvx, dy = cy[k] - pvy; px[k] = pvx + dx * c - dy * s; py[k] = pvy + dx * s + dy * c; }
	uiQuadP(tex, px, py, 0, 1, 1, 0);
}
static void uiRectRotUV(int tex, float x, float y, float w, float h, float ang, float pvx, float pvy, float u0, float v0, float u1, float v1) {
	float cx[4] = { x, x + w, x + w, x }, cy[4] = { y, y, y + h, y + h }, px[4], py[4];
	float c = cosf(ang), s = sinf(ang);
	for (int k = 0; k < 4; k++) { float dx = cx[k] - pvx, dy = cy[k] - pvy; px[k] = pvx + dx * c - dy * s; py[k] = pvy + dx * s + dy * c; }
	uiQuadP(tex, px, py, u0, v0, u1, v1);
}
static float uiSW(int t) { return (float)UI_SIZE[t - UI_BASE][0]; }
static float uiSH(int t) { return (float)UI_SIZE[t - UI_BASE][1]; }
static void uiSpr(int t, float x, float y, float sc) { uiRect(t, x, y, uiSW(t) * sc, uiSH(t) * sc); }
static void uiSprC(int t, float cx, float cy, float sc) { uiRect(t, cx - uiSW(t) * sc * 0.5f, cy - uiSH(t) * sc * 0.5f, uiSW(t) * sc, uiSH(t) * sc); }
static void uiFill(float x, float y, float w, float h) { uiRect(T_white, x, y, w, h); }

// ---------------------------------------------------------------- text
static float uiTextW(int fi, const char* s, float sc) {
	const Font* f = &fonts[fi]; float w = 0;
	for (; *s; s++) { int c = (u8)*s; if (c < 128 && !f->has[c] && c >= 'a' && c <= 'z') c -= 32; if (c < 128 && f->has[c]) w += f->g[c].adv; else w += f->g[' '].adv; }
	return w * sc;
}
// align: 0 = left, 1 = centre, 2 = right; (x, cy) = anchor, cy being the middle of the capital letters
static void uiText(int fi, const char* s, float x, float cy, float sc, int align) {
	const Font* f = &fonts[fi];
	float w = uiTextW(fi, s, sc);
	if (align == 1) x -= w * 0.5f; else if (align == 2) x -= w;
	float oy = cy - f->capc * sc;
	for (; *s; s++) {
		int c = (u8)*s; if (c < 128 && !f->has[c] && c >= 'a' && c <= 'z') c -= 32;
		if (c >= 128 || !f->has[c]) {
#ifdef UI_SOFT
			fprintf(stderr, "missing glyph '%c' in font %d\n", c, fi);
#endif
			x += f->g[' '].adv * sc; continue; }
		const Glyph* g = &f->g[c];
		if (g->w != 0 && g->h != 0) {
			float gx = x + g->x * sc, gy = oy - g->y * sc, gw = g->w * sc, gh = -g->h * sc;
			uiRectUV(f->tex, gx, gy, gw, gh, g->u0, g->v1, g->u1, g->v0);
		}
		x += g->adv * sc;
	}
}
static void uiTextS(int fi, const char* s, float x, float cy, float sc, int align) {   // with a drop shadow
	float r = uiR, g = uiG, b = uiB, a = uiA;
	uiColor(0, 0, 0, a * 0.6f); uiText(fi, s, x + 1.5f * sc + 0.5f, cy + 1.5f * sc + 0.5f, sc, align);
	uiColor(r, g, b, a); uiText(fi, s, x, cy, sc, align);
}
// word-wrapped paragraph; returns the number of lines
static int uiWrap(int fi, const char* s, float x, float cy, float maxw, float sc, float lineh, int align) {
	char line[160]; int n = 0, lines = 0;
	const char* p = s;
	while (*p) {
		const char* e = p; const char* lastSp = NULL; char buf[160]; int len = 0;
		while (*e && len < 150) {
			buf[len++] = *e; buf[len] = 0;
			if (*e == ' ') lastSp = e;
			if (uiTextW(fi, buf, sc) > maxw && lastSp) { len = (int)(lastSp - p); e = lastSp; break; }
			e++;
		}
		if (!*e) len = (int)(e - p);
		memcpy(line, p, len); line[len] = 0; n = len;
		uiText(fi, line, x, cy + lines * lineh, sc, align); lines++;
		p += n; while (*p == ' ') p++;
	}
	return lines;
}
static void uiNum(char* out, int v) {   // 12345 -> "12,345"
	char t[24]; snprintf(t, sizeof t, "%d", v < 0 ? 0 : v); int l = (int)strlen(t), j = 0;
	for (int i = 0; i < l; i++) { if (i && (l - i) % 3 == 0) out[j++] = ','; out[j++] = t[i]; }
	out[j] = 0;
}

// ---------------------------------------------------------------- touch input
static bool tHeld = false, tDown = false, tUp = false; static int tX = 0, tY = 0, tSX = 0, tSY = 0;
static bool uiBlocked = false;      // set while a fade is running so taps do nothing
static void uiPoll(void) {
	touchPosition p; hidTouchRead(&p);
	u32 held = hidKeysHeld();
	bool now = (held & KEY_TOUCH) != 0;
	tDown = now && !tHeld; tUp = !now && tHeld;
	if (now) { tX = p.px; tY = p.py; }
	if (tDown) { tSX = tX; tSY = tY; }
	tHeld = now;
}
static bool inRect(int px, int py, float x, float y, float w, float h) { return px >= x && px < x + w && py >= y && py < y + h; }
// pressed = finger is down and started on this rectangle; returns true once, on release over it
static bool uiHit(float x, float y, float w, float h, bool* pressed) {
	bool pr = !uiBlocked && tHeld && inRect(tSX, tSY, x, y, w, h) && inRect(tX, tY, x, y, w, h);
	if (pressed) *pressed = pr;
	return !uiBlocked && tUp && inRect(tSX, tSY, x, y, w, h) && inRect(tX, tY, x, y, w, h);
}
// a sprite used as a button, centred on (cx,cy); the hit area is slightly larger than the art
static bool uiBtnBox(int t, float x, float y, float w, float h, const char* label, int font, float tsc) {
	float r = uiR, g = uiG, b = uiB, a = uiA; bool pr;
	bool hit = uiHit(x, y, w, h, &pr);
	float k = pr ? 0.94f : 1.0f, d = pr ? 0.8f : 1.0f;
	float cx = x + w * 0.5f, cy = y + h * 0.5f;
	uiColor(r * d, g * d, b * d, a); uiRect(t, cx - w * k * 0.5f, cy - h * k * 0.5f, w * k, h * k);
	if (label) { uiColor(0.13f, 0.06f, 0.30f, a); uiText(font, label, cx, cy, tsc * k, 1); }
	uiWhite(1);
	return hit;
}
// a sprite used as a button, centred on (cx,cy); Uses the current colour/alpha.
static bool uiSprBtn(int t, float cx, float cy, float sc, const char* label, int font, float tsc) {
	float w = uiSW(t) * sc, h = uiSH(t) * sc;
	return uiBtnBox(t, cx - w * 0.5f, cy - h * 0.5f, w, h, label, font, tsc);
}

// ---------------------------------------------------------------- frame / screen set-up
static C3D_Mtx uiProj[2];
static void uiProjInit(void) {
	Mtx_OrthoTilt(&uiProj[0], 0.0f, 400.0f, 240.0f, 0.0f, 1.0f, -1.0f, true);
	Mtx_OrthoTilt(&uiProj[1], 0.0f, 320.0f, 240.0f, 0.0f, 1.0f, -1.0f, true);
}
static void uiFrameStart(void) { uiQN = 0; uiRunStart = 0; uiRunN = 0; }
static void uiBeginScreen(int s) {
	uiScreenNo = s; uiScreenW = s ? 320.0f : 400.0f;
#ifndef UI_SOFT
	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
	C3D_AlphaTest(false, GPU_GREATER, 0);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &uiProj[s]);
	M4 im = m4_identity(); C3D_Mtx id; m4_to_c3d(&im, &id);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &id);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightK, 0, 0, 0, 0);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_uvxf, 1, 1, 0, 0);
#endif
	uiRunN = 0; uiWhite(1);
}
static void uiEndScreen(void) { uiFlushRun(); }
static void uiEndFrame(void) {
#ifndef UI_SOFT
	GSPGPU_FlushDataCache(uiVB, sizeof(Vertex) * 4 * (uiQN > 0 ? uiQN : 1));
#endif
}
