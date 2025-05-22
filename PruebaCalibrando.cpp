#define BLYNK_TEMPLATE_ID "TMPL2OJd6sy3i" 
#define BLYNK_TEMPLATE_NAME "sistema de alarma lacs"
#define BLYNK_AUTH_TOKEN "q-b5VTL5TTTuCI8-d6DgwWguNMVNyGCc"

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>  // NUEVO

#define EEPROM_SIZE 1

Ticker ticker;
WiFiManager wm;
BlynkTimer timer;

#define ALARM_PIN D7
#define PIN_ACTIVAR D1
#define SENSOR_PIN D2
#define LED_WIFI D6
#define BEEP_PIN D5

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

// --------------------- FUNCIONES EEPROM --------------------
void guardarEstadoEEPROM() {
  EEPROM.write(0, alarmaActivada ? 1 : 0);
  EEPROM.commit();
}

void cargarEstadoEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  alarmaActivada = EEPROM.read(0) == 1;
}

// --------------------- BEEP CONTROL --------------------
void beep(int repeticiones = 1, int duracion = 100, int pausa = 100) {
  for (int i = 0; i < repeticiones; i++) {
    digitalWrite(BEEP_PIN, HIGH);
    delay(duracion);
    digitalWrite(BEEP_PIN, LOW);
    delay(pausa);
  }
}

// --------------------- ENVÍO A OTRO ESP8266 --------------------
void enviarDatosESP2() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.1.200/dato?alarma=1");  // REEMPLAZA con la IP real del ESP2
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("📤 Enviado a ESP2 - Código HTTP: %d\n", httpCode);
    } else {
      Serial.printf("⚠️ Error al enviar a ESP2: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("🚫 No conectado a WiFi. No se pudo enviar a ESP2.");
  }
}

// --------------------- BLYNK --------------------
BLYNK_WRITE(V2) {
  int valor = param.asInt();
  if (valor == 0 && alarmaActivada) {
    desactivar();
  } else if (valor == 1 && !alarmaActivada) {
    activar();
  }
}

// --------------------- FUNCIONES DE CONTROL --------------------
void activar() {
  alarmaActivada = true;
  guardarEstadoEEPROM();
  Blynk.virtualWrite(V2, 1);
  beep(1);
  Serial.println("🔴 Alarma ACTIVADA");
}

void desactivar() {
  alarmaActivada = false;
  guardarEstadoEEPROM();
  desactivarAlarma();
  Blynk.virtualWrite(V2, 0);
  beep(2, 80, 100);
  Serial.println("🟢 Alarma DESACTIVADA");
}

void activarAlarma() {
  if (!alarmaSonando) {
    alarmaSonando = true;
    digitalWrite(ALARM_PIN, HIGH);
    tiempoInicioAlarma = millis();
    Blynk.logEvent("alarma_sonando", "¡Intrusión detectada!");
    Serial.println("🚨 ALARMA SONANDO");
    beep(3, 300, 150);
    enviarDatosESP2();  // ENVÍA A ESP2 CUANDO SUENA
  }
}

void desactivarAlarma() {
  alarmaSonando = false;
  digitalWrite(ALARM_PIN, LOW);
  ultimaAlerta = millis();
  Serial.println("🔕 Alarma detenida");
}

// --------------------- WIFI Y BLYNK --------------------
void reconectarBlynk() {
  if (!Blynk.connected()) {
    Serial.println("⏳ Reintentando conexión Blynk...");
    Blynk.connect();
  }
}

void verificarWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📡 Reconectando WiFi...");
    WiFi.reconnect();
  }
}

void tick() {
  digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("⚠️ Modo Configuración WiFi");
  ticker.attach(0.2, tick);
}

// --------------------- SENSOR Y BOTÓN --------------------
void verificarSensores() {
  int estadoSensor = digitalRead(SENSOR_PIN);
  if (alarmaActivada && estadoSensor == HIGH && !alarmaSonando && millis() - ultimaAlerta > INTERVALO_ALERTA) {
    activarAlarma();
    ultimaAlerta = millis();
  }

  if (alarmaSonando && millis() - tiempoInicioAlarma > TIEMPO_SONANDO) {
    desactivarAlarma();
  }

  if (digitalRead(PIN_ACTIVAR) == LOW) {
    if (!botonPresionado) {
      presionadoDesde = millis();
      botonPresionado = true;
    } else if (millis() - presionadoDesde >= TIEMPO_PULSACION_TEST && !alarmaSonando) {
      Serial.println("🛠️ MODO TEST: Alarma suena 5 seg");
      digitalWrite(ALARM_PIN, HIGH);
      delay(5000);
      digitalWrite(ALARM_PIN, LOW);
      botonPresionado = false;
    }
  } else {
    botonPresionado = false;
  }
}

void ICACHE_RAM_ATTR cambiarEstadoAlarma() {
  unsigned long ahora = millis();
  if (ahora - ultimoCambio > DEBOUNCE_MS) {
    if (alarmaActivada) {
      desactivar();
    } else {
      activar();
    }
    ultimoCambio = ahora;
  }
}

// --------------------- SETUP Y LOOP --------------------
void setup() {
  Serial.begin(115200);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(PIN_ACTIVAR, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_WIFI, OUTPUT);
  pinMode(BEEP_PIN, OUTPUT);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(LED_WIFI, HIGH);
  digitalWrite(BEEP_PIN, LOW);

  cargarEstadoEEPROM();

  attachInterrupt(digitalPinToInterrupt(PIN_ACTIVAR), cambiarEstadoAlarma, FALLING);

  wm.setConfigPortalTimeout(60);
  wm.setAPCallback(configModeCallback);

  if (!wm.autoConnect("AlarmaPerimetral", "clave123")) {
    Serial.println("❌ Sin conexión WiFi tras 1 min. Modo manual.");
    modoManual = true;
  } else {
    Serial.println("✅ WiFi conectado");
    Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
    Blynk.virtualWrite(V2, alarmaActivada ? 1 : 0);
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
