//#include <Arduino.h>
#pragma once

template <typename T>
class Smoothed { 
  private:
    byte smoothReadingsFactor = 10; // The smoothing factor. In avergare mode, this is the number of readings to average.   
    byte smoothReadingsPosition = 0; // Current position in the array
    byte smoothReadingsNum = 0; // Number of readings currently being averaged
    T *smoothReading; // Array of readings
    T smoothTotal;
  public:
    Smoothed();
    ~Smoothed(); // Destructor to clean up when class instance killed
    bool begin (T initValue, byte smoothFactor = 10);
    void add (T newReading);
    T getAverage ();
    T get (byte);

};

// Constructor
template <typename T>
Smoothed<T>::Smoothed () { // Constructor
  
}

// Destructor
template <typename T>
Smoothed<T>::~Smoothed () { // Destructor
  delete[] smoothReading;
}

// Inintialise the array for storing sensor values
template <typename T>
bool Smoothed<T>::begin (T initValue, byte smoothFactor) { 
  smoothReadingsFactor = smoothFactor; 
  smoothReading = new T[smoothReadingsFactor]; 
  for (smoothReadingsPosition = 0; smoothReadingsPosition < smoothReadingsFactor; smoothReadingsPosition++) {
      smoothReading[smoothReadingsPosition] = initValue;
      smoothTotal += initValue;
  }
  smoothReadingsPosition = 0;
  return smoothReading != nullptr;
}

// Add a value to the array
template <typename T>
void Smoothed<T>::add (T newReading) {
    smoothTotal = smoothTotal - smoothReading[smoothReadingsPosition] + newReading;
    smoothReading[smoothReadingsPosition] = newReading; // Add the new value
    smoothReadingsPosition = (smoothReadingsPosition + 1) % smoothReadingsFactor;

}

// Get the smoothed result
template <typename T>
T Smoothed<T>::getAverage () {
    return smoothTotal/smoothReadingsFactor;
}

// template <typename T>
// T Smoothed<T>::get (byte i) {
//     return smoothReading[i];
// }
