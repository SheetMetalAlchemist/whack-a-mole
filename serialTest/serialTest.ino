// vim:set sw=4 ts=4 sts=4 ai et filetype=arduino:

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

#define BAUD    1000000
#define UART    Serial2

void setup() {
    int i;

    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(19200);
    UART.begin(BAUD);

    for (i = 0; i < 8; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(33);

        digitalWrite(LED_BUILTIN, LOW);
        delay(33);
    }

    digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
    static int num;
    static unsigned char i;
    char c;

    if (UART.available()) {
        c = UART.read();
        Serial.print(format("%02X ", c));

        if (num++ > 30) {
            Serial.println();
            num = 0;
        }
    }

    UART.print(i++, BYTE);
}
