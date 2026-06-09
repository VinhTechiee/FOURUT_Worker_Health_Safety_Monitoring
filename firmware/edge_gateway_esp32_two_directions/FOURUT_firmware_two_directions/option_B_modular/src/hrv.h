
#ifndef FOURUT_HRV_H
#define FOURUT_HRV_H

bool isValidIBI(unsigned long ibi);
unsigned long medianFilter(unsigned long value);
float calculateSDNN(unsigned long ibis[], int count);
float calculateRMSSD(unsigned long ibis[], int count);
void updateHRVOnEdge(int bpm, int spo2, float svm);

#endif
