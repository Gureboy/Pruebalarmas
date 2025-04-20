#define BLYNK_TEMPLATE_ID "TMPL2OJd6sy3i"
#define BLYNK_TEMPLATE_NAME "sistema de alarma lacs"
#define BLYNK_AUTH_TOKEN "q-b5VTL5TTTuCI8-d6DgwWguNMVNyGCc"

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <EEPROM.h>

#define EEPROM_SIZE 1

Ticker ticker;
WiFiManager wm;
BlynkTimer timer;

#define ALARM_PIN D7
#define PIN_ACTIVAR D1
#define SENSOR_PIN D2
#define LED_WIFI D6

bool alarmaActivada = false;
bool alarmaSonando = false;
bool modoManual = false;

unsigned long ultimaAlerta = 0;
unsigned long tiempoInicioAlarma = 0;
unsigned long presionadoDesde = 0;
bool botonPresionado = false;

const unsigned long INTERVALO_ALERTA = 5000;
const unsigned long TIEMPO_SONANDO = 60000;
const unsigned long TIEMPO_PULSACION_TEST = 3000;
const unsigned long DEBOUNCE_MS = 200;

volatile unsigned long ultimoCambio = 0;

void guardarEstadoEEPROM() {
  EEPROM.write(0, alarmaActivada ? 1 : 0);
  EEPROM.commit();
}

void cargarEstadoEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  alarmaActivada = EEPROM.read(0) == 1;
}

void tick() {
  digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("⚠️ Modo Configuración WiFi");
  ticker.attach(0.2, tick);
}

BLYNK_WRITE(V2) {
  int alarmVal = param.asInt();
  if (alarmVal == 0) {
    desactivar();
  } else {
    activar();
  }
}

void activar() {
  alarmaActivada = true;
  guardarEstadoEEPROM();
  Serial.println("🔴 Alarma ACTIVADA");
}

void desactivar() {
  alarmaActivada = false;
  guardarEstadoEEPROM();
  desactivarAlarma();
  Serial.println("🚨 Alarma DESACTIVADA");
}

void activarAlarma() {
  if (!alarmaSonando) {
    alarmaSonando = true;
    digitalWrite(ALARM_PIN, HIGH);
    Serial.println("🚨 ALARMA SONANDO");
    tiempoInicioAlarma = millis();
    Blynk.logEvent("alarma_sonando", "¡Intrusión detectada!");
  }
}

void desactivarAlarma() {
  alarmaSonando = false;
  digitalWrite(ALARM_PIN, LOW);
  ultimaAlerta = millis();
  Serial.println("🟢 Alarma detenida");
}

void reconectarBlynk() {
  if (!Blynk.connected()) {
    Serial.println("Reintentando conexión Blynk...");
    Blynk.connect();
  }
}

void verificarWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Intentando reconexión WiFi...");
    WiFi.reconnect();
  }
}

void verificarSensores() {
  int sensorEstado = digitalRead(SENSOR_PIN);
  if (alarmaActivada && sensorEstado == HIGH && !alarmaSonando && millis() - ultimaAlerta > INTERVALO_ALERTA) {
    activarAlarma();
    ultimaAlerta = millis();
  }

  if (alarmaSonando && millis() - tiempoInicioAlarma > TIEMPO_SONANDO) {
    desactivarAlarma();
  }

  // Verificar si el botón fue presionado más de 3 segundos para modo test
  if (digitalRead(PIN_ACTIVAR) == LOW) {
    if (!botonPresionado) {
      presionadoDesde = millis();
      botonPresionado = true;
    } else if (millis() - presionadoDesde >= TIEMPO_PULSACION_TEST && !alarmaSonando) {
      Serial.println("🔧 MODO TEST: Alarma suena por 5 seg");
      digitalWrite(ALARM_PIN, HIGH);
      delay(5000);
      digitalWrite(ALARM_PIN, LOW);
      botonPresionado = false; // Evitar repetición
    }
  } else {
    botonPresionado = false;
  }
}

void ICACHE_RAM_ATTR cambiarEstadoAlarma() {
  unsigned long ahora = millis();
  if (ahora - ultimoCambio > DEBOUNCE_MS) {
    alarmaActivada = !alarmaActivada;
    guardarEstadoEEPROM();
    Serial.println(alarmaActivada ? "🔴 ACTIVADA desde botón" : "🔕 DESACTIVADA desde botón");
    ultimoCambio = ahora;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(PIN_ACTIVAR, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(LED_WIFI, HIGH);

  cargarEstadoEEPROM();
  attachInterrupt(digitalPinToInterrupt(PIN_ACTIVAR), cambiarEstadoAlarma, FALLING);

  wm.setConfigPortalTimeout(60);
  wm.setAPCallback(configModeCallback);

  if (!wm.autoConnect("AlarmaPerimetral", "clave123")) {
    Serial.println("⚠️ Sin conexión WiFi tras 1 min. Entrando en modo MANUAL.");
    modoManual = true;
  } else {
    Serial.println("✅ WiFi conectado");
    Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
  }

  timer.setInterval(1000L, verificarSensores);
  timer.setInterval(30000L, verificarWiFi);
}

void loop() {
  if (!modoManual) {
    if (Blynk.connected()) {
      Blynk.run();
    } else {
      reconectarBlynk();
    }
  }

  timer.run();
}
