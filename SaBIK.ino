//Для LGT плат выбирать Nulllab AVR compatible boards (https://github.com/nulllaborg/arduino_nulllab)


// Библиотеки -----------------------
#include <SPI.h>                                          // Подключаем библиотеку  для работы с шиной SPI
#include <nRF24L01.h>                                     // Подключаем файл настроек из библиотеки RF24
#include <RF24.h>                                         // Подключаем библиотеку  для работы с nRF24L01+
#include <EEPROM.h>                                       // Чтение/запись в память

// Константы ------------------------
#define contact_read_delay 1500                            // пауза перед считывание пинов
#define time_to_stanby 9000                               // сек*1000, для перехода в режим ожидания
#define auto_mode 2                                       // пин автоматический режим усилителя
#define on_mode  3                                        // пин усилитель постоянно включен
#define mute_rel  4                                       // пин реле mute вкл/выкл.
#define power_rel 6                                       // пин реле усилителя вкл/выкл.
#define sleep_rel 5                                       // пин реле питания контроллера вкл/выкл.
#define delay_rel 500                                     // Пауза вкл/выкл. реле, сек*1000                                                             
#define delay_sleep 15000                                 // Пауза вкл/выкл. реле, сек*1000
#define ack_timeout 750                                   // Время выполнения manualAck()
#define s_recived 11112                                   // Код ответа, если принят сигнал с флешки
#define pipe_id 0x1234567890LL                            // Идентификатор трубы RF24
#define pipe_no 1                                         // Номер трубы RF24 (число от 0 до 5)


// Переменные -----------------------
volatile bool intD3Flag = true;
volatile bool intFlag = true;                             // флаг прерывания переключателя режимов  
volatile uint32_t but_timer = 0;                          // таймер с момента переключения режимов
RF24           radio(9, 10);                              // Создаём объект radio   для работы с библиотекой RF24, указывая номера выводов nRF24L01+ (CE, CSN)
int         data[1];                                      // Создаём массив для приёма данных
bool radio_mode = false;                                  // состояние приемника вкл/выкл.
bool power_mode = false;                                  // питание усилителя вкл/выкл
bool wake_up = false;                                     // флаг для пробуждения при обнаружении флешки
uint32_t standby_timer = 0;                               // время поступления сигнала от флешки
char PALevel = RF24_PA_LOW;                               // Начальная мощность сигнала на передатчике
int sub_Channel = EEPROM[10];                             // канал связи вуфера и флешки (от 0 до 127)

struct Parcer {
  //Структура функции Parcer
  int s_mode;
  int s_value;
};

Parcer parce(String temp_mode) {
  //Парсим команды с флешки. 1я цифра - режим (команда), остальные - значение.
    Parcer str;
    String temp_value = temp_mode;
    temp_mode.remove(1);
    temp_value.remove(0,1);
    str.s_mode = temp_mode.toInt();                       //Значение, которое возвращается через return
    str.s_value = temp_value.toInt();                     //Значение, которое возвращается через return
    return str;
}

void setup() {
//  EEPROM.update(10, 78);                                // Инициализируем начальный канал для соединения (адрес, канал)
  pinMode(mute_rel, OUTPUT);                              // Объявляем пин реле как выход
  pinMode(power_rel, OUTPUT);                             // Объявляем пин реле как выход
  pinMode(sleep_rel, OUTPUT);                             // Объявляем пин реле как выход
  radio.begin();                                          // Инициируем работу nRF24L01+
  radio.setChannel(sub_Channel);                          // Указываем канал приёма данных (от 0 до 127), 5 - значит приём данных осуществляется на частоте 2,405 ГГц (на одном канале может быть только 1 приёмник и до 6 передатчиков)      
  radio.setDataRate     (RF24_250KBPS);                   // Указываем скорость передачи данных (RF24_250KBPS, RF24_1MBPS, RF24_2MBPS), RF24_1MBPS - 1Мбит/сек
  radio.setPALevel      (PALevel);                        // Указываем мощность передатчика (RF24_PA_MIN=-18dBm, RF24_PA_LOW=-12dBm, RF24_PA_HIGH=-6dBm, RF24_PA_MAX=0dBm)
  radio.openReadingPipe (pipe_no, pipe_id);               // Открываем 1 трубу с идентификатором 0x1234567890 для приема данных (на ожном канале может быть открыто до 6 разных труб, которые должны отличаться только последним байтом идентификатора)
  radio.startListening  ();                               // Включаем приемник, начинаем прослушивать открытую трубу
  pinMode(auto_mode, INPUT_PULLUP);
  pinMode(on_mode, INPUT_PULLUP);
  attachInterrupt(0, isr, CHANGE);                        // Включаем прослушивание прерываний на D2
  attachInterrupt(1, isr, CHANGE);                        // Включаем прослушивание прерываний на D3
  digitalWrite(sleep_rel, HIGH);                          // Включаем реле контроллера
//  Serial.begin(9600);                                   // ОТЛАДКА----   
//  Serial.println("WAKE UP");                            // ОТЛАДКА----
}

void isr() {
//Обработка прерывания при переключение трехпозиционного тумблера (режим колонки)
    intFlag = true;                                       // подняли флаг прерывания
    but_timer = millis(); 
}

void sw_sleep () {
    digitalWrite(mute_rel, HIGH);
    digitalWrite(sleep_rel, LOW);
//    Serial.println("SLEEP");                            // ОТЛАДКА----
}

void sw_on () {
  if (not power_mode) {
  power_mode = true;
  digitalWrite(mute_rel, HIGH);
  delay (delay_rel);
  digitalWrite(power_rel, HIGH);
  delay (delay_rel);
  digitalWrite(mute_rel, LOW);
//  Serial.println("ON");                                  // ОТЛАДКА---- 
  }
}

void sw_off () {
  if (power_mode) {                                       
    digitalWrite(mute_rel, HIGH);
    delay (delay_rel);
    digitalWrite(power_rel, LOW);
//    Serial.println("OFF");                               // ОТЛАДКА----
    delay (delay_sleep);
    digitalWrite(mute_rel, LOW);
    power_mode = false;
  }
}

void sw_stad_by () {
  if (radio_mode) {
    if (power_mode and not wake_up) {
      sw_off ();
//      Serial.println("Stand By");                        // ОТЛАДКА----
    }
    else if (not power_mode and wake_up) {
      sw_on ();
    }
  }
}

void reciever () {
// Читаем поступающий сигнал с флешки. Определяем номер запроса флешки.
  if(radio.available() and radio_mode){                   // Если в буфере имеются принятые данные
        radio.read(&data, sizeof(data));                  // Читаем данные в массив data и указываем сколько байт читать
//        Serial.println(data[0]);                        // ОТЛАДКА----        
        String str_data = String(data[0]);                // Конвертируем входной сигнал int в string
        Parcer str;                                       // Получаем значения из функции Parcaer: str.s_mode, str.s_value
        str = parce(str_data);
        switch (str.s_mode) {                             
          case 1:
          //меняем мощность
            power_rate_trigger (str.s_value);
            break;
          case 3:
          //меняем канал
            ch_update (str.s_value);
            manualAck();
            break;  
        }
        standby_timer = millis ();
        wake_up = true;
    }
   else if (wake_up and millis() - standby_timer > time_to_stanby) {
    wake_up = false;
   }
}

bool manualAck() {
// Переключаемся в режим передатчика, отправляем данные на флешку с новыми настроками.
// Если флешка приняла данные, возвращаем True.
  radio.stopListening  ();                                 //Выключаем приемник
  radio.closeReadingPipe(1);
  radio.openWritingPipe (pipe_id);
  data[0] = s_recived;
  bool Ack_flag = false;
  uint32_t ack_timer = millis();
  while (millis() - ack_timer < ack_timeout) {
    if(radio.write(&data, sizeof(data))){
    //данные приняты приёмником;
      Ack_flag = true;
//      Serial.println("данные приняты приёмником");       //ОТЛАДКА----
      break;
    }
  }
  radio.openReadingPipe (pipe_no, pipe_id);
  radio.startListening  ();                                //Включаем приемник, начинаем прослушивать открытую трубу 
//  Serial.print(Ack_flag);                                //ОТЛАДКА----
  return Ack_flag;                                    
}

void ch_update(int new_Channel) {
//смена канала связи
  radio.setChannel(new_Channel);
  if (manualAck()) {
  EEPROM.update(10, new_Channel);
  sub_Channel = new_Channel;
  }
  else {
  radio.setChannel(sub_Channel);
  }
}

void power_rate_trigger (int power) {
//Меняем можность канала по указанию с флешки
  if (power < 5){
    if (PALevel != power) {
      PALevel = power;
      radio.setPALevel (PALevel);
    }
  }
}

void sw_mode () { 
// Переключаем режимы работы колонки вкл/выкл/авто                         
  if (intFlag and millis()- but_timer > contact_read_delay) {
    int D2Pin = digitalRead(auto_mode);
    int D3Pin = digitalRead(on_mode);
    intFlag = false;                                      // сбрасываем флаг прерывания
    if (D2Pin == 0) {
//      Serial.println("D2");                             //ОТЛАДКА----
      radio.startListening  ();
      radio_mode = true;
      }
    else if (D3Pin == 0) {
//      Serial.println("D3");                             //ОТЛАДКА----
      sw_on ();
      radio.stopListening  ();
      radio_mode = false;
    }
    else {
        radio.stopListening  ();
        radio_mode = false;
        sw_off();
        sw_sleep ();   
    }
  }
}

void loop() {
  sw_mode ();
  reciever ();     
  sw_stad_by ();
}
