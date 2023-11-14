#define LED 2
#define Sensor01 4
#define Sensor02 32
#define Sensor03 33

void setup()
{
    pinMode(Sensor01, INPUT);
    pinMode(Sensor02, INPUT);
    pinMode(Sensor03, INPUT);
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    Serial.println();
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