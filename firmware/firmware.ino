#include <SPI.h>
#include <MFRC522.h>
#define RST_PIN         9        // Пин rfid модуля RST
#define SS_PIN          10       // Пин rfid модуля SS

MFRC522 rfid(SS_PIN, RST_PIN);   // Объект rfid модуля
MFRC522::MIFARE_Key key;         // Объект ключа
MFRC522::StatusCode status;      // Объект статуса

byte block = 6;  // переменная где лежит номер блока
byte buffer[18]; // переменная для хранения данных записи-чтения
byte size = sizeof(buffer); // размер буфера

byte value = 0;


/*
id данных передаваемых по rfid

блок содержит 18 байт данных:
0 - активная-1, неактивна-0  // первый байт говорит использован ли предмет
1 - 0 - восстанавливающий здоровье  //  второй байт указывает на тип предмета
    1 - восстанавливающий радиацию
    2 - улучщающий
    3 - управляющий
    4 - возрождающий
2 - 0...10 // третий байт показывает силу воздействия
3 - 1 - гравитационная // четвертый - тип воздействия
    2 - температурная
    3 - электрическая
    4 - химическая
    5 - пси
    6 - радиация

пример [0,0,10,0] - использованная аптечка 100%
       [1,2,5,4] - улучшает химическую защиту на 50%

*/


const uint8_t pinRX = 7;                          //  Определяем вывод RX (программного UART) на плате Arduino к которому подключён вывод TX модуля. Номер вывода можно изменить.
const uint8_t pinTX = 8;                          //  Определяем вывод TX (программного UART) на плате Arduino к которому подключён вывод RX модуля. Номер вывода можно изменить.

#include <SoftwareSerial.h>                       //  Подключаем библиотеку для работы с программным UART, до подключения библиотеки iarduino_GPS_NMEA.
#include <iarduino_GPS_NMEA.h>                    //  Подключаем библиотеку для расшифровки строк протокола NMEA получаемых по UART.

#include <Wire.h>                                 // Подключаем библиотеку для работы с аппаратной шиной I2C, до подключения библиотеки iarduino_OLED_txt.
#include <iarduino_OLED_txt.h>                    // Подключаем библиотеку iarduino_OLED_txt.
iarduino_OLED_txt myOLED(0x3C);   

SoftwareSerial    SerialGPS(pinRX, pinTX);        //  Объявляем объект SerialGPS для работы с функциями и методами библиотеки SoftwareSerial, указав выводы RX и TX Arduino.
iarduino_GPS_NMEA gps;                            //  Объявляем объект gps для работы с функциями и методами библиотеки iarduino_GPS_NMEA.

struct zone{
  float top;
  float bottom;
  float left;
  float right;
  byte type;
  int strength;
};

zone zones [] = {  // список зон {lat, lat, lon, lon, id, strength}, strength - количество здоровья за секунду
  /*
  1 - гравитационная
  2 - температурная
  3 - электрическая
  4 - химическая
  5 - пси
  6 - радиация
  7 - восстановление TODO фракции для восстановления
  */
  {54.77100, 54.77200, 69.17100, 69.17200, 1, 2},
  {54.77200, 54.77300, 69.17200, 69.17300, 2, 2},
  {54.77300, 54.77400, 69.17300, 69.17400, 3, 2},
  {54.77400, 54.77500, 69.17400, 69.17500, 4, 2},
  {54.77500, 54.77600, 69.17500, 69.17600, 5, 2},
  {54.77600, 54.77700, 69.17600, 69.17700, 6, 1000},
  {54.77700, 54.77800, 69.17700, 69.17800, 7, 2}
  };

struct player{
  float health = 50; // количество здоровья
  long radiation = 10000; // количество радиации

  float armor_gravi = 1; // 1 - нет защиты, 0 - иммунитет
  float armor_heat = 1; // 1 - нет защиты, 0 - иммунитет
  float armor_electro = 1; // 1 - нет защиты, 0 - иммунитет
  float armor_chemi = 1; // 1 - нет защиты, 0 - иммунитет
  float armor_psy = 1; // 1 - нет защиты, 0 - иммунитет
  float armor_rad = 1; // 1 - нет защиты, 0 - иммунитет
};

player current_player;

void setup() {
  Serial.begin(9600);            // Инициализация Serial
  SPI.begin();                   // Инициализация SPI
  rfid.PCD_Init();               // Инициализация модуля
  for (byte i = 0; i < 6; i++) { // Наполняем ключ
    key.keyByte[i] = 0xFF;       // Ключ по умолчанию 0xFFFFFFFFFFFF
  }
  SerialGPS.begin(9600);                       //  Инициируем работу с программной шиной UART для получения данных от GPS модуля на скорости 9600 бит/сек.
  gps.begin(SerialGPS);                        //  Инициируем расшифровку строк NMEA указав объект используемой шины UART (вместо программной шины, можно указывать аппаратные: Serial, Serial1, Serial2, Serial3).
  myOLED.begin(&Wire); // &Wire1, &Wire2 ...   //  Инициируем работу с дисплеем, указав ссылку на объект для работы с шиной I2C на которой находится дисплей (по умолчанию &Wire).
  myOLED.setFont(MediumFont);   

}

void loop() {

  gps.read();                                  //  Читаем данные (чтение может занимать больше 1 секунды). Функции можно указать массив для получения данных о спутниках.
  //if (gps.errPos) return;  // прерываем цикл, из-за недостоверности данных жпс

  //gps.latitude = 54.77701; 
  //gps.longitude = 69.17701;
  zone current_zone = get_zone_status(gps.latitude, gps.longitude);

  current_player.health = current_player.health - get_damage(current_zone); // подсчет урона здоровью
  current_player.health > 100 ? current_player.health = 100 : current_player.health; // огрничение на 100 здоровья


  // вывод информации
  myOLED.print(gps.latitude,1,1,5);
  myOLED.print(gps.longitude,1,3,5);

  myOLED.print(gps.latitude,1,1,5);
  myOLED.print(gps.longitude,1,3,5);

  myOLED.print(current_zone.type, 1, 5);
  myOLED.print(current_player.health, 1, 7);


  Serial.print("Тип зоны: ");
  Serial.print(current_zone.type);
  Serial.print(" Урон зоны: ");
  Serial.println(current_zone.strength);
  Serial.print("Здоровье: ");
  Serial.print(current_player.health);
  Serial.print(" Радиация: ");
  Serial.println(current_player.radiation);

  // работа с RFID _____________________________________________

  if ( ! rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if ( ! rfid.PICC_ReadCardSerial()) {
    return;
  }


  status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid)); // аутентификация на чтение блока
  Serial.println(status);
  status = rfid.MIFARE_Read(block, buffer, &size);
  Serial.println(status);
  for (byte i=0; i < 16; i++){
    Serial.print(buffer[i]);
    Serial.print(' ');
    buffer[i] = value;
  }
  Serial.println();

  status = rfid.MIFARE_Write(block, buffer, 16);
  Serial.println(status);

  value ++;
  if (buffer[0] == 10) value = 0;

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

}


zone get_zone_status(float lat, float lon){  // функция возвращает текущую зону
  byte len = sizeof(zones) / sizeof(zones[0]);
  for(int i=0; i < len; i++){
    if (zones[i].top< lat && zones[i].bottom > lat && zones[i].left < lon && zones[i].right> lon){
        return zones[i];
    } 
  }
  return zone {0,0,0,0,0,0};
}

float get_damage(zone current_zone){ // считает и возвращает урон, накапливает радиацию
  float damage = 0;

  if (current_player.radiation > 0 && current_player.radiation < 100000){
    damage = 0.001;
  }
  else if (current_player.radiation > 100000 && current_player.radiation < 200000){
    damage = 0.05;
  }
  else if (current_player.radiation > 200000){
    damage = 0.5;
  }

  switch (current_zone.type){
    case 0: return damage;
    case 1: return damage + current_zone.strength * current_player.armor_gravi;
    case 2: return damage + current_zone.strength * current_player.armor_heat;
    case 3: return damage + current_zone.strength * current_player.armor_electro;
    case 4: return damage + current_zone.strength * current_player.armor_chemi;
    case 5: return damage + current_zone.strength * current_player.armor_psy;
    case 6: 
        current_player.radiation += (long) current_zone.strength;
        Serial.println(current_player.radiation);
        Serial.println(current_zone.strength);
        return damage;
    case 7: 
        current_player.radiation <= 0 ? current_player.radiation = 0 : current_player.radiation -= 250;
        return - current_zone.strength*0.1;
  }
};
