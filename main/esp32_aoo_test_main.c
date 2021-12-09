// test app for using AOO library on ESP32 board

#include "ethernet_basic.h"

#include "aoo/aoo_source.h"
#include "aoo/aoo_sink.h"
#include "aoo/codec/aoo_pcm.h"
#define AOO_OPUS_MULTISTREAM_H "opus/include/opus_multistream.h"
#include "aoo/codec/aoo_opus.h"

#include <stdio.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "soc/rtc_wdt.h"

#define SOURCE_PORT 9998
#define SOURCE_ID 0
#define SINK_PORT 9999
#define SINK_ID 0
#define SINK_BUFSIZE 100

// "native" settings
#define SAMPLERATE 44100
#define BLOCKSIZE 256
#define CHANNELS 1

// format settings (to test reblocking/resampling)
#define FORMAT_SAMPLERATE 44100
#define FORMAT_BLOCKSIZE 256
#define FORMAT_CHANNELS 1

// we send/receive NUMBLOCKS * NUMLOOPS blocks

// number of blocks to send in a row
// to let AOO sink jitter buffer fill up
#define NUMBLOCKS 8
// to test long term stability
#define NUMLOOPS 8

#define CODEC_PCM
// #define CODEC_OPUS

AooSample input[CHANNELS][BLOCKSIZE * NUMBLOCKS];
AooSample output[CHANNELS][BLOCKSIZE * NUMBLOCKS];

AooSource *source;
AooSink *sink;

AooInt32 AOO_CALL mySendFunction(
        void *user, const AooByte *data, AooInt32 size,
        const void *address, AooAddrSize addrlen, AooFlag flag)
{
    // usually, you would send the packet to the specified
    // socket address. here we just pass it directly to the source/sink.
    if (user == source) {
        AooSource_handleMessage(user, data, size, address, addrlen);
    } else if (user == sink) {
        AooSink_handleMessage(user, data, size, address, addrlen);
    } else {
        printf("mySendFunction: bug\n");
    }

    // usually, you would return the result of the send() function.
    return 0;
}

void AOO_CALL myEventHandler(
        void *user, const AooEvent *event, AooThreadLevel level)
{
    printf("[event] %s: ", user == source ? "AooSource" : "AooSink");
    switch (event->type)
    {
    case kAooEventPing:
    {
        AooEventPing *ping = (AooEventPing *)event;
        AooSeconds latency = aoo_ntpTimeDuration(ping->t1, ping->t2);
        printf("got ping (latency: %f ms)\n", latency * 1000.0);
        break;
    }
    case kAooEventPingReply:
    {
        AooEventPingReply *ping = (AooEventPingReply *)event;
        AooSeconds latency = aoo_ntpTimeDuration(ping->t2, ping->t3);
        AooSeconds rtt = aoo_ntpTimeDuration(ping->t1, ping->t3);
        printf("got ping reply (latency: %f ms, rtt: %f ms)\n",
               latency * 1000.0, rtt * 1000);
        break;
    }
    case kAooEventSourceAdd:
        printf("source added\n");
        break;
    case kAooEventStreamStart:
        printf("stream started\n");
        break;
    case kAooEventStreamStop:
        printf("stream stopped\n");
        break;
    case kAooEventStreamState:
    {
        AooEventStreamState *state = (AooEventStreamState *)event;
        const char *label = state->state == kAooStreamStateActive ?
                    "active" : "inactive";
        printf("stream state changed to %s\n", label);
        break;
    }
    // handle other events
    default:
        printf("other\n");
        break;
    }
}

void myLogFunction(AooLogLevel level, const AooChar *msg, ...)
{
    const char *label;
    switch (level) {
    case kAooLogLevelError:
        label = "error";
        break;
    case kAooLogLevelWarning:
        label = "warning";
        break;
    case kAooLogLevelVerbose:
        label = "verbose";
        break;
    case kAooLogLevelDebug:
        label = "debug";
        break;
    default:
        label = "aoo";
        break;
    }
    printf("[%s] %s\n", label, msg);
}

void sleep_millis(int ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void app_main(void)
{
    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    sleep_millis(100);

    printf("try to initialize ethernet...\n");
    eth_initialize();

    sleep_millis(100);

    printf("try to aoo_initialize()\n");

    aoo_initializeEx(myLogFunction, NULL);

    printf("create input signal\n");
    for (int i = 0; i < (NUMBLOCKS * BLOCKSIZE); ++i) {
        AooSample value = sin((AooSample)i / BLOCKSIZE);
        for (int j = 0; j < CHANNELS; ++j) {
            input[j][i] = value;
        }
    }

    printf("setup socket addresses\n");
    struct sockaddr_in source_addr;
    source_addr.sin_family = AF_INET;
    source_addr.sin_port = htons(SOURCE_PORT);
    source_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    struct sockaddr_in sink_addr;
    sink_addr.sin_family = AF_INET;
    sink_addr.sin_port = htons(SINK_PORT);
    sink_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sleep_millis(1000);

    printf("\n");

    printf("create AooSource\n");
    AooError e;
    source = AooSource_new(SOURCE_ID, 0, &e);
    if (!source) {
        printf("couldn't create: %s\n", aoo_strerror(e));
        goto restart;
    }
    printf("AooSource: set event handler\n");
    AooSource_setEventHandler(source, myEventHandler, source, kAooEventModePoll);
    printf("AooSource: set xrun detection\n");
    AooSource_setXRunDetection(source, kAooFalse);
    printf("AooSource: set dynamic resampling\n");
    AooSource_setDynamicResampling(source, kAooFalse);
    printf("AooSource: set buffer size\n");
    AooSource_setBufferSize(source, 0.025);
    printf("AooSource: set resend buffer size\n");
    AooSource_setResendBufferSize(source, 0);
    printf("AooSource: setup\n");
    AooSource_setup(source, SAMPLERATE, BLOCKSIZE, CHANNELS);
    printf("AooSource: add sink\n");
    AooEndpoint ep;
    ep.address = &sink_addr;
    ep.addrlen = sizeof(sink_addr);
    ep.id = SINK_ID;
    AooSource_addSink(source, &ep, kAooSinkActive);

    printf("\n");

    printf("create AooSink\n");
    sink = AooSink_new(SINK_ID, 0, &e);
    if (!sink) {
        printf("couldn't create: %s\n", aoo_strerror(e));
        goto restart;
    }
    printf("AooSink: set event handler\n");
    AooSink_setEventHandler(sink, myEventHandler, source, kAooEventModePoll);
    printf("AooSink: set dynamic resampling\n");
    AooSink_setDynamicResampling(sink, kAooFalse);
    printf("AooSink: set xrun detection\n");
    AooSink_setXRunDetection(sink, kAooFalse);
    printf("AooSink: set buffer size\n");
    AooSink_setBufferSize(sink, SINK_BUFSIZE * 0.001);
    printf("AooSink: set resend data\n");
    AooSink_setResendData(sink, kAooFalse);
    printf("AooSink: setup\n");
    AooSink_setup(sink, SAMPLERATE, BLOCKSIZE, CHANNELS);

    printf("\n");

    printf("AooSource: set format\n");
#ifdef CODEC_PCM
    AooFormatPcm format;
    AooFormatPcm_init(&format, FORMAT_CHANNELS, FORMAT_SAMPLERATE,
                      FORMAT_BLOCKSIZE, kAooPcmFloat32);
#endif
#ifdef CODEC_OPUS
    AooFormatOpus format;
    AooFormatOpus_init(&format, FORMAT_CHANNELS, FORMAT_SAMPLERATE,
                       FORMAT_BLOCKSIZE, OPUS_APPLICATION_AUDIO);
#endif
    AooSource_setFormat(source, &format.header);

    printf("AooSource: start stream\n");
    AooSource_startStream(source, NULL);

    printf("\n");

    for (int k = 0; k < NUMLOOPS; ++k) {
        printf("# loop iteration %d\n\n", k);

        printf("send audio\n---\n");
        for (int i = 0; i < NUMBLOCKS; ++i) {
            printf("send block %d\n", i);
            AooSample *inChannels[CHANNELS];
            for (int i = 0; i < CHANNELS; ++i) {
                inChannels[i] = input[i] + (i * BLOCKSIZE);
            }
            AooNtpTime t = aoo_getCurrentNtpTime();
            AooSource_process(source, inChannels, BLOCKSIZE, t);
            AooSource_send(source, mySendFunction, sink);
            printf("---\n");
        }

        printf("\n");

        printf("receive audio\n---\n");
        for (int i = 0; i < NUMBLOCKS; ++i) {
            printf("receive block %d\n", i);
            AooSample *outChannels[CHANNELS];
            for (int i = 0; i < CHANNELS; ++i) {
                outChannels[i] = output[i] + (i * BLOCKSIZE);
            }
            AooNtpTime t = aoo_getCurrentNtpTime();
            AooSink_process(sink, outChannels, BLOCKSIZE, t);
            AooSink_send(sink, mySendFunction, source);
            AooSink_pollEvents(sink);
            printf("---\n");
        }

        printf("AooSource: poll events\n");
        AooSource_pollEvents(source);

        printf("\n");
    }

restart:
// ignore free AooSource because results in exception crash
//    if (source) {
//        printf("free AooSource\n");
//        AooSource_free(source);
//    }
    printf("\n");
    if (sink) {
        printf("free AooSink\n");
        AooSink_free(sink);
    }
    printf("\n");

    printf("aoo_terminate()\n");
    aoo_terminate();

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        sleep_millis(1000);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
