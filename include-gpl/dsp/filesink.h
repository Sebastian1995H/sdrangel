#ifndef INCLUDE_FILESINK_H
#define INCLUDE_FILESINK_H

#include <string>
#include <iostream>
#include <fstream>

#include <ctime>
#include "dsp/samplesink.h"
#include "util/export.h"
#include "util/message.h"
#include "util/messagequeue.h"

class SDRANGELOVE_API FileSink : public SampleSink {
public:

    struct Header
    {
        int         sampleRate;
        quint64     centerFrequency;
        std::time_t startTimeStamp;
    };

	FileSink();
	virtual ~FileSink();
    
    quint64 getByteCount() const { return m_byteCount; }

	void configure(MessageQueue* msgQueue, const std::string& filename);

	virtual bool init(const Message& cmd);
	virtual void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);
    void startRecording();
    void stopRecording();
    static void readHeader(std::ifstream& samplefile, Header& header);

private:
	class MsgConfigureFileSink : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const std::string& getFileName() const { return m_fileName; }

		static MsgConfigureFileSink* create(const std::string& fileName)
		{
			return new MsgConfigureFileSink(fileName);
		}

	private:
		std::string m_fileName;

		MsgConfigureFileSink(const std::string& fileName) :
			Message(),
			m_fileName(fileName)
		{ }
	};

	std::string m_fileName;
	int m_sampleRate;
	quint64 m_centerFrequency;
	bool m_recordOn;
    bool m_recordStart;
    std::ofstream m_sampleFile;
    quint64 m_byteCount;

	void handleConfigure(const std::string& fileName);
    void writeHeader();
};

#endif // INCLUDE_FILESINK_H
