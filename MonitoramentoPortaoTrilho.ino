#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient

#define LED 2
#define Sensor01 4
#define Sensor02 32
#define Sensor03 33

#define wifi_ssid "imd0902"
#define wifi_password "imd0902iot"

int wifi_timeout = 100000;

WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000); // Cria um objeto "NTP" com as configurações.utilizada no Brasil
String hora;

void connectWiFi();

void setup()
{
    pinMode(Sensor01, INPUT);
    pinMode(Sensor02, INPUT);
    pinMode(Sensor03, INPUT);
    pinMode(LED, OUTPUT);
    Serial.begin(115200);

    WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
    WiFi.begin(wifi_ssid, wifi_password);
    connectWiFi();

    ntp.begin();       // Inicia o protocolo
    ntp.forceUpdate(); // Atualização
}
void loop()
{
    /* Leitura dos sensores */
    int sensorInicio = digitalRead(Sensor01);
    // int sensorMeio = digitalRead(Sensor02);
    // int sensorFim = digitalRead(Sensor03);

    if (sensorInicio == 0)
    {
        digitalWrite(LED, HIGH);
    }
    else
    {
        digitalWrite(LED, LOW);
    }
}

// Função para conectar o Esp ao Wifi
void connectWiFi()
{
    Serial.print("Conectando à rede WiFi .. ");

    unsigned long tempoInicial = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - tempoInicial < wifi_timeout))
    {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Conexão com WiFi falhou!");
    }
    else
    {
        Serial.print("Conectado com o IP: ");
        Serial.println(WiFi.localIP());
    }
}
