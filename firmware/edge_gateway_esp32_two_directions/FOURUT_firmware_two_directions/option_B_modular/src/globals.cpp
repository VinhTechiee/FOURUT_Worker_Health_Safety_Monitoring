
#include "globals.h"

DFRobot_BloodOxygen_S_I2C max30102(&Wire, MAX30102_ADDR);
Adafruit_MPU6050 mpu;

volatile bool alarmActive = false;
volatile bool alarmCancelled = false;
volatile int currentAlarmLevel = 0;

unsigned long cancelCooldownUntil = 0;
AlertCooldown alertCooldowns[ALERT_COOLDOWN_SLOTS];

bool firstMpuSample = true;
bool max30102Available = false;

unsigned long alarmStartTime = 0;
unsigned long lastHRVCheck = 0;

int rmssdLowCount = 0;
bool fatigueLevel1Sent = false;

unsigned long ibiBuffer[MAX_IBI_BUFFER];
int ibiCount = 0;

unsigned long ibiHistory[MEDIAN_WINDOW];
int historyIndex = 0;
int historyCount = 0;

unsigned long fallLatchUntil = 0;

SensorData latestData;

SemaphoreHandle_t dataMutex = NULL;
QueueHandle_t alertQueue = NULL;
QueueHandle_t hrvQueue = NULL;
QueueHandle_t envQueue = NULL;

WiFiClientSecure espClient;
PubSubClient client(espClient);
