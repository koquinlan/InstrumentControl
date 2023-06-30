#include "decs.hpp"

/**
 * @brief Function to be run in a separate thread in parallel with ATS::AcquireDataMultithreadedContinuous. Fourier transforms incoming data and
 *        pushes the result to the decision making and saving queues.
 * 
 * @param plan - FFTW plan object for the FFT
 * @param N - Number of samples per buffer shared in the data queue
 * @param sharedData - Struct containing data shared between threads
 * @param syncFlags - Struct containing synchronization flags shared between threads
 */
void processingThread(fftw_plan plan, int N, SharedData& sharedData, SynchronizationFlags& syncFlags) {
    int numProcessed = 0;
    while (true) {
        // Wait for signal from dataReadyCondition or immediately continue if the data queue is not empty (lock releases while waiting)
        std::unique_lock<std::mutex> lock(sharedData.mutex);
        sharedData.dataReadyCondition.wait(lock, [&sharedData]() {
            return !sharedData.dataQueue.empty();
        });


        // Process data until data queue is empty (lock is reaquired before checking the data queue)
        while (!sharedData.dataQueue.empty()) {
            // Get the pointer to the data from the queue
            fftw_complex* complexOutput = sharedData.dataQueue.front();
            sharedData.dataQueue.pop();
            lock.unlock();

            // Process the data (lock is released while processing)
            fftw_complex* procData = processDataFFT(complexOutput, plan, N);
            numProcessed++;


            // Acquire a new lock_guard and push the processed data to the shared queue
            {
                std::lock_guard<std::mutex> lock(sharedData.mutex);
                sharedData.dataSavingQueue.push(complexOutput);
                sharedData.processedDataQueue.push(procData);
            }
            sharedData.processedDataReadyCondition.notify_one();
            sharedData.saveReadyCondition.notify_one();
            
            lock.lock();  // Reacquire lock before checking the data queue
        }

        // Check if the acquisition and processing is complete
        {
            std::lock_guard<std::mutex> lock(syncFlags.mutex);
            if (syncFlags.acquisitionComplete && sharedData.dataQueue.empty()) {
                break;  // Exit the processing thread
            }
        }
    }
}



/**
 * @brief Placeholder function to be run in a separate thread in parallel with ATS::AcquireDataMultithreadedContinuous. 
 *        Currently just pops data from the processed data queue and frees the memory.
 * 
 * @param sharedData - Struct containing data shared between threads
 * @param syncFlags - Struct containing synchronization flags shared between threads
 */
void decisionMakingThread(SharedData& sharedData, SynchronizationFlags& syncFlags) {
    int buffersProcessed = 0;
    while (true) {
        // Check if processed data queue is empty
        std::unique_lock<std::mutex> lock(sharedData.mutex);
        sharedData.processedDataReadyCondition.wait(lock, [&sharedData]() {
            return !sharedData.processedDataQueue.empty();
        });


        // Process data if the data queue is not empty
        while (!sharedData.processedDataQueue.empty()) {
            // Get the pointer to the data from the queue
            fftw_complex* processedOutput = sharedData.processedDataQueue.front();
            sharedData.processedDataQueue.pop();
            lock.unlock();

            // Free the memory allocated for the raw data
            fftw_free(processedOutput);
            buffersProcessed++;
            // std::cout << "Decided on " << std::to_string(buffersProcessed) << " buffers." << std::endl;

            if (buffersProcessed >= 50) {
                std::lock_guard<std::mutex> lock(syncFlags.mutex);
                syncFlags.acquisitionComplete = true;
                syncFlags.pauseDataCollection = true;
                break;
            }

            lock.lock();  // Lock again before checking the data queue
        }

        // Check if the acquisition is complete
        {
            std::lock_guard<std::mutex> lock(syncFlags.mutex);
            if (syncFlags.acquisitionComplete) {
                break;  // Exit the processing thread
            }
        }
    }
}



/**
 * @brief Function to be run in a separate thread in parallel with ATS::AcquireDataMultithreadedContinuous. Saves data from the data 
 *        saving queue to binary files. The data is saved in the output directory.
 * 
 * @todo Change file I/O system to HDF5 or similar
 * 
 * @param sharedData - Struct containing data shared between threads
 * @param syncFlags - Struct containing synchronization flags shared between threads
 */
void saveDataToBin(SharedData& sharedData, SynchronizationFlags& syncFlags) {
    // Create the output directory
    std::filesystem::create_directory("output");

    int numSaved = 0;
    while (true) {
        // Check if data queue is empty
        std::unique_lock<std::mutex> lock(sharedData.mutex);
        sharedData.saveReadyCondition.wait(lock, [&sharedData]() {
            return !sharedData.dataSavingQueue.empty();
        });


        // Process data if the data queue is not empty
        while (!sharedData.dataSavingQueue.empty()) {
            // Get the pointer to the data from the queue
            fftw_complex* rawData = sharedData.dataSavingQueue.front();
            sharedData.dataSavingQueue.pop();
            lock.unlock();
        

            std::string filename = "output/Buffer" + std::to_string(numSaved+1) + ".bin";
            // Open the file in binary mode
            std::ofstream file(filename, std::ios::binary);
            if (!file) {
                std::cout << "Failed to open the file." << std::endl;
            }

            // Write the complex data to the file
            file.write(reinterpret_cast<const char*>(rawData), sizeof(fftw_complex) * sharedData.samplesPerBuffer);

            // Close the file
            file.close();
            numSaved++;

            // Free the memory allocated for the raw data
            fftw_free(rawData);

            lock.lock();  // Lock again before checking the data queue
        }

        // Check if the acquisition and saving is complete
        {
            std::lock_guard<std::mutex> lock(syncFlags.mutex);
            if (syncFlags.acquisitionComplete && sharedData.dataSavingQueue.empty()) {
                break;
            }
        }
    }
}