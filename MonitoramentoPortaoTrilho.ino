#include <WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
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

#define LEDWifi 2
#define wifi_ssid "Wokwi-GUEST"
#define wifi_password ""
#define botaoControleAbrir 23
#define botaoControleFechar 22

Button sensorAberto = {4, false, LOW, LOW, 0};
Button sensorMeio = {18, false, LOW, LOW, 0};
Button sensorFechado = {19, false, LOW, LOW, 0};

int wifi_timeout = 10000;

WiFiClient wifi_client;
PubSubClient client(wifi_client);

const char *mqtt_broker = "YourBroker";
const int mqtt_port = 8883;

bool comandoAbrir = false;
bool comandoFechar = false;
bool mqtt_connected = false;

/* Variaveis de arquivo */
String path, state;
String fileName = "/statusLogs.txt";
int maxLines = 10;

WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000); // Cria um objeto "NTP" com as configurações utilizada no Brasil
String hora;

unsigned long int debounceDelay = 50;

unsigned long int limiteTempoAberto = 60000; // 60 segundos
hw_timer_t *Timer0_Cfg = NULL;

//-----------------------------------------------------------------------------------
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
        digitalWrite(LEDWifi, LOW);
    }
    else
    {
        Serial.print("Conectado com o IP: ");
        Serial.println(WiFi.localIP());
        digitalWrite(LEDWifi, HIGH);
    }
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Tentando conexão MQTT...");
        if (client.connect("ESP32Client", "mosquitto", "mosquitto"))
        {
            Serial.println("Conectado");
            // Subscribe
            client.subscribe("/mosquitto/portao/comandos");
        }
        else
        {
            Serial.print("falhou, rc=");
            Serial.print(client.state());
            Serial.println(" tente novamente em 0.5 segundos");
            delay(500);
        }
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

//.................................................................
void openFS(void)
{
    if (!SPIFFS.begin())
        Serial.println("\nErro ao abrir o sistema de arquivos");
    else
        Serial.println("\nSistema de arquivos aberto com sucesso!");
}

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
    File rFile = SPIFFS.open(path, "r");

    if (!rFile)
    {
        Serial.println("Erro ao tentar acessar os logs!");
        return;
    }

    else
    {
        Serial.println("---------- Logs  ---------");
        while (rFile.position() < rFile.size())
        {
            String line = rFile.readStringUntil('\n');
            Serial.println(line);
            filaDeStrings.push(line);
        }

        rFile.close();
    }
}

void formatFile()
{
    SPIFFS.format();
    Serial.println("Formatação concluida!");
}

// -------- Tasks --------
TaskHandle_t controlePortao;
TaskHandle_t reconectMqttHendle;

void ControleRemotoPortao(void *parameter)
{
    while (1)
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
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void reconectMqtt(void *paramter){
  if (client.connected())
    {
      vTaskSuspend(NULL);
    }
  while(1) {
    if (!client.connected())
    {                // Se MQTT não estiver conectado tenta reconectar
        reconnect();
        if (client.connected())
        {
          Serial.println("Reconexão MQTT bem-sucedida!");
          vTaskSuspend(NULL); // Suspende a tarefa atual (reconectMqtt)
        }
        else
        {
          Serial.println("Falha ao reconectar ao MQTT. Tentando novamente...");
        }
    }
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup()
{
    pinMode(sensorAberto.PIN, INPUT);
    pinMode(sensorMeio.PIN, INPUT);
    pinMode(sensorFechado.PIN, INPUT);
    attachInterrupt(sensorAberto.PIN, interrupcaoAberto, CHANGE);
    attachInterrupt(sensorMeio.PIN, interrupcaoMeio, CHANGE);
    attachInterrupt(sensorFechado.PIN, interrupcaoFechado, CHANGE);

    pinMode(LEDWifi, OUTPUT);
    pinMode(botaoControleAbrir, OUTPUT);
    pinMode(botaoControleFechar, OUTPUT);
    Serial.begin(115200);

    openFS();
    //formatFile();

    Timer0_Cfg = timerBegin(80);
    timerWrite(Timer0_Cfg, 0);

    WiFi.mode(WIFI_STA); //"station mode": permite o ESP32 ser um cliente da rede WiFi
    WiFi.begin(wifi_ssid, wifi_password);
    connectWiFi();

    ntp.begin();
    ntp.forceUpdate();

    estadoPortao = FECHADO;

    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);

    xTaskCreatePinnedToCore(
        ControleRemotoPortao,
        "Task controle remoto",
        2048,
        NULL,
        1,
        &controlePortao,
        0);
    
    xTaskCreatePinnedToCore(
        reconectMqtt,
        "Task reconect MQTT",
        2048,
        NULL,
        1,
        &reconectMqttHendle,
        0);

    delay(100);
}

void loop()
{
    acaoSensores();

    uint64_t tempoAberto = timerReadMillis(Timer0_Cfg);

    if (tempoAberto > limiteTempoAberto)
    {
        Serial.println("Portão aberto por mais de 60 segundos");
        timerStop(Timer0_Cfg);
        timerWrite(Timer0_Cfg, 0);
        client.publish("/mosquitto/portao/alarme", "Alarme ativado");
    }
    delay(100);
}
