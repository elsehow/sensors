#include <ESP.h>

ASCIISerialStream stream(0, 9600, 3);
GestureRecognitionPipeline pipeline;
Calibrator calibrator;
TcpOStream oStream("localhost", 5204, 3, "l", "r", " ");

float analogReadToVoltage(float input)
{
    return input / 1024.0 * 5.0;
}

float normalizeADXL335(float input)
{
    return (analogReadToVoltage(input) - 1.66) / 0.333;
}

float normalizeArduino101(float input)
{
    return input / 4096;
}

// accelerometer characteristics to be calculated from calibration
double zeroG, oneG;

vector<double> processAccelerometerData(vector<double> input)
{
    if (input.size() < 3) return input;

    vector<double> output(input.size());

    output[0] = (input[0] - zeroG) / (oneG - zeroG);
    output[1] = (input[1] - zeroG) / (oneG - zeroG);
    output[2] = (input[2] - zeroG) / (oneG - zeroG);

    return output;
}

void restingDataCollected(const MatrixDouble& data)
{
    std::vector<float> mean(3, 0.0);

    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < data.getNumRows(); i++)
        {
            mean[j] += data[i][j];
        }
        mean[j] /= data.getNumRows();
    }

    // TODO: give warning if mean[0] (X acceleration) and mean[1] (Y accleration) are different.
    zeroG = (mean[0] + mean[1]) / 2; // take average of X and Y acceleration as the zero G value
    oneG = mean[2]; // use Z acceleration as one G value (due to gravity)
}

int timeout = 500;
double null_rej = 5.0;

void setup()
{
    stream.useNormalizer(normalizeADXL335);
    stream.setLabelsForAllDimensions({"x", "y", "z"});
    useStream(stream);

    calibrator.setCalibrateFunction(processAccelerometerData);
    // The elaborate version is:
    // CalibrateProcess cp("Resting", "Rest accelerometer on flat surface, w/ z-axis vertical.", restingDataCollected);
    // calibrator.addCalibrateProcess(cp);
    calibrator.addCalibrateProcess("Resting", "Rest accelerometer on flat surface, w/ z-axis vertical.", restingDataCollected);
    useCalibrator(calibrator);
    
    pipeline.addFeatureExtractionModule(TimeDomainFeatures(10, 1, 3, false, true, true, false, false));
    pipeline.setClassifier(ANBC(false, true, null_rej)); // use scaling, use null rejection, null rejection parameter
    // null rejection parameter is multiplied by the standard deviation to determine
    // the rejection threshold. the higher the number, the looser the filter; the
    // lower the number, the tighter the filter.

    pipeline.addPostProcessingModule(ClassLabelTimeoutFilter(timeout));
    usePipeline(pipeline);
    
    registerTuneable(timeout, 10, 1000, "Timeout",
                     "The longer, more filtering effect on the result");
    registerTuneable(null_rej, 0.1, 20.0,
                     "null rejection",
                     "null rejection parameter is multiplied by the standard deviation"
                     " to determine the rejection threshold. "
                     "The higher the number, the looser the filter; "
                     "the lower the number, the tighter the filter.");

    useOStream(oStream);
}