#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <math.h>

// gain parameters (play around with these)
int microphone_gain_dB = 40; // max value 62; values greater than 62dB will not have any effect
float digital_gain = 1; // you shouldn't need to change this, but feel free to see what it does
// humans don't have equal volume sensitivity across all frequencies
// (our ears are more sensitive to frequencies between 1kHz and 3kHz).
// here, we set a digital gain value for each frequency bin to change the brightness of the spectrogram
// so it more closely matches how loud we perceive each frequency
float levelcurve_dB[] = {-15, -5, -5, -1, 0, 0, 4, 5, -4, -12};

AudioInputI2S            i2s1;
AudioMixer4              mixer1;
AudioOutputI2S           i2s2;
AudioAnalyzeFFT1024      fft1024;
AudioConnection          patchCord1(i2s1, 0, mixer1, 0);
AudioConnection          patchCord2(i2s1, 0, i2s2, 0);
AudioConnection          patchCord3(i2s1, 1, mixer1, 1);
AudioConnection          patchCord4(i2s1, 1, i2s2, 1);
AudioConnection          patchCord5(mixer1, fft1024);
AudioControlSGTL5000     audioShield;

int numLEDs = 10;
// if you change the order of wires to LEDs, be sure to change this array
int ledPins[] = {0, 1, 2, 3, 4, 5, 9, 14, 15, 22};
int shown[10];

int low_bins[10];
int high_bins[10];

void setup() {
  AudioMemory(12);
  audioShield.enable();
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.micGain(microphone_gain_dB);
  mixer1.gain(0, digital_gain/2);
  mixer1.gain(1, digital_gain/2);
  for (int i = 0; i < numLEDs; i++) {
    pinMode(ledPins[i], OUTPUT);
    shown[i] = 0;
  }
  Serial.begin(9600);
  float e = FindE(numLEDs,256);
  int count = 0;
  if (e) {
    for (int b = 0; b < numLEDs; b++) {
      float n = pow(e, b);
      int d = int(n + 0.5);
      low_bins[b] = count;
      Serial.print(((float)count)*(48000./1024.));
      Serial.print(" ");
      count += d - 1;
      high_bins[b] = count;
      Serial.println(((float)count)*(48000./1024.));
      count++;
    }
  } else {
    Serial.println("error\n");
  }
}

float bin_energy(int k, float level) {
  // applies digital gain to each bin to match human perception across frequency ranges
  return pow(2, 6) * pow(10, levelcurve_dB[k] / 10) * pow(level, 4);
}

float FindE(int bands, int bins) {
  // helper function for calculating which frequency bins get summed together into an LED
  float increment = 0.1, eTest, n;
  int b, count, d;

  for (eTest = 1; eTest < bins; eTest += increment) {     // Find E through brute force calculations
    count = 0;
    for (b = 0; b < bands; b++) {                         // Calculate full log values
      n = pow(eTest, b);
      d = int(n + 0.5);
      count += d;
    }
    if (count > bins) {     // We calculated over our last bin
      eTest -= increment;   // Revert back to previous calculation increment
      increment /= 10.0;    // Get a finer detailed calculation & increment a decimal point lower
    }
    else if (count == bins)   // We found the correct E
      return eTest;       // Return calculated E
    if (increment < 0.0000001)        // Ran out of calculations. Return previous E. Last bin will be lower than (bins-1)
      return (eTest - increment);
  }
  return 0;                 // Return error 0
}

void loop() {
  // put your main code here, to run repeatedly:
  float total_energy = pow(fft1024.read(0,511), 0.5);
  if (fft1024.available()) {
    for (int i = 0; i < numLEDs; i++) {
      int analogval = 256 * total_energy * bin_energy(i, fft1024.read(low_bins[i], high_bins[i]));
      analogval = min(analogval, 255);
      if (analogval > shown[i]) {
        shown[i] = analogval;
      } else {
        shown[i] = floor(0.9 * shown[i]);
      }
      analogWrite(ledPins[i], shown[i]);
    }
  }
}
