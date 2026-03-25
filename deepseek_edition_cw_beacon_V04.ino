// CW Beacon for CW transceiver Super RM or RockMite 51 or Super Octopus, 25 March 2026
// Modified and fixed version
// V.04
// ATTinyCore by Spence Konde
// http://drazzy.com/package_drazzy.com_index.json
// ATTiny4313
// Fuses (E:FF, H:DF, L:EF)
// avrdude -p t4313 -P /dev/ttyUSB0 -c avrisp -b 19200 -U lfuse:w:0xEF:m  -U hfuse:w:0xDF:m  -U efuse:w:0xFF:m
// Beacon message in EEPROM, ASCII code, uppercase. Value FF(255) ignored.
// avrdude -p t4313 -P /dev/ttyUSB0 -c avrisp -b 19200 -U eeprom:w:message.hex

#define MESS_MAX_SIZE 67                        // Maximum message size over UART
#define F_CPU 1105920UL
#define MIN_SPEED 12                            // Minimum CW speed
#define MAX_SPEED 25                            // Maximum CW speed
#define DEFAULT_SPEED 16                        // Default CW speed
#define MIN_TONE_INDEX 0                        // Minimum tone index
#define MAX_TONE_INDEX 7                        // Maximum tone index
#define DEFAULT_TONE_INDEX 5                    // Default tone index
#define BEEP_DURATION 5                         // Beep duration in pause units
#define WORD_PAUSE_MULTIPLIER 3                 // Word pause multiplier
#define CYCLE_PAUSE_MULTIPLIER 3                // Cycle pause multiplier
#define CYCLE_COUNT 300                         // Number of cycles between repeats

#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <EEPROM.h>
#include <string.h>

// Tone frequencies (PWM values)
const byte freq[8] = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};

// Volatile variables for ISR access
volatile unsigned int c_speed = DEFAULT_SPEED;   // Transmission speed
volatile unsigned int n_freq = DEFAULT_TONE_INDEX; // Current tone index
volatile boolean flag = 0;                      // Transmission flag
volatile boolean eeprom_writing = false;        // EEPROM write in progress flag

// Message buffer
char mess[MESS_MAX_SIZE];
unsigned int mess_size = 0;                     // Current message size
boolean mess_complete = false;                  // Message complete flag

// EEPROM bank definitions
#define EEPROM_BANK0_START 0
#define EEPROM_BANK0_END 63
#define EEPROM_BANK1_START 64
#define EEPROM_BANK1_END 127
#define EEPROM_BANK2_START 128
#define EEPROM_BANK2_END 191
#define EEPROM_BANK3_START 192
#define EEPROM_BANK3_END 255

// Pin definitions
#define TX_PIN PD5
#define PWM_PIN PB2
#define BUTTON_DOT 12
#define BUTTON_DASH 13

// Function prototypes
void setup();
void loop();
void readSerial();
void up_tone();
void down_tone();
void dot(byte a);
void dash(byte a);
void action(int x, byte a);
void pause(unsigned int n);
void beep();
void MemMsg(byte f);
void EEPMsg(char *str);
void SendMsg(char *str, byte f);
void clean_mess();
void CodeMorse(char *str, unsigned int i, byte f);
void disable_interrupts();
void enable_interrupts();

void setup() {
    // Configure Timer0 for PWM
    TCCR0B = 0x05;                              // Set prescaler clk/768
    TCCR0B |= (1 << WGM02);
    TCCR0A |= ((0 << COM0A1) | (1 << COM0A0));  // Set PWM mode
    
    // Configure external interrupts
    attachInterrupt(0, up_tone, LOW);            // INT0 - increase speed
    attachInterrupt(1, down_tone, LOW);          // INT1 - decrease speed
    
    // Configure button pins
    pinMode(BUTTON_DOT, INPUT);
    pinMode(BUTTON_DASH, INPUT);
    
    // Configure TX pin
    DDRD = (1 << TX_PIN);
    PORTD = (1 << TX_PIN);
    
    // Configure pin change interrupts for buttons
    GIMSK |= (1 << PCIE0);
    PCMSK |= (1 << PCINT5) | (1 << PCINT6) | (1 << PCINT7);
    
    // Initial delay for stabilization
    pause(500);
    
    // Initialize serial communication
    Serial.begin(115200);
}

void loop() {
    // Manual keying
    if (digitalRead(BUTTON_DOT) == LOW) {
        dot(freq[n_freq]);
    }
    if (digitalRead(BUTTON_DASH) == LOW) {
        dash(freq[n_freq]);
    }
    
    // Automatic message transmission
    if (flag == 1 && !eeprom_writing) {
        MemMsg(freq[n_freq]);
    }
    
    // Process received message
    if (mess_complete) {
        Serial.println(mess);
        if (!eeprom_writing) {
            SendMsg(mess, freq[n_freq]);
            Serial.println("OK");
        }
        clean_mess();
    }
    
    readSerial();
}

void readSerial() {
    while (Serial.available()) {
        char ch = char(toupper(Serial.read()));
        
        switch (ch) {
            case '\n':                          // Line feed - ignore
                break;
                
            case '\r':                          // Carriage return - end of message
                if (mess_size < MESS_MAX_SIZE) {
                    mess[mess_size] = 0;
                    mess_complete = true;
                }
                break;
                
            case '^':                           // Write to EEPROM
                if (mess_size > 0) {
                    mess[mess_size] = 0;
                    EEPMsg(mess);
                }
                break;
                
            case '~':                           // Toggle transmission
                disable_interrupts();
                flag = !flag;
                enable_interrupts();
                Serial.println("OK");
                break;
                
            default:                            // Add character to buffer
                if (mess_size < MESS_MAX_SIZE - 1) {
                    mess[mess_size] = ch;
                    mess_size++;
                }
                break;
        }
        
        // Check for buffer overflow
        if (mess_size >= MESS_MAX_SIZE - 1) {
            mess[mess_size] = 0;
            mess_complete = true;
        }
    }
}

void up_tone() {
    beep();
    disable_interrupts();
    if (c_speed > MIN_SPEED) {
        c_speed--;
    }
    enable_interrupts();
    pause(100);
}

void down_tone() {
    beep();
    disable_interrupts();
    if (c_speed < MAX_SPEED) {
        c_speed++;
    }
    enable_interrupts();
    pause(100);
}

ISR(PCINT0_vect) {
    // Handle PB5 - Toggle transmission
    if (!(PINB & (1 << PB5))) {
        beep();
        flag = !flag;
        pause(100);
    }
    
    // Handle PB6 - Increase tone
    if (!(PINB & (1 << PB6))) {
        beep();
        if (n_freq < MAX_TONE_INDEX) {
            n_freq++;
        }
        pause(100);
    }
    
    // Handle PB7 - Decrease tone
    if (!(PINB & (1 << PB7))) {
        beep();
        if (n_freq > MIN_TONE_INDEX) {
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
    PORTD = 0x00;                               // Disable TX
    DDRB = (1 << PWM_PIN);                      // Enable PWM output
    OCR0A = a;                                  // Set frequency
    pause(c_speed * x);                         // Key down duration
    PORTD = (1 << TX_PIN);                      // Enable TX
    DDRB = (0 << PWM_PIN);                      // Disable PWM output
    pause(c_speed);                             // Space between elements
}

void pause(unsigned int n) {
    for (unsigned int c = 1; c <= n; c++) {
        _delay_ms(32);
    }
}

void beep() {
    DDRB = (1 << PWM_PIN);
    OCR0A = 0x02;
    pause(BEEP_DURATION);
    OCR0A = 0x01;
    pause(BEEP_DURATION);
    OCR0A = 0x02;
    pause(BEEP_DURATION);
    DDRB = (0 << PWM_PIN);
}

void MemMsg(byte f) {
    unsigned int speed_copy;
    boolean flag_copy;
    
    // Save current state
    disable_interrupts();
    speed_copy = c_speed;
    flag_copy = flag;
    enable_interrupts();
    
    for (unsigned int i = 0; i < EEPROM.length(); i++) {
        // Check flag and stop if transmission disabled
        disable_interrupts();
        if (flag == 0) {
            enable_interrupts();
            break;
        }
        enable_interrupts();
        
        byte ch = EEPROM.read(i);
        if (ch == 0xFF) {                       // End of message marker
            break;
        }
        
        // Transmit character
        char char_buf[2] = {ch, 0};
        CodeMorse(char_buf, 0, f);
        
        // Check flag between characters
        disable_interrupts();
        if (flag == 0) {
            enable_interrupts();
            break;
        }
        enable_interrupts();
        
        pause(c_speed * WORD_PAUSE_MULTIPLIER);  // Pause between words
    }
    
    // Wait between message repeats
    for (unsigned int a = 0; a < CYCLE_COUNT; a++) {
        disable_interrupts();
        if (flag == 0) {
            enable_interrupts();
            Serial.println("QRV");
            return;
        }
        enable_interrupts();
        pause(c_speed * CYCLE_PAUSE_MULTIPLIER);
    }
    
    Serial.println("QRV");
}

void EEPMsg(char *str) {
    unsigned int _start;
    unsigned int _stop;
    unsigned int _count = 1;
    
    // Set EEPROM writing flag to prevent conflicts
    eeprom_writing = true;
    
    // Parse bank selection
    if (str[0] >= '0' && str[0] <= '3') {
        if (str[1] != 0) {
            switch (str[0]) {
                case '0':
                    _start = EEPROM_BANK0_START;
                    _stop = EEPROM_BANK0_END;
                    break;
                case '1':
                    _start = EEPROM_BANK1_START;
                    _stop = EEPROM_BANK1_END;
                    break;
                case '2':
                    _start = EEPROM_BANK2_START;
                    _stop = EEPROM_BANK2_END;
                    break;
                case '3':
                    _start = EEPROM_BANK3_START;
                    _stop = EEPROM_BANK3_END;
                    break;
                default:
                    _start = 0;
                    _stop = 0;
                    break;
            }
            
            // Write to EEPROM
            for (unsigned int c = _start; c <= _stop; c++) {
                if (str[_count] != 0) {
                    EEPROM.write(c, str[_count]);
                    _count++;
                } else {
                    EEPROM.write(c, 0xFF);      // Write end marker
                    Serial.println(mess);
                    Serial.println("OK");
                    clean_mess();
                    eeprom_writing = false;
                    return;
                }
            }
            
            Serial.println(mess);
            Serial.println("OK");
            clean_mess();
        } else {
            Serial.println("ERR");
            clean_mess();
        }
    } else {
        Serial.println("ERR");
        clean_mess();
    }
    
    eeprom_writing = false;
}

void SendMsg(char *str, byte f) {
    for (unsigned int i = 0; i < strlen(str); i++) {
        if (mess_complete) {
            CodeMorse(str, i, f);
        } else {
            return;
        }
        pause(c_speed * WORD_PAUSE_MULTIPLIER);
    }
}

void clean_mess() {
    memset(mess, 0, sizeof(mess));
    mess_size = 0;
    mess_complete = false;
}

void CodeMorse(char *str, unsigned int i, byte f) {
    switch (str[i]) {
        case 'A': dot(f); dash(f); break;
        case 'B': dash(f); dot(f); dot(f); dot(f); break;
        case 'C': dash(f); dot(f); dash(f); dot(f); break;
        case 'D': dash(f); dot(f); dot(f); break;
        case 'E': dot(f); break;
        case 'F': dot(f); dot(f); dash(f); dot(f); break;
        case 'G': dash(f); dash(f); dot(f); break;
        case 'H': dot(f); dot(f); dot(f); dot(f); break;
        case 'I': dot(f); dot(f); break;
        case 'J': dot(f); dash(f); dash(f); dash(f); break;
        case 'K': dash(f); dot(f); dash(f); break;
        case 'L': dot(f); dash(f); dot(f); dot(f); break;
        case 'M': dash(f); dash(f); break;
        case 'N': dash(f); dot(f); break;
        case 'O': dash(f); dash(f); dash(f); break;
        case 'P': dot(f); dash(f); dash(f); dot(f); break;
        case 'Q': dash(f); dash(f); dot(f); dash(f); break;
        case 'R': dot(f); dash(f); dot(f); break;
        case 'S': dot(f); dot(f); dot(f); break;
        case 'T': dash(f); break;
        case 'U': dot(f); dot(f); dash(f); break;
        case 'V': dot(f); dot(f); dot(f); dash(f); break;
        case 'W': dot(f); dash(f); dash(f); break;
        case 'X': dash(f); dot(f); dot(f); dash(f); break;
        case 'Y': dash(f); dot(f); dash(f); dash(f); break;
        case 'Z': dash(f); dash(f); dot(f); dot(f); break;
        case ' ': pause(c_speed * 7); break;
        case '.': dot(f); dash(f); dot(f); dash(f); dot(f); dash(f); break;
        case ',': dash(f); dash(f); dot(f); dot(f); dash(f); dash(f); break;
        case ':': dash(f); dash(f); dash(f); dot(f); dot(f); break;
        case '?': dot(f); dot(f); dash(f); dash(f); dot(f); dot(f); break;
        case '\'': dot(f); dash(f); dash(f); dash(f); dash(f); dot(f); break;
        case '-': dash(f); dot(f); dot(f); dot(f); dot(f); dash(f); break;
        case '/': dash(f); dot(f); dot(f); dash(f); dot(f); break;
        case '(': dash(f); dot(f); dash(f); dash(f); dot(f); break;
        case ')': dash(f); dot(f); dash(f); dash(f); dot(f); dash(f); break;
        case '"': dot(f); dash(f); dot(f); dot(f); dash(f); dot(f); break;
        case '@': dot(f); dash(f); dash(f); dot(f); dash(f); dot(f); break;
        case '=': dash(f); dot(f); dot(f); dot(f); dash(f); break;
        case '0': dash(f); dash(f); dash(f); dash(f); dash(f); break;
        case '1': dot(f); dash(f); dash(f); dash(f); dash(f); break;
        case '2': dot(f); dot(f); dash(f); dash(f); dash(f); break;
        case '3': dot(f); dot(f); dot(f); dash(f); dash(f); break;
        case '4': dot(f); dot(f); dot(f); dot(f); dash(f); break;
        case '5': dot(f); dot(f); dot(f); dot(f); dot(f); break;
        case '6': dash(f); dot(f); dot(f); dot(f); dot(f); break;
        case '7': dash(f); dash(f); dot(f); dot(f); dot(f); break;
        case '8': dash(f); dash(f); dash(f); dot(f); dot(f); break;
        case '9': dash(f); dash(f); dash(f); dash(f); dot(f); break;
    }
}

void disable_interrupts() {
    cli();  // Disable global interrupts
}

void enable_interrupts() {
    sei();  // Enable global interrupts
}
