#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient
#include "SPIFFS.h"
#include <FS.h>
#include <queue>

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
Button sensorAberto = {4, false, LOW, LOW, 0};
Button sensorMeio = {18, false, LOW, LOW, 0};
Button sensorFechado = {19, false, LOW, LOW, 0};

#define wifi_ssid "NPITI-IoT"
#define wifi_password "NPITI-IoT"

int wifi_timeout = 100000;

unsigned long int ultimoClock;

/* Variaveis de arquivo */
String path, state;
String fileName = "/statusLogs.txt";
int maxLines = 10;

WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000); // Cria um objeto "NTP" com as configurações.utilizada no Brasil
String hora;

void connectWiFi();

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

unsigned long int tempoAberto = 0;
unsigned long int limiteTempoAberto = 60000; // 60 segundos
void resetTempoAberto();
void contarTempoAberto();

void acaoSensores();

void setup()
{
    pinMode(sensorAberto.PIN, INPUT);
    pinMode(sensorMeio.PIN, INPUT);
    pinMode(sensorFechado.PIN, INPUT);
    attachInterrupt(sensorAberto.PIN, interrupcaoAberto, CHANGE);
    attachInterrupt(sensorMeio.PIN, interrupcaoMeio, CHANGE);
    attachInterrupt(sensorFechado.PIN, interrupcaoFechado, CHANGE);
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    pinMode(23, OUTPUT);

    WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
    WiFi.begin(wifi_ssid, wifi_password);
    // connectWiFi();

    ntp.begin();       // Inicia o protocolo
    ntp.forceUpdate(); // Atualização

    estadoPortao = FECHADO;

    ultimoClock = millis();

    // formatFile();
    openFS();
}
void loop()
{
    // Executa ações de acordo com os sinais dos botões.
    acaoSensores();

    if (tempoAberto > limiteTempoAberto)
    {
        Serial.println("Portão aberto por mais de 60 segundos");
        // chamar função de alerta aqui
    }

    // tratamento do tempo que o portão fica aberto
    // se estiver aberto mais do que o tempo limite, enviar alerta
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
        // chamar função de abrir portão aqui
    }
    if (sensorMeio.pressed && estadoPortao != MEIO)
    {
        estadoPortao = MEIO;
        Serial.println("Meio");
        hora = ntp.getFormattedTime();
        // writeFile("Passou pelo meio", fileName, hora);
        // chamar função de meio de portao aqui
    }
    if (sensorFechado.pressed && estadoPortao != FECHADO)
    {
        // resetTempoAberto();
        estadoPortao = FECHADO;
        Serial.println("Fechado");
        hora = ntp.getFormattedTime();
        writeFile("Fechou", fileName, hora);

        // chamar função de portao fechado aqui
    }
    // se nenhum sersor está pressionado, o portão está em movimento
    if (!sensorAberto.pressed && !sensorMeio.pressed && !sensorFechado.pressed && estadoPortao != MOVIMENTO)
    {
        estadoPortao = MOVIMENTO;
        // inciar timer
        //  chamar função de portao em movimento aqui
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

void resetTempoAberto()
{
    tempoAberto = 0;
};
void contarTempoAberto()
{
    tempoAberto += (millis() - ultimoClock);
    ultimoClock = millis();
    Serial.printf("Contar tempo aberto %d \n", tempoAberto);
};

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
