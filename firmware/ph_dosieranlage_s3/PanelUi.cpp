#include "PanelUi.h"
#include "Config.h"
#include "Settings.h"
#include "PHMeasurement.h"
#include "StepperPump.h"
#include "PHController.h"
#include "WebInterface.h"

#include <lvgl.h>
#include <LilyGo_AMOLED.h>
#include <time.h>

extern LilyGo_Class amoled;

// --- Farben ----------------------------------------------------------------
// Ohne Blau und ohne Rot: auf schwarzem AMOLED trennen Weiss, Gelb und Gruen
// deutlich besser, besonders bei niedriger Helligkeit.
static const uint32_t C_BG    = 0x000000;   // Schwarz = Pixel aus
static const uint32_t C_CARD  = 0x161A20;
static const uint32_t C_TEXT  = 0xFFFFFF;
static const uint32_t C_MUTED = 0x9AA6B8;
static const uint32_t C_OK    = 0x4FD98A;
static const uint32_t C_WARN  = 0xFFE47A;
static const uint32_t C_ERR   = 0xFFC61A;

// --- pH-Anzeige ------------------------------------------------------------
// Montserrat 48 ist die groesste eingebaute LVGL-Schrift und ergibt auf
// 1,91 Zoll nur rund 4 mm. Der Wert wird deshalb in eine knapp auf das
// Ziffernband zugeschnittene Canvas gezeichnet und als Bild vergroessert.
#define PH_CV_W   126
#define PH_CV_H   44
#define PH_TXT_Y  (-11)
#define ZOOM_BIG  1024      // 4x  -> rund 136 px Zifferhoehe (Aktivansicht)
#define ZOOM_SAVE 768       // 3x  -> rund 102 px, laesst Platz zum Wandern
static lv_color_t phBuf[PH_CV_W * PH_CV_H];

static int16_t scrW = 536, scrH = 240;

static lv_obj_t *root, *phCanvas;
static lv_obj_t *lbl24Cap, *lbl24, *lblState, *lblLocks;
static lv_obj_t *overlay, *lblAsk, *lblAskSub;
static lv_obj_t *toast, *lblToast;

static DispState dstate      = DS_ACTIVE;
static uint32_t  wokeAtMs    = 0;
static uint32_t  dialogOpenMs = 0;
static uint32_t  lastShiftMs = 0;
static uint32_t  toastUntil  = 0;
static uint8_t   shiftIdx    = 0;

static char     lastPhTxt[16] = "";
static uint32_t lastPhCol     = 0xFFFFFFFF;
static int16_t  activeX, activeY;      // Position in der Aktivansicht

// ---------------------------------------------------------------------------
static void styleFlat(lv_obj_t *o) {
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *mkLabel(lv_obj_t *par, const lv_font_t *font, uint32_t color,
                         lv_align_t align, int16_t x, int16_t y, const char *txt) {
  lv_obj_t *l = lv_label_create(par);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, txt);
  lv_obj_align(l, align, x, y);
  return l;
}

static void drawPh(const char *txt, uint32_t col) {
  if (strcmp(txt, lastPhTxt) == 0 && col == lastPhCol) return;
  strncpy(lastPhTxt, txt, sizeof(lastPhTxt) - 1);
  lastPhTxt[sizeof(lastPhTxt) - 1] = 0;
  lastPhCol = col;

  lv_canvas_fill_bg(phCanvas, lv_color_hex(C_BG), LV_OPA_COVER);
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.font  = &lv_font_montserrat_48;
  d.color = lv_color_hex(col);
  d.align = LV_TEXT_ALIGN_CENTER;
  lv_canvas_draw_text(phCanvas, 0, PH_TXT_Y, PH_CV_W, &d, txt);
}

static void openDialog();
static void closeDialog();
static void applyState(DispState st);

// --- Ereignisse ------------------------------------------------------------
static bool swallowWakeTap() { return (millis() - wokeAtMs) < WAKE_GUARD_MS; }

static void onScreenClick(lv_event_t *) {
  // Aus Standby oder Nacht weckt der erste Tipp nur auf.
  if (dstate != DS_ACTIVE) { uiWake(); return; }
  if (swallowWakeTap()) return;
  if (!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN)) return;
  openDialog();
}

static void onYes(lv_event_t *) {
  if (swallowWakeTap()) return;
  closeDialog();

  float ml = (settings.panelRevs * settings.stepsPerRev) / settings.stepsPerMl;
  String err;
  if (controller.manualDose(ml, err))
    uiToast(String(settings.panelRevs, 0) + " Umdr. = " + String(ml, 2) + " ml", true);
  else
    uiToast(err, false);
}

static void onNo(lv_event_t *) {
  if (swallowWakeTap()) return;
  closeDialog();
}

static void openDialog() {
  float ml = (settings.panelRevs * settings.stepsPerRev) / settings.stepsPerMl;
  lv_label_set_text_fmt(lblAsk, "%.0f Umdrehungen freigeben?", settings.panelRevs);
  lv_label_set_text_fmt(lblAskSub, "entspricht ca. %.2f ml pH-Minus", ml);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(overlay);
  dialogOpenMs = millis();
}

static void closeDialog() {
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  dialogOpenMs = 0;
}

// ---------------------------------------------------------------------------
void uiBegin(int16_t w, int16_t h) {
  scrW = w; scrH = h;
  activeX = scrW / 2 - PH_CV_W / 2;
  activeY = 96 - PH_CV_H / 2;

  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, onScreenClick, LV_EVENT_CLICKED, nullptr);

  root = lv_obj_create(scr);
  styleFlat(root);
  lv_obj_set_size(root, scrW, scrH);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_add_flag(root, LV_OBJ_FLAG_EVENT_BUBBLE);

  phCanvas = lv_canvas_create(root);
  lv_canvas_set_buffer(phCanvas, phBuf, PH_CV_W, PH_CV_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(phCanvas, activeX, activeY);
  lv_img_set_zoom(phCanvas, ZOOM_BIG);
  lv_img_set_antialias(phCanvas, true);
  lv_obj_add_flag(phCanvas, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_canvas_fill_bg(phCanvas, lv_color_hex(C_BG), LV_OPA_COVER);
  drawPh("--.--", C_MUTED);

  lbl24Cap = mkLabel(root, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_LEFT, 14, 188, "LETZTE 24 H");
  lbl24    = mkLabel(root, &lv_font_montserrat_32, C_TEXT,  LV_ALIGN_TOP_LEFT, 14, 204, "-- ml");

  lblState = mkLabel(root, &lv_font_montserrat_20, C_MUTED, LV_ALIGN_TOP_RIGHT, -14, 190, "-");
  lblLocks = mkLabel(root, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_RIGHT, -14, 216, "");
  lv_obj_set_width(lblLocks, 300);
  lv_obj_set_style_text_align(lblLocks, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(lblLocks, LV_LABEL_LONG_DOT);
  lv_obj_align(lblLocks, LV_ALIGN_TOP_RIGHT, -14, 216);

  // --- Toast ---
  toast = lv_obj_create(scr);
  styleFlat(toast);
  lv_obj_set_style_bg_color(toast, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(toast, 8, 0);
  lv_obj_set_style_border_width(toast, 2, 0);
  lv_obj_set_style_border_color(toast, lv_color_hex(C_MUTED), 0);
  lv_obj_set_size(toast, scrW - 40, 46);
  lv_obj_align(toast, LV_ALIGN_CENTER, 0, 60);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lblToast = mkLabel(toast, &lv_font_montserrat_18, C_TEXT, LV_ALIGN_CENTER, 0, 0, "");

  // --- Rueckfrage vor der Dosierung ---
  overlay = lv_obj_create(scr);
  styleFlat(overlay);
  lv_obj_set_size(overlay, scrW, scrH);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

  lblAsk    = mkLabel(overlay, &lv_font_montserrat_28, C_TEXT, LV_ALIGN_TOP_MID, 0, 16, "?");
  lblAskSub = mkLabel(overlay, &lv_font_montserrat_20, C_ERR,  LV_ALIGN_TOP_MID, 0, 54, "");
  mkLabel(overlay, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_MID, 0, 82,
          "Alle Sicherheitsgrenzen werden erneut geprueft.");

  lv_obj_t *btnNo = lv_btn_create(overlay);
  lv_obj_set_size(btnNo, 230, 60);
  lv_obj_align(btnNo, LV_ALIGN_BOTTOM_LEFT, 16, -14);
  lv_obj_set_style_bg_color(btnNo, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_border_width(btnNo, 2, 0);
  lv_obj_set_style_border_color(btnNo, lv_color_hex(C_MUTED), 0);
  lv_obj_set_style_radius(btnNo, 10, 0);
  lv_obj_add_event_cb(btnNo, onNo, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *l1 = lv_label_create(btnNo);
  lv_obj_set_style_text_font(l1, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(l1, lv_color_hex(C_TEXT), 0);
  lv_label_set_text(l1, "ABBRECHEN");
  lv_obj_center(l1);

  lv_obj_t *btnYes = lv_btn_create(overlay);
  lv_obj_set_size(btnYes, 230, 60);
  lv_obj_align(btnYes, LV_ALIGN_BOTTOM_RIGHT, -16, -14);
  lv_obj_set_style_bg_color(btnYes, lv_color_hex(C_ERR), 0);
  lv_obj_set_style_radius(btnYes, 10, 0);
  lv_obj_add_event_cb(btnYes, onYes, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *l2 = lv_label_create(btnYes);
  lv_obj_set_style_text_font(l2, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(l2, lv_color_hex(0x000000), 0);
  lv_label_set_text(l2, "FREIGEBEN");
  lv_obj_center(l2);

  amoled.setBrightness(BRIGHT_ACTIVE);
  lastShiftMs = millis();
  wokeAtMs = millis();
}

// ---------------------------------------------------------------------------
void uiToast(const String &msg, bool ok) {
  uiWake();
  lv_label_set_text(lblToast, msg.c_str());
  lv_obj_set_style_text_color(lblToast, lv_color_hex(ok ? C_OK : C_ERR), 0);
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(toast);
  toastUntil = millis() + 5000;
}

DispState uiState() { return dstate; }

const char *uiStateText() {
  switch (dstate) {
    case DS_ACTIVE: return "aktiv";
    case DS_SAVER:  return "Standby";
    case DS_OFF:    return "Nachtmodus (aus)";
  }
  return "?";
}

// ---------------------------------------------------------------------------
// Anzeigezustaende
// ---------------------------------------------------------------------------
static void showDetails(bool on) {
  lv_obj_t *objs[] = { lbl24Cap, lbl24, lblState, lblLocks };
  for (lv_obj_t *o : objs) {
    if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

static void applyState(DispState st) {
  if (st == dstate) return;
  dstate = st;

  switch (st) {
    case DS_ACTIVE:
      showDetails(true);
      lv_obj_clear_flag(phCanvas, LV_OBJ_FLAG_HIDDEN);
      lv_img_set_zoom(phCanvas, ZOOM_BIG);
      lv_obj_set_pos(phCanvas, activeX, activeY);
      amoled.setBrightness(BRIGHT_ACTIVE);
      break;

    case DS_SAVER:
      closeDialog();
      showDetails(false);
      lv_obj_clear_flag(phCanvas, LV_OBJ_FLAG_HIDDEN);
      lv_img_set_zoom(phCanvas, ZOOM_SAVE);
      amoled.setBrightness(BRIGHT_SAVER);
      lastShiftMs = 0;               // sofort eine neue Position waehlen
      break;

    case DS_OFF:
      closeDialog();
      showDetails(false);
      lv_obj_add_flag(phCanvas, LV_OBJ_FLAG_HIDDEN);
      amoled.setBrightness(0);
      break;
  }
  lv_obj_invalidate(lv_scr_act());
}

void uiWake() {
  wokeAtMs = millis();
  applyState(DS_ACTIVE);
}

// Nachtfenster; ohne gueltige Uhrzeit gibt es keinen Nachtmodus.
static bool isNight() {
  if (!settings.nightEnabled) return false;
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  struct tm t;
  localtime_r(&now, &t);
  uint8_t h = t.tm_hour;
  if (settings.nightFrom == settings.nightTo) return false;
  if (settings.nightFrom < settings.nightTo)          // z.B. 1 bis 6
    return h >= settings.nightFrom && h < settings.nightTo;
  return h >= settings.nightFrom || h < settings.nightTo;   // z.B. 20 bis 5
}

// Im Standby wandert der Wert, damit sich nichts einbrennt. Die Positionen
// sind ueber die verfuegbare Flaeche gestreut, nicht nur um wenige Pixel.
static void shiftSaverPos() {
  int16_t dispW = (int32_t)PH_CV_W * ZOOM_SAVE / 256;
  int16_t dispH = (int32_t)PH_CV_H * ZOOM_SAVE / 256;
  int16_t rangeX = scrW - dispW;
  int16_t rangeY = scrH - dispH;
  if (rangeX < 0) rangeX = 0;
  if (rangeY < 0) rangeY = 0;

  static const uint8_t px[6] = { 10, 50, 90, 50, 10, 90 };   // Prozent
  static const uint8_t py[6] = { 15, 50, 85, 15, 85, 50 };
  shiftIdx = (shiftIdx + 1) % 6;

  int16_t cx = rangeX * px[shiftIdx] / 100;
  int16_t cy = rangeY * py[shiftIdx] / 100;
  // Die Canvas wird um ihren Mittelpunkt skaliert: Objektposition entsprechend
  // um den halben Zuwachs zurueckrechnen.
  lv_obj_set_pos(phCanvas, cx + (dispW - PH_CV_W) / 2, cy + (dispH - PH_CV_H) / 2);
}

// ---------------------------------------------------------------------------
void uiRefresh() {
  if (dstate == DS_OFF) return;                 // nichts zeichnen, Display ist aus

  // --- pH ---
  char phTxt[16];
  uint32_t phCol = C_MUTED;
  if (phMeas.valid()) {
    snprintf(phTxt, sizeof(phTxt), "%.2f", phMeas.ph());
    float sp = settings.phSetpoint;
    phCol = C_OK;
    if (phMeas.ph() > sp + 0.30f) phCol = C_WARN;
    if (phMeas.ph() > sp + 0.60f) phCol = C_ERR;
    if (phMeas.ph() < sp - 0.20f) phCol = C_ERR;
  } else {
    strcpy(phTxt, "--.--");
  }
  drawPh(phTxt, phCol);

  if (dstate == DS_SAVER) return;               // im Standby nur der Wert

  // --- Mengen ---
  lv_label_set_text_fmt(lbl24, "%.1f ml", controller.ml24h());
  lv_label_set_text_fmt(lbl24Cap, "LETZTE 24 H   (heute %.1f / %.0f)",
                        settings.dailyMl, settings.maxDailyMl);

  // --- Zustand ---
  if (pump.running()) {
    lv_label_set_text_fmt(lblState, "Dosiert  %.2f / %.2f ml",
                          pump.mlDone(), pump.mlTarget());
    lv_obj_set_style_text_color(lblState, lv_color_hex(C_TEXT), 0);
    lv_label_set_text(lblLocks, "");
  } else {
    lv_label_set_text(lblState, controller.stateText());
    uint32_t col = C_MUTED;
    if (controller.state() == ST_FAULT || controller.estopActive()) col = C_ERR;
    else if (controller.state() == ST_IDLE)                          col = C_OK;
    lv_obj_set_style_text_color(lblState, lv_color_hex(col), 0);

    if (web.apMode()) {
      // Im Einrichtungsmodus ist die Netzinfo wichtiger als die Sperrliste.
      lv_label_set_text(lblLocks, (String("WLAN ") + AP_SSID_DEFAULT +
                                   " / " + AP_PASS_DEFAULT + "  ->  " + web.ip()).c_str());
    } else {
      lv_label_set_text(lblLocks, phMeas.valid() ? controller.lockText().c_str()
                                                 : phMeas.statusText());
    }
  }
  lv_obj_align(lblLocks, LV_ALIGN_TOP_RIGHT, -14, 216);
}

// ---------------------------------------------------------------------------
void uiTick() {
  uint32_t now = millis();

  if (toastUntil && now > toastUntil) {
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    toastUntil = 0;
  }

  if (dialogOpenMs && now - dialogOpenMs > DIALOG_TIMEOUT) closeDialog();

  uint32_t idleMs = lv_disp_get_inactive_time(NULL);

  // Beruehrung holt immer nach vorn
  if (idleMs < 500 && dstate != DS_ACTIVE) uiWake();

  if (dstate == DS_ACTIVE) {
    // Waehrend einer laufenden Dosierung bleibt die Anzeige wach.
    if (!pump.running() && idleMs > (uint32_t)settings.standbyS * 1000UL)
      applyState(isNight() ? DS_OFF : DS_SAVER);
  } else {
    // Wechsel zwischen Standby und Nacht, sobald das Fenster beginnt/endet
    DispState want = isNight() ? DS_OFF : DS_SAVER;
    if (want != dstate) applyState(want);
  }

  if (dstate == DS_SAVER &&
      (lastShiftMs == 0 || now - lastShiftMs > (uint32_t)settings.shiftS * 1000UL)) {
    lastShiftMs = now;
    shiftSaverPos();
  }
}
