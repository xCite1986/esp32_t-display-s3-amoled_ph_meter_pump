#include "PanelUi.h"
#include "PanelNet.h"

#include <lvgl.h>
#include <LilyGo_AMOLED.h>

extern LilyGo_Class amoled;

// --- Farben ----------------------------------------------------------------
// Bewusst ohne Blau und ohne Rot: auf schwarzem AMOLED trennen Weiss, Gelb und
// Gruen deutlich besser, gerade wenn das Panel gedimmt oder unbeleuchtet wirkt.
static const uint32_t C_BG     = 0x000000;   // echtes Schwarz: Pixel aus, kein Einbrennen
static const uint32_t C_CARD   = 0x161A20;
static const uint32_t C_TEXT   = 0xFFFFFF;
static const uint32_t C_MUTED  = 0x9AA6B8;
static const uint32_t C_OK     = 0x4FD98A;   // im Zielbereich
static const uint32_t C_WARN   = 0xFFE47A;   // leicht daneben
static const uint32_t C_ERR    = 0xFFC61A;   // Stoerung / gesperrt
static const uint32_t C_ACC    = 0xFFFFFF;   // Hervorhebung

// --- Helligkeit / Einbrennschutz -------------------------------------------
static const uint8_t  BRIGHT_ON      = 240;
static const uint8_t  BRIGHT_DIM     = 25;
static const uint32_t DIM_AFTER_MS   = 60000;
static const uint32_t SHIFT_EVERY_MS = 90000;
static const uint32_t DIALOG_TIMEOUT = 12000;

// --- pH-Anzeige ------------------------------------------------------------
// Die groesste eingebaute LVGL-Schrift ist Montserrat 48 - auf 1,91 Zoll sind
// das nur rund 4 mm Zifferhoehe. Der Wert wird deshalb in eine knapp auf das
// Ziffernband zugeschnittene Canvas gezeichnet und diese als Bild vierfach
// vergroessert dargestellt: rund 136 px bzw. 11 mm.
#define PH_CV_W   126
#define PH_CV_H   44
#define PH_TXT_Y  (-11)      // schneidet die Leerzeile ueber den Ziffern weg
#define PH_ZOOM   1024       // 256 = 1x, 1024 = 4x
static lv_color_t phBuf[PH_CV_W * PH_CV_H];

static int16_t scrW = 536, scrH = 240;

static lv_obj_t *root, *phCanvas;
static lv_obj_t *lbl24Cap, *lbl24, *lblState, *lblLocks;
static lv_obj_t *overlay, *lblAsk, *lblAskSub, *btnYes, *btnNo;
static lv_obj_t *setup_, *lblSetupSsid, *lblSetupPass, *lblSetupUrl;
static lv_obj_t *toast, *lblToast;

static bool     dimmed       = false;
static uint32_t wokeAtMs     = 0;
static uint32_t dialogOpenMs = 0;
static uint32_t lastShiftMs  = 0;
static uint32_t toastUntil   = 0;
static int8_t   shiftPhase   = 0;

static char     lastPhTxt[16] = "";
static uint32_t lastPhCol     = 0xFFFFFFFF;

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

// pH-Wert in die Canvas zeichnen - nur bei Aenderung, das Skalieren kostet Zeit
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

// --- Ereignisse ------------------------------------------------------------
static bool swallowWakeTap() {
  // Der Tipp, der das gedimmte Display aufweckt, darf nichts ausloesen.
  return (millis() - wokeAtMs) < 450;
}

static void onScreenClick(lv_event_t *) {
  if (swallowWakeTap()) return;
  if (!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN)) return;
  if (!lv_obj_has_flag(setup_, LV_OBJ_FLAG_HIDDEN)) return;   // im AP-Modus nichts
  if (netMode() != NM_STA) return;
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

  // --- pH: nimmt fast die ganze Flaeche ein ---
  phCanvas = lv_canvas_create(root);
  lv_canvas_set_buffer(phCanvas, phBuf, PH_CV_W, PH_CV_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(phCanvas, scrW / 2 - PH_CV_W / 2, 96 - PH_CV_H / 2);
  lv_img_set_zoom(phCanvas, PH_ZOOM);
  lv_img_set_antialias(phCanvas, true);
  lv_obj_add_flag(phCanvas, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_canvas_fill_bg(phCanvas, lv_color_hex(C_BG), LV_OPA_COVER);
  drawPh("--.--", C_MUTED);

  // --- Fusszeile links: Menge der letzten 24 Stunden ---
  lbl24Cap = mkLabel(root, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_LEFT, 14, 188, "LETZTE 24 H");
  lbl24    = mkLabel(root, &lv_font_montserrat_32, C_ACC,   LV_ALIGN_TOP_LEFT, 14, 204, "-- ml");

  // --- Fusszeile rechts: Zustand und Sperrgruende ---
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

  // --- Bestaetigungsdialog ---
  overlay = lv_obj_create(scr);
  styleFlat(overlay);
  lv_obj_set_size(overlay, scrW, scrH);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);   // schluckt Klicks nach hinten

  lblAsk    = mkLabel(overlay, &lv_font_montserrat_28, C_TEXT, LV_ALIGN_TOP_MID, 0, 16, "?");
  lblAskSub = mkLabel(overlay, &lv_font_montserrat_20, C_ERR,  LV_ALIGN_TOP_MID, 0, 54, "");
  mkLabel(overlay, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_MID, 0, 82,
          "Die Anlage prueft die Grenzen erneut.");

  btnNo = lv_btn_create(overlay);
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

  btnYes = lv_btn_create(overlay);
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

  // --- Einrichtungsbildschirm (Access-Point-Modus) ---
  setup_ = lv_obj_create(scr);
  styleFlat(setup_);
  lv_obj_set_size(setup_, scrW, scrH);
  lv_obj_set_style_bg_color(setup_, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(setup_, LV_OPA_COVER, 0);
  lv_obj_add_flag(setup_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(setup_, LV_OBJ_FLAG_CLICKABLE);

  mkLabel(setup_, &lv_font_montserrat_24, C_ERR, LV_ALIGN_TOP_MID, 0, 10, "WLAN-EINRICHTUNG");
  mkLabel(setup_, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_LEFT, 18, 52, "1.  Am Handy verbinden mit");
  lblSetupSsid = mkLabel(setup_, &lv_font_montserrat_28, C_TEXT, LV_ALIGN_TOP_LEFT, 44, 70, PANEL_AP_SSID);
  lblSetupPass = mkLabel(setup_, &lv_font_montserrat_20, C_MUTED, LV_ALIGN_TOP_LEFT, 44, 104,
                         "Passwort:  " PANEL_AP_PASS);
  mkLabel(setup_, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_TOP_LEFT, 18, 140, "2.  Im Browser oeffnen");
  lblSetupUrl  = mkLabel(setup_, &lv_font_montserrat_28, C_TEXT, LV_ALIGN_TOP_LEFT, 44, 158, "http://192.168.4.1");
  mkLabel(setup_, &lv_font_montserrat_16, C_MUTED, LV_ALIGN_BOTTOM_MID, 0, -8,
          "Netz waehlen, Passwort eingeben - das Panel startet dann neu.");

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
  // --- Einrichtungsmodus deckt alles ab ---
  if (netMode() == NM_AP) {
    if (lv_obj_has_flag(setup_, LV_OBJ_FLAG_HIDDEN)) {
      lv_label_set_text(lblSetupUrl, ("http://" + netApIp()).c_str());
      lv_obj_clear_flag(setup_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(setup_);
    }
    return;
  }
  if (!lv_obj_has_flag(setup_, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(setup_, LV_OBJ_FLAG_HIDDEN);

  netLock();
  PanelState s = netState();     // Kopie ziehen, dann sofort freigeben
  netUnlock();

  // --- pH ---
  char phTxt[16];
  uint32_t phCol = C_MUTED;
  if (s.online && s.phValid) {
    snprintf(phTxt, sizeof(phTxt), "%.2f", s.ph);
    phCol = C_OK;
    if (s.ph > s.setpoint + 0.30f) phCol = C_WARN;
    if (s.ph > s.setpoint + 0.60f) phCol = C_ERR;
    if (s.ph < s.setpoint - 0.20f) phCol = C_ERR;
  } else {
    strcpy(phTxt, "--.--");
  }
  drawPh(phTxt, phCol);

  // --- Menge ---
  lv_label_set_text_fmt(lbl24, "%.1f ml", s.last24h);
  lv_label_set_text_fmt(lbl24Cap, "LETZTE 24 H   (heute %.1f / %.0f)", s.today, s.maxDaily);

  // --- Zustand ---
  if (!s.online) {
    lv_label_set_text(lblState, "offline");
    lv_obj_set_style_text_color(lblState, lv_color_hex(C_ERR), 0);
    lv_label_set_text(lblLocks, "keine Verbindung zur Anlage");
  } else if (netDosePending() || s.pumpRun) {
    lv_label_set_text_fmt(lblState, "Dosiert  %.2f / %.2f ml", s.pumpMl, s.pumpTarget);
    lv_obj_set_style_text_color(lblState, lv_color_hex(C_ACC), 0);
    lv_label_set_text(lblLocks, "");
  } else {
    lv_label_set_text(lblState, s.state);
    uint32_t col = C_MUTED;
    if (s.fault || s.estop)                 col = C_ERR;
    else if (strcmp(s.state, "Bereit") == 0) col = C_OK;
    lv_obj_set_style_text_color(lblState, lv_color_hex(col), 0);
    // Ohne gueltigen Messwert ist der Sensorgrund wichtiger als die Sperrliste.
    lv_label_set_text(lblLocks, s.phValid ? s.locks : s.phStatus);
  }
  lv_obj_align(lblLocks, LV_ALIGN_TOP_RIGHT, -14, 216);
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
