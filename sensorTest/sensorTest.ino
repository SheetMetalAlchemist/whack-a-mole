// vim:set sw=4 ts=4 sts=4 ai et:

#define NUM_BUTTONS 4
#define NUM_POTS    6
int buttons[NUM_BUTTONS] = { A0, A1, A2, A3 };
int pots[NUM_POTS] = { A4, A5, A6, A7, A8, A9 };

#define printf(...)   Serial.print(format(__VA_ARGS__))
#define printfln(...) Serial.println(format(__VA_ARGS__))
#include <stdarg.h>
char *format(const char *fmt, ... ) {
    static char buf[128];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end (args);
    return buf;
}

void setup() {
    int i;

    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(19200);

    for (i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i], INPUT_PULLUP);
    }

    for (i = 0; i < NUM_POTS; i++) {
        pinMode(pots[i], INPUT);
    }


    for (i = 0; i < 8; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(33);

        digitalWrite(LED_BUILTIN, LOW);
        delay(33);
    }

    digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
    Serial.println(format("%lu   %d %d %d %d   %4d  %4d  %4d  %4d  %4d  %4d",
                            millis(),
                            digitalRead(buttons[0]),
                            digitalRead(buttons[1]),
                            digitalRead(buttons[2]),
                            digitalRead(buttons[3]),
                            analogRead(pots[0]),
                            analogRead(pots[1]),
                            analogRead(pots[2]),
                            analogRead(pots[3]),
                            analogRead(pots[4]),
                            analogRead(pots[5])));
    delay(10);
}
