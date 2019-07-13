// CW Beacon for CW transceiver Super RM or RockMite 51 or Super Octopus by Igor Kutko, UA3YMC, 27 Jun 2019
// V.02
// ATTinyCore by Spence Konde
// http://drazzy.com/package_drazzy.com_index.json
// ATTiny4313
// Fuses (E:FF, H:DF, L:EF)
// avrdude -p t4313 -P /dev/ttyUSB0 -c avrisp -b 19200 -U lfuse:w:0xEF:m  -U hfuse:w:0xDF:m  -U efuse:w:0xFF:m
// Beacon message in EEPROM, ASCII code, uppercase. Value FF(255) ignored.
// avrdude -p t4313 -P /dev/ttyUSB0 -c avrisp -b 19200 -U eeprom:w:message.hex

#define MESS_MAX_SIZE 71                                                    //Максипальный размер принимаемой строки по UART
#define F_CPU 1105920UL
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <EEPROM.h>

byte freq[8] = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};            //Частоты тона
unsigned int c_speed = 16;                                                  //Скорость передачи 12 - 22
unsigned int n_freq = 5;                                                    //Номер тона по умолчанию
char mess[MESS_MAX_SIZE];
volatile boolean flag = 0;                                                  //Флаг передачи
unsigned count = 0;
unsigned int mess_size = 0;                                                 // текущий размер строки
boolean mess_complete = false;                                              // строка в буфере получена полностью

void setup() {

  TCCR0B = 0x05;                              // set prescaler clk/768
  TCCR0B |= (1 << WGM02);
  TCCR0A |= ((0 << COM0A1) | (1 << COM0A0));  // set PWM mode

  attachInterrupt(0, up_tone, LOW);           // Настройка прерываний INT0,1
  attachInterrupt(1, down_tone, LOW);         //

  pinMode(12, INPUT);                         //key dot
  pinMode(13, INPUT);                         //key dash

  DDRD = (1 << PD5);                          // TX
  PORTD = (1 << PD5);                         //

  GIMSK |= (1 << PCIE0);                                    // Настройка прерываний PCINT5,6,7
  PCMSK |= (1 << PCINT5) | (1 << PCINT6) | (1 << PCINT7);   //

  pause(500);
  Serial.begin(115200);
}

void loop() {
  if (digitalRead(12) == LOW) {
    dot(freq[n_freq]);
  }
  if (digitalRead(13) == LOW) {
    dash(freq[n_freq]);
  }
  if ( flag == 1 ) {
    MemMsg(freq[n_freq]);
  }

  if ( mess_complete ) {
    Serial.println(mess);
    SendMsg(mess, freq[n_freq]);
    Serial.println("OK");
    clean_mess();
  }
  readSerial();
}

void readSerial() {
  while (Serial.available()) {                  //пока в порту есть символы
    char ch = char(toupper(Serial.read()));     // читаем один из порта
    switch (ch) {
      case '\n':                                //перевод строки игнорируем
        break;
      case '\r':                                //возврат каретки - окончание строки
        mess[mess_size] = 0;                    // добавляем ноль в конец строки (признак её окончания)
        mess_complete = true;                   // признак получения полной строки для гл. цикла
        break;
      default:
        mess[mess_size] = ch;                   // если символ - не конец строки - добавляем символ в буфер
        mess_size++;
        break;
    }
    if (mess_size == MESS_MAX_SIZE - 1) {       //если строки размер - максимальный
      mess[mess_size] = 0;                      // добавляем ноль в конец строки (признак её окончания)
      mess_complete = true;                     // признак получения полной строки для гл. цикла
    }
  }
}

void up_tone() {                  // Обработка прерывания INT0
  beep();
  if ( c_speed != 12 ) {
    c_speed--;
  }
  pause(100);
}

void down_tone() {                // Обработка прерывания INT1
  beep();
  if ( c_speed != 25 ) {
    c_speed++;
  }
  pause(100);
}

ISR(PCINT0_vect) {                // Обработка прерывания PCINT5,6,7
  if (!(PINB & (1 << PB5))) {
    beep();
    flag = !flag;
    pause(100);
  }

  if (!(PINB & (1 << PB6))) {
    beep();
    if ( n_freq != 7 ) {
      n_freq++;
    }
    pause(100);
  }

  if (!(PINB & (1 << PB7))) {
    beep();
    if ( n_freq != 0 ) {
      n_freq--;
    }
    pause(100);
  }
}

void dot(byte a) {
  action(1, a);
}

void dash(byte a) {
  action(3, a);
}

void action(int x, byte a) {
  PORTD = B00000000;
  DDRB = (1 << PB2);
  OCR0A = a;
  pause(c_speed * x);
  PORTD = B00100000;
  DDRB = (0 << PB2);
  pause(c_speed);
}

void pause(unsigned int n) {
  for (unsigned int c = 1; c <= n; c++) {
    _delay_ms(32);
  }
}

void beep() {
  DDRB = (1 << PB2);
  OCR0A = 0x02;
  pause(5);
  OCR0A = 0x01;
  pause(5);
  OCR0A = 0x02;
  pause(5);
  DDRB = (0 << PB2);
}

void MemMsg(byte f) {
  unsigned int i;
  char ch[0];
  for (i = 0; i < EEPROM.length(); i++)
  {
    if ( flag == 1 ) {
      ch[0] = EEPROM[i];
      if ( ch == char(255)) {
        return;
      }
      CodeMorse(ch, 0, f);
    }
    else {
      return;
    }
    pause(c_speed * 3);                         //Пауза между словами
  }
  pause(c_speed * 3 * 300);                     //Пауза между повторами
  Counter();
}

void Counter()                                  //Счетчик повторов
{
  Serial.print("#");
  Serial.println(count);
  if ( count == 65534 ) {
    count = 0;
  }
  count++;
}

void SendMsg(char *str, byte f)
{
  unsigned int i;
  for (i = 0; i < strlen(str); i++)
  {
    if ( mess_complete ) {
      CodeMorse(str, i, f);
    }
    else {
      return;
    }
    pause(c_speed * 3);
  }
}

void clean_mess () {                        // очистка буфера порта
  memset(mess, 0, sizeof mess);
  mess_size = 0;
  mess_complete = false;
}

void CodeMorse(char *str, unsigned int i, byte f)
{
  switch (str[i])
  {
    case 'A':
      dot(f); dash(f);  break;
    case 'B':
      dash(f); dot(f); dot(f); dot(f);  break;
    case 'C':
      dash(f); dot(f); dash(f); dot(f);  break;
    case 'D':
      dash(f); dot(f); dot(f);  break;
    case 'E':
      dot(f);  break;
    case 'F':
      dot(f); dot(f); dash(f); dot(f);  break;
    case 'G':
      dash(f); dash(f); dot(f);  break;
    case 'H':
      dot(f); dot(f); dot(f); dot(f);  break;
    case 'I':
      dot(f); dot(f);  break;
    case 'J':
      dot(f); dash(f); dash(f); dash(f);  break;
    case 'K':
      dash(f); dot(f); dash(f);  break;
    case 'L':
      dot(f); dash(f); dot(f); dot(f);  break;
    case 'M':
      dash(f); dash(f);  break;
    case 'N':
      dash(f); dot(f);  break;
    case 'O':
      dash(f); dash(f); dash(f);  break;
    case 'P':
      dot(f); dash(f); dash(f); dot(f);  break;
    case 'Q':
      dash(f); dash(f); dot(f); dash(f);  break;
    case 'R':
      dot(f); dash(f); dot(f);  break;
    case 'S':
      dot(f); dot(f); dot(f);  break;
    case 'T':
      dash(f);  break;
    case 'U':
      dot(f); dot(f); dash(f);  break;
    case 'V':
      dot(f); dot(f); dot(f); dash(f);  break;
    case 'W':
      dot(f); dash(f); dash(f);  break;
    case 'X':
      dash(f); dot(f); dot(f); dash(f);  break;
    case 'Y':
      dash(f); dot(f); dash(f); dash(f);  break;
    case 'Z':
      dash(f); dash(f); dot(f); dot(f);  break;
    case ' ':
      pause(c_speed * 7);  break;
    case '.':
      dot(f); dash(f); dot(f); dash(f); dot(f); dash(f);  break;
    case ',':
      dash(f); dash(f); dot(f); dot(f); dash(f); dash(f);  break;
    case ':':
      dash(f); dash(f); dash(f); dot(f); dot(f);  break;
    case '?':
      dot(f); dot(f); dash(f); dash(f); dot(f); dot(f);  break;
    case '\'':
      dot(f); dash(f); dash(f); dash(f); dash(f); dot(f);  break;
    case '-':
      dash(f); dot(f); dot(f); dot(f); dot(f); dash(f);  break;
    case '/':
      dash(f); dot(f); dot(f); dash(f); dot(f);  break;
    case '(':
      dash(f); dot(f); dash(f); dash(f); dot(f);  break;
    case ')':
      dash(f); dot(f); dash(f); dash(f); dot(f); dash(f);  break;
    case '\"':
      dot(f); dash(f); dot(f); dot(f); dash(f); dot(f);  break;
    case '@':
      dot(f); dash(f); dash(f); dot(f); dash(f); dot(f);  break;
    case '=':
      dash(f); dot(f); dot(f); dot(f); dash(f);  break;
    case '0':
      dash(f); dash(f); dash(f); dash(f); dash(f);  break;
    case '1':
      dot(f); dash(f); dash(f); dash(f); dash(f);  break;
    case '2':
      dot(f); dot(f); dash(f); dash(f); dash(f);  break;
    case '3':
      dot(f); dot(f); dot(f); dash(f); dash(f);  break;
    case '4':
      dot(f); dot(f); dot(f); dot(f); dash(f);  break;
    case '5':
      dot(f); dot(f); dot(f); dot(f); dot(f);  break;
    case '6':
      dash(f); dot(f); dot(f); dot(f); dot(f);  break;
    case '7':
      dash(f); dash(f); dot(f); dot(f); dot(f);  break;
    case '8':
      dash(f); dash(f); dash(f); dot(f); dot(f);  break;
    case '9':
      dash(f); dash(f); dash(f); dash(f); dot(f);  break;
  }
}
