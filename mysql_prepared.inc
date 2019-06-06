#include <YSI_Coding\y_va>

#define MAX_PARAMS  		(32)
#define MAX_FIELD_NAME  	(66)
#define MAX_STATEMENTS  	(16)
#define MAX_STATEMENT_SIZE  (1024)
#define MAX_FIELDS  		(64)

#define Warning(%1)   print(!"Warning: " %1)
#define Warningf(%1)  printf("Warning: " %1)

#define Error(%1)     print(!"Error: " %1)
#define Errorf(%1)    printf("Error: " %1)

#define Debug(%1)  	  printf("Debug: " %1)

#define DEBUG         (true)

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

static enum E_statement {
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

	// How many rows were fetched from the most recent result
	E_STMT_MYSQL_TOTAL_ROWS,

	// Length of parameters in the query string.
	E_STMT_MYSQL_PARAM_LEN[MAX_PARAMS],

	// The database it was created for
	MySQL:E_STMT_MYSQL_HANDLE,

	bool:E_STMT_COUNT_ROW_FIRST
};
static stock
	gs_szBuffer[8192],
	gs_Statements[Statement:MAX_STATEMENTS][E_statement];

// --
//  Internal Functions
// --
static escape_string(szString[], const szEnclosing[] = "'", argSize = sizeof(szString)) {
	new
		iPos
	;

	while (-1 != (iPos = strfind(szString, szEnclosing, _, iPos))) {
		strins(szString, szEnclosing, iPos, argSize);

		iPos += 2;
	}
}

static bool:stmt_execute(Statement:statement, type, const callback[] = "", const fmat[] = "", {Float,_}:...) {
	//no valid statement, throw error.
	if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
		Errorf("(stmt_execute) Invalid Statement (%d).", _:statement);
		return false;
	}

	// no valid connection, throw error.
	if (!gs_Statements[statement][E_STMT_MYSQL_HANDLE]) {
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

	new sucess = type ? mysql_tquery(gs_Statements[statement][E_STMT_MYSQL_HANDLE], gs_szBuffer, callback, fmat, ___(4)) : mysql_pquery(gs_Statements[statement][E_STMT_MYSQL_HANDLE], gs_szBuffer, callback, fmat, ___(4));

	if(!sucess) {
		Errorf("(stmt_execute:%d) failed to execute the query", _:statement);
		return false;
	}

	Debug("(stmt_execute:%d) Statement executed.", _:statement);
	#if DEBUG
		printf("Query executed [%s]", gs_szBuffer);
	#endif
	return true;
}

static bool:stmt_bind_value(&Statement:statement, param, DataType:type, {Float,_}:...)
{

	new
		lengthDifference,
		queryLength
	;

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

	#if DEBUG
	if (ispacked(gs_szBuffer))
		strunpack(gs_szBuffer, gs_szBuffer);

	Debug("(stmt_bind_value:%d) New parameter entered for %d in %d: %s", _:statement, param, gs_Statements[statement][E_STMT_MYSQL_PARAM_POS][param], gs_szBuffer);
	#endif

	gs_Statements[statement][E_STMT_MYSQL_PARAM_LEN][param] = queryLength;
	gs_Statements[statement][E_STMT_MYSQL_PARAM_TYPE][param] = type;

	return true;
}

static stmt_bind_result_field(Statement:statement, field, DataType:type, {Float, _}:...) {
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

	// Without this, STOR.S.pri doesn't seem to do what it should.
	address = 0;

	#emit LOAD.S.pri 24
	#emit STOR.S.pri address

	gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field] = type;
	gs_Statements[statement][E_STMT_MYSQL_FIELD_ADDRESS][field] = address;
	gs_Statements[statement][E_STMT_MYSQL_FIELD_SIZE][field] = argSize;

	Debug("(stmt_bind_result_field:%d) Bound result field %d (type %d) to variable 0x%04x%04x.", _:statement, field, _:type, address >>> 16, address & 0xFFFF);
}

// --
// External
// --

stock Statement:MySQL_PrepareStatement(MySQL:Connection, const szQuery[]) {

	new
		Statement:statement = INVALID_STATEMENT,
		paramPos,
		i,
		queryLength
	;

	if(Connection == INVALID_CONNECTION) {
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
		if (!gs_Statements[Statement:i][E_STMT_MYSQL_HANDLE]) {
			statement = Statement:i;
			break;
		}
	}

	if (statement == INVALID_STATEMENT) {
		Error("(stmt_prepare) No slots found free to do statament. Increase MAX_STATEMENTS.");
		return INVALID_STATEMENT;
	}

	gs_Statements[statement][E_STMT_MYSQL_HANDLE] = Connection;


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
	gs_Statements[statement][E_STMT_MYSQL_PARAM_COUNT] = i;
	gs_Statements[statement][E_STMT_MYSQL_QUERY][0] = 0;
	gs_Statements[statement][E_STMT_MYSQL_TOTAL_ROWS] = 0;
	gs_Statements[statement][E_STMT_COUNT_ROW_FIRST] = true;

	if (ispacked(szQuery)) {
	#if DEBUG
		strunpack(gs_szBuffer, szQuery);
		Debug("(stmt_prepare:%d) Preparing statement with %d parameters: %s", _:statement, i, gs_szBuffer);
	#endif
		strcat(gs_Statements[statement][E_STMT_MYSQL_QUERY], szQuery, MAX_STATEMENT_SIZE);
	} else {
		Debug("(stmt_prepare:%d) Preparing statement with %d parameters: %s", _:statement, i, szQuery);

		strpack(gs_Statements[statement][E_STMT_MYSQL_QUERY], szQuery, MAX_STATEMENT_SIZE);
	}
	return statement;
}

stock bool:stmt_fetch_row(Statement:statement) {
	if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
		Errorf("(stmt_fetch_row) Invalid statement passed (%d).", _:statement);
		return false;
	}

	new
		fields,
		address,
		value,
		count,
		numRows;


	if(gs_Statements[statement][E_STMT_COUNT_ROW_FIRST]) {

		cache_get_row_count(numRows);

		gs_Statements[statement][E_STMT_MYSQL_TOTAL_ROWS] = numRows;
		gs_Statements[statement][E_STMT_COUNT_ROW_FIRST] = false;

		// why not be fussy, singular or plural
		Debug("(stmt_fetch_row:%i) Counted %i row%s", _:statement, numRows, numRows > 1 ? ("s") : (""));
	}

	cache_get_field_count(fields);

	if (fields > MAX_FIELDS) {
		Warning("(stmt_bind_result_field) There are more fields returned than MAX_FIELDS.");
		fields = MAX_FIELDS;
	}

	if(!stmt_rows_left(statement)) {
		Debug("(stmt_fetch_row) No rows left.");
		// if no more rows to recover, we stop the loop
		return false;
	}

	// assign the global total rows to numRows.
	numRows = gs_Statements[statement][E_STMT_MYSQL_TOTAL_ROWS];
	// decrement total rows.
	gs_Statements[statement][E_STMT_MYSQL_TOTAL_ROWS] --;

	for (new field = 0; field < fields; field++) {
		if (gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field] == TYPE_NONE)
			continue;

		count++;

		switch (gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field]) {
			case TYPE_NONE,
				TYPE_NULL:
				continue;

			case TYPE_INT,
				TYPE_FLOAT: {

				if(numRows > 1) {
					for(new _loop_rows = 0; _loop_rows < numRows; _loop_rows++) {
						cache_get_value_index(_loop_rows, field, gs_szBuffer, sizeof(gs_szBuffer) - 1);
					}
				} else {
					cache_get_value_index(0, field, gs_szBuffer, sizeof(gs_szBuffer) - 1);
				}
				address = gs_Statements[statement][E_STMT_MYSQL_FIELD_ADDRESS][field];
				value = (gs_Statements[statement][E_STMT_MYSQL_FIELD_TYPE][field] != TYPE_FLOAT) ? strval(gs_szBuffer) : _:floatstr(gs_szBuffer);

				#emit LOAD.S.pri value
				#emit SREF.S.pri address
			}

			case TYPE_STRING,
			     TYPE_RAW_STRING: {
				new
					argSize
				;

				static const
					sc_szFormatString[] = "%s"
				;

				address  = gs_Statements[statement][E_STMT_MYSQL_FIELD_ADDRESS][field];
				argSize     = gs_Statements[statement][E_STMT_MYSQL_FIELD_SIZE][field];

				#emit PUSH.S    argSize //push address of variable argSize
				#emit PUSH.S    address //push address of variable address
				#emit PUSH.S    field //push address of variable field
				#emit PUSH.C    0 //retrieve first row
				#emit PUSH.C    16
				#emit SYSREQ.C  cache_get_value_index // call cache_get_value_index
				#emit STACK     20 //move stack to 20


				// format(output[], len, const format[], {Float,_}:...)
				#emit PUSH.S     address //push address of variable address
				#emit PUSH.C     sc_szFormatString //(%s into variable)
				#emit PUSH.S     argSize //push address of variable argSize
				#emit PUSH.S     address //push address of variable address
				#emit PUSH.C     16
				#emit SYSREQ.C   format //call format
				#emit STACK      20 //move stack to 20
			}
		}
		Debug("(stmt_fetch_row:%d) Fetched %d fields.", _:statement, count);
	}
	return true;
}

stock stmt_rows_left(&Statement:statement) {

	if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
		Errorf("(stmt_rows_left) Invalid statement passed (%d).", _:statement);
		return false;
	}
	return gs_Statements[statement][E_STMT_MYSQL_TOTAL_ROWS];
}

// Wrappers

// --
// Writing
// --
stock MySQL_Bind(Statement:statement, param, const str[]) {
	stmt_bind_value(statement, param, TYPE_STRING, str);
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

stock MySQL_StatementClose(Statement:statement){
	//no valid statement, throw error.
	if (statement == INVALID_STATEMENT || !(0 <= _:statement < sizeof(gs_Statements))) {
		Errorf("(stmt_close) Invalid Statement (%d).", _:statement);

		return false;
	}

	Debug("(stmt_close:%d) Statament closed.", _:statement);

	// reset
	gs_Statements[statement][E_STMT_MYSQL_HANDLE] = INVALID_CONNECTION;
	statement = INVALID_STATEMENT;
	return true;
}

stock bool:MySQL_ExecuteThreaded(Statement:statement, const callback[] = "", const fmat[] = "", {Float,_}:...) {
	return stmt_execute(statement, 0, callback, fmat, ___(3));
}

stock bool:MySQL_ExecuteParallel(Statement:statement, const callback[] = "", const fmat[] = "", {Float,_}:...) {
	return stmt_execute(statement, 1, callback, fmat, ___(3));
}