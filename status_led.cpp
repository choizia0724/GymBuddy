#include "status_led.h"
#include "power.h"

namespace StatusLED {
    namespace {
    constexpr uint8_t RED_PIN    = 2;
    constexpr uint8_t YELLOW_PIN = 4;
    constexpr uint8_t GREEN_PIN  = 16;
    constexpr uint8_t BLUE_PIN   = 17;

    constexpr uint8_t CHG_STATE_PIN   = 33;
    constexpr uint8_t BAT_GOOD_PIN    = 32;

    constexpr bool    CHG_ACTIVE_LOW  = true;
    constexpr bool    GOOD_ACTIVE_HIGH = true;

    constexpr float   VBAT_LOW_THRESHOLD   = 3.60f;
    constexpr float   VBAT_GOOD_THRESHOLD  = 3.95f;
    constexpr float   VBAT_CHARGED_THRESHOLD = 4.10f;

    constexpr uint32_t STATUS_EVAL_INTERVAL_MS = 200;

    constexpr uint32_t LOW_ON_MS       = 1500;
    constexpr uint32_t LOW_OFF_MS      = 1500;
    constexpr uint32_t CHG_ON_MS       = 500;
    constexpr uint32_t CHG_OFF_MS      = 500;
    constexpr uint32_t GOOD_ON_MS      = 1000;
    constexpr uint32_t GOOD_OFF_MS     = 3000;

    enum class Status {
        Unknown,
        Low,
        Charging,
        Charged,
        BatteryGood,
    };

    struct Step {
        uint32_t durationMs;
        uint8_t  mask;
    };

    constexpr uint8_t MASK_RED    = 0x01;
    constexpr uint8_t MASK_YELLOW = 0x02;
    constexpr uint8_t MASK_GREEN  = 0x04;
    constexpr uint8_t MASK_BLUE   = 0x08;

    constexpr Step PATTERN_LOW[] = {
        {LOW_ON_MS, MASK_RED},
        {LOW_OFF_MS, 0},
    };

    constexpr Step PATTERN_CHARGING[] = {
        {CHG_ON_MS, MASK_YELLOW},
        {CHG_OFF_MS, 0},
    };

    constexpr Step PATTERN_CHARGED[] = {
        {0, MASK_GREEN},
    };

    constexpr Step PATTERN_GOOD[] = {
        {GOOD_ON_MS, MASK_BLUE},
        {GOOD_OFF_MS, 0},
    };

    const Step* g_pattern = nullptr;
    size_t g_patternLength = 0;
    size_t g_stepIndex = 0;
    uint32_t g_stepStart = 0;
    bool g_stepApplied = false;
    Status g_currentStatus = Status::Unknown;
    uint32_t g_lastEval = 0;

    void applyMask(uint8_t mask) {
        digitalWrite(RED_PIN,    (mask & MASK_RED)    ? HIGH : LOW);
        digitalWrite(YELLOW_PIN, (mask & MASK_YELLOW) ? HIGH : LOW);
        digitalWrite(GREEN_PIN,  (mask & MASK_GREEN)  ? HIGH : LOW);
        digitalWrite(BLUE_PIN,   (mask & MASK_BLUE)   ? HIGH : LOW);
    }

    void setPattern(const Step* pattern, size_t length) {
        g_pattern = pattern;
        g_patternLength = length;
        g_stepIndex = 0;
        g_stepStart = millis();
        g_stepApplied = false;
        if (g_pattern && g_patternLength) {
        applyMask(g_pattern[0].mask);
        g_stepApplied = true;
        } else {
        applyMask(0);
        }
    }

    bool isChargingActive() {
        int raw = digitalRead(CHG_STATE_PIN);
        return CHG_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
    }

    bool isGoodSignalActive() {
        int raw = digitalRead(BAT_GOOD_PIN);
        return GOOD_ACTIVE_HIGH ? (raw == HIGH) : (raw == LOW);
    }

    Status determineStatus() {
        float vbat = Power::vbat();
        if (vbat <= VBAT_LOW_THRESHOLD) {
        return Status::Low;
        }

        bool charging = isChargingActive();
        if (charging) {
        return Status::Charging;
        }

        bool goodSignal = isGoodSignalActive();
        if (goodSignal && vbat >= VBAT_CHARGED_THRESHOLD) {
        return Status::Charged;
        }

        if (vbat >= VBAT_GOOD_THRESHOLD) {
        return Status::BatteryGood;
        }

        if (goodSignal) {
        return Status::Charged;
        }

        return Status::BatteryGood;
    }

    void ensurePatternFor(Status status) {
        if (status == g_currentStatus) {
        return;
        }

        g_currentStatus = status;
        switch (status) {
        case Status::Low:
            setPattern(PATTERN_LOW, sizeof(PATTERN_LOW) / sizeof(PATTERN_LOW[0]));
            break;
        case Status::Charging:
            setPattern(PATTERN_CHARGING, sizeof(PATTERN_CHARGING) / sizeof(PATTERN_CHARGING[0]));
            break;
        case Status::Charged:
            setPattern(PATTERN_CHARGED, sizeof(PATTERN_CHARGED) / sizeof(PATTERN_CHARGED[0]));
            break;
        case Status::BatteryGood:
            setPattern(PATTERN_GOOD, sizeof(PATTERN_GOOD) / sizeof(PATTERN_GOOD[0]));
            break;
        case Status::Unknown:
        default:
            setPattern(nullptr, 0);
            break;
        }
    }

    void updatePattern() {
        if (!g_pattern || g_patternLength == 0) {
        return;
        }

        const Step& step = g_pattern[g_stepIndex];
        if (!g_stepApplied) {
        applyMask(step.mask);
        g_stepApplied = true;
        }

        if (step.durationMs == 0) {
        return;
        }

        uint32_t now = millis();
        if (now - g_stepStart >= step.durationMs) {
        g_stepIndex = (g_stepIndex + 1) % g_patternLength;
        g_stepStart = now;
        g_stepApplied = false;
        }
    }
    }

    void begin() {
    pinMode(RED_PIN, OUTPUT);
    pinMode(YELLOW_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    pinMode(CHG_STATE_PIN, INPUT_PULLUP);
    pinMode(BAT_GOOD_PIN, INPUT_PULLUP);

    applyMask(0);
    g_currentStatus = Status::Unknown;
    g_lastEval = millis();
    }

    void update() {
    uint32_t now = millis();
    if (now - g_lastEval >= STATUS_EVAL_INTERVAL_MS) {
        Status status = determineStatus();
        ensurePatternFor(status);
        g_lastEval = now;
    }

    updatePattern();
    }

} // namespace StatusLED