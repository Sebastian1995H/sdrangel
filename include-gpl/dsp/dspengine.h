///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DSPENGINE_H
#define INCLUDE_DSPENGINE_H

#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include "dsp/dsptypes.h"
#include "dsp/fftwindow.h"
#include "dsp/samplefifo.h"
#include "audio/audiooutput.h"
#include "util/messagequeue.h"
#include "util/syncmessenger.h"
#include "util/export.h"

class SampleSource;
class SampleSink;
class AudioFifo;

class SDRANGELOVE_API DSPEngine : public QThread {
	Q_OBJECT

public:
	enum State {
		StNotStarted,  //!< engine is before initialization
		StIdle,        //!< engine is idle
		StReady,       //!< engine is ready to run
		StRunning,     //!< engine is running
		StError        //!< engine is in error
	};

	DSPEngine(QObject* parent = NULL);
	~DSPEngine();

	static DSPEngine *instance();

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }
	MessageQueue* getOutputMessageQueue() { return &m_outputMessageQueue; }

	void start(); //!< This thread start
	void stop();  //!< This thread stop

	bool initAcquisition(); //!< Initialize acquisition sequence
	bool startAcquisition(); //!< Start acquisition sequence
	void stopAcquistion();   //!< Stop acquisition sequence

	void setSource(SampleSource* source); //!< Set the unique sample source

	void addSink(SampleSink* sink); //!< Add a sample sink
	void removeSink(SampleSink* sink); //!< Remove a sample sink

	void addAudioSink(AudioFifo* audioFifo); //!< Add the audio sink
	void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

	void configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection); //!< Configure DSP corrections

	State state() const { return m_state; } //!< Return DSP engine current state

	QString errorMessage(); //!< Return the current error message
	QString sourceDeviceDescription(); //!< Return the source device description

private:
	MessageQueue m_inputMessageQueue;  //<! Input message queue. Post here.
	MessageQueue m_outputMessageQueue; //<! Output message queue. Listen here.
	SyncMessenger m_syncMessenger;     //!< Used to process messages synchronously with the thread

	State m_state;

	QString m_errorMessage;
	QString m_deviceDescription;

	SampleSource* m_sampleSource;

	typedef std::list<SampleSink*> SampleSinks;
	SampleSinks m_sampleSinks;

	AudioOutput m_audioSink;

	uint m_sampleRate;
	quint64 m_centerFrequency;

	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;
	double m_iOffset, m_qOffset;
	qint32 m_iRange;
	qint32 m_qRange;
	qint32 m_imbalance;

	void run();

	void dcOffset(SampleVector::iterator begin, SampleVector::iterator end);
	void imbalance(SampleVector::iterator begin, SampleVector::iterator end);
	void work(); //!< transfer samples from source to sinks if in running state

	State gotoIdle();     //!< Go to the idle state
	State gotoInit();     //!< Go to the acquisition init state from idle
	State gotoRunning();  //!< Go to the running state from ready state
	State gotoError(const QString& errorMsg); //!< Go to an error state

	void handleSetSource(SampleSource* source); //!< Manage source setting

private slots:
	void handleData(); //!< Handle data when samples from source FIFO are ready to be processed
	void handleInputMessages(); //!< Handle input message queue
	void handleSynchronousMessages(Message *message); //!< Handle synchronous messages with the thread
};

#endif // INCLUDE_DSPENGINE_H
