#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include "commons.h"

struct ClientInfo
{
    Sock sock;
    int isHost;
    char username[256];

    volatile int inGame;
};

#define ClientInfo struct ClientInfo

enum GameState {
    NOT_STARTED,
    RUNNING,
    FINISHED
};

#define GameState enum GameState

typedef char * Word;

struct GameDict
{
    Word * words;
    int    wordCount;
};

#define GameDict       struct GameDict
#define GameCustomDict GameDict

struct GameInfo
{
    char name[256];
    int size;
    volatile int playerCount;
    ClientInfo ** players;
    int playerTurn;
    volatile GameState state;
    GameCustomDict * customDict;
};

#define GameInfo struct GameInfo

GameInfo games[1000];
int gameCount = 0;


static GameDict globalGameDict;

void initGlobalDict()
{
    FILE * dictFile = fopen("server-dict.txt", "r");
    int lineCount = 0;

    while ( !feof(dictFile) )
    {
        char line[128];
        fgets(line, 128, dictFile);

        if (strchr(line, '\n'))
            *strchr(line, '\n') = 0;

        if (strlen(line) > 0)
            lineCount++;
    }

    globalGameDict.wordCount = lineCount;
    globalGameDict.words = NEW_ARR(Word, globalGameDict.wordCount);

    fclose(dictFile);
    dictFile = fopen("server-dict.txt", "r");

    int i = 0;
    while ( !feof(dictFile) )
    {
        char line[128];
        fgets(line, 128, dictFile);

        if (strchr(line, '\n'))
            *strchr(line, '\n') = 0;

        if (strlen(line) > 0)
        {
            globalGameDict.words[i] = NEW_ARR(char, 128);
            strcpy(globalGameDict.words[i++], line);
        }
    }

    fclose(dictFile);
}

int isWordValid(GameDict * dict, Word word)
{
    for ( int i = 0; i < dict->wordCount; i++ )
        if ( STR_EQ( word, dict->words[i] ) )
            return 1;

    return 0;
}

GameInfo * newLobby ( char * lobbyName, int lobbySize, GameCustomDict * customDict )
{
    for ( int i = 0; i < gameCount; i++ )
    {
        if ( STR_EQ ( lobbyName, games[i].name ) && games[i].state != FINISHED )
        {
            return NULL;
        }

        if ( games[i].state == FINISHED )
        {
            strcpy ( games[i].name, lobbyName );
            games[i].state = NOT_STARTED;
            games[i].size = lobbySize;
            games[i].playerTurn = 0;
            games[i].playerCount = 0;
            games[i].players = NEW_ARR(ClientInfo *, lobbySize);
            games[i].customDict = customDict;

            return & games[i];
        }
    }

    strcpy ( games[gameCount].name, lobbyName );
    games[gameCount].state = NOT_STARTED;
    games[gameCount].size = lobbySize;
    games[gameCount].playerTurn = 0;
    games[gameCount].playerCount = 0;
    games[gameCount].players = NEW_ARR(ClientInfo *, lobbySize);
    games[gameCount].customDict = customDict;

    return & games [ gameCount ++ ];
}

GameInfo * getLobby ( char * lobbyName )
{
    for ( int i = 0; i < gameCount; i++ )
        if ( STR_EQ ( lobbyName, games[i].name ) )
            return & games[i];

    return NULL;
}

Sock createSock ()
{
    Sock s = socket ( ADDRESS_IPV4, SOCK_STREAM, 0 );
    int activeReuseAddress = 1;
    setsockopt( s, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, & activeReuseAddress, sizeof ( activeReuseAddress ) );

    return s;
}

Sock createServerSock ( SockAddr sockAddr )
{
    Sock s = createSock();

    if ( bind ( s, ( SockAddrPtr ) & sockAddr, sizeof ( SockAddr ) ) == -1 )
    {
        return -1;
    }

    listen ( s, 64 );
    return s;
}

void treatTurn ( GameInfo * game, int turn, char * lastWord )
{
    for ( int i = 0; i < game->playerCount; i++ )
    {
        int playerCount = game->playerCount;
        int hisTurn = turn == i;

        WRITE_INT( game->players[i]->sock, playerCount )
        if ( game->playerCount == 1 )
        {
            game->state = FINISHED;
            game->players[0]->inGame = 0;
            pthread_cancel(pthread_self());
        }

        WRITE_INT( game->players[i]->sock, hisTurn )
        WRITE_STRING( game->players[i]->sock, game->players[i]->username )
        WRITE_STRING( game->players[i]->sock, lastWord )
    }

    char currentWord [ 256 ];
    READ_STRING(game->players[turn]->sock, currentWord )

    int wordOk = 0;

    if ( strlen ( lastWord ) == 0 )
    {
        strcpy ( lastWord, currentWord );
        wordOk = 1;
    } else
    {
        if ( strlen ( currentWord ) < 2 )
            wordOk = 0;
        else
            if ( strlen ( lastWord ) == 1 )
            {
                if ( lastWord[0] == currentWord[0] )
                    wordOk = 1;
                else
                    wordOk = 0;
            } else
                if ( lastWord[strlen(lastWord) - 2] == currentWord[0] &&
                     lastWord[strlen(lastWord) - 1] == currentWord[1] )
                    wordOk = 1;
                else
                    wordOk = 0;
    }

    int validInServerDict = isWordValid( & globalGameDict, currentWord );
    int validInPlayerDict = game->customDict == NULL ? 0 : isWordValid( game->customDict, currentWord );

    wordOk = wordOk && ( validInPlayerDict || validInServerDict ) || (strlen(currentWord) == 1 && strlen(lastWord) == 0);

    WRITE_INT(game->players[turn]->sock, wordOk)

    for ( int i = 0; i < game->playerCount; i++ )
    {
        if ( i != turn )
        {
            WRITE_STRING(game->players[i]->sock, currentWord)
            WRITE_INT(game->players[i]->sock, wordOk)
        }
    }

    if ( ! wordOk )
    {
        game->players[turn]->inGame = 0;
        game->playerCount --;
        for ( int i = turn; i < game->playerCount; i++ )
            game->players[i] = game->players[i + 1];
        memset ( lastWord, 0, 256 );
    }
    if ( wordOk )
        strcpy ( lastWord, currentWord );
    treatTurn( game, ( turn + 1 ) % game->playerCount, lastWord );
}

void startGame( GameInfo * game )
{
    char lastWord[256];
    memset(lastWord, 0, 256);
    game->state = RUNNING;
    treatTurn ( game, 0, lastWord );
}

void lobbyAsHost( char * name, int size, GameCustomDict * customDict, ClientInfo * clientInfo )
{
    int lobbyGood = 1;
    clientInfo->isHost = 1;

    GameInfo * lobby = newLobby ( name, size, customDict );
    lobby->players [ lobby->playerCount ++ ] = clientInfo;

    while ( 1 )
    {
        int currentPlayerCount = lobby->playerCount;
        for ( int i = 0; i < lobby->playerCount; i++ )
        {
            WRITE_INT ( lobby->players[i]->sock, lobbyGood )
            WRITE_INT ( lobby->players[i]->sock, currentPlayerCount )
            WRITE_INT ( lobby->players[i]->sock, lobby->size )

            int isClientAlive = 0;

            READ_INT( lobby->players[i]->sock, isClientAlive )

            if ( ! isClientAlive )
            {
                lobbyGood = 0;
                break;
            }
        }

        if ( currentPlayerCount == lobby->size )
            break;

        while ( currentPlayerCount == lobby->playerCount );
    }

    int hostConfirmation = 1;
    READ_INT(clientInfo->sock, hostConfirmation)

    for ( int i = 1; i < lobby->playerCount; i++ )
    {
        WRITE_INT(lobby->players[i]->sock, i)
    }

    startGame ( lobby );
}

void listLobbies ( ClientInfo * clientInfo )
{
    int inLobbyGames = 0;

    for ( int i = 0; i < gameCount; i++ )
    {
        if ( games[i].state == NOT_STARTED )
        {
            inLobbyGames++;
        }
    }

    WRITE_INT( clientInfo->sock, inLobbyGames )

    for ( int i = 0; i < gameCount; i++ )
    {
        if ( games[i].state == NOT_STARTED )
        {
            WRITE_STRING(clientInfo->sock, games[i].name)
        }
    }
}

int lobbyJoin ( char * lobbyName, ClientInfo * clientInfo )
{
    GameInfo * lobby = getLobby( lobbyName );
    clientInfo->inGame = 1;

    int foundLobby = lobby != NULL;

    WRITE_INT( clientInfo->sock, foundLobby )

    lobby->players[ lobby->playerCount ] = clientInfo;
    lobby->playerCount ++;

    while ( lobby-> state != FINISHED && clientInfo->inGame );

    close(clientInfo->sock);
    return 0;
}

void treatClientStart ( ClientInfo * clientInfo )
{
    READ_STRING( clientInfo->sock, clientInfo->username )

    if ( readDisconnectFlag == 1 )
        return;

    GameCustomDict * customDict = NULL;
    char command [ 256 ];
    char lobbyName [ 256 ];
    int lobbySize = 0;

    memset ( lobbyName, 0, 256 );

    while ( ! STR_EQ(command, "quit") )
    {
        READ_STRING( clientInfo->sock, command)

        if (STR_EQ( command, "create" ) ||
            STR_EQ( command, "join" ))
        {
            READ_STRING( clientInfo->sock, lobbyName )
            if (STR_EQ(command, "create"))
            {
                int useCustomDict = 0;
                customDict = NULL;

                READ_INT(clientInfo->sock, lobbySize)
                READ_INT(clientInfo->sock, useCustomDict)

                if ( useCustomDict )
                {
                    customDict = NEW(GameCustomDict);

                    READ_INT ( clientInfo->sock, customDict->wordCount )
                    customDict->words = NEW_ARR(Word, customDict->wordCount );

                    for ( int i = 0; i < customDict->wordCount; i++ )
                    {
                        customDict->words[i] = NEW_ARR(char, 64);
                        READ_STRING( clientInfo->sock, customDict->words[i] )
                    }
                }


            }
        }

        if ( STR_EQ( command, "create" ) )
        {
            lobbyAsHost( lobbyName, lobbySize, customDict, clientInfo );
            close(clientInfo->sock);
            return;
        }

        if ( STR_EQ( command, "list" ) )
        {
            listLobbies( clientInfo );
        }

        if ( STR_EQ( command, "join" ) )
        {
            lobbyJoin( lobbyName, clientInfo );
            close(clientInfo->sock);
            return;
        }
    }
}

void * treatClientFunc ( void * clientInfoPtr )
{
    treatClientStart ( ( ClientInfo * ) clientInfoPtr );

    if ( ! readDisconnectFlag )
    {

    }

    free ( clientInfoPtr );
    return NULL;
}

int main ()
{
    initGlobalDict();

    pthread_t allThreads [ 500 ]; int threadCounter = 0;
    SockAddr serverAddress, clientAddress;
    serverAddress.sin_addr.s_addr = ADDRESS(INADDR_ANY);
    serverAddress.sin_port        = PORT(SERVER_PORT);
    serverAddress.sin_family      = ADDRESS_IPV4;

    Sock serverSock = createServerSock( serverAddress );

    if ( serverSock == -1 )
    {
        printf("Socket create error\n");
        return 0;
    }

    unsigned int addrSize = sizeof(clientAddress);

    while ( 1 )
    {
        ClientInfo * clientInfo = NEW(ClientInfo);
        Sock s = accept ( serverSock, (SockAddrPtr) & clientAddress, & addrSize );

        clientInfo->sock = s;
        clientInfo->isHost = 0;

        pthread_t newThread;
        pthread_create ( & newThread, NULL, treatClientFunc, clientInfo );

        allThreads [ threadCounter ] = newThread;
        threadCounter ++;

        if ( threadCounter >= 500 )
            break;
    }

    return 0;
}