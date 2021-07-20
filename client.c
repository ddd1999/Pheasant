#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "commons.h"

struct
{
    char username [ 256 ];
    char command [ 256 ];
    char response [ 256 ];

    char lobbyName [ 256 ];
    int lobbySize;

    char dictionaryFilePath [ 256 ];
    int useDictionary;

} globals;



void waitForUsername()
{
    printf("Enter an username : ");
    fflush(stdout);
    scanf ( "%s", globals.username );
}

void printHelp ()
{
    printf("Commands:\n");
    printf("\tlist - lists all lobbies\n");
    printf("\tcreate - creates a lobby\n");
    printf("\tjoin - joins a lobby\n");
    printf("\tquit - disconnects and exists the game\n");
}

void readMatchmakingCommand ()
{
    printf("Enter Command : ");
    fflush(stdout);

    scanf ( "%s", globals.command );

    if ( STR_EQ( globals.command, "create" ) )
    {
        printf("New Lobby Name : ");
        fflush(stdout);

        scanf ( "%s", globals.lobbyName );

        printf("New Lobby Size (3-16): ");
        fflush(stdout);

        scanf ( "%d", & globals.lobbySize );
        while ( ! ( globals.lobbySize >= 3 && globals.lobbySize <= 16 ) )
        {
            printf("Enter a valid lobby size (3-16): ");
            fflush(stdout);

            scanf("%d", & globals.lobbySize);
        }

        printf("Use Custom Dictionary (y/n): ");
        fflush(stdout);

        char conf = '\0';
        scanf (" %c", & conf);

        if ( conf == 'y' || conf == 'Y' )
        {
            globals.useDictionary = 1;
            printf("Enter Dictionary Path : ");
            fflush(stdout);

            scanf ( "%s", globals.dictionaryFilePath );
        }
        else
        {
            globals.useDictionary = 0;
        }
    }
    else if ( STR_EQ ( globals.command, "join" ) )
    {
        printf("Lobby Name : ");
        fflush(stdout);

        scanf ( "%s", globals.lobbyName );
    }
    else if (! STR_EQ ( globals.command, "list" ) &&
             ! STR_EQ ( globals.command, "start" ) &&
             ! STR_EQ ( globals.command, "quit" ))
    {
        strcpy ( globals.command, "none" );
    }
}

void startSendingCustomWords()
{
    printf("Not Yet\n");
}
int justStarted = 0;
int lastPlayerCount;
void gameTurn ( int sock )
{
    char turnUsername[256];
    char lastWord[256];

    int isMyTurn = -1;
    int remainingPlayers;

    READ_INT ( sock, remainingPlayers )

    if ( remainingPlayers == 1 )
    {
        printf("You won!\n");
        close(sock);
        exit(0);
    }

    READ_INT ( sock, isMyTurn )
    READ_STRING( sock, turnUsername )
    READ_STRING( sock, lastWord )

    if ( ! justStarted && remainingPlayers != lastPlayerCount && isMyTurn )
        justStarted = 1;

    lastPlayerCount = remainingPlayers;

    printf("Waiting for %s's turn to finish. Remaining players : %d. Last word : %s\n",
           turnUsername, remainingPlayers, strlen(lastWord) > 0 ? lastWord : "No last word." );

    if ( isMyTurn == 0 )
    {
        int playerTurnOk = 0;
        char word[256];
        READ_STRING( sock, word )
        READ_INT ( sock, playerTurnOk )
        printf( "%s Said %s. He was correct : %s\n", turnUsername, word, playerTurnOk ? "true" : "false" );
        gameTurn( sock );
        return;
    }

    else if ( isMyTurn == 1 )
    {
        char word[256];

        if ( justStarted )
        {
            printf("Select a starting letter : ");fflush(stdout);
            scanf("%s", word);
            word[1] = 0;
            justStarted = 0;
        }
        else
        {
            printf("Type in a word : ");
            fflush(stdout);
            scanf("%s", word);
        }
        WRITE_STRING( sock, word )
        int ok = 0;
        READ_INT( sock, ok )

        if ( ! ok )
        {
            printf("Word was invalid!\nLeaving...\n");
            close(sock);
            exit(0);
        } else
        {
            printf("Word OK!\n");
            gameTurn(sock);
        }
    }
}

void gameStart ( int sock, int turnNumber )
{
    justStarted = turnNumber == 0 ? 1 : 0;
    gameTurn ( sock );
}

void lobbyAsHost( int sock )
{
    int inLobby, lobbyCap, noDisconnects;
    READ_INT( sock, noDisconnects )
    if ( ! noDisconnects )
        return;

    READ_INT( sock, inLobby )
    READ_INT( sock, lobbyCap )

    int stillAlive = 1;

    WRITE_INT( sock, stillAlive )

    printf("Waiting for players to join... status : %d / %d player\n", inLobby, lobbyCap );

    while ( inLobby != lobbyCap )
    {
        READ_INT( sock, noDisconnects )
        if ( ! noDisconnects )
            return;

        READ_INT( sock, inLobby )
        READ_INT( sock, lobbyCap )

        printf("Waiting for players to join... status : %d / %d players\n", inLobby, lobbyCap );

        stillAlive = 1;

        WRITE_INT( sock, stillAlive )
    }

    printf("Game Ready... Start ? (y/n)");

    char c = 'n';
    while ( c != 'y' && c != 'Y' )
        scanf( " %c", &c );

    int turn = 0;
    WRITE_INT(sock, turn)

    printf("Starting Game...\n");

    gameStart( sock, turn );
}

void viewLobbyList ( int sock )
{
    int lobbyCount;
    char lobbyName [ 256 ];

    READ_INT( sock, lobbyCount )

    printf("Found %d lobbies : \n", lobbyCount);
    while ( lobbyCount -- )
    {
        READ_STRING( sock, lobbyName )
        printf("\t%s\n", lobbyName);
    }

    printf("\nUse join <lobby name> to join one\n");
}

void joinLobby ( int sock )
{
    int foundLobby = 0;

    READ_INT( sock, foundLobby );

    if ( ! foundLobby )
    {
        printf("Lobby Not Found\n");
        return;
    }

    int inLobby, lobbyCap, noDisconnects;
    READ_INT( sock, noDisconnects )
    if ( ! noDisconnects )
        return;

    READ_INT( sock, inLobby )
    READ_INT( sock, lobbyCap )

    int stillAlive = 1;

    WRITE_INT( sock, stillAlive )

    printf("Waiting for players to join... status : %d / %d player\n", inLobby, lobbyCap );

    while ( inLobby != lobbyCap )
    {
        READ_INT( sock, noDisconnects )
        if ( ! noDisconnects )
            return;

        READ_INT( sock, inLobby )
        READ_INT( sock, lobbyCap )

        printf("Waiting for players to join... status : %d / %d players\n", inLobby, lobbyCap );

        stillAlive = 1;

        WRITE_INT( sock, stillAlive )
    }

    int playerTurn = 0;

    printf("Waiting for host to start game...\n");
    READ_INT(sock, playerTurn)

    gameStart( sock, playerTurn);
}

void matchMake ( int sock )
{
    waitForUsername();
    WRITE_STRING( sock, globals.username )

    printHelp ();

    while ( ! STR_EQ ( globals.command, "quit" ) )
    {
        readMatchmakingCommand();

        if (STR_EQ(globals.command, "none"))
            continue;

        WRITE_STRING( sock, globals.command )
        if (STR_EQ( globals.command, "create" ) ||
            STR_EQ( globals.command, "join" ))
        {
            WRITE_STRING( sock, globals.lobbyName )
            if ( STR_EQ(globals.command, "create") )
            {
                WRITE_INT(sock, globals.lobbySize)
                WRITE_INT(sock, globals.useDictionary);

                if ( globals.useDictionary )
                {
                    int lineCounter = 0;
                    FILE *wordsFile = fopen(globals.dictionaryFilePath, "r");
                    while ( wordsFile == NULL )
                    {
                        printf("Enter a valid path!\nPath : ");
                        fflush(stdout);

                        scanf("%s", globals.dictionaryFilePath);
                        wordsFile = fopen(globals.dictionaryFilePath, "r");
                    }

                    while (!feof(wordsFile))
                    {
                        char line[128];
                        fgets(line, 128, wordsFile);

                        if (strchr(line, '\n'))
                            *strchr(line, '\n') = 0;

                        if (strlen(line) > 0)
                            lineCounter++;
                    }

                    WRITE_INT(sock, lineCounter)
                    fclose(wordsFile);

                    wordsFile = fopen(globals.dictionaryFilePath, "r");
                    while (!feof(wordsFile))
                    {
                        char line[128];
                        fgets(line, 128, wordsFile);

                        if (strchr(line, '\n'))
                            *strchr(line, '\n') = 0;

                        if (strlen(line) > 0) WRITE_STRING(sock, line)
                    }
                    fclose(wordsFile);
                }
            }
        }

        if ( STR_EQ ( globals.command, "create" ) )
        {
            lobbyAsHost( sock );
        }

        if ( STR_EQ ( globals.command, "list" ) )
        {
            viewLobbyList( sock );
        }

        if ( STR_EQ ( globals.command, "join" ) )
        {
            joinLobby( sock );
        }
    }
}

Sock createSock ()
{
    Sock s = socket ( AF_INET, SOCK_STREAM, 0 );
    int activeReuseAddress = 1;
    setsockopt( s, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, & activeReuseAddress, sizeof ( activeReuseAddress ) );

    return s;
}

Sock createClientSock ( SockAddr sockAddr )
{
    Sock s = createSock();

    if ( connect ( s, ( SockAddrPtr ) & sockAddr, sizeof ( SockAddr ) ) == -1 )
    {
        return -1;
    }

    return s;
}

int main()
{
    SockAddr serverAddress;
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    serverAddress.sin_port        = PORT(SERVER_PORT);
    serverAddress.sin_family      = ADDRESS_IPV4;

    Sock sock = createClientSock( serverAddress );

    if ( sock == -1 )
    {
        printf("Client connect error\n");
        return 0;
    }

    matchMake( sock );

    return 0;
}
