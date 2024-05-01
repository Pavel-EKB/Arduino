const int buttonPin = 13;       // the number of the pushbutton pin
int buttonState = 0;            // variable for reading the pushbutton status
bool stop_flag = 0;

#define obMin  70               //ввести минимальные обороты 1 скорости
#define obMax  300              //ввести максимальные обороты 1 скорости-
#define obMin1  100             //ввести минимальные обороты 2 скорости
#define obMax1  600             //ввести максимальные обороты 2 скорости
#define kImp  90                //ввести кол-во импульсов на 10 оборотов
#define minzn  125              //минимальное значение симмистора на котором начинается вращение.
#define ogrmin  75              //ограничение симистора на минимальных оборотах.
#define mindimming  100         //значение симистора при закллинившем станке (первоначальный импульс)
#define dopuskmin   230         //допуск на минимальных оборотах в минус и плюс
#define dopuskmax   180         //допуск на максимальных оборотах в минус и плюс
#define razgon  20              //переменная разгона 1 - 100
#define zaschita  30            //количество оборотов на которое можно превысить. затем срабатывает защита.
#define AC_LOAD  3              //пин управления симистором
volatile int dimming = 130;     // время задержки от нуля   7 = максимально, 130 = минимально
volatile unsigned long time;    // время в микросекундах срабатывания датчика нуля
unsigned long tims;             // переменная показаний времени
unsigned long loopTime;         //временные переменные для таймера экрана
int dopusk ;                    //допуск оборотов в минус и плюс
int holl = 0;                   //переменная  срабатываня датчика
int pR;                         //показания регулятора
int pRR;                        //переменная для расчёта.
int ogr;                        //переменная ограничений симистора на текущих оборотах
volatile int sp = 0;            //переменная суммы срабатываний датчика
volatile int prOb;              //предвар реальн обороты
volatile int rOb;               //реальные обороты
volatile uint16_t  int_tic;     //переменные для подсчёта времени между импульсами.
volatile uint32_t  tic;
volatile int t = 0;             //минимальное время импульсов *2
int val ;
int tumb ;                      //переменная положение тумблера
int oMax ;
int tempInt;                    //0 - 1023
int tempMapInt;                 //0 - 80
int temp1;                      //0 - 16
int temp2;                      //0 - 5
int var;

void setup()
{
  Serial.begin(9600);
  pRR = obMin;
  t = (15000 / ( obMin * (kImp / 10))) * 2;                               //высчитываем минимальное время импульсов *2
  pinMode(AC_LOAD, OUTPUT);                                               //назначаем выходом пин управления симистором #3
  attachInterrupt(0, zero_crosss_int, RISING);                            //прерывание по пину 2 - переход через 0
  pinMode (8, INPUT);                                                     //вход сигнала ICP( №8 only для atmega328) - Датчик хола

//настройка 16 бит таймера-счётчика 1
  TCCR1B = 0; TCCR1A = 0; TCNT1 = 0;
  TIMSK1 = (1 << ICIE1) | (1 << TOIE1);                                   //создавать прерывание от сигнала на пине ICP1
  TCCR1B = (1 << ICNC1) | (1 << ICES1) | (1 << CS10);                     //div 1
}

ISR (TIMER1_CAPT_vect)                                                    //прерывание захвата сигнала на входе ICP1
{ 
  TCNT1 = 0;
  if (TIFR1 & (1 << TOV1))
  {
    TIFR1 |= 1 << TOV1;
    if (ICR1 < 100)
    {
      int_tic++;
    }
  }
  tic = ((uint32_t)int_tic << 16) | ICR1 ;                                //подсчёт тиков
  int_tic = 0;
  sp = sp + 1 ;                                                           //для подсчёта оборотов в минуту.
  holl = holl + 1;
}                                                                         //после каждого срабатывания датчика холл+1


ISR (TIMER1_OVF_vect)                                                     //прерывание для счёта по переполнению uint
{ 
  int_tic++;                                                              //считать переполнения через 65536 тактов
  if (int_tic > t)
  {
    tic = 0;                                                              //если на входе пусто более минимального времени то обнулить счётчики
    int_tic = 0;
  }
}

// the interrupt function must take no parameters and return nothing
void zero_crosss_int()                                                    //function to be fired at the zero crossing to dim the light
{
  time = micros();
}

void loop()  {
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH)
    {

      val = analogRead(A0);
      tumb = analogRead(A6);
      if (tumb < 500)
      {
        pR = map(val, 0, 1023, obMin, obMax);                             //Приводим показания регулятора к минимальным и максимальным оборотам
        oMax = obMax;
      }
      else
      {
        pR = map(val, 0, 1023, obMin1, obMax1);                           //Приводим показания регулятора к минимальным и максимальным оборотам второй скорости
        oMax = obMax1;
      }
      dopusk = map(pR, obMin, obMax1, dopuskmin, dopuskmax);
    
    if (val > 0 ) {                                                       //если регулятор больше 0 и нажата педаль (нужно дописать логику)
      if ( holl >= 1) {                                                   //если сработал датчик
        prOb = 60000000 / ((tic * 0.0625 ) * kImp / 10);                  //Высчитываем обороты по показаниям датчика
        if ( prOb >= 0) {                                                 //проверяем на соответствие.
          rOb = prOb ;                                                    //если нормально, записываем в реальные обороты

        }
        if ( rOb > (pR + zaschita) ) {                        
          //dimming = 130;                                                //то время задержки 130
          pRR = obMin;
     
//отключаем реле
//не стирать          ctop = false;
//          rew = false;
//          digitalWrite(r1, rew);     //переключаем реле1
//          digitalWrite(r2, rew);     // переключаем реле2

          
        }
        else {


          if ( rOb < pR ) {                                               //сверяем показания регулятора и реальные обороты
            int fff = pR  - rOb;                                          //узнаём разницу между оборотами
            int pRu = map(fff, 1, obMax, 1, razgon);                      //исходя из разницы и разгона высчитываем на сколько увеличить переменную для расчёта
            pRR = pRR + pRu  ;                                            //увеличиваем переменную расчёта
          }
          if ( pR < (rOb - 20) ) {                                        //сверяем показания регулятора и реальные обороты
            int fff = rOb - 20  - pR;                                     //узнаём разницу между оборотами
            int pRu = map(fff, 1, oMax, 1, razgon);                       //исходя из разницы и разгона высчитываем на сколько уменьшить переменную для расчёта
            pRR = pRR - pRu ;                                             //увеличиваем переменную расчёта
          }
          pRR = constrain(pRR, (pR / 10), oMax);                          //задаём пределы переменной для расчёта.
          ogr = map(val, 0, 1023, ogrmin, 7);                             //исходя из показаний регулятора узнаём на сколько может быть открыт симистор.
          int mingran = pR - dopusk;
          if (mingran < 0)mingran = 0;

          dimming = map(rOb, (pRR - dopusk), (pRR + dopusk), ogr, minzn); //рассчитываем управление симистором.
          holl = 0;                                                       //обнуляем срабатывание датчика
//не стирать          ctop = true;                                        //разрешаем включиться реле
        }
      }
      if (tic == 0) {                                                     //если двигатель не вращается
        dimming = mindimming ;                                            //время задержки равно первоначальному импульсу
//не стирать        ctop = true;                                          //разрешаем включиться реле
      }
      dimming = constrain(dimming, ogr, minzn) ;                          //Следим чтоб время задержки было не меньше ограничения и не больше минимального значения
    }
    else {
      dimming = 130;                                                      //Если регулятор на 0 то время задержки 130
      pRR = obMin;
      //отключаем реле
//не стирать      ctop = false;
//      rew = false;
//      digitalWrite(r1, rew);                                            //переключаем реле1
//не стирать      digitalWrite(r2, rew);                                  //переключаем реле2
    }


    int dimtime = (75 * dimming);                                         //For 60Hz =>65
    tims = micros();                                                      //считываем время, прошедшее с момента запуска программы
    if (tims >= (time + dimtime)) {                                       //если время больше или равно времени срабатывания нуля + время задержки

      digitalWrite(AC_LOAD, HIGH);                                        //открываем симистор
      delayMicroseconds(10);                                              //задержка 10 микросекунд (для 60Hz = 8.33)
      digitalWrite(AC_LOAD, LOW);                                         //выключаем сигнал на симистор.
    }
    else {}

  }
}
