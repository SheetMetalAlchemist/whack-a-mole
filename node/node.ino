// vim:set sw=4 ts=4 sts=4 ai et filetype=c:

#include <stdarg.h>

#define THRESHOLD   0

#define MAX_HOPS    8
#define PIEZO_PIN   A8
#define BAUD        1000000

#define INTERVAL    5

#define UART_IN     Serial2
#define UART_OUT    Serial2

#define BUFSIZE     64

#define printf(...)     Serial.print(format(__VA_ARGS__))
#define printfln(...)   Serial.println(format(__VA_ARGS__))
#define debug           printfln

// KISS constants from http://www.ka9q.net/papers/kiss.html
#define FEND    0xC0  // Frame End
#define FESC    0xDB  // Frame Escape
#define TFEND   0xDC  // Transposed Frame End
#define TFESC   0xDD  // Transposed Frame Escape

struct packet {
    uint8_t hops;
    uint8_t checksum;
    uint8_t data[2];
};

/**/

//uint16_t cached1[4];
//uint16_t cached2[4];
//uint16_t *cached = cached1;
uint16_t cached[4];

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

void send_packet(struct packet *p)
{
    uint8_t *buf;
    size_t i;

    buf = (uint8_t*) p;

    p->checksum = calculate_checksum(p);

    UART_OUT.write(FEND);

    for (i = 0; i < sizeof(struct packet); i++)
    {
        switch (buf[i])
        {
            case FEND:
                UART_OUT.write(FESC);
                UART_OUT.write(TFEND);
                break;

            case FESC:
                UART_OUT.write(FESC);
                UART_OUT.write(TFESC);
                break;

            default:
                UART_OUT.write(buf[i]);
        }
    }

    UART_OUT.write(FEND);
    //dump_packet("Send: ", p);

    /*
    for (i = 0; i < p->hops; i++)
        printf("     ");
    printf("%5d\r\n", ((p->data[0] << 8) | p->data[1]));
    */
}

/*
 * Parses packet, forwards packets who's hops has not hit MAX_HOPS
 */
void packet_handler(uint8_t *buf, size_t len)
{
    struct packet *p = (struct packet *) buf;

    /*
    static int i;
    if (++i == 80) {
        i = 0;
        printf("|\r\n%7lu ", millis());
    }
    */

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

    cached[p->hops] = (p->data[0] <<8) | p->data[1];

    // Increment hope count, and do not forward it has reached MAX_HOPS
    p->hops++;
    if (p->hops > MAX_HOPS) {
        printf("."); // printf("<");
        return;
    }

    // Forward the packet
    send_packet(p);

    //printf(".");
}

/*
 * Reads up to @max characters from UART_IN.  If a FEND is found,
 * dispatches the buffer for packet parsing.
 */
void bus_handler(int max) {
    static uint8_t buf[BUFSIZE];
    static size_t len;
    static int overflow;
    static int escape;

    char c;

    while (max-- && UART_IN.available())
    {
        c = UART_IN.read();

        if (escape) {
            escape = 0;
            if (c == TFEND) {
                c = FEND;
            }
            else if (c == TFESC) {
                c = FESC;
            }
            else {
                debug("T"); // Erroneous frame escape
                len = 0;
                continue;
            }
        }
        else {
            if (c == FESC) {
                escape = 1;
                continue;
            }

            if (c == FEND) {
                if (len && !overflow)
                    packet_handler(buf, len);
                len = 0;
                overflow = 0;
                continue;
            }
        }

        if (len < BUFSIZE)
            buf[len++] = c;
        else
            overflow = 1;
    }
}

void read_piezo(void) {
    static unsigned long last_sent;
    struct packet p;
    uint16_t val;
    //char line[128];

    if (millis() < last_sent + INTERVAL)
        return;

    val = analogRead(PIEZO_PIN);
    cached[0] = val;

    int i;
    int hit = 0;

    for (i = 0; i < 4; i++) {
        if (cached[i] >= THRESHOLD)
            hit = 1;
    }

    if (hit) {
        printf("%7lu  ", millis());

        for (i = 0; i < 4;i ++) {
            if (cached[i] >= THRESHOLD)
                printf(" %4d", cached[i]);
            else
                printf(" ____");
        }
        printf("\r\n");
    }

    for (i = 0; i < 4; i++)
        cached[i] = 0;

    p.hops = 0;
    p.data[0] = (val >> 8) & 0xff;
    p.data[1] = (val >> 0) & 0xff;
    send_packet(&p);

    last_sent = millis();

    /*
    if (cached == cached1)
        cached = cached2;
    else
        cached = cached1;
    */
}

void setup() {
    int i;

    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(19200);

    UART_OUT.begin(BAUD);
    UART_IN.begin(BAUD);

    for (i = 0; i < 8; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(33);

        digitalWrite(LED_BUILTIN, LOW);
        delay(33);
    }

    digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
    bus_handler(100);
    read_piezo();
}
