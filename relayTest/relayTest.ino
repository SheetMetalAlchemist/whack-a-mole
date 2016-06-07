// vim:set sw=4 ts=4 sts=4 ai et:

#define NUM_RELAYS 4

//int relays[NUM_RELAYS] =  { A0, A1, A2, A3 };
int relays[NUM_RELAYS] =  { 5, 6, 12, 11 };

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

    for (i = 0; i < NUM_RELAYS; i++) {
        pinMode(relays[i], OUTPUT);
        digitalWrite(relays[i], HIGH);
    }

    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(19200);

    for (i = 0; i < 8; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(33);

        digitalWrite(LED_BUILTIN, LOW);
        delay(33);
    }

    digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
    char c;
    int num;

    if (! Serial.available())
        return;

    c = Serial.read();

    if ('1' <= c && c <= '0' + NUM_RELAYS) {
        num = c - '0' - 1;
        digitalWrite(relays[num], !digitalRead(relays[num]));
        Serial.println(format("%d  %d", num, digitalRead(relays[num])));
    }
    else {
        //Serial.println(format("%lu %d", millis(), c));
    }
}
