// vim:set sw=4 ts=4 sts=4 ai et:

void setup() {
    int i;

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
    int i;

    float foo = analogRead(A9) / 1024.0 * 100.0;

    //if (foo < 10)
        //goto out;

    Serial.print(millis());

    if (foo > 80)
        Serial.print("HIT!HIT!HIT!   ");
    else
        Serial.print("               ");

    for (i = 0; i < 100; i++) {
        if (i < foo)
            Serial.print("@");
        else
            Serial.print(" ");
    }

    Serial.print("  ");
    Serial.println(foo);

    out:

    //if (foo > 80)
        //delay(300);
    //else
        delay(10);
}
