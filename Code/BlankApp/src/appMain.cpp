// PocketMage V3.0
// @Ashtf 2025

#include <pocketmage.h>

static constexpr const char* TAG = "MAIN";

int x = 0;
bool initialized = 0;
char inchar;
String cardName;

// ADD PROCESS/KEYBOARD APP SCRIPTS HERE
void processKB() {
  if (!initialized) {
    u8g2.clearBuffer();
    u8g2.drawStr(x, x, "Tarot Card");
    u8g2.sendBuffer();
  }
  inchar = KB().updateKeypress();

  if (inchar != 0) {
    // Return to pocketMage OS
    rebootToPocketMage();
  }

  delay(10);
}

void applicationEinkHandler() {
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        display.setRotation(3);
        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);

        int cardWidth = 120;
        int cardHeight = 160;
        int cardX = (display.width() - cardWidth) / 2;
        int cardY = (display.height() - cardHeight) / 2;
        display.drawRect(cardX, cardY, cardWidth, cardHeight, GxEPD_BLACK);
        Serial.printf("cardname: %s",cardName);
        // Status bar with random card name
        EINK().drawStatusBar(cardName);

        EINK().refresh();
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
    randomSeed(millis());

    // Pick a random card at app start
    const char* tarotDeck[] = { /* ... */ };
    static const int TAROT_COUNT = sizeof(tarotDeck) / sizeof(tarotDeck[0]);
    int idx = random(TAROT_COUNT);
    cardName = tarotDeck[idx];
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