
#if !defined _INC_y_va
    #error You need to "#include <YSI_Coding/y_va>" before using this library.
#endif

#if defined MYSQL_PREPARED_INC
    #endinput
#endif
#define MYSQL_PREPARED_INC

#if !defined MAX_PARAMS
    #define MAX_PARAMS  		(32)
#endif

#if !defined MAX_FIELD_NAME
    #define MAX_FIELD_NAME  	(66)
#endif

#if !defined MAX_STATEMENTS
    #define MAX_STATEMENTS  	(16)
#endif

#if !defined MAX_STATEMENT_SIZE
    #define MAX_STATEMENT_SIZE  (1024)
#endif

#if !defined MAX_FIELDS
    #define MAX_FIELDS  		(64)
#endif

#define Warning(%1)                 print(!"Warning: " %1)
#define Warningf(%1)                printf("Warning: " %1)

#define Error(%1)                   print(!"Error: " %1)
#define Errorf(%1)                  printf("Error: " %1)

#define Debug(%1)  	                printf("Debug: " %1)

#if !defined MYSQL_PREPARE_DEBUG
    #define MYSQL_PREPARE_DEBUG         (false)
#endif

const
    Statement:INVALID_STATEMENT = Statement:-1,
    MySQL:INVALID_CONNECTION = MYSQL_INVALID_HANDLE;


enum DataType {
    TYPE_NONE,
    TYPE_NULL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_RAW_STRING,
};

static enum QueryType {
    TYPE_THREADED,
    TYPE_PARALLEL
}

static enum E_STMT_MYSQL {
    // The ready-to-run query.
    E_STMT_MYSQL_QUERY[MAX_STATEMENT_SIZE + 1],
    // Types of bound return fields
    DataType:E_STMT_MYSQL_FIELD_TYPE[MAX_FIELDS],
    // Parameter count
    E_STMT_MYSQL_PARAM_COUNT,
    // The parameter types.
    DataType:E_STMT_MYSQL_PARAM_TYPE[MAX_PARAMS],
    // Sizes of bound result fields (used only for strings currently)
    E_STMT_MYSQL_FIELD_SIZE[MAX_FIELDS],
    // Position of parameters in the query string.
    E_STMT_MYSQL_PARAM_POS[MAX_PARAMS],
    // Addresses of bound result fields
    E_STMT_MYSQL_FIELD_ADDRESS[MAX_FIELDS],
    E_STMT_MYSQL_ROWS_FETCHED,
    // Length of parameters in the query string.
    E_STMT_MYSQL_PARAM_LEN[MAX_PARAMS],
    // The database it was created for
    MySQL:E_STMT_MYSQL_HANDLE,
    bool:E_STMT_COUNT_ROW_FIRST
};

static stock
    gs_szBuffer[8192],
    gs_Statements[Statement:MAX_STATEMENTS][E_STMT_MYSQL],
    totalRows[Statement:MAX_STATEMENTS] = 0;

// --
//  Internally used, Deprecated Function Names
// --
static stock escape_string(szString[], const szEnclosing[] = "'", argSize = sizeof(szString)) {
    new
        iPos;

    while (-1 != (iPos = strfind(szString, szEnclosing, _, iPos))) {
        strins(szString, szEnclosing, iPos, argSize);
        iPos += 2;
    }
}

static stock stmt_close(Statement:statement) {
    if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
        Errorf("(stmt_close) Invalid Statement (%d).", _:statement);
        return false;
    }

    #if MYSQL_PREPARE_DEBUG
        Debug("(stmt_close:%d) Statament closed.", _:statement);
    #endif

    // reset data.
    gs_Statements[statement][E_STMT_MYSQL_HANDLE] = INVALID_CONNECTION;
    return true;
}

static stock bool:stmt_execute(Statement:statement, QueryType:type, const callback[] = "", const fmat[] = "", {Float,_}:...) {
    if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
        Errorf("(stmt_execute) Invalid Statement (%d).", _:statement);
        return false;
    }

    if (mysql_errno(gs_Statements[statement][E_STMT_MYSQL_HANDLE]) != 0) {
        Errorf("(stmt_execute) Invalid Connection (%d).", _:statement);
        return false;
    }

    // Make sure all parameters have been set.
    for (new i = 0; i < gs_Statements[statement][E_STMT_MYSQL_PARAM_COUNT]; i++) {
        if (gs_Statements[statement][E_STMT_MYSQL_PARAM_TYPE][i] == TYPE_NONE) {
            Errorf("(stmt_execute) Parameter (%d) not started in statement", i);
            return false;
        }
    }

    // write
    strunpack(gs_szBuffer, gs_Statements[statement][E_STMT_MYSQL_QUERY]);

    new ret =
    type
    ?
        mysql_tquery(gs_Statements[statement][E_STMT_MYSQL_HANDLE], gs_szBuffer, callback, fmat, ___(4))
    :
        mysql_pquery(gs_Statements[statement][E_STMT_MYSQL_HANDLE], gs_szBuffer, callback, fmat, ___(4));

    // throw error if query failed.
    if(!ret) {
        Errorf("(stmt_execute:%d) failed to execute the query", _:statement);
        return false;
    }

    gs_Statements[statement][E_STMT_MYSQL_ROWS_FETCHED] = 0;
    totalRows[statement] = 0;

    #if MYSQL_PREPARE_DEBUG
        Debug("(stmt_execute:%d) Statement executed.", _:statement);
        Debug("(stmt_execute:%d) :Query executed [%s]", _:statement, gs_szBuffer);
    #endif
    return bool:ret;
}

static stock bool:stmt_bind_value(&Statement:statement, param, DataType:type, {Float,_}:...)
{

    new
        lengthDifference,
        queryLength;

    if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
        Warningf("(stmt_bind_value) Invalid statement (%d).", _:statement);
        return false;
    }

    if (param >= gs_Statements[statement][E_STMT_MYSQL_PARAM_COUNT]) {
        Warningf("(stmt_bind_value) Parameter index larger than number of parameters (%d > %d)", param, gs_Statements[statement][E_STMT_MYSQL_PARAM_COUNT]);
        return false;
    }

    // Fill gs_szBuffer with the new contents.
    gs_szBuffer[0] = EOS;

    switch (type) {
        case TYPE_NULL: {
            goto default_case;
        }
        case TYPE_INT: {
            new
                iArgValue = getarg(3)
            ;

            if (iArgValue == cellmin) {
                gs_szBuffer = !"-2147483648";
            } else {
                format(gs_szBuffer, sizeof(gs_szBuffer), "%d", getarg(3));
            }
        }
        case TYPE_FLOAT: {
            format(gs_szBuffer, sizeof(gs_szBuffer), "%f", getarg(3));
        }
        case TYPE_STRING: {

            new ret[128];
            va_getstring(ret, 3);

            strpack(gs_szBuffer, ret, (sizeof(gs_szBuffer) - 3));

            escape_string(gs_szBuffer, "'", sizeof(gs_szBuffer) - 1);

            strins(gs_szBuffer, !"'", 0); // insert an '(apostrophe) first
            strcat(gs_szBuffer, !"'"); // insert an 'apostrophe  end
        }
        case TYPE_RAW_STRING: {
            new ret[128];
            getstringarg(ret, 3);
            strcat(gs_szBuffer, ret);
        }
        default: {
            default_case:
            strcat(gs_szBuffer, "NULL");
        }
    }

    queryLength = strlen(gs_szBuffer);

    lengthDifference = queryLength - gs_Statements[statement][E_STMT_MYSQL_PARAM_LEN][param];

    // Adjust the position of any params after the one being modified.
    for (new i = param + 1; i < gs_Statements[statement][E_STMT_MYSQL_PARAM_COUNT]; i++)
        gs_Statements[statement][E_STMT_MYSQL_PARAM_POS][i] += lengthDifference;

    // Delete the old parameter from the query.
    strdel(gs_Statements[statement][E_STMT_MYSQL_QUERY], gs_Statements[statement][E_STMT_MYSQL_PARAM_POS][param], gs_Statements[statement][E_STMT_MYSQL_PARAM_POS][param] + gs_Statements[statement][E_STMT_MYSQL_PARAM_LEN][param]);

    // Make sure we have enough space.
    if ((strlen(gs_Statements[statement][E_STMT_MYSQL_QUERY]) + queryLength) char > MAX_STATEMENT_SIZE) {
        Error("(stmt_bind_value) Buffer overflow. Aumente MAX_STATEMENT_SIZE.");
        MySQL_StatementClose(statement);
        return false;
    }

    // Insert the new parameter.
    strins(gs_Statements[statement][E_STMT_MYSQL_QUERY], gs_szBuffer, gs_Statements[statement][E_STMT_MYSQL_PARAM_POS][param], MAX_STATEMENT_SIZE);

    #if MYSQL_PREPARE_DEBUG
        if (ispacked(gs_szBuffer)) {
            strunpack(gs_szBuffer, gs_szBuffer);
        }
        Debug("(stmt_bind_value:%d) New parameter entered for %d in %d: %s", _:statement, param, gs_Statements[statement][E_STMT_MYSQL_PARAM_POS][param], gs_szBuffer);
    #endif

    gs_Statements[statement][E_STMT_MYSQL_PARAM_LEN][param] = queryLength;
    gs_Statements[statement][E_STMT_MYSQL_PARAM_TYPE][param] = type;
    return true;
}

static stock stmt_bind_result_field(Statement:statement, field, DataType:type, {Float, _}:...) {
    new
        address,
        argSize,
        countArgs
    ;

    #emit LOAD.S.pri  8
    #emit SHR.C.pri   2
    #emit STOR.S.pri  countArgs

    if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
        Warningf("(stmt_bind_result_field) Invalid statement passed (%d).", _:statement);
        return;
    }

    if (field < 0) {
        Errorf("(stmt_bind_result_field) Negative field index (%d).", field);
        return;
    }

    switch (type) {
        case
            TYPE_STRING,
            TYPE_RAW_STRING: {
            if (countArgs != 5) {
                Error("(stmt_bind_result_field) Invalid number of arguments passed. Strings and arrays require an additional argument containing the string size.");
                return;
            }

            argSize = getarg(4);
        }

        case TYPE_NONE: {
            gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field] = TYPE_NONE;
            return;
        }

        default: {
            if (countArgs != 4) {
                Error("(stmt_bind_result_field) Invalid number of arguments passed.");
                return;
            }

            argSize = 1;
        }
    }

    if (field >= MAX_FIELDS) {
        Warningf("(stmt_bind_result_field) Field index larger than max number of fields (%d > %d). Increase DB_MAX_FIELDS.", field, MAX_FIELDS);
        return;
    }

    {}

    #emit LOAD.S.pri 24
    #emit STOR.S.pri address

    gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field] = type;
    gs_Statements[statement][E_STMT_MYSQL_FIELD_ADDRESS][field] = address;
    gs_Statements[statement][E_STMT_MYSQL_FIELD_SIZE][field] = argSize;

    #if MYSQL_PREPARE_DEBUG
        Debug("(stmt_bind_result_field:%d) Bound result field %d (type %d) to variable 0x%04x%04x.", _:statement, field, _:type, address >>> 16, address & 0xFFFF);
    #endif
}

static stock Statement:stmt_prepare(MySQL:handle, const szQuery[]) {

    new
        Statement:statement = INVALID_STATEMENT,
        paramPos,
        i,
        queryLength;

    if(mysql_errno(handle) != 0) {
        Error("(stmt_prepare) Invalid Connection.");
        return INVALID_STATEMENT;
    }

    // Pretty useless to prepare empty queries.
    if (!(queryLength = strlen(szQuery))) {
        Error("(stmt_prepare) Empty query.");
        return INVALID_STATEMENT;
    }

    if (queryLength char > MAX_STATEMENT_SIZE) {
        Error("(stmt_prepare) Query too long. Increase MAX_STATEMENT_SIZE.");
        return INVALID_STATEMENT;
    }

    // Find an empty slot in gs_Statements.
    for (i = 0; i < sizeof(gs_Statements); i++) {
        if (gs_Statements[Statement:i][E_STMT_MYSQL_HANDLE] == INVALID_CONNECTION) {
            statement = Statement:i;
            break;
        }
    }

    if (statement == INVALID_STATEMENT) {
        Error("(stmt_prepare) No slots found free to do statament. Increase MAX_STATEMENTS.");
        return INVALID_STATEMENT;
    }

    // Make sure no parameters are initialized.
    for (i = 0; i < MAX_PARAMS; i++)
        gs_Statements[statement][E_STMT_MYSQL_PARAM_TYPE][i] = TYPE_NONE;

    paramPos = -1;
    i = 0;

    // Find all parameters
    while (-1 != (paramPos = strfind(szQuery, "?", _, ++paramPos))) {
        gs_Statements[statement][E_STMT_MYSQL_PARAM_POS][i] = paramPos;
        gs_Statements[statement][E_STMT_MYSQL_PARAM_LEN][i] = 1;

        if (++i >= MAX_PARAMS) {
            Error("(stmt_prepare) Parameter limit exceeded. Increase MAX_PARAMS.");
            return INVALID_STATEMENT;
        }
    }

    // init values
    gs_Statements[statement][E_STMT_MYSQL_HANDLE] = handle;
    gs_Statements[statement][E_STMT_MYSQL_PARAM_COUNT] = i;
    gs_Statements[statement][E_STMT_MYSQL_QUERY][0] = 0;
    gs_Statements[statement][E_STMT_MYSQL_ROWS_FETCHED] = 0;
    gs_Statements[statement][E_STMT_COUNT_ROW_FIRST] = true;

    if (ispacked(szQuery)) {
        #if MYSQL_PREPARE_DEBUG
            strunpack(gs_szBuffer, szQuery);
            Debug("(stmt_prepare:%d) Preparing statement with %d parameters: %s", _:statement, i, gs_szBuffer);
        #endif
        strcat(gs_Statements[statement][E_STMT_MYSQL_QUERY], szQuery, MAX_STATEMENT_SIZE);
    }
    else {
        #if MYSQL_PREPARE_DEBUG
            Debug("(stmt_prepare:%d) Preparing statement with %d parameters: %s", _:statement, i, szQuery);
        #endif

        strpack(gs_Statements[statement][E_STMT_MYSQL_QUERY], szQuery, MAX_STATEMENT_SIZE);
    }
    return statement;
}

static stock stmt_rows_left(&Statement:statement) {
    if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
        Errorf("(stmt_rows_left) Invalid statement passed (%d).", _:statement);
        return false;
    }

    new
        rows;
    cache_get_row_count(rows);
    return (rows - gs_Statements[statement][E_STMT_MYSQL_ROWS_FETCHED]);
}

static stock bool:stmt_fetch_row(Statement:statement) {
    if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
        Errorf("(stmt_fetch_row) Invalid statement passed (%d).", _:statement);
        return false;
    }

    new
        fields,
        address,
        value,
        count,
        rows;

    cache_get_field_count(fields);

    if (fields > MAX_FIELDS) {
        Warning("(stmt_fetch_row) There are more fields returned than MAX_FIELDS.");
        fields = MAX_FIELDS;
    }

    // store total rows into 'totalRows' array.
    if(totalRows[statement] == 0) {
        new
            numRows;
        cache_get_row_count(numRows);
        totalRows[statement] = numRows;
    }

    rows = gs_Statements[statement][E_STMT_MYSQL_ROWS_FETCHED];

    // stop the while() loop if function used in one.
    if(rows >= totalRows[statement]) {
        #if MYSQL_PREPARE_DEBUG
            Debug("(stmt_fetch_row) No rows left.");
        #endif
        return false;
    }

    for (new field = 0; field < fields; field++) {
        count ++;
        switch (gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field]) {
            case TYPE_NONE,
                TYPE_NULL:
                continue;

            case TYPE_INT,
                TYPE_FLOAT: {
                cache_get_value_index(rows, field, gs_szBuffer, sizeof(gs_szBuffer) - 1);
                address = gs_Statements[statement][E_STMT_MYSQL_FIELD_ADDRESS][field];
                value =
                    (gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field] != TYPE_FLOAT)
                    ?
                        strval(gs_szBuffer)
                    :
                        _:floatstr(gs_szBuffer
                    );

                #emit LOAD.S.pri value
                #emit SREF.S.pri address
            }

            case TYPE_STRING,
                 TYPE_RAW_STRING: {
                new
                    argSize;

                static const
                    sc_szFormatString[] = "%s";

                address	= gs_Statements[statement][E_STMT_MYSQL_FIELD_ADDRESS][field];
                argSize	= gs_Statements[statement][E_STMT_MYSQL_FIELD_SIZE][field];

                #emit PUSH.S    argSize // push address of variable argSize
                #emit PUSH.S    address // push address of variable address
                #emit PUSH.S    field // push address of variable field
                #emit PUSH.S    rows // retrieve first row
                #emit PUSH.C    16 // bytes passed to cache_get_value_index
                #emit SYSREQ.C  cache_get_value_index // call cache_get_value_index
                #emit STACK     20 //move stack to 20

                {}

                // format(output[], len, const format[], {Float,_}:...)
                #emit PUSH.S     address //push address of variable address
                #emit PUSH.C     sc_szFormatString //(%s into variable)
                #emit PUSH.S     argSize //push address of variable argSize
                #emit PUSH.S     address //push address of variable address
                #emit PUSH.C     16 // bytes passed to format
                #emit SYSREQ.C   format //call format
                #emit STACK      20 //move stack to 20
            }
        }
        #if MYSQL_PREPARE_DEBUG
            Debug("(stmt_fetch_row:%d) Fetched %d fields.", _:statement, count);
        #endif
    }
    gs_Statements[statement][E_STMT_MYSQL_ROWS_FETCHED] ++;
    return true;
}

// --
// External -- Wrappers
// --

// --
// Functions
// --
stock MySQL_StatementClose(Statement:statement) {
    return stmt_close(statement);
}

stock Statement:MySQL_PrepareStatement(MySQL:handle, const query[]) {
    return stmt_prepare(handle, query);
}

stock MySQL_Statement_RowsLeft(&Statement:statement) {
    return stmt_rows_left(statement);
}

stock MySQL_Statement_FetchRow(Statement:statement) {
    return stmt_fetch_row(statement);
}

// --
// Writing
// --
stock MySQL_Bind(Statement:statement, param, const str[], bool:isRaw = false) {
    stmt_bind_value(statement, param, (!isRaw) ? (TYPE_STRING) : (TYPE_RAW_STRING), str);
}

stock MySQL_BindPlayerName(Statement:statement, param, playerid) {
    new playerName[MAX_PLAYER_NAME + 1];
    if (!GetPlayerName(playerid, playerName, sizeof(playerName))) {
        stmt_bind_value(statement, param, TYPE_NULL);
    }
    else {
        stmt_bind_value(statement, param, TYPE_STRING, playerName);
    }
}

stock MySQL_BindPlayerIp(Statement:statement, param, playerid)
{
    new playerIp[16 + 1];
    if (GetPlayerIp(playerid, playerIp, sizeof(playerIp)) == -1) {
        stmt_bind_value(statement, param, TYPE_NULL);
    }
    else {
        stmt_bind_value(statement, param, TYPE_STRING, playerIp);
    }
}

stock MySQL_BindInt(Statement:statement, param, value) {
    stmt_bind_value(statement, param, TYPE_INT, value);
}

stock MySQL_BindFloat(Statement:statement, param, Float:value) {
    stmt_bind_value(statement, param, TYPE_FLOAT, value);
}

// --
// Reading
// --
stock MySQL_BindResult(Statement:statement, field, const result[], len = sizeof(result)) {
    stmt_bind_result_field(statement, field, TYPE_STRING, result, len);
}

stock MySQL_BindResultInt(Statement:statement, field, &result) {
    stmt_bind_result_field(statement, field, TYPE_INT, result);
}

stock MySQL_BindResultFloat(Statement:statement, field, &Float:result) {
    stmt_bind_result_field(statement, field, TYPE_FLOAT, result);
}

// --
// Executing
// --

stock bool:MySQL_ExecuteThreaded(Statement:statement, const callback[] = "", const fmat[] = "", {Float,_}:...) {
    return stmt_execute(statement, TYPE_THREADED, callback, fmat, ___(3));
}

stock bool:MySQL_ExecuteParallel(Statement:statement, const callback[] = "", const fmat[] = "", {Float,_}:...) {
    return stmt_execute(statement, TYPE_PARALLEL, callback, fmat, ___(3));
}

// --
// YSI - Inline
// --
#if defined _INC_y_inline

    #if YSI_VERSION_MAJOR != 5
        #error Please update "https://github.com/pawn-lang/YSI-Includes" to version 5.x in order to use the inline version of this library.
    #endif

    static stock bool:stmt_execute_inline(Statement:statement, QueryType:type, Func:callback<>) {

        if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
            Errorf("(stmt_execute_inline) Invalid Statement (%d).", _:statement);
            return false;
        }

        if (mysql_errno(gs_Statements[statement][E_STMT_MYSQL_HANDLE]) != 0) {
            Errorf("(stmt_execute_inline) Invalid Connection (%d).", _:statement);
            return false;
        }

        // Make sure all parameters have been set.
        for (new i = 0; i < gs_Statements[statement][E_STMT_MYSQL_PARAM_COUNT]; i++) {
            if (gs_Statements[statement][E_STMT_MYSQL_PARAM_TYPE][i] == TYPE_NONE) {
                Errorf("(stmt_execute_inline) Parameter (%d) not started in statement", i);
                return false;
            }
        }

        // write
        strunpack(gs_szBuffer, gs_Statements[statement][E_STMT_MYSQL_QUERY]);

        new
            ret;

        if(_:type == 0) {
            ret = mysql_tquery(gs_Statements[statement][E_STMT_MYSQL_HANDLE], gs_szBuffer, "Indirect_FromCallback", "ii", _:callback, true);
        } else {
            ret = mysql_pquery(gs_Statements[statement][E_STMT_MYSQL_HANDLE], gs_szBuffer, "Indirect_FromCallback", "ii", _:callback, true);
        }

        // throw error if query failed.
        if(!ret) {
            Errorf("(stmt_execute_inline:%d) failed to execute the query", _:statement);
            return false;
        } else {
            // tell the memory to stay
            Indirect_Claim(callback);
        }

        gs_Statements[statement][E_STMT_MYSQL_ROWS_FETCHED] = 0;
        totalRows[statement] = 0;

        #if MYSQL_PREPARE_DEBUG
            Debug("(stmt_execute_inline:%d) Statement executed.", _:statement);
            Debug("(stmt_execute_inline:%d) Query executed [%s]", _:statement, gs_szBuffer);
        #endif
        return bool:ret;
    }
    stock bool:MySQL_ExecuteThreaded_Inline(Statement:statement, Func:callback<>) {
        return stmt_execute_inline(statement, TYPE_THREADED, callback);
    }

    stock bool:MySQL_ExecuteParallel_Inline(Statement:statement, Func:callback<>) {
        return stmt_execute_inline(statement, TYPE_PARALLEL, callback);
    }
#else
    #error You need to "#include <YSI_Coding/y_inline>" before using this library.
#endif


// --
// Removing the debugging function, lets keep it local to this include only.
// This is to avoid any conflict with other libraries that has the following 
// functions below e.g pawn-errors
// --
#undef Warning               
#undef Warningf
#undef Error
#undef Errorf
#undef Debug
