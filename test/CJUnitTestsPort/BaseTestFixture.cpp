/* Copyright (C) 2007 - 2008 MySQL AB, 2008 - 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   There are special exceptions to the terms and conditions of the GPL
   as it is applied to this software. View the full text of the
   exception in file EXCEPTIONS-CONNECTOR-C++ in the directory of this
   software distribution.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <time.h>
#include <stdlib.h>

#include "BaseTestFixture.h"
#include "../common/stringutils.h"

namespace testsuite
{
static const String defaultHost=_T("127.0.0.1");
static const String defaultPort=_T("3306");
static const String defaultDb=_T("test");
static const String defaultLogin=_T("root");
static const String defaultPasswd=_T("root");

int TestFixtureCommon::instanceCount=1;

Properties TestFixtureCommon::sqlProps;

static const char * possiblePropertiesLocations[]={".."
  , "test/CJUnitTestsPort"
  , NULL //last should be NULL
};

int TestFixtureCommon::propsLoaded=resources::LoadProperties("sql.properties"
                                                             , sqlProps
                                                             , possiblePropertiesLocations);

Driver * TestFixtureCommon::driver=NULL;

TestFixtureCommon::TestFixtureCommon()
{
  init();
}

void TestFixtureCommon::init()
{
  host=TestsRunner::theInstance().getStartOptions()->dbUrl;
  login=TestsRunner::theInstance().getStartOptions()->dbUser;
  passwd=TestsRunner::theInstance().getStartOptions()->dbPasswd;
  db=TestsRunner::theInstance().getStartOptions()->dbSchema;
}

String TestFixtureCommon::extractVal(const String & sTableName
                                     , int count
                                     , Properties & sqlProps
                                     , Connection & conn)
{
  String sKeyName;
  String insertString;
  String retStr;
  String parameters;
  List sToken;

  try
  {
    sKeyName=sTableName;
    sKeyName.append("_Insert");

    insertString=sqlProps[ sKeyName ];

    String::size_type openPar=insertString.find_first_of("(") + 1;

    parameters=insertString.substr(
                                   openPar, insertString.find_first_of(")", 1) - openPar);

    StringUtils::split(sToken, parameters, ",");

    retStr=sToken[ count - 1];

    if (sTableName == "Bit_Tab"
        || sTableName == "Boolean_Tab")
    {
      if (retStr == "1")
      {
        retStr="true";
      } else if (retStr == "0")
      {
        retStr="false";
      }
    }
  } catch (std::exception & e)
  {
    logErr(String("Exception ") + e.what());
    FAIL("Call to extractVal is Failed!");
  }

  return retStr;
}

void TestFixtureCommon::logMsg(String message)
{
  TestsListener::theInstance().messagesLog() << message << std::endl;
}

void TestFixtureCommon::logErr(String message)
{
  TestsListener::theInstance().errorsLog() << message << std::endl;
  /*new Throwable().printStackTrace();*/
}

String TestFixtureCommon::randomString()
{
  srand((unsigned) time(NULL));

  int length=(rand() % 32);
  String buf, ch("a");

  buf.resize(length);

  for (int i=0; i < length; i++)
  {
    ch.assign(static_cast<String::size_type> (1), static_cast<String::value_type> ('a' + rand() % 26));
    buf.append(ch);
  }

  return buf;
}

/** value_object methods */

value_object::value_object() : wasNull(true)
{
}

value_object::value_object(const sql::ResultSet * rs, int colNum)
: asString(rs->getString(colNum))
{
  wasNull=rs->wasNull();
}

bool value_object::isNull() const
{
  return wasNull;
}

int value_object::intValue() const
{
  return StringUtils::toInt(asString, wasNull);
}

float value_object::floatValue() const
{
  return StringUtils::toFloat(asString, wasNull);
}

long long value_object::longValue() const
{
  return StringUtils::toLong(asString, wasNull);
}

value_object * getObject(sql::ResultSet * rs, int colNum)
{
  return new value_object(rs, colNum);
}

/* few temporary hacks*/
bool value_object::instanceof(value_type type)
{
  if (type < vtLast)
  {
    switch (type) {
    case vtByte: return true;
    case vtFloat:
    case vtDouble:
    {
      long long llValue=longValue();

      if (llValue != 0)
      {
        double dValue=doubleValue();
        return ( dValue > llValue ? dValue - llValue : llValue - dValue) > 0.001;
      }
    }

    default: return false;
    }

  }

  return false;
}

/**
 * Creates a new BaseTestFixture object.
 *
 * @param name
 *            The name of the JUnit test case
 */


BaseTestFixture::BaseTestFixture(const String & name)
: super(name),

TestFixtureCommon(),

myInstanceNumber(0),
conn(NULL),
pstmt(NULL),
stmt(NULL),
rs(NULL),
/*dbClass           ( "sql::mysql::Driver"),*/
hasSps(true)

{
  this->myInstanceNumber=instanceCount++;
}

/* throws SQLException & */

void BaseTestFixture::createSchemaObject(String objectType, String objectName,
                                         String columnsAndOtherStuff)
{
  this->createdObjects.push_back(objectType);
  this->createdObjects.push_back(objectName);

  dropSchemaObject(objectType, objectName);
  String createSql(_T("CREATE  "));

  createSql.resize(objectName.length()
                   + objectType.length() + columnsAndOtherStuff.length() + 10);

  createSql.append(objectType);
  createSql.append(" ");
  createSql.append(objectName);
  createSql.append(" ");
  createSql.append(columnsAndOtherStuff);
  this->stmt->executeUpdate(createSql);
}

/* throws SQLException & */
void BaseTestFixture::createFunction(String functionName, String functionDefn)
{
  createSchemaObject("FUNCTION", functionName, functionDefn);
}

/* throws SQLException & */
void BaseTestFixture::dropFunction(String functionName)
{
  dropSchemaObject("FUNCTION", functionName);
}

/* throws SQLException & */
void BaseTestFixture::createProcedure(String procedureName, String procedureDefn)
{
  createSchemaObject("PROCEDURE", procedureName, procedureDefn);
}

/* throws SQLException & */

void BaseTestFixture::dropProcedure(String procedureName)
{
  dropSchemaObject("PROCEDURE", procedureName);
}

/* throws SQLException & */

void BaseTestFixture::createTable(String tableName, String columnsAndOtherStuff)
{
  createSchemaObject("TABLE", tableName, columnsAndOtherStuff);
}

/* throws SQLException & */

void BaseTestFixture::dropTable(String tableName)
{
  dropSchemaObject("TABLE", tableName);
}

/* throws SQLException & */

void BaseTestFixture::dropSchemaObject(String objectType, String objectName)
{
  this->stmt->executeUpdate(String("DROP ") + objectType + " IF EXISTS "
                            + objectName);
}

/* throws SQLException & */

sql::Connection * BaseTestFixture::getAdminConnection()
{
  return getAdminConnectionWithProps(Properties());
}

/* throws SQLException & */

sql::Connection * BaseTestFixture::getAdminConnectionWithProps(Properties props)
{
  if (driver)
  {
    return driver->connect(host, /*port,*/ login, passwd); //adminUrl, props);
  } else
  {
    return NULL;
  }
}


/* getConnectionWith Props are legacy. Currently not used. Require changes in order to work */

/* throws SQLException & */

sql::Connection * BaseTestFixture::getConnectionWithProps(const String & propsList)
{
  return getConnectionWithProps(host, propsList);
}

/* throws SQLException & */

sql::Connection * BaseTestFixture::getConnectionWithProps(const String & url
                                                          , const String & propsList)
{
  Properties props;

  List keyValuePairs;
  StringUtils::split(keyValuePairs, propsList, _T(","), false);

  for (Iterator iter=keyValuePairs.begin(); iter != keyValuePairs.end(); ++iter)
  {
    String kvp= *iter;
    List splitUp;
    StringUtils::split(splitUp, kvp, _T("="), false);
    String value;

    for (unsigned i=1; i < splitUp.size(); i++)
    {
      if (i != 1)
      {
        value.append(_T("="));
      }
      value.append(splitUp[ i ]);
    }
    props[ StringUtils::trim(splitUp[ 0 ]) ]=value;
  }

  return getConnectionWithProps(url, props);
}

/**
 * Returns a new connection with the given properties
 *
 * @param props
 *            the properties to use (the URL will come from the standard for
 *            this testcase).
 *
 * @return a new connection using the given properties.
 *
 * @throws SQLException &
 *             DOCUMENT ME!
 */

/* throws SQLException & */

sql::Connection * BaseTestFixture::getConnectionWithProps(const Properties & props)
{
  return driver ? driver->connect(host, /*port,*/ login, passwd) : NULL; //dbUrl, props);
}

/* throws SQLException & */

sql::Connection * BaseTestFixture::getConnectionWithProps(const String & url, const Properties & props)
{
  return driver ? driver->connect(host, /*port,*/ login, passwd) : NULL; //dbUrl, props);
}

/**
 * Returns the per-instance counter (for messages when multi-threading
 * stress tests)
 *
 * @return int the instance number
 */

int BaseTestFixture::getInstanceNumber()
{
  return this->myInstanceNumber;
}

/* throws SQLException & */

String BaseTestFixture::getMysqlVariable(Connection & c
                                         , const String & variableName)
{
  value_object value(getSingleIndexedValueWithQuery(c, 2,
                                                    "SHOW VARIABLES LIKE '" + variableName + "'"));

  if (!value.isNull())
  {
    if (value.instanceof(value_object::vtByte))
    {
      // workaround for bad 4.1.x bugfix  - c/j legacy comment
      return value.toString(); //new String((byte[]) value);
    }
  }

  return value.toString();
}

/**
 * Returns the named MySQL variable from the currently connected server.
 *
 * @param variableName
 *            the name of the variable to return
 *
 * @return the value of the given variable, or NULL if it doesn't exist
 *
 * @throws SQLException &
 *             if an error occurs
 */

/* throws SQLException & */

String BaseTestFixture::getMysqlVariable(const String & variableName)
{
  return getMysqlVariable(this->conn, variableName);
}

/**
 * Returns the properties that represent the default URL used for
 * connections for all testcases.
 *
 * @return properties parsed from sql::mysql::testsuite.url
 *
 * @throws SQLException &
 *             if parsing fails
 */

/* throws SQLException & */
/* we currently don't use neither urls, nor properties - commented out*/
/*template <typename ConcreteTestFixture>
Properties BaseTestFixture::getPropertiesFromTestsuiteUrl()
{
Properties props = new NonRegisteringDriver().parseURL(dbUrl, NULL);
String hostname = props[ NonRegisteringDriver.HOST_PROPERTY_KEY ];

if ( hostname.size() == 0 )
{
props.insert(Properties::value_type(NonRegisteringDriver.HOST_PROPERTY_KEY,
_T("localhost") ));
}
else if ( hostname.substr(0,1) == _T(":") )
{
props.insert(Properties::value_type(NonRegisteringDriver.HOST_PROPERTY_KEY,
_T( "localhost" ) ) );
props.insert( Properties::value_type( NonRegisteringDriver.PORT_PROPERTY_KEY, hostname
.substr(1) ) );
}

String portNumber = props[ NonRegisteringDriver.PORT_PROPERTY_KEY ];

if (portNumber.size() == 0 )
{
props.insert( Properties::value_type( NonRegisteringDriver.PORT_PROPERTY_KEY, _T("3306") ) );
}

return props;
}*/

/* throws SQLException & */

int BaseTestFixture::getRowCount(const String & tableName)
{
  ResultSet countRs;

  try
  {
    countRs.reset(this->stmt->executeQuery("SELECT COUNT(*) FROM "
                                           + tableName));

    countRs->next();

    return countRs->getInt(1);
  } catch (...)
  {
  }

  return -1;
}

/* throws SQLException & */

value_object BaseTestFixture::getSingleIndexedValueWithQuery(Connection & c,
                                                             int columnIndex, const String & query)
{
  ResultSet valueRs;
  Statement svStmt;

  try
  {
    svStmt.reset(c->createStatement());
    valueRs.reset(svStmt->executeQuery(query));

    if (!valueRs->next())
    {
      return value_object();
    }

    return value_object(valueRs.get(), columnIndex);

  } catch (...)
  {
  }

  return value_object();
}

/* throws SQLException & */
value_object BaseTestFixture::getSingleIndexedValueWithQuery(int columnIndex,
                                                             const String & query)
{
  return getSingleIndexedValueWithQuery(this->conn, columnIndex, query);
}

/* throws SQLException & */
value_object BaseTestFixture::getSingleValue(const String & tableName, const String & columnName,
                                             const String & whereClause)
{
  return getSingleValueWithQuery(String("SELECT ") + columnName + " FROM "
                                 + tableName + ((whereClause.size() > 0) ? "" : " " + whereClause));
}

/* throws SQLException & */

value_object BaseTestFixture::getSingleValueWithQuery(const String & query)
{
  return getSingleIndexedValueWithQuery(1, query);
}

bool BaseTestFixture::isAdminConnectionConfigured()
{
  return false/*System.getProperty(ADMIN_CONNECTION_PROPERTY_NAME) != NULL*/;
}

/* throws SQLException & */

bool BaseTestFixture::isServerRunningOnWindows()
{
  return (getMysqlVariable("datadir").find_first_of('\\') != String::npos);
}

void BaseTestFixture::logDebug(const String & message)
{
  // TODO: add possibility to turn on debug messages in command line
  if (false)
    logMsg(message);
}


/**
In conn/j tests newTempBinaryFile used to be in prepared statement's
setBinaryStream. It's not supported by conn/c++ (so far), so - commented out
 */

/*
File BaseTestFixture::newTempBinaryFile(String name, long size)
{
File tempFile = File.createTempFile(name, "tmp");
tempFile.deleteOnExit();
cleanupTempFiles(tempFile, name);
FileOutputStream fos = new FileOutputStream(tempFile);
BufferedOutputStream bos = new BufferedOutputStream(fos);
for (long i = 0; i < size; i++) {
bos.write((byte) i);
}
bos.close();
ASSERT(tempFile.exists());
ASSERT_EQUALS(size, tempFile.length());
return tempFile;
}*/


bool BaseTestFixture::runLongTests()
{
  return runTestIfSysPropDefined("sql::mysql::testsuite.runLongTests");
}

/**
 * Checks whether a certain system property is defined, in order to
 * run/not-run certain tests
 *
 * @param propName
 *            the property name to check for
 *
 * @return true if the property is defined.
 */

bool BaseTestFixture::runTestIfSysPropDefined(const String & propName)
{
  //String prop = System.getProperty(propName);
  return true; //(prop != NULL) && (prop.length() > 0);
}

bool BaseTestFixture::runMultiHostTests()
{
  return true; //!runTestIfSysPropDefined(NO_MULTI_HOST_PROPERTY_NAME);
}

/**
 * Creates resources used by all tests.
 *
 * @throws Exception
 *             if an error occurs.
 */

/* throws Exception */

void BaseTestFixture::setUp()
{
  this->createdObjects.clear();

  try
  {
    this->conn.reset(getConnection());
  } catch (sql::SQLException & sqle)
  {
    logErr(String("Couldn't get connection") + sqle.what());
    throw sqle;
  }

  this->stmt.reset(this->conn->createStatement());

  try
  {
    if (host.find_first_of("mysql") != String::npos)
    {
      this->rs.reset(this->stmt->executeQuery("SELECT VERSION()"));
      this->rs->next();
      logDebug("Connected to " + this->rs->getString(1));
      //this->rs->close();
      this->rs.reset();
    } else
    {
      logDebug("Connected to "
               + this->conn->getMetaData()->getDatabaseProductName()
               + " / "
               + this->conn->getMetaData()->getDatabaseProductVersion());
    }
  } catch (sql::SQLException & sqle)
  {
    logErr(sqle.what());
  }

  if (this->rs.get() != NULL)
    this->rs->close();

  DatabaseMetaData dbmd(conn->getMetaData());

  hasSps=dbmd->supportsStoredProcedures();

  stmt->execute(String("USE ") + (db.length() > 0 ? db : defaultDb));
}

sql::Connection * BaseTestFixture::getConnection()
{
  //Properties props = new Properties();
  //props.setProperty("jdbcCompliantTruncation", "false");
  //props.setProperty("runningCTS13", "true");
  if (driver == NULL)
  {
    driver=get_driver_instance();
    logMsg(String(_T("Done: loaded ")) + driver->getName());
  }

  if (host.length() == 0)
  {
    host=defaultHost;

    if (login.length() == 0)
    {
      login=defaultLogin;
      passwd=defaultPasswd;
    }
  }

  return driver->connect(host, /*port,*/ login, passwd);
}


/**
 * Destroys resources created during the test case.
 *
 * @throws Exception
 *             DOCUMENT ME!
 */

/* throws Exception */
void BaseTestFixture::tearDown()
{
  rs.reset();

  for (int i=0; i < static_cast<int> (this->createdObjects.size() - 1); i+=2)
  {
    try
    {
      dropSchemaObject(this->createdObjects[ i ], this->createdObjects[ i + 1 ]);
    } catch (sql::SQLException &)
    {
    }
  }


  stmt.reset();

  pstmt.reset();

  conn.reset();

}

/**
 * Checks whether the database we're connected to meets the given version
 * minimum
 *
 * @param major
 *            the major version to meet
 * @param minor
 *            the minor version to meet
 *
 * @return bool if the major/minor is met
 *
 * @throws SQLException &
 *             if an error occurs.
 */

/* throws SQLException & */

bool BaseTestFixture::versionMeetsMinimum(int major, int minor)
{
  return versionMeetsMinimum(major, minor, 0);
}

/**
 * Checks whether the database we're connected to meets the given version
 * minimum
 *
 * @param major
 *            the major version to meet
 * @param minor
 *            the minor version to meet
 *
 * @return bool if the major/minor is met
 *
 * @throws SQLException &
 *             if an error occurs.
 */

/* throws SQLException & */

bool BaseTestFixture::versionMeetsMinimum(int major, int minor, int subminor)
{
  return true;
  /*
  ((dynamic_cast<sql::mysql::MySQL_Connection*> (this->conn)->versionMeetsMinimum(
  major, minor, subminor));*/

}

/*template <typename ConcreteTestFixture>
bool BaseTestFixture::isClassAvailable(String classname)
{
try
{
Class.forName(classname);
return true;
}
catch (ClassNotFoundException e)
{
return false;
}
}*/


void BaseTestFixture::closeMemberJDBCResources()
{
  if (this->rs.get() != NULL)
  {
    ResultSet toClose=this->rs;
    this->rs.reset();

    try
    {
      toClose->close();
    } catch (sql::SQLException &)
    {
      // ignore
    }
  }
  if (this->pstmt.get() != NULL)
  {
    PreparedStatement toClose=this->pstmt;
    this->pstmt.reset();
    try
    {
      toClose->close();
    } catch (sql::SQLException &)
    {
      // ignore
    }
  }
}


/**
(almost) Same reasons as for createTempFile - for commenting out
 */

/*
void BaseTestFixture::cleanupTempFiles(const File exampleTempFile, const String tempfilePrefix)
{
File tempfilePath = exampleTempFile.getParentFile();
File[] possibleFiles = tempfilePath.listFiles(new FilenameFilter() {
public bool accept(File dir, String name) {
return (name.find_first_of(tempfilePrefix) != -1
&& !exampleTempFile.getName().equals(name));
}});
for (int i = 0; i < possibleFiles.length; i++) {
try {
possibleFiles[i].delete();
} catch (Throwable t) {
// ignore, we're only making a best effort cleanup attempt here
}
}
}*/

/* throws Exception */

void BaseTestFixture::assertResultSetsEqual(ResultSet & control, ResultSet & test)
{
  int controlNumCols=control->getMetaData()->getColumnCount();
  int testNumCols=test->getMetaData()->getColumnCount();

  ASSERT_EQUALS(controlNumCols, testNumCols);

  while (control->next())
  {
    test->next();
    for (int i=0; i < controlNumCols; i++)
    {
      Object controlObj(getObject(control.get(), i + 1));
      Object testObj(getObject(test.get(), i + 1));

      if (controlObj.get() == NULL)
      {
        ASSERT(testObj.get() == NULL);
      } else
      {
        ASSERT(testObj.get() != NULL);
      }

      /** don't really have getFloat **
      if (controlObj->instanceof( value_object::vtFloat ))
      {
      ASSERT_EQUALS_EPSILON(((float)controlObj).floatValue(),
      ((float)testObj).floatValue(), 0.1);
      }
      else*/

      if (controlObj->instanceof(value_object::vtDouble))
      {
        ASSERT_EQUALS_EPSILON(controlObj->doubleValue(),
                              testObj->doubleValue(), 0.1);
      } else
      {
        ASSERT_EQUALS(controlObj->toString(), testObj->toString());
      }
    }
  }

  ASSERT(!test->next());
}

void BaseTestFixture::initTable(const String & sTableName
                                , Properties & _sqlProps
                                , Connection & _conn)
{
  String execString;
  String sKeyName;
  String binarySize;
  String varbinarySize;
  String createString;
  String createString1;
  String createString2;
  Statement statement;

  try
  {
    clearTable(sTableName, _conn);
    logMsg("deleted rows from table " + sTableName);
  } catch (sql::SQLException & e)
  {
    logMsg("Exception encountered deleting rows from  table: "
           + sTableName + ": " + e.what());
  }

  try
  {
    statement.reset(_conn->createStatement());

    if ((sTableName.find_first_of("Binary_Tab") == 0))
    {
      binarySize=_sqlProps[ "binarySize" ];
      logMsg("Binary Table Size : " + binarySize);

      String insertString=_sqlProps[ "Binary_Tab_Insert" ];

      logMsg("Insert String " + insertString);
      logMsg("Adding rows to the table");
      statement->executeUpdate(insertString);

      logMsg("Successfully inserted the row");
    } else if ((sTableName.find_first_of("Varbinary_Tab") == 0))
    {
      varbinarySize=_sqlProps[ ("varbinarySize") ];

      TestsListener::theInstance().messagesLog()
              << "Varbinary Table Size : " << varbinarySize << std::endl;
      String insertString=_sqlProps[ "Varbinary_Tab_Insert" ];

      logMsg("Insert String " + insertString);
      logMsg("Adding rows to the table");
      statement->executeUpdate(insertString);

      logMsg("Successfully inserted the row");
    } else
    {
      logMsg("Adding rows to the table" + sTableName);
      sKeyName=sTableName;
      sKeyName.append("_Insert");

      logMsg("sKeyName :" + sKeyName);
      execString=_sqlProps[ sKeyName ];
      statement->executeUpdate(execString);

      logMsg("Rows added to the table " + sTableName);
    }
  } catch (sql::SQLException & e)
  {
    logErr("sql::DbcException creating the Table" + sTableName);
    logErr(e.what());
    throw e;
  } catch (std::exception &)
  {
    logErr("Setup Failed!");
    exit(1);
  }
}

void BaseTestFixture::clearTable(const String & sTableName
                                 , Connection & _conn)
{
  logMsg("dropTab");

  String sTag=sTableName;
  sTag.append("_Delete");

  String removeString=sqlProps[ sTag ];

  logMsg("Executable String " + removeString);

  Statement _stmt(_conn->createStatement());

  _stmt->executeUpdate(removeString);
  _stmt->close();
}
}