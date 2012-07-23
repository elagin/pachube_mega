// less -f /dev/ttyACM1 > out.txt
#include <OneWire.h>
#include <DallasTemperature.h>

#if defined(ARDUINO) && ARDUINO > 18
#include <SPI.h>
#endif
#include <Ethernet.h>
#include <EthernetDHCP.h>
#include <EthernetDNS.h>
#include <ERxPachube.h>
#include <BMP085.h>

#define ONE_WIRE_BUS 30         // Шина данных датчиков
#define TEMPERATURE_PRECISION 15 // Точность датчиков

#define PACHUBE_API_KEY	"5d3p_74NetgnpNlf_Rpsy6p9mZMQk3EbIB_FtpFjzwBDMeAAwDy_aCRS9w0bUsS-0Xydzvm-bvU3q2r1AeX9l-v6vt2Rg8VFU8Dj72z6wko77UkGZIzXdnRBMrwJ8jDF" // указываем API key созданный на pachube.com
#define PACHUBE_FEED_ID	43834 // указываем feed id созданный на pachube.com

ERxPachubeDataOut dataout(PACHUBE_API_KEY, PACHUBE_FEED_ID);

#define MAX_DS1820_SENSORS 7

char displayBuf[200];

unsigned long scanTempInterval = 60000;        // При умножении больше 1000 * 25 почему-то = 0

long previousScanTempMillis = 0;     // Предидущее время замера температуры

unsigned char sensorCount = 0;
char buf[40];
char bufSerial[20];

char ledScanTime = 13;
//char ledAlarm    = 40;

BMP085 dps = BMP085();      // Digital Pressure Sensor 
long Temperature = 0, Pressure = 0, Altitude = 0;

bool isRestart = true;

// Just a utility function to nicely format an IP address.
const char* ip_to_str(const uint8_t* ipAddr)
{
    static char buf[16];
    sprintf(buf, "%d.%d.%d.%d\0", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
    return buf;
}

struct sensorData
{
    DeviceAddress   addr;       // Адрес датчика
    char            name[20];   // Имя для отображения
    int             pachubeId;  // Идентификатор для Pachube
    float           temp;       // Текущее значение температуры
};

struct sensor
{
    byte            id;       // Идентификатор датчика из настрек
    DeviceAddress   addr;     // Адрес датчика
};

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xdE, 0xED };

//const char* serverName = "garage.stormway.ru\0";
//const char* serverName = "213.170.79.109\0";
//byte server[] = { 77,88,21,3 }; // ya.ru

//const int printRoundSize = 3;
//int printRound[printRoundSize] = {0,1,2};

OneWire oneWire(ONE_WIRE_BUS);  // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature dallasSensors(&oneWire);    // Pass our oneWire reference to Dallas Temperature.
//DeviceAddress insideThermometer, outsideThermometer;    // arrays to hold device addresses
sensorData sensorsParams[MAX_DS1820_SENSORS] = {
    0x28, 0x45, 0xAF, 0xC7, 0x02, 0x0, 0x0, 0x2C,"Remote", 1, 0,
    0x28, 0x93, 0xBB, 0xC7, 0x02, 0x00, 0x00, 0x39, "rename me", 2, 0,
    0x28, 0xB0, 0xDB, 0xC7, 0x02, 0x00, 0x00, 0xC7, "Out", 3, 0,
    0x28, 0x9B, 0xC5, 0xC7, 0x02, 0x00, 0x00, 0x57, "Ter cold", 4, 0,
    0x28, 0xEE, 0xD6, 0xC7, 0x02, 0x00, 0x00, 0x16, "Ter warm", 5, 0,
    0x28, 0x02, 0xDC, 0xC7, 0x02, 0x00, 0x00, 0xFF, "Battery", 6, 0,
    0x28, 0xFA, 0xDF, 0xC7, 0x02, 0x00, 0x00, 0x62, "Stepan", 7, 0
};

sensor sensors[MAX_DS1820_SENSORS];

void setup()
{
    pinMode(10, OUTPUT);
    pinMode(4, OUTPUT);
    digitalWrite(10, HIGH);
    digitalWrite(4, HIGH);

    delay(200);
    Serial.begin(9600);

    sprintf(displayBuf, "Attempting to obtain a DHCP lease...");
    Serial.println(displayBuf);

    EthernetDHCP.begin(mac);
    const byte* ipAddr = EthernetDHCP.ipAddress();
    EthernetDNS.setDNSServer(EthernetDHCP.dnsIpAddress());

    sprintf(displayBuf, "My IP address is  %s", ip_to_str(ipAddr));
    Serial.println(displayBuf);

    dallasSensors.begin();
    oneWire.reset_search();
    for(int i=0; i < MAX_DS1820_SENSORS; i++)
    {
        //        if(oneWire.search(sensorsParams[i].addr))
        if(oneWire.search(sensors[i].addr)) // Найден следующий датчик
        {
//            dallasSensors.setResolution(sensors[i].addr, TEMPERATURE_PRECISION); // Устанавливаем точность измерения
            for(uint8_t j = 0; j < sizeof(sensorsParams); j++)
            {
                if(compareAddres(sensorsParams[j].addr, sensors[i].addr)) // Ищем датчик по адресу в списке настроек
                {
                    sensors[i].id = j;                           // Связваем подключенный датчик с настройками
                    dataout.addData(sensorsParams[j].pachubeId); // указываем id для Pachube с которыми будем работать
                }
            }
            sensorCount++;
        }
        else
        {
            break; // Датчика закончились
        }

    }
    
    dataout.addData(97); // указываем id для Pachube с которыми будем работать
    dataout.addData(98); // указываем id для Pachube с которыми будем работать
    dataout.addData(99); // указываем id для Pachube с которыми будем работать    
         dallasSensors.setResolution(12);
    pinMode(ledScanTime, OUTPUT);
    sprintf(buf, "Found %d sensors", sensorCount);
    Serial.println(buf);
    delay(500);
    if(sensorCount==0)
    {
        return;
    }

    digitalWrite(4, HIGH);
    int a = 0;
    
    dps.init(MODE_STANDARD, 100374, false);  // 101850Pa = 1018.50hPa, false = using Pa units
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        // zero pad the address if necessary
        if (deviceAddress[i] < 16)
            Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void getSensorData()
{
    Serial.println("-= getSensorData start =-");
    byte present = 0;
    digitalWrite(ledScanTime, HIGH);
    dallasSensors.requestTemperatures();
    for(int i=0; i<sensorCount; i++)
    {
        byte sensor = sensors[i].id; // Выбираем датчик
        sensorsParams[sensor].temp = dallasSensors.getTempC(sensorsParams[sensor].addr); // Сохраняем показание.
        dataout.updateData(sensorsParams[sensor].pachubeId, dallasSensors.getTempC(sensorsParams[sensor].addr)); // Сохраняем показание.
/*        dataout.updateData(sensorsParams[sensor].pachubeId, sensorsParams[sensor].temp); //отправляем данные на Pacube
        sprintf(buf, "%s: ", sensorsParams[sensor].name);
        Serial.print(buf);
        Serial.println(sensorsParams[sensor].temp);*/

    }
    
  dps.getTemperature(&Temperature);   
  int pach_temp = Temperature;
  dataout.updateData(99, pach_temp);  
  
  dps.getPressure(&Pressure);
  float pach_Pressure = Pressure;
  dataout.updateData(99, pach_Pressure);  
  
  dps.getAltitude(&Altitude);
  float pach_Altitude = Altitude;
  dataout.updateData(99, pach_Altitude);  
  
  int status = dataout.updatePachube();
  digitalWrite(ledScanTime, LOW);
  Serial.println("-= getSensorData end =-");
}

bool compareAddres(DeviceAddress deviceAddress, DeviceAddress deviceAddress2)
{
    int count = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        // zero pad the address if necessary
        if (deviceAddress[i] == deviceAddress2[i])
            count ++;
    }
    return count==8;
}

bool sendToPacube()
{
    if(sensorCount > 0)
    {    for(int i=0; i<sensorCount; i++)
        {
            byte sensor = sensors[i].id;
            dataout.updateData(sensorsParams[sensor].pachubeId, sensorsParams[sensor].temp); //отправляем данные на Pacube
            sprintf(buf, "%s: ", sensorsParams[sensor].name);
            Serial.print(buf);
            Serial.println(sensorsParams[sensor].temp);
        }
        int status = dataout.updatePachube();
        sprintf(buf, "sync status code <OK == 200> => %d", status);
        Serial.println(buf);
    }
}

void printTime()
{
    unsigned long Seconds = millis()/1000;
    const unsigned long  SecondsInDay = 60 * 60 * 24;
    int day = Seconds / SecondsInDay;
    // Отбрасываем дни
    Seconds %= SecondsInDay;
    int hours = Seconds / 3600;
    // Отбрасываем часы
    Seconds %= 3600;
    // Вычисляем и выводим количество минут
    int mins = Seconds / 60;
    // Вычисляем и выводим количество секунд
    Seconds = Seconds % 60;
    sprintf(buf, "%02d %02d:%02d:%02d", day, hours, mins, Seconds);
    Serial.println(buf);
}

void loop(void)
{
    unsigned long currentMillis = millis();
    if(isRestart) // Если только что запустились, все делаем без задержки
    {
        getSensorData();
        sendToPacube();
        isRestart = false;
    }

    unsigned long currentInterval = currentMillis - previousScanTempMillis;
    if(currentInterval >= scanTempInterval)
    {
        Serial.print("getSensorData");
        Serial.println(currentMillis - previousScanTempMillis);
        previousScanTempMillis = currentMillis;
        getSensorData();
//        sendToPacube();
    }

    unsigned long testTime = currentMillis - previousScanTempMillis;
    printTime();
    //    Serial.println("------------");
    Serial.print("currentMillis: ");
    Serial.println(currentMillis);

    Serial.print("currentInterval: ");
    Serial.println(currentInterval);

    Serial.print("previousScanTempMillis: ");
    Serial.println(previousScanTempMillis);

    Serial.print("scanTempInterval: ");
    Serial.println(scanTempInterval);
    /*
    Serial.print(" (");
    Serial.print(scanTempInterval);
        Serial.println(")");
    Serial.println("|||||");
    sprintf(buf, "%i (%i)", currentMillis - previousScanTempMillis, scanTempInterval);
    Serial.println(buf);*/
    Serial.println("------------");
    delay(1000);
}

