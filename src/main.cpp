#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <PulseSensorPlayground.h>
#include <LiquidCrystal_I2C.h>

#include "thingProperties.h"
#include <SPI.h>
#include <Arduino_ESP32_OTA.h>

#define PRO_CPU 0
#define APP_CPU 1
#define NOAFF_CPU tskNO_AFFINITY

// Definición de pines y objetos
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
PulseSensorPlayground pulseSensor;
LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);

const int PulseWire = 33; // PulseSensor PURPLE WIRE connected to ANALOG PIN 0
const int LED = LED_BUILTIN; // The on-board Arduino LED, close to PIN 13.
int Threshold = 550; // Determine which Signal to "count as a beat" and which to ignore.
const int buttonPin = 35; // Pin del botón

volatile bool startMeasurement = false;

// Declaración de tareas
void TaskWelcomeMessage(void* pvParameters);
void TaskMeasureSensors(void* pvParameters);
void TaskDisplayData(void* pvParameters);

void IRAM_ATTR startMeasurementISR() {
    startMeasurement = true; // Señal de que el botón fue presionado
}

void setup() {
    // Connect to Arduino IoT Cloud
    initProperties();
    ArduinoCloud.begin(ArduinoIoTPreferredConnection);
    setDebugMessageLevel(2);
    ArduinoCloud.printDebugInfo();

    // Configura SDA en el pin 19 y SCL en el pin 18
    Wire.begin(19, 18);

    Serial.begin(115200);

    // Configuración del botón con resistencia pull-up interna
    pinMode(buttonPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonPin), startMeasurementISR, FALLING);

    // Inicializar pantalla LCD
    while (lcd.begin(16, 2) != 1) {
        Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong."));
        vTaskDelay(5000 / portTICK_PERIOD_MS);   
    }
    lcd.backlight();

    // Inicializar sensor de temperatura
    if (!mlx.begin()) {
        Serial.println("Error al inicializar el sensor MLX90614");
        while (1);
    }

    // Inicializar sensor de pulso
    pulseSensor.analogInput(PulseWire);
    pulseSensor.blinkOnPulse(LED);
    pulseSensor.setThreshold(Threshold);

    if (pulseSensor.begin()) {
        Serial.println("We created a pulseSensor Object!");
    }

    // Crear tareas
    xTaskCreatePinnedToCore(TaskWelcomeMessage, "TaskWelcomeMessage", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskMeasureSensors, "TaskMeasureSensors", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(TaskDisplayData, "TaskDisplayData", 4096, NULL, 1, NULL, 1);
}

void loop() {
    ArduinoCloud.update();
}

void TaskWelcomeMessage(void* pvParameters) {
    while (!startMeasurement) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Bienvenido");
        lcd.setCursor(0, 1);
        lcd.print("Presione boton");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Tarea eliminada tras iniciar la medición
}

void TaskMeasureSensors(void* pvParameters) {
    while (1) {
        if (startMeasurement) {
            float temperature = mlx.readObjectTempC();
            int heartRate = 0;

            if (pulseSensor.sawStartOfBeat()) {
                heartRate = pulseSensor.getBeatsPerMinute();
                Serial.println("♥  A HeartBeat Happened!");
                Serial.print("BPM: ");
                Serial.println(heartRate);
            }

            Serial.print("Temp: ");
            Serial.print(temperature);
            mlx_temperatura = temperature;
            Serial.print(" C, Pulso: ");
            Serial.println(heartRate);
            pulse_sensor_bpm = heartRate;

            vTaskDelay(500 / portTICK_PERIOD_MS); // Leer sensores cada medio segundo
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS); // Esperar hasta que se presione el botón
        }
    }
}

void TaskDisplayData(void* pvParameters) {
    while (1) {
        if (startMeasurement) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Temp: ");
            lcd.print(mlx_temperatura);
            lcd.print(" C");
            lcd.setCursor(0, 1);
            lcd.print("Pulso: ");
            lcd.print(pulse_sensor_bpm);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS); // Esperar hasta que se presione el botón
        }
    }
}
