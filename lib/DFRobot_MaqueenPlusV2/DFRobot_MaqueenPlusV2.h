#ifndef DFROBOT_MAQUEENPLUSV2_H
#define DFROBOT_MAQUEENPLUSV2_H
#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_URM10.h>
#include <DFRobot_NeoPixel.h>
#if defined(NRF5) || defined(NRF52833)
#include <Microbit_Matrix.h>
#endif
typedef void (*IRCall)(int8_t val);

class DFRobot_MaqueenPlusV2 : public DFRobot_URM10, public DFRobot_NeoPixel
{
public:

    enum Dir {
        LEFT = 1,
        RIGHT = 2,
        ALL = 3
    };

    enum SpeedDir {
        CW = 0,
        CCW = 1
    };

    enum Patrol {
        L1 = 1,
        M = 2,
        R1 = 3,
        L2 = 4,
        R2 = 5
    };

    enum Color {
        OFF = 0,
        ON = 1
    };

    uint8_t recvpin;
    bool enable;
    uint32_t irData;
    uint32_t irEventData;

    IRCall IRcb = NULL;
    void enableIRIn();
    void setCallback(IRCall cb);
    int8_t getData(void);


    void motorRun(Dir index, SpeedDir direction, int speed);
    void motorStop(Dir index);

    void setRGB(Dir rgb, Color color);

    uint32_t readPatrol(Patrol patrol);
    uint32_t readPatrolVoltage(Patrol patrol);
    String readVersion();

    void IRbegin(int recvpin1);
    void decode(uint32_t code);
    DFRobot_MaqueenPlusV2();
    ~DFRobot_MaqueenPlusV2();
    void sys_int(void);
    void Line_traking(bool cmd);
    void Motor_select(bool cmd);
    void Line_speed(uint8_t cmd);
    void Distance_control(uint8_t dir, uint16_t data, bool interrupt);
    void Angle_control(int angle, bool interrupt);
    void pid_stop(void);
    uint8_t Inquire_cross(void);
    void Croos1(uint8_t cmd);
    void Croos2(uint8_t cmd);
    void Croos3(uint8_t cmd);
    void Croos4(uint8_t cmd);
    float Get_speed(bool cmd);
    uint16_t Get_light(bool cmd);
    void set_rgbLed(uint8_t cmd, uint32_t data);
protected:
    bool first;
	uint8_t _speedLeft;
	uint8_t _speedRight;
    uint8_t mode_cmd = 0;
    const uint8_t bitBuf[3][5]={{B10001,B01010,B00100,B01010,B10001},{B00000,B00001,B00010,B10100,B01000},{B00000,B00000,B00000,B00000,B00000}};
    void i2cWriteBuffer(int addr, unsigned char *p, int len);
    void i2cWriteBuffer(uint8_t reg, uint8_t data);
    void i2cReadBuffer(uint8_t reg, unsigned char* p, int len);
    uint8_t speedMap(uint8_t speed);
    
};

#endif

