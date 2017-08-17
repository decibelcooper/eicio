#include <fcntl.h>
#include <string.h>
#include <iostream>

#include "event.h"
#include "reader.h"

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace google::protobuf;

eicio::Reader::Reader(std::string filename) {
    stream = NULL;
    inputStream = NULL;

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd != -1) {
        auto fileStream = new io::FileInputStream(fd);
        fileStream->SetCloseOnDelete(true);
        inputStream = fileStream;

        string gzipSuffix = ".gz";
        int sfxLength = gzipSuffix.length();
        if (filename.length() > sfxLength) {
            if (filename.compare(filename.length() - sfxLength, sfxLength, gzipSuffix) == 0) {
                inputStream = new io::GzipInputStream(inputStream);
            }
        }

        stream = new io::CodedInputStream(inputStream);
    }
}

eicio::Reader::~Reader() {
    if (stream) delete stream;
    if (inputStream) delete inputStream;
}

eicio::Event *eicio::Reader::Get() {  // TODO: figure out error handling for this
    if (!stream) return NULL;

    uint32 n;
    if ((n = syncToMagic()) < 4) {
        return NULL;
    }

    uint32 headerSize;
    if (!stream->ReadLittleEndian32(&headerSize)) return NULL;
    uint32 payloadSize;
    if (!stream->ReadLittleEndian32(&payloadSize)) return NULL;

    auto header = new eicio::EventHeader;
    auto headerBuf = new unsigned char[headerSize];  // TODO: This is only temporary.  Move the low-level
                                                     // stuff to an input stream class that inherits from
                                                     // io::*.
                                                     // CodedInputStream is currently only being used for
                                                     // ReadRaw()!
    if (!stream->ReadRaw(headerBuf, headerSize) || !header->ParseFromArray(headerBuf, headerSize)) {
        delete header;
        delete[] headerBuf;
        return Get();  // Indefinitely attempt to resync to magic numbers
    }
    delete[] headerBuf;

    auto event = new Event;
    event->SetHeader(header);
    auto *payload = (unsigned char *)event->SetPayloadSize(payloadSize);
    if (!stream->ReadRaw(payload, payloadSize)) {
        delete event;
        return NULL;
    }

    return event;
}

uint32 eicio::Reader::syncToMagic() {
    unsigned char num;
    uint32 nRead = 0;

    while (stream->ReadRaw(&num, 1)) {
        nRead++;

        if (num == magicBytes[0]) {
            bool goodSeq = true;

            for (int i = 1; i < 4; i++) {
                if (!stream->ReadRaw(&num, 1)) break;
                nRead++;

                if (num != magicBytes[i]) {
                    goodSeq = false;
                    break;
                }
            }
            if (goodSeq) break;
        }
    }
    return nRead;
}