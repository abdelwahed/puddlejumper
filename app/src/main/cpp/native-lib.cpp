#include <jni.h>
#include <math.h>
#include <Oboe/oboe.h>
#include <kiss_fftr.h>
#include <android/log.h>
#include <mutex>
#include <condition_variable>

#define SAMPLING_RATE 48000
#define NFFT 1024
#define NFREQS (NFFT/2 + 1)
#define SAMPLES_PER_CALLBACK 1024

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

SpectrogramListener *listener = NULL;
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
    listener = new SpectrogramListener();
    listener->start();
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
    FMCWTransmitter() : fmcw(10000, 5000, 200) {}

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
