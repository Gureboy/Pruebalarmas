#define BLYNK_TEMPLATE_ID "TMPL2OJd6sy3i"
#define BLYNK_TEMPLATE_NAME "sistema de alarma lacs"
#define BLYNK_AUTH_TOKEN "q-b5VTL5TTTuCI8-d6DgwWguNMVNyGCc"

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <EEPROM.h>  

extern "C" {
  #include "user_interface.h"  // Watchdog
}

Ticker ticker;
WiFiManager wm;
BlynkTimer timer;

#define ALARM_PIN D7
#define PIN_ACTIVAR D1
#define SENSOR_PIN D2
#define LED_WIFI D6

bool alarmaActivada = false;
unsigned long ultimaAlerta = 0;
const unsigned long INTERVALO_ALERTA = 5000;
bool alarmaSonando = false;
const unsigned long TIEMPO_SONANDO = 60000;
unsigned long tiempoInicioAlarma = 0;
unsigned long tiempoInicioConexion = 0;
bool modoManual = false;

void guardarEstadoAlarma() {
  EEPROM.begin(512);
  EEPROM.write(0, alarmaActivada ? 1 : 0);
  EEPROM.commit();
}

void cargarEstadoAlarma() {
  EEPROM.begin(512);
  alarmaActivada = EEPROM.read(0) == 1;
}

void tick() {
  digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("⚠️ Modo Configuración WiFi");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

// Manejo desde Blynk
BLYNK_WRITE(V2) {
  int alarmVal = param.asInt();
  if (alarmVal == 0) {
    desactivar();
  } else {
    activar();
  }
}

void verificarSensores() {
  int sensorEstado = digitalRead(SENSOR_PIN);
  if ((alarmaActivada || modoManual) && sensorEstado == HIGH && !alarmaSonando && millis() - ultimaAlerta > INTERVALO_ALERTA) {
    Serial.println("⚠️ ALERTA: Movimiento detectado!");
    activarAlarma();
    ultimaAlerta = millis();
  }
  if (alarmaSonando && millis() - tiempoInicioAlarma > TIEMPO_SONANDO) {
    desactivarAlarma();
  }
}

// Repiques de alarma al activarse
void repicarAlarma() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(ALARM_PIN, HIGH);
    delay(300);
    digitalWrite(ALARM_PIN, LOW);
    delay(300);
  }
}

void activarAlarma() {
  if (!alarmaSonando) {
    alarmaSonando = true;
    digitalWrite(ALARM_PIN, HIGH);
    repicarAlarma(); 
    Serial.println("🔴 Alarma ACTIVADA");
    tiempoInicioAlarma = millis();
  }
}

void desactivarAlarma() {
  alarmaSonando = false;
  digitalWrite(ALARM_PIN, LOW);
  ultimaAlerta = millis();
  Serial.println("🚨 Alarma DESACTIVADA");
}

void reconectarBlynk() {
  if (!Blynk.connected()) {
    Serial.println("⚠️ Intentando reconectar a Blynk...");
    if (Blynk.connect()) {
      Serial.println("✅ Reconectado a Blynk!");
      modoManual = false;
      Blynk.virtualWrite(V2, alarmaActivada ? 1 : 0);  
    } else {
      Serial.println("❌ Falló la reconexión. Sigue en modo manual.");
      modoManual = true;
    }
  }
}

void verificarWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi desconectado. Intentando reconectar...");
    WiFi.reconnect();
  }
}

// Botón físico para alternar estado
void ICACHE_RAM_ATTR cambiarEstadoAlarma() {
  alarmaActivada = !alarmaActivada;
  guardarEstadoAlarma();
  Serial.println(alarmaActivada ? "🔴 Alarma ACTIVADA (botón físico)" : "🚨 Alarma DESACTIVADA (botón físico)");
  Blynk.virtualWrite(V2, alarmaActivada ? 1 : 0);  
}

void setup() {
  Serial.begin(115200);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(PIN_ACTIVAR, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_WIFI, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ACTIVAR), cambiarEstadoAlarma, FALLING);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(LED_WIFI, HIGH);

  // Watchdog
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);

  cargarEstadoAlarma();

  wm.setConfigPortalTimeout(180);
  wm.setAPCallback(configModeCallback);

  if (!wm.autoConnect("AlarmaPerimetral", "clave123")) {
    Serial.println("⚠️ No se pudo conectar al WiFi, reiniciando...");
    ESP.restart();
  }

  Serial.println("✅ Conectado a WiFi!");
  tiempoInicioConexion = millis();
  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());

  timer.setInterval(1000L, verificarSensores);
  timer.setInterval(10000L, verificarWiFi);
  timer.setInterval(30000L, reconectarBlynk);
}

void loop() {
  ESP.wdtFeed();

  if (Blynk.connected()) {
    Blynk.run();
  } else {
    if (!modoManual && millis() - tiempoInicioConexion > 120000) {
      modoManual = true;
      Serial.println("🛑 No se pudo conectar a Blynk. Activando modo MANUAL.");
    }
  }

  timer.run();
}

void activar() {
  alarmaActivada = true;
  guardarEstadoAlarma();
  Blynk.virtualWrite(V2, 1);
  Serial.println("🔴 Alarma ACTIVADA");
}

void desactivar() {
  alarmaActivada = false;
  guardarEstadoAlarma();
  desactivarAlarma();
  Blynk.virtualWrite(V2, 0);
  Serial.println("🚨 Alarma DESACTIVADA");
}
