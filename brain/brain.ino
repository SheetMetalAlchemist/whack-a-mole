// vim:set sw=4 ts=4 sts=4 ai et filetype=arduino:

#include <stdarg.h>
#include <Adafruit_WS2801.h>
#include <Adafruit_NeoPixel.h>

#define THRESHOLD_MOLE1     50
#define THRESHOLD_MOLE2     50
#define THRESHOLD_MOLE3     50
#define THRESHOLD_MOLE4     50
#define THRESHOLD_PLAYER1   300
#define THRESHOLD_PLAYER2   300

//#define ECHO

//#define DEBUG

/**/

#define Player1_UART    Serial1
#define Player2_UART    Serial3

#define SAMPLE_INTERVAL      5
#define HIT_CHECK_INTERVAL  40

#define BUFSIZE         64
#define BUS_MAX_READ    100
#define BAUD            1000000

#define NUM_MOLES       4

#define MOLE_LED_DATA   2  // Yellow wire
#define MOLE_LED_CLOCK  3  // Green wire

#define printf(...)     Serial.print(format(__VA_ARGS__))
#define printfln(...)   Serial.println(format(__VA_ARGS__))

#ifdef DEBUG
    #define debug           printfln
#else
    #define debug(M, ...)
#endif



// KISS constants from http://www.ka9q.net/papers/kiss.html
#define FEND    0xC0  // Frame End
#define FESC    0xDB  // Frame Escape
#define TFEND   0xDC  // Transposed Frame End
#define TFESC   0xDD  // Transposed Frame Escape

Adafruit_WS2801 strip = Adafruit_WS2801(NUM_MOLES*2, MOLE_LED_DATA, MOLE_LED_CLOCK);
Adafruit_NeoPixel panel_leds = Adafruit_NeoPixel(6, A8, NEO_GRB + NEO_KHZ800);

#ifdef ECHO
  #define ERR(x) do { Serial.println(x); return; } while (0) /* no semicolon */
#else
  #define ERR(x) return
#endif

struct packet {
    uint8_t hops;
    uint8_t checksum;
    uint8_t data[2];
};

struct node_status {
    int threshold;
    int value;
};

struct node_status moles[NUM_MOLES] = { 
    { .threshold = THRESHOLD_MOLE1 },
    { .threshold = THRESHOLD_MOLE2 },
    { .threshold = THRESHOLD_MOLE3 },
    { .threshold = THRESHOLD_MOLE4 },
};

struct node_status player1 =
    { .threshold = THRESHOLD_PLAYER1 };

struct node_status player2 =
    { .threshold = THRESHOLD_PLAYER2 };

struct UART_state {
    uint8_t buf[BUFSIZE];
    size_t len;
    int overflow;
    int escape;
} Player1_UART_State, Player2_UART_State;

int piezos[NUM_MOLES] = { A4, A5, A6, A7 };
#define SAMPLE_INTERVAL 5

#define NUM_RELAYS 4
int relay_pins[NUM_RELAYS] =  { 5, 6, 12, 11 };
int button_pins[NUM_RELAYS] = { A1, A2, A0, A3 };
int relay_state[NUM_RELAYS] = { 0, 0, 0, 0 };

char *format(const char *fmt, ... ) {
    static char buf[128];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end (args);
    return buf;
}

int calculate_checksum(struct packet *p) {
    return p->hops ^ p->data[0] ^ p->data[1];
}

void dump_packet(const char *prefix, struct packet *p) {
    debug("%sHops %d, Checksum %2x, %2x%02x\r\n",
           prefix, p->hops, p->checksum, p->data[0], p->data[1]);
}

void packet_handler(uint8_t *buf,
                    size_t len,
                    struct node_status *nodes,
                    int num_nodes)
{
    struct packet *p = (struct packet *) buf;
    int new_value;

    if (len != sizeof(struct packet)) {
        //debug("Size mismatch");
        return;
    }

    if (p->checksum != calculate_checksum(p)) {
        debug("Checksum failure");
        return;
    }

    if (p->hops > num_nodes) {
        dump_packet("Hop count too high:", p);
        return;
    }

    new_value = (p->data[0] <<8) | p->data[1];
    if (nodes[p->hops].value < new_value)
        nodes[p->hops].value = new_value;
}

void bus_handler(struct UART_state *state,
                 char c,
                 struct node_status *nodes,
                 int num_nodes)
{
    if (state->escape) {
        state->escape = 0;
        if (c == TFEND) {
            c = FEND;
        }
        else if (c == TFESC) {
            c = FESC;
        }
        else {
            debug("Erroneous frame escape");
            state->len = 0;
            return;
        }
    }
    else {
        if (c == FESC) {
            state->escape = 1;
            return;
        }

        if (c == FEND) {
            if (state->len && !state->overflow)
                packet_handler(state->buf, state->len, nodes, num_nodes);
            state->len = 0;
            state->overflow = 0;
            return;
        }
    }

    if (state->len < BUFSIZE)
        state->buf[state->len++] = c;
    else
        state->overflow = 1;
}

void setup() {
    int i;

    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(19200);

    for (i = 0; i < NUM_RELAYS; i++) {
        pinMode(relay_pins[i], OUTPUT);
        pinMode(button_pins[i], INPUT_PULLUP);
        digitalWrite(relay_pins[i], HIGH);
    }

    for (i = 0; i < NUM_MOLES; i++)
        pinMode(piezos[i], INPUT);

    Player1_UART.begin(BAUD);
    Player2_UART.begin(BAUD);

    for (i = 0; i < 8; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(33);

        digitalWrite(LED_BUILTIN, LOW);
        delay(33);
    }

    digitalWrite(LED_BUILTIN, HIGH);

    strip.begin();
    for (i = 0; i < NUM_MOLES*2; i++)
        strip.setPixelColor(i, 255, 255, 255);
    strip.show();

    panel_leds.begin();
    for (i = 0; i < 6; i++)
        panel_leds.setPixelColor(i, 0, 255, 0);
    panel_leds.show();
}

void parse_processing(char *buf)
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
    else
        mole--;

    if (strcasecmp(buf, "up") == 0) {
        relay_state[mole] = 0;
        return;
    }

    if (strcasecmp(buf, "down") == 0) {
        relay_state[mole] = 1;
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

    int offset;

    switch (mole) {
        case 0: offset = 0; break;
        case 1: offset = 2; break;
        case 2: offset = 4; break;
        case 3: offset = 6; break;
    }

    strip.setPixelColor(offset,   r, g, b);
    strip.setPixelColor(offset+1, r, g, b);
    strip.show();
}

void processing_handler(void)
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
            parse_processing(buf);
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

void update_relays(void) {
    static unsigned int button_state[NUM_RELAYS] = { 1, 1, 1, 1 };
    int i;

    for (i = 0; i < NUM_RELAYS; i++) {
        if (digitalRead(button_pins[i]) == LOW)
            button_state[i] = (button_state[i] << 1) | 0;
        else
            button_state[i] = (button_state[i] << 1) | 1;
    }

    for (i = 0; i < NUM_RELAYS; i++) {
        if (relay_state[i] || button_state[i] == 0)
            digitalWrite(relay_pins[i], LOW);
        else
            digitalWrite(relay_pins[i], HIGH);
    }
}

void evaluate_hit(struct node_status *player,
                  int player_number,
                  struct node_status *moles,
                  int num_moles)
{
    int i;

    if (player->value < player->threshold)
        return;

    for (i = 0; i < NUM_MOLES; i++) {
        if (moles[i].value > moles[i].threshold) {
            char letter = 'a' + i;
            if (player_number == 2)
                letter = toupper(letter);

            debug("%lu  HIT!  Player %d, Mole %d\r\n", millis(), player_number, i+1);
            Serial.print(letter);

        }
    }
}

void show_status(void)
{
    int i;

    printf("%7lu MASTER ", millis());
    printf("   %4d", player1.value);
    printf("   %4d", player2.value);
    printf("  |  ");

    for (i = 0; i < NUM_MOLES; i ++) {
        printf("     %4d", moles[i].value);
    }

    /*
    printf("   ");
    for (i = 0; i < 4;i ++) {
        if (cached[i] >= threshold[i])
            printf("  XXXX");
        else
            printf("  ____");
    }

    for (i = 0; i < 4;i ++) {
        cached[i] = 0;
    }
    */

    Serial.println();
}

void hit_handler(void)
{
    static unsigned long last;
    int i;

    if (millis() < last + HIT_CHECK_INTERVAL)
        return;
    last = millis();

    #ifdef DEBUG
    show_status();
    #endif

    evaluate_hit(&player1, 1, moles, NUM_MOLES);
    evaluate_hit(&player2, 2, moles, NUM_MOLES);

    for (i = 0; i < NUM_MOLES; i++)
        moles[i].value = 0;
    player1.value = 0;
    player2.value = 0;
}

void update_panel_leds(void) {
}

void sample_piezos(void)
{
    static unsigned long last_sample;
    int i;

    if (millis() < last_sample + SAMPLE_INTERVAL)
        return;

    last_sample = millis();

    for (i = 0; i < NUM_MOLES; i++)
        moles[i].value = analogRead(piezos[i]);
}

void loop() {
    int left;

    left = 100; while (Player1_UART.available() && left--) { bus_handler(&Player1_UART_State, Player1_UART.read(), &player1, 1); } 
    left = 100; while (Player2_UART.available() && left--) { bus_handler(&Player1_UART_State, Player2_UART.read(), &player2, 1); }

    sample_piezos();
    update_relays();
    update_panel_leds();
    processing_handler();
    hit_handler();
    strip.show();
    //panel_leds.show();
}
