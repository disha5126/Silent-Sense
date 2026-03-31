#include <disha5126-project-1_v5_inferencing.h>

#define EI_CLASSIFIER_ALLOCATION_STATIC 1

#include <string.h>

// BLYNK DEFINES MUST BE BEFORE INCLUDE
#define BLYNK_TEMPLATE_ID "TMPL321GYr2LY"
#define BLYNK_TEMPLATE_NAME "ESP32APP"
#define BLYNK_AUTH_TOKEN "ZzDafKS5S0tkZt9E9030n8HtF8oNxr4t"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <driver/i2s.h>

char auth[] = "ZzDafKS5S0tkZt9E9030n8HtF8oNxr4t";
char ssid[] = "Disha's A56";
char pass[] = "disha1234";

bool wifi_started = false;

// I2S Pins
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 27
#define I2S_PORT I2S_NUM_0

// RGB LED PINS
#define PIN_RED   13
#define PIN_GREEN 12
#define PIN_BLUE  14
#define VIB_MOTOR_PIN 4

// PWM Channels
#define CH_RED   0
#define CH_GREEN 1
#define CH_BLUE  2

// Thresholds
#define THRESHOLD_DANGER 0.65
#define THRESHOLD_MUSIC  0.70 
#define THRESHOLD_SPEECH 0.70

// PRIORITY VALUES
#define PRIORITY_DANGER 3
#define PRIORITY_SPEECH 2
#define PRIORITY_MUSIC  1
#define PRIORITY_NOISE  0

// Globals
int16_t *inference_buffer;
int32_t *raw_i2s_buffer;

int current_priority = -1;
int last_sent_priority = -1;

bool blynk_cleared = false; 

// Ambient sound tracking
float ambient_level = 0;

// NEW PATTERN VIBRATION GLOBALS
unsigned long previous_vib_millis = 0;
int vib_mode = 0; // 0=off,1=danger,2=speech,3=music
int vib_step = 0;

// Blynk cooldown
unsigned long last_blynk_event_millis = 0;
const unsigned long BLYNK_COOLDOWN_MS = 10000;
bool first_blynk_event = true;

// Function Prototypes
void i2s_install();
void i2s_setpin();
bool record_audio();
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
void setRGB(int r, int g, int b);
void handle_notifications();

void setup() {
    Serial.begin(115200);
    delay(2000); 
    Serial.println("--- SilentSense Booting ---");

    inference_buffer = (int16_t*)malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int16_t));
    raw_i2s_buffer = (int32_t*)malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int32_t));

    if(inference_buffer == NULL || raw_i2s_buffer == NULL) {
        Serial.println("CRITICAL: RAM allocation failed!");
        while(1);
    }

    ledcSetup(CH_RED, 5000, 8);
    ledcSetup(CH_GREEN, 5000, 8);
    ledcSetup(CH_BLUE, 5000, 8);
    ledcAttachPin(PIN_RED, CH_RED);
    ledcAttachPin(PIN_GREEN, CH_GREEN);
    ledcAttachPin(PIN_BLUE, CH_BLUE);
    pinMode(VIB_MOTOR_PIN, OUTPUT);

    setRGB(50, 0, 0); 

    i2s_install();
    i2s_setpin();
    i2s_start(I2S_PORT);

    delay(500);

    Serial.println("Warming up classifier...");
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    run_classifier(&signal, &result, false);

    Serial.println("Classifier ready.");

    Serial.println("Starting Blynk...");
    Blynk.begin(auth, ssid, pass);

    if (Blynk.connected()) {
        Serial.println("Blynk Connected!");
        wifi_started = true;
    } else {
        Serial.println("Blynk Failed!");
    }

    setRGB(0,0,0);
}

void loop() {

    if(wifi_started && WiFi.status() == WL_CONNECTED) {
        if(!Blynk.connected()) {
            Blynk.connect();
        }
        Blynk.run();
    }

    handle_notifications();

    if(!record_audio()) return;

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.printf("ERR: Failed to run classifier (%d)\n", res);
        return;
    }

    for(size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Serial.print(result.classification[ix].label);
        Serial.print(": ");
        Serial.println(result.classification[ix].value);
    }
    Serial.println("=================");

    int best_idx = -1;
    float best_val = 0.0;

    for(size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if(result.classification[ix].value > best_val) {
            best_val = result.classification[ix].value;
            best_idx = ix;
        }
    }

    float noise_factor = ambient_level / 2000.0;

    float adaptive_danger = min(THRESHOLD_DANGER + noise_factor, 0.95);
    float adaptive_speech = min(THRESHOLD_SPEECH + noise_factor, 0.95);
    float adaptive_music  = min(THRESHOLD_MUSIC  + noise_factor, 0.95);

    int detected_priority = PRIORITY_NOISE;
    const char* label = "noise";

    if(best_idx >= 0) {
        label = result.classification[best_idx].label;

        if(strcmp(label,"danger")==0 && best_val > adaptive_danger) {
            detected_priority = PRIORITY_DANGER;
        }
        else if(strcmp(label,"speech")==0 && best_val > adaptive_speech) {
            detected_priority = PRIORITY_SPEECH;
        }
        else if(strcmp(label,"music")==0 && best_val > adaptive_music) {
            detected_priority = PRIORITY_MUSIC;
        }
    }

    if(detected_priority >= current_priority) {
        current_priority = detected_priority;
    }

    if(current_priority != last_sent_priority) {

        if(current_priority == PRIORITY_DANGER) {
            setRGB(255,0,0);

            vib_mode = 1;
            vib_step = 0;

            if(Blynk.connected()) {
                Blynk.virtualWrite(V1,3);
                Blynk.logEvent("danger_detected", "Emergency: Danger sound detected!");
            }
        }
        else if(current_priority == PRIORITY_SPEECH) {
            setRGB(0,255,0);

            vib_mode = 2;
            vib_step = 0;

            if(Blynk.connected()) {
                Blynk.virtualWrite(V1,1);
                Blynk.logEvent("speech_detected", "Speech detected nearby.");
            }
        }
        else if(current_priority == PRIORITY_MUSIC) {
            setRGB(0,0,255);

            vib_mode = 3;
            vib_step = 0;

            if(Blynk.connected()) {
                Blynk.virtualWrite(V1,2);
                Blynk.logEvent("music_detected", "Music detected.");
            }
        }
        else {
            setRGB(0,0,0);

            vib_mode = 0;
            digitalWrite(VIB_MOTOR_PIN, LOW);

            if(Blynk.connected()) {
                Blynk.virtualWrite(V1,0);
            }
        }

        last_sent_priority = current_priority;
    }

    if(detected_priority == PRIORITY_NOISE) {
        current_priority = PRIORITY_NOISE;
    }
}

void setRGB(int r,int g,int b) {
    ledcWrite(CH_RED,r);
    ledcWrite(CH_GREEN,g);
    ledcWrite(CH_BLUE,b);
}

void i2s_install() {
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = EI_CLASSIFIER_FREQUENCY,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false
    };
    i2s_driver_install(I2S_PORT,&i2s_config,0,NULL);
}

void i2s_setpin() {
    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };
    i2s_set_pin(I2S_PORT,&pin_config);
}

bool record_audio() {
    size_t bytesIn = 0;
    size_t bytes_to_read = EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int32_t);

    i2s_read(I2S_PORT,(char*)raw_i2s_buffer,bytes_to_read,&bytesIn,1000/portTICK_PERIOD_MS);

    if(bytesIn == 0) return false;

    long sum = 0;

    for(int i=0;i<EI_CLASSIFIER_RAW_SAMPLE_COUNT;i++) {
        int32_t sample = raw_i2s_buffer[i] >> 15;
        if(abs(sample) < 10) sample = 0;

        sum += abs(sample);

        inference_buffer[i] = (int16_t)constrain(sample,-32768,32767);
    }

    ambient_level = sum / (float)EI_CLASSIFIER_RAW_SAMPLE_COUNT;

    return true;
}

int microphone_audio_signal_get_data(size_t offset,size_t length,float *out_ptr) {
    numpy::int16_to_float(&inference_buffer[offset],out_ptr,length);
    return 0;
}

// PATTERN VIBRATION LOGIC
void handle_notifications() {

    unsigned long now = millis();

    if(vib_mode == 0) {
        digitalWrite(VIB_MOTOR_PIN, LOW);
        return;
    }

    if(vib_mode == 1) { // DANGER
        int pattern[] = {100,100,100,100,100,300};
        int steps = 6;

        if(now - previous_vib_millis >= pattern[vib_step]) {
            previous_vib_millis = now;
            vib_step = (vib_step + 1) % steps;
            digitalWrite(VIB_MOTOR_PIN, (vib_step % 2 == 0));
        }
    }

    else if(vib_mode == 2) { // SPEECH
        int pattern[] = {120,100,120,400};
        int steps = 4;

        if(now - previous_vib_millis >= pattern[vib_step]) {
            previous_vib_millis = now;
            vib_step = (vib_step + 1) % steps;
            digitalWrite(VIB_MOTOR_PIN, (vib_step % 2 == 0));
        }
    }

    else if(vib_mode == 3) { // MUSIC
        int pattern[] = {200,400};
        int steps = 2;

        if(now - previous_vib_millis >= pattern[vib_step]) {
            previous_vib_millis = now;
            vib_step = (vib_step + 1) % steps;
            digitalWrite(VIB_MOTOR_PIN, (vib_step % 2 == 0));
        }
    }
}