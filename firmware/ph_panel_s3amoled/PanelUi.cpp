#include "PanelUi.h"
#include "PanelNet.h"

#include <lvgl.h>
#include <LilyGo_AMOLED.h>

extern LilyGo_Class amoled;

// --- Farben ---------------------------------------------------------------
static const uint32_t C_BG     = 0x000000;   // echtes Schwarz spart am AMOLED Strom
static const uint32_t C_CARD   = 0x141A22;
static const uint32_t C_LINE   = 0x2A3341;
static const uint32_t C_TEXT   = 0xE8EDF4;
static const uint32_t C_MUTED  = 0x8896AA;
static const uint32_t C_OK     = 0x3FBF7F;
static const uint32_t C_WARN   = 0xE2B93B;
static const uint32_t C_ERR    = 0xE05C5C;
static const uint32_t C_ACC    = 0x4EA1FF;

// --- Helligkeit / Einbrennschutz ------------------------------------------
static const uint8_t  BRIGHT_ON      = 240;
static const uint8_t  BRIGHT_DIM     = 25;
static const uint32_t DIM_AFTER_MS   = 60000;
static const uint32_t SHIFT_EVERY_MS = 90000;
static const uint32_t DIALOG_TIMEOUT = 12000;

static int16_t scrW = 536, scrH = 240;

static lv_obj_t *root, *lblNet, *lblPh, *lblPhSub, *lblSp;
static lv_obj_t *lbl24, *lbl24Cap, *lblToday, *lblState, *lblLocks;
static lv_obj_t *btnDose, *lblBtn;
static lv_obj_t *overlay, *lblAsk, *lblAskSub, *btnYes, *btnNo;
static lv_obj_t *toast, *lblToast;

static bool     dimmed      = false;
static uint32_t wokeAtMs    = 0;
static uint32_t dialogOpenMs = 0;
static uint32_t lastShiftMs = 0;
static uint32_t toastUntil  = 0;
static int8_t   shiftPhase  = 0;

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

static void openDialog();
static void closeDialog();

// --- Ereignisse ------------------------------------------------------------
static bool swallowWakeTap() {
  // Der Tipp, der das gedimmte Display aufweckt, darf nichts ausloesen.
  return (millis() - wokeAtMs) < 450;
}

static void onScreenClick(lv_event_t *) {
  if (swallowWakeTap()) return;
  if (!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN)) return;
  openDialog();
}

static void onYes(lv_event_t *) {
  if (swallowWakeTap()) return;
  closeDialog();
  netRequestDose();
  uiToast("Anforderung gesendet ...", true);
}

static void onNo(lv_event_t *) {
  if (swallowWakeTap()) return;
  closeDialog();
}

static void openDialog() {
  float ml = netDoseMl();
  lv_label_set_text_fmt(lblAsk, "%.0f Umdrehungen freigeben?", panelCfg.revs);
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

  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, onScreenClick, LV_EVENT_CLICKED, nullptr);

  // Wurzelcontainer - wird fuer den Einbrennschutz leicht verschoben
  root = lv_obj_create(scr);
  styleFlat(root);
  lv_obj_set_size(root, scrW, scrH);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_add_flag(root, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- Kopfzeile ---
  mkLabel(root, &lv_font_montserrat_14, C_MUTED, LV_ALIGN_TOP_LEFT, 12, 8, "pH-DOSIERUNG");
  lblNet = mkLabel(root, &lv_font_montserrat_14, C_MUTED, LV_ALIGN_TOP_RIGHT, -12, 8, "...");

  // --- pH gross, links ---
  lblPh    = mkLabel(root, &lv_font_montserrat_48, C_TEXT,  LV_ALIGN_TOP_LEFT, 14, 42, "--.--");
  lblSp    = mkLabel(root, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_LEFT, 16, 100, "Soll --");
  lblPhSub = mkLabel(root, &lv_font_montserrat_14, C_MUTED, LV_ALIGN_TOP_LEFT, 16, 124, "-");

  // --- 24-h-Menge, rechts ---
  lv_obj_t *card = lv_obj_create(root);
  styleFlat(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_size(card, 208, 106);
  lv_obj_align(card, LV_ALIGN_TOP_RIGHT, -12, 36);
  lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);

  lbl24Cap = mkLabel(card, &lv_font_montserrat_12, C_MUTED, LV_ALIGN_TOP_MID, 0, 10, "LETZTE 24 STUNDEN");
  lbl24    = mkLabel(card, &lv_font_montserrat_32, C_ACC,   LV_ALIGN_TOP_MID, 0, 30, "-- ml");
  lblToday = mkLabel(card, &lv_font_montserrat_12, C_MUTED, LV_ALIGN_BOTTOM_MID, 0, -10, "heute -- / -- ml");

  // --- Zustandszeile ---
  lblState = mkLabel(root, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_LEFT, 14, 152, "-");
  lblLocks = mkLabel(root, &lv_font_montserrat_12, C_MUTED, LV_ALIGN_TOP_LEFT, 14, 174, "");
  lv_obj_set_width(lblLocks, scrW - 28);
  lv_label_set_long_mode(lblLocks, LV_LABEL_LONG_DOT);

  // --- Dosierknopf ---
  btnDose = lv_btn_create(root);
  lv_obj_set_size(btnDose, scrW - 28, 44);
  lv_obj_align(btnDose, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(btnDose, lv_color_hex(C_ACC), 0);
  lv_obj_set_style_radius(btnDose, 10, 0);
  lv_obj_add_event_cb(btnDose, onScreenClick, LV_EVENT_CLICKED, nullptr);
  lblBtn = lv_label_create(btnDose);
  lv_obj_set_style_text_font(lblBtn, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(lblBtn, lv_color_hex(0x061220), 0);
  lv_label_set_text(lblBtn, "DOSIEREN");
  lv_obj_center(lblBtn);

  // --- Toast ---
  toast = lv_obj_create(scr);
  styleFlat(toast);
  lv_obj_set_style_bg_color(toast, lv_color_hex(C_LINE), 0);
  lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(toast, 8, 0);
  lv_obj_set_size(toast, scrW - 60, 34);
  lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -60);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lblToast = mkLabel(toast, &lv_font_montserrat_14, C_TEXT, LV_ALIGN_CENTER, 0, 0, "");

  // --- Bestaetigungsdialog ---
  overlay = lv_obj_create(scr);
  styleFlat(overlay);
  lv_obj_set_size(overlay, scrW, scrH);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);   // schluckt Klicks nach hinten

  lv_obj_t *box = lv_obj_create(overlay);
  styleFlat(box);
  lv_obj_set_style_bg_color(box, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 12, 0);
  lv_obj_set_style_border_width(box, 2, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(C_WARN), 0);
  lv_obj_set_size(box, scrW - 60, scrH - 50);
  lv_obj_center(box);

  lblAsk    = mkLabel(box, &lv_font_montserrat_24, C_TEXT,  LV_ALIGN_TOP_MID, 0, 16, "?");
  lblAskSub = mkLabel(box, &lv_font_montserrat_16, C_WARN,  LV_ALIGN_TOP_MID, 0, 50, "");
  mkLabel(box, &lv_font_montserrat_12, C_MUTED, LV_ALIGN_TOP_MID, 0, 74,
          "Die Anlage prueft die Sicherheitsgrenzen erneut.");

  btnNo = lv_btn_create(box);
  lv_obj_set_size(btnNo, 180, 46);
  lv_obj_align(btnNo, LV_ALIGN_BOTTOM_LEFT, 10, -12);
  lv_obj_set_style_bg_color(btnNo, lv_color_hex(C_LINE), 0);
  lv_obj_set_style_radius(btnNo, 10, 0);
  lv_obj_add_event_cb(btnNo, onNo, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *l1 = lv_label_create(btnNo);
  lv_obj_set_style_text_font(l1, &lv_font_montserrat_18, 0);
  lv_label_set_text(l1, "ABBRECHEN");
  lv_obj_center(l1);

  btnYes = lv_btn_create(box);
  lv_obj_set_size(btnYes, 180, 46);
  lv_obj_align(btnYes, LV_ALIGN_BOTTOM_RIGHT, -10, -12);
  lv_obj_set_style_bg_color(btnYes, lv_color_hex(C_ERR), 0);
  lv_obj_set_style_radius(btnYes, 10, 0);
  lv_obj_add_event_cb(btnYes, onYes, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *l2 = lv_label_create(btnYes);
  lv_obj_set_style_text_font(l2, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(l2, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(l2, "FREIGEBEN");
  lv_obj_center(l2);

  amoled.setBrightness(BRIGHT_ON);
  lastShiftMs = millis();
}

// ---------------------------------------------------------------------------
void uiToast(const String &msg, bool ok) {
  lv_label_set_text(lblToast, msg.c_str());
  lv_obj_set_style_text_color(lblToast, lv_color_hex(ok ? C_OK : C_ERR), 0);
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(toast);
  toastUntil = millis() + 5000;
}

void uiRefresh() {
  netLock();
  PanelState s = netState();     // Kopie ziehen, dann sofort freigeben
  netUnlock();

  // --- pH ---
  if (s.online && s.phValid) {
    lv_label_set_text_fmt(lblPh, "%.2f", s.ph);
    uint32_t col = C_OK;
    if (s.ph > s.setpoint + 0.30f)      col = C_WARN;
    if (s.ph > s.setpoint + 0.60f)      col = C_ERR;
    if (s.ph < s.setpoint - 0.20f)      col = C_ERR;
    lv_obj_set_style_text_color(lblPh, lv_color_hex(col), 0);
  } else {
    lv_label_set_text(lblPh, "--.--");
    lv_obj_set_style_text_color(lblPh, lv_color_hex(C_MUTED), 0);
  }

  lv_label_set_text_fmt(lblSp, "Soll %.2f", s.setpoint);
  if (!s.online) {
    lv_label_set_text(lblPhSub, "keine Verbindung zur Anlage");
    lv_obj_set_style_text_color(lblPhSub, lv_color_hex(C_ERR), 0);
  } else {
    lv_label_set_text_fmt(lblPhSub, "%s%s", s.phStatus,
                          s.phValid ? (s.stable ? " - stabil" : " - schwankt") : "");
    lv_obj_set_style_text_color(lblPhSub,
                                lv_color_hex(s.phValid ? C_MUTED : C_WARN), 0);
  }

  // --- Mengen ---
  lv_label_set_text_fmt(lbl24, "%.1f ml", s.last24h);
  lv_label_set_text_fmt(lblToday, "heute %.1f / %.0f ml", s.today, s.maxDaily);

  // --- Zustand ---
  if (s.pumpRun) {
    lv_label_set_text_fmt(lblState, "Dosiert  %.2f / %.2f ml", s.pumpMl, s.pumpTarget);
    lv_obj_set_style_text_color(lblState, lv_color_hex(C_ACC), 0);
  } else {
    lv_label_set_text(lblState, s.online ? s.state : "offline");
    uint32_t col = C_MUTED;
    if (s.fault || s.estop) col = C_ERR;
    else if (strcmp(s.state, "Bereit") == 0) col = C_OK;
    lv_obj_set_style_text_color(lblState, lv_color_hex(col), 0);
  }
  lv_label_set_text(lblLocks, s.online ? s.locks : "");

  // --- Knopf ---
  bool busy = netDosePending() || s.pumpRun || !s.online;
  if (busy) {
    lv_obj_add_state(btnDose, LV_STATE_DISABLED);
    lv_label_set_text(lblBtn, s.online ? "PUMPE LAEUFT" : "OFFLINE");
  } else {
    lv_obj_clear_state(btnDose, LV_STATE_DISABLED);
    lv_label_set_text_fmt(lblBtn, "%.0f UMDREHUNGEN DOSIEREN", panelCfg.revs);
  }

  lv_label_set_text(lblNet, netWifiInfo().c_str());
}

// ---------------------------------------------------------------------------
void uiTick() {
  uint32_t now = millis();

  // Ergebnis einer Dosieranforderung abholen
  String msg; bool ok;
  if (netTakeResult(msg, ok)) uiToast(msg, ok);

  if (toastUntil && now > toastUntil) {
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    toastUntil = 0;
  }

  // Dialog nicht offen stehen lassen
  if (dialogOpenMs && now - dialogOpenMs > DIALOG_TIMEOUT) closeDialog();

  // Helligkeit
  uint32_t idle = lv_disp_get_inactive_time(NULL);
  if (dimmed && idle < 500) {
    dimmed = false;
    wokeAtMs = now;
    amoled.setBrightness(BRIGHT_ON);
  } else if (!dimmed && idle > DIM_AFTER_MS) {
    dimmed = true;
    amoled.setBrightness(BRIGHT_DIM);
    if (dialogOpenMs) closeDialog();
  }

  // Einbrennschutz: Inhalt langsam um wenige Pixel wandern lassen
  if (now - lastShiftMs > SHIFT_EVERY_MS) {
    lastShiftMs = now;
    shiftPhase = (shiftPhase + 1) & 0x03;
    const int8_t dx[4] = { 0, 3, 0, -3 };
    const int8_t dy[4] = { 0, 2, 4, 2 };
    lv_obj_set_pos(root, dx[shiftPhase], dy[shiftPhase]);
  }
}
