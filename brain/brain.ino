// vim:set sw=4 ts=4 sts=4 ai et filetype=arduino:

#include <stdarg.h>
#include <Adafruit_WS2801.h>

#define BUS_MAX_READ    100
#define BAUD        1000000
#define PIEZO_PIN   A8
#define THRESHOLD   1000

#define INTERVAL    5

#define UART_MOLES      Serial1
#define UART_PLAYER1    Serial2
#define UART_PLAYER2    Serial3

#define BUFSIZE     64

#define printf(...)     Serial.print(format(__VA_ARGS__))
#define printfln(...)   Serial.println(format(__VA_ARGS__))
#define debug           printfln

// KISS constants from http://www.ka9q.net/papers/kiss.html
#define FEND    0xC0  // Frame End
#define FESC    0xDB  // Frame Escape
#define TFEND   0xDC  // Transposed Frame End
#define TFESC   0xDD  // Transposed Frame Escape

#define NUM_MOLES   4
#define NUM_UARTS   3

#define HOW_OFTEN   10  // 1/100 Hz = 10 milliseconds

#define MOLE_LED_DATA_PIN   3   // Yellow wire
#define MOLE_LED_CLOKC_PIN  4   // Green wire
Adafruit_WS2801 strip = Adafruit_WS2801(NUM_MOLES, MOLE_LED_DATA_PIN, MOLE_LED_CLOKC_PIN);

#define ECHO

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

/**/

struct node_status {
    int potPin;
    int threshold;
    int value;
};

struct node_status moles[NUM_MOLES] = {
    { .potPin = A0 },
    { .potPin = A1 },
    { .potPin = A2 },
    { .potPin = A3 },
};

struct node_status player1 = { .potPin = A4 };
struct node_status player2 = { .potPin = A5 };

struct UART_state {
    HardwareSerial UART;
    uint8_t buf[BUFSIZE];
    size_t len;
    int overflow;
    int escape;
} UART_states[NUM_UARTS] = {
    { .UART = Serial1 },
    { .UART = Serial3 },
    { .UART = Serial3 },
};

int solenoid_pins[4] = { 3, 4, 5, 6 };

/**/


char *format(const char *fmt, ... )
{
    static char buf[128];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end (args);
    return buf;
}

void dump_packet(const char *prefix, struct packet *p)
{
    printf("%sHops %d, Checksum %2x, %2x%02x\r\n",
        prefix,
        p->hops, p->checksum, p->data[0], p->data[1]);
}

int calculate_checksum(struct packet *p)
{
    return p->hops ^ p->data[0] ^ p->data[1];
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
        printf("S");
        return;
    }

    // Verify checksum
    if (p->checksum != calculate_checksum(p)) {
        //debug("Checksum failure");
        printf("C");
        return;
    }

    //dump_packet("Recv: ", p);

    if (p->hops > num_nodes) {
        //debug("Hop count too high");
        printf("H");
        return;
    }

    new_value = (p->data[0] <<8) | p->data[1];

    if (nodes[p->hops].value < new_value)
        nodes[p->hops].value = new_value;
}

void bus_handler(struct UART_state *state,
                 struct node_status *nodes,
                 int num_nodes)
{
    char c;
    int max_read = BUS_MAX_READ;

    while (max_read-- && state->UART.available())
    {
        c = state->UART.read();

        if (state->escape) {
            state->escape = 0;
            if (c == TFEND) {
                c = FEND;
            }
            else if (c == TFESC) {
                c = FESC;
            }
            else {
                debug("T"); // Erroneous frame escape
                state->len = 0;
                continue;
            }
        }
        else {
            if (c == FESC) {
                state->escape = 1;
                continue;
            }

            if (c == FEND) {
                if (state->len && !state->overflow)
                    packet_handler(state->buf, state->len, nodes, num_nodes);
                state->len = 0;
                state->overflow = 0;
                continue;
            }
        }

        if (state->len < BUFSIZE)
            state->buf[state->len++] = c;
        else
            state->overflow = 1;
    }
}

void setup() {
    int i;

    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(19200);

    for (i = 0; i < NUM_UARTS; i++)
        UART_states[i].UART.begin(BAUD);

    for (i = 0; i < NUM_MOLES; i++)
        pinMode(moles[i].potPin, INPUT);

    pinMode(player1.potPin, INPUT);
    pinMode(player2.potPin, INPUT);

    for (i = 0; i < 8; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(33);

        digitalWrite(LED_BUILTIN, LOW);
        delay(33);
    }

    digitalWrite(LED_BUILTIN, HIGH);

    strip.begin();
    //strip.setBrightness(32);
    strip.show();
}

void pot_handler(node_status *nodes, int num_nodes)
{
    for (int i = 0; i < num_nodes; i++)
        nodes[i].threshold = analogRead(nodes[i].potPin);
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
            printf("HIT!  Player %d, Mole %d\r\n", player_number, i);
        }
    }
}

void hit_handler(void)
{
    static unsigned long last_time;
    int i;

    if (millis() < last_time + HOW_OFTEN)
        return;

    last_time = millis();

    evaluate_hit(&player1, 1, moles, NUM_MOLES);
    evaluate_hit(&player2, 2, moles, NUM_MOLES);

    player1.value = 0;
    player2.value = 0;

    for (i = 0; i < NUM_MOLES; i++)
        moles[i].value = 0;
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
        digitalWrite(solenoid_pins[mole], HIGH);
        return;
    }

    if (strcasecmp(buf, "down") == 0) {
        digitalWrite(solenoid_pins[mole], LOW);
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


void loop() {
    bus_handler(&UART_states[0], moles, NUM_MOLES);
    bus_handler(&UART_states[1], &player1, 1);
    bus_handler(&UART_states[2], &player2, 1);

    processing_handler();

    pot_handler(moles, NUM_MOLES);
    pot_handler(&player1, 1);
    pot_handler(&player2, 1);

    hit_handler();
}
