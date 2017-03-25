#include <EEPROM.h>
#include <RTC.h>
#include <DHT12.h>
#include <TM1637Display.h>

const int CLK = 9; //Set the CLK pin connection to the display
const int DIO = 8; //Set the DIO pin connection to the display
TM1637Display display(CLK, DIO);  //set up the 4-Digit Display.
DHT12 dht12; // start DHT sensor
int  TM1637DisplayColon = false;
int  semisec = true;
bool isTimeSet = false;

int ldr = 0; // фоторезистор датчик
int mode = 0; // режим
int modemax = 15;
//int modeSetTime

int circle = 0; // такт
float TEMP = 0; // температура
float Humidity = 0; // влажность
boolean TEMP_ENABLED = true;
boolean TEMP_IS_DHT = false;
// config byte #0
byte cb0;
bool asOn = true; // 0.1
bool dbgOn = true;      // 0.0
bool hsOn = true;      // 0.2
bool tsOn = true;      // 0.3 show temp in C

const uint8_t PIN_button_SET  = 7; // кнопка SET
const uint8_t PIN_button_UP   = 6; // кнопка UP
const uint8_t PIN_button_DOWN = 5; // кнопка DOWN

#define SERIESRESISTOR 10000 // значение 'другого' резистора
#define THERMISTORPIN A1 // к какому пину подключается термистор
#define THERMISTORNOMINAL 10000 // сопротивление при 25 градусах по Цельсию
#define TEMPERATURENOMINAL 25 // temp. для номинального сопротивления (практически всегда равна 25 C)
#define BCOEFFICIENT 3950 // бета коэффициент термистора (обычно 3000-4000)
#define NUMSAMPLES 10 // сколько показаний берется для определения среднего значения
int samples[NUMSAMPLES];

RTC    time;
void setup() {
   delay(300);
   Serial.begin(9600);
   time.begin(RTC_DS3231);
   display.setBrightness(0x0f);
   // кнопки
   pinMode(PIN_button_SET,  INPUT_PULLUP);
   pinMode(PIN_button_UP,  INPUT_PULLUP);
   pinMode(PIN_button_DOWN,  INPUT_PULLUP);
   // термодатчик
   int termopin = analogRead(THERMISTORPIN);
   if(termopin > 1000 || termopin == 0) TEMP_ENABLED = false;
   // is_DHT
   if(dht12.readTemperature() != 0.01){ TEMP_IS_DHT=true; TEMP_ENABLED=true; }
   getConfig();
}

void getConfig(){
  EEPROM.get(0, cb0); // read configbyte_0
  dbgOn = bitRead(cb0, 0);
  asOn = bitRead(cb0, 1);
  hsOn = bitRead(cb0, 2);
  tsOn = bitRead(cb0, 3);
  Serial.print("configbyte_0 "); Serial.println(cb0);
  }
void putByteConfig(int a, byte b, int k, int v){ // http://arduino.ru/forum/programmirovanie/eeprom-rabota-s-bitami
  bitWrite(b, k, v);
  putConfig(a,b);
  //EEPROM.update(a,b);
  //getConfig();
  }
void putConfig(int a, byte b){
  EEPROM.update(a,b);
  getConfig();
  }

void loop(){
   
   // перехватываем команду на serial порт
    if (Serial.available()) { //поступила команда с временем http://zelectro.cc/RTC_DS1307_arduino
        cmdHandler(Serial.readStringUntil('\n'));
        isTimeSet = true; //дата была задана  
    }
  
  // меняем режим?
  if(circle>10 && asOn){
    if(mode >=0 && mode <= 2){
      mode ++;
      if(mode == 1 && (!TEMP_ENABLED || !tsOn)) mode++; // skip showtemp
      if(mode == 2 && (!TEMP_IS_DHT || !hsOn)) mode++; // skip showtemp
      if(mode > 2) mode=0; else circle++;
      }
    circle=0;
    }

  // в цикле через 100мс
  if(millis()%100==0) setBrightness();

  // в цикле через пол секунды
   if(millis()%500==0){
    switch(mode){
      //case 0: showCurrentTime(); break;
      case 0: time.gettime(); showTime(time.Hours,time.minutes,false,false); break;
      case 1: showThermistor(); break;
      case 2: showHumidity(); break;
      case 10: time.gettime(); showTime(time.Hours,time.minutes,true,false); break;
      case 11: time.gettime(); showTime(time.Hours,time.minutes,false,true); break;
      case 12: showDate(time.day,time.month,true,false); break;
      case 13: showDate(time.day,time.month,false,true); break;
      case 14: showFullYear(); break;
      case 15: showDbgState(); break;
      //default: showCurrentTime(); break;
      }
   if(TEMP_ENABLED && mode != 1){
    if(TEMP_IS_DHT) getDHT();
    else getThermistor();
    }
    
   semisec = !semisec;
    //Serial.print("mode=");Serial.println(mode);
    //Serial.print("circle=");Serial.println(circle);
    //erial.println(time.gettime("Y-m-d H:i:s (D)"));
    circle++;
    }
  
  buttonEventHundler();

}

void setBrightness(){
  // brightness
  int phr = analogRead(ldr);
  int br = phr / 128; if(!br) br=1;
  uint8_t br_s = (uint8_t) (br+7);
  display.setBrightness(br_s);
  }

/*void showCurrentTime(){
      time.gettime();
      int CurTimeHour = time.Hours;
      int segtoHourDec = CurTimeHour / 10;
      int segtoHour = CurTimeHour % 10;
      uint8_t segto = 0x00;
      uint8_t segtoDec = 0x00;
      if(mode != 10 || mode == 10 && semisec){ // set hour blink
        segto = (TM1637DisplayColon) ? (0x80 | display.encodeDigit(segtoHour)) : display.encodeDigit(segtoHour);
        segtoDec = (segtoHourDec) ? display.encodeDigit(segtoHourDec) : 0x00;
       }
      display.setSegments(&segtoDec,1,0);
      display.setSegments(&segto,   1,1);
      display.showNumberDec(time.minutes,true,2,2);
      if(mode == 11 && semisec){
        uint8_t emptydig = 0x00;
        //display.setSegments(emptydig, 1, 2);
        display.setSegments(&emptydig, 1, 2);
        display.setSegments(&emptydig, 1, 3);
        }
      //Serial.println(time.gettime("H:i:s"));
      TM1637DisplayColon = !TM1637DisplayColon;
  }*/

void showTime(int hours, int minutes, boolean blink1, boolean blink2){
      //time.gettime();
      int CurTimeHour = hours;
      int segtoHourDec = CurTimeHour / 10;
      int segtoHour = CurTimeHour % 10;
      uint8_t segto = 0x00;
      uint8_t segtoDec = 0x00;
      if(!blink1 || blink1 && semisec){ // set hour blink
        segto = (TM1637DisplayColon) ? (0x80 | display.encodeDigit(segtoHour)) : display.encodeDigit(segtoHour);
        segtoDec = (segtoHourDec) ? display.encodeDigit(segtoHourDec) : 0x00;
       }
      display.setSegments(&segtoDec,1,0);
      display.setSegments(&segto,   1,1);
      display.showNumberDec(minutes,true,2,2);
      if(mode == 11 && semisec){
        uint8_t emptydig = 0x00;
        display.setSegments(&emptydig, 1, 2);
        display.setSegments(&emptydig, 1, 3);
        }
      //Serial.println(time.gettime("H:i:s"));
      TM1637DisplayColon = !TM1637DisplayColon;
  }


void showDate(int days, int months, bool blink1, bool blink2){
  display.showNumberDec(time.day,true,2,0);
  display.showNumberDec(time.month,true,2,2);
  uint8_t emptydig = 0x00;
  if(blink1 && semisec){display.setSegments(&emptydig, 2, 0);display.setSegments(&emptydig, 1, 1);}
  if(blink2 && semisec){display.setSegments(&emptydig, 1, 2);display.setSegments(&emptydig, 1, 3);}
  }

void showFullYear(){
  int y = time.year + 2000;
  display.showNumberDec(y,true,4,0);
  }

void showThermistor(){
  uint8_t tt = B01100011;
  uint8_t t0 = (TEMP >0) ? B01111000 : B01000000; // t or minus
  display.setSegments(&t0, 1, 0);
  //display.setSegments(0b00, 1, 1);
  //display.setSegments(0b00, 1, 2);
  display.setSegments(&tt, 1, 3);

  int tempDec = abs(round(TEMP));
  display.showNumberDec(tempDec,false,2,1);
  
  //Serial.println(display.encodeDigit(8));
  }

void showHumidity(){
  display.showNumberDec(Humidity,false,2,0);
  uint8_t t2 = B01100011;//display.encodeExtra(1); // grad
  uint8_t t3 = B01011100;//display.encodeExtra(3); // _grad
  display.setSegments(&t2, 1, 2);
  display.setSegments(&t3, 1, 3);
  }
  
void showDbgState(){
	uint8_t d0 = B01011110;display.setSegments(&d0, 1, 0); // d
	uint8_t d1 = B01111100;display.setSegments(&d1, 1, 1); // b
	uint8_t d2 = B01001000;display.setSegments(&d2, 1, 2); // =	
	uint8_t d3 = display.encodeDigit( (dbgOn) ? 1 : 0 );display.setSegments(&d3, 1, 3);
	}

void switchDbg(){
  dbgOn = !dbgOn; putByteConfig(0,cb0,0,dbgOn);//bitWrite(cb0, 0, dbgOn);
  }


void buttonEventHundler(){
  //int val = 
  if(digitalRead(PIN_button_SET) == LOW ){
    while(digitalRead(PIN_button_SET) == LOW ) delay(50);
    mode++;
    if(mode == 1 && !TEMP_ENABLED) mode++;
    if(mode == 2 && !TEMP_IS_DHT) mode++;
    if(mode > 2 && mode < 10) mode=10;
    if(mode > modemax) mode = 0;
    Serial.println("SET_button_SET");
    circle = 0;
    }

  if(digitalRead(PIN_button_UP) == LOW ){
    int st=5;
    if(st) while(digitalRead(PIN_button_UP) == LOW ){
      delay(50);
      st--;
    }
    Serial.println("PIN_button_UP");
    int val;
    switch (mode){
      case 0: switcher(B11110111,B11101101,asOn,0,cb0,1); /* switchAs(); */ /* switcher(uint8_t d0, uint8_t d1, bool &variable, int a, byte cv, int i); */ break;
      case 1: switcher(B01111000,B11101101,tsOn,0,cb0,3); /*switchTs();*/ break;
      case 2: switcher(B11110100,B11101101,hsOn,0,cb0,2); /*switchDH();*/ break;
      case 10: val = time.Hours+1;   if(val>23) val=0; time.settime(time.seconds,time.minutes,val); break; // hours blink
      case 11: val = time.minutes+1; if(val>59) val=0; time.settime(0,val,time.Hours); break; // minutes blink
      case 12: val = time.day+1;     if(val>31) val=1; time.settime(time.seconds,time.minutes,time.Hours,val); break;  // day blink
      case 13: val = time.month+1; if(val>12) val=0; time.settime(time.seconds,time.minutes,time.Hours,time.day,val); break;  // month blink
      case 14: time.settime(time.seconds,time.minutes,time.Hours,time.day,time.month,time.year+1); break;  // year blink
      case 15: switchDbg(); break;
      }
    circle = 0;
    }
  
  if(digitalRead(PIN_button_DOWN) == LOW ){
    while(digitalRead(PIN_button_DOWN) == LOW ) delay(50);
    Serial.println("PIN_button_DOWN");
    switch (mode){
      case 0: switcher(B11110111,B11101101,asOn,0,cb0,1); /* switchAs(); */ /* switcher(uint8_t d0, uint8_t d1, bool &variable, int a, byte cv, int i); */ break;
      case 1: switcher(B01111000,B11101101,tsOn,0,cb0,3); /*switchTs();*/ break;
      case 2: switcher(B11110100,B11101101,hsOn,0,cb0,2); /*switchDH();*/ break;
      case 10: time.settime(time.seconds,time.minutes,time.Hours-1);break;
      case 11: time.settime(0,time.minutes-1,time.Hours);break;
      case 12: time.settime(time.seconds,time.minutes,time.Hours,time.day-1); break;
      case 13: time.settime(time.seconds,time.minutes,time.Hours,time.day,time.month-1); break;
      case 14: time.settime(time.seconds,time.minutes,time.Hours,time.day,time.month,time.year-1); break;
      case 15: switchDbg(); break;
      }
    circle = 0;
    }
  
  }

/*void switchAs(){
  asOn = !asOn; putByteConfig(0,cb0,1,asOn);//bitWrite(cb0, 1, asOn); 
  uint8_t d0 = B11110111;display.setSegments(&d0, 1, 0);// a
  uint8_t d1 = B11101101;display.setSegments(&d1, 1, 1);// s
  uint8_t d2 = B01001000;display.setSegments(&d2, 1, 2);// =
  uint8_t d3 = display.encodeDigit( (asOn) ? 1 : 0 );display.setSegments(&d3, 1, 3);
  delay(200);
  }

void switchDH(){
  hsOn = !hsOn; putByteConfig(0,cb0,2,hsOn);
  uint8_t d0 = B11110100;display.setSegments(&d0, 1, 0);// h
  uint8_t d1 = B11101101;display.setSegments(&d1, 1, 1);// s
  uint8_t d2 = B01001000;display.setSegments(&d2, 1, 2);// =
  uint8_t d3 = display.encodeDigit( (hsOn) ? 1 : 0 );display.setSegments(&d3, 1, 3);
  delay(200);
}

void switchTs(){
  tsOn = !tsOn; putByteConfig(0,cb0,3,tsOn);
  uint8_t d0 = B01111000;display.setSegments(&d0, 1, 0);// t
  uint8_t d1 = B11101101;display.setSegments(&d1, 1, 1);// s
  uint8_t d2 = B01001000;display.setSegments(&d2, 1, 2);// =
  uint8_t d3 = display.encodeDigit( (tsOn) ? 1 : 0 );display.setSegments(&d3, 1, 3);
  delay(200);
  }*/

void switcher(uint8_t d0, uint8_t d1, bool &variable, int a, byte cv, int i){
  variable = !variable; putByteConfig(a,cv,i,variable); uint8_t d2 = B01001000;
  display.setSegments(&d0, 1, 0);display.setSegments(&d1, 1, 1);display.setSegments(&d2, 1, 2);
  uint8_t d3 = display.encodeDigit( (variable) ? 1 : 0 );display.setSegments(&d3, 1, 3);
  }

void getThermistor(){
  //Serial.print("raw a1 input");
  //Serial.println(analogRead(THERMISTORPIN));
  uint8_t i;
  float average;
  for (i=0; i< NUMSAMPLES; i++) {
    samples[i] = analogRead(THERMISTORPIN);
    delay(10);
    }
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
    average += samples[i];
    }
  average /= NUMSAMPLES;
  float reading;//
  reading = average;//analogRead(THERMISTORPIN);
  reading = (1023 / reading) - 1;
  reading = SERIESRESISTOR / reading;
  //Serial.print("Thermistor resistance ");
  //Serial.println(reading);
  float steinhart;
  steinhart = reading / THERMISTORNOMINAL; // (R/Ro)
  steinhart = log(steinhart); // ln(R/Ro)
  steinhart /= BCOEFFICIENT; // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart; // инвертируем
  steinhart -= 273.15; // конвертируем в градусы по Цельсию
  //Serial.print("Temperature ");
  //Serial.println(steinhart);
  TEMP = steinhart;
  //Serial.print("t=");
  //Serial.println(TEMP);
  }

void getDHT(){
  TEMP = dht12.readTemperature();
  Humidity = dht12.readHumidity();
  }

void cmdHandler(String timestr)
{
  Serial.print("> ");
  Serial.println(timestr);
  int sep = timestr.indexOf(" ");
  String cmd = timestr.substring(0,sep);
  String arg = timestr.substring(sep+1);
  if(cmd=="settime"){
    int hours = arg.substring(0, 2).toInt();
    int minutes = arg.substring(3, 5).toInt();
    int seconds = arg.substring(6, 8).toInt();
      if(hours && minutes){
        time.settime(seconds,minutes,hours);
        Serial.print("Time updated: ");
        Serial.println(time.gettime("H:i:s"));
        }
    }
  if(cmd=="setdate"){
    int d = arg.substring(0, 2).toInt();
    int m = arg.substring(3, 5).toInt();
    int y = arg.substring(6, 12).toInt();// - 1970
    Serial.println(y);
    if(d && m && y)
      time.settime(time.seconds,time.minutes,time.Hours,d,m,y);
      Serial.print("Date updated: ");
      Serial.println(time.gettime("Y-m-d"));
    }
}
