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


#include "SWGMapReport.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGMapReport::SWGMapReport(QString* json) {
    init();
    this->fromJson(*json);
}

SWGMapReport::SWGMapReport() {
    date_time = nullptr;
    m_date_time_isSet = false;
}

SWGMapReport::~SWGMapReport() {
    this->cleanup();
}

void
SWGMapReport::init() {
    date_time = new QString("");
    m_date_time_isSet = false;
}

void
SWGMapReport::cleanup() {
    if(date_time != nullptr) { 
        delete date_time;
    }
}

SWGMapReport*
SWGMapReport::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGMapReport::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&date_time, pJson["dateTime"], "QString", "QString");
    
}

QString
SWGMapReport::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGMapReport::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(date_time != nullptr && *date_time != QString("")){
        toJsonValue(QString("dateTime"), date_time, obj, QString("QString"));
    }

    return obj;
}

QString*
SWGMapReport::getDateTime() {
    return date_time;
}
void
SWGMapReport::setDateTime(QString* date_time) {
    this->date_time = date_time;
    this->m_date_time_isSet = true;
}


bool
SWGMapReport::isSet(){
    bool isObjectUpdated = false;
    do{
        if(date_time && *date_time != QString("")){
            isObjectUpdated = true; break;
        }
    }while(false);
    return isObjectUpdated;
}
}

