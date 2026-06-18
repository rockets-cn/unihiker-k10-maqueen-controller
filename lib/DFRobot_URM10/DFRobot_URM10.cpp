#include "DFRobot_URM10.h"
#if defined(ESP_PLATFORM)
#include "pins_arduino.h"
#endif

float DFRobot_URM10::getDistanceCM(int trigPin, int echoPin)
{
    long duration;
    if (trigPin == echoPin)
    {
        pinMode(trigPin, OUTPUT);
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);

        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);

        pinMode(trigPin, INPUT);
        duration = myPulseIn(trigPin, HIGH, 100000);
    }
    else
    {
        pinMode(trigPin, OUTPUT);
        pinMode(echoPin, INPUT);
        digitalWrite(trigPin, LOW);

        if (digitalRead(echoPin) == 0)
        {
            digitalWrite(trigPin, LOW);
            digitalWrite(trigPin, HIGH);
            delayMicroseconds(20);
            digitalWrite(trigPin, LOW);
            duration = myPulseIn(echoPin, HIGH);
        }
        else
        {
            digitalWrite(trigPin, HIGH);
            digitalWrite(trigPin, LOW);
            duration = myPulseIn(echoPin, LOW);
        }
    }

    duration = duration / 58.0;
    // if ((duration <=0) || (duration > 500)) return 0;
    delay(10);
    return duration;
}

float DFRobot_URM10::getDistanceIN(int trigPin, int echoPin)
{
    return getDistanceCM(trigPin, echoPin) / 2.54;
}

uint64_t DFRobot_URM10::myPulseIn(uint32_t pin, uint32_t value, long maxDuration)
{
    int pulse = value == HIGH ? 1 : 0;
    uint64_t tick = micros();
    uint64_t maxd = (uint64_t)maxDuration;
    while (digitalRead(pin) != pulse)
    {
        if (micros() - tick > maxd)
            return 0;
    }
    uint64_t start = micros();
    while (digitalRead(pin) == pulse)
    {
        if (micros() - tick > maxd)
            return 0;
    }
    uint64_t end = micros();
    return end - start;
}