#include <miplayer.h>
#include <unistd.h>

// TODO: make the volume editable using left and right arrow keys.
int main(int argc, char **argv)
{
	if (argc < 2) 
	{
		printf("[USAGE] %s <mp3-file>\n", argv[0]);
		printf("[ERROR] Please provide an audio file to play.\n");
		return -1;
	}

	InitPlayerLock();
	MiAudioPlayer *player = init_player(argv[1]);
	pthread_t inputThread;
	pthread_t playerThread;
	
	pthread_create(&playerThread, NULL, player_visualize_audio, player);
	pthread_create(&inputThread,  NULL, player_get_input,       player);
	
	pthread_join(playerThread, NULL);
	pthread_join(inputThread,  NULL);
	// Clean
    pthread_cancel(playerThread);
    pthread_cancel(inputThread);
	DistroyPlayerMutex(&player->Mutx);
	deallocate_player(player);

	return 0;
}

