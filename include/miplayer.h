#ifndef MIPLAYER
#define MIPLAYER
#include <miaudio.h>

typedef struct {
	MiAudio   audio;
	char 	  file[512];
	float     volume;
	uint8_t   play;
	uint8_t   quit;
	ma_device dev;
	// WINDOW    *win;
	pthread_mutex_t Mutx; // a mutex to synchronize the plyer and visualizer.
} MiAudioPlayer;
// NOTE: This api will abstract the miniaudio api and make it more easy to work with.
MiAudioPlayer *init_player(char *file);
void deallocate_player(MiAudioPlayer *player);
void start_playing(MiAudioPlayer *player);
void player_play(MiAudioPlayer *player);
void toggle_play(MiAudioPlayer *player);
void player_pause(MiAudioPlayer *player);
void seek_position(MiAudioPlayer *player, int offset_in_secs);
void rewind_audio(MiAudioPlayer *player);
void *player_get_input(void *player_data); // this function gets the player in a seperate thread and it get input from the user
												 // and makes any neccessiry adjusments.
void volume(MiAudioPlayer *player, float new_volume); // apply a new volume if it is provided.
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount); // This function send loaded data from the MiAudio samples to the device.
																								 // doing any neccessiry computations fft(...) dft(...) volume(...)

void *player_visualize_audio(void *audioPlayerData); // this function is used to visualize the frames using ncurses in a seperate thread. it is like 
													   // the entry point of visualization.

// synch/mutex
void DistroyPlayerMutex();
void LockPlayer();
void unLockPlayer();
void InitPlayerLock();
#endif // MIPLAYER
