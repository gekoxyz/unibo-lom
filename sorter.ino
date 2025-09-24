#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library

const unsigned long COIN_COOLDOWN_MS = 150;
const int NUM_COIN_TYPES = 5;

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
const int CLK_PIN = A1;  // The "Clock" pin
const int DT_PIN  = A2;  // The "Data" pin
const int SW_PIN  = A3;  // The "Switch" (button) pin
float encoder_counter = 0.0;
int last_encoder_clock_state;

const unsigned long ENCODER_BUTTON_DEBOUNCE_MS = 150;
const unsigned long ENCODER_ROTATION_DEBOUNCE_MS = 10;

int sensors_locked = LOW;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void check_coin_sensor(Coin &coin, unsigned long current_time);
float getTotal();

void setup() {
  // coin counter initialization
  Serial.begin(9600);
  while (!Serial);
  Serial.println("IR sensor coin counter | initialized");
  Serial.println("------------------------------------");
  for (int i = 0; i < NUM_COIN_TYPES; i++) pinMode(coins[i].pin, INPUT_PULLUP);

  // display initialization
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);

  // encoder
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);
  last_encoder_clock_state = digitalRead(CLK_PIN);
}

void loop() {
  unsigned long current_time = millis();

  handleEncoderButton();

  if (!sensors_locked) {
    for (int i = 0; i < NUM_COIN_TYPES; i++) check_coin_sensor(coins[i], current_time);
  } else {
    handleEncoderRotation();
  }
  updateDisplay();
}

void handleEncoderButton() {
  static unsigned long last_debounce_time = 0;
  static int last_button_state = HIGH;
  
  int current_button_state = digitalRead(SW_PIN);

  // Check for a falling edge (button press)
  if (last_button_state == HIGH && current_button_state == LOW) {
    if (millis() - last_debounce_time > ENCODER_BUTTON_DEBOUNCE_MS) {
      // Toggle the state
      sensors_locked = !sensors_locked;
      last_debounce_time = millis();
      if (sensors_locked) {
        Serial.println("Sensors LOCKED. Entering adjustment mode.");
        encoder_counter = 0.0;
      } else {
        Serial.println("Sensors UNLOCKED. Exiting adjustment mode.");
        applyRemoval(encoder_counter);
        encoder_counter = 0.0;
      }
    }
  }
  last_button_state = current_button_state;
}

void applyRemoval(float amount_to_remove) {
  int target_amount_cents = round(amount_to_remove * 100);
  int remaining_to_remove_cents = target_amount_cents;
  if (remaining_to_remove_cents <= 0) {
    Serial.println("Nothing to remove!");
    return;
  }
  Serial.println("Attempting to remove: ");
  Serial.print(amount_to_remove, 2);
  Serial.println(" EUR")

  for (int i = NUM_COIN_TYPES - 1; i >= 0; i--) {
    Coin &coin = coins[i];
    int coin_value_cents = round(coin.value * 100);
    if (coin_value_cents <= 0) continue;
    int num_possible_from_value = remaining_to_remove_cents / coin_value_cents;
    int num_available = coin.count;
    // take the minimum amount of coins to ensure we don't remove more than we have or more 
    // than needed for the target amount
    int num_to_remove = min(num_possible_from_value, num_available);

    if (num_to_remove > 0) {
      coin.count -= num_to_remove;
      remaining_to_remove_cents -= (num_to_remove * coin_value_cents);
      Serial.print("Removed ");
      Serial.print(num_to_remove);
      Serial.print("x");
      Serial.print(coin.name);
      Serial.println("");
    }
    if (remaining_to_remove_cents <= 0) break;
  }

  if (remaining_to_remove_cents > 0) {
    Serial.println("[WARNING] Could not remove the full amount. Coins left: ")
    Serial.print((float)remaining_to_remove_cents/100.0, 2);
    Serial.println(" EUR")
  } else {
    Serial.println("Removal successful!");
  }
  Serial.print("New Total: ");
  Serial.println(getTotal(), 2);
}

/**
 * @brief Reads the encoder rotation. Called only when in the "locked" state.
 */
void handleEncoderRotation() {
  static unsigned long last_rotation_time = 0;
  
  int current_clk_state = digitalRead(CLK_PIN);

  if (current_clk_state != last_encoder_clock_state) {
    if (millis() - last_rotation_time > ENCODER_ROTATION_DEBOUNCE_MS) {
      if (current_clk_state == LOW) {
        if (digitalRead(DT_PIN) == current_clk_state) {
          encoder_counter += 0.1; // Clockwise
        } else {
          encoder_counter = max(0.0, encoder_counter - 0.1)
        }
        
        Serial.print("Adjustment value: ");
        Serial.println(encoder_counter, 2);
      } 
      last_rotation_time = millis();
    }
  }
  last_encoder_clock_state = current_clk_state;
}


/**
 * @brief Updates the TFT display based on the current system state.
 * This function uses a simple timer to avoid flickering.
 */
void updateDisplay() {
  static unsigned long last_display_update = 0;
  const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 200;

  if (millis() - last_display_update > DISPLAY_UPDATE_INTERVAL_MS) {
    tft.setTextSize(1);
    
    if (!sensors_locked) {
      // --- UNLOCKED DISPLAY ---
      tft.setCursor(2, 2);
      tft.print("Total: ");
      tft.print(getTotal(), 2);
      tft.print(" EUR  "); // Padding to clear old text

      tft.setCursor(2, 11);
      tft.print("SENSORI SBLOCCATI");

      // Clear the adjustment line
      tft.fillRect(0, 20, tft.width(), 10, ST77XX_BLACK);
      
    } else {
      // --- LOCKED DISPLAY ---
      tft.setCursor(2, 2);
      tft.print("Total: ");
      tft.print(getTotal(), 2);
      tft.print(" EUR  ");

      tft.setCursor(2, 11);
      tft.print("SENSORI BLOCCATI "); // Padding
      
      tft.setCursor(2, 20);
      tft.print("Rimuovi: ");
      tft.print(encoder_counter, 2);
      tft.print(" EUR  ");
    }
    last_display_update = millis();
  }
}

/**
  * Check a coin sensor for coins passing by
  */
void check_coin_sensor(Coin &coin, unsigned long current_time) {
  int current_state = digitalRead(coin.pin);
  if (coin.prev_state == HIGH && current_state == LOW) {
    if (current_time - coin.last_detection_time > COIN_COOLDOWN_MS) {
      coin.count++;
      coin.last_detection_time = current_time;
      Serial.print(coin.name);
      Serial.print(" coin detected! | Total: ");
      Serial.println(getTotal(), 2); // Print total with 2 decimal places
    }
  }
  coin.prev_state = current_state;
}

/**
  * Calculate the total amount of coins that the user has
  */
float getTotal() {
  float totalValue = 0.0;
  for (int i = 0; i < NUM_COIN_TYPES; i++) totalValue += (coins[i].count * coins[i].value);
  return totalValue;
}