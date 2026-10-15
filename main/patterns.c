#include "patterns.h"
#include <math.h>
#include <stdlib.h>
#include "esp_random.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Helper: clamp
static inline uint16_t clamp_duty(int32_t val) {
    if (val < 0) return 0;
    if (val > PWM_MAX_DUTY) return PWM_MAX_DUTY;
    return (uint16_t)val;
}

// 1. Solid / Full ON (Master brightness controls the actual level)
void pattern_solid(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        out_duty[i] = PWM_MAX_DUTY;
    }
}

// 2. Breathing / Slow Sine Pulse (4 channels in unison)
void pattern_breathe(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    // 1 cycle = ~150 frames (approx 3.0s at 50Hz, speed=1.0)
    float angle = (frame * speed * 0.04f);
    float sine = (sinf(angle) + 1.0f) * 0.5f; // 0.0 ~ 1.0
    // Gamma correction approximation: square the sine for a more natural human eye perception
    float factor = sine * sine;
    uint16_t val = (uint16_t)(factor * PWM_MAX_DUTY);
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        out_duty[i] = val;
    }
}

// 3. Thruster / Spacecraft Afterburner (Random rapid flicker around high duty)
void pattern_thruster(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        // High baseline ~70%, flicker randomly between 50% and 100%
        int noise = (int)(esp_random() % (PWM_MAX_DUTY / 2));
        out_duty[i] = clamp_duty(PWM_MAX_DUTY / 2 + noise);
    }
}

// 4. Knight Rider / Sequential Scanner (0 -> 1 -> 2 -> 3 -> 2 -> 1)
void pattern_scanner(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    // 6 steps per sweep: 0, 1, 2, 3, 2, 1
    float pos = fmodf(frame * speed * 0.15f, 6.0f);
    float current_led = pos < 3.0f ? pos : (6.0f - pos);

    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        float dist = fabsf((float)i - current_led);
        if (dist < 1.0f) {
            float brightness = 1.0f - dist;
            out_duty[i] = (uint16_t)(brightness * brightness * PWM_MAX_DUTY);
        } else {
            out_duty[i] = (uint16_t)(PWM_MAX_DUTY * 0.03f); // faint trail
        }
    }
}

// 5. Warp Charge (Accelerating flash build up then blast)
void pattern_warp_charge(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    uint32_t cycle_len = (uint32_t)(250 / (speed > 0.1f ? speed : 1.0f));
    uint32_t t = frame % cycle_len;
    float progress = (float)t / (float)cycle_len;

    if (progress < 0.85f) {
        // Charging phase: frequency of pulse increases as progress rises
        float freq = 2.0f + progress * 25.0f;
        float angle = progress * freq * 2.0f * M_PI;
        float val = (sinf(angle) > 0.0f) ? (progress * 0.8f + 0.2f) : 0.0f;
        uint16_t duty = (uint16_t)(val * PWM_MAX_DUTY);
        for (int i = 0; i < LED_NUM_CHANNELS; i++) {
            out_duty[i] = duty;
        }
    } else if (progress < 0.95f) {
        // Maximum blast flash
        for (int i = 0; i < LED_NUM_CHANNELS; i++) {
            out_duty[i] = PWM_MAX_DUTY;
        }
    } else {
        // Cool down
        float cool = 1.0f - ((progress - 0.95f) / 0.05f);
        uint16_t duty = (uint16_t)(cool * PWM_MAX_DUTY);
        for (int i = 0; i < LED_NUM_CHANNELS; i++) {
            out_duty[i] = duty;
        }
    }
}

// 6. Navigation Strobe (Aircraft: CH0/1 steady position, CH2/3 double strobe)
void pattern_nav_strobe(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    // CH0 and CH1: Position lights (steady ~35%)
    out_duty[0] = (uint16_t)(PWM_MAX_DUTY * 0.35f);
    out_duty[1] = (uint16_t)(PWM_MAX_DUTY * 0.35f);

    // CH2 and CH3: Strobe light (double pulse every ~1.2 sec)
    uint32_t cycle = (uint32_t)(60 / (speed > 0.1f ? speed : 1.0f));
    uint32_t t = frame % cycle;

    bool strobe_on = (t <= 2) || (t >= 6 && t <= 8);
    uint16_t strobe_duty = strobe_on ? PWM_MAX_DUTY : 0;
    out_duty[2] = strobe_duty;
    out_duty[3] = strobe_duty;
}

// 7. Landing Lights (Smooth ramp up to full bright)
void pattern_landing_lights(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    uint32_t cycle = (uint32_t)(150 / (speed > 0.1f ? speed : 1.0f));
    uint32_t t = frame % cycle;
    float ramp = (float)t / (float)(cycle * 0.6f);
    if (ramp > 1.0f) ramp = 1.0f;

    uint16_t val = (uint16_t)(ramp * ramp * PWM_MAX_DUTY);
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        out_duty[i] = val;
    }
}

// 8. Hazard Flasher (Synchronous blinking, car turn signal / hazard)
void pattern_hazard(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    uint32_t cycle = (uint32_t)(40 / (speed > 0.1f ? speed : 1.0f));
    uint32_t t = frame % cycle;
    uint16_t val = (t < cycle / 2) ? PWM_MAX_DUTY : 0;
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        out_duty[i] = val;
    }
}

// 9. Engine Start / Starter Motor (Cranking chatter then solid running)
void pattern_engine_start(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    uint32_t cycle = (uint32_t)(200 / (speed > 0.1f ? speed : 1.0f));
    uint32_t t = frame % cycle;

    if (t < 50) {
        // Cranking: rhythmic clicks/blinks
        bool on = (t % 10 < 3);
        uint16_t val = on ? (uint16_t)(PWM_MAX_DUTY * 0.4f) : 0;
        for (int i = 0; i < LED_NUM_CHANNELS; i++) out_duty[i] = val;
    } else if (t < 80) {
        // Catching fire: irregular sputters
        int noise = (int)(esp_random() % PWM_MAX_DUTY);
        for (int i = 0; i < LED_NUM_CHANNELS; i++) out_duty[i] = (uint16_t)noise;
    } else {
        // Idling smooth light with subtle engine vibration
        float idle_vib = sinf(frame * 0.8f) * 0.05f + 0.95f;
        uint16_t val = clamp_duty((int32_t)(idle_vib * PWM_MAX_DUTY));
        for (int i = 0; i < LED_NUM_CHANNELS; i++) out_duty[i] = val;
    }
}

// 10. Arc Welding / Electric Spark (Intense erratic flashes)
void pattern_welding(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    // Sporadic welding burst
    uint32_t burst = (frame / 20) % 5;
    if (burst == 0 || burst == 1 || burst == 3) {
        for (int i = 0; i < LED_NUM_CHANNELS; i++) {
            if (esp_random() % 100 < 35) {
                out_duty[i] = PWM_MAX_DUTY;
            } else if (esp_random() % 100 < 60) {
                out_duty[i] = (uint16_t)(PWM_MAX_DUTY * 0.3f);
            } else {
                out_duty[i] = 0;
            }
        }
    } else {
        // Calm pause with tiny afterglow
        for (int i = 0; i < LED_NUM_CHANNELS; i++) {
            out_duty[i] = (uint16_t)(PWM_MAX_DUTY * 0.02f);
        }
    }
}

// 11. Glitched / Contact Loose (Occasional sudden dips and flickers)
void pattern_glitch(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    // Normal state is steady ON
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        out_duty[i] = PWM_MAX_DUTY;
    }
    // Random glitch trigger
    if (esp_random() % 100 < 8) {
        int target = esp_random() % LED_NUM_CHANNELS;
        out_duty[target] = (esp_random() % 2 == 0) ? 0 : (uint16_t)(PWM_MAX_DUTY * 0.2f);
    }
}

// 12. Fireflies / Multi-phase organic pulse (Each LED breathes out of phase)
void pattern_fireflies(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        float offset = (float)i * (M_PI * 0.5f);
        float angle = (frame * speed * 0.035f) + offset;
        float sine = (sinf(angle) + 1.0f) * 0.5f;
        float factor = sine * sine * sine; // peaky flash
        out_duty[i] = (uint16_t)(factor * PWM_MAX_DUTY);
    }
}

// 13. Alternate Flasher (Police / Emergency: CH0/1 vs CH2/3)
void pattern_alternate(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    uint32_t cycle = (uint32_t)(30 / (speed > 0.1f ? speed : 1.0f));
    uint32_t phase = (frame / (cycle / 2)) % 2;

    uint16_t on_val = PWM_MAX_DUTY;
    uint16_t off_val = 0;

    out_duty[0] = phase == 0 ? on_val : off_val;
    out_duty[1] = phase == 0 ? on_val : off_val;
    out_duty[2] = phase == 1 ? on_val : off_val;
    out_duty[3] = phase == 1 ? on_val : off_val;
}

// 14. Manual (Handled specifically by led_controller with individual channel duties)
void pattern_manual(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    // Will be overridden by manual channel settings in controller
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        out_duty[i] = PWM_MAX_DUTY;
    }
}

// 15. All OFF
void pattern_off(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]) {
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        out_duty[i] = 0;
    }
}

// Pattern Registry
const pattern_def_t PATTERN_LIST[] = {
    // Spacecraft
    {"breathe",   "動力炉ブレス",     "space",    "深呼吸するように全体が優しく明滅", pattern_breathe},
    {"thruster",  "スラスター噴射",   "space",    "高出力バーニアのように激しくゆらぐ光", pattern_thruster},
    {"scanner",   "索敵スキャナー",   "space",    "左右を行き交うセンサー光線", pattern_scanner},
    {"warp",      "ワープチャージ",   "space",    "徐々に加速して一気に閃光を放つ", pattern_warp_charge},

    // Aircraft
    {"nav_strobe","航行灯＆ストロボ", "aircraft", "主翼ポジション点灯＋周期的ダブルストロボ", pattern_nav_strobe},
    {"landing",   "着陸ライト",       "aircraft", "アプローチ時のようにじわじわ最大光量へ", pattern_landing_lights},

    // Automotive
    {"hazard",    "ハザード点滅",     "auto",     "規則正しいウインカー/警告点滅", pattern_hazard},
    {"engine",    "セル始動＆アイドリング","auto","始動時のクランキングから安定燃焼へ", pattern_engine_start},
    {"alternate", "緊急パトロール",   "auto",     "2灯ずつ交互に高速点滅", pattern_alternate},

    // Special FX
    {"welding",   "溶接スパーク",     "fx",       "火花が散るような不規則なバチバチ発光", pattern_welding},
    {"glitch",    "接触不良 / ノイズ","fx",       "たまに瞬断するリアルなダメージ演出", pattern_glitch},
    {"fireflies", "ホタル同期",       "fx",       "4つの光がバラバラの波長で幻想的に揺れる", pattern_fireflies},

    // Basics & Experiments
    {"solid",     "常時点灯 (全体)",  "basic",    "ディスプレイ用。スライダーで明るさ調整", pattern_solid},
    {"manual",    "4ch個別マニュアル","manual",   "各LEDの明るさをスライダーで自由調整", pattern_manual},
    {"off",       "全消灯 (OFF)",     "basic",    "LEDを消灯します", pattern_off},
};

const int PATTERN_COUNT = sizeof(PATTERN_LIST) / sizeof(PATTERN_LIST[0]);
