#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>
#include <ncurses.h>


WINDOW *init_ncurses();
typedef struct {
    ma_decoder decoder;
    ma_uint64  position;
    ma_uint64  totalFrames;
	double     duration;
} audio_data;

// A function that returns a frame in seconds.
double get_secs_repr(ma_decoder *decoder, ma_uint64 frame)
{
	return (double) (frame / decoder->outputSampleRate);
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    audio_data* pAudioData = (audio_data*)pDevice->pUserData;
    if (pAudioData == NULL) {
        return;
    }

    ma_uint64 framesToRead = frameCount;
    ma_uint64 totalFramesAvailable = pAudioData->totalFrames - pAudioData->position;

    if (framesToRead > totalFramesAvailable) {
        framesToRead = totalFramesAvailable;
    }

    if (framesToRead > 0) {
        ma_decoder_read_pcm_frames(&pAudioData->decoder, pOutput, framesToRead, NULL);
        pAudioData->position += framesToRead;

        // Zero out the remaining buffer if we've reached the end of the audio
        ma_uint64 remainingFrames = frameCount - framesToRead;
        if (remainingFrames > 0) {
            ma_silence_pcm_frames((float*)pOutput + framesToRead * pDevice->playback.channels, remainingFrames, ma_format_f32, pDevice->playback.channels);
        }
    } else {
        ma_silence_pcm_frames(pOutput, frameCount, ma_format_f32, pDevice->playback.channels);
    }

    (void)pInput; // Unused.
}

int main(int argc, char **argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    audio_data audioData = {0};
	char *file = NULL;
	WINDOW *win = NULL;	
	if (argc < 2) 
	{
		printf("[USAGE] %s <mp3-file>\n", argv[0]);
		printf("[ERROR] Please provide an audio file to play.\n");
		return -1;
	}
	
	
	win = init_ncurses();
    // Initialize the decoder
    file = argv[1];
	decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000); // Stereo, 48kHz
    result = ma_decoder_init_file(file, &decoderConfig, &audioData.decoder);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.\n");
        return -1;
    }

	ma_decoder_get_length_in_pcm_frames(&audioData.decoder, &(audioData.totalFrames));
    audioData.position = 0;

	// Initialize the device
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = audioData.decoder.outputFormat;
    deviceConfig.playback.channels = audioData.decoder.outputChannels;
    deviceConfig.sampleRate        = audioData.decoder.outputSampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &audioData;

    ma_device device;
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize playback device.\n");
        ma_decoder_uninit(&audioData.decoder);
        return -1;
    }
    // Start the device
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&audioData.decoder);
        return -1;
    }
	audioData.duration = get_secs_repr(&audioData.decoder, audioData.totalFrames);
	int bar = 100;

	while (audioData.totalFrames - audioData.position > 0)
	{
		 
		 mvprintw(0, 0, "Playing.");
		 mvprintw(1, 0, "Length -> %fs", audioData.duration);
		 mvprintw(2, 0, "Cursor -> %fs", get_secs_repr(&audioData.decoder, audioData.position));
		 int current = (get_secs_repr(&audioData.decoder, audioData.position) / (audioData.duration)) * bar;
			 

		 for (int x = 0; (x < current); ++x)
			mvaddch(3, x, ' ');
		 refresh();
	}

	mvprintw(0, 0, "Finished playing.");
    // Clean up
    ma_device_uninit(&device);
    ma_decoder_uninit(&audioData.decoder);
	endwin();
    return 0;
}


WINDOW *init_ncurses() 
{
	WINDOW *win = initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
	start_color();
	init_pair(1, COLOR_YELLOW, COLOR_GREEN);
    set_escdelay(0); 
    {
        // Mouse stuff.
        mousemask(ALL_MOUSE_EVENTS, NULL);
        mouseinterval(0);
    }
    return win;
}
