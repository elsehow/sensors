#include "istream.h"
#include "ofApp.h"

#include <chrono>         // std::chrono::milliseconds
#include <thread>         // std::this_thread::sleep_for

void useStream(IStream &stream) {
    ((ofApp *) ofGetAppPtr())->useStream(stream);
}

void usePipeline(GRT::GestureRecognitionPipeline &pipeline) {
    ((ofApp *) ofGetAppPtr())->usePipeline(pipeline);
}

IStream::IStream() : has_started_(false), data_ready_callback_(nullptr) {}

vector<double> IStream::normalize(vector<double> input) {
    if (vectorNormalizer_ != nullptr) {
        return vectorNormalizer_(input);
    } else if (normalizer_ != nullptr) {
        vector<double> output;
        std::transform(input.begin(), input.end(), back_inserter(output), normalizer_);
        return output;
    } else {
        return input;
    }
}

void IStream::setLabelsForAllDimensions(const vector<string> labels) {
    istream_labels_ = labels;
}

void IStream::setLabelsForAllDimensions(std::initializer_list<string> list) {
    if (list.size() != getNumOutputDimensions()) { return; }
    vector<string> labels(list);
    istream_labels_ = labels;
}

const vector<string>& IStream::getLabels() const {
    return istream_labels_;
}

AudioStream::AudioStream(uint32_t downsample_rate)
        : downsample_rate_(downsample_rate),
          sound_stream_(new ofSoundStream()) {
    sound_stream_->setup(this, 0, 2,
                         kOfSoundStream_SamplingRate,
                         kOfSoundStream_BufferSize,
                         kOfSoundStream_nBuffers);
    sound_stream_->stop();
}

void AudioStream::start() {
    if (!has_started_) {
        sound_stream_->start();
        has_started_ = true;
    }
}

void AudioStream::stop() {
    if (has_started_) {
        sound_stream_->stop();
        has_started_ = false;
    }
}

int AudioStream::getNumInputDimensions() {
    return 1; // set by the call to sound_stream->setup() above
}

void AudioStream::audioIn(float* input, int buffer_size, int nChannel) {
    // set nChannel as 1 to load only a single channel (left).
    nChannel = 1;
    GRT::MatrixDouble data(buffer_size / nChannel / downsample_rate_, nChannel);

    for (int i = 0; i < buffer_size / nChannel / downsample_rate_; i++)
        for (int j = 0; j < nChannel; j++)
            data[i][j] = input[i * nChannel * downsample_rate_ + j];

    if (data_ready_callback_ != nullptr) {
        data_ready_callback_(data);
    }
}

SerialStream::SerialStream(uint32_t port, uint32_t baud = 115200)
        : port_(port), baud_(baud), serial_(new ofSerial()) {
    // Print all devices for convenience.
    // serial_->listDevices();
}

void SerialStream::start() {
    if (port_ == -1) {
        ofLog(OF_LOG_ERROR) << "USB Port has not been properly set";
    }

    if (!has_started_) {
        serial_->setup(port_, baud_);
        reading_thread_.reset(new std::thread(&SerialStream::readSerial, this));
        has_started_ = true;
    }
}

void SerialStream::stop() {
    has_started_ = false;
    if (reading_thread_ != nullptr && reading_thread_->joinable()) {
        reading_thread_->join();
    }
}

int SerialStream::getNumInputDimensions() {
    return 1;
}

void SerialStream::readSerial() {
    // TODO(benzh) This readSerial is running in a different thread
    // and performing a busy polling (100% CPU usage). Should be
    // optimized.
    int sleep_time = kBufferSize_ * 1000 / (baud_ / 10);
    ofLog() << "Serial port will be read every " << sleep_time << " ms";
    while (has_started_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

        int local_buffer_size = kBufferSize_;
        int bytesRequired = kBufferSize_;
        unsigned char bytes[bytesRequired];
        int bytesRemaining = bytesRequired;
        while (bytesRemaining > 0) {
            // check for data
            if (serial_->available() > 0) {
                // try to read - note offset into the bytes[] array, this is so
                // that we don't overwrite the bytes we already have
                int bytesArrayOffset = bytesRequired - bytesRemaining;
                int result = serial_->readBytes(&bytes[bytesArrayOffset],
                                                bytesRemaining);

                // check for error code
                if (result == OF_SERIAL_ERROR) {
                    // something bad happened
                    ofLog(OF_LOG_ERROR) << "Error reading from serial";
                    break;
                } else if (result == OF_SERIAL_NO_DATA) {
                    // nothing was read, try again
                } else {
                    // we read some data!
                    bytesRemaining -= result;
                }
            }
        }
        GRT::MatrixDouble data(local_buffer_size, 1);
        for (int i = 0; i < local_buffer_size; i++) {
            int b = bytes[i];
            data[i][0] = (normalizer_ != nullptr) ? normalizer_(b) : b;
        }
        if (data_ready_callback_ != nullptr) {
            data_ready_callback_(data);
        }
    }
}

ASCIISerialStream::ASCIISerialStream(uint32_t port, uint32_t baud, uint32_t dim)
        : serial_(new ofSerial()), port_(port), baud_(baud), numDimensions_(dim) {
    // Print all devices for convenience.
    //serial_->listDevices();
}

void ASCIISerialStream::start() {
    if (port_ == -1) {
        ofLog(OF_LOG_ERROR) << "USB Port has not been properly set";
    }

    if (!has_started_) {
        serial_->setup(port_, baud_);
        reading_thread_.reset(new std::thread(&ASCIISerialStream::readSerial, this));
        has_started_ = true;
    }
}

void ASCIISerialStream::stop() {
    has_started_ = false;
    if (reading_thread_ != nullptr && reading_thread_->joinable()) {
        reading_thread_->join();
    }
}

int ASCIISerialStream::getNumInputDimensions() {
    return numDimensions_;
}

void ASCIISerialStream::readSerial() {
    // TODO(benzh) This readSerial is running in a different thread
    // and performing a busy polling (100% CPU usage). Should be
    // optimized.
    int sleep_time = 10;
    ofLog() << "Serial port will be read every " << sleep_time << " ms";
    while (has_started_) {
        string s;

        do {
            while (serial_->available() < 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
            }
            s += serial_->readByte();
        } while(s[s.length() - 1] != '\n');

        //ofLog() << "read: '" << s << "'" << endl;

        if (data_ready_callback_ != nullptr) {
            istringstream iss(s);
            vector<double> data;
            double d;

            while (iss >> d) data.push_back(d);

            if (data.size() > 0) {
                data = normalize(data);

                GRT::MatrixDouble matrix;
                matrix.push_back(data);

                data_ready_callback_(matrix);
            }
        }
    }
}

FirmataStream::FirmataStream(uint32_t port) : port_(port) {
    ofSerial serial;
    serial.listDevices();
}

void FirmataStream::useAnalogPin(int i) {
    pins_.push_back(i);
};

void FirmataStream::start() {
    if (port_ == -1) {
        ofLog(OF_LOG_ERROR) << "USB Port has not been properly set";
        return;
    }

    if (pins_.size() == 0) {
        ofLog(OF_LOG_ERROR) << "Pin has not been properly set";
        return;
    }


    if (!has_started_) {
        ofSerial serial;
        configured_arduino_ = false;
        arduino_.connect(serial.getDeviceList()[port_].getDevicePath());
        update_thread_.reset(new std::thread(&FirmataStream::update, this));
        has_started_ = true;
    }
}

void FirmataStream::stop() {
    has_started_ = false;
    if (update_thread_ != nullptr && update_thread_->joinable()) {
        update_thread_->join();
    }
}

int FirmataStream::getNumInputDimensions() {
    return pins_.size();
}

void FirmataStream::update() {
    int sleep_time = 10;
    ofLog() << "Serial port will be read every " << sleep_time << " ms";
    while (has_started_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        arduino_.update();

        if (configured_arduino_) {
            vector<double> data(pins_.size());
            for (int i = 0; pins_.size(); i++)
                data[i] = arduino_.getAnalog(pins_[i]);
            data = normalize(data);
            GRT::MatrixDouble matrix;
            matrix.push_back(data);
            if (data_ready_callback_ != nullptr) data_ready_callback_(matrix);
        } else if (arduino_.isInitialized()) {
            ofLog() << "Configuring Arduino.";
            for (int i = 0; i < pins_.size(); i++)
                arduino_.sendAnalogPinReporting(pins_[i], ARD_ON);
            configured_arduino_ = true;
        }
    }
}
