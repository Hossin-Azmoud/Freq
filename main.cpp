#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <assert.h>
#include <sys/wait.h>

#include <complex.h>
// const double PI = acos(-1);
#define SPACE ' '
WINDOW *init_ncurses();
pthread_mutex_t ncurses_mutex;

typedef struct {
    ma_decoder decoder;
    ma_uint64  position = 0;
    ma_uint64  srate = 0;
	float 	   *samples;
    ma_uint64  totalFrames = 0;
	double     duration = 0;
	bool       ready = 0;
	uint32_t framecount = 0;
} MiAudio;

typedef struct {
	MiAudio  audio;
	char 	 file[512];
	float    volume = 0.5f;
	uint8_t  play = 0;
	uint8_t  quit = 0;
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
	ma_uint64 sr;
	ma_uint64 factor_to_move;
	while (!(player->quit))
	{

		c = getchar();
		Lockcurses();
		sr = player->audio.srate;
		factor_to_move = (5 * sr);

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
			if (player->play)
				player->play = 0;
			else
				player->play = 1;
		} break;
		case 'a': { // sub cursor pos

			if ((int64_t)player->audio.position - (int64_t)factor_to_move < 0) {
				factor_to_move = player->audio.position; 
			}

			if (player->audio.position > 0) {
				player->audio.position = (player->audio.position - factor_to_move); // Basically rewind 5 seconds
			}
		} break;
		case 'd': { // add to cursor pos
			if (player->audio.position + factor_to_move >=  player->audio.totalFrames) {
				factor_to_move = player->audio.totalFrames - player->audio.position;
			}
			if (player->audio.position < player->audio.totalFrames) {
				player->audio.position = (player->audio.position + factor_to_move); // Basically seek to 5 seconds after;
			}
		} break;
		case 'r': { // rezero.
			player->audio.position = 0; // Replay.
		} break;
		case 'g': {
			player->audio.position = factor_to_move * 5; // Replay.
		} break;
		}	
		unLockcurses();
	}
	return NULL;
}


void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	// ma_decoder_read_pcm_frames(&audio->decoder, pOutput, framesToRead, NULL);
	Lockcurses();
	MiAudioPlayer *player = (MiAudioPlayer *) pDevice->pUserData;
    MiAudio *audio = &player->audio; 
	ma_uint32 channels = pDevice->playback.channels;
	if (audio == NULL || player->quit) 
        return;
	
	ma_uint32 leftSamples = (audio->totalFrames - audio->position);
	ma_uint32 samples_to_process = ((frameCount > leftSamples) ? leftSamples : frameCount);
	audio->framecount = samples_to_process;
	
	if (leftSamples > 0 && player->play)
	{
		float* out = (float*)pOutput;
		for (int k = 0; k < samples_to_process * channels; ++k)
			out[k] = *((audio->samples + audio->position) + k) * player->volume;

		// TODO: Copy the samples to be visualized,	
		audio->position += samples_to_process * channels;
	}

    (void)pInput; // Unused.
	unLockcurses();
}

void *play_and_visualize_audio(void *audioPlayerData) 
{
    Lockcurses();
	MiAudioPlayer *player = (MiAudioPlayer *) audioPlayerData;
	WINDOW *win = NULL;
	ma_result result;
    ma_decoder_config decoderConfig;	
	decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000); // Stereo, 48kHz
    result = ma_decoder_init_file(player->file, &decoderConfig,	&(player->audio.decoder));
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.\n");
        return NULL;
    }

	ma_decoder_get_length_in_pcm_frames(&player->audio.decoder, &(player->audio.totalFrames));

	// Load all the frames into the samples buffer.
	player->audio.samples = (float *) malloc(sizeof(float) * (player->audio.totalFrames * player->audio.decoder.outputChannels + 1));
	memset(player->audio.samples, 
			0, sizeof(float) 
			* (player->audio.totalFrames * player->audio.decoder.outputChannels + 1));
	ma_uint64 read = 0;
	ma_decoder_read_pcm_frames(&player->audio.decoder, 
			player->audio.samples, 
			player->audio.totalFrames,
			&read
			);

	assert(read == player->audio.totalFrames);
	win = init_ncurses();
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
	int h, w;
	getmaxyx(win, h, w);
	player->audio.srate = deviceConfig.sampleRate;
	unLockcurses(); // Everythin is ready
	while (player->audio.totalFrames - player->audio.position > 0 && !(player->quit))
	{
		
		erase();
		mvprintw(0, 0, "Playing.");
		mvprintw(1, 0, "Length %fs", player->audio.duration);
		mvprintw(2, 0, "Cursor %fs", get_secs_repr(&player->audio.decoder, player->audio.position));
		mvprintw(3, 0, "Chan   %i", deviceConfig.playback.channels);
		mvprintw(4, 0, "Rate   %i", deviceConfig.sampleRate);
		mvprintw(5, 0, "VOL	   %f [UP: w][DOWN: S]", player->volume);
	
		int current = (get_secs_repr(&player->audio.decoder, player->audio.position) / (player->audio.duration)) * bar;
		for (int x = 0; (x < current); ++x)
			mvaddch(6, x, ' ');
		mvchgat(6, 0, current, A_NORMAL, 1, NULL);

		// render the possible freqs
		int N = player->audio.framecount;
		
		if (N > h)
			N = h;

		for (int k = 0, y = 1; k < (N * (player->audio.decoder.outputChannels)); k += 2, y++) {
			float t = *((player->audio.samples + player->audio.position) + k) * 100;
			if (t < 0) t = -t;
			for (int x = 0; (x < t); ++x)
				mvaddch(6 + y + 1, x, ' ');
			mvchgat(6 + y + 1, 0, t, A_NORMAL, 1, NULL);
		}
		refresh();
		
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

