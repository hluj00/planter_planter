#include <Arduino.h>

class AnalogSensor
{
private:
    int pin;
    int min;
    int max;
    double lastValue;
    int controlPin;
    double mesureValue();
    bool invert;
    bool customCalculation;
    int (*calculation)(int);
public:
    AnalogSensor(int pin, int min, int max, int controlPin = 0);
    AnalogSensor(int pin, int (*caculation)(int), int controlPin = 0);
    double getValue();
    double getLastValue();
    void setInvertResult(bool invert);
};

AnalogSensor::AnalogSensor(int pin, int (*caculation)(int), int controlPin){
        this->controlPin = controlPin;
        this->pin = pin;
        this->calculation = caculation; 
        this->invert = false;
        this->customCalculation = true;
    }

AnalogSensor::AnalogSensor(int pin, int min, int max, int controlPin){
    this->pin = pin;
    this->min = min;
    this->max = max;
    this->controlPin = controlPin;
    this->invert = false;
    this->customCalculation = false;
}


/**
 *  vrací hodnotu jako procento v rozpětí min-max
 */
double AnalogSensor::getValue(){
    int value = mesureValue();
    if (this->customCalculation){
        lastValue = calculation(value);
    }else{
        value = (value > max) ? max : value;
        value = (value < min) ? min : value;
        lastValue = (value - min)/(double)(max - min)*100;
    
        if (this->invert)
        {
            lastValue = 1 - lastValue;
        }
    }

    return lastValue;
};

double AnalogSensor::mesureValue(){
    double value = 0;
    if (controlPin)
    {
        digitalWrite(SENSOR_SWITCH_PIN, HIGH);
        for (int i = 0; i < 10; i++){
            delay(50);
            value += analogRead(pin);
        }
        digitalWrite(SENSOR_SWITCH_PIN, LOW);
    }else{
        for (int i = 0; i < 10; i++){
            delay(50);
            value += analogRead(pin);
        }
    }
    
    return value/10;
}

double AnalogSensor::getLastValue(){
    return lastValue;
}

void AnalogSensor::setInvertResult(bool invert){
    this->invert = invert;
}