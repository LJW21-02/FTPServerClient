#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define PATH_LEN 500

void convert_to_absolute_path(char *path, char *cwp, char *tgt);
void combine_slashes(char *path);
int count_commas(const char *str);
int is_port_available(int port);
void remove_root_folder(char *path);
int store_file(int controlfd, int datafd, char *filepath);
int retrieve_file(int controlfd, int datafd, char *input);
int do_list(int controlfd, int datafd, char *cwp);
void trim_spaces(char *str);

int main(int argc, char **argv) {
	int listenfd, connfd, datafd;		// 监听socket和连接socket不一样，后者用于数据传输
	struct sockaddr_in addr, data_addr;
    socklen_t data_len = sizeof(data_addr);
	int len;
    pid_t pid;

    // Variables
    int tgt_port_number = 21;
	char path_to_file_area[1002];
    char cur_working_path[1002], tgt_file[1002];
    char valid_username[100], valid_password[100];
    strcpy(valid_username, "anonymous");     // default username
    strcpy(valid_password, "anonymous@");    // default password

    // Extract port and root arguments if exist
    char tgt_path[500] = "/tmp";
	if (argc == 3 || argc == 5) {
		for (int i = 1; i < argc - 1; i += 2) {
			if (strcmp(argv[i], "-port") == 0) {
				tgt_port_number = atoi(argv[i+1]);
			}
			else if (strcmp(argv[i], "-root") == 0) {
                memset(tgt_path, 0, sizeof(tgt_path));
				strcpy(tgt_path, argv[i+1]);
			}
			else {
				printf("Usage: ./server -port n -root path\n");
				printf("The arguments port and root are optional, and can be accepted in any order.\n");
			}
		}
	}
	else if (argc != 1) {
		printf("Usage: ./server -port n -root path\n");
		printf("The arguments port and root are optional, and can be accepted in any order.\n");
	}
    
    // Convert root path into absolute path
    int tmp_len = strlen(tgt_path);
    combine_slashes(tgt_path);
    if (tgt_path[tmp_len - 1] == '/') tgt_path[tmp_len - 1] = '\0';
    if (tgt_path[0] == '/') strcpy(path_to_file_area, tgt_path);
    else if (strncmp(tgt_path, "~/", 2) == 0) {
        char *home = getenv("HOME");
        if (home) {
            strcpy(path_to_file_area, home);
            strcat(path_to_file_area, tgt_path + 1);
        }
        else {
            printf("HOME environment variable not set.\n");
        }
    }
    else {
        char cwd[PATH_LEN];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(path_to_file_area, sizeof(path_to_file_area), "%s/%s", cwd, tgt_path);
        } 
        else {
            printf("getcwd() error\n");
            return 1;
        }
    } 

    // Create root directory if not exist
    DIR* dir = opendir(path_to_file_area);
    if (dir) {
        printf("Root directory already exists.\n");
        strcpy(cur_working_path, path_to_file_area);
        closedir(dir);
    } 
    else {
        printf("Root directory currently not exist.\n");
        if (mkdir(path_to_file_area, 0777) == 0) {
            printf("Root directory created successfully.\n");
            strcpy(cur_working_path, path_to_file_area);
        } else {
            printf("Failed to create root directory.\n");
        }
    }
	
	// 创建socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 设置本机的ip和port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(tgt_port_number);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);	// 监听"0.0.0.0"

	// 将本机的ip和port与socket绑定
	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 开始监听socket
	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 持续监听连接请求
	while (1) {
		// 等待client的连接 -- 阻塞函数
		if ((connfd = accept(listenfd, NULL, NULL)) == -1) {
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}

        printf("New client detected.\n");
        char welcome_msg[100];
        strcpy(welcome_msg, "220 Anonymous FTP server ready.\r\n");
        write(connfd, welcome_msg, strlen(welcome_msg));

        
        if((pid = fork()) == 0) {
            close(listenfd);

            // Variables for a specific client
            int user_done = 0, pass_done = 0, to_rename = 0;
            int port_port, pasv_port;
            char port_ip[20], pasv_ip[20];
            char sentence[8192];

            // 榨干socket传来的内容
		    while (1) {
                // receive_input command
                memset(sentence, 0, sizeof(sentence));
			    int n = read(connfd, sentence, sizeof(sentence));
    			if (n < 0) {
	    			printf("Error read(): %s(%d)\n", strerror(errno), errno);
		    		break;
    			} 
                else if (n == 0) {
                    printf("Connection closed by the client.\n");
                    close(connfd);
                    break;
                }
                int sentence_len = strlen(sentence);
                if (sentence[sentence_len - 1] == '\n') sentence[sentence_len - 1] = '\0';
                if (sentence[sentence_len - 2] == '\r') sentence[sentence_len - 2] = '\0';
                printf("Received string: %s\n", sentence);

                // If cmd "USER" is not run yet
                if (!user_done) {
                    // Only cmd "USER" can be accepted
                    if (strncmp(sentence, "USER", 4) == 0) {
                        char input_username[100];
                        snprintf(input_username, sizeof(input_username), "%s", sentence + 5);
                        memset(sentence, 0, sizeof(sentence));
                        if (strcmp(valid_username, input_username) == 0) {
                            strcpy(sentence, "331 Guest login ok, send your complete e-mail address as password.\r\n");
                            user_done = 1;
                        }
                        else {
                            strcpy(sentence, "530 User not found.\r\n");
                        }   
                    }
                    else {
                        memset(sentence, 0, sizeof(sentence));
                        strcpy(sentence, "503 Bad sequence of commands.\r\n");
                    }
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // If user has logged in, proceed
                if (strncmp(sentence, "USER", 4) == 0) {
                    memset(sentence, 0, sizeof(sentence));
                    strcpy(sentence, "230 User logged in, proceed.\r\n");
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // If user logged in and need authentication
                if (!pass_done) {
                    if (strncmp(sentence, "PASS", 4) == 0) {
                        char input_email[200];
                        snprintf(input_email, sizeof(input_email), "%s", sentence + 5);
                        memset(sentence, 0, sizeof(sentence));
                        if (strcmp(input_email, valid_password) == 0) {
                            strcpy(sentence, "230 Guest login ok, access restrictions apply.\r\n");
                            pass_done = 1;
                        }
                        else {
                            strcpy(sentence, "530 User not authorized.\r\n");
                        }
                    }
                    else {
                        memset(sentence, 0, sizeof(sentence));
                        strcpy(sentence, "503 Bad sequence of commands.\r\n");
                    }
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To rename a target file
                if (to_rename) {
                    // accept RNTO after RNFR is run
                    if (strncmp(sentence, "RNTO", 4) == 0) {
                        char input_path[500], full_path[1002];
                        snprintf(input_path, sizeof(input_path), "%s", sentence + 5);
                        combine_slashes(input_path);

                        char tmp_file[1002];
                        strcpy(tmp_file, tgt_file);
                        remove_root_folder(tmp_file);
                        memset(sentence, 0, sizeof(sentence));

                        // Invalid path
                        if ((strlen(input_path) == 0) || (strlen(input_path) == 1 && input_path[0] == '/')) {
                            snprintf(sentence, sizeof(sentence), "550 Failed to rename \"%s\" to \"%s\"\r\n", tmp_file, input_path);
                        }
                        else {
                            convert_to_absolute_path(input_path, cur_working_path, full_path);
                            char tmp_new_path[1002];
                            strcpy(tmp_new_path, full_path);
                            remove_root_folder(tmp_new_path);
                            if (strstr(full_path, path_to_file_area) != NULL) {
                                if (access(full_path, F_OK) != -1) {
                                    snprintf(sentence, sizeof(sentence), "550 Failed to rename \"%s\" to \"%s\"\r\n", tmp_file, tmp_new_path);
                                } 
                                else {
                                    if (rename(tgt_file, full_path) == 0) {
                                        snprintf(sentence, sizeof(sentence), "250 \"%s\" renamed to \"%s\" successfully.\r\n", tmp_file, tmp_new_path);
                                    }
                                    else {
                                        snprintf(sentence, sizeof(sentence), "550 Failed to rename \"%s\" to \"%s\"\r\n", tmp_file, tmp_new_path);
                                    }
                                }
                            }
                            else {
                                snprintf(sentence, sizeof(sentence), "550 Failed to rename \"%s\" to \"%s\"\r\n", tmp_file, tmp_new_path);
                            }
                        }
                        to_rename = 0;
                        write(connfd, sentence, strlen(sentence));
                        continue;
                    }
                    else {
                        memset(sentence, 0, sizeof(sentence));
                        strcpy(sentence, "503 Bad sequence of commands.\r\n");
                        write(connfd, sentence, strlen(sentence));
                        continue;
                    }
                }
                // If authentication is checked, proceed
                if (strncmp(sentence, "PASS", 4) == 0) {
                    memset(sentence, 0, sizeof(sentence));
                    strcpy(sentence, "230 User logged in, proceed.\r\n");
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To determine servers os and type settings
                if (strncmp(sentence, "SYST", 4) == 0) {
                    memset(sentence, 0, sizeof(sentence));
                    strcpy(sentence, "215 UNIX Type: L8\r\n");
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To set the TYPE to binary
                if (strncmp(sentence, "TYPE", 4) == 0) {
                    char input_type[100];
                    snprintf(input_type, sizeof(input_type), "%s", sentence + 5);
                    memset(sentence, 0, sizeof(sentence));
                    if (strcmp(input_type, "I") == 0) {
                        strcpy(sentence, "200 Type set to I.\r\n");
                    }
                    else {
                        strcpy(sentence, "504 Command not implemented for that parameter.\r\n");
                    }
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To send PORT command
                if (strncmp(sentence, "PORT", 4) == 0) {
                    // Check format of input string
                    if (count_commas(sentence + 5) != 5) {
                        memset(sentence, 0, sizeof(sentence));
                        strcpy(sentence, "501 Syntax error in parameters or arguments.\r\n");
                        write(connfd, sentence, strlen(sentence));
                        continue;
                    }
                    else {
                        // Extract client ip and port
                        char *token;
                        token = strtok(sentence + 5, ",");
                        char port_ip_addr[50] = "";
                        int count = 0;
                        while ((token != NULL) && (++count < 5)) {
                            strcat(port_ip_addr, ".");
                            strcat(port_ip_addr, token);
                            token = strtok(NULL, ",");
                        }
                        int h1 = atoi(token);
                        token = strtok(NULL, ",");
                        int h2 = atoi(token);
                        port_port = (h1 << 8) + h2; 
                        strcpy(port_ip, port_ip_addr + 1);

                        memset(sentence, 0, sizeof(sentence));
                        strcpy(sentence, "200 Command OK; client IP and port received.\r\n");
                        len = strlen(sentence);
                        write(connfd, sentence, len);

                        // Get command after running PORT
                        char data_sentence[8192];
                        memset(data_sentence, 0, sizeof(data_sentence));
                        int n = read(connfd, data_sentence, sizeof(data_sentence));
                        if (n < 0) {
                            printf("Error read(): %s(%d)\n", strerror(errno), errno);
                            break;
                        } 
                        else if (n == 0) {
                            printf("Connection closed by the client.\n");
                            close(connfd);
                            break;
                        }
                        int data_sentence_len = strlen(data_sentence);
                        if (data_sentence[data_sentence_len - 1] == '\n') data_sentence[data_sentence_len - 1] = '\0';
                        if (data_sentence[data_sentence_len - 2] == '\r') data_sentence[data_sentence_len - 2] = '\0';
                        printf("Received string: %s\n", data_sentence);

                        // To store file from client to server
                        if (strncmp(data_sentence, "STOR", 4) == 0) {
                            // Create socket for data transfer
                            if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                                return 1;
                            }

                            memset(&data_addr, 0, sizeof(data_addr));
                            data_addr.sin_family = AF_INET;
                            data_addr.sin_port = htons(port_port);
                            if (inet_pton(AF_INET, port_ip, &data_addr.sin_addr) <= 0) {			//转换ip地址:点分十进制-->二进制
                                printf("ip: %s\n", port_ip);
                                printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                                return 1;
                            }

                            // Connect to client
                            if (connect(datafd, (struct sockaddr*)&data_addr, data_len) < 0) {
                                printf("Error connecting to data port\n");
                                return 1;
                            }
                            memset(sentence, 0, sizeof(sentence));
                            strcpy(sentence, "150 Opening binary connection to the IP address and port number specified by the earlier port command.\r\n");
                            write(connfd, sentence, strlen(sentence));

                            // Convert input filename to actual path
                            char filename[256];
                            snprintf(filename, sizeof(filename), "%s", data_sentence + 5);
                            char processed_filename[256];
                            convert_to_absolute_path(filename, cur_working_path, processed_filename);

                            // Store file from client side to cwp of server
                            if (store_file(connfd, datafd, processed_filename) > 0) {
                                memset(sentence, 0, sizeof(sentence));
                                strcpy(sentence, "226 Closing data connection; file transfer complete.\r\n");
                            }
                            else {
                                memset(sentence, 0, sizeof(sentence));
                                strcpy(sentence, "550 File not found.\r\n");
                            }
                        }
                        // To retrieve file from server to client
                        else if (strncmp(data_sentence, "RETR", 4) == 0) {
                            // Create socket for data transfer
                            if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                                return 1;
                            }

                            memset(&data_addr, 0, sizeof(data_addr));
                            data_addr.sin_family = AF_INET;
                            data_addr.sin_port = htons(port_port);
                            if (inet_pton(AF_INET, port_ip, &data_addr.sin_addr) <= 0) {			//转换ip地址:点分十进制-->二进制
                                printf("ip: %s\n", port_ip);
                                printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                                return 1;
                            }

                            // Connect to client
                            if (connect(datafd, (struct sockaddr*)&data_addr, data_len) < 0) {
                                perror("Error connecting to data port");
                                return 1;
                            }
                            memset(sentence, 0, sizeof(sentence));
                            strcpy(sentence, "150 Opening binary connection to the IP address and port number specified by the earlier port command.\r\n");
                            write(connfd, sentence, strlen(sentence));

                            // Convert input filename to actual path
                            char filename[256];
                            snprintf(filename, sizeof(filename), "%s", data_sentence + 5);
                            char processed_filename[256];
                            convert_to_absolute_path(filename, cur_working_path, processed_filename);

                            // Retrieve file from server to client side
                            int result = retrieve_file(connfd, datafd, processed_filename);
                            memset(sentence, 0, sizeof(sentence));
                            printf("result: %d\n", result);
                            if (result) strcpy(sentence, "226 Closing data connection; file transfer complete.\r\n");
                            else strcpy(sentence, "550 File not found.\r\n");
                        }
                        // To list content in cwp of server
                        else if (strncmp(data_sentence, "LIST", 4) == 0) {
                            // Create socket for data transfer
                            if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                                return 1;
                            }

                            memset(&data_addr, 0, sizeof(data_addr));
                            data_addr.sin_family = AF_INET;
                            data_addr.sin_port = htons(port_port);
                            if (inet_pton(AF_INET, port_ip, &data_addr.sin_addr) <= 0) {			//转换ip地址:点分十进制-->二进制
                                printf("ip: %s\n", port_ip);
                                printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                                return 1;
                            }

                            // Connect to client
                            if (connect(datafd, (struct sockaddr*)&data_addr, data_len) < 0) {
                                perror("Error connecting to data port");
                                return 1;
                            }
                            memset(sentence, 0, sizeof(sentence));
                            strcpy(sentence, "150 Opening binary connection to the IP address and port number specified by the earlier port command.\r\n");
                            write(connfd, sentence, strlen(sentence));

                            // List out contents of cwp of server
                            int result = do_list(connfd, datafd, cur_working_path);
                            memset(sentence, 0, sizeof(sentence));
                            if (result) strcpy(sentence, "226 Closing data connection; file transfer complete.\r\n");
                            else strcpy(sentence, "451 Requested action aborted; local error in processing.\r\n");
                        }
                        // Invalid command after PORT
                        else {
                            memset(sentence, 0, sizeof(sentence));
                            strcpy(sentence, "503 Bad sequence of commands.\r\n");
                        }
                        write(connfd, sentence, strlen(sentence));
                        close(datafd);
                        continue;
                    } 
                }
                // To send PASV command
                if (strncmp(sentence, "PASV", 4) == 0) {
                    // Setup server ip and port
                    strcpy(pasv_ip, "127,0,0,1");
                    pasv_port = 30000;
                    while (!is_port_available(pasv_port)) pasv_port += 1;

                    // Create socket for data transfer
                    if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                        return 1;
                    }

                    memset(&data_addr, 0, sizeof(data_addr));
                    data_addr.sin_family = AF_INET;
                    data_addr.sin_addr.s_addr = INADDR_ANY; 
                    data_addr.sin_port = htons(pasv_port);

                    if (bind(datafd, (struct sockaddr*)&data_addr, sizeof(data_addr)) == -1) {
                        printf("Error bind(): %s(%d)\n", strerror(errno), errno);
                        close(datafd);
                        close(connfd);
                        return 1;
                    }

                    if (listen(listenfd, 10) == -1) {
                        printf("Error listen(): %s(%d)\n", strerror(errno), errno);
                        return 1;
                    }

                    memset(sentence, 0, sizeof(sentence));
                    snprintf(sentence, sizeof(sentence), "227 Entering Passive Mode (%s,%d,%d).\r\n", pasv_ip, pasv_port / 256, pasv_port % 256);
                    len = strlen(sentence);
                    write(connfd, sentence, len);
                    data_len = sizeof(data_addr);

                    while (1) {
                        // Waiting for client connection
                        if ((datafd = accept(datafd, (struct sockaddr*)&data_addr, &data_len)) == -1) {
                            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                            continue;
                        }
                        printf("Client data connection detected.\n");

                        memset(sentence, 0, sizeof(sentence));
                        strcpy(sentence, "150 Opening binary connection to the IP address and port number specified by the earlier port command.\r\n");
                        write(connfd, sentence, strlen(sentence));

                        close(listenfd);

                        // Get command after running PASV
                        char data_sentence[8192];
                        memset(data_sentence, 0, sizeof(data_sentence));
                        int data_n = read(connfd, data_sentence, sizeof(data_sentence));
                        if (data_n < 0) {
                            printf("Error read(): %s(%d)\n", strerror(errno), errno);
                            break;
                        } 
                        else if (data_n == 0) {
                            printf("Connection closed by the client.\n");
                            close(connfd);
                            break;
                        }
                        int data_sentence_len = strlen(data_sentence);
                        if (data_sentence[data_sentence_len - 1] == '\n') data_sentence[data_sentence_len - 1] = '\0';
                        if (data_sentence[data_sentence_len - 2] == '\r') data_sentence[data_sentence_len - 2] = '\0';
                        printf("Received string: %s\n", data_sentence);

                        // To store file from client to server
                        if (strncmp(data_sentence, "STOR", 4) == 0) {
                            // Convert input filename to actual path
                            char filename[256];
                            snprintf(filename, sizeof(filename), "%s", data_sentence + 5);
                            char processed_filename[256];
                            convert_to_absolute_path(filename, cur_working_path, processed_filename);

                            // Store file from client side to cwp of server
                            if (store_file(connfd, datafd, processed_filename) > 0) {
                                memset(sentence, 0, sizeof(sentence));
                                strcpy(sentence, "226 Closing data connection; file transfer complete.\r\n");
                            }
                            else {
                                memset(sentence, 0, sizeof(sentence));
                                strcpy(sentence, "550 File not found.\r\n");
                            }
                        }
                        // To retrieve file from server to client
                        else if (strncmp(data_sentence, "RETR", 4) == 0) {
                            // Convert input filename to actual path
                            char filename[256];
                            snprintf(filename, sizeof(filename), "%s", data_sentence + 5);
                            char processed_filename[256];
                            convert_to_absolute_path(filename, cur_working_path, processed_filename);

                            // Retrieve file from server to client side
                            int result = retrieve_file(connfd, datafd, processed_filename);
                            memset(sentence, 0, sizeof(sentence));
                            if (result) strcpy(sentence, "226 Closing data connection; file transfer successful.\r\n");
                            else strcpy(sentence, "550 File not found.\r\n");
                        }
                        // To list content in cwp of server
                        else if (strncmp(data_sentence, "LIST", 4) == 0) {
                            // List out contents of cwp of server
                            int result = do_list(connfd, datafd, cur_working_path);
                            memset(sentence, 0, sizeof(sentence));
                            if (result) strcpy(sentence, "226 Closing data connection; file transfer successful.\r\n");
                            else strcpy(sentence, "451 Requested action aborted; local error in processing.\r\n");
                        }
                        // Invalid command after PORT
                        else {
                            memset(sentence, 0, sizeof(sentence));
                            strcpy(sentence, "503 Bad sequence of commands.\r\n");
                        }
                        write(connfd, sentence, strlen(sentence));
                        close(datafd);
                        break;
                    }    
                    continue;
                }
                // To end connection with client
                if (strncmp(sentence, "QUIT", 4) == 0) {
                    printf("Closing connection...\n");
                    memset(sentence, 0, sizeof(sentence));
                    strcpy(sentence, "221-Thank you for using FTP service on Anonymous.\r\n221 Goodbye.\r\n");
                    len = strlen(sentence);
                    write(connfd, sentence, len);
                    close(connfd);
                    break;
                }
                // To create directory in server
                if (strncmp(sentence, "MKD", 3) == 0) {
                    char input_path[PATH_LEN], full_path[2* PATH_LEN + 2];
                    snprintf(input_path, sizeof(input_path), "%s", sentence + 4);
                    combine_slashes(input_path);

                    // Invalid directory name
                    if ((strlen(input_path) == 0) || (strlen(input_path) == 1 && input_path[0] == '/')) {
                        snprintf(sentence, sizeof(sentence), "550 %s creation failed.\r\n", input_path);
                    }
                    else {
                        // Convert target directory name into actual path
                        convert_to_absolute_path(input_path, cur_working_path, full_path);
                        printf("MKD full path: %s\n", full_path);
                        if (strstr(full_path, path_to_file_area) != NULL) {
                            remove_root_folder(input_path);
                            if (mkdir(full_path, 0777) == 0) {
                                snprintf(sentence, sizeof(sentence), "250 %s created successfully.\r\n", input_path);
                            } 
                            else {
                                snprintf(sentence, sizeof(sentence), "550 %s creation failed.\r\n", input_path);
                            }
                        }
                        else {
                            remove_root_folder(input_path); 
                            snprintf(sentence, sizeof(sentence), "550 %s creation failed.\r\n", input_path);
                        }
                    }  
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To remove directory in server
                if (strncmp(sentence, "RMD", 3) == 0) {
                    char input_path[500], full_path[1000];
                    snprintf(input_path, sizeof(input_path), "%s", sentence + 4);
                    combine_slashes(input_path);

                    // Invalid directory
                    if ((strlen(input_path) == 0) || (strlen(input_path) == 1 && input_path[0] == '/')) {
                        snprintf(sentence, sizeof(sentence), "550 %s deletion failed.\r\n", input_path);
                    }
                    else {
                        convert_to_absolute_path(input_path, cur_working_path, full_path);
                        if (strstr(full_path, path_to_file_area) != NULL) {
                            remove_root_folder(input_path);
                            if (rmdir(full_path) == 0) {
                                snprintf(sentence, sizeof(sentence), "250 %s removed successfully.\r\n", input_path);
                            } else {
                                snprintf(sentence, sizeof(sentence), "550 %s deletion failed.\r\n", input_path);
                            }
                        }
                        else {
                            remove_root_folder(input_path);
                            snprintf(sentence, sizeof(sentence), "550 %s deletion failed.\r\n", input_path);
                        }
                    }
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To print out current working path on server
                if (strncmp(sentence, "PWD", 3) == 0) {
                    char tmp_cwp[1002];
                    strcpy(tmp_cwp, cur_working_path);
                    remove_root_folder(tmp_cwp);
                    // TODO
                    snprintf(sentence, sizeof(sentence), "257 \"%s\" is the current directory.\r\n", tmp_cwp);
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To change current working path
                if (strncmp(sentence, "CWD", 3) == 0) {
                    char input_path[500], full_path[1000];
                    snprintf(input_path, sizeof(input_path), "%s", sentence + 4);
                    combine_slashes(input_path);

                    // Invalid path
                    if ((strlen(input_path) == 0) || (strlen(input_path) == 1 && input_path[0] == '/')) {
                        snprintf(sentence, sizeof(sentence), "550 Failed to change directory: \"%s\"\r\n", input_path);
                    }
                    else {
                        convert_to_absolute_path(input_path, cur_working_path, full_path);
                        if (strstr(full_path, path_to_file_area) != NULL) {
                            DIR* dir = opendir(full_path);
                            if (dir) {
                                char tmp_cwp[1002];
                                strcpy(tmp_cwp, full_path);
                                remove_root_folder(tmp_cwp);
                                memset(cur_working_path, 0, sizeof(cur_working_path));
                                strcpy(cur_working_path, full_path);
                                snprintf(sentence, sizeof(sentence), "250 Directory successfully changed to \"%s\"\r\n", tmp_cwp);
                                closedir(dir);
                            } 
                            else {
                                snprintf(sentence, sizeof(sentence), "550 Failed to change directory: \"%s\"\r\n", input_path);
                            }
                        }
                        else {
                            snprintf(sentence, sizeof(sentence), "550 Failed to change directory: \"%s\"\r\n", input_path);
                        }
                    }
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // To target a directory for renaming
                if (strncmp(sentence, "RNFR", 4) == 0) {
                    char input_path[500], full_path[1002];
                    snprintf(input_path, sizeof(input_path), "%s", sentence + 5);
                    combine_slashes(input_path);

                    // Invalid path
                    if ((strlen(input_path) == 0) || (strlen(input_path) == 1 && input_path[0] == '/')) {
                        snprintf(sentence, sizeof(sentence), "550 Directory invalid: \"%s\"\r\n", input_path);
                    }
                    else {
                        convert_to_absolute_path(input_path, cur_working_path, full_path);
                        if (strstr(full_path, path_to_file_area) != NULL) {
                            // Check if the directoy exist
                            if (access(full_path, F_OK) != -1) {
                                memset(tgt_file, 0, sizeof(tgt_file));
                                strcpy(tgt_file, full_path);
                                char tmp_file[1002];
                                strcpy(tmp_file, tgt_file);
                                remove_root_folder(tmp_file);
                                snprintf(sentence, sizeof(sentence), "350 Directory \"%s\" valid, pending for new directory.\r\n", tmp_file);
                                to_rename = 1;
                            } 
                            else {
                                snprintf(sentence, sizeof(sentence), "550 Directory invalid: \"%s\"\r\n", input_path);
                            }
                        }
                        else {
                            snprintf(sentence, sizeof(sentence), "550 Directory invalid: \"%s\"\r\n", input_path);
                        }
                    }
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
                // Other not supported command
                else {
                    memset(sentence, 0, sizeof(sentence));
                    strcpy(sentence, "500 Syntax error, command unrecognized.\r\n");
                    write(connfd, sentence, strlen(sentence));
                    continue;
                }
		    }   

            close(connfd);    
            _exit(1);     
        }
        else {
            close(connfd);
        }
	}

	close(listenfd);
}

// ======================================================================
// Convert path into absolute path
void convert_to_absolute_path(char *path, char *cwp, char *tgt) {

    // absolute path
    if (path[0] == '/') {
        strcpy(tgt, path);
    }

    // path relative to home directory
    else if (strncmp(path, "~/", 2) == 0) {
        char *home = getenv("HOME");
        if (home) {
            strcpy(tgt, home);
            strcat(tgt, path + 1);
        }
        else {
            printf("HOME environment variable not set.\n");
        }
    }

    // path relative to current directory
    else if (path[0] != '.') {
        strcpy(tgt, cwp);
        strcat(tgt, "/");
        strcat(tgt, path);
    }

    // current directory
    else if (strcmp(path, ".") == 0) {
        strcpy(tgt, cwp);
    }

    // path relative to current directory
    else if (strncmp(path, "./", 2) == 0) {
        strcpy(tgt, cwp);
        strcat(tgt, path + 1);
    }

    else {
        // parent directory
        strcpy(tgt, cwp);
        char *lastSlash = strrchr(tgt, '/');
        if (lastSlash != NULL) {
            if (lastSlash != tgt) {
                *lastSlash = '\0';
            } else {
               tgt[0] = '\0';
            }
        }
        else {
            tgt[0] = '\0';
        }

        // path relative to parent directory
        if (strncmp(path, "../", 3) == 0) {
            strcat(tgt, path + 2);
        }
    }
}

// Combine consecutive multiple slashes into one
void combine_slashes(char *path) {
    char *src = path;
    char *dst = path;
    int slash_count = 0;

    while (*src) {
        *dst = *src;

        if (*src == '/') slash_count++;
        else slash_count = 0;

        if (slash_count > 1) {
            while (*(src + 1) == '/') {
                src++;
            }
        }

        src++;
        dst++;
    }
    *dst = '\0';
}

// Count number of commas
int count_commas(const char *str) {
    int count = 0;
    
    while (*str) {
        if (*str == ',') {
            count++;
        }
        str++;
    }
    
    return count;
}

// Test availability of port
int is_port_available(int port) {
    int test_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (test_socket == -1) {
        perror("Error creating test socket");
        return 0; // Port is not available (error creating socket)
    }

    struct sockaddr_in test_addr;
    memset(&test_addr, 0, sizeof(test_addr));
    test_addr.sin_family = AF_INET;
    test_addr.sin_port = htons(port);
    test_addr.sin_addr.s_addr = INADDR_ANY;

    int bind_result = bind(test_socket, (struct sockaddr*)&test_addr, sizeof(test_addr));

    close(test_socket);

    if (bind_result < 0) {
        if (errno == EADDRINUSE) {
            return 0; 
        } else {
            perror("Error binding test socket");
            return 0; 
        }
    }

    return 1; 
}

// Remove root folder
void remove_root_folder(char *path) {
    char *firstSlash = strchr(path, '/'); 
    
    if (firstSlash == path) {
        char *secondSlash = strchr(firstSlash + 1, '/');
        
        if (secondSlash) memmove(path, secondSlash, strlen(secondSlash) + 1);
        else path[0] = '\0';
    }
}

// List contents in current working directory of server
int do_list(int controlfd, int datafd, char *cwp){
    int MAXLINE = 8192;

	char filelist[1024], sendline[MAXLINE+1], str[MAXLINE+1];
	bzero(filelist, (int)sizeof(filelist));

    sprintf(str, "ls %s", cwp);
    FILE *in;
    extern FILE *popen();

    if (!(in = popen(str, "r"))) {
        return 0;
    }

    while (fgets(sendline, MAXLINE, in) != NULL) {
        write(datafd, sendline, strlen(sendline));
        bzero(sendline, (int)sizeof(sendline));
    }

    pclose(in);
    return 1;
}

// Remove spaces at the start & end of string
void trim_spaces(char *str) {
    int start = 0, end = strlen(str) - 1;
    while (str[start] == ' ' || str[start] == '\t' || str[start] == '\n') {
        start++;
    }
    while (end > start && (str[end] == ' ' || str[end] == '\t' || str[end] == '\n')) {
        end--;
    }
    if (end < start) {
        str[0] = '\0';
    } 
    else {
        int i, j = 0;
        for (i = start; i <= end; i++) {
            str[j++] = str[i];
        }
        str[j] = '\0'; 
    }
}

// Retrieve file from server to client
int retrieve_file(int controlfd, int datafd, char *filename){
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("fopen");
        return 0;  // Failed to open the file
    }

    char buffer[8192];  // Set an appropriate buffer size
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (write(datafd, buffer, n) < 0) {
            perror("write");
            fclose(file);
            return 0;  // Failed to write to datafd
        }
    }

    fclose(file);
    return 1; 
}

// Store file from client to server
int store_file(int controlfd, int datafd, char *filepath) {
    FILE *file = fopen(filepath, "wb");
    if (file == NULL) {
        perror("fopen");
        return 0;  // Failed to open the file
    }

    char buffer[8192];  // Set an appropriate buffer size
    size_t n;

    while ((n = read(datafd, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, n, file) < n) {
            perror("fwrite");
            fclose(file);
            return 0;  // Failed to write to the file
        }
    }

    fclose(file);
    return 1;  // File successfully received and written to the file
}