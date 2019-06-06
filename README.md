# mysql-prepared-stmt

[![sampctl](https://shields.southcla.ws/badge/sampctl-mysql--prepared--stmt-2f2f2f.svg?style=for-the-badge)](https://github.com/PatrickGTR/mysql-prepared-stmt)

<!--
Short description of your library, why it's useful, some examples, pictures or
videos. Link to your forum release thread too.

Remember: You can use "forumfmt" to convert this readme to forum BBCode!

What the sections below should be used for:

`## Installation`: Leave this section un-edited unless you have some specific
additional installation procedure.

`## Testing`: Whether your library is tested with a simple `main()` and `print`,
unit-tested, or demonstrated via prompting the player to connect, you should
include some basic information for users to try out your code in some way.

And finally, maintaining your version number`:

* Follow [Semantic Versioning](https://semver.org/)
* When you release a new version, update `VERSION` and `git tag` it
* Versioning is important for sampctl to use the version control features

Happy Pawning!
-->

## Special Thanks to:
* **maddinat0r** - MySQL
* **Southclaws** - sampctl
* **JustMichael** - constructive discussion on Discord
* **Y_Less** - For YSI and constructive discussion on Discord
* **Dayvison** - Initial Idea


## Installation

Simply install to your project:

```bash
sampctl package install PatrickGTR/mysql-prepared-stmt
```

Include in your code and begin using the library:

```pawn
#include <mysql_prepared>
```

## Usage

<!--
Write your code documentation or examples here. If your library is documented in
the source code, direct users there. If not, list your API and describe it well
in this section. If your library is passive and has no API, simply omit this
section.
-->
#### Functions
```pawn
MySQL_StatementClose(Statement:statement)
MySQL_PrepareStatement(MySQL:handle, const query[])
MySQL_Statement_RowsLeft(&Statement:statement)
MySQL_Statement_FetchRow(Statement:statement)
```
#### Writing
```pawn
MySQL_Bind(Statement:statement, param, const str[]) 
MySQL_BindInt(Statement:statement, param, value)
MySQL_BindFloat(Statement:statement, param, Float:value)
```
#### Reading
```pawn
MySQL_BindResult(Statement:statement, field, const result[], len = sizeof(result))
MySQL_BindResultInt(Statement:statement, field, &result)
MySQL_BindResultFloat(Statement:statement, field, &Float:result)
```
#### Executing 
```pawn
MySQL_ExecuteThreaded(Statement:statement, const callback[] = "", const fmat[] = "", {Float,_}:...)
MySQL_ExecuteParallel(Statement:statement, const callback[] = "", const fmat[] = "", {Float,_}:...)
MySQL_ExecuteThreaded_Inline(Statement:statement, Func:callback<>)
MySQL_ExecuteParallel_Inline(Statement:statement, Func:callback<>)
```

## Testing

<!--
Depending on whether your package is tested via in-game "demo tests" or
y_testing unit-tests, you should indicate to readers what to expect below here.
-->

To test, simply run the package:

```bash
sampctl package run
```


## Examples

### Reading Data (Using inline)
```pawn
  stmt_readloop = MySQL_PrepareStatement(MySQLHandle, "SELECT * FROM spawns");

  // Run Threaded on statement
  inline OnSpawnsLoad() {
      new
      spawnID,
      Float:spawnX,
      Float:spawnY,
      Float:spawnZ,
      Float:spawnA;

      MySQL_BindResultInt(stmt_readloop, 0, spawnID);
      MySQL_BindResultFloat(stmt_readloop, 1, spawnX);
      MySQL_BindResultFloat(stmt_readloop, 2, spawnY);
      MySQL_BindResultFloat(stmt_readloop, 3, spawnZ);
      MySQL_BindResultFloat(stmt_readloop, 4, spawnA);

      while(MySQL_Statement_FetchRow(stmt_readloop)) {
          printf("%i, %.3f, %.3f, %.3f", spawnID, spawnX, spawnY, spawnZ, spawnA);
      }
      MySQL_StatementClose(stmt_readloop);
  }
  MySQL_ExecuteThreaded_Inline(stmt_readloop, using inline OnSpawnsLoad);
```

### Writing Data
```pawn
  new Statement: stmt_insert = MySQL_PrepareStatement(MySQLHandle, "INSERT INTO accounts(username, password, salt, money, kills, deaths) VALUES (?,?,?,?,?,?) " );

  // Arrow values in questions (first 0, second is 1, etc ...)
  MySQL_Bind(stmt_insert, 0 , "patrickgtr");
  MySQL_Bind(stmt_insert, 1 , "patrickgtrpassword");
  MySQL_Bind(stmt_insert, 2 , "pgtrhash");
  MySQL_BindInt(stmt_insert, 3, 100);
  MySQL_BindInt(stmt_insert, 4, 200);
  MySQL_BindInt(stmt_insert, 5, 300);

  MySQL_ExecuteParallel(stmt_insert);
  MySQL_StatementClose(stmt_insert);
```
