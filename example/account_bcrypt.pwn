
#include <a_samp>
#include <a_mysql>

#define BCRYPT_COST 12
#include <samp_bcrypt>

#define YSI_NO_HEAP_MALLOC

#include <YSI_Coding\y_inline>
#include <YSI_Visual\y_dialog>

#include <YSI_Extra\y_inline_mysql>
#include <YSI_Extra\y_inline_bcrypt>

#include <mysql_prepared>


new
// player
    Player_UniqueID[MAX_PLAYERS],
    Player_Kills[MAX_PLAYERS],
    Player_Deaths[MAX_PLAYERS],
// connection
    MySQL: MySQL_Handle,
// statements
    Statement: stmt_checkPlayer,
    Statement: stmt_insertPlayer,
    Statement: stmt_loadPlayerData,
    Statement: stmt_savePlayerData
// end
;

main () {

}

public OnGameModeInit() {

    // Connect to MySQL
    MySQL_Handle = mysql_connect("localhost", "root", "password", "example");

    if(mysql_errno() != 0) {
        printf("ERROR: MySQL could not connect to database!");
        SendRconCommand("exit");
    }

    // Prepare Statement
    stmt_checkPlayer = MySQL_PrepareStatement(MySQL_Handle, "SELECT uid, password FROM accounts WHERE username = ?");
    stmt_insertPlayer = MySQL_PrepareStatement(MySQL_Handle, "INSERT INTO accounts (username, password) VALUES (?, ?)");
    stmt_loadPlayerData = MySQL_PrepareStatement(MySQL_Handle, "SELECT kills, deaths FROM accounts WHERE uid = ?");
    stmt_savePlayerData = MySQL_PrepareStatement(MySQL_Handle, "UPDATE accounts SET kills = ?, deaths = ? WHERE uid = ?");
    return 1;
}

public OnGameModeExit() {
    mysql_close(MySQL_Handle);
    return 1;
}


public OnPlayerConnect(playerid) {
    inline const OnDataLoad() {
        new
            playerHash[65];

        MySQL_BindResultInt(stmt_checkPlayer, 0, Player_UniqueID[playerid]);
        MySQL_BindResult(stmt_checkPlayer, 1, playerHash, sizeof (playerHash));

        // run the statement with the binded data and fetch
        // the rows and store it to the variables assigned
        if(MySQL_Statement_FetchRow(stmt_checkPlayer)) {
            Account_PromptLogin(playerid, playerHash);
        }
        else {
            Account_PromptRegister(playerid);
        }
    }

    new
        playerName[MAX_PLAYER_NAME];
    GetPlayerName(playerid, playerName, sizeof (playerName));

    // insert player name to first ? (question mark)
    MySQL_Bind(stmt_checkPlayer, 0, playerName);
    // execute the query.
    MySQL_ExecuteParallel_Inline(stmt_checkPlayer, using inline OnDataLoad);
    return 1;
}

public OnPlayerDeath(playerid, killerid, reason) {
    // check if the killer is valid.
    // this check is to avoid Out of Bounds.
    if(killerid != INVALID_PLAYER_ID){
        Player_Kills[killerid] ++;
    }
    Player_Deaths[playerid] ++;
    return 1;
}

public OnPlayerDisconnect(playerid, reason) {
    Account_Save(playerid);
    return 1;
}

// Account_PromptLogin shows the login dialog.
// this function is called under OnPlayerConnect.
// return --> none.
Account_PromptLogin(playerid, const password[]) {
    // y_inline fix
    // this issue: https://github.com/pawn-lang/YSI-Includes/issues/249
    // copy to local

    new
        fix_password[64];

    strcpy(fix_password, password, sizeof(fix_password));
    inline PromptLogin_Response(pid, dialogid, response, listitem, string:inputtext[]) {
        #pragma unused pid, dialogid, listitem

        // if user does not respond, kick him.
        if (!response) {
            Kick(playerid);
            return;
        }
        Validate(playerid, inputtext, fix_password);
    }

    new
        string[MAX_PLAYER_NAME + 37],
        playerName[MAX_PLAYER_NAME];

    GetPlayerName(playerid, playerName, sizeof(playerName));
    format(string, sizeof(string), "Hello %s. Welcome back to the server!", playerid);

    Dialog_ShowCallback(
        playerid,
        using inline PromptLogin_Response, // Handler
        DIALOG_STYLE_PASSWORD,             // Style
        "Please login...",                 // Title
        string,                            // Content
        "Login",                           // Button Left
        "Leave"                            // Button Right
    );
}

// Account_PromptRegister shows the register dialog.
// this function is called under OnPlayerConnect.
Account_PromptRegister(playerid) {

    inline _response(pid, dialogid, response, listitem, string:inputtext[]) {
        #pragma unused pid, dialogid, listitem
        if (!response) {
            Kick(playerid);
            return;
        }
        Create(playerid, inputtext);
    }

    new
        string[MAX_PLAYER_NAME + 32],
        playerName[MAX_PLAYER_NAME];

    GetPlayerName(playerid, playerName, sizeof(playerName));
    format(string, sizeof(string), "Hello %s. Welcome to the server!", playerName);

    Dialog_ShowCallback(
        playerid,
        using inline _response, // Handler
        DIALOG_STYLE_PASSWORD,  // Style
        "Please register...",   // Title
        string,                 // Content
        "Register",             // Button left
        "Leave"                 // Button right
    );
}

// Account_Save saves the player's stats, in this example it only saves kills and deaths.
// NOTE: Although for best practices you should be saving data everytime it changes.
// as this is only an example we'll do it the basic way :)
Account_Save(playerid) {
    // insert kills to first ? (question mark)
    MySQL_BindInt(stmt_savePlayerData, 0, Player_Kills[playerid]);
    // insert deaths to second ? (question mark)
    MySQL_BindInt(stmt_savePlayerData, 1, Player_Deaths[playerid]);
    // insert unique id to third ? (question mark)
    MySQL_BindInt(stmt_savePlayerData, 2, Player_UniqueID[playerid]);
    // execute statement
    MySQL_ExecuteThreaded(stmt_savePlayerData);
}

// ----------------------------------------------------------------
// Local function, can't be accessed outside this file.
// ----------------------------------------------------------------

// Validate(playerid, const input[], dbPassword[])
// compares whether the password from the database
// matches the user input, if it doesn't match we prompt login
// if password match we load the remaining data that needs loading.
static Validate(playerid, const input[], const dbPassword[]) {

    // -- DO NOT REMOVE, IMPORTANT! -- //
    // y_inline fix
    // this issue: https://github.com/pawn-lang/YSI-Includes/issues/249
    // copy to local
    new
        fix_password[64];

    strcpy(fix_password, dbPassword, sizeof(fix_password));
    // -- DO NOT REMOVE, IMPORTANT! -- //

    inline const OnPasswordVerify(bool: success) {
        if(!success) {
            Account_PromptLogin(playerid, fix_password);
            SendClientMessage(playerid, -1, "Could not validate password, try again.");
            return;
        }

        Load(playerid);
    }
    BCrypt_CheckInline(input, dbPassword, using inline OnPasswordVerify);
}

// Load(playerid) retrieves the remaining data from the database
// so in this case we load kills and deaths
// and store it to global variable so it can be
// modified and accessed for later purposes
static Load(playerid) {

    inline const OnDataLoad() {
        MySQL_BindResultInt(stmt_loadPlayerData, 0, Player_Kills[playerid]);
        MySQL_BindResultInt(stmt_loadPlayerData, 1, Player_Deaths[playerid]);

        if(MySQL_Statement_FetchRow(stmt_loadPlayerData)) {
            SendClientMessage(playerid, -1, "You have successfully logged in. Welcome back!");
        }
    }

    MySQL_BindInt(stmt_loadPlayerData, 0, Player_UniqueID[playerid]);
    MySQL_ExecuteParallel_Inline(stmt_loadPlayerData, using inline OnDataLoad);
}

// Create(playerid, const password[]) is called to initiate account creation process.
// Current validations are...
// checks if password is more than 3 but less than 20.
// checks if password contains all numbers.
// checks if password contains valid characters
static Create(playerid, const password[]) {

    if (!(3 <= strlen(password) <= 20)) {
        SendClientMessage(playerid, -1, "Invalid length on the password. It should be between 3-20 characters" );
        Account_PromptRegister(playerid);
        return;
    }
    if (isnumeric(password)) {
        SendClientMessage(playerid, -1, "Your password is invalid. The password should include alphabets.");
        Account_PromptRegister(playerid);
        return;
    }

    new
        bool: ret;
    for(new i = 0; password[i] != EOS; ++i) {
        switch(password[i]) {
            case '0'..'9', 'A'..'Z', 'a'..'z': {
                ret = true;
            }
            default: {
                ret = false;
            }
        }
    }

    if(!ret) {
        SendClientMessage(playerid, -1, "Your password is invalid. Valid characters are: A-Z, a-z, 0-9.");
        Account_PromptRegister(playerid);
        return;
    }

    inline const OnPasswordHash(string:hash[]) {
        InsertToDB(playerid, hash);
    }
    BCrypt_HashInline(password, BCRYPT_COST, using inline OnPasswordHash);
}

static InsertToDB(playerid, const password[]) {
    inline const OnRegister() {
        Player_UniqueID[playerid] = cache_insert_id();

        SendClientMessage(playerid, -1, "You have just registered to our server! You have been automatically logged in!");
    }

    new
        playerName[MAX_PLAYER_NAME];
    GetPlayerName(playerid, playerName, sizeof(playerName));

    MySQL_Bind(stmt_insertPlayer, 0, playerName);
    MySQL_Bind(stmt_insertPlayer, 1, password);
    MySQL_ExecuteThreaded_Inline(stmt_insertPlayer, using inline OnRegister);
}