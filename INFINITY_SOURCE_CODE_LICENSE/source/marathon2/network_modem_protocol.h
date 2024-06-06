/*

	network_modem_protocol.h
	Tuesday, October 17, 1995 5:47:20 PM- rdm created.

*/

boolean ModemEnter(void);
void ModemExit(void);

boolean ModemSync(void);
boolean ModemUnsync(void);

short ModemAddDistributionFunction(NetDistributionProc proc, boolean lossy);
void ModemDistributeInformation(short type, void *buffer, short buffer_size, 
	boolean send_to_self);
void ModemRemoveDistributionFunction(short type);

boolean ModemGather(void *game_data, short game_data_size, void *player_data, 
	short player_data_size);

boolean ModemGatherPlayer(short player_index, CheckPlayerProcPtr check_player);
boolean ModemGameJoin(char *player_name, char *player_type, void *player_data, 
	short player_data_size, short version_number);

void ModemCancelJoin(void);
void ModemCancelGather(void);

short ModemUpdateJoinState(void);

short ModemGetLocalPlayerIndex(void);
short ModemGetPlayerIdentifier(short player_index);
short ModemGetNumberOfPlayers(void);

void *ModemGetPlayerData(short player_index);
void *ModemGetGameData(void);

boolean ModemNumberOfPlayerIsValid(void);

boolean ModemStart(void);
boolean ModemChangeMap(struct entry_point *entry);

long ModemGetNetTime(void);
