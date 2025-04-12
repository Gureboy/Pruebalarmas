#define BLYNK_TEMPLATE_ID "TMPL2OJd6sy3i"
#define BLYNK_TEMPLATE_NAME "sistema de alarma lacs"
#define BLYNK_AUTH_TOKEN "q-b5VTL5TTTuCI8-d6DgwWguNMVNyGCc"

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <BlynkTimer.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiType.h>
#include <ESP8266WiFiSTA.h>
#include <Esp.h>
#include <ArduinoOTA.h>
#include <user_interface.h>  // Para el watchdog

#define EEPROM_SIZE 512
#define EEPROM_ADDR_ESTADO 0

Ticker ticker;
WiFiManager wm;
BlynkTimer timer;

#define ALARM_PIN D7
#define PIN_ACTIVAR D1
#define SENSOR_PIN D2
#define LED_WIFI D6

bool modoManual = false;
bool alarmaActivada = false;
unsigned long ultimaAlerta = 0;
const unsigned long INTERVALO_ALERTA = 5000;
bool alarmaSonando = false;
const unsigned long TIEMPO_SONANDO = 60000;
unsigned long tiempoInicioAlarma = 0;
unsigned long tiempoInicioWiFi = 0;

void tick() {
  digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("⚠️ Modo Configuración WiFi");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
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

void guardarEstadoEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_ADDR_ESTADO, alarmaActivada ? 1 : 0);
  EEPROM.commit();
  EEPROM.end();
}

void cargarEstadoEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  int estado = EEPROM.read(EEPROM_ADDR_ESTADO);
  EEPROM.end();
  alarmaActivada = (estado == 1);
  Serial.println(alarmaActivada ? "🔄 Estado EEPROM: ACTIVADA" : "🔄 Estado EEPROM: DESACTIVADA");
}

void verificarSensores() {
  int sensorEstado = digitalRead(SENSOR_PIN);
  if (alarmaActivada && sensorEstado == HIGH && !alarmaSonando && millis() - ultimaAlerta > INTERVALO_ALERTA) {
    Serial.println("⚠️ ALERTA: Movimiento detectado!");
    activarAlarma();
    ultimaAlerta = millis();
  }
  if (alarmaSonando && millis() - tiempoInicioAlarma > TIEMPO_SONANDO) {
    desactivarAlarma();
  }
}

void activarAlarma() {
  if (!alarmaSonando) {
    alarmaSonando = true;
    digitalWrite(ALARM_PIN, HIGH);
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
  if (!Blynk.connected() && !modoManual) {
    Serial.println("⚠️ Blynk desconectado. Intentando reconectar...");
    if (Blynk.connect()) {
      Serial.println("✅ Reconexion Blynk exitosa!");
    } else {
      Serial.println("❌ Fallo en la reconexión a Blynk");
    }
  }
}

void verificarWiFi() {
  if (WiFi.status() != WL_CONNECTED && !modoManual) {
    Serial.println("⚠️ WiFi desconectado. Intentando reconectar...");
    WiFi.reconnect();
  } else if (WiFi.status() == WL_CONNECTED && modoManual) {
    Serial.println("🔄 WiFi detectado nuevamente. Saliendo del modo manual...");
    modoManual = false;
    Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
  }
}

void ICACHE_RAM_ATTR cambiarEstadoAlarma() {
  alarmaActivada = !alarmaActivada;
  guardarEstadoEEPROM();
  Serial.println(alarmaActivada ? "🔴 Alarma ACTIVADA" : "🚨 Alarma DESACTIVADA");
}

void activar() {
  alarmaActivada = true;
  guardarEstadoEEPROM();
  Serial.println("🔴 Alarma ACTIVADA");
}

void desactivar() {
  alarmaActivada = false;
  desactivarAlarma();
  guardarEstadoEEPROM();
  Serial.println("🚨 Alarma DESACTIVADA");
}

void setup() {
  Serial.begin(115200);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(PIN_ACTIVAR, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(LED_WIFI, HIGH);
  attachInterrupt(digitalPinToInterrupt(PIN_ACTIVAR), cambiarEstadoAlarma, FALLING);

  cargarEstadoEEPROM();

  wm.setConfigPortalTimeout(60); // 1 minuto para intentar conectarse
  wm.setAPCallback(configModeCallback);

  tiempoInicioWiFi = millis();
  bool conectado = wm.autoConnect("AlarmaPerimetral", "clave123");

  if (!conectado) {
    Serial.println("⏱️ Tiempo excedido sin WiFi. Entrando en modo manual...");
    modoManual = true;
    ticker.detach();
    digitalWrite(LED_WIFI, LOW);
  } else {
    Serial.println("✅ Conectado a WiFi!");
    Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
  }

  timer.setInterval(1000L, verificarSensores);
  timer.setInterval(30000L, verificarWiFi);

  // Watchdog: resetea si se cuelga más de 8s
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
}

void loop() {
  if (!modoManual && Blynk.connected()) {
    Blynk.run();
  } else if (!modoManual) {
    reconectarBlynk();
  }

  timer.run();

  // Watchdog refresh
  ESP.wdtFeed();
}
