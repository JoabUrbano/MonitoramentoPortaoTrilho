#include <WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient
#include "SPIFFS.h"
#include <FS.h>
#include <queue>

#include <PubSubClient.h>

std::queue<String> filaDeStrings;

struct Button
{
    const uint8_t PIN;
    bool pressed;
    int buttonState;
    int lastButtonState;
    unsigned long int lastBounceTime;
};

enum EstadoPortao
{
    ABERTO,
    FECHADO,
    MEIO,
    MOVIMENTO
};

enum EstadoPortao estadoPortao;

#define LED 2
#define LEDFormat 14
#define Format 12

Button sensorAberto = {4, false, LOW, LOW, 0};
Button sensorMeio = {18, false, LOW, LOW, 0};
Button sensorFechado = {19, false, LOW, LOW, 0};

#define wifi_ssid "NPITI-IoT"
#define wifi_password "NPITI-IoT"

int wifi_timeout = 100000;

WiFiClient wifi_client;
PubSubClient client(wifi_client);

const char *mqtt_broker = "homeassistant.local";
const int mqtt_port = 1883;
int mqtt_timeout = 10000;

/* Variaveis de arquivo */
String path, state;
String fileName = "/statusLogs.txt";
int maxLines = 10;

WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000); // Cria um objeto "NTP" com as configurações.utilizada no Brasil
String hora;

void connectWiFi();
void connectMQTT();

bool mqtt_connected = false;

/* Definição Funções de Interação com arquivos */
void writeFile(String state, String path, String hora);
void readFile(String path);
void formatFile();
void openFS();

// Funções de interrupção dos sensores
void IRAM_ATTR interrupcaoFechado();
void IRAM_ATTR interrupcaoMeio();
void IRAM_ATTR interrupcaoAberto();
void debounceBotao(Button *button); // função de debounce
unsigned long int debounceDelay = 50;

// a partir do momento que o portão é aberto, o tempo é contado
// quando o portão for fechado, o tempo é resetado

unsigned long int limiteTempoAberto = 60000; // 60 segundos
hw_timer_t *Timer0_Cfg = NULL;

void acaoSensores();

bool comandoAbrir = false;
bool comandoFechar = false;

#define botaoControleAbrir 23
#define botaoControleFechar 22

void abrirPortao();
void fecharPortao();
void acaoControle(int botao);

void tratarComandoMqtt(String comando);
void callback(char *topic, byte *message, unsigned int length);

void reconnect();

// criar uma task para executar as acoes do portao em outro core
TaskHandle_t Core0;

void setup()
{
    pinMode(sensorAberto.PIN, INPUT);
    pinMode(sensorMeio.PIN, INPUT);
    pinMode(sensorFechado.PIN, INPUT);
    attachInterrupt(sensorAberto.PIN, interrupcaoAberto, CHANGE);
    attachInterrupt(sensorMeio.PIN, interrupcaoMeio, CHANGE);
    attachInterrupt(sensorFechado.PIN, interrupcaoFechado, CHANGE);

    pinMode(LED, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(LEDFormat, OUTPUT);
    pinMode(Format, INPUT);
    pinMode(botaoControleAbrir, OUTPUT);
    pinMode(botaoControleFechar, OUTPUT);
    Serial.begin(115200);

    Timer0_Cfg = timerBegin(0, 80, true);
    timerWrite(Timer0_Cfg, 0);

    WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
    WiFi.begin(wifi_ssid, wifi_password);
    connectWiFi();

    ntp.begin();       // Inicia o protocolo
    ntp.forceUpdate(); // Atualização

    estadoPortao = FECHADO;

    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);

    xTaskCreatePinnedToCore(
        Task1Code,
        "Task1",
        10000,
        NULL,
        1,
        &Core0,
        0);

    delay(500);

    openFS();
}

void Task1Code(void *parameter)
{
    for (;;)
    {

        if (comandoAbrir)
        {
            abrirPortao();
            comandoAbrir = false;
        }
        if (comandoFechar)
        {
            fecharPortao();
            comandoFechar = false;
        }
        delay(100);
    }
}

void loop()
{
    /* Verifica o botão de formatação */
    int val = digitalRead(Format);
    if (val == HIGH)
    {
        digitalWrite(LEDFormat, HIGH);
        Serial.println("Formatando Esp32");
        formatFile();
        digitalWrite(LEDFormat, LOW);
        delay(1000);
    }

    if (!client.connected())
    {                // Se MQTT não estiver conectado
        reconnect(); // Tente se conectar ao broker
    }
    client.loop();

    // Executa ações de acordo com os sinais dos botões.
    acaoSensores();

    uint64_t tempoAberto = timerReadMilis(Timer0_Cfg);

    if (tempoAberto > limiteTempoAberto)
    {
        Serial.println("Portão aberto por mais de 60 segundos");
        timerStop(Timer0_Cfg);
        timerWrite(Timer0_Cfg, 0);
        client.publish("/mosquitto/portao/alarme", "Alarme ativado");
    }
    delay(100);
}

void acaoSensores()
{
    if (sensorAberto.pressed && estadoPortao != ABERTO)
    {
        estadoPortao = ABERTO;
        Serial.println("Aberto");
        hora = ntp.getFormattedTime();
        writeFile("Foi aberto", fileName, hora);
        client.publish("/mosquitto/portao/estadoSensores", "Aberto");
    }
    if (sensorMeio.pressed && estadoPortao != MEIO)
    {
        estadoPortao = MEIO;
        Serial.println("Meio");
        hora = ntp.getFormattedTime();
        writeFile("Passou pelo meio", fileName, hora);
        client.publish("/mosquitto/portao/estadoSensores", "Meio");
    }
    if (sensorFechado.pressed && estadoPortao != FECHADO)
    {
        estadoPortao = FECHADO;
        timerStop(Timer0_Cfg);
        timerWrite(Timer0_Cfg, 0);
        Serial.println("Fechado");
        hora = ntp.getFormattedTime();
        writeFile("Fechou", fileName, hora);
        client.publish("/mosquitto/portao/estadoSensores", "Fechado");
    }
    // se nenhum sersor está pressionado, o portão está em movimento
    if (!sensorAberto.pressed && !sensorMeio.pressed && !sensorFechado.pressed && estadoPortao != MOVIMENTO)
    {
        estadoPortao = MOVIMENTO;
        timerStart(Timer0_Cfg);
    }
};

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
        digitalWrite(LED, LOW);
    }
    else
    {
        Serial.print("Conectado com o IP: ");
        Serial.println(WiFi.localIP());
        digitalWrite(LED, HIGH);
    }
}

void abrirPortao()
{
    Serial.println("Abrindo portão");
    acaoControle(botaoControleAbrir);
};

void fecharPortao()
{
    Serial.println("Fechando portão");
    acaoControle(botaoControleFechar);
};

void acaoControle(int botao)
{
    digitalWrite(botao, HIGH);
    delay(500);
    digitalWrite(botao, LOW);
};

void tratarComandoMqtt(String comando)
{
    Serial.println("Comando recebido: " + comando);
    if (comando == "abrir")
    {
        comandoAbrir = true;
    }
    if (comando == "fechar")
    {
        comandoFechar = true;
    }
}

void callback(char *topic, byte *message, unsigned int length)
{
    Serial.print("Mensagem Recebida no topic: ");
    Serial.print(topic);
    Serial.print(". Mensagem: ");
    String messageTemp;

    for (int i = 0; i < length; i++)
    {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }
    Serial.println();

    if (String(topic) == "/mosquitto/portao/comandos")
    {
        tratarComandoMqtt(messageTemp);
    }
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("ESP32Client", "mosquitto", "mosquitto"))
        {
            Serial.println("connected");
            // Subscribe
            client.subscribe("/mosquitto/portao/comandos");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 0.5 seconds");
            delay(500);
        }
    }
}

// Declaração das funçoẽs de interrupção
void IRAM_ATTR interrupcaoFechado()
{
    debounceBotao(&sensorFechado);
};
void IRAM_ATTR interrupcaoMeio()
{
    debounceBotao(&sensorMeio);
};
void IRAM_ATTR interrupcaoAberto()
{
    debounceBotao(&sensorAberto);
};

void debounceBotao(Button *button)
{
    button->buttonState = digitalRead(button->PIN);
    // O debounce só acontece se o botão mudar de estado
    if ((millis() - button->lastBounceTime > debounceDelay) && button->buttonState != button->lastButtonState)
    {
        if (button->buttonState == HIGH)
        {
            button->pressed = true;
            button->lastButtonState = HIGH;
        }
        else
        {
            button->lastButtonState = LOW;
            button->pressed = false;
        }
        button->lastBounceTime = millis();
    }
}

/* Funções de interação arquivos ESP */

void writeFile(String state, String path, String hora)
{
    while (!filaDeStrings.empty())
    {
        filaDeStrings.pop();
    }

    readFile(path);
    File rFile = SPIFFS.open(path, "w");

    if (!rFile)
    {
        Serial.println("Erro ao abrir arquivo!");
        return;
    }
    filaDeStrings.push(state + "," + hora);

    // Escrever no arquivo
    while (filaDeStrings.size() > maxLines)
    {
        filaDeStrings.pop();
    }
    while (!filaDeStrings.empty())
    {
        rFile.println(filaDeStrings.front());
        filaDeStrings.pop();
    }

    rFile.close();
}

void readFile(String path)
{
    Serial.println("Read file");

    File rFile = SPIFFS.open(path, "r"); // r+ leitura e escrita

    if (!rFile)
    {
        Serial.println("Erro ao abrir arquivo!");

        return;
    }

    else
    {
        Serial.print("---------- Lendo arquivo ");

        Serial.print(path);

        Serial.println("  ---------");
        while (rFile.position() < rFile.size())
        {
            String line = rFile.readStringUntil('\n'); // Lê uma linha do arquivo
            Serial.println(line);
            filaDeStrings.push(line);
        }

        rFile.close();
    }
}

void formatFile()
{
    SPIFFS.format();
    Serial.println("Formatou SPIFFS");
}

void openFS(void)
{
    if (!SPIFFS.begin())
        Serial.println("\nErro ao abrir o sistema de arquivos");
    else
        Serial.println("\nSistema de arquivos aberto com sucesso!");
}
