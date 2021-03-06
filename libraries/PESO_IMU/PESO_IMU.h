#ifndef __PESO_IMU__
#define __PESO_IMU__

#include <Arduino.h>
#include <inv_mpu.h>
#include <inv_mpu_dmp_motion_driver.h>

class IMU
{
  public:
  
  short mpuIntStatus;   // holds actual interrupt status byte from MPU
  
  long  q[4];          // [w, x, y, z]         quaternion container
  short gyro[3];       // [x, y, z]            accel sensor measurements
  short aa[3];         // [x, y, z]            accel sensor measurements
  
  float gyro_sens;
  unsigned short aa_sens;
  
  short sensors;
  unsigned char more;
  unsigned long timestamp;
  long temperature;
  
  IMU();
  int initialize();
  void update();
  
};

#endif __PESO_IMU__