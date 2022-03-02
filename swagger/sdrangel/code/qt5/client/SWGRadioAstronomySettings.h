/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube    ---   Limitations and specifcities:    * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV and DATV demodulators, Channel Analyzer NG, LoRa demodulator   * The device settings and report structures contains only the sub-structure corresponding to the device type. The DeviceSettings and DeviceReport structures documented here shows all of them but only one will be or should be present at a time   * The channel settings and report structures contains only the sub-structure corresponding to the channel type. The ChannelSettings and ChannelReport structures documented here shows all of them but only one will be or should be present at a time    --- 
 *
 * OpenAPI spec version: 6.0.0
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */

/*
 * SWGRadioAstronomySettings.h
 *
 * RadioAstronomy
 */

#ifndef SWGRadioAstronomySettings_H_
#define SWGRadioAstronomySettings_H_

#include <QJsonObject>


#include "SWGChannelMarker.h"
#include "SWGRollupState.h"
#include <QString>

#include "SWGObject.h"
#include "export.h"

namespace SWGSDRangel {

class SWG_API SWGRadioAstronomySettings: public SWGObject {
public:
    SWGRadioAstronomySettings();
    SWGRadioAstronomySettings(QString* json);
    virtual ~SWGRadioAstronomySettings();
    void init();
    void cleanup();

    virtual QString asJson () override;
    virtual QJsonObject* asJsonObject() override;
    virtual void fromJsonObject(QJsonObject &json) override;
    virtual SWGRadioAstronomySettings* fromJson(QString &jsonString) override;

    qint64 getInputFrequencyOffset();
    void setInputFrequencyOffset(qint64 input_frequency_offset);

    qint32 getSampleRate();
    void setSampleRate(qint32 sample_rate);

    qint32 getRfBandwidth();
    void setRfBandwidth(qint32 rf_bandwidth);

    qint32 getIntegration();
    void setIntegration(qint32 integration);

    qint32 getFftSize();
    void setFftSize(qint32 fft_size);

    qint32 getFftWindow();
    void setFftWindow(qint32 fft_window);

    QString* getFilterFreqs();
    void setFilterFreqs(QString* filter_freqs);

    QString* getStarTracker();
    void setStarTracker(QString* star_tracker);

    QString* getRotator();
    void setRotator(QString* rotator);

    qint32 getRunMode();
    void setRunMode(qint32 run_mode);

    qint32 getSweepStartAtTime();
    void setSweepStartAtTime(qint32 sweep_start_at_time);

    QString* getSweepStartDateTime();
    void setSweepStartDateTime(QString* sweep_start_date_time);

    qint32 getSweepType();
    void setSweepType(qint32 sweep_type);

    float getSweep1Start();
    void setSweep1Start(float sweep1_start);

    float getSweep1Stop();
    void setSweep1Stop(float sweep1_stop);

    float getSweep1Step();
    void setSweep1Step(float sweep1_step);

    float getSweep1Delay();
    void setSweep1Delay(float sweep1_delay);

    float getSweep2Start();
    void setSweep2Start(float sweep2_start);

    float getSweep2Stop();
    void setSweep2Stop(float sweep2_stop);

    float getSweep2Step();
    void setSweep2Step(float sweep2_step);

    float getSweep2Delay();
    void setSweep2Delay(float sweep2_delay);

    qint32 getRgbColor();
    void setRgbColor(qint32 rgb_color);

    QString* getTitle();
    void setTitle(QString* title);

    qint32 getStreamIndex();
    void setStreamIndex(qint32 stream_index);

    qint32 getUseReverseApi();
    void setUseReverseApi(qint32 use_reverse_api);

    QString* getReverseApiAddress();
    void setReverseApiAddress(QString* reverse_api_address);

    qint32 getReverseApiPort();
    void setReverseApiPort(qint32 reverse_api_port);

    qint32 getReverseApiDeviceIndex();
    void setReverseApiDeviceIndex(qint32 reverse_api_device_index);

    qint32 getReverseApiChannelIndex();
    void setReverseApiChannelIndex(qint32 reverse_api_channel_index);

    SWGChannelMarker* getChannelMarker();
    void setChannelMarker(SWGChannelMarker* channel_marker);

    SWGRollupState* getRollupState();
    void setRollupState(SWGRollupState* rollup_state);


    virtual bool isSet() override;

private:
    qint64 input_frequency_offset;
    bool m_input_frequency_offset_isSet;

    qint32 sample_rate;
    bool m_sample_rate_isSet;

    qint32 rf_bandwidth;
    bool m_rf_bandwidth_isSet;

    qint32 integration;
    bool m_integration_isSet;

    qint32 fft_size;
    bool m_fft_size_isSet;

    qint32 fft_window;
    bool m_fft_window_isSet;

    QString* filter_freqs;
    bool m_filter_freqs_isSet;

    QString* star_tracker;
    bool m_star_tracker_isSet;

    QString* rotator;
    bool m_rotator_isSet;

    qint32 run_mode;
    bool m_run_mode_isSet;

    qint32 sweep_start_at_time;
    bool m_sweep_start_at_time_isSet;

    QString* sweep_start_date_time;
    bool m_sweep_start_date_time_isSet;

    qint32 sweep_type;
    bool m_sweep_type_isSet;

    float sweep1_start;
    bool m_sweep1_start_isSet;

    float sweep1_stop;
    bool m_sweep1_stop_isSet;

    float sweep1_step;
    bool m_sweep1_step_isSet;

    float sweep1_delay;
    bool m_sweep1_delay_isSet;

    float sweep2_start;
    bool m_sweep2_start_isSet;

    float sweep2_stop;
    bool m_sweep2_stop_isSet;

    float sweep2_step;
    bool m_sweep2_step_isSet;

    float sweep2_delay;
    bool m_sweep2_delay_isSet;

    qint32 rgb_color;
    bool m_rgb_color_isSet;

    QString* title;
    bool m_title_isSet;

    qint32 stream_index;
    bool m_stream_index_isSet;

    qint32 use_reverse_api;
    bool m_use_reverse_api_isSet;

    QString* reverse_api_address;
    bool m_reverse_api_address_isSet;

    qint32 reverse_api_port;
    bool m_reverse_api_port_isSet;

    qint32 reverse_api_device_index;
    bool m_reverse_api_device_index_isSet;

    qint32 reverse_api_channel_index;
    bool m_reverse_api_channel_index_isSet;

    SWGChannelMarker* channel_marker;
    bool m_channel_marker_isSet;

    SWGRollupState* rollup_state;
    bool m_rollup_state_isSet;

};

}

#endif /* SWGRadioAstronomySettings_H_ */