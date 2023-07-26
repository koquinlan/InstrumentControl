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
            try {
                double value = std::stod(cell);
                row.push_back(value);
            } catch (const std::exception& e) {
                // Handle invalid data in CSV (e.g., non-numeric entries).
            }
        }

        data.push_back(row);

        linesRead++;
        if (maxLines > 0 && linesRead >= maxLines) {
            break; // Stop reading after maxLines lines are read.
        }
    }

    return data;
}