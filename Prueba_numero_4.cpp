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
  #include "user_interface.h" // Watchdog
}

// --- Objetos globales ---
Ticker ticker;           
Ticker alarmaTicker;     
WiFiManager wm;
BlynkTimer timer;


#define ALARM_PIN D7
#define PIN_ACTIVAR D1
#define SENSOR_PIN D2
#define LED_WIFI D6

// --- Variables ---
bool alarmaActivada = false;
bool alarmaSonando = false;
bool modoManual = false;

unsigned long ultimaAlerta = 0;
unsigned long tiempoInicioAlarma = 0;

const unsigned long INTERVALO_ALERTA = 5000;   // Tiempo entre alertas
const unsigned long TIEMPO_SONANDO = 60000;    // Duración de la alarma

// --- Funciones EEPROM ---
void guardarEstadoAlarma() {
  EEPROM.begin(512);
  EEPROM.write(0, alarmaActivada ? 1 : 0);
  EEPROM.commit();
}

void cargarEstadoAlarma() {
  EEPROM.begin(512);
  alarmaActivada = EEPROM.read(0) == 1;
}

// --- Función parpadeo LED WiFi ---
void tick() {
  digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
}

// --- Callback modo configuración WiFi ---
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("⚠️ Modo Configuración WiFi");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

// --- Función Blynk (V2) ---
BLYNK_WRITE(V2) {
  int val = param.asInt();
  if (val == 0) desactivar();
  else activar();
}

// --- Repiques iniciales de sirena ---
void repicarAlarma() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(ALARM_PIN, HIGH);
    delay(300);
    digitalWrite(ALARM_PIN, LOW);
    delay(300);
  }
}

// --- Parpadeo de sirena cuando está activa ---
void toggleAlarma() {
  digitalWrite(ALARM_PIN, !digitalRead(ALARM_PIN));
}

// --- Activar alarma ---
void activarAlarma() {
  if (!alarmaSonando) {
    alarmaSonando = true;
    repicarAlarma();
    alarmaTicker.attach(0.5, toggleAlarma);
    tiempoInicioAlarma = millis();
    Serial.println("🔴 Alarma SONANDO");
  }
}

// --- Desactivar alarma ---
void desactivarAlarma() {
  alarmaSonando = false;
  alarmaTicker.detach();
  digitalWrite(ALARM_PIN, LOW);
  ultimaAlerta = millis();
  Serial.println("🚨 Alarma DESACTIVADA");
}


void verificarSensores() {
  int estadoSensor = digitalRead(SENSOR_PIN);

  if ((alarmaActivada || modoManual) && estadoSensor == HIGH && !alarmaSonando && millis() - ultimaAlerta > INTERVALO_ALERTA) {
    Serial.println("⚠️ Movimiento detectado!");
    activarAlarma();
    ultimaAlerta = millis();
  }

  if (alarmaSonando && millis() - tiempoInicioAlarma > TIEMPO_SONANDO) {
    desactivarAlarma();
  }
}

// --- Reconectar WiFi si se cae ---
void verificarWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi desconectado. Intentando reconectar...");
    WiFi.reconnect();
  }
}

// --- Reconectar Blynk ---
void reconectarBlynk() {
  if (!Blynk.connected()) {
    Serial.println("⚠️ Reintentando conexión Blynk...");
    if (Blynk.connect()) {
      Serial.println("✅ Reconexión exitosa!");
      modoManual = false;
      Blynk.virtualWrite(V2, alarmaActivada ? 1 : 0);
    } else {
      Serial.println("❌ Fallo conexión Blynk. Modo Manual activado.");
      modoManual = true;
    }
  }
}

// --- Botón físico (interrupción) ---
void ICACHE_RAM_ATTR cambiarEstadoAlarma() {
  alarmaActivada = !alarmaActivada;
  guardarEstadoAlarma();
  Serial.println(alarmaActivada ? "🔴 Alarma ACTIVADA (botón)" : "🚨 Alarma DESACTIVADA (botón)");
  Blynk.virtualWrite(V2, alarmaActivada ? 1 : 0);
}

// --- Activar desde app ---
void activar() {
  alarmaActivada = true;
  guardarEstadoAlarma();
  Blynk.virtualWrite(V2, 1);
  Serial.println("🔴 Alarma ACTIVADA (Blynk)");
}

// --- Desactivar desde app ---
void desactivar() {
  alarmaActivada = false;
  guardarEstadoAlarma();
  desactivarAlarma();
  Blynk.virtualWrite(V2, 0);
  Serial.println("🚨 Alarma DESACTIVADA (Blynk)");
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(PIN_ACTIVAR, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, HIGH);

  cargarEstadoAlarma();

  attachInterrupt(digitalPinToInterrupt(PIN_ACTIVAR), cambiarEstadoAlarma, FALLING);

  wm.setConfigPortalTimeout(180);
  wm.setAPCallback(configModeCallback);

  if (!wm.autoConnect("AlarmaPerimetral", "clave123")) {
    Serial.println("⚠️ No se pudo conectar. Reiniciando...");
    ESP.restart();
  }

  Serial.println("✅ Conectado a WiFi!");
  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());

  timer.setInterval(1000L, verificarSensores);
  timer.setInterval(30000L, verificarWiFi);
}

// --- LOOP ---
void loop() {
  if (Blynk.connected()) {
    Blynk.run();
  } else {
    reconectarBlynk();
  }
  timer.run();
}
