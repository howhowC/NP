#include <arpa/inet.h>
#include <cstdlib> // For exit() and EXIT_FAILURE
#include <cstring>
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

static int return_pw(void *data, int argc, char **argv, char **azColName) {
  memset(re_cli_pw, 0, sizeof(re_cli_pw));
  // argv[3] is the password
  strcat(re_cli_pw, argv[3]);
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
  sqlite3 *db;
  char *zErrMsg = 0;
  int rc;
  char *sql, insert_value[1024];
  const char *data = "Callback function called",
             *select_all = "SELECT * from CLIENT;",
             *insert_to_table = "INSERT INTO CLIENT (Username,Email,Password) ",
             *create_table =
                 "CREATE TABLE CLIENT("
                 "UID           INTEGER     PRIMARY KEY AUTOINCREMENT,"
                 "Username      TEXT        NOT NULL UNIQUE,"
                 "Email         TEXT        NOT NULL,"
                 "Password      TEXT        NOT NULL);";

  /* Open database */
  rc = sqlite3_open("client.db", &db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    exit(1);
  } else {
    fprintf(stderr, "Opened database successfully\n");
  }

  /* Execute SQL statement */
  rc = sqlite3_exec(db, create_table, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    fprintf(stdout, "Create table successfully\n");
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

        char cmd[100];
        memset(cmd, '\0', sizeof(char) * 100);
        printf("\nWaiting for new message...\n");
        auto bytesRead = read(index, cmd, 100);
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
              next = pch = nullptr;
              strcat(buf, ");");
              memset(insert_value, 0, sizeof(insert_value));
              strcat(insert_value, insert_to_table);
              strcat(insert_value, buf);
              // finish insert cmd
              printf("%s\n", insert_value);
              // Execute SQL statement
              rc = sqlite3_exec(db, insert_value, callback, (void *)data,
                                &zErrMsg);
              // register successfully
              if (rc != SQLITE_OK) {
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
          // ------------------------------login------------------------------
        } else if (strncmp(cmd, "login", 5) == 0) {
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
                rc = sqlite3_exec(db, insert_value, return_pw, (void *)data,
                                  &zErrMsg);
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

          // ----------------------------see all----------------------------
          // } else if (strncmp(cmd, "see", 3) == 0) {
          //   char tmp[1024] = {0};
          //   // Execute SQL statement
          //   rc = sqlite3_exec(db, select_all, callback, (void *)data,
          //   &zErrMsg); if (rc != SQLITE_OK) {
          //     fprintf(stderr, "SQL error: %s\n", zErrMsg);
          //     sqlite3_free(zErrMsg);
          //     sprintf(tmp, "SQL error: %s\n", zErrMsg);
          //   } else {
          //     fprintf(stdout, "%s", sql_op_success_msg);
          //     sprintf(tmp, "%s", sql_op_success_msg);
          //   }
          //   strcat(tmp, "% ");
          //   send(index, tmp, sizeof(tmp), 0);

          // ------------------------------logout------------------------------
        } else if (strncmp(cmd, "logout", 6) == 0) {
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

          // ------------------------------whoami------------------------------
        } else if (strncmp(cmd, "whoami", 6) == 0) {
          // find_user_fd(index);
          if (now_state == VISITOR) {
            std::string response = "Please login first.\n% ";
            send(index, response.c_str(), response.size(), 0);
          } else {
            char cli_whoami_msg[256] = {0};
            sprintf(cli_whoami_msg, "%s\n%% ", now_user);
            send(index, cli_whoami_msg, sizeof(cli_whoami_msg), 0);
          }

          // ------------------------------exit------------------------------
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
