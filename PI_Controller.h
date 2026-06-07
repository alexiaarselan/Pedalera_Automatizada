#pragma once

class PI_Controller
{
public:
    PI_Controller(float kp, float ki, float sampleTime,
                  float deadZone = 0.0f, float u_max = 255.0f);

    void  setGains(float kp, float ki);
    void  setMaxOutput(float u_max);
    void  setDeadZone(float deadZone);
    void  reset();
    float output(float input, float setPoint);

private:
    float kp, ki;
    float sampleTime;
    float deadZone;
    float u_max;
    float a, b;
    float e[2] = {0, 0};
    float u[2] = {0, 0};
};