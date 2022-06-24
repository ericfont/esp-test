// test app for using AOO library on ESP32 board

#include "ethernet_basic.h"

#include "aoo/aoo_source.h"
#include "aoo/aoo_sink.h"
#include "aoo/codec/aoo_pcm.h"
//#define AOO_OPUS_MULTISTREAM_H "opus/include/opus_multistream.h"
//#include "aoo/codec/aoo_opus.h"

#include <stdio.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "soc/rtc_wdt.h"

#define SOURCE_PORT 9998
#define SOURCE_ID 0
#define SINK_PORT 9999
#define SINK_ID 1
#define SINK_BUFSIZE 100

// "native" settings
#define SAMPLERATE 44100
#define BLOCKSIZE 16
#define CHANNELS 1

// format settings (to test reblocking/resampling)
#define FORMAT_SAMPLERATE 44100
#define FORMAT_BLOCKSIZE 16
#define FORMAT_CHANNELS 1

// we send/receive NUMBLOCKS * NUMLOOPS blocks

// number of blocks to send in a row
// to let AOO sink jitter buffer fill up
#define NUMBLOCKS 1
// to test long term stability
#define NUMLOOPS 1

#define CODEC_PCM
// #define CODEC_OPUS

static const char *TAG = "aoo_test";

AooSample input[CHANNELS][BLOCKSIZE * NUMBLOCKS];
AooSample output[CHANNELS][BLOCKSIZE * NUMBLOCKS];

AooSource *source;
AooSink *sink;

int source_socket = -1;
int sink_socket = -1;

AooInt32 AOO_CALL mySendFunction(
        void *user, const AooByte *data, AooInt32 size,
        const void *address, AooAddrSize addrlen, AooFlag flag)
{
    struct sockaddr_in *dest_addr = (struct sockaddr_in *) address;
    unsigned char *bytesAddress = &dest_addr->sin_addr;
    ESP_LOGI(TAG, "mySendFunction: size %d, addrlen %d, flag %x, sin_family 0x%x, sin_port 0x%x, IPv4 addr %d.%d.%d.%d", size, addrlen, flag, dest_addr->sin_family, dest_addr->sin_port, bytesAddress[0], bytesAddress[1], bytesAddress[2], bytesAddress[3]);


    if (user == source) {
        //AooSource_handleMessage(user, data, size, address, addrlen);

        
    } else if (user == sink) {
        //AooSink_handleMessage(user, data, size, address, addrlen);

        int err = sendto(sink_socket, data, size, 0, (struct sockaddr *) address, addrlen);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            return -1;
        }
        ESP_LOGI(TAG, "Message sent");

  /*      while (1) {

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0) {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
            }

            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }*/
    } else {
        ESP_LOGI(TAG, "mySendFunction: bug\n");
    }

    // usually, you would return the result of the send() function.
    return 0;
}

void AOO_CALL myEventHandler(
        void *user, const AooEvent *event, AooThreadLevel level)
{
    ESP_LOGI(TAG, "[event] %s: ", user == source ? "AooSource" : "AooSink");
    switch (event->type)
    {
    case kAooEventPing:
    {
        AooEventPing *ping = (AooEventPing *)event;
        AooSeconds latency = aoo_ntpTimeDuration(ping->t1, ping->t2);
        ESP_LOGI(TAG, "got ping (latency: %f ms)\n", latency * 1000.0);
        break;
    }
    case kAooEventPingReply:
    {
        AooEventPingReply *ping = (AooEventPingReply *)event;
        AooSeconds latency = aoo_ntpTimeDuration(ping->t2, ping->t3);
        AooSeconds rtt = aoo_ntpTimeDuration(ping->t1, ping->t3);
        ESP_LOGI(TAG, "got ping reply (latency: %f ms, rtt: %f ms)\n",
               latency * 1000.0, rtt * 1000);
        break;
    }
    case kAooEventSourceAdd:
        ESP_LOGI(TAG, "source added\n");
        break;
    case kAooEventStreamStart:
        ESP_LOGI(TAG, "stream started\n");
        break;
    case kAooEventStreamStop:
        ESP_LOGI(TAG, "stream stopped\n");
        break;
    case kAooEventStreamState:
    {
        AooEventStreamState *state = (AooEventStreamState *)event;
        const char *label = state->state == kAooStreamStateActive ?
                    "active" : "inactive";
        ESP_LOGI(TAG, "stream state changed to %s\n", label);
        break;
    }
    // handle other events
    default:
        ESP_LOGI(TAG, "other\n");
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
    ESP_LOGI(TAG, "[%s] %s\n", label, msg);
}

void sleep_millis(int ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void app_main(void)
{
    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

    ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGI(TAG, "Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    sleep_millis(100);

    ESP_LOGI(TAG, "try to initialize ethernet...\n");
    eth_initialize();

    sleep_millis(100);

    ESP_LOGI(TAG, "try to aoo_initialize()\n");

    AooSettings settings;
    AooSettings_init(&settings);
    settings.logFunc = myLogFunction;
    aoo_initialize(&settings);

    ESP_LOGI(TAG, "create input signal\n");
    for (int i = 0; i < (NUMBLOCKS * BLOCKSIZE); ++i) {
        AooSample value = sin((AooSample)i / BLOCKSIZE);
        for (int j = 0; j < CHANNELS; ++j) {
            input[j][i] = value;
        }
    }

    ESP_LOGI(TAG, "setup socket addresses\n");
    struct sockaddr_in source_addr;
    source_addr.sin_family = AF_INET;
    source_addr.sin_port = htons(SOURCE_PORT);
    source_addr.sin_addr.s_addr = inet_addr("192.168.1.6");//htonl(INADDR_LOOPBACK);
    source_socket = socket(source_addr.sin_family, SOCK_DGRAM, IPPROTO_IP);
    if (source_socket < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto restart;
    }
    ESP_LOGI(TAG, "source_socket created: #%d", source_socket);

    struct sockaddr_in sink_addr;
    sink_addr.sin_family = AF_INET;
    sink_addr.sin_port = htons(SINK_PORT);
    sink_addr.sin_addr.s_addr = inet_addr("192.168.1.6");//htonl(INADDR_LOOPBACK);
    sink_socket = socket(sink_addr.sin_family, SOCK_DGRAM, IPPROTO_IP);
    if (sink_socket < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto restart;
    }
    ESP_LOGI(TAG, "sink_socket created: #%d", sink_socket);

    sleep_millis(1000);

    ESP_LOGI(TAG, "\n");

    ESP_LOGI(TAG, "create AooSource\n");
    AooError e;
    source = AooSource_new(SOURCE_ID, 0, &e);
    if (!source) {
        ESP_LOGI(TAG, "couldn't create: %s\n", aoo_strerror(e));
        goto restart;
    }
    ESP_LOGI(TAG, "AooSource: set event handler\n");
    AooSource_setEventHandler(source, myEventHandler, source, kAooEventModePoll);
    ESP_LOGI(TAG, "AooSource: set xrun detection\n");
    AooSource_setXRunDetection(source, kAooFalse);
    ESP_LOGI(TAG, "AooSource: set dynamic resampling\n");
    AooSource_setDynamicResampling(source, kAooFalse);
    ESP_LOGI(TAG, "AooSource: set buffer size\n");
    AooSource_setBufferSize(source, 0.025);
    ESP_LOGI(TAG, "AooSource: set resend buffer size\n");
    AooSource_setResendBufferSize(source, 0);
    ESP_LOGI(TAG, "AooSource: setup\n");
    AooSource_setup(source, SAMPLERATE, BLOCKSIZE, CHANNELS);
    ESP_LOGI(TAG, "AooSource: add sink\n");
    AooEndpoint ep;
    ep.address = &sink_addr;
    ep.addrlen = sizeof(sink_addr);
    ep.id = SINK_ID;
    AooSource_addSink(source, &ep, kAooSinkActive);

    ESP_LOGI(TAG, "\n");

    ESP_LOGI(TAG, "create AooSink\n");
    sink = AooSink_new(SINK_ID, 0, &e);
    if (!sink) {
        ESP_LOGI(TAG, "couldn't create: %s\n", aoo_strerror(e));
        goto restart;
    }
    ESP_LOGI(TAG, "AooSink: set event handler\n");
    AooSink_setEventHandler(sink, myEventHandler, source, kAooEventModePoll);
    ESP_LOGI(TAG, "AooSink: set dynamic resampling\n");
    AooSink_setDynamicResampling(sink, kAooFalse);
    ESP_LOGI(TAG, "AooSink: set xrun detection\n");
    AooSink_setXRunDetection(sink, kAooFalse);
    ESP_LOGI(TAG, "AooSink: set buffer size\n");
    AooSink_setBufferSize(sink, SINK_BUFSIZE * 0.001);
    ESP_LOGI(TAG, "AooSink: set resend data\n");
    AooSink_setResendData(sink, kAooFalse);
    ESP_LOGI(TAG, "AooSink: setup\n");
    AooSink_setup(sink, SAMPLERATE, BLOCKSIZE, CHANNELS);

    ESP_LOGI(TAG, "\n");

    ESP_LOGI(TAG, "AooSource: set format\n");
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

    ESP_LOGI(TAG, "AooSource: start stream\n");
    AooSource_startStream(source, NULL);

    ESP_LOGI(TAG, "\n");

    for (int k = 0; k < NUMLOOPS; ++k) {
        ESP_LOGI(TAG, "# loop iteration %d\n\n", k);

        ESP_LOGI(TAG, "send audio\n---\n");
        for (int i = 0; i < NUMBLOCKS; ++i) {
            ESP_LOGI(TAG, "send block %d\n", i);
            AooSample *inChannels[CHANNELS];
            for (int i = 0; i < CHANNELS; ++i) {
                inChannels[i] = input[i] + (i * BLOCKSIZE);
            }
            AooNtpTime t = aoo_getCurrentNtpTime();
            ESP_LOGI(TAG, "calling AooSource_process");
            AooSource_process(source, inChannels, BLOCKSIZE, t);
            ESP_LOGI(TAG, "calling AooSource_send");
            AooSource_send(source, mySendFunction, sink);
            ESP_LOGI(TAG, "---\n");
        }

        ESP_LOGI(TAG, "\n");

        ESP_LOGI(TAG, "receive audio\n---\n");
        for (int i = 0; i < NUMBLOCKS; ++i) {
            ESP_LOGI(TAG, "receive block %d\n", i);
            AooSample *outChannels[CHANNELS];
            for (int i = 0; i < CHANNELS; ++i) {
                outChannels[i] = output[i] + (i * BLOCKSIZE);
            }
            AooNtpTime t = aoo_getCurrentNtpTime();
            AooSink_process(sink, outChannels, BLOCKSIZE, t);
            AooSink_send(sink, mySendFunction, source);
            AooSink_pollEvents(sink);
            ESP_LOGI(TAG, "---\n");
        }

        ESP_LOGI(TAG, "AooSource: poll events\n");
        AooSource_pollEvents(source);

        ESP_LOGI(TAG, "\n");
    }

restart:
    if (source) {
        if (source_socket >= 0) {
            ESP_LOGI(TAG, "Shutting down source_socket...");
            shutdown(source_socket, 0);
            close(source_socket);
        }
        ESP_LOGI(TAG, "free AooSource\n");
        AooSource_free(source);
    }
    ESP_LOGI(TAG, "\n");
    if (sink) {
        if (sink_socket >= 0) {
            ESP_LOGI(TAG, "Shutting down sink_socket...");
            shutdown(sink_socket, 0);
            close(sink_socket);
        }
        ESP_LOGI(TAG, "free AooSink\n");
        AooSink_free(sink);
    }
    ESP_LOGI(TAG, "\n");

    ESP_LOGI(TAG, "aoo_terminate()\n");
    aoo_terminate();

    for (int i = 10; i >= 0; i--) {
        ESP_LOGI(TAG, "Restarting in %d seconds...\n", i);
        sleep_millis(1000);
    }
    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
