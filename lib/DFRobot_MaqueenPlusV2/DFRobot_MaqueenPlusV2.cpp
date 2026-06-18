#include "DFRobot_MaqueenPlusV2.h"

#if defined(NRF5) || defined(NRF52833)
#include <avr/interrupt.h>
#include <basic.h>
#endif


volatile uint32_t tmpV2=0;
volatile int counterV2=0;
volatile unsigned long tsV2;
volatile uint32_t dataV2;
volatile uint32_t eventDataV2;
volatile bool iravailableV2;
long micrV2 = 0;
uint32_t rec_dataV2;

#if defined(NRF52833)
int ir_codeV2 = 0x00;
int ir_addrV2 = 0x00;
#endif

DFRobot_MaqueenPlusV2 *IRremote_dal_ptV2 = NULL;
bool task_initV2;

int8_t transMindV2(uint8_t dataV2){
	switch(dataV2){
		case 255: dataV2 = 0; break;
		case 127: dataV2 = 1; break;
		case 191: dataV2 = 2; break;
		case 223: dataV2 = 4; break;
		case 95: dataV2 = 5; break;
		case 159: dataV2 = 6; break;
		case 239: dataV2 = 8; break;
		case 111: dataV2 = 9; break;
		case 175: dataV2 = 10; break;
		case 207: dataV2 = 12; break;
		case 79: dataV2 = 13; break;
		case 143: dataV2 = 14; break;
		case 247: dataV2 = 16; break;
		case 119: dataV2 = 17; break;
		case 183: dataV2 = 18; break;
		case 215: dataV2 = 20; break;
		case 87: dataV2 = 21; break;
		case 151: dataV2 = 22; break;
		case 231: dataV2 = 24; break;
		case 103: dataV2 = 25; break;
		case 167: dataV2 = 26; break;
		default: return -1;
	}
	return dataV2;
}

#if defined(NRF5) || defined(NRF52833)
void loopEventV2(){
    while(true){
        uint32_t val = eventDataV2;
        eventDataV2 = 0;
        if(val != 0){
            //if(((val&0xff)^((val>>8)&0xff)) == 0xff){
                if(IRremote_dal_ptV2->IRcb != NULL){
                    IRremote_dal_ptV2->IRcb(transMindV2(val));
                }
            //}
        }

    fiber_sleep(50);

    }
}
#endif



#if defined(NRF52833)
void Ir_init(int e) {
	if(e == 0) return;
    iravailableV2 = false;

    unsigned long long x = micros();
    unsigned long interval = x-tsV2;

    tsV2 = x;
    if(interval > 3000){
        counterV2 = 0;
        tmpV2 = 0;
        return;
    }

    if((interval > 2000) && (interval<2500)){
        tmpV2 <<= 1;
        tmpV2 |=  1;
    }else{
        tmpV2 <<= 1;
    }

    counterV2++;

    if(counterV2 == 32){
        IRremote_dal_ptV2->decode(tmpV2); 
        counterV2 = 0;
    }

}
#else
void Ir_init() {
    iravailableV2 = false;

    unsigned long long x = micros();
    unsigned long interval = x-tsV2;

    tsV2 = x;
    if(interval > 3000){
        counterV2 = 0;
        tmpV2 = 0;
        return;
    }

    if((interval > 2000) && (interval<2500)){
        tmpV2 <<= 1;
        tmpV2 |=  1;
    }else{
        tmpV2 <<= 1;
    }

    counterV2++;

    if(counterV2 == 32){
        //Serial.print("tmpV2= ");Serial.println(tmpV2);
        IRremote_dal_ptV2->decode(tmpV2); 
        counterV2 = 0;
    }

}
#endif

DFRobot_MaqueenPlusV2::DFRobot_MaqueenPlusV2()
{
#if defined(NRF52833)      //解决麦昆V2 huskylens 同时上电启动后初始化不成功 wsq
    delay(500);
#elif defined(NRF5)         //microbit v1.3 也有上电后初始化不成功 wsq
    delay(200);
#endif
    IRremote_dal_ptV2 = this;
    first = true;
	_speedLeft = 0;
	_speedRight = 0;
    Wire.begin();
}

DFRobot_MaqueenPlusV2::~DFRobot_MaqueenPlusV2()
{
    first = true;
}

int8_t DFRobot_MaqueenPlusV2::getData(void) {
    uint32_t _data = irData;
    irData = 0;
    return transMindV2(_data);
}

void DFRobot_MaqueenPlusV2::decode(uint32_t code) {  
    //byte add = (code>>24)&0xff;
    dataV2 = (code)&0xffffffff;
    eventDataV2 = dataV2;
    irData = dataV2;
	irEventData = irData;
    iravailableV2 = true;
}

void DFRobot_MaqueenPlusV2::enableIRIn() {
    pinMode(recvpin,INPUT);
    attachInterrupt(recvpin,Ir_init,RISING);
}


void DFRobot_MaqueenPlusV2::IRbegin(int recvpin1) {
	recvpin = recvpin1;
#if defined(NRF52833)
    if(recvpin1 == 20){//maqueen maqueenPlus  //锟斤拷锟斤拷p16锟叫断诧拷锟斤拷锟斤拷 bug wsq
		;
	}else{
		this->enableIRIn();
		this->enable = true;
	}
#endif
    this->enableIRIn();
    this->enable = true;

}

void DFRobot_MaqueenPlusV2::setCallback(IRCall cb){
    IRcb = cb;
    if(!task_initV2){
#if defined(NRF5)
    create_fiber(loopEventV2);
#elif defined(NRF52833)
    if(recvpin == 20){  // bug wsq 16引脚可以进中断
		//this->resume();
		//this->enable = false;
		// create_fiber(loopEventMaqueen);
	}else{
		create_fiber(loopEventV2);
	}
#endif
        task_initV2 = true;
    }
}
uint8_t DFRobot_MaqueenPlusV2::speedMap(uint8_t speed)
{
    float outputSpeed;
    if(speed > 255) {
        outputSpeed = 255;
    } else if (speed > 200 && speed <= 255) {
        outputSpeed = 0.43*speed;
    } else if (speed > 150 && speed <= 200) {
        outputSpeed = 0.75*speed - 0.0016*speed*speed;
    } else if (speed > 100 && speed <= 150) {
        outputSpeed = 0.9*speed - 0.0024*speed*speed;
    } else if (speed > 50 && speed <= 100) {
        outputSpeed = 1.23*speed - 0.006*speed*speed;
    } else {
        outputSpeed = speed;
    }
    return outputSpeed;
}

void DFRobot_MaqueenPlusV2::motorRun(Dir index, SpeedDir direction, int speed)
{
    // hardwareReset();
    // speed = speedMap(speed);    //新电机速度映射
    speed = abs(speed);
    if(speed >= 255) {
        speed = 255;
    }
    if(index > 3 || index < 1)
        return;
    if(index == 1){
        uint8_t buf[] = {0x00, direction, speed};
        this->i2cWriteBuffer(0x10, buf, 3);
		_speedLeft = speed;
    }
    if(index == 2){
        uint8_t buf[] = {0x02, direction, speed};
        this->i2cWriteBuffer(0x10, buf, 3);
		_speedRight = speed;
    }
    if(index == 3){
        uint8_t buf[] = {0x00, direction, speed, direction, speed};
        this->i2cWriteBuffer(0x10, buf, 5);
		_speedLeft = speed;
		_speedRight = speed;
    }
}

void DFRobot_MaqueenPlusV2::motorStop(Dir index)
{
    // hardwareReset();
    motorRun(index, CW, 0);
}

void DFRobot_MaqueenPlusV2::setRGB(Dir rgb, Color color)
{
    // hardwareReset();
    if (rgb == 1){
        uint8_t buf[2];
        buf[0] = 0x0B;
        buf[1] = color;
        i2cWriteBuffer(0x10, buf, 2);
    } else if (rgb == 2){
        uint8_t buf[2];
        buf[0] = 0x0C;
        buf[1] = color;
        i2cWriteBuffer(0x10, buf, 2);
    } else if (rgb == 3){
        uint8_t buf[3];
        buf[0] = 0x0B;
        buf[1] = color;
        buf[2] = color;
        i2cWriteBuffer(0x10, buf, 3);
    }
}

uint32_t DFRobot_MaqueenPlusV2::readPatrol(Patrol patrol)
{
    // hardwareReset();
    uint8_t y[1];
    i2cReadBuffer(0x1D, y, 1);
    uint8_t mark;
    switch(patrol){
        case 1: mark = (y[0] & 0x08) == 0x08 ? 1 : 0; break;
        case 2: mark = (y[0] & 0x04) == 0x04 ? 1 : 0; break;
        case 3: mark = (y[0] & 0x02) == 0x02 ? 1 : 0; break;
        case 4: mark = (y[0] & 0x10) == 0x10 ? 1 : 0; break;
        case 5: mark = (y[0] & 0x01) == 0x01 ? 1 : 0; break;
    }
    return mark;
}

uint32_t DFRobot_MaqueenPlusV2::readPatrolVoltage(Patrol patrol)
{
    // hardwareReset();
    uint8_t y[2];
    // i2cReadBuffer(0x1E, y, 12);
    if (patrol == 1) {
        i2cReadBuffer(0x24, y, 2);
        return y[0] | y[1] << 8;
    } else if (patrol == 2) {
        i2cReadBuffer(0X22, y, 2);
        return y[0] | y[1] << 8;
    } else if (patrol == 3) {
        i2cReadBuffer(0X20, y, 2);
        return y[0] | y[1] << 8;
    } else if (patrol == 4) {
        i2cReadBuffer(0X26, y, 2);
        return y[0] | y[1] << 8;
    } else if (patrol == 5) {
        i2cReadBuffer(0X1E, y, 2);
        return y[0] | y[1] << 8;
    }
    return 0;
}

String DFRobot_MaqueenPlusV2::readVersion()
{
    // hardwareReset();
    uint8_t len;
    i2cReadBuffer(0x32, &len, 1);
    uint8_t versionName[len+1];
    versionName[len] = '\0';
    i2cReadBuffer(0x33, versionName, len);
    return String((char *)versionName);
}

void DFRobot_MaqueenPlusV2::i2cWriteBuffer(int addr, unsigned char *p, int len)
{
    Wire.setClock(100000);
    Wire.beginTransmission(addr);
    for(int i=0; i<len; i++)
        Wire.write((uint8_t)p[i]);
    Wire.endTransmission();
}

void DFRobot_MaqueenPlusV2::i2cReadBuffer(uint8_t reg, unsigned char* p, int len)
{
    Wire.setClock(100000);
    Wire.beginTransmission(0x10);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(0x10, len);
    for(int i=0; i<len; i++)
        p[i] = Wire.read();
}

void DFRobot_MaqueenPlusV2::i2cWriteBuffer(uint8_t reg, uint8_t data) {
    Wire.beginTransmission(0x10);
    Wire.write(reg);                            
    Wire.write(data);
    Wire.endTransmission();
}

//初始化
void DFRobot_MaqueenPlusV2::sys_int(void) {
    i2cWriteBuffer(73 ,0x01);//初始化状态，防止microbit下载后和底板状态不同步
    delay(200); //waiting  reset
#if defined(NRF5) || defined(NRF52833)
    uint8_t version = 0;
    i2cReadBuffer(0x32, &version, 1);
    while (version == 0) {
        MMatrix.show(bitBuf[0]);
        delay(500);
        MMatrix.show(bitBuf[2]);
        i2cReadBuffer(0x32, &version, 1);
    }
    MMatrix.show(bitBuf[1]);
    delay(500);
    MMatrix.show(bitBuf[2]);
#else
    uint8_t version = 0;
    i2cReadBuffer(0x32, &version, 1);
    while (version == 0) {
        delay(500);
        i2cReadBuffer(0x32, &version, 1);
    }
#endif
}

//1开始 0停止
void DFRobot_MaqueenPlusV2::Line_traking(bool cmd) {
    if(cmd == 1)
        i2cWriteBuffer(60, 0x04|0x01); //0x04开始运行 0x08停止巡线 //7.11有改动
    else
        i2cWriteBuffer(60 ,0x08);
}

//选择电机
void DFRobot_MaqueenPlusV2::Motor_select(bool cmd) {
    if(cmd == 1)
        i2cWriteBuffer(62 ,0x01);
    else
        i2cWriteBuffer(62 ,0x00);
}

//巡线速度
void DFRobot_MaqueenPlusV2::Line_speed(uint8_t cmd){
    i2cWriteBuffer(63 ,cmd);
    mode_cmd=0x01;//开启用户巡线模式
}
//距离控制方向、速度固定2、距离 1前进 2后退
//增加了是否允许中断PID，如果选择不允许中断PID控制，就一直读地址87的寄存器，若该寄存器为1时就循环等待，不为1跳出
void DFRobot_MaqueenPlusV2::Distance_control(uint8_t dir, uint16_t data, bool interrupt){ 
    uint8_t speed = 2;
    mode_cmd=0x02;//开启精确控制
    if(data >= 6000) 
        data = 60000;
    i2cWriteBuffer(64 ,dir);
    i2cWriteBuffer(85 ,speed);//特殊写速度
    i2cWriteBuffer(65 ,data>>8);
    i2cWriteBuffer(66 ,data);
    i2cWriteBuffer(60 ,0x04|0x02);
    uint8_t flagBuffer = 0;
    if(interrupt == 0) { //中断
        i2cReadBuffer(87, &flagBuffer, 1);
        while (flagBuffer == 1) {
            delay(10);
            i2cReadBuffer(87, &flagBuffer, 1);
        }
    } 
}

//控制方向、速度固定2、角度 顺时针1 逆时针2
//增加了是否允许中断PID，如果选择不允许中断PID控制，就一直读地址87的寄存器，若该寄存器为1时就循环等待，不为1跳出
void DFRobot_MaqueenPlusV2::Angle_control(int angle, bool interrupt){
    uint8_t speed = 2;
    mode_cmd=0x02;
    i2cWriteBuffer(67 , angle >= 0 ? 1 : 2);
    i2cWriteBuffer(86 , speed);//特殊写速度
    // i2cWriteBuffer(68 ,0xff);//转角识别符号 删除
    i2cWriteBuffer(68 ,abs(angle));
    i2cWriteBuffer(60 ,0x04|0x02);
    uint8_t flagBuffer = 0;
    if(interrupt == 0) { //中断
        i2cReadBuffer(87, &flagBuffer, 1);
        while (flagBuffer == 1) {
            delay(10);
            i2cReadBuffer(87, &flagBuffer, 1);
        }
    } 
}

void DFRobot_MaqueenPlusV2::pid_stop(void){
    i2cWriteBuffer(60 ,0x10); //停止精确控制
}

//查询反馈路口状态1-4
 /*注意读寄存器应该-1*/
uint8_t DFRobot_MaqueenPlusV2::Inquire_cross(void){
    uint8_t buf;
    i2cReadBuffer(61 ,&buf ,1);
    return buf;
}

//1-4左、右、直行、停止
//十字路口
void DFRobot_MaqueenPlusV2::Croos1(uint8_t cmd){
    i2cWriteBuffer(69 ,cmd);
}

//只能1、2、4左、右、停止
void DFRobot_MaqueenPlusV2::Croos2(uint8_t cmd){
    if(cmd == 3) return;
    i2cWriteBuffer(70 ,cmd);
}

//只能1、3、4直、左、停止
void DFRobot_MaqueenPlusV2::Croos3(uint8_t cmd){
    if(cmd == 2) return;
    i2cWriteBuffer(71 ,cmd);
}

//只能2、3、4直、右、停止
void DFRobot_MaqueenPlusV2::Croos4(uint8_t cmd){
    if(cmd == 1) return;
    i2cWriteBuffer(72 ,cmd);
}

//1读左轮 0右轮   速度cm/s 此处得到值放大了5倍
float DFRobot_MaqueenPlusV2::Get_speed(bool cmd){
    uint8_t buf[2] = {0};
    i2cReadBuffer(76, &buf[0], 2);
    if(cmd) 
        return buf[0] / 5.0;
    else 
        return buf[1] / 5.0;
}

//读取光线强度  1读左 0右   0-1023
uint16_t DFRobot_MaqueenPlusV2::Get_light(bool cmd){
    uint16_t temp_light;
    uint8_t buf[4] = {0};
    i2cReadBuffer(78, &buf[0], 4); 
    if(cmd)  
        temp_light= buf[0] << 8 | buf[1];
    else  
        temp_light= buf[2] << 8 | buf[3];
    return temp_light;
}
//设置RGB车灯  1左 0右  
void DFRobot_MaqueenPlusV2::set_rgbLed(uint8_t cmd, uint32_t data){
    uint8_t buf;
    switch (data)
    {
        case 0xFF0000: buf = 1; break;
        case 0x00FF00: buf = 2; break;
        case 0xFFFF00: buf = 3; break;
        case 0x0000FF: buf = 4; break;
        case 0xFF00FF: buf = 5; break;
        case 0x00FFFF: buf = 6; break;
        case 0xFFFFFF: buf = 7; break;
        case 0x000000: buf = 0; break;
        default: buf = 0; break;
    }
    if(cmd == 2){
        i2cWriteBuffer(11, buf);
        i2cWriteBuffer(12, buf);
        return;
    }
    if(cmd == 1)
        i2cWriteBuffer(11, buf);
    else
        i2cWriteBuffer(12, buf);
}