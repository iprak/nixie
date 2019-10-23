
#include <TimeLib.h>

#define DATA_PIN 15 // 74595 pin#14
#define CLOCK_PIN 5 // 74595 pin#11
#define LATCH_PIN 4 // 74595 pin#12

#define NIXIE_A 16 // 74171 pin#3
#define NIXIE_B 12 // 74171 pin#4
#define NIXIE_C 13 // 74171 pin#6
#define NIXIE_D 14 // 74171 pin#7

#define NUMBER_OF_NIXIES 6
#define DATETIME_DISPLAY_DURATION 5000
#define IP_ADDRESS_DISPLAY_DURATION 1500
#define ANTIPOISION_ADDRESS_DISPLAY_DURATION 1000

#define NIXIE_BINARY_POSITION(n) (1 << (NUMBER_OF_NIXIES - (n)-1))

#define NIXIE_MODE_NONE 0
#define NIXIE_MODE_DEMO 1
#define NIXIE_MODE_CLOCK 2
#define NIXIE_MODE_POISON 3
#define NIXIE_MODE_IP 4
#define NIXIE_MODE_MAX 5

int _timeValue[NUMBER_OF_NIXIES];
int _dateValue[NUMBER_OF_NIXIES];
int _buffer[NUMBER_OF_NIXIES];
int _mode, _renderCount;
int _ipAddrOctet = 0;
unsigned long _renderTime = 0;
unsigned long _renderWaitTime = 0;
unsigned long _lastNTPInstant = 0;
unsigned long _lastDemoInstant = 0;
unsigned long _demoUpdateDelay = 50;
unsigned long _delayBetweenDigits = 2500;

void _copyToBuffer(int *source) {
    for (int i = 0; i < NUMBER_OF_NIXIES; i++) {
        _buffer[i] = *(source + i);
    }
}

void _fillBuffer(int value) {
    for (int i = 0; i < NUMBER_OF_NIXIES; i++) {
        _buffer[i] = value;
    }
}

void _updateTimeFrame() {
    unsigned long current_time = millis();

    if ((current_time - _lastNTPInstant) >= 1000) { // ntpSynced() &&
        _lastNTPInstant = current_time;

        time_t t = now();
        int piece = hour(t);
        _timeValue[0] = piece / 10;
        if (_timeValue[0] == 0) { // Turn off left most time digit if 0
            _timeValue[0] = -1;
        }
        _timeValue[1] = piece % 10;
        piece = minute(t);
        _timeValue[2] = piece / 10;
        _timeValue[3] = piece % 10;
        if (NUMBER_OF_NIXIES == 6) {
            piece = second(t);
            _timeValue[4] = piece / 10;
            _timeValue[5] = piece % 10;
        }

        piece = month(t);
        _dateValue[0] = piece / 10;
        _dateValue[1] = piece % 10;
        piece = day(t);
        _dateValue[2] = piece / 10;
        _dateValue[3] = piece % 10;
        if (NUMBER_OF_NIXIES == 6) {
            piece = year(t) % 100;
            _dateValue[4] = piece / 10;
            _dateValue[5] = piece % 10;
        }
        // DEBUG_MSG_P(PSTR("%d%d:%d%d:%d%d\n"), _timeValue[0], _timeValue[1], _timeValue[2], _timeValue[3],
        // _timeValue[4],  _timeValue[5]);
    }
}

void _updateAntiPoisonFrame() {
    unsigned long current_time = millis();
    if ((current_time - _lastDemoInstant) >= ANTIPOISION_ADDRESS_DISPLAY_DURATION) {
        _lastDemoInstant = current_time;
        _fillBuffer((_buffer[0] + 1) % 10);
    }
}

void _updateIpFrame() {
    unsigned long current_time = millis();
    if ((current_time - _lastDemoInstant) >= IP_ADDRESS_DISPLAY_DURATION) {
        _lastDemoInstant = current_time;
        IPAddress ip = WiFi.localIP();

        _fillBuffer(-1); // Turn all digits off

        int value = ip[_ipAddrOctet];               // e.g.   192
        _buffer[NUMBER_OF_NIXIES - 1] = value % 10; // 2
        value = value / 10;                         // 19
        if (value > 0) {
            _buffer[NUMBER_OF_NIXIES - 2] = value % 10; // 9
            value = value / 10;                         // 1
            if (value > 0) {
                _buffer[NUMBER_OF_NIXIES - 3] = value; // 1
            }
        }

        _ipAddrOctet = (_ipAddrOctet + 1) % 4;
    }
}

void _updateDemoFrame() {
    unsigned long current_time = millis();
    if ((current_time - _lastDemoInstant) >= _demoUpdateDelay) {
        _lastDemoInstant = current_time;

        int i = NUMBER_OF_NIXIES - 1;
        _buffer[i]++; // Increment right most, adjust rest
        for (; i > -1; i--) {
            if (_buffer[i] == 10) {
                _buffer[i] = 0;

                if (i > 0) {
                    if (_buffer[i - 1] == -1) {
                        _buffer[i - 1] = 1;
                    } else {
                        _buffer[i - 1]++;
                    }
                }
            }
        }
    }
}

void _printStatus() {
    switch (_mode) {
    case NIXIE_MODE_NONE:
        DEBUG_MSG_P(PSTR("none"));
        break;
    case NIXIE_MODE_DEMO:
        DEBUG_MSG_P(PSTR("demo"));
        break;
    case NIXIE_MODE_CLOCK:
        DEBUG_MSG_P(PSTR("clock"));
        break;
    case NIXIE_MODE_POISON:
        DEBUG_MSG_P(PSTR("poison"));
        break;
    case NIXIE_MODE_IP:
        DEBUG_MSG_P(PSTR("ip"));
        break;
    }
    DEBUG_MSG_P(PSTR(" render count=%d wait=%d time=%d\n"), _renderCount, _renderWaitTime, _renderTime);
}

void _writeValue(int value) {
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

    default:
        A = B = C = D = 1;
        break;
    }

    digitalWrite(NIXIE_A, A);
    digitalWrite(NIXIE_B, B);
    digitalWrite(NIXIE_C, C);
    digitalWrite(NIXIE_D, D);
}

void _turnNixieOn(int which) {
    digitalWrite(LATCH_PIN, 0);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, which);
    digitalWrite(LATCH_PIN, 1);
}

void _turnAllNixiessOff() { _turnNixieOn(0); }
void _turnAllNixiessOn() { _turnNixieOn(255); }

void nixieSwitchMode() {
    _mode++;
    if (_mode >= NIXIE_MODE_MAX) {
        _mode = NIXIE_MODE_DEMO;
    }

    switch (_mode) {
    case NIXIE_MODE_DEMO:
        _fillBuffer(-1);
        break;
    case NIXIE_MODE_POISON:
        _fillBuffer(0);
        break;
    }

    _printStatus();
}

void _nixieTerminal() {
    terminalRegisterCommand(F("status"), [](Embedis *e) {
        _printStatus();
        terminalOK();
    });

    terminalRegisterCommand(F("delay"), [](Embedis *e) {
        if (e->argc > 1) {
            _delayBetweenDigits = String(e->argv[1]).toInt();
            DEBUG_MSG_P(PSTR("delayBetweenDigits=%d\n"), _delayBetweenDigits);
        }
        terminalOK();
    });

    terminalRegisterCommand(F("disp"), [](Embedis *e) {
        int value;
        if (e->argc > 1) {
            value = String(e->argv[1]).toInt();

            _mode = NIXIE_MODE_NONE;
            _writeValue(value);

            if (e->argc > 2) {
                int nixie = String(e->argv[2]).toInt();
                _turnNixieOn(NIXIE_BINARY_POSITION(nixie + 1));
                DEBUG_MSG_P(PSTR("value=%d on nixie=%d\n"), value, nixie);
            } else {
                DEBUG_MSG_P(PSTR("value=%d on all\n"), value);
                _turnAllNixiessOn();
            }
        }

        _printStatus();
        terminalOK();
    });

    terminalRegisterCommand(F("mode"), [](Embedis *e) {
        int value;
        if (e->argc > 1) {
            value = String(e->argv[1]).toInt();
            switch (value) {
            case NIXIE_MODE_DEMO:
                if (e->argc > 2) {
                    _demoUpdateDelay = String(e->argv[2]).toInt();
                }
                _fillBuffer(-1);
                _mode = NIXIE_MODE_DEMO;
                break;
            case NIXIE_MODE_CLOCK:
                _mode = NIXIE_MODE_CLOCK;
                break;
            case NIXIE_MODE_POISON:
                _fillBuffer(0);
                _mode = NIXIE_MODE_POISON;
                break;
            case NIXIE_MODE_IP:
                _mode = NIXIE_MODE_IP;
                break;
            }
        }
        _printStatus();
        terminalOK();
    });
}

void _nixieLoop() {
    static unsigned long lastDraw = millis();
    static bool showingDate;
    static unsigned long displayDuration;

    if (_mode == NIXIE_MODE_NONE || Update.isRunning()) { // Don't draw during updates
        return;
    }

    unsigned long ts = millis();
    _renderCount++;
    if (_renderCount > 3000) {
        _renderCount = 1;
        _renderWaitTime = _renderTime = 0;
    }
    _renderWaitTime += ts - lastDraw;
    displayDuration += ts - lastDraw;

    switch (_mode) {
    case NIXIE_MODE_DEMO:
        _updateDemoFrame();
        break;
    case NIXIE_MODE_CLOCK:
        _updateTimeFrame();

        // Swap date and time every DATETIME_DISPLAY_DURATION
        if (displayDuration > DATETIME_DISPLAY_DURATION) {
            displayDuration = 0;
            showingDate = !showingDate;
        }
        _copyToBuffer(showingDate ? _dateValue : _timeValue);
        break;
    case NIXIE_MODE_POISON:
        _updateAntiPoisonFrame();
        break;
    case NIXIE_MODE_IP:
        _updateIpFrame();
        break;
    }

    bool sameValue = true;
    for (int i = 1; i < NUMBER_OF_NIXIES; i++) {
        sameValue = sameValue && (_buffer[i - 1] == _buffer[i]);
    }

    if (sameValue) {
        if (_buffer[0] < 0) {
            _turnAllNixiessOff();
        } else {
            _writeValue(_buffer[0]);
            _turnAllNixiessOn();
            delayMicroseconds(_delayBetweenDigits);
        }

    } else {
        for (int i = 0; i < NUMBER_OF_NIXIES; i++) {
            int valueToWrite = _buffer[i];
            if (valueToWrite >= 0) {
                _turnNixieOn(NIXIE_BINARY_POSITION(i));
                _writeValue(valueToWrite);
                delayMicroseconds(_delayBetweenDigits);
            }
        }
    }

    // Capture times
    lastDraw = ts;
    _renderTime += millis() - ts;
}

void nixieSetup() {
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(DATA_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(NIXIE_A, OUTPUT);
    pinMode(NIXIE_B, OUTPUT);
    pinMode(NIXIE_C, OUTPUT);
    pinMode(NIXIE_D, OUTPUT);

    // https://www.forward.com.au/pfod/ESP8266/GPIOpins/ESP8266_01_pin_magic.html
    pinMode(3, INPUT_PULLUP);

    // Default values
    for (int i = 0; i < NUMBER_OF_NIXIES; i++) {
        _timeValue[i] = _dateValue[i] = i + 1;
    }
    _mode = NIXIE_MODE_CLOCK;
    espurnaRegisterLoop(_nixieLoop);
    _nixieTerminal();
}