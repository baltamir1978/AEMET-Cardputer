#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <M5Unified.h>

const char* ssid = "";
const char* password = "";
const char* apiKey = "";
float volt = M5.Power.getBatteryVoltage();
struct Municipio {
  const char* nombre;
  const char* codigoINE;
};

Municipio municipios[] = {
  { "Madrid", "28079" },
  { "San Ildefonso", "40181" },
  { "Posada de Llanes", "33036" }
};

const int totalMunicipios = sizeof(municipios) / sizeof(municipios[0]);
int municipioActual = 0;
int diaActual = 0;  // 0 = hoy, 1 = mañana
unsigned long ultimoCambio = 0;
const unsigned long intervalo = 4000;

void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    float volt = M5.Power.getBatteryVoltage();
    M5.Lcd.fillRect(0, 30, 200, 60, BLACK);
    mostrarBateria(volt);
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("\nWiFi conectada");
  mostrarPrediccion(municipios[municipioActual].codigoINE, diaActual);
  ultimoCambio = millis();
}

void loop() {
  M5.update();  // Necesario para leer los botones correctamente
  // Prioridad al botón G0 (BtnA)
  if (M5.BtnA.wasPressed()) {
    municipioActual = (municipioActual + 1) % totalMunicipios;
    diaActual = 0;  // Mostrar "hoy"
    mostrarPrediccion(municipios[municipioActual].codigoINE, diaActual);
    ultimoCambio = millis() - intervalo;  // Evitar que se dispare el cambio automático justo después
    return;  // Salimos del loop para evitar doble actualización
  }
  // Cambio automático cada 4 segundos
  if (millis() - ultimoCambio >= intervalo) {
    mostrarPrediccion(municipios[municipioActual].codigoINE, diaActual);

    diaActual++;
    if (diaActual > 1) {
      diaActual = 0;
    }

    ultimoCambio = millis();
  }
}

void mostrarPrediccion(const char* codigoINE, int diaIndice) {
  String metadataUrl = "https://opendata.aemet.es/opendata/api/prediccion/especifica/municipio/diaria/" + String(codigoINE) + "/";

  HTTPClient http;
  http.begin(metadataUrl);
  http.addHeader("api_key", apiKey);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    String datosUrl = doc["datos"];
    http.end();

    http.begin(datosUrl);
    int datosCode = http.GET();
    if (datosCode == 200) {
      String rawJson = http.getString();
      http.end();

      DynamicJsonDocument datos(8192);
      deserializeJson(datos, rawJson);

      String ciudad = datos[0]["nombre"];
      String provincia = datos[0]["provincia"];
      JsonObject prediccion = datos[0]["prediccion"]["dia"][diaIndice];
      

      String fecha = prediccion["fecha"];
      int tempMax = prediccion["temperatura"]["maxima"];
      int tempMin = prediccion["temperatura"]["minima"];
      int probPrecipitacion = prediccion["probPrecipitacion"][0];
      String estadoCielo = "";
      JsonArray estados = prediccion["estadoCielo"];

      for (JsonObject tramo : estados) {
        const char* periodo = tramo["periodo"];
        const char* valor = tramo["value"];

        // Si hay un estado general sin "periodo", úsalo
        if (!periodo || String(periodo).length() == 0) {
          estadoCielo = valor;
          break;
        }

        // Si no hay uno general, usamos el primero disponible
        if (estadoCielo == "") {
          estadoCielo = valor;
        }
      }

      String dia = fecha.substring(8, 10);
      String mes = fecha.substring(5, 7);
      String fechaCorta = dia + "/" + mes;

      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextSize(2);

      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.printf("%s (%s)\n\n", ciudad.c_str(), provincia.c_str());

      M5.Lcd.setTextColor(YELLOW);
      M5.Lcd.printf("%s:\n", diaIndice == 0 ? "Hoy" : "Manana");

      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.printf("Fecha: %s\n", fechaCorta.c_str());

      M5.Lcd.setTextColor(RED);
      M5.Lcd.printf("Max: ");
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.printf("%d C\n", tempMax);

      M5.Lcd.setTextColor(BLUE);
      M5.Lcd.printf("Min: ");
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.printf("%d C\n", tempMin);

      M5.Lcd.setTextColor(CYAN);
      M5.Lcd.printf("Lluvia: ");
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.printf("%d %%\n", probPrecipitacion);
      mostrarIcono(estadoCielo);
    } else {
      mostrarError("Error JSON");
      http.end();
    }
  } else {
    mostrarError("Error metadata");
    http.end();
  }
  
}
void mostrarError(const char* msg) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(RED);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println(msg);
}

void mostrarBateria(float volt) {
  int x = 10;
  int y = 40;
  int ancho = 100;
  int alto = 20;

  // Calcula el porcentaje (volt * 100 convierte a rango 300-420 para 0-100%)
  int porcentaje = map((int)(volt * 100), 300, 420, 0, 100);
  porcentaje = constrain(porcentaje, 0, 100);

  // Dibuja el contorno de la batería
  M5.Lcd.drawRect(x, y, ancho, alto, WHITE);

  // Dibuja el "pico" de la batería
  M5.Lcd.fillRect(x + ancho, y + alto/4, 6, alto/2, WHITE);

  // Rellena la barra proporcional al porcentaje
  int relleno = (ancho - 4) * porcentaje / 100;
  M5.Lcd.fillRect(x + 2, y + 2, relleno, alto - 4, GREEN);

  // Borra el área del texto y escribe el porcentaje dentro de la barra
  M5.Lcd.fillRect(x + ancho + 10, y, 60, alto, BLACK);
  M5.Lcd.setCursor(x + ancho + 10, y + 2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.printf("%d%%", porcentaje);

  // Muestra el voltaje debajo
  M5.Lcd.fillRect(x, y + alto + 2, 100, 20, BLACK);
  M5.Lcd.setCursor(x, y + alto + 4);
  M5.Lcd.printf("Volt: %.2f V", volt);
}

void mostrarIcono(String estadoCielo) {
  int x = 200;  // posición derecha de la pantalla
  int y = 80;

  // Elimina icono anterior
  M5.Lcd.fillRect(x - 10, y - 10, 80, 80, BLACK);

  if (estadoCielo == "11") {  // Sol
    M5.Lcd.fillCircle(x, y, 20, YELLOW);
  } 
  else if (estadoCielo == "12" || estadoCielo == "13") {  // Poco nuboso, intervalos
    M5.Lcd.fillCircle(x - 10, y, 15, YELLOW);
    M5.Lcd.fillEllipse(x + 10, y + 10, 20, 15, LIGHTGREY);
  } 
  else if (estadoCielo == "14" || estadoCielo == "43" || estadoCielo == "45") {  // Nuboso, muy nuboso, cubierto
    M5.Lcd.fillEllipse(x, y, 30, 20, LIGHTGREY);
  } 
  else if (estadoCielo == "17") {  // Nubes altas
    int offset = 5;
    M5.Lcd.drawCircle(x, y, 20, LIGHTGREY);
    M5.Lcd.drawCircle(x + offset, y - offset, 18, LIGHTGREY);
    M5.Lcd.drawCircle(x - offset, y + offset, 18, LIGHTGREY);
  }
  else if (estadoCielo == "51" || estadoCielo == "52" || estadoCielo == "53" ||
           estadoCielo == "61" || estadoCielo == "63" || estadoCielo == "65") {  // Lluvia y chubascos
    M5.Lcd.fillEllipse(x, y, 30, 20, LIGHTGREY);
    M5.Lcd.fillTriangle(x - 5, y + 20, x, y + 30, x + 5, y + 20, BLUE);
  } 
  else if (estadoCielo == "71" || estadoCielo == "73" || estadoCielo == "75") {  // Nieve
    M5.Lcd.fillCircle(x, y, 20, WHITE);
    M5.Lcd.drawLine(x - 10, y, x + 10, y, BLUE);
    M5.Lcd.drawLine(x, y - 10, x, y + 10, BLUE);
  }
  else if (estadoCielo == "80") {  // Niebla
    M5.Lcd.fillRect(x - 20, y - 10, 40, 20, LIGHTGREY);
  }
  else if (estadoCielo == "81" || estadoCielo == "82") {  // Tormenta o ventisca
    M5.Lcd.fillRect(x - 15, y - 10, 30, 20, DARKGREY);
    M5.Lcd.fillTriangle(x - 15, y + 10, x - 5, y - 10, x + 15, y + 10, YELLOW);
  }
  else {
    M5.Lcd.drawString("?", x, y, 4);
  }
}
