// vim:set sw=4 ts=4 sts=4 ai et filetype=c:

#include <stdarg.h>

#define ZIGAMORPH	'!'
#define INITIAL_TTL 4
#define BUFSIZE     64
#define UART_IN     Serial1
#define UART_OUT    Serial2
#define BAUD        (115200*2)
#define PIEZO_PIN   A9
#define INTERVAL    250  /* After how many milliseconds should we send data? */

#define printf(...)     Serial.print(format(__VA_ARGS__))
#define printfln(...)   Serial.println(format(__VA_ARGS__))
#define debug           printfln

struct packet {
    uint8_t ttl;
    uint8_t checksum;
    uint8_t data_len;
    uint8_t data[];
};

/**/

char *format(const char *fmt, ... ) {
    static char buf[128];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end (args);
    return buf;
}

void hexdump_packet(char *prefix, struct packet *p) {
	printf("%s", prefix);
	for (size_t i = 0; i < sizeof(struct packet) + p->data_len; i++)
		printf(" %02x", ((uint8_t*)p)[i]);
    printf("\r\n");
}

/*
 * Calculates the CRC of the packet, which is the XOR of every field of the
 * header *except* the CRC, and also the payload data.
 */
int calculate_checksum(struct packet *p) {
	int checksum = p->ttl ^ p->data_len;
	for (int i = 0; i < p->data_len; i++)
		checksum ^= p->data[i];
	return checksum;
}

void send_packet(struct packet *p) {
    p->checksum = calculate_checksum(p);
    UART_OUT.write(ZIGAMORPH);
    UART_OUT.write((uint8_t*)p, sizeof(struct packet) + p->data_len);
    UART_OUT.write(ZIGAMORPH);
}

/*
 * Parses packet headers, and forwards packets who's TTL has not expired
 */
void packet_handler(uint8_t *buf, size_t len) {
    struct packet *p = (struct packet *) buf;

    // Ensure the packet is at least the size of our header
    if (len < sizeof(struct packet)) {
        debug("Runt");
        return;
    }

    // Ensure the payload length matches the size of the packet we received
    if (len != sizeof(struct packet) + p->data_len) {
        debug("Size mismatch");
        return;
    }

    // Verify checksum
    if (p->checksum != calculate_checksum(p)) {
        debug("Checksum failure");
        return;
    }

    // Decrement the TTL, and do not forward it he TTL has reached zero
    p->ttl--;
    if (p->ttl < 1)
        return;

    // Forward the packet
    send_packet(p);
}

/*
 * Reads up to @max characters from UART_IN.  If a zigamorph is found,
 * dispatches the buffer for packet parsing.
 */
void bus_handler(int max) {
    static uint8_t buf[BUFSIZE];
    static size_t len;
    static bool overflow;

    while (max-- && UART_IN.available()) {
        int c;

        c = UART_IN.read();

        if (c == ZIGAMORPH) {
            if (len && !overflow)
                packet_handler(buf, len);
            len = 0;
            overflow = false;
            continue;
        }

        if (len < BUFSIZE)
            buf[len++] = c;
        else
            overflow = true;
    }
}

void read_piezo(void) {
    static unsigned long last_sent;
    static uint8_t buf[sizeof(struct packet) + 2];
    static struct packet *p = (struct packet *)&buf;

    uint16_t val = analogRead(PIEZO_PIN);

    if (last_sent + INTERVAL < millis())
        return;

    p->ttl = INITIAL_TTL;
    p->data_len = 2;
    p->data[0] = (val >> 8) & 0xff;
    p->data[1] = (val >> 0) & 0xff;
    send_packet(p);

    last_sent = millis();
}

void setup() {
    int i;

    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(19200);

    UART_OUT.begin(BAUD);
    UART_IN.begin(BAUD);

    for (i = 0; i < 3; i++) {
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
