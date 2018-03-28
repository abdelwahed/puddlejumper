#include <jni.h>
#include <math.h>
#include <Oboe/oboe.h>
#include <kiss_fftr.h>
#include <android/log.h>
#include <mutex>
#include <condition_variable>
#include <vector>

#define SAMPLING_RATE 48000
#define SAMPLES_PER_CALLBACK 1024

#define FMCW_BASEBAND 10000 // hertz
#define FMCW_BANDWIDTH 6400 // hertz
#define FMCW_DURATION 20 // milliseconds
#define FMCW_DURATION_SAMPLES (SAMPLING_RATE * FMCW_DURATION / 1000)

#define NFFT FMCW_DURATION_SAMPLES
#define NFREQS (NFFT/2 + 1)

#define PILOT_WIDTH (FMCW_DURATION_SAMPLES/16)

#define FMCW_BASEBAND_BIN (FMCW_BASEBAND * NFFT / SAMPLING_RATE)
#define FMCW_BINWIDTH (FMCW_BANDWIDTH * NFFT / SAMPLING_RATE)

class FMCWSweepGenerator {

public:
    FMCWSweepGenerator(int baseband_hz, int bandwidth_hz, int duration_millis) {
        this->baseband_hz = (float)baseband_hz;
        this->bandwidth_hz = (float)bandwidth_hz;

        duration_secs = duration_millis / 1000.0f;
        t_secs = 0;

        duration_samples = SAMPLING_RATE * duration_millis / 1000;
        t_samples = 0;
    }

    void generate(int num_samples, float *audio_data) {
        for (int i = 0; i < num_samples; ++i) {
            float sample;
            if (resting) {
                sample = 0;
            } else {
                sample = sinf(
                        bandwidth_hz / duration_secs * (float) M_PI *
                        powf(t_secs + baseband_hz / bandwidth_hz * duration_secs, 2)
                );
            }
            audio_data[i] = sample;
            t_secs += 1.0 / SAMPLING_RATE;
            t_samples++;

            if (t_samples >= duration_samples) {
                t_samples = 0;
                t_secs = 0;
                resting = !resting;
            }
        }
    }

private:
    float baseband_hz;
    float bandwidth_hz;
    float duration_secs;
    float t_secs;

    int duration_samples;
    int t_samples;

    bool resting = false;
};


class Transceiver : public oboe::AudioStreamCallback {
public:
    Transceiver(oboe::Direction direction) {
        oboe::AudioStreamBuilder builder;
        builder.setAudioApi(oboe::AudioApi::OpenSLES);
        builder.setDirection(direction);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(1);
        builder.setFramesPerCallback(SAMPLES_PER_CALLBACK);
        builder.setCallback(this);

        oboe::Result res;
        res = builder.openStream(&stream);
        check_error(res, "openStream");
    }

    ~Transceiver() {
        if(stream) {
            stream->close();
        }
    }

    void start() {
        check_error(
                stream->requestStart(),
                "requestStart"
        );
    }

    void stop() {
        check_error(
                stream->requestStop(),
                "requestStop"
        );
    }

    virtual oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void*, int32_t) = 0;

private:
    oboe::AudioStream *stream;

    void check_error(oboe::Result res, const char* msg) {
        if(res != oboe::Result::OK) {
            __android_log_print(
                    ANDROID_LOG_ERROR,
                    "PuddleJumper::Transceiver",
                    "%s failed with error: %s",
                    msg, oboe::convertToText(res)
            );
            abort();
        }
    }
};

class Receiver : public Transceiver {
public:
    Receiver() : Transceiver(oboe::Direction::Input) {}
};

class Transmitter : public Transceiver {
public:
    Transmitter() : Transceiver(oboe::Direction::Output) {}
};


class FMCWListener : public Receiver {
public:
    FMCWListener() : fmcw(FMCW_BASEBAND, FMCW_BANDWIDTH, FMCW_DURATION) {
        cfg = kiss_fftr_alloc(NFFT, 0, 0, 0);
        fmcw.generate(PILOT_WIDTH, pilot);
    }

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *stream, void *audioData, int32_t numFrames) {
        float *fAudioData = (float*)audioData;

        if(sweepOffset == -1) {
            // detect pilot sequence

            float maxSim = 0;
            int offset = 0;

            // loop over offsets
            for(int i = 0; i < numFrames - PILOT_WIDTH; ++i) {

                // loop over coordinates
                float sim = 0;
                float dot = 0;
                float a_ssq = 0;
                float b_ssq = 0;
                for(int j = 0; j < PILOT_WIDTH;  ++j) {
                    dot += pilot[j] * fAudioData[i + j];
                    a_ssq += powf(pilot[j], 2);
                    b_ssq += powf(fAudioData[i + j], 2);
                }
                float norm = sqrtf(a_ssq) * sqrtf(b_ssq);
                if (norm > 0) {
                    sim = dot / norm;
                }

                // store offset if beats maximum
                if(sim > maxSim) {
                    maxSim = sim;
                    offset = i;
                }

            }
            if(maxSim > 0.5) {
                sweepOffset = offset;
                __android_log_print(
                        ANDROID_LOG_DEBUG,
                        "PuddleJumper",
                        "sweep detected with similarity %f at offset %d",
                        maxSim, sweepOffset
                );
            }
        }

        if(sweepOffset != -1) {
            // pilot sequence detected, collect sweep

            while(sweepOffset < SAMPLES_PER_CALLBACK) {
                sweepBuffer[t_samples] = fAudioData[sweepOffset];

                sweepOffset++;
                t_samples++;

                // full sweep collected: process
                if(t_samples == FMCW_DURATION_SAMPLES) {

                    if(!resting) {
                        kiss_fftr(cfg, sweepBuffer, frequencies);
                        magnitude_lock.lock();
                        for(int i = FMCW_BASEBAND_BIN, j = 0;
                            j < FMCW_BINWIDTH; i++, j++) {

                            magnitudes[j] = sqrtf(
                                    powf(frequencies[i].i, 2) +
                                    powf(frequencies[i].r, 2)
                            );
                        }
                        magnitude_lock.unlock();
                    }

                    t_samples = 0;
                    resting = !resting;
                }

            }

            sweepOffset = 0;
        }

        return oboe::DataCallbackResult::Continue;
    }

    jfloatArray get_magnitudes(JNIEnv * env) {
        jfloatArray res = env->NewFloatArray(FMCW_BINWIDTH);
        magnitude_lock.lock();
        env->SetFloatArrayRegion(res, 0, FMCW_BINWIDTH, magnitudes);
        magnitude_lock.unlock();
        return res;
    }

    ~FMCWListener() {
        free(cfg);
    }

private:
    kiss_fftr_cfg cfg;
    kiss_fft_cpx frequencies[NFREQS];

    FMCWSweepGenerator fmcw;
    float pilot[PILOT_WIDTH];

    int sweepOffset = -1;

    float sweepBuffer[FMCW_DURATION_SAMPLES];
    int t_samples = 0;
    bool resting = false;

    float magnitudes[FMCW_BINWIDTH] = {0};
    std::mutex magnitude_lock;
};

class Recorder : public Receiver {
public:
    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *stream, void *audioData, int32_t numFrames) {
        float *fAudioData = (float*)audioData;
        for(int i = 0; i < numFrames; ++i) {
            log.push_back(fAudioData[i]);
        }
        return oboe::DataCallbackResult::Continue;
    }

    jfloatArray recordFor(int timeout_millis, JNIEnv *env) {
        start();
        usleep(timeout_millis * 1000);
        stop();

        jsize result_len = (jsize)log.size();
        jfloatArray result = env->NewFloatArray(result_len);
        env->SetFloatArrayRegion(result, 0, result_len, log.data());
        return result;
    }

private:
    std::vector<float> log;
};

extern "C"
JNIEXPORT jfloatArray
JNICALL
Java_edu_washington_cs_puddlejumper_MainActivity_recordFor(
        JNIEnv *env, jobject, jint timeout_millis
) {
    Recorder rec;
    return rec.recordFor(timeout_millis, env);
}


class SpectrogramListener : public Receiver {

public:
    SpectrogramListener() {
        cfg = kiss_fftr_alloc(NFFT, 0, 0, 0);
    }

    ~SpectrogramListener() {
        free(cfg);
    }

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *stream, void *audioData, int32_t numFrames) {
        kiss_fftr(cfg, (float*)audioData, frequencies);
        static float local_mags[NFFT];

        for(int i = 0; i < NFREQS; ++i) {
            local_mags[i] = sqrtf(
                    powf(frequencies[i].i, 2) +
                    powf(frequencies[i].r, 2)
            );
        }

        magnitude_lock.lock();
        memcpy(magnitudes, local_mags, NFREQS * sizeof(float));
        magnitude_lock.unlock();

        return oboe::DataCallbackResult::Continue;
    }

    jfloatArray get_magnitudes(JNIEnv * env) {
        jfloatArray res = env->NewFloatArray(NFREQS);

        magnitude_lock.lock();
        env->SetFloatArrayRegion(res, 0, NFREQS, magnitudes);
        magnitude_lock.unlock();

        return res;
    }

private:
    kiss_fft_cpx frequencies[NFREQS];
    kiss_fftr_cfg cfg;

    float magnitudes[NFREQS];
    std::mutex magnitude_lock;
};

FMCWListener *listener = NULL;
std::mutex listener_lock;
std::condition_variable listener_ready;

extern "C"
JNIEXPORT void
JNICALL
Java_edu_washington_cs_puddlejumper_MainActivity_startCapture(
        JNIEnv *env, jobject
) {
    listener_lock.lock();
    if(listener) {
        return;
    }
    listener = new FMCWListener();
    listener->start();
    listener_ready.notify_one();
    listener_lock.unlock();
}

extern "C"
JNIEXPORT void
JNICALL
Java_edu_washington_cs_puddlejumper_MainActivity_stopCapture(
        JNIEnv *env, jobject
) {
    listener_lock.lock();
    if(listener) {
        listener->stop();
        listener = NULL;
    }
    listener_lock.unlock();
}

extern "C"
JNIEXPORT jfloatArray
JNICALL
Java_edu_washington_cs_puddlejumper_SpectrogramView_getMagnitudes(
        JNIEnv *env, jobject
) {
    std::unique_lock<std::mutex> lk(listener_lock);
    while(!listener) {
        listener_ready.wait(lk);
    }
    jfloatArray res = listener->get_magnitudes(env);
    lk.unlock();
    return res;
}


class FMCWTransmitter : public Transmitter {
public:
    FMCWTransmitter() : fmcw(FMCW_BASEBAND, FMCW_BANDWIDTH, FMCW_DURATION) {}

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *stream, void *audioData, int32_t numFrames) {

        fmcw.generate(numFrames, (float*)audioData);

        return oboe::DataCallbackResult::Continue;
    }

private:
    FMCWSweepGenerator fmcw;
};

FMCWTransmitter *transmitter = NULL;

extern "C"
JNIEXPORT void
JNICALL
Java_edu_washington_cs_puddlejumper_MainActivity_startFMCW(
        JNIEnv *env, jobject
) {
    if(!transmitter) {
        transmitter = new FMCWTransmitter();
        transmitter->start();
    }
}

extern "C"
JNIEXPORT void
JNICALL
Java_edu_washington_cs_puddlejumper_MainActivity_stopFMCW(
        JNIEnv *env, jobject
) {
    if(transmitter) {
        transmitter->stop();
        delete transmitter;
        transmitter = NULL;
    }
}
