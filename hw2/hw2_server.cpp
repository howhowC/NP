#include <arpa/inet.h>
#include <cstdlib> // For exit() and EXIT_FAILURE
#include <cstring>
#include <ctime>    // For time setting
#include <iostream> // For cout
#include <netdb.h>
#include <netinet/in.h> // For sockaddr_in
#include <sqlite3.h>    // For sqlite
#include <sys/socket.h> // For socket functions
#include <sys/types.h>
#include <unistd.h> // For read
#include <vector>

enum state { USER = 100, VISITOR };
state now_state = VISITOR;
char now_user[256] = {0};
char re_cli_pw[256] = {0};

struct Users {
  int user_fd;
  char user_name[256];
  state nw_state;
};

std::vector<Users> vec;

static int callback(void *data, int argc, char **argv, char **azColName) {
  int in_i;
  fprintf(stderr, "%s: \n", (const char *)data);
  // data = argv[2];
  // printf("%s\n", data);
  for (in_i = 0; in_i < argc; in_i++) {
    // printf("%d %d\n", in_i, argc);
    printf("%s = %s\n", azColName[in_i], argv[in_i] ? argv[in_i] : "NULL");
  }
  printf("\n");
  return 0;
}

char tmp_list_splcmd[1024] = {0};
int list_i = 0;
static int list_board(void *data, int argc, char **argv, char **azColName) {
  int in_i;
  fprintf(stderr, "%s: \n", (const char *)data);
  // data = argv[2];
  // printf("%s\n", data);
  strcat(tmp_list_splcmd, "\t");
  for (in_i = 0; in_i < argc; in_i++) {
    // printf("%d %d\n", in_i, argc);
    printf("%s = %s\n", azColName[in_i], argv[in_i] ? argv[in_i] : "NULL");
    if (strcmp(azColName[in_i], "BoardID") == 0) {
      char tmp[64] = {0};
      list_i++;
      sprintf(tmp, "%d", list_i);
      strcat(tmp_list_splcmd, tmp);
    } else {
      strcat(tmp_list_splcmd, argv[in_i] ? argv[in_i] : "NULL");
    }
    if (argc - in_i == 1)
      strcat(tmp_list_splcmd, "\n");
    else
      strcat(tmp_list_splcmd, "\t\t");
  }
  printf("\n");
  return 0;
}

static int list_post(void *data, int argc, char **argv, char **azColName) {
  int in_i;
  fprintf(stderr, "%s: \n", (const char *)data);
  // data = argv[2];
  // printf("%s\n", data);
  strcat(tmp_list_splcmd, "\t");
  for (in_i = 0; in_i < argc; in_i++) {
    // printf("%d %d\n", in_i, argc);
    printf("%s = %s\n", azColName[in_i], argv[in_i] ? argv[in_i] : "NULL");
    strcat(tmp_list_splcmd, argv[in_i] ? argv[in_i] : "NULL");

    if (argc - in_i == 1)
      strcat(tmp_list_splcmd, "\n");
    else
      strcat(tmp_list_splcmd, "\t\t");
  }
  printf("\n");
  return 0;
}

char return_post[4096] = {0};
static int read_post(void *data, int argc, char **argv, char **azColName) {
  int in_i;
  fprintf(stderr, "%s: \n", (const char *)data);
  for (in_i = 0; in_i < argc; in_i++) {
    printf("%s = %s\n", azColName[in_i], argv[in_i] ? argv[in_i] : "NULL");
    char tmp[512] = {0};
    if ((in_i == 0) || (in_i == 1)) {
      sprintf(tmp, "%s\t:%s\n", azColName[in_i],
              argv[in_i] ? argv[in_i] : "NULL");
    } else if (in_i == 2) {
      sprintf(tmp, "Date\t:%s\n", argv[in_i] ? argv[in_i] : "NULL");
    } else {
      sprintf(tmp, "--\n%s\n--\n", argv[in_i] ? argv[in_i] : "NULL");
    }
    strcat(return_post, tmp);
  }
  printf("\n");
  return 0;
}

char return_comment[1024] = {0};
static int read_comment(void *data, int argc, char **argv, char **azColName) {
  int in_i;
  char tmp[1024] = {0};
  fprintf(stderr, "%s: \n", (const char *)data);
  for (in_i = 0; in_i < argc; in_i++) {
    printf("%s = %s\n", azColName[in_i], argv[in_i] ? argv[in_i] : "NULL");
  }
  sprintf(tmp, "%s: %s\n", argv[0], argv[1]);
  strcat(return_comment, tmp);
  printf("\n");
  return 0;
}

static int return_pw(void *data, int argc, char **argv, char **azColName) {
  memset(re_cli_pw, 0, sizeof(re_cli_pw));
  // argv[3] is the password
  strcat(re_cli_pw, argv[3]);
  return 0;
}

char input_cmd_board[128] = {0};
bool if_find_board = false;
static int find_board(void *data, int argc, char **argv, char **azColName) {
  int in_i;
  fprintf(stderr, "%s: \n", (const char *)data);
  // data = argv[2];
  // printf("%s\n", data);
  strcat(tmp_list_splcmd, "\t");
  for (in_i = 0; in_i < argc; in_i++) {
    // printf("%d %d\n", in_i, argc);
    printf("%s = %s\n", azColName[in_i], argv[in_i] ? argv[in_i] : "NULL");
    if (strcmp(input_cmd_board, argv[in_i]) == 0) {
      if_find_board = true;
      printf("find board is %s\n", argv[in_i]);
      break;
    }
  }
  printf("\n");
  return 0;
}

char return_postowner[128] = {0};
static int find_post(void *data, int argc, char **argv, char **azColName) {
  int in_i;
  fprintf(stderr, "%s: \n", (const char *)data);
  printf("find Author is %s\n", argv[0]);
  strcpy(return_postowner, argv[0]);

  return 0;
}

void login_user_fd(int index) {
  int vec_i;
  for (vec_i = 0; vec_i < sizeof(vec); vec_i++) {
    if (vec[vec_i].user_fd == index) {
      memset(vec[vec_i].user_name, 0, sizeof(vec[vec_i].user_name));
      strcpy(vec[vec_i].user_name, now_user);
      vec[vec_i].nw_state = now_state;

      printf("login--> %d (%s) %d\n", vec[vec_i].user_fd, vec[vec_i].user_name,
             vec[vec_i].nw_state);
    }
  }
}

void logout_user_fd(int index) {
  int vec_i;
  for (vec_i = 0; vec_i < sizeof(vec); vec_i++) {
    if (vec[vec_i].user_fd == index) {
      memset(vec[vec_i].user_name, 0, sizeof(vec[vec_i].user_name));
      strcpy(vec[vec_i].user_name, now_user);
      vec[vec_i].nw_state = now_state;

      printf("logout--> %d (%s) %d\n", vec[vec_i].user_fd, vec[vec_i].user_name,
             vec[vec_i].nw_state);
    }
  }
}

void find_user_fd(int index) {
  int vec_i;
  for (vec_i = 0; vec_i < sizeof(vec); vec_i++) {
    if (vec[vec_i].user_fd == index) {
      memset(now_user, 0, sizeof(now_user));
      strcpy(now_user, vec[vec_i].user_name);
      now_state = vec[vec_i].nw_state;

      printf("find--> %d (%s) %d\n", vec[vec_i].user_fd, vec[vec_i].user_name,
             vec[vec_i].nw_state);
    }
  }
}

int main(int argc, char const *argv[]) {

  if (argc != 2) {
    printf("Usage ./server <port number>\n");
    exit(1);
  }

  char delims[] = " ";
  const char
      *welcome_msg =
          "********************************\n** Welcome to the BBS server. "
          "**\n********************************\n% ",
      *logout_msg = "Please logout first.\n",
      *sql_op_success_msg = "Operation done successfully\n",
      *find_cli_pw_msg = "SELECT * from CLIENT where Username = \'";

  int yes = 1; // 供底下的 setsockopt() 設定 SO_REUSEADDR

  // sql part
  sqlite3 *client_db, *board_db, *post_db, *comment_db;
  char *zErrMsg = 0;
  int rc;
  char *sql, insert_value[1024];
  const char *data = "Callback function called",
             *select_all_client = "SELECT * from CLIENT;",
             *select_all_board = "SELECT * from BOARD;",
             *select_all_post = "SELECT * from POST;",
             *select_all_comment = "SELECT * from COMMENT;",
             *insert_to_table = "INSERT INTO CLIENT (Username,Email,Password) ",
             *insert_to_board = "INSERT INTO BOARD (Name,Moderator) ",
             *insert_to_post =
                 "INSERT INTO POST (Board,Title,Author,Date1,Date2,Content)",
             *create_table =
                 "CREATE TABLE CLIENT("
                 "UID           INTEGER     PRIMARY KEY AUTOINCREMENT,"
                 "Username      TEXT        NOT NULL UNIQUE,"
                 "Email         TEXT        NOT NULL,"
                 "Password      TEXT        NOT NULL"
                 ");",
             *create_board =
                 "CREATE TABLE BOARD("
                 "BoardID       INTEGER     PRIMARY KEY AUTOINCREMENT,"
                 "Name          TEXT        NOT NULL UNIQUE,"
                 "Moderator     TEXT        NOT NULL"
                 ");",
             *create_post =
                 "CREATE TABLE POST("
                 "PostID        INTEGER     PRIMARY KEY AUTOINCREMENT,"
                 "Board         TEXT        NOT NULL,"
                 "Title         TEXT        NOT NULL,"
                 "Author        TEXT        NOT NULL,"
                 "Date1         TEXT        NOT NULL,"
                 "Date2         TEXT        NOT NULL,"
                 "Content       TEXT        NOT NULL"
                 ");",
             *create_comment = "CREATE TABLE COMMENT("
                               "PostID        INTEGER     NOT NULL,"
                               "Username      TEXT        NOT NULL,"
                               "Comment       TEXT        NOT NULL"
                               ");";

  // declare time
  // time_t t = time(0);
  // char tmp[64];
  // strftime(tmp, sizeof(tmp), "%Y/%m/%d %X %A 本年第%j天 %z", localtime(&t));
  // puts(tmp);

  /* Open database */
  rc = sqlite3_open("client.db", &client_db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(client_db));
    exit(1);
  } else {
    fprintf(stderr, "Opened database successfully\n");
  }

  rc = sqlite3_open("board.db", &board_db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(board_db));
    exit(1);
  } else {
    fprintf(stderr, "Opened database successfully\n");
  }

  rc = sqlite3_open("post.db", &post_db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(post_db));
    exit(1);
  } else {
    fprintf(stderr, "Opened database successfully\n");
  }

  rc = sqlite3_open("comment.db", &comment_db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(comment_db));
    exit(1);
  } else {
    fprintf(stderr, "Opened database successfully\n");
  }

  /* Execute SQL statement */
  rc = sqlite3_exec(client_db, create_table, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    fprintf(stdout, "Create table successfully\n");
  }

  rc = sqlite3_exec(board_db, create_board, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    fprintf(stdout, "Create board successfully\n");
  }

  rc = sqlite3_exec(post_db, create_post, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    fprintf(stdout, "Create post successfully\n");
  }

  rc = sqlite3_exec(comment_db, create_comment, callback, (void *)data,
                    &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    fprintf(stdout, "Create comment successfully\n");
  }

  fd_set master;   // master file descriptor 清單
  fd_set read_fds; // 給 select() 用的暫時 file descriptor 清單
  int fdmax;       // 最大的 file descriptor 數目

  int listener;   // listening socket descriptor
  int connection; // 新接受的 accept() socket descriptor
  // sockaddr_in remoteaddr; // client address
  // socklen_t remote_addrlen;
  FD_ZERO(&master); // 清除 master 與 temp sets
  FD_ZERO(&read_fds);

  // Create a socket (IPv4, TCP)
  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener == -1) {
    std::cout << "Failed to create socket. errno: " << errno << std::endl;
    exit(EXIT_FAILURE);
  }

  if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("setsockopt");
    exit(1);
  }

  // Listen to port 8888 on any address
  sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = INADDR_ANY;
  sockaddr.sin_port = htons(atoi(argv[1])); // htons is necessary to convert a
                                            // number to network byte order
  if (bind(listener, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
    std::cout << "Failed to bind to port " << argv[1] << ". errno: " << errno
              << std::endl;
    exit(EXIT_FAILURE);
  }

  // Start listening. Hold at most 10 connections in the queue
  if (listen(listener, 11) < 0) {
    std::cout << "Failed to listen on socket. errno: " << errno << std::endl;
    exit(EXIT_FAILURE);
  }

  // 將 listener 新增到 master set
  FD_SET(listener, &master);

  // 持續追蹤最大的 file descriptor
  fdmax = listener; // 到此為止，就是它了
  // printf("fd max = %d\n", fdmax);
  // printf("-->%d\n", listener);

  while (1) {
    read_fds = master; // 複製 master

    // printf("fd max = %d\n", fdmax);
    // printf("-->%d\n", listener);

    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("select");
      exit(4);
    }
    // listen to connection
    if (FD_ISSET(listener, &read_fds)) {
      auto addrlen = sizeof(sockaddr);
      connection =
          accept(listener, (struct sockaddr *)&sockaddr, (socklen_t *)&addrlen);
      if (connection < 0) {
        std::cout << "Failed to grab connection. errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
      }

      FD_SET(connection, &master); // 新增到 master set
      if (connection > fdmax) {    // 持續追蹤最大的 fd
        fdmax = connection;
      }

      printf("Accept connection %d from %s:%d\n", listener,
             inet_ntoa(sockaddr.sin_addr), (int)ntohs(sockaddr.sin_port));

      send(connection, welcome_msg, strlen(welcome_msg), 0);
      printf("new connection is %d\n", connection);

      Users tmp_user;
      tmp_user.user_fd = connection;
      memset(tmp_user.user_name, 0, sizeof(tmp_user.user_name));
      tmp_user.nw_state = VISITOR;
      vec.push_back(tmp_user);
      // printf("-->%d (%s) %d\n", tmp_user.user_fd, tmp_user.user_name,
      //        tmp_user.nw_state);
    }

    for (int index = 5; index <= fdmax; index++) {
      if (FD_ISSET(index, &read_fds) && index != listener) {
        // 處理來自 client 的資料
        find_user_fd(index); // find which connection

        char cmd[4096];
        memset(cmd, '\0', sizeof(char) * 4096);
        printf("\nWaiting for new message...\n");
        auto bytesRead = read(index, cmd, 4096);
        // printf("index %d\n", index);
        // printf("---%d\n", bytesRead);

        cmd[bytesRead - 2] = '\0';
        cmd[bytesRead - 1] = '\0'; /* insure line null-terminated  */
        bytesRead -= 2;
        std::cout << "The message was: \"" << cmd << "\"\n";

        if (bytesRead <= 0) {
          // got error or connection closed by client
          if (bytesRead == 0) {
            // 關閉連線
            printf("selectserver: socket %d hung up\n", index);
          } else {
            perror("recv");
          }
          // close(index);           // bye!
          // FD_CLR(index, &master); // 從 master set 中移除

        }
        // ----------------------------register----------------------------
        else if (strncmp(cmd, "register", 8) == 0) {
          // find_user_fd(index);
          if (now_state == VISITOR) {
            if (bytesRead == 8) {
              std::string response =
                  "Usage: register <username> <email> <password>\n% ";
              send(index, response.c_str(), response.size(), 0);
            } else {
              char *next = nullptr;
              char buf[1024];
              sprintf(buf, "VALUES (");

              char *pch = strtok_r(cmd, delims, &next);
              pch = strtok_r(nullptr, delims, &next);
              // char tmp[128] = {0};
              bool first = true;
              int register_argc = 0;
              for (; pch; pch = strtok_r(nullptr, delims, &next)) {
                register_argc++;
                if (first) {
                  // printf("%s", pch);
                  first = false;
                } else {
                  // printf(", %s", pch);
                  strcat(buf, ", ");
                }
                strcat(buf, "\'");
                strcat(buf, pch);
                strcat(buf, "\'");
              }
              next = pch = nullptr;
              strcat(buf, ");");
              memset(insert_value, 0, sizeof(insert_value));
              strcat(insert_value, insert_to_table);
              strcat(insert_value, buf);
              // finish insert cmd
              printf("%s\n", insert_value);
              // Execute SQL statement
              rc = sqlite3_exec(client_db, insert_value, callback, (void *)data,
                                &zErrMsg);
              // register successfully
              if (register_argc != 3) {
                printf("register Usage error\n");
                std::string response =
                    "Usage: register <username> <email> <password>\n% ";
                send(index, response.c_str(), response.size(), 0);
              } else if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                std::string response = "Username is already used.\n% ";
                send(index, response.c_str(), response.size(), 0);
              }
              // register Failed
              else {
                fprintf(stdout, "%s", sql_op_success_msg);
                std::string response = "Register successfully.\n% ";
                send(index, response.c_str(), response.size(), 0);
              }
            }
          } else {
            std::string response =
                "Usage: register <username> <email> <password>\n% ";
            send(index, response.c_str(), response.size(), 0);
          }
        }
        // ------------------------------login------------------------------
        else if (strncmp(cmd, "login", 5) == 0) {
          // find_user_fd(index);
          if (now_state == VISITOR) {
            if (bytesRead == 5) {
              std::string response = "Usage: login <username> <password>\n% ";
              send(index, response.c_str(), response.size(), 0);
            } else {
              char *next = nullptr;
              char buf[1024] = {0}, login_name[256] = {0}, login_pw[256] = {0};
              char *pch = strtok_r(cmd, delims, &next);
              pch = strtok_r(nullptr, delims, &next);
              char tmp[128] = {0};
              bool first = true;
              int cli_argc = 0;
              for (; pch; pch = strtok_r(nullptr, delims, &next)) {
                if (cli_argc == 0) {
                  // printf("%s", pch);
                  // first = false;
                  strcat(login_name, pch);
                  cli_argc++;
                } else {
                  // printf(", %s", pch);
                  if (cli_argc == 1) {
                    strcat(login_pw, pch);
                  }
                  strcat(buf, ", ");
                  cli_argc++;
                }
                strcat(buf, "\'");
                strcat(buf, pch);
                strcat(buf, "\'");
              }
              next = pch = nullptr;
              printf("name is (%s)\n", login_name);
              printf("pw is (%s)\n", login_pw);

              // client argc
              if (cli_argc == 2) {
                memset(insert_value, 0, sizeof(insert_value));
                strcat(insert_value, find_cli_pw_msg);
                strcat(insert_value, login_name);
                strcat(insert_value, "\';");
                // finish insert cmd
                printf("%s\n", insert_value);

                // Execute SQL statement
                rc = sqlite3_exec(client_db, insert_value, return_pw,
                                  (void *)data, &zErrMsg);
                // printf("rv-> %s\n", re_cli_pw);
                if (rc != SQLITE_OK) {
                  fprintf(stderr, "SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                  sprintf(tmp, "SQL error: %s\n", zErrMsg);
                } else {
                  fprintf(stdout, "%s", sql_op_success_msg);
                  sprintf(tmp, "%s", sql_op_success_msg);
                }

                // login pw is right
                if (strcmp(login_pw, re_cli_pw) == 0) {
                  strcat(now_user, login_name);
                  now_state = USER;
                  login_user_fd(index);

                  char cli_welcome_msg[512] = {0};
                  sprintf(cli_welcome_msg, "Welcome, %s.\n%% ", now_user);
                  send(index, cli_welcome_msg, sizeof(cli_welcome_msg), 0);
                }
                // login pw is wrong
                else {
                  std::string response = "Login failed.\n% ";
                  send(index, response.c_str(), response.size(), 0);
                }

              } else {
                std::string response = "Usage: login <username> <password>\n% ";
                send(index, response.c_str(), response.size(), 0);
              }
            }
          } else {
            std::string response = "Please logout first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          }
        }
        // ----------------------------see all----------------------------
        else if (strncmp(cmd, "see", 3) == 0) {
          char tmp[1024] = {0};
          // Execute SQL statement
          rc = sqlite3_exec(client_db, select_all_client, callback,
                            (void *)data, &zErrMsg);
          if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            sprintf(tmp, "SQL error: %s\n", zErrMsg);
          } else {
            fprintf(stdout, "%s", sql_op_success_msg);
            // sprintf(tmp, "%s", sql_op_success_msg);
          }
          send(index, tmp, sizeof(tmp), 0);

          memset(tmp, 0, sizeof(tmp));
          rc = sqlite3_exec(board_db, select_all_board, callback, (void *)data,
                            &zErrMsg);
          if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            sprintf(tmp, "SQL error: %s\n", zErrMsg);
          } else {
            fprintf(stdout, "%s", sql_op_success_msg);
            // sprintf(tmp, "%s", sql_op_success_msg);
          }

          rc = sqlite3_exec(post_db, select_all_post, callback, (void *)data,
                            &zErrMsg);
          if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            sprintf(tmp, "SQL error: %s\n", zErrMsg);
          } else {
            fprintf(stdout, "%s", sql_op_success_msg);
          }

          rc = sqlite3_exec(comment_db, select_all_comment, callback,
                            (void *)data, &zErrMsg);
          if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            sprintf(tmp, "SQL error: %s\n", zErrMsg);
          } else {
            fprintf(stdout, "%s", sql_op_success_msg);
          }

          strcat(tmp, "% ");
          send(index, tmp, sizeof(tmp), 0);

        }
        // ------------------------------logout------------------------------
        else if (strncmp(cmd, "logout", 6) == 0) {
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            char cli_logout_msg[512] = {0};
            sprintf(cli_logout_msg, "Bye, %s.\n%% ", now_user);
            send(index, cli_logout_msg, sizeof(cli_logout_msg), 0);

            now_state = VISITOR;
            memset(now_user, 0, sizeof(now_user));
            logout_user_fd(index);
          }

        }
        // ------------------------------whoami------------------------------
        else if (strncmp(cmd, "whoami", 6) == 0) {
          // find_user_fd(index);
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            char cli_whoami_msg[256] = {0};
            sprintf(cli_whoami_msg, "%s\n%% ", now_user);
            send(index, cli_whoami_msg, sizeof(cli_whoami_msg), 0);
          }
        }
        // -------------------------create-board-------------------------
        else if (strncmp(cmd, "create-board", 12) == 0) {
          find_user_fd(index);
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else if (strlen(cmd) == 12) {
            std::string response = "Usage: create-board <board>\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {

            char *next = nullptr;
            char buf[1024] = {0};
            sprintf(buf, "VALUES (");

            char *pch = strtok_r(cmd, delims, &next);
            pch = strtok_r(nullptr, delims, &next);
            char tmp[128] = {0};
            bool first = true;
            for (; pch; pch = strtok_r(nullptr, delims, &next)) {
              if (first) {
                // printf("%s", pch);
                first = false;
              } else {
                // printf(", %s", pch);
                strcat(buf, ", ");
              }
              strcat(buf, "\'");
              strcat(buf, pch);
              strcat(buf, "\'");
            }
            strcat(buf, ", ");
            strcat(buf, "\'");
            strcat(buf, now_user);
            strcat(buf, "\'");
            strcat(buf, ");");
            next = pch = nullptr;
            memset(insert_value, 0, sizeof(insert_value));
            strcat(insert_value, insert_to_board);
            strcat(insert_value, buf);
            // finish insert cmd
            printf("%s\n", insert_value);
            // Execute SQL statement
            rc = sqlite3_exec(board_db, insert_value, callback, (void *)data,
                              &zErrMsg);
            if (rc != SQLITE_OK) {
              fprintf(stderr, "SQL error: %s\n", zErrMsg);
              sqlite3_free(zErrMsg);
              sprintf(tmp, "Board already exist.\n");
              printf("ErrMsg number is %d\n", rc);
            } else {
              fprintf(stdout, "%s", sql_op_success_msg);
              sprintf(tmp, "Create board successfully.\n");
            }
            strcat(tmp, "% ");
            send(index, tmp, sizeof(tmp), 0);
            strcat(buf, "\n");
          }
        }
        // -------------------------create-post-------------------------
        else if (strncmp(cmd, "create-post", 11) == 0) {
          find_user_fd(index);
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            char tmp[1024] = {0}, buf[1024] = {0}, title[128] = {0},
                 content[1024] = {0}, *ret, *title_pos, *content_pos;
            int content_len = 0;
            std::string checkargc(cmd, cmd + strlen(cmd));
            std::size_t foundboard, foundtitle, foundcontent;
            foundboard = checkargc.find("create-post ");
            foundtitle = checkargc.find("--title ");
            foundcontent = checkargc.find("--content ");
            if ((foundboard == std::string::npos) ||
                (foundtitle == std::string::npos) ||
                (foundcontent == std::string::npos)) {
              std::string response = "Usage: create-post <board> --title "
                                     "<title> --content <content>\n% ";
              send(index, response.c_str(), response.size(), 0);
            } else {
              ret = strstr(cmd, "create-post ");
              title_pos = strstr(cmd, "--title ");
              content_pos = strstr(cmd, "--content ");

              memcpy(input_cmd_board, ret + 12 * sizeof(char),
                     title_pos - (ret + 13 * sizeof(char)));
              memcpy(title, title_pos + 8 * sizeof(char),
                     content_pos - (title_pos + 9 * sizeof(char)));
              strcpy(buf, content_pos + 10 * sizeof(char));

              content_len =
                  strlen(cmd) - (content_pos + 10 * sizeof(char) - cmd);

              memset(insert_value, 0, sizeof(insert_value));
              strcat(insert_value, buf);
              // finish insert cmd
              // printf("%s\n", ret + 12 * sizeof(char));
              // printf("search board is (%s)\n", input_cmd_board);
              // printf("title is (%s)\n", title);
              // printf("content is (%s)\n", buf);

              std::string str_content(buf, buf + content_len);
              // printf("str_content is (%s)\n", str_content.c_str());
              std::size_t found;
              found = str_content.find("<br>");
              while (found != std::string::npos) {
                str_content.replace(found, 4, "\n");
                found = str_content.find("<br>");
              }
              // printf("str_content 2 is (%s)\n", str_content.c_str());

              strncpy(content, str_content.c_str(), str_content.length() + 1);
              // printf("content without br is \n---\n%s\n---\n", content);
              memset(tmp, 0, sizeof(tmp));
              memset(buf, 0, sizeof(buf));
              sprintf(buf, "select Name from BOARD where Name = \'%s\';",
                      input_cmd_board);
              // printf("%s\n", buf);

              // Execute SQL statement
              rc = sqlite3_exec(board_db, buf, find_board, (void *)data,
                                &zErrMsg);
              if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                // sprintf(tmp, "Board is already exist.\n");
                printf("ErrMsg number is %d\n", rc);
              } else {
                fprintf(stdout, "find board successfully\n");
                // sprintf(tmp, "Create board successfully.\n");
              }

              if (if_find_board == false) {
                strcat(tmp, "Board does not exist.\n");
              } else {
                time_t t = time(0);
                char data_type1[64], data_type2[64];
                strftime(data_type1, sizeof(data_type1), "%Y/%m/%d",
                         localtime(&t));
                // puts(data_type1);
                strftime(data_type2, sizeof(data_type2), "%m/%d",
                         localtime(&t));
                // puts(data_type2);

                memset(buf, 0, sizeof(buf));
                sprintf(buf,
                        "%s VALUES(\'%s\', \'%s\', \'%s\', \'%s\', \'%s\', "
                        "\'%s\');",
                        insert_to_post, input_cmd_board, title, now_user,
                        data_type1, data_type2, content);
                // printf("%s\n", buf);

                // Execute insert post
                rc = sqlite3_exec(post_db, buf, callback, (void *)data,
                                  &zErrMsg);
                if (rc != SQLITE_OK) {
                  fprintf(stderr, "SQL error: %s\n", zErrMsg);
                  sprintf(tmp, "SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                  printf("ErrMsg number is %d\n", rc);
                } else {
                  fprintf(stdout, "insert post successfully\n");
                  strcat(tmp, "Create post successfully.\n");
                }
              }
              if_find_board = false;
              strcat(tmp, "% ");
              send(index, tmp, sizeof(tmp), 0);
              memset(input_cmd_board, 0, sizeof(input_cmd_board));
              strcat(buf, "\n");
            }
          }
        }
        // --------------------------list-board--------------------------
        else if (strncmp(cmd, "list-board", 10) == 0) {
          char list_board_msg[2048] = {0};
          sprintf(list_board_msg, "\tIndex\t\tName\t\tModerator\n");
          find_user_fd(index);

          const char ch = '#';
          char *key;
          key = strchr(cmd, ch); // advance key search
          if (key == nullptr) {
            // printf("the key is null\n");
            rc = sqlite3_exec(board_db, select_all_board, list_board,
                              (void *)data, &zErrMsg);
            if (rc != SQLITE_OK) {
              fprintf(stderr, "SQL error: %s\n", zErrMsg);
              sqlite3_free(zErrMsg);
            } else {
              fprintf(stdout, "%s", sql_op_success_msg);
            }
            strcat(list_board_msg, tmp_list_splcmd);
            strcat(list_board_msg, "% ");
            send(index, list_board_msg, sizeof(list_board_msg), 0);
            memset(tmp_list_splcmd, 0, sizeof(tmp_list_splcmd));
          } else {
            key = key + 2 * sizeof(char);
            char tmp_sql_cmd[1024], tmp_key[256];
            strcpy(tmp_key, key);
            sprintf(tmp_sql_cmd,
                    "select * from BOARD where BoardID like \'%%%s%%\' or Name "
                    "like \'%%%s%%\' or Moderator like \'%%%s%%\';",
                    tmp_key, tmp_key, tmp_key);
            printf("The key is (%s)\n", key);
            printf("spl cmd is %s\n", tmp_sql_cmd);
            key = nullptr;

            rc = sqlite3_exec(board_db, tmp_sql_cmd, list_board, (void *)data,
                              &zErrMsg);
            if (rc != SQLITE_OK) {
              fprintf(stderr, "SQL error: %s\n", zErrMsg);
              sqlite3_free(zErrMsg);
            } else {
              fprintf(stdout, "%s", sql_op_success_msg);
            }
            strcat(list_board_msg, tmp_list_splcmd);
            strcat(list_board_msg, "% ");
            send(index, list_board_msg, sizeof(list_board_msg), 0);
            memset(tmp_list_splcmd, 0, sizeof(tmp_list_splcmd));
          }
          list_i = 0;
        }
        // --------------------------list-post--------------------------
        else if (strncmp(cmd, "list-post", 9) == 0) {
          std::string str_cmd(cmd, cmd + strlen(cmd));
          if_find_board = false;
          if (str_cmd.length() == 9) {
            std::string server_remsg = "Usage: list-post <board> ##key\n% ";
            send(index, server_remsg.c_str(), server_remsg.length(), 0);
          } else {
            char list_post_msg[2048] = {0}, tmp[1024] = {0};
            std::string cmd_input_board(cmd + 10 * sizeof(char),
                                        strlen(cmd) - 10 * sizeof(char)),
                str_tmp;
            printf("cmd_input_board is (%s)\n", cmd_input_board.c_str());
            std::size_t found = cmd_input_board.find("##");
            if (found == std::string::npos) {
              str_tmp = cmd_input_board;
              // printf("found is %d\n", found);
            } else {
              str_tmp = cmd_input_board.substr(0, found - 1);
            }
            printf("cmd_input_board is (%s)\n", str_tmp.c_str());

            memset(tmp, 0, sizeof(tmp));
            sprintf(tmp, "select Name from BOARD where Name = \'%s\';",
                    str_tmp.c_str());
            // printf("%s\n", tmp);
            strcpy(input_cmd_board, str_tmp.c_str());
            printf("input_cmd_board is (%s)\n", input_cmd_board);

            // Execute SQL statement
            rc =
                sqlite3_exec(board_db, tmp, find_board, (void *)data, &zErrMsg);
            if (rc != SQLITE_OK) {
              fprintf(stderr, "SQL error: %s\n", zErrMsg);
              sqlite3_free(zErrMsg);
              // sprintf(tmp, "Board is already exist.\n");
              printf("ErrMsg number is %d\n", rc);
            } else {
              fprintf(stdout, "find board sql successfully\n");
              // sprintf(tmp, "Create board successfully.\n");
            }

            // printf("if_find_board is %d\n", if_find_board);

            memset(tmp, 0, sizeof(tmp));
            if (if_find_board == false) {
              std::string error = "Board does not exist.\n% ";
              send(index, error.c_str(), error.length(), 0);
            } else {
              memset(tmp_list_splcmd, 0, sizeof(tmp_list_splcmd));
              sprintf(list_post_msg, "\tIndex\t\tTitle\t\tAuthor\t\tDate\n");
              find_user_fd(index);
              char list_post_sql[1024] = {0};
              sprintf(list_post_sql,
                      "select PostID, Title, Author, Date2 from Post where "
                      "Board = \'%s\'",
                      str_tmp.c_str());

              const char ch = '#';
              char *key;
              key = strchr(cmd, ch); // advance key search
              if (key == nullptr) {
                // printf("the key is null\n");
                rc = sqlite3_exec(post_db, list_post_sql, list_post,
                                  (void *)data, &zErrMsg);
                if (rc != SQLITE_OK) {
                  fprintf(stderr, "SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                } else {
                  fprintf(stdout, "%s", sql_op_success_msg);
                }
                strcat(list_post_msg, tmp_list_splcmd);
                strcat(list_post_msg, "% ");
                send(index, list_post_msg, sizeof(list_post_msg), 0);
                memset(tmp_list_splcmd, 0, sizeof(tmp_list_splcmd));
              } else {
                key = key + 2 * sizeof(char);
                char tmp_sql_cmd[1024], tmp_key[256];
                strcpy(tmp_key, key);
                sprintf(tmp_sql_cmd,
                        "select PostID, Title, Author, Date2 from Post where "
                        "Board = \'%s\' AND (PostID like \'%%%s%%\' or Title "
                        "like \'%%%s%%\' or Author like \'%%%s%%\' or    Date2 "
                        "like \'%%%s%%\');",
                        str_tmp.c_str(), tmp_key, tmp_key, tmp_key, tmp_key);
                // printf("The key is (%s)\n", key);
                // printf("spl cmd is %s\n", tmp_sql_cmd);
                key = nullptr;

                rc = sqlite3_exec(post_db, tmp_sql_cmd, list_post, (void *)data,
                                  &zErrMsg);
                if (rc != SQLITE_OK) {
                  fprintf(stderr, "SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                } else {
                  fprintf(stdout, "%s", sql_op_success_msg);
                }
                strcat(list_post_msg, tmp_list_splcmd);
                strcat(list_post_msg, "% ");
                send(index, list_post_msg, sizeof(list_post_msg), 0);
                memset(tmp_list_splcmd, 0, sizeof(tmp_list_splcmd));
              }
            }
            if_find_board = false;
            memset(input_cmd_board, 0, sizeof(input_cmd_board));
            list_i = 0;
          }

        }
        // --------------------------update-post--------------------------
        else if (strncmp(cmd, "update-post", 11) == 0) {
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else if (strncmp(cmd, "update-post ", 12) != 0) {
            std::string response =
                "Usage: update-post <post-id> --title/content <new>\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            std::string str_cmd(cmd, cmd + strlen(cmd));
            std::size_t found_title = str_cmd.find("--title"),
                        found_content = str_cmd.find("--content");
            if ((found_title == std::string::npos) &&
                (found_content == std::string::npos)) {
              std::string response =
                  "Usage: update-post <post-id> --title/content <new>\n% ";
              send(index, response.c_str(), response.size(), 0);
            } else {
              std::string str_postid, str_change, title_or_content;
              if (found_title != std::string::npos) {
                str_postid = str_cmd.substr(12, found_title - 1 - 12);
                str_change = str_cmd.substr(found_title + 8);
                // printf("postid is (%s)\n", str_postid.c_str());
                // printf("update part is (%s)\n", str_change.c_str());
                title_or_content = "TITLE";
              } else {
                str_postid = str_cmd.substr(12, found_content - 1 - 12);
                str_change = str_cmd.substr(found_content + 10);
                // printf("postid is (%s)\n", str_postid.c_str());
                // printf("update part is (%s)\n", str_change.c_str());
                title_or_content = "CONTENT";
              }
              char tmp[1024] = {0}, buf[1024] = {0};
              sprintf(tmp, "select Author from POST where PostId = %s;",
                      str_postid.c_str());
              rc =
                  sqlite3_exec(post_db, tmp, find_post, (void *)data, &zErrMsg);
              if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
              } else {
                fprintf(stdout, "%s", sql_op_success_msg);
              }

              if (strlen(return_postowner) == 0) {
                std::string response = "Post does not exist.\n% ";
                send(index, response.c_str(), response.size(), 0);
              } else {
                if (strcmp(now_user, return_postowner) != 0) {
                  std::string response = "Not the post owner.\n% ";
                  send(index, response.c_str(), response.size(), 0);
                } else {
                  std::string response = "Update successfully.\n% ";
                  send(index, response.c_str(), response.size(), 0);
                  memset(tmp, 0, sizeof(tmp));

                  if (title_or_content == "CONTENT") {
                    std::size_t found;
                    found = str_change.find("<br>");
                    while (found != std::string::npos) {
                      str_change.replace(found, 4, "\n");
                      found = str_change.find("<br>");
                    }
                  }

                  sprintf(tmp, "UPDATE POST SET %s = \'%s\' where PostId = %s;",
                          title_or_content.c_str(), str_change.c_str(),
                          str_postid.c_str());
                  // printf("update sql cmd is [%s]\n", tmp);
                  rc = sqlite3_exec(post_db, tmp, callback, (void *)data,
                                    &zErrMsg);
                  if (rc != SQLITE_OK) {
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                  } else {
                    fprintf(stdout, "update post successfully");
                  }
                }
                memset(return_postowner, 0, sizeof(return_postowner));
              }
            }
          }
        }
        // --------------------------delete-post--------------------------
        else if (strncmp(cmd, "delete-post", 11) == 0) {
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else if (strncmp(cmd, "delete-post ", 12) != 0) {
            std::string response = "Usage: delete-post <post-id>\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            std::string str_cmd(cmd, cmd + strlen(cmd)),
                str_postid = str_cmd.substr(12);

            char tmp[1024] = {0}, buf[1024] = {0};
            sprintf(tmp, "select Author from POST where PostId = %s;",
                    str_postid.c_str());
            rc = sqlite3_exec(post_db, tmp, find_post, (void *)data, &zErrMsg);
            if (rc != SQLITE_OK) {
              fprintf(stderr, "SQL error: %s\n", zErrMsg);
              sqlite3_free(zErrMsg);
            } else {
              fprintf(stdout, "find post successfully");
            }

            if (strlen(return_postowner) == 0) {
              std::string response = "Post does not exist.\n% ";
              send(index, response.c_str(), response.size(), 0);
            } else {
              if (strcmp(now_user, return_postowner) != 0) {
                std::string response = "Not the post owner.\n% ";
                send(index, response.c_str(), response.size(), 0);
              } else {
                std::string response = "Delete successfully.\n% ";
                send(index, response.c_str(), response.size(), 0);
                memset(tmp, 0, sizeof(tmp));

                sprintf(tmp, "DELETE from POST where PostId = %s;",
                        str_postid.c_str());
                // printf("delete sql cmd is [%s]\n", tmp);
                rc = sqlite3_exec(post_db, tmp, callback, (void *)data,
                                  &zErrMsg);
                if (rc != SQLITE_OK) {
                  fprintf(stderr, "SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                } else {
                  fprintf(stdout, "Delete successfully");
                }
              }
              memset(return_postowner, 0, sizeof(return_postowner));
            }
          }
        }
        // --------------------------read--------------------------
        else if (strncmp(cmd, "read", 4) == 0) {
          find_user_fd(index);
          std::string read_post_sql(cmd, cmd + strlen(cmd));
          if (strncmp(cmd, "read ", 5) != 0) {
            std::string response = "Usage: read <post-id>\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            char tmp[1024] = {0}, buf[1024] = {0}, *postid = nullptr;
            postid = cmd + 5 * sizeof(char);
            sprintf(tmp,
                    "select Author, Title, Date1, Content from POST where "
                    "PostId = %s;",
                    postid);

            rc = sqlite3_exec(post_db, tmp, read_post, (void *)data, &zErrMsg);
            if (rc != SQLITE_OK) {
              fprintf(stderr, "SQL error: %s\n", zErrMsg);
              sqlite3_free(zErrMsg);
            } else {
              fprintf(stdout, "%s", sql_op_success_msg);
            }

            if (strlen(return_post) == 0) {
              std::string response = "Post does not exist.\n% ";
              send(index, response.c_str(), response.size(), 0);
            } else {
              // find comment
              memset(tmp, 0, sizeof(tmp));
              sprintf(
                  tmp,
                  "select username, comment from COMMENT where PostId = %s;",
                  postid);
              printf("[%s]\n", tmp);
              rc = sqlite3_exec(comment_db, tmp, read_comment, (void *)data,
                                &zErrMsg);
              if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
              } else {
                fprintf(stdout, "%s", sql_op_success_msg);
              }

              strcat(buf, return_post);
              strcat(buf, return_comment);
              strcat(buf, "% ");
              send(index, buf, sizeof(buf), 0);
            }
            memset(return_post, 0, sizeof(return_post));
            memset(return_comment, 0, sizeof(return_comment));
          }
        }
        // ----------------------------comment----------------------------
        else if (strncmp(cmd, "comment", 7) == 0) {
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else if (strncmp(cmd, "comment ", 8) != 0) {
            std::string response = "Usage: comment <post-id> <comment>\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            std::string str_cmd(cmd, cmd + strlen(cmd));
            std::size_t found_postid, found_comment;
            found_postid = str_cmd.find(" ");
            std::string str_postid = str_cmd.substr(found_postid + 1);
            found_comment = str_postid.find(" ");
            if (found_comment == std::string::npos) {
              std::string response = "Usage: comment <post-id> <comment>\n% ";
              send(index, response.c_str(), response.size(), 0);
            } else {
              std::string str_comment = str_postid.substr(found_comment + 1);
              std::string str_tmp3 = str_postid.substr(0, found_comment);
              str_postid.swap(str_tmp3);
              // printf("postid is (%s)\n", str_postid.c_str());
              // printf("comment is (%s)\n", str_comment.c_str());
              char tmp[1024] = {0}, buf[1024] = {0};
              sprintf(tmp,
                      "select Author, Title, Date1, Content from POST where "
                      "PostId = %s;",
                      str_postid.c_str());

              rc =
                  sqlite3_exec(post_db, tmp, read_post, (void *)data, &zErrMsg);
              if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
              } else {
                fprintf(stdout, "%s", sql_op_success_msg);
              }

              if (strlen(return_post) == 0) {
                std::string response = "Post does not exist.\n% ";
                send(index, response.c_str(), response.size(), 0);
              } else {
                char insert_to_comment[1024] = {0};
                sprintf(insert_to_comment,
                        "INSERT INTO COMMENT (PostID,Username,comment) VALUES "
                        "(\'%s\', \'%s\', \'%s\')",
                        str_postid.c_str(), now_user, str_comment.c_str());

                rc = sqlite3_exec(comment_db, insert_to_comment, callback,
                                  (void *)data, &zErrMsg);
                if (rc != SQLITE_OK) {
                  fprintf(stderr, "SQL error: %s\n", zErrMsg);
                  sqlite3_free(zErrMsg);
                } else {
                  fprintf(stdout, "insert comment successfully.\n");
                }
              }
              std::string response = "Comment successfully.\n% ";
              send(index, response.c_str(), response.size(), 0);
              memset(return_post, 0, sizeof(return_post));
            }
          }
          // ----------------------------exit----------------------------
        } else if (strncmp(cmd, "exit", 4) == 0) {
          // std::string response = "exit\n";
          // send(index, response.c_str(), response.size(), 0);
          printf("cancel connection %d from %s:%d\n", listener,
                 inet_ntoa(sockaddr.sin_addr), (int)ntohs(sockaddr.sin_port));

          for (int vec_i = 0; vec_i < sizeof(vec); vec_i++) {
            if (vec[vec_i].user_fd == index) {
              vec.erase(vec.begin() + vec_i);
            }
          }
          close(index);
          FD_CLR(index, &master);

          // ---------------------------unknown cmd---------------------------
        } else {
          std::string response = "Command not found.\n% ";
          send(index, response.c_str(), response.size(), 0);
        }
      }
    }
    // Close the connections
    // close(connection);
  }
  close(listener);

  if (remove("client.db") != 0)
    perror("Error deleting file");
  else
    puts("File successfully deleted");
}
