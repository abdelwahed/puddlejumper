#include <jni.h>
#include <math.h>
#include <Oboe/oboe.h>
#include <kiss_fftr.h>

#define SAMPLING_RATE 48000

class FMCWSweepGenerator {

public:
    FMCWSweepGenerator(int baseband_hz, int bandwidth_hz, int duration_millis) {
        this->baseband_hz = (float)baseband_hz;
        this->bandwidth_hz = (float)bandwidth_hz;
        duration_secs = duration_millis / 1000.0f;
        t = 0;
    }

    void generate(int num_samples, float *audio_data) {
        for (int i = 0; i < num_samples; ++i) {
            float sample = sinf(
                    bandwidth_hz / duration_secs * (float) M_PI *
                    powf(t + baseband_hz / bandwidth_hz * duration_secs, 2)
            );
            audio_data[i] = sample;
            t += 1.0 / SAMPLING_RATE;
            if (t >= duration_secs) {
                t = 0;
            }
        }
    }

private:
    float t;

    float baseband_hz;
    float bandwidth_hz;
    float duration_secs;
};

