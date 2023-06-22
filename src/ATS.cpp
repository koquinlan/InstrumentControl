#include "ATS.hpp"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <stdio.h>
#include <conio.h>

#include <string>
#include <vector>

#include <cmath>
#include <algorithm>

#include <fftw3.h>


ATS::ATS(int systemId, int boardId) {
    boardHandle = AlazarGetBoardBySystemID(systemId, boardId);
    if (boardHandle == NULL) {
        throw std::runtime_error("Unable to open board system ID " + std::to_string(systemId) + " board ID " + std::to_string(boardId) + "\n");
    }
}



ATS::~ATS() { 
    if (boardHandle != NULL) {
        AlazarClose(boardHandle);
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



void ATS::setBandwidthLimit(char channel, bool limit) {
    int channelID = getChannelID(channel);

    retCode = AlazarSetBWLimit(
        boardHandle,			// HANDLE -- board handle
        channelID,				// U8 -- channel identifier
        limit					// U32 -- 0 = disable, 1 = enable
    );
    if (retCode != ApiSuccess) {
        throw std::runtime_error(std::string("Error: AlazarSetBWLimit failed -- ") + AlazarErrorToText(retCode) + "\n");
    }
}



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



void ATS::setAcquisitionParameters(U32 sampleRate, U32 samplesPerAcquisition, U32 buffersPerAcquisition, double inputRange, double inputImpedance){
    // 
    if (buffersPerAcquisition <= 0){
        buffersPerAcquisition = suggestBufferNumber(sampleRate, samplesPerAcquisition);
    }


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

    // This is calculated from the above parameters since rounding could cause the actual samples to be different than requested
    acquisitionParams.samplesPerAcquisition = acquisitionParams.samplesPerBuffer*acquisitionParams.buffersPerAcquisition;

    // Send parameters to board
    double realSampleRate = setExternalSampleClock(acquisitionParams.sampleRate);
    if (realSampleRate != acquisitionParams.sampleRate){
        std::cout << "Sample rate adjusted from requsted " << std::to_string(acquisitionParams.sampleRate/1e6) << " MHz to " 
        << std::to_string(realSampleRate/1e6) << " MHz." << std::endl;
    }

    setInputParameters('a', "dc", acquisitionParams.inputRange, acquisitionParams.inputImpedance);
    setBandwidthLimit('a', 1);

    setInputParameters('b', "dc", acquisitionParams.inputRange, acquisitionParams.inputImpedance);
    setBandwidthLimit('b', 1);                                   

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


    // Allocate and post memory for DMA buffers
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
		printf("Capturing %d buffers ... press any key to abort\n", acquisitionParams.buffersPerAcquisition);

		DWORD startTickCount = GetTickCount();
		U32 buffersCompleted = 0;
		INT64 bytesTransferred = 0;

        

		while (buffersCompleted < acquisitionParams.buffersPerAcquisition) {
            // Send a software trigger to begin acquisition
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

			if (retCode == ApiSuccess) {
                // This buffer is full and has been removed from the list of buffers available to the board.
                // DWORD startProcTickCount = GetTickCount();

                std::vector<unsigned short> bufferData(reinterpret_cast<unsigned short*>(pIoBuffer->pBuffer),
                                      reinterpret_cast<unsigned short*>(pIoBuffer->pBuffer) + (acquisitionParams.bytesPerBuffer / sizeof(unsigned short)));

                fullDataA.insert(fullDataA.end(), bufferData.begin(), bufferData.begin() + bufferData.size()/2);
                fullDataB.insert(fullDataB.end(), bufferData.begin() + bufferData.size()/2, bufferData.end());

                for (unsigned int i=0; i < bufferData.size()/2; i++) {
                    int index = buffersCompleted*acquisitionParams.samplesPerBuffer + i;

                    complexOutput[index][0] = (bufferData[i]   / (double)0xFFFF) * 2 * acquisitionParams.inputRange - acquisitionParams.inputRange;
                    complexOutput[index][1] = (bufferData[bufferData.size()/2 + i]  / (double)0xFFFF) * 2 * acquisitionParams.inputRange - acquisitionParams.inputRange;

                    // Trick to 0-center the dft
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

			// Display progress
			printf("Completed %u buffers\r", buffersCompleted);
		}

		// Display results
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
		printf("Transferred %I64d bytes (%.4g bytes per sec)\n", bytesTransferred, bytesPerSec);
	}

	// Abort the acquisition
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

    return complexOutput;
}



U32 ATS::suggestBufferNumber(U32 sampleRate, U32 samplesPerAcquisition){
    // For optimal performance buffer sizes should be between 1MB and 16MB. Absolute maximum is 64MB
    // As per https://docs.alazartech.com/ats-sdk-user-guide/latest/reference/AlazarBeforeAsyncRead.html

    int channelCount = 2; 
    int desiredBytesPerBuffer = (int)4e6; // Shoot for 4MB buffer sizes

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



fftw_complex* processDataFFT(fftw_complex* sampleData, fftw_plan plan, int N) {
    // Sample codes are unsigned by default so that:
    // - a sample code of 0x0000 represents a negative full scale input signal;
    // - a sample code of 0x8000 represents a 0V signal;
    // - a sample code of 0xFFFF represents a positive full scale input signal;
    DWORD startTickCount = GetTickCount();

    fftw_complex *FFTData = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * 2 * N));
    fftw_execute_dft(plan, sampleData, FFTData);
    fftw_execute(plan);

    double transferTime_sec = (GetTickCount() - startTickCount) / 1000.;
	printf("\nProcessing and FFT completed in %.3lf sec\n\n", transferTime_sec);

    return FFTData;
}