#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <EEPROM.h>
#include <SPI.h>

const unsigned long COIN_COOLDOWN_MS = 150;
const int NUM_COIN_TYPES = 5;
const int BUZZER = 7;

struct Coin {
  const char* name;
  const uint8_t pin;
  const float value;
  int count;
  int prev_state;
  unsigned long last_detection_time;
};

Coin coins[NUM_COIN_TYPES] = {
  {"10 cent", 6, 0.10, 0, HIGH, 0},
  {"20 cent", 5, 0.20, 0, HIGH, 0},
  {"50 cent", 3, 0.50, 0, HIGH, 0},
  {"1 euro", 4, 1.00, 0, HIGH, 0},
  {"2 euro", 2, 2.00, 0, HIGH, 0},
};

// display
const int TFT_CS = 10;
const int TFT_RST = 8;
const int TFT_DC = 9;

// encoder
const int CLK_PIN = A1;  // clock
const int DT_PIN = A2;   // data
const int SW_PIN = A3;   // switch
int encoder_counter = 0;
int last_encoder_clock_state;
int coin_index_for_removal = 0;

const unsigned long ENCODER_ROTATION_DEBOUNCE_MS = 1;
const unsigned long ENCODER_BUTTON_DEBOUNCE_MS = 25;
const unsigned long DOUBLE_CLICK_WINDOW_MS = 350;
const unsigned long LONG_PRESS_DURATION_MS = 4000;  // reset timer

const int DISPLAY_X_OFFSET = 6;

int coins_to_remove = 0;
int sensors_locked = LOW;
int sensors_locked_submenu = LOW;
bool display_needs_update = true;

// audio feedback
unsigned long buzzer_timer = 0;
int buzzer_state = 0;  // 0=Idle, 1=First Tone, 2=Second Tone

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

enum ClickType {
  NO_CLICK,
  SINGLE_CLICK,
  DOUBLE_CLICK,
  LONG_PRESS
};

void setup() {
  Serial.begin(19200);
  while (!Serial);
  Serial.println("------------------------------------");
  Serial.println("IR sensor coin counter | initialized");
  Serial.println("------------------------------------");
  for (int i = 0; i < NUM_COIN_TYPES; i++) pinMode(coins[i].pin, INPUT_PULLUP);

  load_coin_count();

  tft.initR(INITR_144GREENTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);

  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  last_encoder_clock_state = digitalRead(CLK_PIN);
}

void loop() {
  unsigned long current_time = millis();

  ClickType button_event = checkEncoderButton();

  update_coin_feedback();

  switch (button_event) {
    case LONG_PRESS:
      // reset all coin counts
      reset_all_coins();
      Serial.println("LONG PRESS DETECTED - ALL COINS RESET!");
      tft.fillScreen(ST77XX_BLACK);
      display_needs_update = true;
      trigger_reset_feedback();
      break;
    case DOUBLE_CLICK:
      sensors_locked = !sensors_locked;
      if (sensors_locked) {
        Serial.println("Sensors LOCKED. Entering adjustment mode.");
        tft.fillScreen(ST77XX_BLACK);
      } else {
        Serial.println("Sensors UNLOCKED. Exiting adjustment mode.");
        tft.fillScreen(ST77XX_BLACK);
      }
      display_needs_update = true;
      break;
    case SINGLE_CLICK:
      if (sensors_locked) {
        if (!sensors_locked_submenu) {
          // entering submenu
          sensors_locked_submenu = true;
          coins_to_remove = 0;  // reset counter when entering
          Serial.print("GO TO SUBMENU OF CURRENT COIN");
          tft.fillScreen(ST77XX_BLACK);
        } else {
          // exiting submenu (confirm removal)
          sensors_locked_submenu = false;

          // apply the removal
          if (coins_to_remove > 0) {
            coins[encoder_counter].count -= coins_to_remove;
            save_coin_count(encoder_counter);
            Serial.print("Removed ");
            Serial.print(coins_to_remove);
            Serial.println(" coins.");
          }
          Serial.print("EXIT FROM SUBMENU");
          tft.fillScreen(ST77XX_BLACK);
        }
        display_needs_update = true;
      }
      break;
    case NO_CLICK:
    default:
      break;
  }

  if (!sensors_locked) {
    // check for coins passing in front of the IR sensor
    for (int i = 0; i < NUM_COIN_TYPES; i++) check_coin_sensor(i, current_time);
  } else {
    // handle the encoder rotation to manage the removal submenu
    handleEncoderRotation();
  }

  updateDisplay();
}

ClickType checkEncoderButton() {
  static int last_stable_state = HIGH;
  static int last_flicker_state = HIGH;
  static unsigned long last_debounce_time = 0;
  static int click_count = 0;
  static unsigned long last_press_time = 0;
  static unsigned long button_press_start = 0;
  static bool long_press_triggered = false;

  int current_flicker_state = digitalRead(SW_PIN);
  unsigned long current_time = millis();

  if (current_flicker_state != last_flicker_state) {
    last_debounce_time = current_time;
  }
  last_flicker_state = current_flicker_state;

  if ((current_time - last_debounce_time) > ENCODER_BUTTON_DEBOUNCE_MS) {
    if (current_flicker_state != last_stable_state) {
      last_stable_state = current_flicker_state;

      if (last_stable_state == LOW) {
        // button pressed down
        button_press_start = current_time;
        long_press_triggered = false;
        click_count++;

        if (click_count == 1) {
          last_press_time = current_time;
        } else if (click_count == 2) {
          if ((current_time - last_press_time) < DOUBLE_CLICK_WINDOW_MS) {
            Serial.println("DETECTED DOUBLE CLICK");
            click_count = 0;
            return DOUBLE_CLICK;
          } else {
            click_count = 1;
            last_press_time = current_time;
          }
        }
      } else {
        // button released
        long_press_triggered = false;
      }
    }
  }

  // check for long press while button is held
  if (last_stable_state == LOW && !long_press_triggered) {
    if ((current_time - button_press_start) >= LONG_PRESS_DURATION_MS) {
      long_press_triggered = true;
      click_count = 0;
      Serial.println("DETECTED LONG PRESS");
      return LONG_PRESS;
    }
  }

  // check for single click timeout
  if (click_count == 1 && (current_time - last_press_time) > DOUBLE_CLICK_WINDOW_MS) {
    Serial.println("DETECTED SINGLE CLICK");
    click_count = 0;
    return SINGLE_CLICK;
  }

  return NO_CLICK;
}

void handleEncoderRotation() {
  static unsigned long last_rotation_time = 0;
  int current_clk_state = digitalRead(CLK_PIN);
  if (current_clk_state != last_encoder_clock_state) {
    if (millis() - last_rotation_time > ENCODER_ROTATION_DEBOUNCE_MS) {
      if (current_clk_state == LOW) {
        bool direction_up = (digitalRead(DT_PIN) == current_clk_state);
        if (sensors_locked_submenu) {
          // choosing amount to remove
          int max_removable = coins[encoder_counter].count;
          if (direction_up) {
            if (coins_to_remove < max_removable) {
              coins_to_remove++;
            }
          } else {
            if (coins_to_remove > 0) {
              coins_to_remove--;
            }
          }
        } else {
          // choosing coin type
          if (direction_up) {
            encoder_counter++;
            encoder_counter = encoder_counter % NUM_COIN_TYPES;
          } else {
            encoder_counter--;
            if (encoder_counter < 0) {
              encoder_counter = NUM_COIN_TYPES - 1;
            }
          }
        }
        display_needs_update = true;
      }
      last_rotation_time = millis();
    }
  }
  last_encoder_clock_state = current_clk_state;
}

void updateDisplay() {
  if (display_needs_update) {
    tft.setTextSize(1);
    int cursor_y_coord = 8;
    tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);

    if (!sensors_locked) {
      tft.print("#    Monete");
      cursor_y_coord += 10;
      tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
      tft.print("--------------------");
      cursor_y_coord += 10;
      tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
      for (int i = 0; i < NUM_COIN_TYPES; i++) {
        tft.print(coins[i].count);
        tft.print("    ");
        tft.print(coins[i].name);
        cursor_y_coord += 10;
        tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
      }
      tft.setTextSize(2);
      cursor_y_coord += 6;
      tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
      tft.print("Tot: ");
      tft.print(get_total(), 2);
      tft.setTextSize(1);
    } else {
      tft.print(" MODALITA RIMOZIONE");
      cursor_y_coord += 10;
      tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
      tft.print("--------------------");
      cursor_y_coord += 10;
      tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
      
      if (sensors_locked_submenu) {
        // sensors locked in submenu -> manage coin to remove
        tft.print("Rimozione di ");
        tft.print(coins[encoder_counter].name);

        cursor_y_coord += 10;
        tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
        tft.print("Disponibili: ");
        tft.print(coins[encoder_counter].count);

        // remove ghost text
        cursor_y_coord += 10;
        tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
        tft.print("                 ");

        cursor_y_coord += 10;
        tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
        tft.print("Stai rimuovendo: ");
        tft.print(coins_to_remove);

        // remove ghost text
        cursor_y_coord += 10;
        tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
        tft.print("                 ");
      } else {
        // sensors locked not in submenu -> selecting coin to remove
        for (int i = 0; i < NUM_COIN_TYPES; i++) {
          // if current coin is selected then print >
          if (encoder_counter == i) {
            tft.print("> ");
          } else {
            tft.print("  ");
          }
          tft.print(coins[i].count);
          tft.print("  ");
          tft.print(coins[i].name);
          cursor_y_coord += 10;
          tft.setCursor(DISPLAY_X_OFFSET, cursor_y_coord);
        }
      }
    }
    display_needs_update = false;
  }
}

void check_coin_sensor(int coin_index, unsigned long current_time) {
  Coin& coin = coins[coin_index];
  int current_state = digitalRead(coin.pin);
  if (coin.prev_state == HIGH && current_state == LOW) {
    if (current_time - coin.last_detection_time > COIN_COOLDOWN_MS) {
      coin.count++;
      save_coin_count(coin_index);
      coin.last_detection_time = current_time;
      trigger_coin_feedback();
      Serial.print(coin.name);
      Serial.print(" coin detected! | Total: ");
      Serial.println(get_total(), 2);
      display_needs_update = true;
    }
  }
  coin.prev_state = current_state;
}

float get_total() {
  float totalValue = 0.0;
  for (int i = 0; i < NUM_COIN_TYPES; i++) totalValue += (coins[i].count * coins[i].value);
  return totalValue;
}

void trigger_coin_feedback() {
  buzzer_state = 1;
  buzzer_timer = millis();
  tone(BUZZER, 3000);
}

void update_coin_feedback() {
  if (buzzer_state == 0) return;  // idle buzzer

  unsigned long current_time = millis();

  if (buzzer_state == 1) {
    if (current_time - buzzer_timer >= 65) {
      buzzer_state = 2;
      buzzer_timer = current_time;
      tone(BUZZER, 2000);
    }
  } else if (buzzer_state == 2) {
    if (current_time - buzzer_timer >= 25) {
      buzzer_state = 0;
      noTone(BUZZER);
    }
  }
}

void reset_all_coins() {
  for (int i = 0; i < NUM_COIN_TYPES; i++) {
    coins[i].count = 0;
    save_coin_count(i);
  }
  Serial.println("All coin counts have been reset to 0.");
}

void trigger_reset_feedback() {
  tone(BUZZER, 2500, 150);
  delay(150);
  tone(BUZZER, 2000, 150);
  delay(150);
  tone(BUZZER, 1500, 150);
}

void load_coin_count() {
  int address = 0;
  for (int i = 0; i < NUM_COIN_TYPES; i++) {
    int storedValue;
    EEPROM.get(address, storedValue);

    // Check if the value is valid.
    // If it's negative, we assume it's fresh/invalid and set to 0.
    if (storedValue < 0) {
      coins[i].count = 0;
    } else {
      coins[i].count = storedValue;
    }

    address += sizeof(int);  // Move address forward by size of an integer
  }
  Serial.println("Coin counts loaded from EEPROM.");
}

void save_coin_count(int index) {
  int address = index * sizeof(int);
  EEPROM.put(address, coins[index].count);
}