#include <a_samp>
#include <a_mysql>

#include <YSI_Coding\y_inline>
#include <YSI_Visual\y_dialog>

#include "../mysql_prepared"

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
    MySQL_Handle = mysql_connect("localhost", "root", "", "example");

    if(mysql_errno() != 0) {
        printf("ERROR: MySQL could not connect to database!");
        SendRconCommand("exit");
    }

    // Prepare Statement
    stmt_checkPlayer = MySQL_PrepareStatement(MySQL_Handle, "SELECT uid, hash, salt FROM accounts WHERE username = ?");
    stmt_insertPlayer = MySQL_PrepareStatement(MySQL_Handle, "INSERT INTO accounts (username, hash, salt) VALUES (?, ?, ?)");
    stmt_loadPlayerData = MySQL_PrepareStatement(MySQL_Handle, "SELECT kills, deaths FROM accounts WHERE uid = ?");
    stmt_savePlayerData = MySQL_PrepareStatement(MySQL_Handle, "UPDATE accounts SET kills = ?, deaths = ? WHERE uid = ?");
    return 1;
}

public OnGameModeExit() {
    mysql_close(MySQL_Handle);
    return 1;
}

public OnPlayerConnect(playerid) {

    inline OnDataLoad() {

        new
            playerHash[64 + 1],
            playerSalt[11 + 1];

        MySQL_BindResultInt(stmt_checkPlayer, 0, Player_UniqueID[playerid]);
        MySQL_BindResult(stmt_checkPlayer, 1, playerHash, sizeof (playerHash));
        MySQL_BindResult(stmt_checkPlayer, 2, playerSalt, sizeof (playerSalt));

        if(MySQL_Statement_FetchRow(stmt_checkPlayer)) {
            Account_PromptLogin(playerid, playerHash, playerSalt);
        } else {
            Account_PromptRegister(playerid);
        }
    }
    // insert player name to first ? (question mark)
    MySQL_BindPlayerName(stmt_checkPlayer, 0, playerid);
    
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

// Account_InsertToDatabase inserts the data to the database
// this function should be only used under Account_Create
// return --> none
stock Account_InsertToDatabase(playerid, const hash[], const salt[]) {
    inline RegisterPlayer() {
        Player_UniqueID[playerid] = cache_insert_id();
        SendClientMessage(playerid, -1, "You have just registered to our server! You have been automatically logged in!");
    }
    MySQL_BindPlayerName(stmt_insertPlayer, 0, playerid);
    MySQL_Bind(stmt_insertPlayer, 1, hash);
    MySQL_Bind(stmt_insertPlayer, 2, salt);
    MySQL_ExecuteThreaded_Inline(stmt_insertPlayer, using inline RegisterPlayer);
}

// Account_Load loads the remaining data from the database
// this function should be only used under Account_PromptLogin
// return --> none
stock Account_Load(playerid) {

    inline OnDataLoad() {
        MySQL_BindResultInt(stmt_loadPlayerData, 0, Player_Kills[playerid]);
        MySQL_BindResultInt(stmt_loadPlayerData, 1, Player_Deaths[playerid]);

        if(MySQL_Statement_FetchRow(stmt_loadPlayerData)) {
            SendClientMessage(playerid, -1, "You have successfully logged in. Welcome back!");
        }
    }

    MySQL_BindInt(stmt_loadPlayerData, 0, Player_UniqueID[playerid]);
    MySQL_ExecuteParallel_Inline(stmt_loadPlayerData, using inline OnDataLoad);
}


// Account_PromptLogin shows the login dialog.
// this function is called under OnPlayerConnect.
// return --> none.
stock Account_PromptLogin(playerid, const hash[], const salt[]) {
    // y_inline fix
    // this issue: https://github.com/pawn-lang/YSI-Includes/issues/249
    new
        fix_salt[11 + 1],
        fix_hash[64 + 1];

    // copy to local
    strcpy(fix_salt, salt, sizeof(fix_salt));
    strcpy(fix_hash, hash, sizeof(fix_hash));

    inline PromptLogin_Response(pid, dialogid, response, listitem, string:inputtext[]) {
        #pragma unused pid, dialogid, listitem

        // if user does not respond, kick him.
        if (!response) {
            Kick(playerid);
            return;
        }

        new
            buf[65];

        SHA256_PassHash(inputtext, fix_salt, buf, sizeof(buf));
        printf("fix_hash: %s | buf: %s", fix_hash, buf);

        // compare password
        // if password from database isn't the same as the user input ==> wrong
        // if password is empty ==> wrong
        if (strcmp(buf, fix_hash) != 0 && !isnull(inputtext)) {
            SendClientMessage(playerid, -1, "Incorrect password");
            Account_PromptLogin(playerid, fix_hash, fix_salt);
            return;
        }

        Account_Load(playerid);
    }

    new
        string[MAX_PLAYER_NAME + 37],
        playerName[MAX_PLAYER_NAME];

    GetPlayerName(playerid, playerName, sizeof(playerName));
    format(string, sizeof(string), "Hello %s. Welcome back to the server!", playerName);

    Dialog_ShowCallback(playerid,
        using inline PromptLogin_Response, // Handler
        DIALOG_STYLE_PASSWORD,             // Style
        "Please login...",                 // Title
        string,                            // Content
        "Login",                           // Button Left
        "Leave");                          // Button Right
}

// Account_PromptRegister shows the register dialog.
// this function is called under OnPlayerConnect.
// return --> none.
stock Account_PromptRegister(playerid) {

    inline PromptRegister_Response(pid, dialogid, response, listitem, string:inputtext[]) {
        #pragma unused pid, dialogid, listitem
        if (!response) {
            Kick(playerid);
            return;
        }
        Account_Create(playerid, inputtext);
    }

    new
        string[MAX_PLAYER_NAME + 32],
        playerName[MAX_PLAYER_NAME];

    GetPlayerName(playerid, playerName, sizeof(playerName));
    format(string, sizeof(string), "Hello %s. Welcome to the server!", playerName);

    Dialog_ShowCallback(playerid,
        using inline PromptRegister_Response,
        DIALOG_STYLE_PASSWORD, // style
        "Please register...",  // title
        string,                // content
        "Register",            // button left
        "Leave");              // button right
}

// Account_Save saves the player's stats, in this example it only saves kills and deaths.
// this function is called under OnPlayerDisconnect.
// return --> none.
stock Account_Save(playerid) {
    // insert kills to first ? (question mark)
    MySQL_BindInt(stmt_savePlayerData, 0, Player_Kills[playerid]);
    // insert deaths to second ? (question mark)
    MySQL_BindInt(stmt_savePlayerData, 1, Player_Deaths[playerid]);
    // insert unique id to third ? (question mark)
    MySQL_BindInt(stmt_savePlayerData, 2, Player_UniqueID[playerid]);
    // execute statement
    MySQL_ExecuteThreaded(stmt_savePlayerData);
}

// Account_Create is called to initiate account creation process.
// this function checks if password is more than 3 but less than 20.
// this function checks if password contains all numbers.
// this function checks if password contains valid characters
// if password passes all checks, we create a randomised string for salt
// then we hash the password using SHA256_PassHash -> This is not recommended to use but I am using this as an example only.
// return --> none.
stock Account_Create(playerid, const password[]) {

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

    new
        salt[11 + 1],
        hash[64 + 1];

    for(new i = 0; i < 10; i++) {
        salt[i] = random(79) + 47;
    }
    salt[10] = 0;
    SHA256_PassHash(password, salt, hash, sizeof(hash));

    // insert
    Account_InsertToDatabase(playerid, hash, salt);
}
