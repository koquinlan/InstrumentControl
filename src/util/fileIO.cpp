/**
 * @file fileIO.cpp
 * @author Kyle Quinlan (kyle.quinlan@colorado.edu)
 * @brief 
 * @version 0.1
 * @date 2023-07-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "decs.hpp"

std::vector<std::vector<double>> readCSV(std::string filename, int maxLines = -1) {
    std::vector<std::vector<double>> data;

    std::ifstream file(filename);
    if (!file) {
        // Handle file not found or other errors.
        return data;
    }

    std::string line;
    int linesRead = 0; // Keep track of the number of lines read

    while (std::getline(file, line)) {
        std::vector<double> row;
        std::stringstream ss(line);
        std::string cell;

        while (std::getline(ss, cell, ',')) {
            double value = std::stod(cell);
            row.push_back(value);
        }

        data.push_back(row);

        linesRead++;
        if (maxLines > 0 && linesRead >= maxLines) {
            break; // Stop reading after maxLines lines are read.
        }
    }

    return data;
}


std::vector<double> readVector(const std::string& filename) {
    std::vector<double> data;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return data;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        double value;
        while (iss >> value) {
            data.push_back(value);
            char comma;
            if (!(iss >> comma)) {
                break; // No more characters to read on this line
            }
        }
    }

    file.close();
    return data;
}


void saveVector(std::vector<double> data, std::string filename) {
    std::ofstream dataFile(filename);
    if (dataFile.is_open()) {
        dataFile << data[0];
        for (size_t i = 1; i < data.size(); i++) {
            dataFile << "," << data[i];
        }
        dataFile.close();
    } else {
        std::cerr << "Unable to open file " << filename << " to save data." << std::endl;
    }
}


void saveVector(std::vector<int> data, std::string filename) {
    std::ofstream dataFile(filename);
    if (data.size() == 0){ return; }
    
    if (dataFile.is_open()) {
        dataFile << data[0];
        for (size_t i = 1; i < data.size(); i++) {
            dataFile << "," << data[i];
        }
        dataFile.close();
    } else {
        std::cerr << "Unable to open file " << filename << " to save data." << std::endl;
    }
}


void saveSpectrum(Spectrum data, std::string filename) {
    std::ofstream dataFile(filename);
    if (dataFile.is_open()) {
        dataFile << data.powers[0];
        for (size_t i = 1; i < data.powers.size(); i++) {
            dataFile << "," << data.powers[i];
        }

        dataFile << "\n";

        dataFile << data.freqAxis[0];
        for (size_t i = 1; i < data.freqAxis.size(); i++) {
            dataFile << "," << data.freqAxis[i];
        }

        dataFile.close();
    } else {
        std::cerr << "Unable to open file " << filename << " to save data." << std::endl;
    }
}


void saveCombinedSpectrum(CombinedSpectrum data, std::string filename) {
    std::ofstream dataFile(filename);
    if (dataFile.is_open()) {
        dataFile << data.powers[0];
        for (size_t i = 1; i < data.powers.size(); i++) {
            dataFile << "," << data.powers[i];
        }

        dataFile << "\n";

        dataFile << data.freqAxis[0];
        for (size_t i = 1; i < data.freqAxis.size(); i++) {
            dataFile << "," << data.freqAxis[i];
        }

        dataFile << "\n";

        dataFile << data.sigmaCombined[0];
        for (size_t i = 1; i < data.sigmaCombined.size(); i++) {
            dataFile << "," << data.sigmaCombined[i];
        }

        dataFile.close();
    } else {
        std::cerr << "Unable to open file " << filename << " to save data." << std::endl;
    }
}


std::string getDateTimeString() {
    // Save info for exclusion comparisons
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_s(&localTime, &time);

    // Format the date and time as a string
    char datetimeStr[100];
    std::strftime(datetimeStr, sizeof(datetimeStr), "%Y-%m-%d_%H-%M-%S", &localTime);

    return std::string(datetimeStr);
}



void saveSpectraFromQueue(std::queue<Spectrum>& spectraQueue, std::string filename) {
    std::ofstream dataFile(filename);
    if (dataFile.is_open()) {
        // Write the frequency axis (assuming all spectra have the same frequency axis)
        if (!spectraQueue.empty()) {
            Spectrum firstSpectrum = spectraQueue.front();
            dataFile << firstSpectrum.freqAxis[0];
            for (size_t i = 1; i < firstSpectrum.freqAxis.size(); i++) {
                dataFile << "," << firstSpectrum.freqAxis[i];
            }
            dataFile << "\n";
        } else {
            dataFile << "\n"; // Empty line if the queue is empty
        }

        // Write each spectrum's powers
        while (!spectraQueue.empty()) {
            Spectrum spectrum = spectraQueue.front();
            dataFile << spectrum.powers[0];
            for (size_t i = 1; i < spectrum.powers.size(); i++) {
                dataFile << "," << spectrum.powers[i];
            }
            dataFile << "\n";

            spectraQueue.pop();
        }

        dataFile.close();
    } else {
        std::cerr << "Unable to open file " << filename << " to save data." << std::endl;
    }
}
