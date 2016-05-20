// vim:set sw=4 ts=4 sts=4 ai et:

#include <Adafruit_NeoPixel.h>

#define DATA_PIN    20
#define NUM_LEDS    8
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, NEO_RGB + NEO_KHZ800);

#define NUM_BUTTONS 8

struct {
    char *name;
    int pin;
    //uint8_t debounce;
    uint64_t debounce;
    bool pressed;
} buttons[NUM_BUTTONS] = {
    {  "A",   8,   1  },
    {  "B",  12,   1  },
    {  "C",  17,   1  },
    {  "D",  15,   1  },

    {  "a",  38,   1  },
    {  "b",  40,   1  },
    {  "c",  45,   1  },
    {  "d",  43,   1  },
};

//#define ECHO

#ifdef ECHO
  #define ERR(x) do { Serial.println(x); return; } while (0);
#else
  #define ERR(x) return
#endif

void setup()
{
    int i;

    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(19200);

    strip.begin();
    strip.setBrightness(32);
    strip.show();

    for (i = 0; i < NUM_BUTTONS; i++)
        pinMode(buttons[i].pin, INPUT_PULLUP);

    for (i = 0; i < 8; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(33);

        digitalWrite(LED_BUILTIN, LOW);
        delay(33);
    }

    digitalWrite(LED_BUILTIN, HIGH);
}

void parse(char *buf)
{
    #define DELIM " \t"
    char *cmd, *mole_str, *r_str, *g_str, *b_str;
    int mole, r, g, b;

    if (!buf || !buf[0])
        ERR("No line");

    cmd = strtok(buf, DELIM);
    if (!cmd)
        ERR("No cmd");

    mole_str = strtok(NULL, DELIM);
    if (!mole_str)
        ERR("No mole");

    mole = atoi(mole_str);
    if (! (1 <= mole && mole <= 4))
        ERR("mole atoi failed");

    mole--;

    if (strcasecmp(buf, "up") == 0) {
        strip.setPixelColor(3-mole+4, 255, 255, 255);
        strip.show();
        return;
    }

    if (strcasecmp(buf, "down") == 0) {
        strip.setPixelColor(3-mole+4, 0);
        strip.show();
        return;
    }

    if (strcasecmp(buf, "color") != 0)
        ERR("Unknown command");

    r_str = strtok(NULL, DELIM);
    if (!r_str)
        ERR("r arg missing");

    g_str = strtok(NULL, DELIM);
    if (!g_str)
        ERR("g arg missing");

    b_str = strtok(NULL, DELIM);
    if (!b_str)
        ERR("b arg missing");

    r = atoi(r_str);
    g = atoi(g_str);
    b = atoi(b_str);

    if (! (0 <= r && r <= 0xff)) ERR("r out of bounds");
    if (! (0 <= g && g <= 0xff)) ERR("g out of bounds");
    if (! (0 <= b && b <= 0xff)) ERR("b out of bounds");

    strip.setPixelColor(mole, r, g, b);
    strip.show();
}

void read_serial(void)
{
    #define BUF_SIZE 32
    static char buf[BUF_SIZE];
    static size_t len;

    while (1) {
        char c;

        if (! Serial.available())
            break;

        c = Serial.read();

        if (c == '\r')
            c = '\n';

        if (c == '\n') {
            #ifdef ECHO
            Serial.println();
            #endif
            buf[len] = 0;
            parse(buf);
            len = 0;
            continue;
        }

        #ifdef ECHO
        Serial.print(c);
        #endif

        if (len < BUF_SIZE-1)
            buf[len++] = c;
    }
}

void loop()
{
    int i;

    read_serial();

    for (i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(buttons[i].pin))
            buttons[i].debounce = (buttons[i].debounce << 1) | 1;
        else
            buttons[i].debounce = (buttons[i].debounce << 1) | 0;

        if (buttons[i].debounce)
            buttons[i].pressed = false;

        if (buttons[i].debounce == 0 && !buttons[i].pressed) {
            buttons[i].pressed = true;
            //Serial.println(buttons[i].name);
            Serial.print(buttons[i].name);
        }
    }
    //delay(10);
}
