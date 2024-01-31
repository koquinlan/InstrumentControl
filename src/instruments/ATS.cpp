/**
 * @file ATS.cpp
 * @author Kyle Quinlan (kyle.quinlan@colorado.edu)
 * @brief Method definitions and documentation for ATS class. See include\ATS.hpp for class definition.
 * @version 0.1
 * @date 2023-06-30
 * 
 * @copyright Copyright (c) 2023
 * 
 * @note Some of the code in this file is adapted from the AlazarTech SDK examples. See the following for more information: 
 * http://research.physics.illinois.edu/bezryadin/labprotocol/ATS-SDK-Guide-5.8.2.pdf
 * 
 * @warning Some functions assume a 2-channel acquisition. If you want to use this with a single channel, you will need to modify the code.
 * 
 */

#include "decs.hpp"

#define VERBOSE_OUTPUT (0)


/**
 * @brief Construct a new ATS::ATS object. This will open the board and set the boardHandle member variable. Default to system ID 1, board ID 1.
 * 
 * @param systemId 
 * @param boardId 
 */
ATS::ATS(int systemId, int boardId) {
    boardHandle = AlazarGetBoardBySystemID(systemId, boardId);
    if (boardHandle == NULL) {
        throw std::runtime_error("Unable to open board system ID " + std::to_string(systemId) + " board ID " + std::to_string(boardId) + "\n");
    }
}


/**
 * @brief Destroy the ATS::ATS object. This will close the board and free the boardHandle member variable.
 * 
 */
ATS::~ATS() { 
    if (boardHandle != NULL) {
        boardHandle = NULL;
    }
}



/**
 * @brief Configures the sample clock of the ATS9462.
 * 
 * @details This assumes that you are using a 10 MHz external reference clock. The effective sample rate is somewhat restricted; 
    the board's bare sample rate must be between 150 and 180 Msps in steps of 1 Msps. You may then define an arbitrary decimation 
    factor of an integer between 1 and 10,000.

    This code will perform a little bit of math to figure out the optimal sampling settings given the requested sampling rate, and 
    then return the actual sampling rate. If you request a standard sample rate such as 10 Msps, the card will agree.  If you want 
    something nonstandard such as 9.57 Msps, you will get an actual sampling rate which is very close, but not exact
 * 
 * @param requestedSampleRate desired sample rate in MHz
 * @return double - Actual sample rate generated by the board
 */
double ATS::setExternalSampleClock(double requestedSampleRate) {
    // Fix sample rate to be positive and less than the absolute maximum of 180 MHz
    requestedSampleRate = min(std::abs(requestedSampleRate), 180e6);

    // Establish the range of bare sample rates the board is capable of (150 MHz - 180 MHz with steps of 1 MHz)
    std::vector<int> bareSampleRates;
    for (int rate = (int)150e6; rate <= (int)180e6; rate += (int)1e6) {
        bareSampleRates.push_back(rate);
    }

    // Find the best decimation factor for each bare sample rate
    std::vector<int> decimationFactors;
    std::vector<double> effectiveSampleRates;
    for (int& rate : bareSampleRates) {
        int factor = (int) min(std::round((double)rate/requestedSampleRate),10000);

        decimationFactors.push_back(factor);
        effectiveSampleRates.push_back((double)rate/(double)factor);
    }


    // Find the closest bare sample rate & decimation factor pair to approximate the requested sample rate
    int bestIndex = 0;
    double minError = DBL_MAX;
    for (int i=0; i < effectiveSampleRates.size(); i++){
        double error = std::abs(effectiveSampleRates[i] - requestedSampleRate);

        if (error < minError){
            minError = error;
            bestIndex = i;

            if (minError == 0) {
                break;
            }
        }
    }

    // Send the sample rate to the card
    retCode = AlazarSetCaptureClock(
        boardHandle,			        // HANDLE -- board handle
        EXTERNAL_CLOCK_10MHz_REF,		// U32 -- clock source id
        bareSampleRates[bestIndex],		// U32 -- sample rate id
        CLOCK_EDGE_RISING,		        // U32 -- clock edge id
        decimationFactors[bestIndex]-1  // U32 -- clock decimation 
    );
	if (retCode != ApiSuccess) {
		throw std::runtime_error(std::string("Error: AlazarSetCaptureClock failed -- ") + AlazarErrorToText(retCode) + "\n");
	}

    return effectiveSampleRates[bestIndex];
}



/**
 * @brief Sets up the input parameters for a particular channel
 * 
 * @param channel           'a' or 'b'              - which channel to set
 * @param coupling          'dc' or 'ac'            - coupling mode
 * @param inputRange        0.2, 0.4, 0.8 or 2 V    - full scale voltage range
 * @param inputImpedance    50 or 1e6 ohms          - input impedance
 */
void ATS::setInputParameters(char channel, std::string coupling, double inputRange, double inputImpedance) {
    // Get the channel ID
    int channelID = getChannelID(channel);

    // Get the coupling mode
    int couplingMode;
    if (coupling == "DC" || coupling == "dc")       { couplingMode = DC_COUPLING; } 
    else if (coupling == "AC" || coupling == "ac")  { couplingMode = AC_COUPLING; } 
    else {
        throw std::runtime_error("Invalid coupling selection. Select 'DC' or 'AC'");
    }

    // Get the input range ID
    int rangeID;
    if (inputRange <= 0.2) {
        rangeID = INPUT_RANGE_PM_200_MV;
        inputRange = 0.2;
    } else if (inputRange <= 0.4) {
        rangeID = INPUT_RANGE_PM_400_MV;
        inputRange = 0.4;
    } else if (inputRange <= 0.8) {
        rangeID = INPUT_RANGE_PM_800_MV;
        inputRange = 0.8;
    } else {
        rangeID = INPUT_RANGE_PM_2_V;
        inputRange = 2.0;
    }

    // Get the input impedance ID (I believe the 9462 only accepts 50 ohm or 1 Mohm)
    int impedanceID;
    if (inputImpedance <= 50) {
        impedanceID = IMPEDANCE_50_OHM;
        inputImpedance = 50;
    // } else if (inputImpedance <= 75) {
    //     impedanceID = IMPEDANCE_75_OHM;
    //     inputImpedance = 75;
    // } else if (inputImpedance <= 300) {
    //     impedanceID = IMPEDANCE_300_OHM;
    //     inputImpedance = 300;
    } else {
        impedanceID = IMPEDANCE_1M_OHM;
        inputImpedance = 1e6;
    }

    // Call AlazarInputControl to set the input parameters
    retCode = AlazarInputControl(
        boardHandle,			// HANDLE -- board handle
        channelID,				// U8 -- channel identifier
        couplingMode,			// U32 -- input coupling id
        rangeID,	            // U32 -- input range id
        impedanceID     		// U32 -- input impedance id
    );
    if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarInputControl failed -- ") + AlazarErrorToText(retCode) + "\n");
    }
}



/**
 * @brief Toggles the 20MHz low pass filter for a particular channel
 * 
 * @param channel           'a' or 'b'      - which channel to set
 * @param enable            0 or 1          - 0 = disable, 1 = enable
 */
void ATS::toggleLowPass(char channel, bool enable) {
    int channelID = getChannelID(channel);

    retCode = AlazarSetBWLimit(
        boardHandle,			// HANDLE -- board handle
        channelID,				// U8 -- channel identifier
        enable					// U32 -- 0 = disable, 1 = enable
    );
    if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarSetBWLimit failed -- ") + AlazarErrorToText(retCode) + "\n");
    }
}



/**
 * @brief Converts a channel character to a channel ID. Purely for convenience in other methods.
 * 
 * @param channel 'a' or 'b'      - which channel to set
 * @return int - channel ID (CHANNEL_A or CHANNEL_B)
 */
int ATS::getChannelID(char channel){
    int channelID;
    channel = std::toupper(channel);
    
    if      (channel == 'A') { channelID = CHANNEL_A; } 
    else if (channel == 'B') { channelID = CHANNEL_B; } 
    else {
        throw std::runtime_error("Invalid channel selection. Select channel 'A' or 'B'");
    }

    return channelID;
}



/**
 * @brief Suggests a number of buffers to use for a given sample rate and number of samples per acquisition. Attempts to optimize buffer size for DMA speed
 * 
 * @param sampleRate - desired sample rate in Hz
 * @param samplesPerAcquisition - desired number of samples over the full acquisition
 * @return U32 - suggested number of buffers to use for given parameters
 */
U32 ATS::suggestBufferNumber(U32 sampleRate, U32 samplesPerAcquisition){
    // For optimal performance buffer sizes should be between 1MB and 16MB. Absolute maximum is 64MB
    // As per https://docs.alazartech.com/ats-sdk-user-guide/latest/reference/AlazarBeforeAsyncRead.html

    int channelCount = 2; 
    int desiredBytesPerBuffer = (int)2e6; // Shoot for 2MB buffer sizes

    // Get the sample size in bits, and the on-board memory size in samples per channel
	U8 bitsPerSample;
	U32 maxSamplesPerChannel;
	RETURN_CODE retCode = AlazarGetChannelInfo(boardHandle, &maxSamplesPerChannel, &bitsPerSample);
	if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarGetChannelInfo failed -- ") + AlazarErrorToText(retCode) + "\n");
	}


    // Calculate remaining parameters
    U32 bytesPerSample = (bitsPerSample + 7) / 8;
	U32 buffersPerAcquisition = max(1, (U32)std::round(bytesPerSample * samplesPerAcquisition * channelCount / desiredBytesPerBuffer)); 

    // Make sure the number of buffers evenly divides the number of samples
    int spread = 1;
    while((samplesPerAcquisition % buffersPerAcquisition != 0)) {
        if (samplesPerAcquisition % (buffersPerAcquisition + spread) == 0) {
            buffersPerAcquisition = buffersPerAcquisition + spread;
        } 
        else if (samplesPerAcquisition % (buffersPerAcquisition - spread) == 0) {
            buffersPerAcquisition = buffersPerAcquisition - spread;
        }
        spread++;
    }


    return buffersPerAcquisition;
}



void ATS::printBufferSize(U32 samplesPerAcquisition, U32 buffersPerAcquisition){
    // Get the sample size in bits, and the on-board memory size in samples per channel
	U8 bitsPerSample;
	U32 maxSamplesPerChannel;
	RETURN_CODE retCode = AlazarGetChannelInfo(boardHandle, &maxSamplesPerChannel, &bitsPerSample);
	if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarGetChannelInfo failed -- ") + AlazarErrorToText(retCode) + "\n");
	}


    // Calculate remaining parameters
    U32 bytesPerSample = (bitsPerSample + 7) / 8;
    double megaBytesPerBuffer = bytesPerSample * samplesPerAcquisition / buffersPerAcquisition / 1e6;

    std::cout << "Acquisition parameters result in buffer size of " << std::to_string(megaBytesPerBuffer) << "MB per buffer." << std::endl;

    return;
}



/**
 * @brief Sets the acquisition parameters for the ATS9462. This will set all parameters in the acquisitionParams struct.
 * 
 * @param sampleRate - desired sample rate in Hz
 * @param samplesPerAcquisition - desired number of samples over the full acquisition
 * @param buffersPerAcquisition - desired number of buffers to split the acquisition into. If 0, will be calculated automatically for efficiency.
 * @param inputRange - desired full scale voltage range. Valid values are 0.2, 0.4, 0.8, and 2 V
 * @param inputImpedance - desired input impedance. Valid values are 50 and 1e6 ohms
 */
void ATS::setAcquisitionParameters(U32 sampleRate, U32 samplesPerAcquisition, U32 buffersPerAcquisition, double inputRange, double inputImpedance){
    if (buffersPerAcquisition <= 0){
        buffersPerAcquisition = suggestBufferNumber(sampleRate, samplesPerAcquisition);
    }
    printBufferSize(samplesPerAcquisition, buffersPerAcquisition);


    // Get the sample size in bits, and the on-board memory size in samples per channel
	U8 bitsPerSample;
	U32 maxSamplesPerChannel;
	RETURN_CODE retCode = AlazarGetChannelInfo(boardHandle, &maxSamplesPerChannel, &bitsPerSample);
	if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarGetChannelInfo failed -- ") + AlazarErrorToText(retCode) + "\n");
	}


    // Calculate and save remaining parameters
    int channelCount = 2;
    U32 recordsPerBuffer = 1;   
    
    acquisitionParams.sampleRate = sampleRate;
    acquisitionParams.buffersPerAcquisition = buffersPerAcquisition;
    acquisitionParams.inputRange = inputRange;
    acquisitionParams.inputImpedance = inputImpedance;
    acquisitionParams.recordsPerAcquisition = recordsPerBuffer*buffersPerAcquisition; 

    acquisitionParams.samplesPerBuffer = samplesPerAcquisition/buffersPerAcquisition;
    acquisitionParams.bytesPerSample = (bitsPerSample + 7) / 8;
    acquisitionParams.bytesPerBuffer = acquisitionParams.bytesPerSample * acquisitionParams.samplesPerBuffer * channelCount;

    // Calculated from the above parameters since rounding could cause the actual sample number to be slightly different than requested
    acquisitionParams.samplesPerAcquisition = acquisitionParams.samplesPerBuffer*acquisitionParams.buffersPerAcquisition;


    // Send parameters to board
    double realSampleRate = setExternalSampleClock(acquisitionParams.sampleRate);
    if (realSampleRate != acquisitionParams.sampleRate){
        std::cout << "Sample rate adjusted from requsted " << std::to_string(acquisitionParams.sampleRate/1e6) << " MHz to " 
        << std::to_string(realSampleRate/1e6) << " MHz." << std::endl;
    }

    setInputParameters('a', "dc", acquisitionParams.inputRange, acquisitionParams.inputImpedance);
    toggleLowPass('a', 1);

    setInputParameters('b', "dc", acquisitionParams.inputRange, acquisitionParams.inputImpedance);
    toggleLowPass('b', 1);                                   

    retCode = AlazarSetRecordSize(
        boardHandle,			    // HANDLE -- board handle
        0,
        acquisitionParams.samplesPerBuffer
    );
    if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarSetRecordSize failed -- ") + AlazarErrorToText(retCode) + "\n");
    }

    retCode = AlazarSetRecordCount(
        boardHandle,			    // HANDLE -- board handle
        acquisitionParams.buffersPerAcquisition			    
    );
    if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarSetRecordCount failed -- ") + AlazarErrorToText(retCode) + "\n");
    }
}



/**
 * @brief Begins an aquisition on the ATS9462 based on parameters set in acquisitionParams struct. Intended for immediate FFT with FFTW library.
 * 
 * @warning This function will allocate memory for the raw data. It is the caller's responsibility to free this memory.
 * @warning Alternating signs applied to the real and imaginary components of the output to center the assumed DFT at 0 later in processing.
 *
 * @return fftw_complex* - pointer to the raw data as voltages with channel A and B in real and imaginary components, respectively.
 */
fftw_complex* ATS::AcquireData() {
    // Set basic flags
    U32 channelMask = CHANNEL_A | CHANNEL_B;
    U32 admaFlags = ADMA_TRIGGERED_STREAMING | ADMA_EXTERNAL_STARTCAPTURE;         // Start acquisition when AlazarStartCapture is called
    int channelCount = 2; 


    // Prime board for acquisition
    retCode = AlazarBeforeAsyncRead(
        boardHandle,			                    // HANDLE -- board handle
        channelMask,			                    // U32 -- enabled channel mask
        0,						                    // long -- offset from trigger in samples
        acquisitionParams.samplesPerBuffer,		    // U32 -- samples per buffer
        1,		                                    // U32 -- records per buffer (must be 1)
        acquisitionParams.recordsPerAcquisition,    // U32 -- records per acquisition 
        admaFlags				                    // U32 -- AutoDMA flags
    ); 
    if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarBeforeAsyncRead failed -- ") + AlazarErrorToText(retCode) + "\n");
    }


    // Allocate memory for DMA buffers and post them to board
	int bufferIndex;
	for (bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++)
	{
		IoBufferArray[bufferIndex] = CreateIoBuffer(acquisitionParams.bytesPerBuffer);
		if (IoBufferArray[bufferIndex] == NULL) {
            throw std::runtime_error(std::string("Error: Alloc ") + AlazarErrorToText(retCode) +  "bytes failed\n");
		}
	}
    bool success = TRUE;

	for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && (success == TRUE); bufferIndex++)
	{
		IO_BUFFER *pIoBuffer = IoBufferArray[bufferIndex];
		if (!ResetIoBuffer(pIoBuffer)) {
			success = FALSE;
		}
		else {
			retCode = AlazarPostAsyncBuffer(
                boardHandle,					// HANDLE -- board handle
                pIoBuffer->pBuffer,				// void* -- buffer
                pIoBuffer->uBufferLength_bytes	// U32 -- buffer length in bytes
            );				
			if (retCode != ApiSuccess) {
                throw std::runtime_error(std::string("Error: AlazarAsyncRead ") + std::to_string(bufferIndex) + 
                                                     " failed -- " + AlazarErrorToText(retCode) + "\n");
			}
		}
	}


	// Arm the board to begin the acquisition 
	if (success) {
		retCode = AlazarStartCapture(boardHandle);
		if (retCode != ApiSuccess) {
			throw std::runtime_error(std::string("Error: AlazarStartCapture failed -- ") + AlazarErrorToText(retCode) + "\n");
            success = false;
		}
	}

    std::vector<unsigned short> fullDataA;
    std::vector<unsigned short> fullDataB;

    fftw_complex* complexOutput = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * acquisitionParams.samplesPerAcquisition));

	// Wait for each buffer to be filled, process the buffer, and re-post it to the board.
	if (success) {
        #if VERBOSE_OUTPUT
		printf("Capturing %d buffers ... press any key to abort\n", acquisitionParams.buffersPerAcquisition);
        #endif

		DWORD startTickCount = GetTickCount();
		U32 buffersCompleted = 0;
		INT64 bytesTransferred = 0;

        
        // Main acquisition logic
		while (buffersCompleted < acquisitionParams.buffersPerAcquisition) {
            // Send a software trigger to begin acquisition (technically should be unecessary but won't hurt anything)
            retCode = AlazarForceTrigger(boardHandle);
            if (retCode != ApiSuccess) {
                throw std::runtime_error(std::string("Error: Trigger failed to send -- ") + AlazarErrorToText(retCode) + "\n");
            }

            // Timeout after 10x the expected time for 1 buffer
			DWORD timeout_ms = (DWORD)(10*1e3*acquisitionParams.samplesPerBuffer/acquisitionParams.sampleRate); 


			// Wait for the buffer at the head of the list of available buffers to be filled by the board.
			bufferIndex = buffersCompleted % BUFFER_COUNT;
			IO_BUFFER *pIoBuffer = IoBufferArray[bufferIndex];
			retCode = AlazarWaitAsyncBufferComplete(
                boardHandle, 
                pIoBuffer->pBuffer, 
                timeout_ms
            );

            // Process the buffer that was just filled. This buffer is full and has been removed from the list of buffers available to the board.
			if (retCode == ApiSuccess) {
                // DWORD startProcTickCount = GetTickCount();

                std::vector<unsigned short> bufferData(reinterpret_cast<unsigned short*>(pIoBuffer->pBuffer),
                                      reinterpret_cast<unsigned short*>(pIoBuffer->pBuffer) + (acquisitionParams.bytesPerBuffer / sizeof(unsigned short)));

                // Deprecated from before I was using FFTW immediately after acquisition
                fullDataA.insert(fullDataA.end(), bufferData.begin(), bufferData.begin() + bufferData.size()/2);
                fullDataB.insert(fullDataB.end(), bufferData.begin() + bufferData.size()/2, bufferData.end());

                // Current implementation - pre processes the data into a complex array for FFT
                for (unsigned int i=0; i < bufferData.size()/2; i++) {
                    int index = buffersCompleted*acquisitionParams.samplesPerBuffer + i;

                    complexOutput[index][0] = (bufferData[i]   / (double)0xFFFF) * 2 * acquisitionParams.inputRange - acquisitionParams.inputRange;
                    complexOutput[index][1] = (bufferData[bufferData.size()/2 + i]  / (double)0xFFFF) * 2 * acquisitionParams.inputRange - acquisitionParams.inputRange;

                    // WARNING: Alternating signs trick to 0-center the dft
                    if (index % 2 == 1) {
                        complexOutput[index][0] *= -1;
                        complexOutput[index][1] *= -1;
                    }
                }

                // double bufferProcTime_sec = (GetTickCount() - startProcTickCount) / 1000.;
	            // std::cout << "Buffer processed in " << bufferProcTime_sec << " sec" << std::endl;

                buffersCompleted++;
				bytesTransferred += acquisitionParams.bytesPerBuffer;	
            }
			else {
				// The wait failed
				success = FALSE;

				switch (retCode)
				{
				case ApiWaitTimeout:
					printf("Error: Wait timeout after %lu ms\n", timeout_ms);
					break;

				case ApiBufferOverflow:
					printf("Error: Board overflowed on-board memory\n");
					break;

                case ApiBufferNotReady:
					printf("Error: Buffer not found in list of available\n");
					break;

                case ApiDmaInProgress:
					printf("Error: Buffer not at the head of available buffers\n");
					break;

				default:
					printf("Error: Wait failed with error -- %u\n", GetLastError());
					break;
				}
			}


			// Add the buffer to the end of the list of available buffers on the board
			if (success) {
				if (!ResetIoBuffer (pIoBuffer)) {
					success = FALSE;
				}
				else {
                    retCode = AlazarPostAsyncBuffer(
                        boardHandle,					// HANDLE -- board handle
                        pIoBuffer->pBuffer,				// void* -- buffer
                        pIoBuffer->uBufferLength_bytes	// U32 -- buffer length in bytes
                    );		
					if (retCode != ApiSuccess) {
						printf("Error: AlazarPostAsyncBuffer failed -- %s\n", AlazarErrorToText(retCode));
						success = FALSE;
					}
				}
			}
			// If the acquisition failed, exit the acquisition loop
			if (!success) {
				break;
            }

			// If a key was pressed, exit the acquisition loop					
			if (_kbhit()) {
				printf("Aborted...\n");
				break;
			}

            #if VERBOSE_OUTPUT
			// Display progress
			printf("Completed %u buffers\r", buffersCompleted);
            #endif
		}

        #if VERBOSE_OUTPUT
		// Display timing results for the acquisition
		double transferTime_sec = (GetTickCount() - startTickCount) / 1000.;
		printf("Capture completed in %.3lf sec\n", transferTime_sec);

		double buffersPerSec;
		double bytesPerSec;
		if (transferTime_sec > 0.)
		{
			buffersPerSec = buffersCompleted / transferTime_sec;
			bytesPerSec = bytesTransferred / transferTime_sec;
		}
		else
		{
			buffersPerSec = 0.;
			bytesPerSec = 0.;
		}

		printf("Captured %d buffers (%.4g buffers per sec)\n", buffersCompleted, buffersPerSec);
		printf("Transferred %I64d bytes (%.4g bytes per sec)\n\n", bytesTransferred, bytesPerSec);
        #endif
	}


	// Abort the acquisition ("abort" is potentially misleading here - should call even if the acquisition completed normally)
	retCode = AlazarAbortAsyncRead(boardHandle);
	if (retCode != ApiSuccess) {
		printf("Error: AlazarAbortAsyncRead failed -- %s\n", AlazarErrorToText(retCode));
		success = FALSE;
	}


	// Free all memory allocated
	for (bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++) {
		if (IoBufferArray[bufferIndex] != NULL)
			DestroyIoBuffer(IoBufferArray[bufferIndex]);
	}


    // Return the fftw_complex array for further processing
    return complexOutput;
}



/**
 * @brief Deprecated function to process channels from the ATS9462. This will return a pair of vectors containing the voltage data for each channel.
 * 
 * @param sampleData - Pair of double vectors containing the raw sample data for each channel saved as unsigned shorts
 * @param acquisitionParams - Struct containing the acquisition parameters used to collect sampleData
 * @return std::pair<std::vector<double>, std::vector<double>> - Pair of double vectors containing the voltage data for each channel
 */
std::pair<std::vector<double>, std::vector<double>> processData(std::pair<std::vector<unsigned short>, std::vector<unsigned short>> sampleData, AcquisitionParameters acquisitionParams) {
    // Sample codes are unsigned by default so that:
    // - a sample code of 0x0000 represents a negative full scale input signal;
    // - a sample code of 0x8000 represents a 0V signal;
    // - a sample code of 0xFFFF represents a positive full scale input signal;

    // Convert the sample data to voltage values
    std::vector<double> voltageDataA;
    std::vector<double> voltageDataB;

    voltageDataA.reserve(acquisitionParams.samplesPerAcquisition);
    voltageDataB.reserve(acquisitionParams.samplesPerAcquisition);

    for (unsigned int i=0; i < sampleData.first.size(); i++) {
        double voltageA = (sampleData.first[i]   / (double)0xFFFF) * 2 * acquisitionParams.inputRange;
        double voltageB = (sampleData.second[i]  / (double)0xFFFF) * 2 * acquisitionParams.inputRange;

        voltageDataA.push_back(voltageA - acquisitionParams.inputRange);
        voltageDataB.push_back(voltageB - acquisitionParams.inputRange);
    }

    return std::make_pair(voltageDataA, voltageDataB);
}



/**
 * @brief Fourier transforms the time domain data from the ATS9462. This will return a fftw_complex array containing frequency domain 
 * voltage data (raw spectra).
 * 
 * @param sampleData - fftw_complex array containing the raw sample data. The real and imaginary components of each element are the 
 * voltage data for channels A and B, respectively. Should be pre-processed into voltages (currently done in acquisition loop)
 * @param plan - fftw_plan object containing the plan for the FFT. Should be created before calling this function.
 * @param N - Number of samples in sampleData. Should be the same as acquisitionParams.samplesPerBuffer or acquisitionParams.samplesPerAcquisition
 * @return fftw_complex* - Fourier transformed data in the frequency domain
 */
fftw_complex* processDataFFT(fftw_complex* sampleData, fftw_plan plan, int N) {
    fftw_complex *FFTData = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * 2 * N));
    fftw_execute_dft(plan, sampleData, FFTData);

    return FFTData;
}



/**
 * @brief Data acquisition loop for the fully parallelized acquisition. Designed to acquire data continuously until the pauseDataCollection flag 
 * is set to true or the fixed horizon is hit. This function will acquire data, process it into voltage, and save it to the sharedData struct.
 * 
 * @param sharedData - Struct containing the shared data between threads. This function will write to dataQueue and signal dataReadyCondition
 * @param syncFlags - Struct containing the synchronization flags between threads. This function will read the pauseDataCollection flag
 */
void ATS::AcquireDataMultithreadedContinuous(SharedDataBasic& sharedData, SynchronizationFlags& syncFlags) {
    try{
    // Set basic flags
    U32 channelMask = CHANNEL_A | CHANNEL_B;
    U32 admaFlags = ADMA_TRIGGERED_STREAMING | ADMA_EXTERNAL_STARTCAPTURE;         // Start acquisition when AlazarStartCapture is called
    int channelCount = 2; 


    // Prime board for acquisition
    retCode = AlazarBeforeAsyncRead(
        boardHandle,			                    // HANDLE -- board handle
        channelMask,			                    // U32 -- enabled channel mask
        0,						                    // long -- offset from trigger in samples
        acquisitionParams.samplesPerBuffer,		    // U32 -- samples per buffer
        1,		                                    // U32 -- records per buffer (must be 1)
        acquisitionParams.recordsPerAcquisition,    // U32 -- records per acquisition 
        admaFlags				                    // U32 -- AutoDMA flags
    ); 
    if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarBeforeAsyncRead failed -- ") + AlazarErrorToText(retCode) + "\n");
    }


    // Allocate memory for DMA buffers and post them to board
	int bufferIndex;
	for (bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++)
	{
		IoBufferArray[bufferIndex] = CreateIoBuffer(acquisitionParams.bytesPerBuffer);
		if (IoBufferArray[bufferIndex] == NULL) {
            throw std::runtime_error(std::string("Error: Alloc ") + AlazarErrorToText(retCode) +  "bytes failed\n");
		}
	}

    bool success = TRUE;
	for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && (success == TRUE); bufferIndex++)
	{
		IO_BUFFER *pIoBuffer = IoBufferArray[bufferIndex];
		if (!ResetIoBuffer(pIoBuffer)) {
			success = FALSE;
		}
		else {
			retCode = AlazarPostAsyncBuffer(
                boardHandle,					// HANDLE -- board handle
                pIoBuffer->pBuffer,				// void* -- buffer
                pIoBuffer->uBufferLength_bytes	// U32 -- buffer length in bytes
            );				
			if (retCode != ApiSuccess) {
                throw std::runtime_error(std::string("Error: AlazarAsyncRead ") + std::to_string(bufferIndex) + 
                                                     " failed -- " + AlazarErrorToText(retCode) + "\n");
			}
		}
	}


	// Arm the board to begin the acquisition 
	if (success) {
		retCode = AlazarStartCapture(boardHandle);
		if (retCode != ApiSuccess) {
			throw std::runtime_error(std::string("Error: AlazarStartCapture failed -- ") + AlazarErrorToText(retCode) + "\n");
            success = false;
		}
	}


	// Wait for each buffer to be filled, process the buffer, and re-post it to the board.
	if (success) {
        startTimer(TIMER_ACQUISITION);
        #if VERBOSE_OUTPUT
		printf("Capturing %d buffers ... press any key to abort\n", acquisitionParams.buffersPerAcquisition);
        #endif

		DWORD startTickCount = GetTickCount();
		U32 buffersCompleted = 0;
		INT64 bytesTransferred = 0;

        // Main acquisition logic     
		while (buffersCompleted < acquisitionParams.buffersPerAcquisition) {
            // Acquire a mutex lock and check if the pause flag has been set
            {
                std::lock_guard<std::mutex> lock(syncFlags.mutex);
                if (syncFlags.pauseDataCollection) {
                    std::cout << "Received pause signal" << std::endl;
                    break;
                }
            }


            // Send a software trigger to begin acquisition (technically should be unecessary but won't hurt anything)
            retCode = AlazarForceTrigger(boardHandle);
            if (retCode != ApiSuccess) {
                throw std::runtime_error(std::string("Error: Trigger failed to send -- ") + AlazarErrorToText(retCode) + "\n");
            }

            // Timeout after 10x the expected time for 1 buffer
			DWORD timeout_ms = (DWORD)(10*1e3*acquisitionParams.samplesPerBuffer/acquisitionParams.sampleRate); 


			// Wait for the buffer at the head of the list of available buffers to be filled by the board.
			bufferIndex = buffersCompleted % BUFFER_COUNT;
			IO_BUFFER *pIoBuffer = IoBufferArray[bufferIndex];
			retCode = AlazarWaitAsyncBufferComplete(
                boardHandle, 
                pIoBuffer->pBuffer, 
                timeout_ms
            );


            // Process the buffer that was just filled. This buffer is full and has been removed from the list of buffers available to the board.
			if (retCode == ApiSuccess) {
                // DWORD startProcTickCount = GetTickCount();
                fftw_complex* complexOutput = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * acquisitionParams.samplesPerBuffer));

                std::vector<unsigned short> bufferData(reinterpret_cast<unsigned short*>(pIoBuffer->pBuffer),
                                      reinterpret_cast<unsigned short*>(pIoBuffer->pBuffer) + (acquisitionParams.bytesPerBuffer / sizeof(unsigned short)));


                for (unsigned int i=0; i < bufferData.size()/2; i++) {
                    complexOutput[i][0] = (bufferData[i]   / (double)0xFFFF) * 2 * acquisitionParams.inputRange - acquisitionParams.inputRange;
                    complexOutput[i][1] = (bufferData[bufferData.size()/2 + i]  / (double)0xFFFF) * 2 * acquisitionParams.inputRange - acquisitionParams.inputRange;

                    // Trick to 0-center the dft
                    if (i % 2 == 1) {
                        complexOutput[i][0] *= -1;
                        complexOutput[i][1] *= -1;
                    }
                }

                fftw_complex* backupComplexOutput = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * acquisitionParams.samplesPerBuffer));
                std::memcpy(backupComplexOutput, complexOutput, sizeof(fftw_complex) * acquisitionParams.samplesPerBuffer);

                // Push the data to the shared data queue and notify the processing thread that data is ready
                {
                    std::lock_guard<std::mutex> lock(sharedData.mutex);
                    sharedData.dataQueue.push(complexOutput);
                    sharedData.backupDataQueue.push(backupComplexOutput);
                }
                sharedData.dataReadyCondition.notify_one();

                complexOutput = nullptr;
                backupComplexOutput = nullptr;

                buffersCompleted++;
				bytesTransferred += acquisitionParams.bytesPerBuffer;	

                // std::cout << "Acquired " << buffersCompleted << " buffers." << std::endl;

                // double bufferProcTime_sec = (GetTickCount() - startProcTickCount) / 1000.;
	            // std::cout << "Buffer processed in " << bufferProcTime_sec << " sec" << std::endl;
            }
			else {
				// The wait failed
				success = FALSE;

				switch (retCode)
				{
				case ApiWaitTimeout:
					printf("Error: Wait timeout after %lu ms\n", timeout_ms);
					break;

				case ApiBufferOverflow:
					printf("Error: Board overflowed on-board memory\n");
					break;

                case ApiBufferNotReady:
					printf("Error: Buffer not found in list of available\n");
					break;

                case ApiDmaInProgress:
					printf("Error: Buffer not at the head of available buffers\n");
					break;

				default:
					printf("Error: Wait failed with error -- %u\n", GetLastError());
					break;
				}
			}


			// Add the buffer to the end of the list of available buffers so that 
			// the board can fill it with data from another segment of the acquisition.
			if (success) {
				if (!ResetIoBuffer (pIoBuffer)) {
					success = FALSE;
				}
				else {
                    retCode = AlazarPostAsyncBuffer(
                        boardHandle,					// HANDLE -- board handle
                        pIoBuffer->pBuffer,				// void* -- buffer
                        pIoBuffer->uBufferLength_bytes	// U32 -- buffer length in bytes
                    );		
					if (retCode != ApiSuccess) {
						printf("Error: AlazarPostAsyncBuffer failed -- %s\n", AlazarErrorToText(retCode));
						success = FALSE;
					}
				}
			}


			// If the acquisition failed, exit the acquisition loop
			if (!success) {
				break;
            }
	
			// If a key was pressed, exit the acquisition loop					
			if (_kbhit()) {
				printf("Aborted...\n");
				break;
			}

            #if VERBOSE_OUTPUT
			// Display progress
			printf("Completed %u buffers\r", buffersCompleted);
            #endif
		}
        stopTimer(TIMER_ACQUISITION);

        #if VERBOSE_OUTPUT
		double buffersPerSec;
		double bytesPerSec;

        double transferTime_sec = (GetTickCount() - startTickCount) / 1000.;
        double minTime = (double)acquisitionParams.samplesPerBuffer/acquisitionParams.sampleRate*buffersCompleted;

		if (transferTime_sec > 0.)
		{
			buffersPerSec = buffersCompleted / transferTime_sec;
			bytesPerSec = bytesTransferred / transferTime_sec;
		}
		else
		{
			buffersPerSec = 0.;
			bytesPerSec = 0.;
		}

        // Display results
        printf("Captured %d buffers (%.4g buffers per sec)\n", buffersCompleted, buffersPerSec);

		printf("Capture completed in %.3lf sec\n", transferTime_sec);
        printf("Minimum possible time was %.3lf sec for a duty cycle of %.3lf\n", minTime, minTime/transferTime_sec);
		
		printf("Transferred %I64d bytes (%.4g bytes per sec)\n", bytesTransferred, bytesPerSec);
        #endif
	}


    // Signal the end of data acquisition if the decision making didn't stop it
    {
        std::lock_guard<std::mutex> lock(syncFlags.mutex);
        syncFlags.acquisitionComplete = true;
    }


	// Abort the acquisition ("abort" is potentially misleading here - should call even if the acquisition completed normally)
	retCode = AlazarAbortAsyncRead(boardHandle);
	if (retCode != ApiSuccess) {
		printf("Error: AlazarAbortAsyncRead failed -- %s\n", AlazarErrorToText(retCode));
		success = FALSE;
	}


	// Free all memory allocated
	for (bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++) {
		if (IoBufferArray[bufferIndex] != NULL)
			DestroyIoBuffer(IoBufferArray[bufferIndex]);
	}

    return;

    }
    catch(const std::exception& e)
    {
        std::cout << "Acquisition thread exiting due to exception." << std::endl;
        std::cerr << e.what() << '\n';
    }
}
