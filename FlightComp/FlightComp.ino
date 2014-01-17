// Includes
#include <Thread.h>
#include <ThreadController.h>

#include <Wire.h>
#include <I2Cdev.h>
//#include <VirtualWire.h>

#include <SerialCommand.h>

#include <PESO_Timer.h>

#include <SdFat.h>
#include <PESO_Logger.h>

#include <inv_mpu.h>
#include <inv_mpu_dmp_motion_driver.h>
#include <PESO_IMU.h>

#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <PESO_GPS.h>

// Pin definitions
#define IMU_INTRPT    36
#define MICROSD_CS    33

#define CUTDOWN_PIN   2
#define SPARE_PIN     3
#define LED_PIN       5

#define RSSI_PIN      1    // Analog pin number

// C++ style serial prints
template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; }

// Objects
Logger logger;    // Logs to microSD over SPI
IMU imu;          // Inertial measurement unit - MPU6050 breakout
GPS gps;          // Global positioning system - Adafruit GPS breakout

/* For when we get the transceiver
HardwareSerial &Xceiver = Serial2;  // Serial connection to the high power transceiver
SerialCommand GROUND_cmdr(Xceiver); // Command and control from the ground
*/

HardwareSerial &XBee = Serial3;     // Serial connection to the XBee radio
SerialCommand XBEE_cmdr(XBee);      // Command and control over XBee

// Define threads
ThreadController Controller = ThreadController();
Thread IMU_thread = Thread();
Thread GPS_thread = Thread();
Thread COMMS_thread = Thread();
Thread CUTDOWN_thread = Thread();
Thread RUN_LED_thread = Thread();

// Variables
unsigned char led_off_count = 0;
long max_altitude = 0;


void setup()
{
  // Power settle delay
  delay(100);
  
  pinMode(CUTDOWN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);
  XBee.begin(9600);

  logger.initialize(MICROSD_CS, false);
  logger.append << setprecision(6) << "PESO Flight Computer Log File\n";
  logger.append << "IMU,timestamp,temperature,q_w,q_x,q_y,q_z,aa_x,aa_y,aa_z\n";
  logger.append << "GPS,timestamp,time,date,lat,lon,speed,alt\n";
  logger.append << "COMMS,timestamp,rssi,message\n";
  logger.append << "CUTDOWN,timestamp,message";
  logger.echo();
  logger.recordln();

  imu.initialize();
  
  gps.initialize();
  
  // Configure threads
  IMU_thread.onRun(IMU_CB);
  IMU_thread.setInterval(5);
  
  GPS_thread.onRun(GPS_CB);
  GPS_thread.setInterval(1000);
  
  CUTDOWN_thread.onRun(CUTDOWN_CB);
  CUTDOWN_thread.setInterval(2000);
  
  RUN_LED_thread.onRun(RUN_LED_CB);
  RUN_LED_thread.setInterval(500);
  
  Controller.add(&IMU_thread);
  Controller.add(&GPS_thread);
  Controller.add(&CUTDOWN_thread);
  Controller.add(&RUN_LED_thread);

  // Configure ground control commands
  XBEE_cmdr.addCommand("GET",XBEE_GET_CMD);
  XBEE_cmdr.addDefaultHandler(XBEE_UNKNOWN_CMD);
}

void loop()
{
  // Run threads
  Controller.run();

  // Check for command from ground control
  XBEE_cmdr.readSerial();
}


// Thread callbacks

void IMU_CB()
{
  if (digitalRead(IMU_INTRPT) == LOW)
    return;
 
  // Get data
  imu.update();
  
  // Append data type and time
  logger.append << "IMU," << imu.timestamp << ",";
  
  // Append temperature
  logger.append << (imu.temperature/(float)65536) << ",";
  
  // Append IMU data
  logger.append << (imu.q[0]/(float)1073741824) << "," << (imu.q[1]/(float)1073741824) << "," << (imu.q[2]/(float)1073741824) << "," << (imu.q[3]/(float)1073741824) << ",";
  logger.append << (imu.aa[0]/(float)imu.aa_sens) << "," << (imu.aa[1]/(float)imu.aa_sens) << "," << (imu.aa[2]/(float)imu.aa_sens);
  
  logger.echo();
  logger.recordln();
}
  
void GPS_CB()
{
  // Get data
  gps.update();
  
  if ((long)gps.altitude > max_altitude) { max_altitude = (long)gps.altitude; }

  // Append data type and time
  logger.append << "GPS," << millis() << ",";
  
  // Append GPS data
  logger.append << setfill('0') << setw(2) << int(gps.hour) << ":";
  logger.append << setw(2) << int(gps.minute) << ":";
  logger.append << setw(2) << int(gps.seconds) << "." << setw(3) << int(gps.milliseconds) << ",";
  
  logger.append << setw(2) << int(gps.day) << "/";
  logger.append << setw(2) << int(gps.month) << "/20";
  logger.append << setw(2) << int(gps.year) << ",";
  
  logger.append << gps.latitude << ",";
  logger.append << gps.longitude << ",";
  logger.append << gps.speed << ",";
  logger.append << gps.altitude;
  
  logger.echo();
  logger.recordln();
}

void CUTDOWN_CB()
{
  Serial.println("CUTDOWN_CB: Begin");
  digitalWrite(CUTDOWN_PIN, LOW);
  if (max_altitude > logger.getTopAlt())
  {
    digitalWrite(CUTDOWN_PIN, HIGH);
    
    // Append cutdown message
    logger.append << "CUTDOWN," << millis() << ",";
    logger.append << "Altitude reached: " << max_altitude;
    
    logger.echo();
    logger.recordln();
  }
}

void RUN_LED_CB()
{
  Serial.println("RUN_LED_CB: Begin");
  digitalWrite(LED_PIN, LOW);
  if (led_off_count++ > 5)
  {
    digitalWrite(LED_PIN, HIGH);
    led_off_count = 0;
  }
}

void XBEE_GET_CMD()
{
  char *arg = XBEE_cmdr.next();

  if (strcmp(arg, "IMU") == 0)
  {
    print_imu(XBee);
  }

  if (strcmp(arg, "GPS") == 0)
  {
    print_gps(XBee);
  }
}

void XBEE_UNKNOWN_CMD()
{
  XBee << "Unknown command" << endl;
}

void print_imu(HardwareSerial *_serial)
{
  _serial << "IMU at time " << imu.timestamp << endl;
  _serial << "TEMP: " << (imu.temperature/(float)65536) << endl;
  _serial << "QUAT: w=" << (imu.q[0]/(float)1073741824);
  _serial << " x=" << (imu.q[1]/(float)1073741824);
  _serial << " y=" << (imu.q[2]/(float)1073741824);
  _serial << " z=" << (imu.q[3]/(float)1073741824) << endl;
  _serial << "ACCL: x=" << (imu.aa[0]/(float)imu.aa_sens);
  _serial << " y=" << (imu.aa[1]/(float)imu.aa_sens);
  _serial << " z=" << (imu.aa[2]/(float)imu.aa_sens) << endl;
}

void print_gps(HardwareSerial *_serial)
{
  _serial << "GPS at time ";
  _serial << setfill('0') << setw(2) << int(gps.hour) << ":";
  _serial << setw(2) << int(gps.minute) << ":" << int(gps.seconds);
  _serial << "." << setw(3) << int(gps.milliseconds) << " ";
  _serial << setw(2) << int(gps.day) << "/";
  _serial << setw(2) << int(gps.month) << "/20";
  _serial << setw(2) << int(gps.year) << endl;
  _serial << "LAT: " << gps.latitude << endl;
  _serial << "LON: " << gps.longitude << endl;
  _serial << "SPD: " << gps.speed << endl;
  _serial << "ALT: " << gps.altitude << endl;
}