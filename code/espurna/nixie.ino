
#include <TimeLib.h>

#define DATA_PIN 15 // 74595 pin#14
#define CLOCK_PIN 5 // 74595 pin#11
#define LATCH_PIN 4 // 74595 pin#12

#define NIXIE_A 16 // 74171 pin#3
#define NIXIE_B 12 // 74171 pin#4
#define NIXIE_C 13 // 74171 pin#6
#define NIXIE_D 14 // 74171 pin#7

#define NUMBER_OF_NIXIES 4
#define NIXIE_BINARY_POSITION(n) (1 << (NUMBER_OF_NIXIES - n - 1))

bool _demoMode, _clockMode;
int _delay;
int _antiPoisonDelay = 50;
unsigned int currentValue[NUMBER_OF_NIXIES];

void _nixieAntiPoison() {
    for (int k = 0; k < 5; k++) {
        for (int i = 0; i < 10; i++) {
            writeValue(i);
            int activeNixie = 1;
            for (int j = 0; j < NUMBER_OF_NIXIES; j++) {
                enableNixie(activeNixie);
                activeNixie = activeNixie << 1;
                nice_delay(_antiPoisonDelay);
            }
        }
    }
}

void _nixieLoop() {
    static unsigned long lastDemo = millis();
    static unsigned long lastNTP = millis();
    static unsigned long lastDraw = millis();
    static int valueWritten = -1;
    static int activeNixie = 0;

    if (_demoMode) {
        if (millis() - lastDemo < _delay)
            return;

        lastDemo = millis();

        valueWritten++;
        if (valueWritten >= 10) {
            valueWritten = 0;

            activeNixie++;
            if (activeNixie >= NUMBER_OF_NIXIES) {
                activeNixie = 0;
            }
        }

        writeValue(valueWritten);
        enableNixie(NIXIE_BINARY_POSITION(activeNixie));
        nice_delay(250);
    } else if (_clockMode) {

        unsigned long ts = millis();
        if (((ts - lastNTP) >= 1000)) { // ntpSynced() &&
            lastNTP = ts;

            time_t t = now();
            int hr = hour(t);
            int min = minute(t);
            int sec = second(t);
            currentValue[0] = hr / 10;
            currentValue[1] = hr % 10;
            currentValue[2] = min / 10;
            currentValue[3] = min % 10;
            // currentValue[3] = sec % 10;
            // currentValue[3] = sec % 10;
            // DEBUG_MSG_P(PSTR("%d %d => %d%d:%d%d\n"), hr, min, currentValue[0], currentValue[1], currentValue[2],
            //            currentValue[3]);
        }

        if ((ts - lastDraw) >= _delay) {
            activeNixie++;
            if (activeNixie >= NUMBER_OF_NIXIES) {
                activeNixie = 0;
            }

            lastDraw = ts;
            enableNixie(NIXIE_BINARY_POSITION(activeNixie));
            writeValue(currentValue[activeNixie]);
        }
    }
}

unsigned char decToBcd(unsigned char val) { return (((val / 10) * 16) + (val % 10)); }

void nixieSetup() {
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(DATA_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);

    pinMode(NIXIE_A, OUTPUT);
    pinMode(NIXIE_B, OUTPUT);
    pinMode(NIXIE_C, OUTPUT);
    pinMode(NIXIE_D, OUTPUT);

    // Default values
    for (int i = 0; i < NUMBER_OF_NIXIES; i++) {
        currentValue[i] = i + 1;
    }

    _clockMode = true;
    _delay = 1;

    espurnaRegisterLoop(_nixieLoop);
    _nixieTerminal();
}

void _nixieTerminal() {
    terminalRegisterCommand(F("display"), [](Embedis *e) {
        int value;
        if (e->argc > 1) {
            value = String(e->argv[1]).toInt();
            writeValue(value);
            DEBUG_MSG_P(PSTR("value=%d\n"), value);
        }

        if (e->argc > 2) {
            value = String(e->argv[2]).toInt();
            enableNixie(value);
            DEBUG_MSG_P(PSTR("nixie=%d\n"), value);
        }
        terminalOK();
    });

    terminalRegisterCommand(F("delay"), [](Embedis *e) {
        if (e->argc > 1) {
            int value = String(e->argv[1]).toInt();
            _delay = value;
        }
        DEBUG_MSG_P(PSTR("delay=%d\n"), _delay);
        terminalOK();
    });

    terminalRegisterCommand(F("mode"), [](Embedis *e) {
        int value;
        if (e->argc > 1) {
            value = String(e->argv[1]).toInt();

            _demoMode = _clockMode = false;
            switch (value) {
            case 1:
                _demoMode = true;
                DEBUG_MSG_P(PSTR("demoMode\n"));
                break;
            case 2:
                _clockMode = true;
                DEBUG_MSG_P(PSTR("clockMode\n"));
                break;
            case 3:
                if (e->argc > 2) {
                    _antiPoisonDelay = String(e->argv[2]).toInt();
                }

                DEBUG_MSG_P(PSTR("nixieAntiPoison =%d\n"), _antiPoisonDelay);
                _nixieAntiPoison();
                break;
            }
        }
        terminalOK();
    });
}

void writeValue(int value) {
    unsigned int D = 0;
    unsigned int C = 0;
    unsigned int B = 0;
    unsigned int A = 0;

    switch (value) {
    case 1:
        A = 1;
    case 0:
        break;

    case 3:
        A = 1;
    case 2:
        B = 1;
        break;

    case 5:
        A = 1;
    case 4:
        C = 1;
        break;

    case 7:
        A = 1;
    case 6:
        C = B = 1;
        break;

    case 9:
        A = 1;
    case 8:
        D = 1;
        break;
    }

    digitalWrite(NIXIE_A, A);
    digitalWrite(NIXIE_B, B);
    digitalWrite(NIXIE_C, C);
    digitalWrite(NIXIE_D, D);
}

void enableNixie(int which) {
    digitalWrite(LATCH_PIN, 0);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, which);
    digitalWrite(LATCH_PIN, 1);
}