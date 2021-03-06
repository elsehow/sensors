#include <ESP.h>

// Normalize by dividing each dimension by the total magnitude.
// Also add the magnitude as an additional feature.
vector<double> normalize(vector<double> input) {
    double magnitude = 0.0;

    for (int i = 0; i < input.size(); i++) magnitude += (input[i] * input[i]);
    magnitude = sqrt(magnitude);
    for (int i = 0; i < input.size(); i++) input[i] /= magnitude;

    return input;
}

ASCIISerialStream stream(0, 9600, 3);
GestureRecognitionPipeline pipeline;
TcpOStream oStream("localhost", 5204, 3, "l", "r", " ");

bool scaling = false;
double null_rej = 5.0;

void setup() {
    stream.useNormalizer(normalize);
    stream.setLabelsForAllDimensions({"red", "green", "blue"});
    useStream(stream);

    pipeline.addPreProcessingModule(MovingAverageFilter(5, 3));
    // use scaling, use null rejection, null rejection parameter
    pipeline.setClassifier(ANBC(scaling, true, null_rej));

    // null rejection parameter is multiplied by the standard deviation to determine
    // the rejection threshold. the higher the number, the looser the filter; the
    // lower the number, the tighter the filter.

    usePipeline(pipeline);
    useOStream(oStream);

    registerTuneable(scaling, "scaling",
                     "If true, the training and prediction data will be scaled "
                     "to a specific range. Default should be set false.");

    registerTuneable(null_rej, 1.0, 10.0,
                     "null rejection",
                     "null rejection parameter is multiplied by the standard deviation"
                     " to determine the rejection threshold. "
                     "The higher the number, the looser the filter; "
                     "the lower the number, the tighter the filter.");
}
