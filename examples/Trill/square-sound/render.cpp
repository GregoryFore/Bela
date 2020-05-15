 /**
 * \example Trill/trill-square-oscillator-pad-A
 *
 * Trill Square oscillator pad 
 * ===========================
 *
 * This project showcases an example of how to communicate with the Trill Square sensor using
 * the Trill library and sonifies the X-Y position and size of the touch via an oscillator.
 *
 * The Trill sensor is scanned on an auxiliary task running parallel to the audio thread 
 * and the X-Y position and size stored on global variables.
 *
 * The vertical position of the touch is mapped to frequency, while the horizontal position
 * maps to left/right panning. Touch size is used to control the overal amplitude of the
 * oscillator.
 * Changes in frequency, amplitude and panning are smoothed using LP filters to avoid artifacts.
 *
 **/

#include <Bela.h>
#include <cmath>
#include <libraries/Trill/Trill.h>
#include <libraries/OnePole/OnePole.h>
#include <libraries/Oscillator/Oscillator.h>

// Trill object declaration
Trill touchSensor;

// Horizontal and vertical position for Trill sensor
float gTouchPosition[2] = { 0.0 , 0.0 };
// Touch size
float gTouchSize = 0.0;

// Oscillator object declaration
Oscillator osc;

// Range for oscillator frequency mapping
float gFreqRange[2] = { 200.0, 1500.0 };

// One Pole filters objects declaration
OnePole freqFilt, panFilt, ampFilt;

// Default  panning values for the sinewave
float gAmpL = 1.0;
float gAmpR = 1.0;

// Sleep time for auxiliary task
int gTaskSleepTime = 5000;

/*
 * Function to be run on an auxiliary task that reads data from the Trill sensor.
 * Here, a loop is defined so that the task runs recurrently for as long as the 
 * audio thread is running.
 */
void loop(void*)
{
	while(!gShouldStop)
	{
		// Read locations from Trill sensor
		touchSensor.readLocations(); 
		gTouchSize = touchSensor.compoundTouchSize();
		gTouchPosition[0] = touchSensor.compoundTouchHorizontalLocation();
		gTouchPosition[1] = touchSensor.compoundTouchLocation();
		usleep(gTaskSleepTime);
	}
}

bool setup(BelaContext *context, void *userData)
{
	if(touchSensor.setup(1, 0x28, Trill::CENTROID) != 0) {
		fprintf(stderr, "Unable to initialise touch sensor\n");
		return false;
	}
	
	touchSensor.printDetails();

	// Exit program if sensor is not a Trill Square
	if(touchSensor.deviceType() != Trill::SQUARE) {
		fprintf(stderr, "This example is supposed to work only with the Trill SQUARE. \n You may have to adapt it to make it work with other Trill devices.\n");
		return false;
	}

	// Set and schedule auxiliary task for reading sensor data from the I2C bus
	Bela_scheduleAuxiliaryTask(Bela_createAuxiliaryTask(loop, 50, "I2C-read", NULL));	
		
	// Setup low pass filters for smoothing frequency, amplitude and panning
	freqFilt.setup(1, context->audioSampleRate); // Cut-off frequency = 1Hz
	panFilt.setup(1, context->audioSampleRate); // Cut-off frequency = 1Hz
	ampFilt.setup(1, context->audioSampleRate); // Cut-off frequency = 1Hz

	// Setup triangle oscillator	
	osc.setup(context->audioSampleRate, Oscillator::triangle);

	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
	
		float frequency;
		// Map Y-axis to a frequency range	
		frequency = map(gTouchPosition[1], 0, 1, gFreqRange[0], gFreqRange[1]);
		// Smooth frequency using low-pass filter
		frequency = freqFilt.process(frequency);
	
		// Smooth panning (given by the X-axis) changes using low-pass filter
		float panning = panFilt.process(gTouchPosition[0]);
		// Calculate amplitude of left and right channels
		gAmpL = 1 - panning; 
		gAmpR = panning;
	
		// Smooth changes in the amplitude of the oscillator (given by touch
		// size) using a low-pass filter	
		float amplitude = ampFilt.process(gTouchSize);
		// Calculate output of the oscillator	
		float out = amplitude * osc.process(frequency);

		// Write oscillator to left and right channels
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			if(channel == 0) {
				audioWrite(context, n, channel, gAmpL*out);
			} else if (channel == 1) {
				audioWrite(context, n, channel, gAmpR*out);	
			}
		}
	}
}

void cleanup(BelaContext *context, void *userData)
{}
