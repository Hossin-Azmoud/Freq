#include <miplayer.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

pthread_mutex_t Mutx;
void DistroyPlayerMutex()
{
	pthread_mutex_destroy(&Mutx);
}

void InitPlayerLock()
{
	pthread_mutex_init(&Mutx, NULL);
	printf("initing Lock\n");
}

void LockPlayer()
{
	pthread_mutex_lock(&Mutx);
}

void unLockPlayer()
{
	pthread_mutex_unlock(&Mutx);
}

static WINDOW *init_ncurses() 
{
	WINDOW *win = initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_YELLOW);
	set_escdelay(0); 
    {
        // Mouse stuff.
        mousemask(ALL_MOUSE_EVENTS, NULL);
        mouseinterval(0);
    }
    return win;
}


MiAudioPlayer *init_player(char *file)
{
	MiAudioPlayer *player = (MiAudioPlayer *)malloc(sizeof(MiAudioPlayer));
	MiAudio       *audio  = &player->audio;
	ma_result result;
	memset(player, 0, sizeof(*player));
	strcpy(player->file, file);

	// Init configuration.
	player->volume = 1.0f;
	player->play   = 0;
 	player->quit   = 0;
    ma_decoder_config decoderConfig;	
	decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000); // Stereo, 48kHz
    result = ma_decoder_init_file(player->file, &decoderConfig,	&(audio->decoder));

	if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.\n");
        return NULL;
    }

	// initialize the device that will play the audio.
    ma_device_config deviceConfig  = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = player->audio.decoder.outputFormat;
    deviceConfig.playback.channels = player->audio.decoder.outputChannels;
    deviceConfig.sampleRate        = player->audio.decoder.outputSampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = player;
    player->audio.srate            = player->audio.decoder.outputSampleRate;
	init_audio(audio);
	
	ma_device dev;
	result = ma_device_init(NULL, &deviceConfig, &dev);

	if (result != MA_SUCCESS) {
        printf("Failed to initialize playback device.\n");
        ma_decoder_uninit(&player->audio.decoder);
        return NULL;
    }

	result = ma_device_start(&dev);


	if (result != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&dev);
        ma_decoder_uninit(&player->audio.decoder);
    	exit(1);
	}

	player->dev = dev;
	return (player);
}

void deallocate_player(MiAudioPlayer *player)
{
    ma_device_uninit(&player->dev);
    ma_decoder_uninit(&player->audio.decoder);
	endwin();
	free(player->audio.samples);
	free(player);
}

void start_playing(MiAudioPlayer *player)
{
	ma_result result = ma_device_start(&player->dev);
    if (result != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&player->dev);
        ma_decoder_uninit(&player->audio.decoder);
    	exit(1);
	}
}

void player_play(MiAudioPlayer *player)
{
	player->play = 1;
}

void player_pause(MiAudioPlayer *player)
{
	player->play = 0;
}

void toggle_play(MiAudioPlayer *player)
{
	if (player->play) {
	   	player_pause(player);
		return;
	}
	player_play(player);
}

void seek_position(MiAudioPlayer *player, int offset_in_secs)
{
	ma_uint64 sr = player->audio.srate;
	MiAudio  *audio = &player->audio;
	int moveoffsetframes = (offset_in_secs * sr);
	if (moveoffsetframes > 0) {
		// Move to the left.
		if (player->audio.position + moveoffsetframes >=  player->audio.totalFrames * audio->totalFrames * audio->decoder.outputChannels) {
			moveoffsetframes = (player->audio.totalFrames - player->audio.position);
		}

		if (player->audio.position < player->audio.totalFrames * audio->totalFrames * audio->decoder.outputChannels) {
			player->audio.position += moveoffsetframes; // Basically seek to 5 seconds after;
		}
	} else {
		// Move backwards to the right.
		if ((int)player->audio.position + (int64_t)moveoffsetframes < 0) {
			moveoffsetframes = player->audio.position; 
		}

		if (player->audio.position > 0) {
			player->audio.position += moveoffsetframes; // Basically rewind 5 seconds
		}
	}
}

void rewind_audio(MiAudioPlayer *player)
{
	player->audio.position = 0; // Replay.
}

void *player_get_input(void *player_data)
{
	MiAudioPlayer *player = (MiAudioPlayer *) player_data;
	int c = 0;

	while (!player->quit)
	{
		c = getchar();
		LockPlayer();
		switch(c) {
		case 'q': {
			player->quit = 1;
		} break;
		case 'w': {
			volume(player, player->volume + .1f);
		} break;
		case 's': {
			volume(player, player->volume - .1f);
		} break;
		case SPACE: {
			toggle_play(player);
		} break;
		case 'a': {
			seek_position(player, -5);
		} break;
		case 'd': {
			seek_position(player, 5);
		} break;
		case 'r': { // rezero.
			rewind_audio(player);
		} break;
		}
		unLockPlayer();
	}
	return NULL;
}
												 
void volume(MiAudioPlayer *player, float new_volume)
{
	player->volume = new_volume;

	if (new_volume > 1.0f)
		player->volume = 1.0f;
	if (new_volume < 0.0f)
		player->volume = 0.0f;
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	
	MiAudioPlayer *player = (MiAudioPlayer *) pDevice->pUserData;
    MiAudio *audio = &player->audio; 

	ma_uint32 channels = pDevice->playback.channels;
	if (audio == NULL || player->quit) 
        return;
	
	ma_uint32 leftSamples = (audio->totalFrames - audio->position);
	ma_uint32 samples_to_process = ((frameCount > leftSamples) ? leftSamples : frameCount);
	audio->framecount = samples_to_process;
	LockPlayer();

	if (leftSamples > 0 && player->play)
	{
		float* out = (float*)pOutput;
		for (int k = 0; k < samples_to_process * channels; ++k)
			out[k] = *((audio->samples + audio->position) + k) * player->volume;

		// TODO: Copy the samples to be visualized,	
		audio->position += samples_to_process * channels;
	}

    (void)pInput; // Unused.
	unLockPlayer();
}

void *player_visualize_audio(void *audioPlayerData)
{	
	// We load the audio in this thread for some reasonl
	LockPlayer();
	MiAudioPlayer *player = (MiAudioPlayer *) audioPlayerData;	
	MiAudio       *audio  = &player->audio;
	int h, w;
	int bar = 100; // 100% i meant
	WINDOW *win = init_ncurses();
	getmaxyx(win, h, w);
	player->play = 1;
	unLockPlayer();
	curs_set(0);
	while (player->audio.totalFrames - player->audio.position > 0 && !(player->quit))
	{	
		erase();

		mvprintw(0, 0, "Playing %s", player->file);
		mvprintw(1, 0, "Length  %fs", player->audio.duration);
		mvprintw(2, 0, "Cursor  %fs", get_frames_as_seconds(&player->audio.decoder, player->audio.position));
		mvprintw(3, 0, "Chan    %i",  player->audio.decoder.outputChannels);
		mvprintw(4, 0, "Rate    %i",  player->audio.decoder.outputSampleRate);
		mvprintw(5, 0, "VOL	    %f [UP: w][DOWN: S]", player->volume);
		int current = 
			(get_frames_as_seconds(&player->audio.decoder, player->audio.position) / (player->audio.duration)) * bar;
		
		for (int x = 0; (x < current); ++x)
			mvaddch(6, x, ' ');
		mvchgat(6, 0, current, A_NORMAL, 1, NULL);
		
		int viw = 80;
		int vih = 50;
		
		// render the possible freqs
		int N = player->audio.framecount;
		
		if (N > viw - 2)
			N = viw - 2;

		float t1 = 0;

		for (int k = 0, x = (w/2-viw/2); k < (N * (player->audio.decoder.outputChannels)); k += 2, x++) {
			// The value of the sample.
			t1 = *((player->audio.samples + player->audio.position) + k) * vih;
			if (t1 < 0) t1 = -t1;

			for (int y = (h/2+vih/2); (y > (h/2+vih/2) - t1); --y) {
				mvaddch(y, x, ' ');
				// mvaddch(6 + y + 1, x, ' ');	
				mvchgat(y, x, 1, A_NORMAL, 1, NULL);
			}
		}
/*
		for (int k = 0, y = 1; k < (N * (player->audio.decoder.outputChannels)); k += 2, y++) {
			t1 = *((player->audio.samples + player->audio.position) + k) * 100;
			if (t1 < 0) t1 = -t1;
			if (t1 > t2) {

				for (int x = t2; (x < t1); ++x) {
					mvaddch(6 + y + 1, x, ' ');	
				}
				mvchgat(6 + y + 1, 0, t1, A_NORMAL, 1, NULL);
			} else {
				for (int x = 0; (x < t1); ++x) {
					mvaddch(6 + y + 1, x, ' ');	
				}
				mvchgat(6 + y + 1, 0, t1, A_NORMAL, 1, NULL);
			}
			t2 = t1;
		}
*/
		refresh();
	}
	return NULL;
}

