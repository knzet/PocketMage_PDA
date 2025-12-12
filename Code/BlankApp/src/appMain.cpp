// PocketMage V3.0
// @knzet 2025



#include <pocketmage.h>

#include <vector>

// Card image size
#define CARD_W 128
#define CARD_H 218

#define TOTAL_CARDS 22

static std::vector<int> drawnCards;  // Keeps track of indices already drawn

static constexpr const char* TAG = "TAROT";
static volatile bool alreadyDrawnThisEinkPage = false;

int x = 0;
bool initialized = false;
char inchar;
String cardName;

int selectedCard = -1;
bool tarotLoaded = false;

bool drawTarot(int idx) {
  char path[32];
  snprintf(path, sizeof(path), "/apps/images/ar%02d.bin", idx);

  const int BYTES = CARD_W * CARD_H / 8;
  static uint8_t tarotImage[BYTES];
  static volatile bool sdActive = true;
  OLED().setSD(&sdActive);
  OLED().oledWord("Drawing a card...");
  if (!SD().readBinaryFile(path, tarotImage, BYTES)) {
    ESP_LOGI(TAG, "ERR: Failed to read %s\n", path);

    OLED().oledWord("SD Read Error");
    return false;
  }
  sdActive = false;

  display.setRotation(3);
  display.setFullWindow();
  display.setTextColor(GxEPD_BLACK);  // solid black

  int cardX = (display.width() - CARD_W) / 2;
  int cardY = (display.height() - CARD_H) / 2;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(cardX, cardY, CARD_W, CARD_H, GxEPD_BLACK);
    display.drawBitmap(cardX, cardY, tarotImage, CARD_W, CARD_H, GxEPD_BLACK);
    // EINK().drawStatusBar(cardName);
  } while (display.nextPage());
  String msg = String(idx) + " - " + cardName;
  OLED().oledWord(msg);

  return true;
}

// ADD PROCESS/KEYBOARD APP SCRIPTS HERE
void processKB() {
  inchar = KB().updateKeypress();
  if (inchar == 21) {
    alreadyDrawnThisEinkPage = false;

  } else if (inchar != 0) {
    // Return to pocketMage OS
    rebootToPocketMage();
  }

  delay(10);
}

void applicationEinkHandler() {
  if (!alreadyDrawnThisEinkPage) {
    alreadyDrawnThisEinkPage = true;

    const char* majorArcana[] = {
        "The Fool",         "The Magician", "The High Priestess", "The Empress", "The Emperor",
        "The Hierophant",   "The Lovers",   "The Chariot",        "Strength",    "The Hermit",
        "Wheel of Fortune", "Justice",      "The Hanged Man",     "Death",       "Temperance",
        "The Devil",        "The Tower",    "The Star",           "The Moon",    "The Sun",
        "Judgement",        "The World"};

    static const int MAJOR_ARCANA_COUNT = sizeof(majorArcana) / sizeof(majorArcana[0]);
    // Pick a random card at app start and when user draws another
    static const int TAROT_COUNT = sizeof(majorArcana) / sizeof(majorArcana[0]);
    if (drawnCards.size() >= TOTAL_CARDS) {
      ESP_LOGI(TAG, "Listing directory %s\r\n", dirname);
      OLED().oledWord("End of deck reached!", true);
      return;
    }

    int idx;
    do {
      idx = esp_random() % TOTAL_CARDS;
    } while (std::find(drawnCards.begin(), drawnCards.end(), idx) != drawnCards.end());

    drawnCards.push_back(idx);  // Mark this card as drawn

    cardName = majorArcana[idx];

    ESP_LOGI(TAG, "cardname: %s\r\n", cardName);

    drawTarot(idx);
  }
}

/////////////////////////////////////////////////////////////
//  ooo        ooooo       .o.       ooooo ooooo      ooo  //
//  `88.       .888'      .888.      `888' `888b.     `8'  //
//   888b     d'888      .8"888.      888   8 `88b.    8   //
//   8 Y88. .P  888     .8' `888.     888   8   `88b.  8   //
//   8  `888'   888    .88ooo8888.    888   8     `88b.8   //
//   8    Y     888   .8'     `888.   888   8       `888   //
//  o8o        o888o o88o     o8888o o888o o8o        `8   //
/////////////////////////////////////////////////////////////
// SETUP

void setup() {
  PocketMage_INIT();
}

void loop() {
  // Check battery
  pocketmage::power::updateBattState();

  // Run KB loop
  processKB();

  // Yield to watchdog
  vTaskDelay(50 / portTICK_PERIOD_MS);
  yield();
}

// migrated from einkFunc.cpp
void einkHandler(void* parameter) {
  vTaskDelay(pdMS_TO_TICKS(250));
  for (;;) {
    applicationEinkHandler();

    vTaskDelay(pdMS_TO_TICKS(50));
    yield();
  }
}