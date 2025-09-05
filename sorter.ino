// Assign each pin to a specific coin type
const int SENSOR_10CENT_PIN = 6;
const int SENSOR_20CENT_PIN = 5;
const int SENSOR_50CENT_PIN = 3;
const int SENSOR_1EURO_PIN  = 4;
const int SENSOR_2EURO_PIN  = 2;

// Cooldown period in milliseconds to prevent double counting
const unsigned long COOLDOWN_MS = 150; 

// Variables to store the previous state of each sensor
int prev_10cent_state;
int prev_20cent_state;
int prev_50cent_state;
int prev_1euro_state;
int prev_2euro_state;

// Variables to store the time of the last valid detection for each sensor
unsigned long last_10cent_time = 0;
unsigned long last_20cent_time = 0;
unsigned long last_50cent_time = 0;
unsigned long last_1euro_time  = 0;
unsigned long last_2euro_time  = 0;

// Coin counters
int counter_10cent = 0;
int counter_20cent = 0;
int counter_50cent = 0;
int counter_1euro  = 0;
int counter_2euro  = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial); // Wait for Serial monitor to open

  Serial.println("IR Sensor Coin Counter - Initialized");
  Serial.println("------------------------------------");

  // Set sensor pins as inputs with internal pull-up resistors
  pinMode(SENSOR_10CENT_PIN, INPUT_PULLUP);
  pinMode(SENSOR_20CENT_PIN, INPUT_PULLUP);
  pinMode(SENSOR_50CENT_PIN, INPUT_PULLUP);
  pinMode(SENSOR_1EURO_PIN, INPUT_PULLUP);
  pinMode(SENSOR_2EURO_PIN, INPUT_PULLUP);

  // Read the initial state of the sensors
  // (They should be HIGH when nothing is blocking them)
  prev_10cent_state = digitalRead(SENSOR_10CENT_PIN); 
  prev_20cent_state = digitalRead(SENSOR_20CENT_PIN); 
  prev_50cent_state = digitalRead(SENSOR_50CENT_PIN); 
  prev_1euro_state  = digitalRead(SENSOR_1EURO_PIN); 
  prev_2euro_state  = digitalRead(SENSOR_2EURO_PIN); 
}

// Function to calculate the total value of all coins
float getTotal() {
  return (0.1 * counter_10cent) + (0.2 * counter_20cent) + (0.5 * counter_50cent) + (1.0 * counter_1euro) + (2.0 * counter_2euro);
}

void loop() {
  // Get the current time at the start of the loop
  unsigned long current_time = millis();

  // --- Read current sensor states ---
  int current_10cent_state = digitalRead(SENSOR_10CENT_PIN);
  int current_20cent_state = digitalRead(SENSOR_20CENT_PIN);
  int current_50cent_state = digitalRead(SENSOR_50CENT_PIN);
  int current_1euro_state  = digitalRead(SENSOR_1EURO_PIN);
  int current_2euro_state  = digitalRead(SENSOR_2EURO_PIN);

  // --- Process 10 Cent Coin Sensor ---
  if (prev_10cent_state == HIGH && current_10cent_state == LOW) {
    if (current_time - last_10cent_time > COOLDOWN_MS) {
      counter_10cent++;
      last_10cent_time = current_time;
      Serial.print("10 Cent coin detected!  |  Total: ");
      Serial.println(getTotal(), 2);
    }
  }
  prev_10cent_state = current_10cent_state;

  // --- Process 20 Cent Coin Sensor ---
  if (prev_20cent_state == HIGH && current_20cent_state == LOW) {
    if (current_time - last_20cent_time > COOLDOWN_MS) {
      counter_20cent++;
      last_20cent_time = current_time;
      Serial.print("20 Cent coin detected!  |  Total: ");
      Serial.println(getTotal(), 2);
    }
  }
  prev_20cent_state = current_20cent_state;

  // --- Process 50 Cent Coin Sensor ---
  if (prev_50cent_state == HIGH && current_50cent_state == LOW) {
    if (current_time - last_50cent_time > COOLDOWN_MS) {
      counter_50cent++;
      last_50cent_time = current_time;
      Serial.print("50 Cent coin detected!  |  Total: ");
      Serial.println(getTotal(), 2);
    }
  }
  prev_50cent_state = current_50cent_state;

  // --- Process 1 Euro Coin Sensor ---
  if (prev_1euro_state == HIGH && current_1euro_state == LOW) {
    if (current_time - last_1euro_time > COOLDOWN_MS) {
      counter_1euro++;
      last_1euro_time = current_time;
      Serial.print("1 Euro coin detected!   |  Total: ");
      Serial.println(getTotal(), 2);
    }
  }
  prev_1euro_state = current_1euro_state;

  // --- Process 2 Euro Coin Sensor ---
  if (prev_2euro_state == HIGH && current_2euro_state == LOW) {
    if (current_time - last_2euro_time > COOLDOWN_MS) {
      counter_2euro++;
      last_2euro_time = current_time;
      Serial.print("2 Euro coin detected!   |  Total: ");
      Serial.println(getTotal(), 2);
    }
  }
  prev_2euro_state = current_2euro_state;
}