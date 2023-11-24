#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient
#include "SPIFFS.h"

struct Button
{
    const uint8_t PIN;
    bool pressed;
    int buttonState;
    int lastButtonState;
    unsigned long int lastBounceTime;
};

#define LED 2
Button sensorAberto = {4, false, LOW, LOW, 0};
Button sensorMeio = {18, false, LOW, LOW, 0};
Button sensorFechado = {19, false, LOW, LOW, 0};

#define wifi_ssid "NPITI-IoT"
#define wifi_password "NPITI-IoT"

int wifi_timeout = 100000;

/* Variaveis de arquivo */
String path, state, s;

WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000); // Cria um objeto "NTP" com as configurações.utilizada no Brasil
String hora;

void connectWiFi();

/* Definição Funções de Interação com arquivos */
void writeFile(String state, String path, String hora);
String readFile(String path);
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

    //formatFile()

    WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
    WiFi.begin(wifi_ssid, wifi_password);
    connectWiFi();

    ntp.begin();       // Inicia o protocolo
    ntp.forceUpdate(); // Atualização
}
void loop()
{
    // Executa ações de acordo com os sinais dos botões. Seta pra false pra a ação não ser executada mais de uma vez
    if (sensorAberto.pressed)
    {
        sensorAberto.pressed = false;
        Serial.println("Aberto");
        // chamar função de abrir portão aqui
    }
    if (sensorMeio.pressed)
    {
        sensorMeio.pressed = false;
        Serial.println("Meio");
        // chamar função de meio de portao aqui
    }
    if (sensorFechado.pressed)
    {
        sensorFechado.pressed = false;
        resetTempoAberto();
        Serial.println("Fechado");
        // chamar função de portao fechado aqui
    }
    // se nenhum sersor está pressionado, o portão está em movimento
    if (!sensorAberto.pressed && !sensorMeio.pressed && !sensorFechado.pressed)
    {
        Serial.println("Em movimento");
        // chamar função de portao em movimento aqui
        contarTempoAberto();
    }

    if (tempoAberto > limiteTempoAberto)
    {
        Serial.println("Portão aberto por mais de 60 segundos");
        // chamar função de alerta aqui
    }

    // tratamento do tempo que o portão fica aberto
    // se estiver aberto mais do que o tempo limite, enviar alerta
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
    tempoAberto += millis();
};

/* Funções de interação arquivos ESP */

void writeFile(String state, String path, String hora)
{
  File rFile = SPIFFS.open(path, "r+"); // 'r+' para leitura e escrita

  if (!rFile)
  {
    Serial.println("Erro ao abrir arquivo!");
    return;
  }

  rFile.seek(0); // Mover o cursor para o início do arquivo

  int lineCount = 0;
  int maxLines = 10;
  String lines[maxLines];

  // Ler linhas do arquivo
  while (rFile.position() < rFile.size())
  {
    lines[lineCount] = rFile.readStringUntil('\n');
    lineCount++;
  }

  rFile.seek(0); // Mover o cursor para o início do arquivo

  if (lineCount / 2 >= maxLines) // Se atingir o máximo de elementos, sobrescrever a partir da segunda entrada
    rFile.seek(lines[2].length() + lines[3].length() + 4); // 4 é a quantidade de caracteres adicionados por println

  // Escrever no arquivo
  rFile.println(hora);
  rFile.println(state);
  Serial.println("Gravou!");

  rFile.close();
}


String readFile(String path)
{
  Serial.println("Read file");
  File rFile = SPIFFS.open(path, "r"); // r+ leitura e escrita
  if (!rFile)
  {
    Serial.println("Erro ao abrir arquivo!");
    s = "";

    return s;
  }

  else
  {
    Serial.print("---------- Lendo arquivo " + path + "  ---------");
    while (rFile.position() < rFile.size())
    {
      String line = rFile.readStringUntil('\n'); // Lê uma linha do arquivo
      Serial.println(line);
      s = line;
    }
    rFile.close();

    return s;
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
