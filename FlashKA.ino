//для Arduino Nano
//Команды через терминал
//1...128                                              Смена канала связи
//on...off                                             Вкл/выкл корректора мощности, при потере соединения
//min...low...high...max                               Мощность соединения флешки и колонки
//? или help                                           Вызов меню комманд   

// Библиотеки --------------------------------------------
#include <SPI.h>                                         // Подключаем библиотеку для работы с шиной SPI
#include <nRF24L01.h>                                    // Подключаем файл настроек из библиотеки RF24
#include <RF24.h>                                        // Подключаем библиотеку для работы с nRF24L01+
#include <EEPROM.h>                                      // Чтение/запись в память

// Константы ---------------------------------------------
#define ack_timeout  750                                 // Время ожидания сигнала приемником в manualAck()
#define time_to_stanby 5000                              // сек*1000, для перехода в режим ожидания
#define s_confirmed  11112                               // Сабвуфер подтвердил смену настроек 
#define power_pref   10000                               // Префикс несущей команды 
#define channel_pref 30000                               // Префикс команды для смены канала
#define pipe_id 0x1234567890LL                           // Идентификатор трубы RF24
#define pipe_no 1                                        // Номер трубы RF24
#define s_delay_ask 1000                                 // Пауза для смены режима всеми устройствами перед завершением manualAck()
#define s_delay_con 1500                                 // Пауза signal_delay, когда устройства соединены
#define s_delay_sch 1500                                 // Пауза signal_delay, когда ведется поиск сабвуфера
#define s_delay_err 750                                  // Пауза signal_delay, когда ERROR

// Переменные --------------------------------------------
RF24     radio(10, 9);                                   // Создаём объект radio для работы с библиотекой RF24, указывая номера выводов nRF24L01+ (CE, CSN)
uint32_t err_timer;                                      // Таймер для восстановления соединения с сабом
int power = EEPROM[5];                                   // Мощность передатчика
bool auto_power = EEPROM[4];                             // Разрешение на корректировку мощности при потере соединения (адрес, вкл/выкл). 0-выкл, 1-вкл.
int signal_delay = 750;                                  // Задержка ID signal
int data[1];                                             // Создаём массив для приёма/отправки данных
int f_Channel = EEPROM[10];                              // Считываем номер канала для передачи данных
bool s_error = false;                                    // Ошибка соединения, считаем кол-во ошибок
bool s_connected = false;                                // Соединение отсутствует
int num_errors = 0;                                      // Количество ошибок соединения

void setup(){
//EEPROM.update(10, 108);                                // Инициализируем начальный канал для соединения (адрес, канал): 1...128
//EEPROM.update(5, 0);                                   // Инициализируем мощность для соединения (адрес, код мощности). 0...3-мощность, 4-автонастройка
//EEPROM.update(4, 0);                                   // Инициализируем разрешение на корректировку мощности при потере соединения (адрес, вкл/выкл). 0-выкл, 1-вкл.
    signal_delay = s_delay_sch;                          // Устанавливаем паузу для поиска сабвуфера s_delay_sch
    data[0] = power_pref + power;                        // Присваиваем значение мощности несущей команде
    radio.begin();                                       // Инициируем работу nRF24L01+
    radio.setChannel(f_Channel);                         // Указываем канал передачи данных (от 0 до 127), 5 - значит передача данных осуществляется на частоте 2,405 ГГц (на одном канале может быть только 1 приёмник и до 6 передатчиков)
    radio.setDataRate     (RF24_250KBPS);                // Указываем скорость передачи данных (RF24_250KBPS, RF24_1MBPS, RF24_2MBPS), RF24_1MBPS - 1Мбит/сек
    radio.setPALevel      (power);                       // Указываем мощность передатчика (RF24_PA_MIN=-18dBm, RF24_PA_LOW=-12dBm, RF24_PA_HIGH=-6dBm, RF24_PA_MAX=0dBm)
    radio.openWritingPipe (pipe_id);                     // Открываем трубу с идентификатором 0x1234567890 для передачи данных (на одном канале может быть открыто до 6 разных труб, которые должны отличаться только последним байтом идентификатора)
    Serial.begin(9600);                                  // Меню настройки
    }    

void sub_parcer (){
  // Читаем данные с терминала
  if (Serial.available()) {
      String new_set = Serial.readString();              // Прием данных из строки терминала
      new_set.trim();
    if (isDigit(new_set.charAt(1)))
    {
        int num = new_set.toInt(); 
        if (!ch_update(num)) {
           Serial.println("Команда не доступна");
           Serial.println();
        }
    }
    else
    {
      if (new_set == "help" || new_set == "?") {
         help_menu ();
         return;
         }
      if (!power_rate (new_set)) {
         Serial.println("Команда не доступна");
         Serial.println();
      }
    }
    help_menu ();
  }
}

bool sub_radio () {
// Отправляет управляющий сигнал на сабвуфер.
// Возвращает true если сигнал принят сабвуфером.
  sub_parcer();                                          // Читаем данные с терминала
  if (radio.write(&data, sizeof(data))) {                // отправляем данные из массива data указывая сколько байт массива мы хотим отправить. Отправить данные можно с проверкой их доставки: if( radio.write(&data, sizeof(data)) ){данные приняты приёмником;}else{данные не приняты приёмником;}
    if (!s_connected or s_error) {                       // Если соединение восстановлено, сбрасываем флаг ошибки, устанавливаем флаг соединения.
//      Serial.println("Соединение установлено");        // ОТЛАДКА---- 
       s_connected = true;
       s_error = false;
       signal_delay = s_delay_con; 
    }
    return true; 
  }
  else {
    return false;
  }
}

void sub_error () {
// Обрабатываем ошибки соединения
  if (!s_error) {
      Serial.println("ERROR");                            // ОТЛАДКА----
      err_timer = millis(); 
      signal_delay = s_delay_err;
      s_error = true;
      if (s_connected) {
         ++num_errors;
      }
   }
   else if (s_connected and millis() > err_timer + time_to_stanby) {
//      Serial.println("Stand by");                      // ОТЛАДКА----
      signal_delay = s_delay_sch;
      s_connected = false;
  }  
}

void loop(){
//    Serial.println(data[0]);                           // ОТЛАДКА---- 
    if (!sub_radio ()) {
      sub_error ();
    }                         
    delay (signal_delay);
}

bool power_rate (String powLevel) {
  int new_power = power;
  int set_auto_power = auto_power;
   if (powLevel == "min") {
      new_power = 0;
   }
   else if (powLevel == "low") {
      new_power = 1;
   }
   else if (powLevel == "high") {
      new_power = 2;
   }
    else if (powLevel == "max") {
      new_power = 3;
   }
   else if (powLevel == "on") {
      set_auto_power = 1;
   }
   else if (powLevel == "off") {
      set_auto_power = 0;
   }
  
  if (new_power != power) {
    power = new_power;  
    data[0] = power_pref + power;
    EEPROM.update(5, power);
    return true;
  }
  else if (set_auto_power != auto_power) {
    auto_power = set_auto_power;
    EEPROM.update(4, auto_power);
    return true;
  }
  else {
    return false;
  }
}

bool ch_update(int new_Channel) {
//смена канала связи
  if (!s_connected) {
    Serial.println("Смена канала возможна после установки соединения");
    return false; 
  }
  if (128 > new_Channel > 0) {
   data[0] = 30000 + new_Channel;
   radio.write(&data, sizeof(data));
   radio.setChannel(new_Channel);  
   if (manualAck()) {  
     EEPROM.update(10, new_Channel);
     f_Channel = new_Channel;
   }
   else {
     radio.setChannel(f_Channel);
   }
   data[0] = power_pref + power;
//   Serial.println(radio.getChannel());                    // ОТЛАДКА----
   return true;
  }
  else {
    return false;
  }
}

bool manualAck() {
// Переключаемся в режим приемника, принимаем подтверждение с саба о приеме настроек.
// Если флешка приняла данные, возвращаем True.
  radio.openReadingPipe (pipe_no, pipe_id);
  radio.startListening  ();                                 // Включаем приемник
  bool Ack_flag = false;
  uint32_t ack_timer = millis();                            // Засекаем время
  while (millis() - ack_timer < ack_timeout) {              // Ждем данные в течении ack_timeout
    if(radio.available()){                                  // Если в буфере имеются принятые данные
       radio.read(&data, sizeof(data));                     // Читаем данные в массив data и указываем сколько байт читать
//       Serial.println("данные приняты приёмником");       // ОТЛАДКА----
       if (data[0] == s_confirmed) {
          Ack_flag = true;
          data[0] = power_pref + power;
          break; 
       }  
    }
  }
  radio.stopListening  ();                                  // Выключаем приемник, 
  radio.closeReadingPipe(1);
  radio.openWritingPipe (pipe_id);
  delay(s_delay_ask);                                       // Пауза для смены режима всеми устройствами
//  Serial.print(Ack_flag);                                 // ОТЛАДКА----
//  Serial.println(" - принято");                           // ОТЛАДКА----
  return Ack_flag;                                    
}

void help_menu () {
  Serial.print ("Соединение: ");
   switch (s_connected) {
    case 0:
     Serial.println("отсутствует");
    break;
    case 1:
     Serial.println("подключено");
    break;
   }
  Serial.print ("Ошибки подключения: "); 
  Serial.println(num_errors);
  Serial.println(); 
  Serial.println ("Текущие настройки:");
  Serial.print ("Канал: ");
  Serial.println (f_Channel);
  Serial.print ("Мощность: ");
  switch (power) {
    case 0:
     Serial.println("RF24_PA_MIN");
    break;
    case 1:
     Serial.println("RF24_PA_LOW");
    break;
    case 2:
     Serial.println("RF24_PA_HIGH");
    break;  
    case 3:
     Serial.println("RF24_PA_MAX");
    break;
  }
  Serial.print ("Автоподстройка мощности: ");
  switch (auto_power) {
    case 1:
     Serial.println("ON");
    break;
    case 0:
     Serial.println("OFF");
    break;  
  } 
 Serial.println(); 
 Serial.println("Доступные команды"); 
 Serial.println("Смена канала связи: от 1 до 128");
 Serial.println("Вкл/выкл корректора мощности, при потере соединения: on/off");
 Serial.println("Мощность соединения флешки и колонки: min/low/high/max"); 
 Serial.println();                              
}
