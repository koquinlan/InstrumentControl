/**
 * @file threadedTesting.cpp
 * @author Kyle Quinlan (kyle.quinlan@colorado.edu)
 * @brief For rapid prototyping and testing of multithreaded control and acquisition code. See src/controlTests.cpp for single threaded development.
 * @version 0.1
 * @date 2023-06-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "decs.hpp"

#define REFRESH_PROCESSOR (0)

int main() {
    int maxSpectraPerStep = 50;
    int minSpectraPerStep = 13;
    int subSpectraAveragingNumber = 15;
    double maxIntegrationTime = maxSpectraPerStep*subSpectraAveragingNumber*0.01; // seconds

    double stepSize = 0.1; // MHz
    int numSteps = 50;


    ScanRunner scanRunner(maxIntegrationTime, 0, 0);
    scanRunner.subSpectraAveragingNumber = subSpectraAveragingNumber;
    scanRunner.setTarget(6.5e-5);
    scanRunner.decisionAgent.minShots = minSpectraPerStep;


    #if REFRESH_PROCESSOR
    scanRunner.refreshBaselineAndBadBins(1, 32, 1);
    #endif

    scanRunner.acquireData();
    for (int i = 0; i < numSteps; i++) {
        scanRunner.step(stepSize);
        scanRunner.acquireData();
    }

    std::cout << "Saving data..." << std::endl;
    scanRunner.saveData();

    std::cout << "Exited Normally" << std::endl;
    return 0;
}
