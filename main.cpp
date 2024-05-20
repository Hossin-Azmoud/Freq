#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>

#define SPACE ' '
WINDOW *init_ncurses();
pthread_mutex_t ncurses_mutex;

typedef struct {
    ma_decoder decoder;
    ma_uint64  position = 0;
	float 	   *samples;
    ma_uint64  totalFrames = 0;
	double     duration = 0;
} MiAudio;

typedef struct {
	MiAudio audio;
	char 	file[512];
	float   volume = 0.5f;
	uint8_t play = 0;
	uint8_t quit = 0;
	uint32_t  fcount = 0;
} MiAudioPlayer;

// A function that returns a frame in seconds.
double get_secs_repr(ma_decoder *decoder, ma_uint64 frame)
{
	return (double) (frame / decoder->outputSampleRate);
}

static void Lockcurses()
{
	pthread_mutex_lock(&ncurses_mutex);
}

static void unLockcurses() 
{
	pthread_mutex_unlock(&ncurses_mutex);
}

// Get audio player commands (In a seperate thread of crs)
void *audio_player_get_input(void *player_data)
{
	MiAudioPlayer *player = (MiAudioPlayer *) player_data;
	int c = 0;
	while (1 && !(player->quit))
	{
		Lockcurses();
		c = getchar();
		switch(c) {
		case 'q': {
			player->quit = 1;
		} break;
		case 'w': {
			if (player->volume < 1.0f) {
				player->volume += 0.1f;
			}
		} break;
		case 's': {
			if (player->volume > 0.0f) {
				player->volume -= 0.1f;
				if (player->volume < 0.0f)
					player->volume = 0.0f;
			}
		} break;
		case SPACE: {
			player->play = !player->play;
		} break;
		}
		unLockcurses();
	}

	return NULL;
}


void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	// ma_decoder_read_pcm_frames(&audio->decoder, pOutput, framesToRead, NULL);
	MiAudioPlayer *player = (MiAudioPlayer *) pDevice->pUserData;
    MiAudio *audio = &player->audio; 
	ma_uint32 channels = pDevice->playback.channels;
	if (audio == NULL || !(player->play) || player->quit) 
        return;
	
	ma_uint32 leftSamples = (audio->totalFrames - audio->position);
	ma_uint32 samples_to_process = ((frameCount > leftSamples) ? leftSamples : frameCount);

	if (leftSamples > 0)
	{
		ma_decoder_read_pcm_frames(&audio->decoder, pOutput, samples_to_process , NULL);
		audio->position += samples_to_process;
	}

    (void)pInput; // Unused.
}

void *play_and_visualize_audio(void *audioPlayerData) 
{
    MiAudioPlayer *player = (MiAudioPlayer *) audioPlayerData;
	ma_result result;
    ma_decoder_config decoderConfig;
	WINDOW *win = init_ncurses();
	decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000); // Stereo, 48kHz
    result = ma_decoder_init_file(player->file, &decoderConfig,	&(player->audio.decoder));
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.\n");
        return NULL;
    }

	ma_decoder_get_length_in_pcm_frames(&player->audio.decoder, &(player->audio.totalFrames));

	// Start the device
	// player->audio.samples = (float *) malloc(sizeof(float) * player->audio.totalFrames * player->audio.decoder.outputChannels);
	// memset(player->audio.samples, 0, sizeof(float) * player->audio.totalFrames * player->audio.decoder.outputChannels);
	

	// Initialize the device
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = player->audio.decoder.outputFormat;
    deviceConfig.playback.channels = player->audio.decoder.outputChannels;
    deviceConfig.sampleRate        = player->audio.decoder.outputSampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = player;

    ma_device device;
    
	result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize playback device.\n");
        ma_decoder_uninit(&player->audio.decoder);
        return NULL;
    }
		
	result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&player->audio.decoder);
        return NULL;
    }

	player->audio.duration = get_secs_repr(&player->audio.decoder, player->audio.totalFrames);
	int bar = 100;
	curs_set(0);

	while (player->audio.totalFrames - player->audio.position > 0 && !(player->quit))
	{
		Lockcurses();
		mvprintw(0, 0, "Playing.");
		mvprintw(1, 0, "Length -> %fs", player->audio.duration);
		mvprintw(2, 0, "Cursor -> %fs", get_secs_repr(&player->audio.decoder, player->audio.position));
		mvprintw(3, 0, "Chan -> %i", deviceConfig.playback.channels);
		mvprintw(4, 0, "Rate -> %i", deviceConfig.sampleRate);
		mvprintw(5, 0, "VOL-> %f", player->volume);
		
		int current = (get_secs_repr(&player->audio.decoder, player->audio.position) / (player->audio.duration)) * bar;
		for (int x = 0; (x < current); ++x)
			mvaddch(6, x, ' ');
		
		mvchgat(6, 0, current, A_NORMAL, 1, NULL);	
		
		mvprintw(7, 0, "frameCount -> %li", player->fcount);
		refresh();
		unLockcurses();
	}

    ma_device_uninit(&device);
    ma_decoder_uninit(&player->audio.decoder);
	// Clean up
	endwin();
	return NULL;
}

// TODO: make the volume editable using left and right arrow keys.
int main(int argc, char **argv)
{
    MiAudioPlayer audioPlayer = {0};
	audioPlayer.play = 1; 	
	if (argc < 2) 
	{
		printf("[USAGE] %s <mp3-file>\n", argv[0]);
		printf("[ERROR] Please provide an audio file to play.\n");
		return -1;
	}

    // Initialize the decoder
	strcpy(audioPlayer.file, argv[1]);
	pthread_t inputThread;
	pthread_t playerThread;
	
	pthread_create(&playerThread, NULL, play_and_visualize_audio, &audioPlayer);
	pthread_create(&inputThread,  NULL, audio_player_get_input,   &audioPlayer);

	pthread_join(playerThread, NULL);
	pthread_join(inputThread,  NULL);
    
	// Clean
    pthread_cancel(playerThread);
    pthread_cancel(inputThread);
	pthread_mutex_destroy(&ncurses_mutex);
	return 0;
}


WINDOW *init_ncurses() 
{
	WINDOW *win = initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_YELLOW);
    // Initialize the mutex
    pthread_mutex_init(&ncurses_mutex, NULL);
	set_escdelay(0); 
    {
        // Mouse stuff.
        mousemask(ALL_MOUSE_EVENTS, NULL);
        mouseinterval(0);
    }
    return win;
}

