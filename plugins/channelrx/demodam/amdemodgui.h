///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>        //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                    //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_AMDEMODGUI_H
#define INCLUDE_AMDEMODGUI_H

#include <QIcon>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "amdemodsettings.h"

class PluginAPI;
class DeviceUISet;

class AMDemod;
class BasebandSampleSink;

namespace Ui {
	class AMDemodGUI;
}

class AMDemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static AMDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
	void channelMarkerChangedByCursor();
	void channelMarkerHighlightedByCursor();

private:
	Ui::AMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	AMDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_doApplySettings;

	AMDemod* m_amDemod;
	bool m_squelchOpen;
    int m_audioSampleRate; //!< for display purposes
	bool m_samUSB;
	uint32_t m_tickCount;
	MessageQueue m_inputMessageQueue;

    QIcon m_iconDSBUSB;
    QIcon m_iconDSBLSB;

	bool m_invalidChannel;

	explicit AMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~AMDemodGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
	void calcOffset();
	void applySnap();
	void displaySnap();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
	void on_frequencyMode_currentIndexChanged(int index);
	void on_deltaFrequency_changed(qint64 value);
	void on_pll_toggled(bool checked);
	void on_ssb_toggled(bool checked);
	void on_bandpassEnable_toggled(bool checked);
	void on_rfBW_valueChanged(int value);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void on_audioMute_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect(const QPoint& p);
    void samSSBSelect(const QPoint& p);
	void snapClicked();
	void tick();
};

#endif // INCLUDE_AMDEMODGUI_H
